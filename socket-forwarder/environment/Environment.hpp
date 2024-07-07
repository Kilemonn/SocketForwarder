#pragma once

#include <string>
#include <optional>

namespace forwarder
{
    const std::string TCP_PORT = "TCP_PORT";
    const std::string UDP_PORT = "UDP_PORT";

    std::optional<std::string> getEnvironmentVariableValue(std::string environmentVariableKey)
    {
        char *val = std::getenv(environmentVariableKey.c_str());
        return val == nullptr ? std::nullopt : std::make_optional(std::string(val));
    }
}
