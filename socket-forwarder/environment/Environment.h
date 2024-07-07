#pragma once

#include <string>
#include <optional>

namespace forwarder
{
    const std::string TCP_PORT = "TCP_PORT";
    const std::string UDP_PORT = "UDP_PORT";
    const std::string NEW_CLIENT_PREFIX = "NEW_CLIENT_PREFIX";
    const std::string MAX_READ_IN_SIZE = "MAX_READ_IN_SIZE";

    const std::string NEW_CLIENT_PREFIX_DEFAULT = "SOCKETFORWARDER-NEW:";
    const unsigned short MAX_READ_IN_DEFAULT = 10640;

    std::optional<std::string> getEnvironmentVariableValue(std::string);
}
