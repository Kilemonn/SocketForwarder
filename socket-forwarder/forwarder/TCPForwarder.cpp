#include "TCPForwarder.h"

#include <socketexceptions/SocketException.hpp>
#include <socketexceptions/TimeoutException.hpp>

namespace forwarder
{
    TCPForwarder::TCPForwarder(ForwarderConfig config): config(config)
    {
        
    }

    TCPForwarder::~TCPForwarder()
    {
        stop();
        join();
    }

    bool TCPForwarder::start()
    {
        if (!config.getTcpGroups().empty())
        {
            forwarderIsRunning = true;
            return startForwarder();
        }
        else
        {
            LOG4CXX_INFO(logger, "No TCP groups were configured, TCP forwarding is disabled.");
            return false;
        }
    }

    bool TCPForwarder::startForwarder()
    {
        for (const auto& tcpConfig : config.getTcpGroups())
        {
            for (const kt::SocketAddress& address : tcpConfig.preConfiguredAddresses)
            {
                try
                {
                    kt::TCPSocket preConfigured(address);
                    addSocketToTCPReceiverGroup(tcpConfig.groupId, preConfigured);
                    LOG4CXX_INFO(logger, "[" + tcpConfig.groupId + "] Registered preconfigured address [" + address.getAddress().value_or("") + ":" + std::to_string(address.getPortNumber()) + "].");
                }
                catch(const kt::SocketException& e)
                {
                    LOG4CXX_WARN(logger, "[" + tcpConfig.groupId + "] Failed to connect to and register preconfigured address [" + address.getAddress().value_or("") + ":" + std::to_string(address.getPortNumber()) + "] with error: " + e.what());
                }
            }
            
            std::optional<std::pair<kt::TCPServerSocket, kt::TCPServerSocket>> sockets = initializeConnectionAcceptingSocketsForGroup(tcpConfig);
            if (!sockets.has_value())
            {
                LOG4CXX_ERROR(logger, "[" + tcpConfig.groupId + "] Failed to initialize TCP connection sockets using receiving port [" + std::to_string(tcpConfig.receiverPort) + "] and sending port [" + std::to_string(tcpConfig.senderPort) + "]. This group will be unavailable.");
            }
            else
            {
                std::thread listeningThread(&TCPForwarder::startTCPConnectionListener, this, sockets.value(), tcpConfig.groupId);
                tcpRunningThreads.push_back(std::move(listeningThread));

                std::thread dataForwarderThread(&TCPForwarder::startTCPDataForwarder, this, tcpConfig.groupId);
                tcpRunningThreads.push_back(std::move(dataForwarderThread));
            }
        }

        return true;
    }

