#pragma once

#include <unordered_map>
#include <thread>
#include <optional>

#include "NetworkForwarder.h"
#include "ForwarderConfig.h"

#include <serversocket/TCPServerSocket.h>
#include <socket/TCPSocket.h>

#include <log4cxx/logger.h>

namespace forwarder
{
    class TCPForwarder: public NetworkForwarder
    {
    protected:
        log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("TCPForwarder");

        bool forwarderIsRunning = false;

        // Group ID -> receiver sockets
        std::unordered_map<std::string, std::vector<kt::TCPSocket>> tcpReceiverSockets;
        // GroupId -> sender sockets
        std::unordered_map<std::string, std::vector<kt::TCPSocket>> tcpSenderSockets;

        std::vector<std::thread> tcpRunningThreads;

        ForwarderConfig config;

        bool startForwarder();
        std::optional<std::pair<kt::TCPServerSocket, kt::TCPServerSocket>> initializeConnectionAcceptingSocketsForGroup(ForwarderGroup);
        void startTCPDataForwarder(const std::string);
        void startTCPConnectionListener(std::pair<kt::TCPServerSocket, kt::TCPServerSocket>, const std::string);

        void addSocketToTCPReceiverGroup(const std::string&, kt::TCPSocket);
        void addSocketToTCPSenderGroup(const std::string&, kt::TCPSocket);

    public:
        TCPForwarder(ForwarderConfig);
        TCPForwarder& operator=(const TCPForwarder&) = delete;
        ~TCPForwarder();

        bool start() override;
        void join() override;
        void stop() override;

        bool groupWithIdExists(std::string&);
        size_t groupCount(std::string&);
    };
}