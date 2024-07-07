#include <iostream>
#include <optional>
#include <cstdlib>
#include <string>

#include <serversocket/ServerSocket.h>
#include <enums/SocketType.h>
#include <socket/UDPSocket.h>
#include <socketexceptions/BindingException.hpp>

#include "environment/Environment.hpp"

std::optional<kt::ServerSocket> setUpTcpServerSocket()
{
    std::optional<std::string> tcpPort = forwarder::getEnvironmentVariableValue(forwarder::TCP_PORT);

    if (!tcpPort.has_value())
    {
        return std::nullopt;
    }

    const unsigned short portNumber = static_cast<unsigned short>(std::stoi(tcpPort.value()));
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
    std::optional<kt::ServerSocket> serverSocket = setUpTcpServerSocket();
    std::optional<kt::UDPSocket> udpSocket = setUpUDPSocket();

    if (serverSocket.has_value())
    {
        std::cout << "TCP Socket setup" << std::endl;
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
