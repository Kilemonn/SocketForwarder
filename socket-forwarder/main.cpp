#include <iostream>
#include <optional>
#include <cstdlib>
#include <string>
#include <thread>
#include <algorithm>

#include <csignal>

#include "sockets/Sockets.h"
#include "environment/Environment.h"
#include "forwarder/Forwarder.h"

// Make sure version of built image matches
const std::string VERSION = "0.2.1";

int main(int argc, char** argv)
{
    // TODO: Add env arg to provide a list of UDP clients that are already part of the UDP group on start up

    std::cout << "Running SocketForwarder v" << VERSION << std::endl;

    const std::string newClientPrefix = forwarder::getEnvironmentVariableValueOrDefault(forwarder::NEW_CLIENT_PREFIX, forwarder::NEW_CLIENT_PREFIX_DEFAULT);
    const unsigned short maxReadInSize = std::atoi(forwarder::getEnvironmentVariableValueOrDefault(forwarder::MAX_READ_IN_SIZE, std::to_string(forwarder::MAX_READ_IN_DEFAULT)).c_str());
    const bool debug = forwarder::getEnvironmentVariableValue(forwarder::DEBUG).has_value();

    std::cout << "Using new client prefix: [" << newClientPrefix << "].\n" 
        << "Using max read in size: [" << maxReadInSize << "].\n"
        << "DEBUG flag set to [" << debug << "]." << std::endl;

    std::optional<kt::ServerSocket> serverSocket = forwarder::setUpTcpServerSocket(argc > 1 ? std::make_optional(std::string(argv[1])) : std::nullopt);
    std::optional<kt::UDPSocket> udpSocket = forwarder::setUpUDPSocket(argc > 2 ? std::make_optional(std::string(argv[2])) : std::nullopt);

    std::optional<std::pair<std::thread, std::thread>> tcpRunningThreads = std::nullopt;
    std::optional<std::pair<std::thread, std::thread>> udpRunningThreads = std::nullopt;

    if (serverSocket.has_value())
    {
        std::cout << "[TCP] - Running TCP forwarder on port [" << serverSocket.value().getPort() << "]" << std::endl;
        tcpRunningThreads = forwarder::startTCPForwarder(serverSocket.value(), newClientPrefix, maxReadInSize, debug);
    }
    else
    {
        std::cout << "[TCP] - No TCP socket was setup and listening since [" << forwarder::TCP_PORT << "] environment variable was not provided." << std::endl;
    }

    if (udpSocket.has_value() && udpSocket.value().getListeningPort().has_value())
    {        
        std::cout << "[UDP] - Running UDP forwarder on port [" << udpSocket.value().getListeningPort().value() << "]" << std::endl;

        udpRunningThreads = forwarder::startUDPForwarder(udpSocket.value(), newClientPrefix, maxReadInSize, debug);
    }
    else
    {
        std::cout << "[UDP] - No UDP socket was setup and listening since [" << forwarder::UDP_PORT << "] environment variable was not provided." << std::endl;
    }

    if (tcpRunningThreads.has_value())
    {
        tcpRunningThreads.value().first.join();
        tcpRunningThreads.value().second.join();
    }

    if (udpRunningThreads.has_value())
    {
        udpRunningThreads.value().first.join();
        udpRunningThreads.value().second.join();
    }
}
