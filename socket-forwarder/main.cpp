#include <iostream>
#include <optional>
#include <cstdlib>
#include <string>
#include <thread>
#include <algorithm>
#include <csignal>
#include <functional>

#include "environment/Environment.h"
#include "forwarder/Forwarder.h"
#include "forwarder/ForwarderConfig.h"
#include "logger/Logger.h"


// Make sure version of built image matches
const std::string VERSION = "0.4.0";

std::function<void(int)> createShutdownHandler(forwarder::Forwarder& forwarder)
{
    return [&forwarder](int signal) { forwarder.stop(); forwarder.join(); };
}

std::function<void(int)> handlerFunction;

// A wrapper function to pass to std::signal()
void signalHandler(int signal)
{
    if (handlerFunction)
    {
        handlerFunction(signal);
    }
}

int main(int argc, char** argv, char* envp[])
{
    forwarder::initialiseConsoleLogger();

    auto logger = log4cxx::Logger::getLogger("main");

    LOG4CXX_INFO(logger, "Running SocketForwarder v" << VERSION);

    const unsigned short tcpMaxReadInSize = std::atoi(forwarder::getEnvironmentVariableValueOrDefault(forwarder::TCP_MAX_READ_IN_SIZE, std::to_string(forwarder::MAX_READ_IN_DEFAULT)).c_str());
    const unsigned short udpMaxReadInSize = std::atoi(forwarder::getEnvironmentVariableValueOrDefault(forwarder::UDP_MAX_READ_IN_SIZE, std::to_string(forwarder::MAX_READ_IN_DEFAULT)).c_str());

    const kt::InternetProtocolVersion ipVersion = forwarder::getHostIPVersion();
    const std::string hostAddress = forwarder::getEnvironmentVariableValueOrDefault(forwarder::HOST_ADDRESS, ipVersion == kt::InternetProtocolVersion::IPV6 ? forwarder::HOST_IPV6_ADDRESS_DEFAULT : forwarder::HOST_IPV4_ADDRESS_DEFAULT);
    LOG4CXX_INFO(logger, "Using max read in size for TCP: [" << tcpMaxReadInSize << "].");
    LOG4CXX_INFO(logger, "Using max read in size for UDP: [" << udpMaxReadInSize << "].");
    LOG4CXX_INFO(logger, "Binding to host address [" << hostAddress << "].");

    std::vector<forwarder::ForwarderGroup> tcpGroups = forwarder::getForwarderGroups(envp, forwarder::TCP_GROUP_PREFIX, true);
    std::vector<forwarder::ForwarderGroup> udpGroups = forwarder::getForwarderGroups(envp, forwarder::UDP_GROUP_PREFIX, false);

    forwarder::ForwarderConfig config(hostAddress, ipVersion, tcpMaxReadInSize, udpMaxReadInSize, tcpGroups, udpGroups);
    // Validate ports and atleast 1 group here?

    forwarder::Forwarder forwarder(config);

    handlerFunction = createShutdownHandler(forwarder);

    // We have to provide an actual function here, we cannot reference a std::function
    std::signal(SIGKILL, signalHandler);
    std::signal(SIGTERM, signalHandler);

    forwarder.start();
    forwarder.join();
}
