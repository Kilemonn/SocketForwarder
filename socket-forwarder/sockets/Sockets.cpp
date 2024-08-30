#include <optional>

#include <socketexceptions/SocketException.hpp>
#include <socketexceptions/BindingException.hpp>

#include "../environment/Environment.h"
#include "Sockets.h"

namespace forwarder
{
    std::optional<kt::ServerSocket> setUpTcpServerSocket(std::optional<std::string> defaultPort)
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
            kt::ServerSocket serverSocket(kt::SocketType::Wifi, std::string("0.0.0.0"), portNumber);
            return std::make_optional(serverSocket);
        }
        catch(const kt::BindingException e)
        {
            std::cout << "[TCP] - Failed to bind server socket on port: " << portNumber << ". " << e.what() << std::endl;
            return std::nullopt;
        }
        catch (const kt::SocketException e)
        {
            std::cout << "[TCP] - Failed to create server socket: " << e.what() << std::endl;
            return std::nullopt;
        }
    }

    std::optional<kt::UDPSocket> setUpUDPSocket(std::optional<std::string> defaultPort)
    {
        std::optional<std::string> udpPort = forwarder::getEnvironmentVariableValue(forwarder::UDP_PORT);

        if (!udpPort.has_value() && !defaultPort.has_value())
        {
            return std::nullopt;
        }

        std::string portAsString = udpPort.has_value() ? udpPort.value() : defaultPort.value();
        const unsigned short portNumber = static_cast<unsigned short>(std::stoi(portAsString));

        try
        {
            kt::UDPSocket udpSocket;
            if (!udpSocket.bind(portNumber).first)
            {
                std::cout << "[UDP] - Failed to bind to provided port " << portNumber << "\n";
                return std::nullopt;
            }
            return std::make_optional(udpSocket);
        }
        catch(const kt::BindingException e)
        {
            std::cout << "[UDP] - Failed to bind UDP socket on port: " << portNumber << ". " << e.what() << std::endl;
            return std::nullopt;
        }
        catch (const kt::SocketException e)
        {
            std::cout << "[UDP] - Failed to create UDP socket: " << e.what() << std::endl;
            return std::nullopt;
        }
    }
}
