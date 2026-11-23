#include "Forwarder.h"
#include "../environment/Environment.h"

#include <socketexceptions/SocketException.hpp>
#include <socketexceptions/BindingException.hpp>
#include <socketexceptions/TimeoutException.hpp>

#include <thread>
#include <chrono>
#include <vector>

#include <uuid/uuid.h>

namespace forwarder
{
    Forwarder::Forwarder(ForwarderConfig config): config(config)
    {
        // Register preconfigured addresses for specific groups
    }

    void Forwarder::addSocketToTCPReceiverGroup(const std::string &groupId, kt::TCPSocket socket)
    {
        std::string addressString = socket.getSocketAddress().getAddress().value_or("") + ":" + std::to_string(socket.getSocketAddress().getPortNumber());

        if (tcpReceiverSockets.find(groupId) == tcpReceiverSockets.end())
        {
            LOG4CXX_INFO(logger, "[TCP] - Creating new group with ID [" << groupId << "], adding address [" << addressString << "] to receiver group.");
            // No existing groups with this ID, creating new
            std::vector<kt::TCPSocket> sockets = { socket };
            tcpReceiverSockets.insert(std::make_pair(groupId, sockets));
        }
        else
        {
            LOG4CXX_DEBUG(logger, "[TCP] - Adding new connection [" << addressString << "] to receiver group [" << groupId << "].");
            tcpReceiverSockets[groupId].push_back(socket);
        }
    }

    void Forwarder::addSocketToTCPSenderGroup(const std::string& groupId, kt::TCPSocket socket)
    {
        std::string addressString = socket.getSocketAddress().getAddress().value_or("") + ":" + std::to_string(socket.getSocketAddress().getPortNumber());

        if (tcpSenderSockets.find(groupId) == tcpSenderSockets.end())
        {
            LOG4CXX_INFO(logger, "[TCP] - Creating new group with ID [" << groupId << "], adding address [" << addressString << "] to sender group.");
            // No existing groups with this ID, creating new
            std::vector<kt::TCPSocket> sockets = { socket };
            tcpSenderSockets.insert(std::make_pair(groupId, sockets));
        }
        else
        {
            LOG4CXX_DEBUG(logger, "[TCP] - Adding new sender connection [" << addressString << "] to group [" << groupId << "].");
            tcpSenderSockets[groupId].push_back(socket);
        }
    }

    void Forwarder::addAddressToReceiverUDPGroup(std::string groupId, kt::SocketAddress address)
    {
        auto group = udpReceiverAddresses.find(groupId);
        if (group != udpReceiverAddresses.end())
        {
            group->second.emplace(address);
        }
        else
        {
            std::unordered_set<kt::SocketAddress, forwarder::Forwarder::AddressHash, forwarder::Forwarder::AddressEqual> set;
            set.emplace(address);
            udpReceiverAddresses.emplace(groupId, set);
        }
    }

    void Forwarder::start()
    {
        forwarderIsRunning = true;

        if (!config.getTcpGroups().empty())
        {
            startTCPForwarder();
        }
        else
        {
            LOG4CXX_INFO(logger, "[TCP] - No TCP groups were configured, TCP forwarding is disabled.");
        }

        if (!config.getUdpGroups().empty())
        {
            startUDPForwarder();
        }
        else
        {
            LOG4CXX_INFO(logger, "[UDP] - No UDP groups were configured, UDP forwarding is disabled.");
        }
    }

