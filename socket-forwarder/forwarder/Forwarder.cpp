#include "Forwarder.h"
#include "../environment/Environment.h"

#include <thread>
#include <chrono>
#include <vector>

namespace forwarder
{
    std::unordered_map<std::string, std::vector<kt::TCPSocket>> tcpSessions;

    std::unordered_map<std::string, std::vector<kt::SocketAddress>> udpSessions;

    std::pair<std::thread, std::thread> startTCPForwarder(kt::ServerSocket& serverSocket)
    {
        // Start one thread constantly receiving new connections and adding them to their correct group
        std::thread listeningThread(startTCPConnectionListener, serverSocket);
        
        // Start a second thread that is constantly polling all TCPSockets in the session map and forwarding 
        // those messages to the other sockets in its group.
        std::thread dataForwarderThread(startTCPDataForwarder);

        return std::make_pair(std::move(listeningThread), std::move(dataForwarderThread));
    }

    void startTCPConnectionListener(kt::ServerSocket serverSocket)
    {
        while(true)
        {
            kt::TCPSocket socket = serverSocket.acceptTCPConnection();
            std::string firstMessage = socket.receiveAmount(MAX_READ_IN_DEFAULT);

            if (firstMessage.rfind(NEW_CLIENT_PREFIX_DEFAULT, 0))
            {
                std::string groupId = firstMessage.substr(NEW_CLIENT_PREFIX_DEFAULT.size() + 1);
                if (tcpSessions.find(groupId) == tcpSessions.end())
                {
                    // No existing sessions, create new
                    std::cout << "Creating new session [" << groupId << "]." << std::endl;
                    std::vector<kt::TCPSocket> sockets;
                    sockets.push_back(socket);
                    tcpSessions.insert(std::make_pair(groupId, sockets));
                }
                else
                {
                    std::vector<kt::TCPSocket> sockets = tcpSessions.at(groupId);
                    sockets.push_back(socket);
                }
            }
            else
            {
                // First message does not start with prefix, just close connection
                socket.close();
            }

            using namespace std::chrono_literals;
            std::this_thread::sleep_for(2ms);
        }
    }

    void startTCPDataForwarder()
    {
        while (true)
        {
            for (auto it = tcpSessions.begin(); it != tcpSessions.end(); ++it)
            {
                std::string groupID = it->first;
                for (size_t i = 0; i < it->second.size(); i++)
                {
                    kt::TCPSocket receiveSocket = it->second[i];
                    if (receiveSocket.ready())
                    {
                        std::string received = receiveSocket.receiveAmount(MAX_READ_IN_DEFAULT);
                        for (size_t j = 0; j < it->second.size(); j++)
                        {
                            if (j != i)
                            {
                                kt::TCPSocket forwardToSocket = it->second[j];
                                if (!forwardToSocket.send(received))
                                {
                                    std::cout << "Failed to forward message in group [" << groupID << "]." << std::endl;
                                }
                            }
                        }
                    }
                }
            }

            using namespace std::chrono_literals;
            std::this_thread::sleep_for(10ms);
        }
    }
}
