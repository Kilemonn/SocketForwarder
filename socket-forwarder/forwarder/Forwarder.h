#pragma once

#include <string>
#include <vector>

#include "ForwarderConfig.h"
#include "NetworkForwarder.h"
#include "TCPForwarder.h"
#include "UDPForwarder.h"

#include <log4cxx/logger.h>

namespace forwarder
{

    class Forwarder: public NetworkForwarder
    {
    protected:
        log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("Forwarder");
        
        TCPForwarder tcpForwarder;
        UDPForwarder udpForwarder;

    public:
        Forwarder(ForwarderConfig);

        bool tcpGroupExists(std::string&);
        size_t tcpGroupCount(std::string&);
        bool udpGroupExists(std::string&);
        size_t udpGroupCount(std::string&);
        
        bool start() override;
        void join() override;
        void stop() override;
    };
}