    void Forwarder::startTCPForwarder()
    {
        for (const auto& tcpConfig : config.getTcpGroups())
        {
            for (const kt::SocketAddress& address : tcpConfig.preConfiguredAddresses)
            {
                try
                {
                    kt::TCPSocket preConfigured(address);
                    addSocketToTCPReceiverGroup(tcpConfig.groupId, preConfigured);
                    LOG4CXX_INFO(logger, "Registered preconfigured address [" + address.getAddress().value_or("") + ":" + std::to_string(address.getPortNumber()) + "] in TCP group [" + tcpConfig.groupId + "]");
                }
                catch(const kt::SocketException& e)
                {
                    LOG4CXX_WARN(logger, "Failed to connect to and register preconfigured address [" + address.getAddress().value_or("") + ":" + std::to_string(address.getPortNumber()) + "] in TCP group [" + tcpConfig.groupId + "] with error: " + e.what());
                }
            }
            
            std::optional<std::pair<kt::TCPServerSocket, kt::TCPServerSocket>> sockets = initializeConnectionAcceptingSocketsForGroup(tcpConfig);
            if (!sockets.has_value())
            {
                LOG4CXX_ERROR(logger, "Failed to initialize TCP connection sockets for group [" + tcpConfig.groupId + "] using receiving port [" + std::to_string(tcpConfig.receiverPort) + "] and sending port [" + std::to_string(tcpConfig.senderPort) + "]. This group will be unavailable.");
            }
            else
            {
                std::thread listeningThread(&Forwarder::startTCPConnectionListener, this, sockets.value(), tcpConfig.groupId);
                tcpRunningThreads.push_back(std::move(listeningThread));

                std::thread dataForwarderThread(&Forwarder::startTCPDataForwarder, this, tcpConfig.groupId);
                tcpRunningThreads.push_back(std::move(dataForwarderThread));
            }
        }
    }

    std::optional<std::pair<kt::TCPServerSocket, kt::TCPServerSocket>> Forwarder::initializeConnectionAcceptingSocketsForGroup(ForwarderGroup group)
    {
        try
        {
            kt::TCPServerSocket receiverSocket(config.getHostAddress(), group.receiverPort, 20, config.getIpVersion());

            try
            {
                kt::TCPServerSocket senderSocket(config.getHostAddress(), group.senderPort, 20, config.getIpVersion());

                return std::make_pair(receiverSocket, senderSocket);
            }
            catch(const kt::SocketException e)
            {
                LOG4CXX_ERROR(logger, "Failed to bind to sender listener socket for TCP group [" + group.groupId + "] on port [" + std::to_string(group.senderPort) + "].");
            }
        }
        catch(const kt::SocketException e)
        {
            LOG4CXX_ERROR(logger, "Failed to bind to receiver listener socket for TCP group [" + group.groupId +"] on port [" + std::to_string(group.receiverPort) + "].");
        }

        return std::nullopt;
    }

    void Forwarder::startTCPConnectionListener(std::pair<kt::TCPServerSocket, kt::TCPServerSocket> sockets, const std::string groupId)
    {
        auto& [receiverSocket, senderSocket] = sockets;

        LOG4CXX_INFO(logger, "[TCP] - Starting TCP connection listener for group [" + groupId + "]");
        while(forwarderIsRunning)
        {
            // Accept from receiver socket
            try
            {
                kt::TCPSocket socket = receiverSocket.accept(10000); // microseconds
                std::string addressString = socket.getSocketAddress().getAddress().value_or("") + ":" + std::to_string(socket.getSocketAddress().getPortNumber());

                LOG4CXX_INFO(logger, "[TCP] - Group [" << groupId << "] - Accepted new receiver connection from [" << addressString << "].");

                addSocketToTCPReceiverGroup(groupId, socket);
            }
            catch(kt::TimeoutException e)
            {
                // Timeout occurred its fine
            }
            catch(kt::SocketException e)
            {
                LOG4CXX_ERROR(logger, "[TCP] - Failed to accept incoming receiver client: " << e.what());
            }

            // Accept from sender socket
            try
            {
                kt::TCPSocket socket = senderSocket.accept(10000);
                std::string addressString = socket.getSocketAddress().getAddress().value_or("") + ":" + std::to_string(socket.getSocketAddress().getPortNumber());

                LOG4CXX_INFO(logger, "[TCP] - Group [" << groupId << "] - Accepted new sender connection from [" << addressString << "].");
                addSocketToTCPSenderGroup(groupId, socket);
            }
            catch(kt::TimeoutException e)
            {
                // Timeout occurred its fine
            }
            catch(kt::SocketException e)
            {
                LOG4CXX_ERROR(logger, "[TCP] - Failed to accept incoming sender client: " << e.what());
            }
        }

        LOG4CXX_INFO(logger, "[TCP] - Stopping listener thread.");

        // If we exit the loop, close the server sockets
        receiverSocket.close();
        senderSocket.close();
    }

