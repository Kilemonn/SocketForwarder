#include "UDPForwarder.h"

#include <socketexceptions/SocketException.hpp>

namespace forwarder
{
    UDPForwarder::UDPForwarder(ForwarderConfig config): config(config)
    {

    }

    UDPForwarder::~UDPForwarder()
    {
        stop();
        join();
    }

    bool UDPForwarder::start()
    {
        if (!config.getUdpGroups().empty())
        {
            return startUDPForwarder();
        }
        else
        {
            LOG4CXX_INFO(logger, "[UDP] - No UDP groups were configured, UDP forwarding is disabled.");
            return false;
        }
    }

    void UDPForwarder::join()
    {
        if (!forwarderIsRunning)
        {
            return;
        }

        for (auto& thread : udpRunningThreads)
        {
            thread.join();
        }
    }

    void UDPForwarder::stop()
    {
        forwarderIsRunning = false;
    }

    bool UDPForwarder::groupWithIdExists(std::string& groupId)
    {
        return udpReceiverAddresses.find(groupId) != udpReceiverAddresses.end(); 
    }

    size_t UDPForwarder::groupCount(std::string& groupId)
    {
        auto group = udpReceiverAddresses.find(groupId);
        if (group != udpReceiverAddresses.end())
        {
            return group->second.size();
        }

        return 0;
    }

    void UDPForwarder::startUDPDataForwarder(std::string groupId, kt::UDPSocket sendSocket)
    {
        LOG4CXX_INFO(logger, "[UDP] - Group [" + groupId + "] - Starting UDP data forwarder listener...");
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

    std::optional<std::pair<kt::UDPSocket, kt::UDPSocket>> UDPForwarder::initializeUdpSocketsForConnection(ForwarderGroup udpGroup)
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

    bool UDPForwarder::startUDPForwarder()
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
                std::thread listeningThread(&UDPForwarder::startUDPListener, this, sockets.value(), udpConfig.groupId);
                udpRunningThreads.push_back(std::move(listeningThread));
                
                // Pass in the receiver socket, which will be re-used to send the replies back to the registered recievers.
                // Since we are re-using the same socket that the incoming message was initially recieved from underlying network
                // mechanisms make sure that the response sent is properly captured and returned. Especially when the connection is going through docker-proxy.
                std::thread responderThread(&UDPForwarder::startUDPDataForwarder, this, udpConfig.groupId, sockets.value().first);
                udpRunningThreads.push_back(std::move(responderThread));
            }
        }
        return true;
    }

    void UDPForwarder::startUDPListener(std::pair<kt::UDPSocket, kt::UDPSocket> sockets, std::string groupId)
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
                    LOG4CXX_INFO(logger, "[UDP] - New client joined UDP group from address [" << addressString << "].");

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

    void UDPForwarder::addAddressToReceiverUDPGroup(std::string groupId, kt::SocketAddress address)
    {
        auto group = udpReceiverAddresses.find(groupId);
        if (group != udpReceiverAddresses.end())
        {
            group->second.emplace(address);
        }
        else
        {
            std::unordered_set<kt::SocketAddress, forwarder::UDPForwarder::AddressHash, forwarder::UDPForwarder::AddressEqual> set;
            set.emplace(address);
            udpReceiverAddresses.emplace(groupId, set);
        }
    }
}
