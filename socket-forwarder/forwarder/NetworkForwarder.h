#pragma once

#include <string> // For std::string

namespace forwarder
{
    class NetworkForwarder
    {
    public:
        virtual ~NetworkForwarder() = default;

        virtual bool start() = 0;
        virtual void stop() = 0;
        virtual void join() = 0;

        std::string getNewUUID();
    };
}
