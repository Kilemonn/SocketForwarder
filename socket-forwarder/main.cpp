#include <iostream>
#include <optional>
#include <cstdlib>
#include <string>
#include <thread>
#include <algorithm>

#include <csignal>

#include "sockets/Sockets.h"
#include "environment/Environment.h"
#include "forwarder/Forwarder.h"

// Make sure version of built image matches
const std::string VERSION = "0.2.1";

int main(int argc, char** argv)
{
    // TODO: Add env arg to provide a list of UDP clients that are already part of the UDP group on start up

    std::cout << "Running SocketForwarder v" << VERSION << std::endl;

    const std::string newClientPrefix = forwarder::getEnvironmentVariableValueOrDefault(forwarder::NEW_CLIENT_PREFIX, forwarder::NEW_CLIENT_PREFIX_DEFAULT);
    const unsigned short maxReadInSize = std::atoi(forwarder::getEnvironmentVariableValueOrDefault(forwarder::MAX_READ_IN_SIZE, std::to_string(forwarder::MAX_READ_IN_DEFAULT)).c_str());
    const bool debug = forwarder::getEnvironmentVariableValue(forwarder::DEBUG).has_value();

    std::cout << "Using new client prefix: [" << newClientPrefix << "].\n" 
        << "Using max read in size: [" << maxReadInSize << "].\n"
        << "DEBUG flag set to [" << debug << "]." << std::endl;

    std::optional<kt::ServerSocket> serverSocket = forwarder::setUpTcpServerSocket(argc > 1 ? std::make_optional(std::string(argv[1])) : std::nullopt);
    std::optional<kt::UDPSocket> udpSocket = forwarder::setUpUDPSocket(argc > 2 ? std::make_optional(std::string(argv[2])) : std::nullopt);

    forwarder::Forwarder forwarder(serverSocket, udpSocket, newClientPrefix, maxReadInSize, debug);

    forwarder.start();
    forwarder.join();
}
