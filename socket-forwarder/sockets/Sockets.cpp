#include <optional>

#include <socketexceptions/SocketException.hpp>
#include <socketexceptions/BindingException.hpp>

#include "../environment/Environment.h"
#include "Sockets.h"

#include <log4cxx/logger.h>

namespace forwarder
{
    auto logger = log4cxx::Logger::getLogger("SocketForwarder");

    std::optional<kt::ServerSocket> setUpTcpServerSocket(std::optional<std::string> defaultPort)
    {
        std::optional<std::string> tcpPort = forwarder::getEnvironmentVariableValue(forwarder::TCP_PORT);

        if (!tcpPort.has_value() && !defaultPort.has_value())
        {
            LOG4CXX_INFO(logger, "Skipping TCP socket creation since value for [" + forwarder::TCP_PORT + "] was not provided.");
            return std::nullopt;
        }

        std::string portAsString = tcpPort.value_or(*defaultPort);
        const unsigned short portNumber = static_cast<unsigned short>(std::stoi(portAsString));

        try
        {
            kt::ServerSocket serverSocket(kt::SocketType::Wifi, getEnvironmentVariableValueOrDefault(HOST_ADDRESS, HOST_ADDRESS_DEFAULT), portNumber);
            return std::make_optional(serverSocket);
        }
        catch(const kt::BindingException e)
        {
            LOG4CXX_ERROR(logger, "[TCP] - Failed to bind server socket on port [" << portNumber << "]. " << e.what());
            return std::nullopt;
        }
        catch (const kt::SocketException e)
        {
            LOG4CXX_ERROR(logger, "[TCP] - Failed to create server socket: " << e.what());
            return std::nullopt;
        }
    }

    std::optional<kt::UDPSocket> setUpUDPSocket(std::optional<std::string> defaultPort)
    {
        std::optional<std::string> udpPort = forwarder::getEnvironmentVariableValue(forwarder::UDP_PORT);

        if (!udpPort.has_value() && !defaultPort.has_value())
        {
            LOG4CXX_INFO(logger, "Skipping UDP socket creation since value for [" + forwarder::UDP_PORT + "] was not provided.");
            return std::nullopt;
        }

        std::string portAsString = udpPort.value_or(*defaultPort);
        const unsigned short portNumber = static_cast<unsigned short>(std::stoi(portAsString));

        try
        {
            kt::UDPSocket udpSocket;
            if (!udpSocket.bind(getEnvironmentVariableValueOrDefault(HOST_ADDRESS, HOST_ADDRESS_DEFAULT), portNumber).first)
            {
                LOG4CXX_ERROR(logger, "[UDP] - Failed to bind to provided port [" << portNumber << "].");
                return std::nullopt;
            }
            return std::make_optional(udpSocket);
        }
        catch(const kt::BindingException e)
        {
            LOG4CXX_ERROR(logger, "[UDP] - Failed to bind UDP socket on port: [" << portNumber << "]. " << e.what());
            return std::nullopt;
        }
        catch (const kt::SocketException e)
        {
            LOG4CXX_ERROR(logger, "[UDP] - Failed to create UDP socket: " << e.what());
            return std::nullopt;
        }
    }

    /**
     * Expected format for TCP connections is "<groupID>:<address>:<port>,<groupID2>:<address2>:<port2>".
     */
    std::unordered_map<std::string, std::vector<kt::SocketAddress>> getPreconfiguredTCPAddresses(const std::string defaultValue)
    {
        std::unordered_map<std::string, std::vector<kt::SocketAddress>> addresses;
        std::vector<std::string> addressStrings = split(getEnvironmentVariableValueOrDefault(PRECONFIG_TCP_ADDRESSES, defaultValue), ",");
        
        for (const std::string& s : addressStrings)
        {
            std::vector<std::string> parts = split(s, ":");
            if (parts.size() == 1 && parts[0].empty())
            {
                LOG4CXX_WARN(logger, "[TCP] - Skipping processing address with value [" << s << "], expected format to be \"<groupId>:<address>:<port number>\".");
            }
            else if (parts.size() < 3)
            {
                LOG4CXX_INFO(logger, "[TCP] - Unable to add address [" << s << "], expected format to be \"<groupId>:<address>:<port number>\".");
            }
            else
            {
                if (parts.size() > 3)
                {
                    LOG4CXX_INFO(logger, "[TCP] - Multiple ':' provided in address string [" << s << "]. Attempting to parse and add address to group [" << parts[0] << "] using second and third elements as the address [" << parts[1] << ", " << parts[2] << "].");
                }

                unsigned short portNumber = static_cast<unsigned short>(std::atoi(parts[2].c_str()));
                addrinfo info = kt::createTcpHints();
                std::pair<std::vector<kt::SocketAddress>, int> resolvedAddresses = kt::resolveToAddresses(parts[1], portNumber, info);
                if (resolvedAddresses.first.empty())
                {
                    LOG4CXX_ERROR(logger, "[TCP] - Failed to resolve address [" << parts[1] << ":" << portNumber << "]. Address will not be added to TCP group [" << parts[0] << "].");
                }
                else
                {
                    kt::SocketAddress addr = resolvedAddresses.first.at(0);
                    LOG4CXX_INFO(logger, "[TCP] - Resolved and added pre-configured address [" << kt::getAddress(addr).value_or("") + ":" + std::to_string(kt::getPortNumber(addr)) << "] to group [" << parts[0] << "].");

                    if (addresses.find(parts[0]) == addresses.end())
                    {
                        std::vector<kt::SocketAddress> addrVec = { addr };
                        addresses.insert(std::make_pair(parts[0], addrVec));
                    }
                    else
                    {
                        addresses[parts[0]].push_back(addr);
                    }
                }
            }
        }

        return addresses;
    }

    /**
     * Expected format for UDP connections is "<address>:<port>,<address2>:<port2>".
     */
    std::vector<kt::SocketAddress> getPreconfiguredUDPAddresses(const std::string defaultValue)
    {
        std::vector<kt::SocketAddress> addresses;
        std::vector<std::string> addressStrings = split(getEnvironmentVariableValueOrDefault(PRECONFIG_UDP_ADDRESSES, defaultValue), ",");

        for (const std::string& s : addressStrings)
        {
            std::vector<std::string> parts = split(s, ":");
            if (parts.size() == 1 && parts[0].empty())
            {
                LOG4CXX_WARN(logger, "[UDP] - Skipping processing address with value [" << s << "], expected format to be \"<address>:<port number>\".");
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
}
