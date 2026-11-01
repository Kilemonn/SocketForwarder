#include "Environment.h"

#include <algorithm>
#include <cctype>
#include <set>

#include <log4cxx/logger.h>

namespace forwarder
{
    auto logger = log4cxx::Logger::getLogger("Environment");

    std::optional<std::string> getEnvironmentVariableValue(std::string environmentVariableKey)
    {
        char *val = std::getenv(environmentVariableKey.c_str());
        return val == nullptr ? std::nullopt : std::make_optional(std::string(val));
    }

    std::string getEnvironmentVariableValueOrDefault(std::string environmentVariableKey, std::string defaultValue)
    {
        char *val = std::getenv(environmentVariableKey.c_str());
        return val == nullptr ? defaultValue : std::string(val);
    }

    std::vector<std::pair<std::string, std::string>> getVariablesWithPrefix(const std::string &prefix, char **envp)
    {
        std::vector<std::pair<std::string, std::string>> variables;
        for (size_t i = 0; envp[i] != nullptr; ++i)
        {
            std::string envVariable = envp[i];
            
            // Split key and value at the first '=' character
            size_t pos = envVariable.find('=');
            if (pos != std::string::npos)
            {
                std::string key = envVariable.substr(0, pos);
                std::string value = envVariable.substr(pos + 1);

                std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
                    return std::tolower(c);
                });

                if (key.starts_with(prefix))
                {
                    variables.push_back(std::make_pair(key, value));
                }
            }
        }
        return variables;
    }

    std::set<std::string> getGroupsWithPrefix(const std::string& prefix, std::vector<std::pair<std::string, std::string>> variables)
    {
        std::set<std::string> groups;
        for (const auto& [key, value] : variables)
        {
            if (key.starts_with(prefix))
            {
                size_t pos = key.find(".", prefix.size());
                if (pos != std::string::npos)
                {
                    groups.emplace(key.substr(prefix.size(), pos));
                }
            }
        }

        return groups;
    }

    std::vector<std::string> split(const std::string& input, const std::string& delimiter)
    {
        std::vector<std::string> tokens;
        size_t index = 0;

        while(index != -1)
        {
            size_t result = input.find(delimiter, index);
            if (result != -1)
            {
                tokens.push_back(input.substr(index, result - index));
                index = result + delimiter.size();
            }
            else
            {
                tokens.push_back(input.substr(index));
                index = -1;
            }
        }

        return tokens;
    }

    /**
     * Expected format for connections is "<address>:<port>,<address2>:<port2>".
     */
    std::vector<kt::SocketAddress> getPreconfiguredAddresses(const std::string& environmentVariable, const std::string defaultValue)
    {
        std::vector<kt::SocketAddress> addresses;
        std::vector<std::string> addressStrings = split(getEnvironmentVariableValueOrDefault(environmentVariable, defaultValue), ",");

        for (const std::string& s : addressStrings)
        {
            std::vector<std::string> parts = split(s, ":");
            if (parts.size() == 1 && parts[0].empty())
            {
                // Don't even log since its empty
            }
            else if (parts.size() < 2)
            {
                LOG4CXX_INFO(logger, "[UDP] - Unable to add address [" << s << "], expected format to be \"<address>:<port number>\".");
            }
            else
            {
                if (parts.size() > 2)
                {
                    LOG4CXX_INFO(logger, "[UDP] - Multiple ':' provided in address string [" << s << "]. Attempting to parse as address using first two elements [" << parts[0] << ", " << parts[1] << "].");
                }

                unsigned short portNumber = static_cast<unsigned short>(std::atoi(parts[1].c_str()));
                addrinfo info = kt::createUdpHints();
                std::pair<std::vector<kt::SocketAddress>, int> resolvedAddresses = kt::resolveToAddresses(parts[0], portNumber, info);
                if (resolvedAddresses.first.empty())
                {
                    LOG4CXX_ERROR(logger, "[UDP] - Failed to resolve address [" << parts[0] << ":" << portNumber << "]. Address will not be added to UDP group.");
                }
                else
                {
                    kt::SocketAddress addr = resolvedAddresses.first.at(0);
                    LOG4CXX_INFO(logger, "[UDP] - Resolved and added pre-configured address [" << kt::getAddress(addr).value_or("") + ":" + std::to_string(kt::getPortNumber(addr)) << "] to UDP group.");
                    addresses.push_back(addr);
                }
            }
        }

        return addresses;
    }

    std::vector<ForwarderGroup> getForwarderGroups(char** envp, const std::string& prefix)
    {
        std::vector<ForwarderGroup> groups;
        std::vector<std::pair<std::string, std::string>> variables = getVariablesWithPrefix(prefix, envp);
        std::set<std::string> groupNames = getGroupsWithPrefix(prefix, variables);

        for (const std::string& groupName : groupNames)
        {
            ForwarderGroup forwarderGroup{};
            forwarderGroup.groupId = groupName;
            std::vector<std::pair<std::string, std::string>> groupVariables = getVariablesWithPrefix(prefix + groupName + ".", envp);

            std::optional<std::string> senderPort = getEnvironmentVariableValue(prefix + groupName + "." + forwarder::SENDER_PREFIX + forwarder::PORT_PREFIX);
            if (!senderPort.has_value())
            {
                LOG4CXX_WARN(logger, "Failed to register group with Id [" + groupName + "] because [" + prefix + groupName + "." + forwarder::SENDER_PREFIX + forwarder::PORT_PREFIX + "] is not configured.");
                continue;
            }

            std::optional<std::string> receiverPort = getEnvironmentVariableValue(prefix + groupName + "." + forwarder::RECEIVER_PREFIX + forwarder::PORT_PREFIX);
            if (!receiverPort.has_value())
            {
                LOG4CXX_WARN(logger, "Failed to register group with Id [" + groupName + "] because [" + prefix + groupName + "." + forwarder::RECEIVER_PREFIX + forwarder::PORT_PREFIX + "] is not configured.");
                continue;
            }

            forwarderGroup.preConfiguredAddresses = getPreconfiguredAddresses(prefix + groupName + "." + forwarder::RECEIVER_PREFIX + forwarder::PRECONFIG_ADDRESSES_SUFFIX);

            forwarderGroup.senderPort = static_cast<unsigned short>(std::stoi(senderPort.value()));
            forwarderGroup.receiverPort = static_cast<unsigned short>(std::stoi(receiverPort.value()));

            // TODO: Check no other forwarder group is using these ports - this will fail on initialisation but we should validate and exit early

            groups.push_back(forwarderGroup);
        }
        
        return groups;
    }
}
