#pragma once

#include <string>
#include <vector>
#include <optional>

#include <address/SocketAddress.h>

namespace forwarder
{
    struct ForwarderGroup
    {
        unsigned short receiverPort;
        unsigned short senderPort;
        std::string groupId;
        std::vector<kt::SocketAddress> preConfiguredAddresses;
    };

    class ForwarderConfig
    {
        private:
            std::string hostAddress;
            kt::InternetProtocolVersion ipVersion;
            unsigned short tcpMaxReadInSize;
            unsigned short udpMaxReadInSize;
            std::vector<ForwarderGroup> tcpGroups;
            std::vector<ForwarderGroup> udpGroups;

            std::string conflictingPortsMessage(std::string, std::string, std::string, unsigned short, std::string, std::string, unsigned short);

        public:
            ForwarderConfig(std::string, kt::InternetProtocolVersion, unsigned short, unsigned short, std::vector<ForwarderGroup>, std::vector<ForwarderGroup>);
            std::string getHostAddress() const;
            kt::InternetProtocolVersion getIpVersion() const;
            unsigned short getTcpMaxReadInSize() const;
            unsigned short getUdpMaxReadInSize() const;
            std::vector<ForwarderGroup> getTcpGroups() const;
            std::vector<ForwarderGroup> getUdpGroups() const;

            std::optional<std::string> validate();
    };
}
