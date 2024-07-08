#pragma once

#include <string>
#include <thread>
#include <vector>

#include <serversocket/ServerSocket.h>
#include <socket/TCPSocket.h>
#include <socket/UDPSocket.h>

namespace forwarder
{
    std::pair<std::thread, std::thread> startTCPForwarder(kt::ServerSocket&, std::string, unsigned short);
    void startTCPConnectionListener(kt::ServerSocket, std::string, unsigned short);
    void startTCPDataForwarder(unsigned short);
    bool tcpGroupWithIdExists(std::string&);

    std::thread startUDPForwarder(kt::UDPSocket&, std::string, unsigned short);
    void startUDPListener(kt::UDPSocket, std::string, unsigned short);
    size_t udpGroupMemberCount();
    
    void stopForwarder();
}
