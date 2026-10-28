#pragma once

#include <optional>
#include <unordered_map>
#include <vector>

#include <serversocket/TCPServerSocket.h>
#include <socket/UDPSocket.h>

namespace forwarder
{
    std::optional<kt::TCPServerSocket> setUpTcpServerSocket(std::optional<std::string> = std::nullopt);

    std::optional<kt::UDPSocket> setUpUDPSocket(std::optional<std::string> = std::nullopt);

    std::unordered_map<std::string, std::vector<kt::SocketAddress>> getPreconfiguredTCPAddresses(const std::string = "");

    std::vector<kt::SocketAddress> getPreconfiguredUDPAddresses(const std::string = "");

    std::vector<std::string> split(const std::string&, const std::string&);
}
