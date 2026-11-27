#include "Forwarder.h"
#include "../environment/Environment.h"

#include <socketexceptions/SocketException.hpp>
#include <socketexceptions/BindingException.hpp>
#include <socketexceptions/TimeoutException.hpp>

#include <thread>
#include <chrono>
#include <vector>

namespace forwarder
{
    Forwarder::Forwarder(ForwarderConfig config): tcpForwarder(config), udpForwarder(config)
    {
        // Register preconfigured addresses for specific groups
    }

    bool Forwarder::udpGroupExists(std::string& groupId)
    {
        return udpForwarder.groupWithIdExists(groupId);
    }

    bool Forwarder::start()
    {
        bool tcpStarted = tcpForwarder.start();
        bool udpStarted = udpForwarder.start();

        return tcpStarted || udpStarted;
    }

    bool Forwarder::tcpGroupExists(std::string& groupId)
    {
        return tcpForwarder.groupWithIdExists(groupId);
    }

    size_t Forwarder::tcpGroupCount(std::string& groupId)
    {
        return tcpForwarder.groupCount(groupId);
    }

    size_t Forwarder::udpGroupCount(std::string &groupId)
    {
        return udpForwarder.groupCount(groupId);
    }

    void Forwarder::stop()
    {
        tcpForwarder.stop();
        udpForwarder.stop();
    }

    void Forwarder::join()
    {
        tcpForwarder.join();
        udpForwarder.join();
    }
}
