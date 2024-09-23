#include "Forwarder.h"
#include "../environment/Environment.h"

#include <socketexceptions/SocketException.hpp>
#include <socketexceptions/TimeoutException.hpp>

#include <thread>
#include <chrono>
#include <vector>

#include <uuid/uuid.h>

namespace forwarder
{
    Forwarder::Forwarder(std::optional<kt::ServerSocket> tcpSocket, std::optional<kt::UDPSocket> udpSocket, const std::string prefix, const unsigned short maxRead, const bool debugFlag):
        tcpServerSocket(tcpSocket), udpRecieveSocket(udpSocket), newClientPrefix(prefix), maxReadInSize(maxRead), debug(debugFlag)
    { }

    void Forwarder::addAddressToTCPGroup(const std::string& groupId, kt::SocketAddress address)
    {
        try
        {
            kt::TCPSocket socket(address);
            addSocketToTCPGroup(groupId, socket);
        }
        catch(kt::SocketException ex)
        {
            std::string addressString = kt::getAddress(address).value_or("") + ":" + std::to_string(kt::getPortNumber(address));
            std::cout << "[TCP] - Failed to add address [" << addressString << "], failed to connect to the provided address. Error [" << ex.what() << "]." << std::endl;
        }
    }

    void Forwarder::addSocketToTCPGroup(const std::string& groupId, kt::TCPSocket socket)
    {
        std::string addressString = kt::getAddress(socket.getSocketAddress()).value_or("") + ":" + std::to_string(kt::getPortNumber(socket.getSocketAddress()));

        if (tcpSessions.find(groupId) == tcpSessions.end())
        {
            std::cout << "[TCP] - Creating new group with ID [" << groupId << "], adding address [" << addressString << "] to group.\n";
            // No existing groups with this ID, creating new
            std::vector<kt::TCPSocket> sockets = { socket };
            tcpSessions.insert(std::make_pair(groupId, sockets));
        }
        else
        {
            if (debug)
            {
                std::cout << "[TCP] - Adding new connection [" << addressString << "] to group [" << groupId << "].\n";
            }
            tcpSessions[groupId].push_back(socket);
        }
    }

    void Forwarder::addAddressToUDPGroup(kt::SocketAddress address)
    {
        udpKnownPeers.emplace(address);
    }

    void Forwarder::start()
    {
        if (tcpServerSocket.has_value())
        {
            std::cout << "[TCP] - Running TCP forwarder on port [" << tcpServerSocket->getPort() << "]" << std::endl;
            startTCPForwarder();
        }
        else
        {
            std::cout << "[TCP] - No TCP socket was provided, TCP forwarding is disabled." << std::endl;
        }

        if (udpRecieveSocket.has_value())
        {
            if (udpRecieveSocket->isUdpBound())
            {
                std::cout << "[UDP] - Running UDP forwarder on port [" << udpRecieveSocket->getListeningPort().value() << "]" << std::endl;
                startUDPForwarder();
            }
            else
            {
                std::cout << "[UDP] - Provided UDP socket needs to be bound before it is passed into the forwarder. UDP forwarding will be disabled." << std::endl; 
            }
        }
        else
        {
            std::cout << "[UDP] - No UDP socket was provided, UDP forwarding is disabled." << std::endl;
        }
    }

    void Forwarder::startTCPForwarder()
    {
        forwarderIsRunning = true;

        // Start one thread constantly receiving new connections and adding them to their correct group
        std::thread listeningThread(&Forwarder::startTCPConnectionListener, this);
        
        // Start a second thread that is constantly polling all TCPSockets in the session map and forwarding 
        // those messages to the other sockets in its group.
        std::thread dataForwarderThread(&Forwarder::startTCPDataForwarder, this);

        tcpRunningThreads = std::make_pair(std::move(listeningThread), std::move(dataForwarderThread));
    }

