#pragma once

#include <string>
#include <optional>
#include <vector>
#include <set>
#include "../forwarder/ForwarderConfig.h"

#include <enums/InternetProtocolVersion.h>

namespace forwarder
{
    const std::string IP_VERSION = "ip_version";

    const std::string SOCKET_FORWARDER_PREFIX = "socketforwarder.";

    // `socketforwarder.host_address`
    const std::string HOST_ADDRESS = SOCKET_FORWARDER_PREFIX + "host_address";

    // `socketforwarder.ip_version`
    const std::string HOST_IP_VERSION = SOCKET_FORWARDER_PREFIX + IP_VERSION;

    const std::string GROUP_PREFIX = "group.";
    const std::string RECEIVER_PREFIX = "reciever.";
    const std::string SENDER_PREFIX = "sender.";

    // `socketforwarder.tcp.group.<my-group>.reciever.port`
    // `socketforwarder.tcp.group.<my-group>.sender.port`
    // `socketforwarder.tcp.group.<my-group>.reciever.preconfig_addresses`

    // `socketforwarder.udp.group.<my-group>.reciever.port`
    // `socketforwarder.udp.group.<my-group>.sender.port`
    // `socketforwarder.udp.group.<my-group>.reciever.preconfig_addresses`

    const std::string PRECONFIG_ADDRESSES_SUFFIX = "preconfig_addresses";
    const std::string MAX_READ_IN_SIZE = "max_read_in_size";
    const std::string PORT = "port";

    const std::string TCP = "tcp.";
    const std::string TCP_GROUP_PREFIX = SOCKET_FORWARDER_PREFIX + TCP + GROUP_PREFIX;
    const std::string TCP_MAX_READ_IN_SIZE = SOCKET_FORWARDER_PREFIX + TCP + MAX_READ_IN_SIZE;
    
    const std::string UDP = "udp.";
    const std::string UDP_GROUP_PREFIX = SOCKET_FORWARDER_PREFIX + UDP + GROUP_PREFIX;
    const std::string UDP_MAX_READ_IN_SIZE = SOCKET_FORWARDER_PREFIX + UDP + MAX_READ_IN_SIZE;

    const unsigned short MAX_READ_IN_DEFAULT = 10240;
    const std::string HOST_IPV4_ADDRESS_DEFAULT = "0.0.0.0";
    const std::string HOST_IPV6_ADDRESS_DEFAULT = "::";

    std::optional<std::string> getEnvironmentVariableValue(std::string);

    std::string getEnvironmentVariableValueOrDefault(std::string, std::string);

    kt::InternetProtocolVersion getHostIPVersion();

    std::vector<std::pair<std::string, std::string>> getVariablesWithPrefix(const std::string&, char**);

    std::vector<ForwarderGroup> getForwarderGroups(char**, const std::string&, const bool);

    std::vector<kt::SocketAddress> getPreconfiguredAddresses(const std::string&, const std::string, const bool);

    std::set<std::string> getGroupsWithPrefix(const std::string&, std::vector<std::pair<std::string, std::string>>);

    std::vector<std::string> split(const std::string&, const std::string&);
}
