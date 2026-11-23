#pragma once

#include <string>
#include <thread>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>

#include <cstring> // for memcmp()

#include <serversocket/TCPServerSocket.h>
#include <socket/TCPSocket.h>
#include <socket/UDPSocket.h>

#include "ForwarderConfig.h"

#include <log4cxx/logger.h>

namespace forwarder
{

    class Forwarder
    {
    protected:
        // Group ID -> receiver sockets
        std::unordered_map<std::string, std::vector<kt::TCPSocket>> tcpReceiverSockets;
        // GroupId -> sender sockets
        std::unordered_map<std::string, std::vector<kt::TCPSocket>> tcpSenderSockets;

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

        // GroupID -> set of socket addresses
        std::unordered_map<std::string, std::unordered_set<kt::SocketAddress, AddressHash, AddressEqual>> udpReceiverAddresses;
        
        // GroupId -> queue of string messages
        std::unordered_map<std::string, std::queue<std::string>> udpMessageQueue;

        std::vector<std::thread> tcpRunningThreads;
        std::vector<std::thread> udpRunningThreads;

        log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("Forwarder");
        bool forwarderIsRunning = false;
        ForwarderConfig config;

        void startUDPForwarder();
        void startUDPListener(std::pair<kt::UDPSocket, kt::UDPSocket>, std::string);
        void startUDPDataForwarder(std::string);

        void startTCPForwarder();
        void startTCPConnectionListener(std::pair<kt::TCPServerSocket, kt::TCPServerSocket>, const std::string);
        void startTCPDataForwarder(std::string);

        std::optional<std::pair<kt::TCPServerSocket, kt::TCPServerSocket>> initializeConnectionAcceptingSocketsForGroup(ForwarderGroup);
        std::optional<std::pair<kt::UDPSocket, kt::UDPSocket>> initializeUdpSocketsForConnection(ForwarderGroup);

        void addSocketToTCPReceiverGroup(const std::string&, kt::TCPSocket);
        void addSocketToTCPSenderGroup(const std::string&, kt::TCPSocket);

    public:
        Forwarder(ForwarderConfig);

        void addAddressToReceiverUDPGroup(std::string, kt::SocketAddress);

        bool tcpGroupWithIdExists(std::string&);
        size_t tcpGroupMemberCount(std::string&);
        size_t udpGroupMemberCount(std::string&);
        
        void start();
        void join();
        void stop();
    };

    std::string getNewUUID();
}