    std::optional<std::pair<kt::TCPServerSocket, kt::TCPServerSocket>> TCPForwarder::initializeConnectionAcceptingSocketsForGroup(ForwarderGroup group)
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
                LOG4CXX_ERROR(logger, "[" + group.groupId + "] Failed to bind to sender listener socket on port [" + std::to_string(group.senderPort) + "].");
            }
        }
        catch(const kt::SocketException e)
        {
            LOG4CXX_ERROR(logger, "[" + group.groupId + "] Failed to bind to receiver listener socket on port [" + std::to_string(group.receiverPort) + "].");
        }

        return std::nullopt;
    }

    void TCPForwarder::startTCPConnectionListener(std::pair<kt::TCPServerSocket, kt::TCPServerSocket> sockets, const std::string groupId)
    {
        auto& [receiverSocket, senderSocket] = sockets;

        LOG4CXX_INFO(logger, "[" + groupId + "] Starting TCP connection listener...");
        while(forwarderIsRunning)
        {
            // Accept from receiver socket
            try
            {
                kt::TCPSocket socket = receiverSocket.accept(10000); // microseconds
                std::string addressString = socket.getSocketAddress().getAddress().value_or("") + ":" + std::to_string(socket.getSocketAddress().getPortNumber());

                LOG4CXX_INFO(logger, "[" + groupId + "] Accepted new receiver connection from [" << addressString << "].");

                addSocketToTCPReceiverGroup(groupId, socket);
            }
            catch(kt::TimeoutException e)
            {
                // Timeout occurred its fine
            }
            catch(kt::SocketException e)
            {
                LOG4CXX_ERROR(logger, "[" + groupId + "] Failed to accept incoming receiver client: " << e.what());
            }

            // Accept from sender socket
            try
            {
                kt::TCPSocket socket = senderSocket.accept(10000);
                std::string addressString = socket.getSocketAddress().getAddress().value_or("") + ":" + std::to_string(socket.getSocketAddress().getPortNumber());

                LOG4CXX_INFO(logger, "[" + groupId + "] Accepted new sender connection from [" << addressString << "].");
                addSocketToTCPSenderGroup(groupId, socket);
            }
            catch(kt::TimeoutException e)
            {
                // Timeout occurred its fine
            }
            catch(kt::SocketException e)
            {
                LOG4CXX_ERROR(logger, "[" + groupId + "] Failed to accept incoming sender client: " << e.what());
            }
        }

        LOG4CXX_INFO(logger, "[" << groupId << "] Stopping listener thread.");

        // If we exit the loop, close the server sockets
        receiverSocket.close();
        senderSocket.close();
    }

    void TCPForwarder::startTCPDataForwarder(const std::string groupId)
    {
        LOG4CXX_INFO(logger, "[" + groupId + "] Starting TCP forwarder listener...");
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
                            LOG4CXX_DEBUG(logger, "[" + groupId + " - " + uuidString + "] Group with [" << received.size() << "] nodes. Received content [" << received << "] forwarding to receiver peers...");
                            
                            std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
                            auto receivers = tcpReceiverSockets.find(groupId);
                            if (receivers == tcpReceiverSockets.end())
                            {
                                LOG4CXX_DEBUG(logger, "[" + groupId + " - " + uuidString + "] No receiver sockets registered, dropping message.");
                                continue;
                            }

                            for (auto receiver = receivers->second.begin(); receiver != receivers->second.end(); ++receiver)
                            {
                                const kt::TCPSocket& forwardToSocket = *receiver;
                                size_t distance = std::distance(receivers->second.begin(), receiver);
                                std::string addressString = forwardToSocket.getSocketAddress().getAddress().value_or("") + ":" + std::to_string(forwardToSocket.getSocketAddress().getPortNumber());
                                if (!forwardToSocket.connected())
                                {
                                    
                                    LOG4CXX_DEBUG(logger, "[" + groupId + " - " + uuidString + "] Peer [" << addressString << "] is no longer connected, marking for removal from group.");
                                    receiverToRemove.push_back(distance);
                                }
                                else
                                {
                                    if (forwardToSocket.send(received, MSG_NOSIGNAL) > 0)
                                    {
                                        LOG4CXX_DEBUG(logger, "[" + groupId + " - " + uuidString + "] Successfully forwarded to peer [" << addressString << "]");
                                    }
                                    else
                                    {
                                        LOG4CXX_INFO(logger, "[" + groupId + " - " + uuidString + "] Failed to send to peer [" << addressString << "], marking for removal from group.");
                                        receiverToRemove.push_back(distance);
                                    }
                                }
                            }

                            std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
                            LOG4CXX_DEBUG(logger, "[" + groupId + " - " + uuidString + "] Took [" << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms] to forward message to [" << receivers->second.size() << "] peers.");
                            
                            for (size_t index : receiverToRemove)
                            {
                                std::vector<kt::TCPSocket>::iterator socketPosition = std::next(receivers->second.begin(), index);
                                std::string address = socketPosition->getSocketAddress().getAddress().value_or("") + ":" + std::to_string(socketPosition->getSocketAddress().getPortNumber());
                                LOG4CXX_INFO(logger, "[" + groupId + " - " + uuidString + "] Closing and removing receiver socket with address [" << address << "].");
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
                    std::string address = socketPosition->getSocketAddress().getAddress().value_or("") + ":" + std::to_string(socketPosition->getSocketAddress().getPortNumber());
                    LOG4CXX_INFO(logger, "[" << groupId << "] Closing and removing sender socket with address [" << address << "].");
                    socketPosition->close();
                    senderSockets->second.erase(socketPosition);
                }
                senderToRemove.clear();
            }
        }

        LOG4CXX_INFO(logger, "[" << groupId << "] Stopping forwarder thread.");

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

    void TCPForwarder::join()
    {
        for (auto& thread : tcpRunningThreads)
        {
            if (thread.joinable())
            {
                thread.join();
            }
        }
        tcpRunningThreads.clear();
    }

    void TCPForwarder::stop()
    {
        forwarderIsRunning = false;
    }

    bool TCPForwarder::groupWithIdExists(std::string& groupId)
    {
        return tcpReceiverSockets.find(groupId) != tcpReceiverSockets.end() || tcpSenderSockets.find(groupId) != tcpSenderSockets.end();
    }

    size_t TCPForwarder::groupCount(std::string& groupId)
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

    void TCPForwarder::addSocketToTCPReceiverGroup(const std::string &groupId, kt::TCPSocket socket)
    {
        std::string addressString = socket.getSocketAddress().getAddress().value_or("") + ":" + std::to_string(socket.getSocketAddress().getPortNumber());

        if (tcpReceiverSockets.find(groupId) == tcpReceiverSockets.end())
        {
            LOG4CXX_INFO(logger, "[" << groupId << "] Creating new group, adding address [" << addressString << "] to receiver group.");
            // No existing groups with this ID, creating new
            std::vector<kt::TCPSocket> sockets = { socket };
            tcpReceiverSockets.insert(std::make_pair(groupId, sockets));
        }
        else
        {
            LOG4CXX_DEBUG(logger, "[" << groupId << "] Adding new connection [" << addressString << "] to receiver group.");
            tcpReceiverSockets[groupId].push_back(socket);
        }
    }

    void TCPForwarder::addSocketToTCPSenderGroup(const std::string& groupId, kt::TCPSocket socket)
    {
        std::string addressString = socket.getSocketAddress().getAddress().value_or("") + ":" + std::to_string(socket.getSocketAddress().getPortNumber());

        if (tcpSenderSockets.find(groupId) == tcpSenderSockets.end())
        {
            LOG4CXX_INFO(logger, "[" << groupId << "] Creating new group, adding address [" << addressString << "] to sender group.");
            // No existing groups with this ID, creating new
            std::vector<kt::TCPSocket> sockets = { socket };
            tcpSenderSockets.insert(std::make_pair(groupId, sockets));
        }
        else
        {
            LOG4CXX_DEBUG(logger, "[" << groupId << "] Adding new sender connection [" << addressString << "] to group.");
            tcpSenderSockets[groupId].push_back(socket);
        }
    }
}
