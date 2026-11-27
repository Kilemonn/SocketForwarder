#pragma once

#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <cstring> // for memcmp()
#include <thread>

#include "ForwarderConfig.h"
#include "NetworkForwarder.h"

#include <socket/UDPSocket.h>

#include <log4cxx/logger.h>

namespace forwarder
{
    class UDPForwarder: public NetworkForwarder
    {
    private:
        struct AddressHash
        {
            std::size_t operator()(const kt::SocketAddress& k) const
            {
                return std::hash<std::string>()(k.getAddress().value_or("") + ":" + std::to_string(k.getPortNumber()));
            }
        };

        struct AddressEqual
        {
            bool operator()(const kt::SocketAddress& lhs, const kt::SocketAddress& rhs) const
            {
                return std::memcmp(&lhs, &rhs, sizeof(lhs));
            }
        };

    protected:
        log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("UDPForwarder");

        ForwarderConfig config;
        bool forwarderIsRunning = false;

        // GroupID -> set of socket addresses
        std::unordered_map<std::string, std::unordered_set<kt::SocketAddress, AddressHash, AddressEqual>> udpReceiverAddresses;
        
        // GroupId -> queue of string messages
        std::unordered_map<std::string, std::queue<std::string>> udpMessageQueue;

        std::vector<std::thread> udpRunningThreads;

        bool startUDPForwarder();
        void startUDPListener(std::pair<kt::UDPSocket, kt::UDPSocket>, std::string);
        void startUDPDataForwarder(std::string, kt::UDPSocket);

        std::optional<std::pair<kt::UDPSocket, kt::UDPSocket>> initializeUdpSocketsForConnection(ForwarderGroup);

        void addAddressToReceiverUDPGroup(std::string, kt::SocketAddress);
    public:
        UDPForwarder(ForwarderConfig);
        UDPForwarder& operator=(const UDPForwarder&) = delete;
        ~UDPForwarder();

        bool start() override;
        void join() override;
        void stop() override;

        bool groupWithIdExists(std::string&);
        size_t groupCount(std::string&);
    };
}