    void Forwarder::startTCPConnectionListener()
    {
        kt::ServerSocket& serverSocket = tcpServerSocket.value();

        std::cout << "[TCP] - Starting TCP connection listener..." << std::endl;
        while(forwarderIsRunning)
        {
            try
            {
                kt::TCPSocket socket = serverSocket.acceptTCPConnection(10000); // microseconds
                std::string firstMessage = socket.receiveAmount(maxReadInSize);

                std::string addressString = kt::getAddress(socket.getSocketAddress()).value_or("") + ":" + std::to_string(kt::getPortNumber(socket.getSocketAddress()));
                std::cout << "[TCP] - Accepted new connection from [" << addressString << "] and read message of size [" << firstMessage.size() << "].\n";

                if (debug)
                {   
                    std::cout << "[TCP] - Accepted connection message: " << firstMessage << "\n";
                }

                if (firstMessage.rfind(newClientPrefix, 0) == 0)
                {
                    std::string groupId = firstMessage.substr(newClientPrefix.size());
                    addSocketToTCPGroup(groupId, socket);
                }
                else
                {
                    // First message does not start with prefix, just close connection
                    std::cout << "[TCP] - First message from address [" << addressString << "] did not start with prefix: [" << newClientPrefix << "]. Closing connection.\n";
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

            std::cout << std::flush;
        }

        // If we exit the loop, close the server socket
        serverSocket.close();
    }

    void Forwarder::startTCPDataForwarder()
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
                    const kt::TCPSocket& receiveSocket = it->second[i];
                    if (receiveSocket.ready(1))
                    {
                        std::string received = receiveSocket.receiveAmount(maxReadInSize);
                        if (received.empty())
                        {
                            continue;
                        }
                        
                        std::string uuidString = getNewUUID();
                        if (debug)
                        {
                            std::cout << "[TCP - " + uuidString + "] - Group [" << groupID << "] with [" << it->second.size() << "] nodes. Received content [" << received << "] from peer [" << i << "] forwarding to other peers...\n";
                        }
                        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
                        for (size_t j = 0; j < it->second.size(); j++)
                        {
                            if (j != i)
                            {
                                const kt::TCPSocket& forwardToSocket = it->second[j];
                                if (!forwardToSocket.connected())
                                {
                                    if (debug)
                                    {
                                        std::cout << "[TCP - " + uuidString + "] - Group [" << groupID << "], peer [" << j << "] is no longer connected, removing from group.\n";
                                    }
                                    toRemove.push_back(j);
                                }
                                else
                                {
                                    if (forwardToSocket.send(received, MSG_NOSIGNAL).first)
                                    {
                                        if (debug)
                                        {
                                            std::cout << "[TCP - " + uuidString + "] - Group [" << groupID << "], successfully forwarded to peer [" << j << "]\n";
                                        }
                                    }
                                    else
                                    {
                                        if (debug)
                                        {
                                            std::cout << "[TCP - " + uuidString + "] - Group [" << groupID << "], failed to send to peer [" << j << "], removing from group.\n";
                                        }
                                        toRemove.push_back(j);
                                    }
                                }
                            }
                        }
                        if (debug)
                        {
                            std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
                            std::cout << "[TCP - " + uuidString + "] - Group [" << groupID << "] took [" << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms] to forward message to [" << it->second.size() - 1 << "] peers.\n";
                        }
                        
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
            for (const kt::TCPSocket& socket : it->second)
            {
                socket.close();
            }
        }
        tcpSessions.clear();
    }

    bool Forwarder::tcpGroupWithIdExists(std::string& groupId)
    {
        return tcpSessions.find(groupId) != tcpSessions.end();
    }

    size_t Forwarder::tcpGroupMemberCount(std::string& groupId)
    {
        auto group = tcpSessions.find(groupId);
        if (group != tcpSessions.end())
        {
            return group->second.size();
        }
        return 0;
    }