    void Forwarder::startTCPDataForwarder(std::string groupId)
    {
        LOG4CXX_INFO(logger, "[TCP] - Starting TCP forwarder listener for group [" + groupId + "]");
        while (forwarderIsRunning)
        {
            std::vector<size_t> receiverToRemove;
            std::vector<size_t> senderToRemove;

            auto senderSockets = tcpSenderSockets.find(groupId);
            if (senderSockets != tcpSenderSockets.end())
            {
                for (size_t i = 0; i < senderSockets->second.size(); i++)
                {
                    const kt::TCPSocket& senderSocket = senderSockets->second[i];
                    if (!senderSocket.connected())
                    {
                        senderToRemove.push_back(i);
                    }
                    else
                    {
                        if (senderSocket.ready(1))
                        {
                            std::string received = senderSocket.receiveAmount(config.getTcpMaxReadInSize());
                            if (received.empty())
                            {
                                continue;
                            }
                            
                            std::string uuidString = getNewUUID();
                            LOG4CXX_DEBUG(logger, "[TCP - " + uuidString + "] - Group [" << groupId << "] with [" << received.size() << "] nodes. Received content [" << received << "] forwarding to receiver peers...");
                            
                            std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
                            auto receivers = tcpReceiverSockets.find(groupId);
                            if (receivers == tcpReceiverSockets.end())
                            {
                                LOG4CXX_DEBUG(logger, "[TCP - " + uuidString + "] - Group [" << groupId << "] No receiver sockets registered, dropping message.");
                                continue;
                            }

                            for (auto receiver = receivers->second.begin(); receiver != receivers->second.end(); ++receiver)
                            {
                                const kt::TCPSocket& forwardToSocket = *receiver;
                                size_t distance = std::distance(receivers->second.begin(), receiver);
                                if (!forwardToSocket.connected())
                                {
                                    LOG4CXX_DEBUG(logger, "[TCP - " + uuidString + "] - Group [" << groupId << "], peer [" << distance << "] is no longer connected, marking for removal from group.");
                                    receiverToRemove.push_back(distance);
                                }
                                else
                                {
                                    if (forwardToSocket.send(received, MSG_NOSIGNAL) > 0)
                                    {
                                        LOG4CXX_DEBUG(logger, "[TCP - " + uuidString + "] - Group [" << groupId << "], successfully forwarded to peer [" << distance << "]");
                                    }
                                    else
                                    {
                                        LOG4CXX_INFO(logger, "[TCP - " + uuidString + "] - Group [" << groupId << "], failed to send to peer [" << distance << "], marking for removal from group.");
                                        receiverToRemove.push_back(distance);
                                    }
                                }
                            }

                            std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
                            LOG4CXX_DEBUG(logger, "[TCP - " + uuidString + "] - Group [" << groupId << "] took [" << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms] to forward message to [" << receivers->second.size() << "] peers.");
                            
                            for (size_t index : receiverToRemove)
                            {
                                std::vector<kt::TCPSocket>::iterator socketPosition = std::next(receivers->second.begin(), index);
                                LOG4CXX_INFO(logger, "[TCP - " + uuidString + "] - Group [" << groupId << "] - Closing and removing receiver socket with address [" << socketPosition->getSocketAddress().getAddress().value_or("") + ":" + std::to_string(socketPosition->getSocketAddress().getPortNumber()) << "].");
                                socketPosition->close();
                                receivers->second.erase(socketPosition);
                            }
                            receiverToRemove.clear();
                        }
                    }
                }

                for (size_t index : senderToRemove)
                {
                    std::vector<kt::TCPSocket>::iterator socketPosition = std::next(senderSockets->second.begin(), index);
                    LOG4CXX_INFO(logger, "[TCP] - Group [" << groupId << "] - Closing and removing sender socket with address [" << socketPosition->getSocketAddress().getAddress().value_or("") + ":" + std::to_string(socketPosition->getSocketAddress().getPortNumber()) << "].");
                    socketPosition->close();
                    senderSockets->second.erase(socketPosition);
                }
                senderToRemove.clear();
            }
        }

        LOG4CXX_INFO(logger, "[TCP] - Group [" + groupId + "] Stopping forwarder thread.");

        // Once we are out of the loop just run through and close all sockets in this group
        auto senderSockets = tcpSenderSockets.find(groupId);
        if (senderSockets != tcpSenderSockets.end())
        {
            for (kt::TCPSocket& socket : senderSockets->second)
            {
                socket.close();
            }
        }

        auto receiverSockets = tcpReceiverSockets.find(groupId);
        if (receiverSockets != tcpReceiverSockets.end())
        {
            for (kt::TCPSocket& socket : receiverSockets->second)
            {
                socket.close();
            }
        }
    }

