#include <iostream>
#include <optional>
#include <cstdlib>
#include <string>
#include <thread>
#include <algorithm>

#include "sockets/Sockets.h"
#include "environment/Environment.h"
#include "forwarder/Forwarder.h"
#include "logger/Logger.h"


// Make sure version of built image matches
const std::string VERSION = "0.4.0";

int main(int argc, char** argv)
{
    forwarder::initialiseConsoleLogger();

    auto logger = log4cxx::Logger::getLogger("main");

    LOG4CXX_INFO(logger, "Running SocketForwarder v" << VERSION);

    const std::string newClientPrefix = forwarder::getEnvironmentVariableValueOrDefault(forwarder::NEW_CLIENT_PREFIX, forwarder::NEW_CLIENT_PREFIX_DEFAULT);
    const unsigned short maxReadInSize = std::atoi(forwarder::getEnvironmentVariableValueOrDefault(forwarder::MAX_READ_IN_SIZE, std::to_string(forwarder::MAX_READ_IN_DEFAULT)).c_str());

    LOG4CXX_INFO(logger, "Using new client prefix: [" << newClientPrefix << "].");
    LOG4CXX_INFO(logger, "Using max read in size: [" << maxReadInSize << "].");
    LOG4CXX_INFO(logger, "Binding to host address [" << forwarder::getEnvironmentVariableValueOrDefault(forwarder::HOST_ADDRESS, forwarder::HOST_ADDRESS_DEFAULT) << "].");

    std::optional<kt::ServerSocket> serverSocket = forwarder::setUpTcpServerSocket(argc > 1 ? std::make_optional(std::string(argv[1])) : std::nullopt);
    std::optional<kt::UDPSocket> udpSocket = forwarder::setUpUDPSocket(argc > 2 ? std::make_optional(std::string(argv[2])) : std::nullopt);

    forwarder::Forwarder forwarder(serverSocket, udpSocket, newClientPrefix, maxReadInSize);

    std::vector<kt::SocketAddress> udpPreconfiguredAddresses = forwarder::getPreconfiguredUDPAddresses();
    if (!udpPreconfiguredAddresses.empty())
    {
        LOG4CXX_INFO(logger, "UDP preconfigured addresses provided, setting into forwarder...");
        for (const kt::SocketAddress& addr : udpPreconfiguredAddresses)
        {
            forwarder.addAddressToUDPGroup(addr);
        }
    }

    std::unordered_map<std::string, std::vector<kt::SocketAddress>> tcpPreconfiguredAddresses = forwarder::getPreconfiguredTCPAddresses();
    if (!tcpPreconfiguredAddresses.empty())
    {
        LOG4CXX_INFO(logger, "TCP preconfigured addresses provided, setting into forwarder...");
        for (const auto& it : tcpPreconfiguredAddresses)
        {
            for (const kt::SocketAddress& addr : it.second)
            {
                forwarder.preConfigureTCPAddress(it.first, addr);
            }
        }
    }

    forwarder.start();
    forwarder.join();
}
