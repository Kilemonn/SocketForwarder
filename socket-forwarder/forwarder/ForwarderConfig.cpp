#include "ForwarderConfig.h"

namespace forwarder
{
    ForwarderConfig::ForwarderConfig(std::string hostAddress, kt::InternetProtocolVersion ipVersion, unsigned short tcpMaxReadInSize, unsigned short udpMaxReadInSize, std::vector<ForwarderGroup> tcpGroups, std::vector<ForwarderGroup> udpGroups): 
        hostAddress(hostAddress), ipVersion(ipVersion), tcpMaxReadInSize(tcpMaxReadInSize), udpMaxReadInSize(udpMaxReadInSize), tcpGroups(tcpGroups), udpGroups(udpGroups)
    {
    }

    std::string ForwarderConfig::getHostAddress() const
    {
        return hostAddress;
    }

    kt::InternetProtocolVersion ForwarderConfig::getIpVersion() const
    {
        return ipVersion;
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

    std::optional<std::string> ForwarderConfig::validate()
    {
        // We don't need to check group IDs are unique since the environment properties guarantee this
        // We don't need to check that atleast 1 group is defined because the forwarder will just not run

        // Check ports are all unique per configuration, make sure no two configurations are using any of the same ports
        for (int i = 0; i < tcpGroups.size(); i++)
        {
            const ForwarderGroup& tcpGroup = tcpGroups[i];
            for (int j = i + 1; j < tcpGroups.size(); j++)
            {
                const ForwarderGroup& group = tcpGroups[j];
                if (tcpGroup.receiverPort == group.receiverPort)
                {
                    return conflictingPortsMessage("receiver", tcpGroup.groupId, tcpGroup.receiverPort, "receiver", group.groupId, group.receiverPort);
                }
                else if (tcpGroup.receiverPort == group.senderPort)
                {
                    return conflictingPortsMessage("receiver", tcpGroup.groupId, tcpGroup.receiverPort, "sender", group.groupId, group.senderPort);
                }
                else if (tcpGroup.senderPort == group.receiverPort)
                {
                    return conflictingPortsMessage("sender", tcpGroup.groupId, tcpGroup.senderPort, "receiver", group.groupId, group.receiverPort);
                }
                else if (tcpGroup.senderPort == group.senderPort)
                {
                    return conflictingPortsMessage("sender", tcpGroup.groupId, tcpGroup.senderPort, "sender", group.groupId, group.senderPort);
                }
            }

            for (const ForwarderGroup& udpGroup : udpGroups)
            {
                if (tcpGroup.receiverPort == udpGroup.receiverPort)
                {
                    return conflictingPortsMessage("receiver", tcpGroup.groupId, tcpGroup.receiverPort, "receiver", udpGroup.groupId, udpGroup.receiverPort);
                }
                else if (tcpGroup.receiverPort == udpGroup.senderPort)
                {
                    return conflictingPortsMessage("receiver", tcpGroup.groupId, tcpGroup.receiverPort, "sender", udpGroup.groupId, udpGroup.senderPort);
                }
                else if (tcpGroup.senderPort == udpGroup.receiverPort)
                {
                    return conflictingPortsMessage("sender", tcpGroup.groupId, tcpGroup.senderPort, "receiver", udpGroup.groupId, udpGroup.receiverPort);
                }
                else if (tcpGroup.senderPort == udpGroup.senderPort)
                {
                    return conflictingPortsMessage("sender", tcpGroup.groupId, tcpGroup.senderPort, "sender", udpGroup.groupId, udpGroup.senderPort);
                }
            }
        }

        return std::nullopt;
    }

    std::string ForwarderConfig::conflictingPortsMessage(std::string port1Type, std::string port1GroupId, unsigned short port1, std::string port2Type, std::string port2GroupId, unsigned short port2)
    {
        return "Conflicting ports detected for " + port1Type + " port for group " + port1GroupId + " [" + std::to_string(port1) + "] with " 
            + port2Type + " port for group " + port2GroupId + " [" + std::to_string(port2) + "].";
    }
}
