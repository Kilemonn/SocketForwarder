#pragma once

#include <optional>

#include <serversocket/ServerSocket.h>
#include <socket/UDPSocket.h>

namespace forwarder
{
    std::optional<kt::ServerSocket> setUpTcpServerSocket(std::optional<std::string> = std::nullopt);

    std::optional<kt::UDPSocket> setUpUDPSocket(std::optional<std::string> = std::nullopt);
}
