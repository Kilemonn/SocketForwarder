#include "Forwarder.h"
#include "../environment/Environment.h"

#include <socketexceptions/SocketException.hpp>
#include <socketexceptions/TimeoutException.hpp>

#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <chrono>
#include <vector>
#include <queue>

namespace forwarder
{
    std::unordered_map<std::string, std::vector<kt::TCPSocket>> tcpSessions;

    struct AddressHash
    {
        std::size_t operator()(const kt::SocketAddress& k) const
        {
            return std::hash<std::string>()(kt::getAddress(k).value() + ":" + std::to_string(kt::getPortNumber(k)));
        }
    };
 
    struct AddressEqual
    {
        bool operator()(const kt::SocketAddress& lhs, const kt::SocketAddress& rhs) const
        {
            return std::memcmp(&lhs, &rhs, sizeof(lhs));
        }
    };

    // For UDP since we don't know who is sending specific messages from, ALL UDP connections will be treated as the same group
    std::unordered_set<kt::SocketAddress, AddressHash, AddressEqual> udpKnownPeers;
    std::queue<std::string> udpMessageQueue;

    bool forwarderIsRunning;

    std::pair<std::thread, std::thread> startTCPForwarder(kt::ServerSocket& serverSocket, std::string newClientPrefix, unsigned short maxReadInSize)
    {
        forwarderIsRunning = true;

        // Start one thread constantly receiving new connections and adding them to their correct group
        std::thread listeningThread(startTCPConnectionListener, serverSocket, newClientPrefix, maxReadInSize);
        
        // Start a second thread that is constantly polling all TCPSockets in the session map and forwarding 
        // those messages to the other sockets in its group.
        std::thread dataForwarderThread(startTCPDataForwarder, maxReadInSize);

        return std::make_pair(std::move(listeningThread), std::move(dataForwarderThread));
    }

    void startTCPConnectionListener(kt::ServerSocket serverSocket, std::string newClientPrefix, unsigned short maxReadInSize)
    {
        std::cout << "[TCP] - Starting TCP connection listener..." << std::endl;
        while(forwarderIsRunning)
        {
            try
            {
                kt::TCPSocket socket = serverSocket.acceptTCPConnection(10000); // microseconds
                std::string firstMessage = socket.receiveAmount(maxReadInSize);

                std::cout << "[TCP] - Accepted new connection and read: " << firstMessage << "\n";

                if (firstMessage.rfind(newClientPrefix, 0) == 0)
                {
                    std::string groupId = firstMessage.substr(newClientPrefix.size());
                    if (tcpSessions.find(groupId) == tcpSessions.end())
                    {
                        std::cout << "[TCP] - Group [" << groupId << "] does not exist, creating new group.\n";
                        // No existing sessions, create new
                        std::vector<kt::TCPSocket> sockets;
                        sockets.push_back(socket);
                        tcpSessions.insert(std::make_pair(groupId, sockets));
                    }
                    else
                    {
                        std::cout << "[TCP] - Group [" << groupId << "] exists! Adding new connection to group.\n";
                        tcpSessions[groupId].push_back(socket);
                    }
                }
                else
                {
                    // First message does not start with prefix, just close connection
                    std::cout << "[TCP] - First message from TCP connection did not start with prefix: [" << newClientPrefix << "]. Closing connection.\n";
                    socket.close();
                }
            }
            catch(kt::TimeoutException e)
            {
                // Timeout occurred its fine
            }
            catch(kt::SocketException e)
            {
                std::cout << "[TCP] - Failed to accept incoming client: " << e.what() << std::endl;
            }
        }

        // If we exit the loop, close the server socket
        serverSocket.close();
    }

    void startTCPDataForwarder(unsigned short maxReadInSize)
    {
        std::cout << "[TCP] - Starting TCP forwarder listener..." << std::endl;
        while (forwarderIsRunning)
        {
            std::vector<size_t> toRemove;
            for (auto it = tcpSessions.begin(); it != tcpSessions.end(); ++it)
            {
                std::string groupID = it->first;
                for (size_t i = 0; i < it->second.size(); i++)
                {
                    kt::TCPSocket receiveSocket = it->second[i];
                    if (receiveSocket.ready(1))
                    {
                        std::string received = receiveSocket.receiveAmount(maxReadInSize);
                        if (received.empty())
                        {
                            continue;
                        }
                        std::cout << "[TCP] - Group [" << groupID << "] with [" << it->second.size() << "] nodes. Received content [" << received << "] from peer [" << i << "] forwarding to other peers...\n";
                        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
                        for (size_t j = 0; j < it->second.size(); j++)
                        {
                            if (j != i)
                            {
                                kt::TCPSocket forwardToSocket = it->second[j];
                                if (!forwardToSocket.connected())
                                {
                                    std::cout << "[TCP] - Group [" << groupID << "], peer [" << j << "] is no longer connected, removing from group.\n";
                                    toRemove.push_back(j);
                                }
                                else
                                {
                                    if (forwardToSocket.send(received, MSG_NOSIGNAL).first)
                                    {
                                        // TODO For debug
                                        // std::cout << "[TCP] - Group [" << groupID << "], successfully forwarded to peer [" << j << "]\n";
                                    }
                                    else
                                    {
                                        std::cout << "[TCP] - Group [" << groupID << "], failed to send to peer [" << j << "], removing from group.\n";
                                        toRemove.push_back(j);
                                    }
                                }
                            }
                        }
                        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
                        std::cout << "[TCP] - Group [" << groupID << "] took [" << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms] to forward message to [" << it->second.size() - 1 << "] peers.\n";

                        for (size_t index : toRemove)
                        {
                            std::vector<kt::TCPSocket>::iterator socketPosition = std::next(it->second.begin(), index);
                            socketPosition->close();
                            it->second.erase(socketPosition);
                        }
                        toRemove.clear();
                    }
                }
            }
        }

