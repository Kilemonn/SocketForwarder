#pragma once

#include <string>
#include <optional>

namespace forwarder
{
    const std::string SOCKET_FORWARDER_PREFIX = "socketforwarder.";
    const std::string HOST_ADDRESS = SOCKET_FORWARDER_PREFIX + "host_address";
    const std::string NEW_CLIENT_PREFIX = SOCKET_FORWARDER_PREFIX + "new_client_prefix";
    const std::string MAX_READ_IN_SIZE = SOCKET_FORWARDER_PREFIX + "max_read_in_size";
    const std::string DEBUG = SOCKET_FORWARDER_PREFIX + "debug";

    const std::string PRECONFIG_ADDRESSES_SUFFIX = "preconfig_addresses";
    const std::string PORT_SUFFIX = "port";

    const std::string TCP = "tcp.";
    const std::string TCP_PORT = SOCKET_FORWARDER_PREFIX + TCP + PORT_SUFFIX;
    const std::string PRECONFIG_TCP_ADDRESSES = SOCKET_FORWARDER_PREFIX + TCP + PRECONFIG_ADDRESSES_SUFFIX;
    
    const std::string UDP = "udp.";
    const std::string UDP_PORT = SOCKET_FORWARDER_PREFIX + UDP + PORT_SUFFIX;
    const std::string PRECONFIG_UDP_ADDRESSES = SOCKET_FORWARDER_PREFIX + UDP + PRECONFIG_ADDRESSES_SUFFIX;

    const std::string NEW_CLIENT_PREFIX_DEFAULT = "SOCKETFORWARDER-NEW:";
    const unsigned short MAX_READ_IN_DEFAULT = 10240;
    const std::string HOST_ADDRESS_DEFAULT = "0.0.0.0";

    std::optional<std::string> getEnvironmentVariableValue(std::string);

    std::string getEnvironmentVariableValueOrDefault(std::string, std::string);
}
