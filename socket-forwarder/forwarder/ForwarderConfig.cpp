#include "ForwarderConfig.h"

namespace forwarder
{
    ForwarderConfig::ForwarderConfig(std::string hostAddress, unsigned short tcpMaxReadInSize, unsigned short udpMaxReadInSize, std::vector<ForwarderGroup> tcpGroups, std::vector<ForwarderGroup> udpGroups): 
        hostAddress(hostAddress), tcpMaxReadInSize(tcpMaxReadInSize), udpMaxReadInSize(udpMaxReadInSize), tcpGroups(tcpGroups), udpGroups(udpGroups)
    {
    }

    std::string ForwarderConfig::getHostAddress() const
    {
        return hostAddress;
    }

    unsigned short ForwarderConfig::getTcpMaxReadInSize() const
    {
        return tcpMaxReadInSize;
    }

    unsigned short ForwarderConfig::getUdpMaxReadInSize() const
    {
        return udpMaxReadInSize;
    }

    std::vector<ForwarderGroup> ForwarderConfig::getTcpGroups() const
    {
        return tcpGroups;
    }

    std::vector<ForwarderGroup> ForwarderConfig::getUdpGroups() const
    {
        return udpGroups;
    }
}
