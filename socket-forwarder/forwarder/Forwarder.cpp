#include "Forwarder.h"
#include "../environment/Environment.h"

#include <socketexceptions/SocketException.hpp>
#include <socketexceptions/TimeoutException.hpp>

#include <thread>
#include <chrono>
#include <vector>

namespace forwarder
{
    std::unordered_map<std::string, std::vector<kt::TCPSocket>> tcpSessions;

    std::unordered_map<std::string, std::vector<kt::SocketAddress>> udpGroupToAddress;
    std::unordered_map<kt::SocketAddress, std::string> udpAddressToGroupId;

    bool forwarderIsRunning = true;

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
        while(forwarderIsRunning)
        {
            try
            {
                kt::TCPSocket socket = serverSocket.acceptTCPConnection(10000); // microseconds
                std::string firstMessage = socket.receiveAmount(MAX_READ_IN_DEFAULT);

                std::cout << "Accepted new connection and read: " << firstMessage << std::endl;

                if (firstMessage.rfind(NEW_CLIENT_PREFIX_DEFAULT, 0) == 0)
                {
                    std::string groupId = firstMessage.substr(NEW_CLIENT_PREFIX_DEFAULT.size());
                    if (tcpSessions.find(groupId) == tcpSessions.end())
                    {
                        std::cout << "GroupID " << groupId << " does not exist, creating new group." << std::endl;
                        // No existing sessions, create new
                        std::cout << "Creating new session [" << groupId << "]." << std::endl;
                        std::vector<kt::TCPSocket> sockets;
                        sockets.push_back(socket);
                        tcpSessions.insert(std::make_pair(groupId, sockets));
                    }
                    else
                    {
                        std::cout << "GroupID " << groupId << " exists! Adding new connection to group." << std::endl;
                        tcpSessions[groupId].push_back(socket);
                    }
                }
                else
                {
                    // First message does not start with prefix, just close connection
                    std::cout << "First message did not start with prefix: [" << NEW_CLIENT_PREFIX_DEFAULT << "]. Closing connection." << std::endl;
                    socket.close();
                }
            }
            catch(kt::TimeoutException e)
            {
                // Timeout occurred its fine
            }
            catch(kt::SocketException e)
            {
                std::cout << "Failed to accept incoming client: " << e.what() << std::endl;
            }
            
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(2ms);
        }

        // If we exit the loop, close the server socket
        serverSocket.close();
    }

    void startTCPDataForwarder()
    {
        while (forwarderIsRunning)
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
                        std::cout << "Group [" << groupID << "] with [" << it->second.size() << "] nodes. Received content [" << received << "] from peer [" << i << "] forwarding to other peers..." << std::endl;
                        for (size_t j = 0; j < it->second.size(); j++)
                        {
                            if (j != i)
                            {
                                kt::TCPSocket forwardToSocket = it->second[j];
                                if (!forwardToSocket.send(received))
                                {
                                    std::cout << "Failed to forward message in group [" << groupID << "]." << std::endl;
                                }
                                else
                                {
                                    std::cout << "Group [" << groupID << "], successfully forwarded to peer [" << j << "]" << std::endl;
                                }
                            }
                        }
                    }
                }
            }

            using namespace std::chrono_literals;
            std::this_thread::sleep_for(10ms);
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

    std::thread startUDPForwarder(kt::UDPSocket& udpSocket)
    {
        std::thread listeningThread(startUDPListener, udpSocket);
        return listeningThread;
    }

    void startUDPListener(kt::UDPSocket udpSocket)
    {
        while (forwarderIsRunning)
        {
            std::pair<std::optional<std::string>, std::pair<int, kt::SocketAddress>> result = udpSocket.receiveFrom(MAX_READ_IN_DEFAULT);
            
            if (result.second.first > 0 && result.first.has_value())
            {
                std::string message = result.first.value();
                kt::SocketAddress address = result.second.second;

                // Check if we have have received from this address already
                auto it = udpAddressToGroupId.find(address);
                if (it != udpAddressToGroupId.end())
                {
                    // Existing client
                    std::string groupId = it->second;
                    auto entry = udpGroupToAddress.find(groupId);
                    std::cout << "Received message from client we have seen before in group [" << groupId << "] with [" << entry->second.size() << "] node(s)." << std::endl;
                    for (kt::SocketAddress addr : entry->second)
                    {
                        if (std::memcmp(&addr, &address, sizeof(address)) != 0)
                        {
                            udpSocket.sendTo(message, addr, sizeof(addr));
                        }
                    }
                }
                else
                {
                    // This is a new client, check their first message content
                    if (message.rfind(NEW_CLIENT_PREFIX_DEFAULT, 0) == 0)
                    {
                        std::string groupId = message.substr(NEW_CLIENT_PREFIX.size());
                        std::cout << "New client requested to join group [" << groupId << "]" << std::endl;
                        
                        udpAddressToGroupId.insert({address, groupId});

                        if (udpGroupToAddress.find(groupId) != udpGroupToAddress.end())
                        {
                            std::cout << "Group [" << groupId << "] does not exist, creating new group." << std::endl;
                            
                            std::vector<kt::SocketAddress> addresses;
                            addresses.push_back(address);
                            udpGroupToAddress.insert(std::make_pair(groupId, addresses));
                        }
                        else
                        {
                            std::cout << "Group [" << groupId << "] exists! Adding client to the existing group." << std::endl;
                            udpGroupToAddress[groupId].push_back(address);
                        }
                    }
                    else
                    {
                        // No-Op, a client we have no seen before and their first message is not to join a group
                        std::cout << "Unseen client sent message that does not match new client prefix." << std::endl;
                    }
                }
            }
        }
    }

    bool udpGroupWithIdExists(std::string& groupId)
    {
        return udpGroupToAddress.find(groupId) != udpGroupToAddress.end();
    }

    void stopForwarder()
    {
        forwarderIsRunning = false;
    }
}