        // Once we are out of the loop just run through and close everything
        for (auto it = tcpSessions.begin(); it != tcpSessions.end(); ++it)
        {
            for (kt::TCPSocket socket : it->second)
            {
                socket.close();
            }
        }
        tcpSessions.clear();
    }

    bool tcpGroupWithIdExists(std::string& groupId)
    {
        return tcpSessions.find(groupId) != tcpSessions.end();
    }

    size_t tcpGroupMemberCount(std::string& groupId)
    {
        auto group = tcpSessions.find(groupId);
        if (group != tcpSessions.end())
        {
            return group->second.size();
        }
        return 0;
    }

    std::pair<std::thread, std::thread> startUDPForwarder(kt::UDPSocket& udpSocket, std::string newClientPrefix, unsigned short maxReadInSize)
    {
        forwarderIsRunning = true;

        std::thread listeningThread(startUDPListener, udpSocket, newClientPrefix, maxReadInSize);
        std::thread responderThread(startUDPDataForwarder);
        return std::make_pair(std::move(listeningThread), std::move(responderThread));
    }

    void startUDPListener(kt::UDPSocket udpSocket, std::string newClientPrefix, unsigned short maxReadInSize)
    {
        std::cout << "[UDP] - Starting UDP forwarder connection listener..." << std::endl;
        while (forwarderIsRunning)
        {
            std::pair<std::optional<std::string>, std::pair<int, kt::SocketAddress>> result = udpSocket.receiveFrom(maxReadInSize);
            
            if (result.second.first > 0 && result.first.has_value())
            {
                // TODO Add as "DEBUG" flag
                // std::cout << "[UDP] - Read in successful with result " << result.second.first << " data: " << (result.first.has_value() ? result.first.value() : "") << "\n";

                std::string message = result.first.value();
                kt::SocketAddress address = result.second.second;

                std::cout << "[UDP] - Received message [" << message << "] from address: " << kt::getAddress(address).value() << ":" << kt::getPortNumber(address) << "\n";

                // This is a new client, check their first message content
                if (message.rfind(newClientPrefix, 0) == 0)
                {
                    std::string recievingPort = message.substr(newClientPrefix.size());
                    std::cout << "[UDP] - New client requested to joined UDP group from address [" << kt::getAddress(address).value() << ":" << kt::getPortNumber(address) << "] with request reply port [" << recievingPort << "]\n";

                    address.ipv4.sin_port = htons(std::atoi(recievingPort.c_str()));
                    udpKnownPeers.emplace(address);
                }
                else
                {
                    udpMessageQueue.push(message);
                }
            }
        }
        udpSocket.close();
    }

    void startUDPDataForwarder()
    {
        std::cout << "[UDP] - Starting UDP data forwarder listener..." << std::endl;
        kt::UDPSocket sendSocket;
        while (forwarderIsRunning)
        {
            if (!udpMessageQueue.empty())
            {
                std::string message = udpMessageQueue.front();
                udpMessageQueue.pop();
                
                // TODO Debug
                // std::cout << "[UDP] - Received message [" << message << "] forwarding to peers.\n";
                    
                std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
                for (kt::SocketAddress addr : udpKnownPeers)
                {
                    std::pair<bool, int> result = sendSocket.sendTo(message, addr);
                    // TODO: For debug
                    // std::cout << "Forwarded to peer with address: " << kt::getAddress(addr).value() << ":" << kt::getPortNumber(addr) << ". With result " << result.second << "\n";
                }
                std::cout << "[UDP] - Forwarded to [" << udpKnownPeers.size() << "] peer(s).\n";
                std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
                std::cout << "[UDP] - Took [" << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms] to forward message to [" << udpKnownPeers.size() << "] peers.\n";
            }
            else
            {
                using namespace std::chrono_literals;
                std::this_thread::sleep_for(10us);
            }
        }

        udpKnownPeers.clear();
    }

    size_t udpGroupMemberCount()
    {
        return udpKnownPeers.size();
    }

    void stopForwarder()
    {
        forwarderIsRunning = false;
    }
}
