#pragma once

#include <string>
#include <thread>
#include <vector>

#include <serversocket/ServerSocket.h>
#include <socket/TCPSocket.h>
#include <socket/UDPSocket.h>

namespace forwarder
{
    std::pair<std::thread, std::thread> startTCPForwarder(kt::ServerSocket&);
    void startTCPConnectionListener(kt::ServerSocket);
    void startTCPDataForwarder();
    bool tcpGroupWithIdExists(std::string&);

    std::thread startUDPForwarder(kt::UDPSocket&);
    void startUDPListener(kt::UDPSocket);
    size_t udpGroupMemberCount();
    
    void stopForwarder();
}