    void Forwarder::startUDPForwarder()
    {
        forwarderIsRunning = true;

        std::thread listeningThread(&Forwarder::startUDPListener, this);
        std::thread responderThread(&Forwarder::startUDPDataForwarder, this);
        udpRunningThreads = std::make_pair(std::move(listeningThread), std::move(responderThread));
    }

    void Forwarder::startUDPListener()
    {
        kt::UDPSocket& udpSocket = udpRecieveSocket.value();

        std::cout << "[UDP] - Starting UDP forwarder connection listener..." << std::endl;
        while (forwarderIsRunning)
        {
            if (udpSocket.ready())
            {
                std::pair<std::optional<std::string>, std::pair<int, kt::SocketAddress>> result = udpSocket.receiveFrom(maxReadInSize);
            
                if (result.second.first > 0 && result.first.has_value())
                {
                    std::string addressString = kt::getAddress(result.second.second).value_or("") + ":" + std::to_string(kt::getPortNumber(result.second.second));
                    std::string& message = result.first.value();

                    if (debug)
                    {
                        std::cout << "[UDP] - Received message [" << message << "] from address: [" << addressString << "]\n";
                    }

                    // This is a new client, check their first message content
                    if (message.rfind(newClientPrefix, 0) == 0)
                    {
                        std::string recievingPort = message.substr(newClientPrefix.size());
                        std::cout << "[UDP] - New client joined UDP group from address [" << addressString << "] with request reply port [" << recievingPort << "]\n";

                        kt::SocketAddress address = result.second.second;
                        address.ipv4.sin_port = htons(std::atoi(recievingPort.c_str()));
                        addAddressToUDPGroup(address);
                    }
                    else
                    {
                        udpMessageQueue.push(message);
                    }
                }
            }
            std::cout << std::flush;
        }
        udpSocket.close();
    }

    void Forwarder::startUDPDataForwarder()
    {
        std::cout << "[UDP] - Starting UDP data forwarder listener..." << std::endl;
        kt::UDPSocket sendSocket;
        while (forwarderIsRunning)
        {
            if (!udpMessageQueue.empty())
            {
                std::string uuidString = getNewUUID();
                std::string message = udpMessageQueue.front();
                udpMessageQueue.pop();

                if (debug)
                {
                    std::cout << "[UDP - " + uuidString + "] - Received message [" << message << "] forwarding to peers.\n";
                }

                std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
                for (const kt::SocketAddress& addr : udpKnownPeers)
                {
                    std::pair<bool, int> result = sendSocket.sendTo(message, addr);
                    if (debug)
                    {
                        std::cout << "[UDP - " + uuidString + "] - Forwarded to peer with address: [" << kt::getAddress(addr).value_or("") + ":" + std::to_string(kt::getPortNumber(addr)) << "]. With result [" << result.second << "]\n";
                    }
                }

                if (debug)
                {
                    std::cout << "[UDP - " + uuidString + "] - Forwarded to [" << udpKnownPeers.size() << "] peer(s).\n";
                    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
                    std::cout << "[UDP - " + uuidString + "] - Took [" << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms] to forward message to [" << udpKnownPeers.size() << "] peers.\n";
                }
            }
            else
            {
                using namespace std::chrono_literals;
                std::this_thread::sleep_for(10us);
            }
        }

        udpKnownPeers.clear();
    }

    size_t Forwarder::udpGroupMemberCount()
    {
        return udpKnownPeers.size();
    }

    void Forwarder::stop()
    {
        forwarderIsRunning = false;
    }

    void Forwarder::join()
    {
        if (tcpRunningThreads.has_value())
        {
            tcpRunningThreads->first.join();
            tcpRunningThreads->second.join();
        }

        if (udpRunningThreads.has_value())
        {
            udpRunningThreads->first.join();
            udpRunningThreads->second.join();
        }
    }

    std::string getNewUUID()
    {
        uuid_t uuid{};
        char temp[100];
        uuid_generate(uuid);
        uuid_unparse(uuid, temp);
        return std::string(temp);
    }
}
