#include "Environment.h"

namespace forwarder
{
    std::optional<std::string> getEnvironmentVariableValue(std::string& environmentVariableKey)
    {
        char *val = std::getenv(environmentVariableKey.c_str());
        return val == nullptr ? std::nullopt : std::make_optional(std::string(val));
    }
}
