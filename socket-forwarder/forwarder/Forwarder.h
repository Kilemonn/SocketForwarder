#pragma once

#include <string>
#include <thread>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>

#include <serversocket/ServerSocket.h>
#include <socket/TCPSocket.h>
#include <socket/UDPSocket.h>

namespace forwarder
{
    class Forwarder
    {
    protected:
        std::unordered_map<std::string, std::vector<kt::TCPSocket>> tcpSessions;

        struct AddressHash
        {
            std::size_t operator()(const kt::SocketAddress& k) const
            {
                return std::hash<std::string>()(kt::getAddress(k).value_or("") + ":" + std::to_string(kt::getPortNumber(k)));
            }
        };

        struct AddressEqual
        {
            bool operator()(const kt::SocketAddress& lhs, const kt::SocketAddress& rhs) const
            {
                return std::memcmp(&lhs, &rhs, sizeof(lhs));
            }
        };

        // For UDP since we don't know who is sending specific messages from, ALL UDP connections will be treated as the same group
        std::unordered_set<kt::SocketAddress, AddressHash, AddressEqual> udpKnownPeers;
        std::queue<std::string> udpMessageQueue;

        std::optional<std::pair<std::thread, std::thread>> tcpRunningThreads = std::nullopt;
        std::optional<std::pair<std::thread, std::thread>> udpRunningThreads = std::nullopt;

        bool forwarderIsRunning = false;
        std::string newClientPrefix;
        unsigned short maxReadInSize;
        bool debug = false;

        std::optional<kt::UDPSocket> udpRecieveSocket = std::nullopt;
        std::optional<kt::ServerSocket> tcpServerSocket = std::nullopt;

        void startUDPForwarder();
        void startUDPListener();
        void startUDPDataForwarder();

        void startTCPForwarder();
        void startTCPConnectionListener();
        void startTCPDataForwarder();

        void addSocketToTCPGroup(const std::string&, kt::TCPSocket);

    public:
        Forwarder(std::optional<kt::ServerSocket>, std::optional<kt::UDPSocket>, const std::string, const unsigned short, const bool);

        void addAddressToTCPGroup(const std::string&, kt::SocketAddress);
        void addAddressToUDPGroup(kt::SocketAddress);

        bool tcpGroupWithIdExists(std::string&);
        size_t tcpGroupMemberCount(std::string&);
        size_t udpGroupMemberCount();
        
        void start();
        void join();
        void stop();
    };

    std::string getNewUUID();
}