    bool Forwarder::tcpGroupWithIdExists(std::string& groupId)
    {
        return tcpReceiverSockets.find(groupId) != tcpReceiverSockets.end() || tcpSenderSockets.find(groupId) != tcpSenderSockets.end();
    }

    size_t Forwarder::tcpGroupMemberCount(std::string& groupId)
    {
        size_t total = 0;
        auto receiverGroup = tcpReceiverSockets.find(groupId);
        if (receiverGroup != tcpReceiverSockets.end())
        {
            total += receiverGroup->second.size();
        }

        auto senderGroup = tcpSenderSockets.find(groupId);
        if (senderGroup != tcpSenderSockets.end())
        {
            total += senderGroup->second.size();
        }

        return total;
    }

    std::optional<std::pair<kt::UDPSocket, kt::UDPSocket>> Forwarder::initializeUdpSocketsForConnection(ForwarderGroup udpGroup)
    {
        try
        {
            kt::UDPSocket receiverSocket;
            receiverSocket.bind(config.getIpVersion(), config.getHostAddress(), udpGroup.receiverPort);
            try
            {
                kt::UDPSocket senderSocket;
                senderSocket.bind(config.getIpVersion(), config.getHostAddress(), udpGroup.senderPort);
                return std::make_pair(receiverSocket, senderSocket);
            }
            catch (const kt::SocketException e)
            {
                LOG4CXX_ERROR(logger, "Failed to bind to sender listener socket for UDP group [" + udpGroup.groupId + "] on port [" + std::to_string(udpGroup.senderPort) + "].");
            }
        }
        catch (const kt::SocketException e)
        {
            LOG4CXX_ERROR(logger, "Failed to bind to sender listener socket for UDP group [" + udpGroup.groupId + "] on port [" + std::to_string(udpGroup.senderPort) + "].");
        }
        return std::nullopt;
    }

    void Forwarder::startUDPForwarder()
    {
         for (const auto& udpConfig : config.getUdpGroups())
        {
            for (const kt::SocketAddress& address : udpConfig.preConfiguredAddresses)
            {
                try
                {
                    addAddressToReceiverUDPGroup(udpConfig.groupId, address);
                    LOG4CXX_INFO(logger, "Registered preconfigured address [" + address.getAddress().value_or("") + ":" + std::to_string(address.getPortNumber()) + "] in UDP group [" + udpConfig.groupId + "]");
                }
                catch(const kt::SocketException& e)
                {
                    LOG4CXX_WARN(logger, "Failed to connect to and register preconfigured address [" + address.getAddress().value_or("") + ":" + std::to_string(address.getPortNumber()) + "] in UDP group [" + udpConfig.groupId + "] with error: " + e.what());
                }
            }
            
            std::optional<std::pair<kt::UDPSocket, kt::UDPSocket>> sockets = initializeUdpSocketsForConnection(udpConfig);
            if (!sockets.has_value())
            {
                LOG4CXX_ERROR(logger, "Failed to initialize connection sockets for group [" + udpConfig.groupId + "] using receiving port [" + std::to_string(udpConfig.receiverPort) + "] and sending port [" + std::to_string(udpConfig.senderPort) + "]. This group will be unavailable.");
            }
            else
            {
                std::thread listeningThread(&Forwarder::startUDPListener, this, sockets.value(), udpConfig.groupId);
                udpRunningThreads.push_back(std::move(listeningThread));
                std::thread responderThread(&Forwarder::startUDPDataForwarder, this, udpConfig.groupId);
                udpRunningThreads.push_back(std::move(responderThread));
            }
        }
    }

