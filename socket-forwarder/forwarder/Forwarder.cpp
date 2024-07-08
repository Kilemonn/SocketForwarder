#include "Forwarder.h"
#include "../environment/Environment.h"

#include <socketexceptions/SocketException.hpp>
#include <socketexceptions/TimeoutException.hpp>

#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <chrono>
#include <vector>

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
    // std::unordered_set<kt::SocketAddress, AddressHash, AddressEqual> udpKnownPeers;
    std::unordered_set<std::string> udpKnownPeers;

    bool forwarderIsRunning;

    std::pair<std::thread, std::thread> startTCPForwarder(kt::ServerSocket& serverSocket)
    {
        forwarderIsRunning = true;

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

                std::cout << "Accepted new connection and read: " << firstMessage << "\n";

                if (firstMessage.rfind(NEW_CLIENT_PREFIX_DEFAULT, 0) == 0)
                {
                    std::string groupId = firstMessage.substr(NEW_CLIENT_PREFIX_DEFAULT.size());
                    if (tcpSessions.find(groupId) == tcpSessions.end())
                    {
                        std::cout << "GroupID " << groupId << " does not exist, creating new group.\n";
                        // No existing sessions, create new
                        std::cout << "Creating new session [" << groupId << "].\n";
                        std::vector<kt::TCPSocket> sockets;
                        sockets.push_back(socket);
                        tcpSessions.insert(std::make_pair(groupId, sockets));
                    }
                    else
                    {
                        std::cout << "GroupID " << groupId << " exists! Adding new connection to group.\n";
                        tcpSessions[groupId].push_back(socket);
                    }
                }
                else
                {
                    // First message does not start with prefix, just close connection
                    std::cout << "First message did not start with prefix: [" << NEW_CLIENT_PREFIX_DEFAULT << "]. Closing connection.\n";
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
        }

        // If we exit the loop, close the server socket
        serverSocket.close();
    }

    void startTCPDataForwarder()
    {
        std::cout << "Starting TCP forwarder listener..." << std::endl;
        while (forwarderIsRunning)
        {
            for (auto it = tcpSessions.begin(); it != tcpSessions.end(); ++it)
            {
                std::string groupID = it->first;
                for (size_t i = 0; i < it->second.size(); i++)
                {
                    kt::TCPSocket receiveSocket = it->second[i];
                    if (receiveSocket.ready(1))
                    {
                        std::string received = receiveSocket.receiveAmount(MAX_READ_IN_DEFAULT);
                        if (received.empty())
                        {
                            continue;
                        }
                        std::cout << "Group [" << groupID << "] with [" << it->second.size() << "] nodes. Received content [" << received << "] from peer [" << i << "] forwarding to other peers...\n";
                        for (size_t j = 0; j < it->second.size(); j++)
                        {
                            if (j != i)
                            {
                                kt::TCPSocket forwardToSocket = it->second[j];
                                if (!forwardToSocket.send(received))
                                {
                                    std::cout << "Failed to forward message in group [" << groupID << "].\n";
                                }
                                else
                                {
                                    std::cout << "Group [" << groupID << "], successfully forwarded to peer [" << j << "]\n";
                                }
                            }
                        }
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

    std::thread startUDPForwarder(kt::UDPSocket& udpSocket)
    {
        forwarderIsRunning = true;

        std::thread listeningThread(startUDPListener, udpSocket);
        return listeningThread;
    }

    void startUDPListener(kt::UDPSocket udpSocket)
    {
        std::cout << "Starting UDP forwarder listener..." << std::endl;
        while (forwarderIsRunning)
        {
            std::pair<std::optional<std::string>, std::pair<int, kt::SocketAddress>> result = udpSocket.receiveFrom(MAX_READ_IN_DEFAULT);
            
            if (result.second.first > 0 && result.first.has_value())
            {
                std::cout << "UDP - Read in successful with result " << result.second.first << " data: " << (result.first.has_value() ? result.first.value() : "") << "\n";

                std::string message = result.first.value();
                kt::SocketAddress address = result.second.second;

                std::cout << "Received message [" << message << "] from address: " << kt::getAddress(address).value() << ":" << kt::getPortNumber(address) << "\n";

                // This is a new client, check their first message content
                if (message.rfind(NEW_CLIENT_PREFIX_DEFAULT, 0) == 0)
                {
                    std::string recievingPort = message.substr(NEW_CLIENT_PREFIX_DEFAULT.size());
                    std::cout << "New client requested to joined UDP group with port [" << recievingPort << "]\n";

                    address.ipv4.sin_port = htons(std::atoi(recievingPort.c_str()));
                    addrinfo hints{};
                    hints.ai_family = AF_UNSPEC;
                    hints.ai_socktype = SOCK_DGRAM;
                    hints.ai_protocol = IPPROTO_UDP;
                    kt::SocketAddress resolvedAddress = kt::resolveToAddresses(kt::getAddress(address).value(), std::atoi(recievingPort.c_str()), hints).first[0];

                    std::cout << "Resolved address with port to: " << kt::getAddress(resolvedAddress).value() << ":" << kt::getPortNumber(resolvedAddress) << "\n";
                    // udpKnownPeers.emplace(resolvedAddress);
                    udpKnownPeers.emplace(kt::getAddress(resolvedAddress).value() + ":" + std::to_string(kt::getPortNumber(resolvedAddress)));
                }
                else
                {
                    std::cout << "UDP - Received message [" << message << "] forwarding to peers.\n";
                
                    // for (kt::SocketAddress addr : udpKnownPeers)
                    for (std::string addr : udpKnownPeers)
                    {
                        // std::pair<bool, int> result = udpSocket.sendTo(message, addr, sizeof(addr));
                        // std::cout << "Forwarded to peer with address: " << kt::getAddress(addr).value() << ":" << kt::getPortNumber(addr) << ". With result " << result.second << "\n";
                    
                        std::string hostname = addr.substr(0, addr.find_first_of(':'));
                        std::string port = addr.substr(addr.find_first_of(':') + 1);
                        std::pair<bool, std::pair<int, kt::SocketAddress>> result = udpSocket.sendTo(hostname, std::atoi(port.c_str()), message);
                        std::cout << "Forwarded to peer with address: " << hostname << " : " << port << ". With result " << result.second.first << "\n";
                    }
                }
            }
            else
            {
                std::cout << "UDP - Read in skipped with result" << result.second.first << "\n";
            }
        }
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
