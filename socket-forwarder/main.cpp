#include <iostream>
#include <optional>
#include <cstdlib>
#include <string>
#include <thread>

#include <serversocket/ServerSocket.h>
#include <enums/SocketType.h>
#include <socket/UDPSocket.h>
#include <socketexceptions/BindingException.hpp>

#include "environment/Environment.h"
#include "forwarder/Forwarder.h"

std::optional<kt::ServerSocket> setUpTcpServerSocket(std::optional<std::string> defaultPort = std::nullopt)
{
    std::optional<std::string> tcpPort = forwarder::getEnvironmentVariableValue(forwarder::TCP_PORT);

    if (!tcpPort.has_value() && !defaultPort.has_value())
    {
        return std::nullopt;
    }

    std::string portAsString = tcpPort.has_value() ? tcpPort.value() : defaultPort.value();
    const unsigned short portNumber = static_cast<unsigned short>(std::stoi(portAsString));

    try
    {
        kt::ServerSocket serverSocket(kt::SocketType::Wifi, portNumber);
        return std::make_optional(serverSocket);
    }
    catch(const kt::BindingException e)
    {
        std::cout << "Failed to bind server socket on port: " << portNumber << ". " << e.what() << std::endl;
        return std::nullopt;
    }
    catch (const kt::SocketException e)
    {
        std::cout << "Failed to create server socket: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::optional<kt::UDPSocket> setUpUDPSocket()
{
    std::optional<std::string> udpPort = forwarder::getEnvironmentVariableValue(forwarder::UDP_PORT);

    if (!udpPort.has_value())
    {
        return std::nullopt;
    }

    return std::make_optional(kt::UDPSocket());
}

int main(int argc, char** argv)
{
    std::optional<kt::ServerSocket> serverSocket = setUpTcpServerSocket(argc > 1 ? std::make_optional(std::string(argv[1])) : std::nullopt);
    std::optional<kt::UDPSocket> udpSocket = setUpUDPSocket();

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

    if (udpSocket.has_value())
    {
        std::cout << "UDP Socket setup" << std::endl;
    }
    else
    {
        std::cout << "No UDP socket was setup and listening since [" << forwarder::UDP_PORT << "] environment variable was not provided." << std::endl;
    }
}
