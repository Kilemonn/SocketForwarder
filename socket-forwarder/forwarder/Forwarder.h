#pragma once

#include <string>
#include <thread>
#include <vector>

#include <serversocket/ServerSocket.h>
#include <socket/TCPSocket.h>
#include <socket/UDPSocket.h>

namespace forwarder
{
    std::pair<std::thread, std::thread> startTCPForwarder(kt::ServerSocket&, std::string, unsigned short, bool);
    void startTCPConnectionListener(kt::ServerSocket, std::string, unsigned short, bool);
    void startTCPDataForwarder(unsigned short, bool);
    bool tcpGroupWithIdExists(std::string&);
    size_t tcpGroupMemberCount(std::string&);

    std::pair<std::thread, std::thread> startUDPForwarder(kt::UDPSocket&, std::string, unsigned short, bool);
    void startUDPListener(kt::UDPSocket, std::string, unsigned short, bool);
    void startUDPDataForwarder(bool);
    size_t udpGroupMemberCount();
    
    void stopForwarder();

    std::string getNewUUID();
}
