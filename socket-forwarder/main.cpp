#include <iostream>
#include <optional>
#include <cstdlib>
#include <string>
#include <thread>

#include "Sockets.h"
#include "environment/Environment.h"
#include "forwarder/Forwarder.h"

int main(int argc, char** argv)
{
    std::optional<kt::ServerSocket> serverSocket = forwarder::setUpTcpServerSocket(argc > 1 ? std::make_optional(std::string(argv[1])) : std::nullopt);
    std::optional<kt::UDPSocket> udpSocket = forwarder::setUpUDPSocket(argc > 2 ? std::make_optional(std::string(argv[2])) : std::nullopt);

    if (serverSocket.has_value())
    {
        std::cout << "Running TCP forwarder on port [" << serverSocket.value().getPort() << "]" << std::endl;
        std::pair<std::thread, std::thread> tcpRunningThreads = forwarder::startTCPForwarder(serverSocket.value());
        
        tcpRunningThreads.first.join();
        tcpRunningThreads.second.join();
        serverSocket.value().close();
    }
    else
    {
        std::cout << "No TCP socket was setup and listening since [" << forwarder::TCP_PORT << "] environment variable was not provided." << std::endl;
    }

    if (udpSocket.has_value() && udpSocket.value().getListeningPort().has_value())
    {        
        std::cout << "Running UDP forwarder on port [" << udpSocket.value().getListeningPort().value() << "]" << std::endl;

        
    }
    else
    {
        std::cout << "No UDP socket was setup and listening since [" << forwarder::UDP_PORT << "] environment variable was not provided." << std::endl;
    }
}