    void Forwarder::startUDPListener(std::pair<kt::UDPSocket, kt::UDPSocket> sockets, std::string groupId)
    {
        auto& [receiverSocket, senderSocket] = sockets;

        LOG4CXX_INFO(logger, "[UDP] - Group [" + groupId + "] Starting UDP forwarder connection listener...");
        while (forwarderIsRunning)
        {
            if (receiverSocket.ready(1))
            {
                std::expected<std::pair<std::string, kt::SocketAddress>, int> result = receiverSocket.receiveFrom(config.getUdpMaxReadInSize());
                if (result)
                {
                    std::string addressString = result.value().second.getAddress().value_or("") + ":" + std::to_string(result.value().second.getPortNumber());
                    std::string message = result.value().first;
                    LOG4CXX_INFO(logger, "[UDP] - New client joined UDP group from address [" << addressString << "] with request reply port [" << message << "]");

                    kt::SocketAddress address = result.value().second;
                    addAddressToReceiverUDPGroup(groupId, address);
                }
            }

            if (senderSocket.ready(1))
            {
                std::expected<std::pair<std::string, kt::SocketAddress>, int> result = senderSocket.receiveFrom(config.getUdpMaxReadInSize());
                if (result)
                {
                    std::string addressString = result.value().second.getAddress().value_or("") + ":" + std::to_string(result.value().second.getPortNumber());
                    std::string& message = result.value().first;

                    LOG4CXX_DEBUG(logger, "[UDP] - Group [" + groupId + "] - Enqueuing received message of size [" << message.size() << "] from sender.");

                    auto queue = udpMessageQueue.find(groupId);
                    if (queue == udpMessageQueue.end())
                    {
                        std::queue<std::string> newQueue;
                        newQueue.push(message);
                        udpMessageQueue.emplace(std::make_pair(groupId, newQueue));
                    }
                    else
                    {
                        queue->second.push(message);
                    }
                }
            }
        }

        LOG4CXX_INFO(logger, "[UDP] - Group [" + groupId + "] Stopping listener thread.");

        receiverSocket.close();
        senderSocket.close();
    }

    void Forwarder::startUDPDataForwarder(std::string groupId)
    {
        LOG4CXX_INFO(logger, "[UDP] - Group [" + groupId + "] - Starting UDP data forwarder listener...");
        kt::UDPSocket sendSocket;
        while (forwarderIsRunning)
        {
            auto messageQueue = udpMessageQueue.find(groupId);
            if (messageQueue != udpMessageQueue.end() && !messageQueue->second.empty())
            {
                std::string uuidString = getNewUUID();
                std::string message = messageQueue->second.front();
                messageQueue->second.pop();

                LOG4CXX_DEBUG(logger, "[UDP - " + uuidString + "] Group [" + groupId + "] - Received message [" << message << "] forwarding to peers.");

                std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
                auto receivers = udpReceiverAddresses.find(groupId);
                if (receivers != udpReceiverAddresses.end())
                {
                    for (auto& addr : receivers->second)
                    {
                        int result = sendSocket.sendTo(addr, message);
                        LOG4CXX_DEBUG(logger, "[UDP - " + uuidString + "] Group [" + groupId + "] - Forwarded to peer with address: [" << addr.getAddress().value_or("") + ":" + std::to_string(addr.getPortNumber()) << "]. With result [" << result << "]");
                    }
                }

                size_t peerCount = receivers != udpReceiverAddresses.end() ? receivers->second.size() : 0;
                LOG4CXX_DEBUG(logger, "[UDP - " + uuidString + "] Group [" + groupId + "] - Forwarded to [" << peerCount << "] peer(s).");
                std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
                LOG4CXX_DEBUG(logger, "[UDP - " + uuidString + "] Group [" + groupId + "] - Took [" << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms] to forward message to [" << peerCount << "] peers.");
            }
            else
            {
                using namespace std::chrono_literals;
                std::this_thread::sleep_for(10us);
            }
        }

        LOG4CXX_INFO(logger, "[UDP] - Group [" + groupId + "] Stopping forwarder thread.");
    }

    size_t Forwarder::udpGroupMemberCount(std::string& groupId)
    {
        auto group = udpReceiverAddresses.find(groupId);
        if (group != udpReceiverAddresses.end())
        {
            return group->second.size();
        }

        return 0;
    }

    void Forwarder::stop()
    {
        forwarderIsRunning = false;
    }

    void Forwarder::join()
    {
        for (auto& t : tcpRunningThreads)
        {
            t.join();
        }

        for (auto& t : udpRunningThreads)
        {
            t.join();
        }
    }

    std::string getNewUUID()
    {
        uuid_t uuid{};
        char temp[100];
        uuid_generate(uuid);
        uuid_unparse(uuid, temp);
        return std::string(temp);
    }
}
