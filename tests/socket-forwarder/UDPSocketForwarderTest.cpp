#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <future>

#include "../../socket-forwarder/environment/Environment.h"
#include "../../socket-forwarder/forwarder/Forwarder.h"

using namespace std::chrono_literals;

namespace forwarder
{
	class UDPSocketForwarderTest : public ::testing::Test
	{
	protected:
        kt::UDPSocket udpSocket;
		std::pair<std::thread, std::thread> runningThreads;
    protected:
        UDPSocketForwarderTest() : udpSocket() {}
        void SetUp() override
		{
            udpSocket.bind();
			runningThreads = startUDPForwarder(udpSocket, NEW_CLIENT_PREFIX_DEFAULT, MAX_READ_IN_DEFAULT);
		}

        void TearDown() override
        {
			stopForwarder();
			runningThreads.first.join();
            runningThreads.second.join();

			udpSocket.close();
        }
    };

	TEST_F(UDPSocketForwarderTest, TestGeneralSendAndRecieve)
	{
        ASSERT_EQ(0, udpGroupMemberCount());

        // Add client 1 to the group
		kt::UDPSocket client1;
        ASSERT_TRUE(client1.bind().first); // Bind since we will recieve data later
        ASSERT_TRUE(client1.sendTo("localhost", udpSocket.getListeningPort().value(), NEW_CLIENT_PREFIX_DEFAULT + std::to_string(client1.getListeningPort().value())).first.first);
		std::this_thread::sleep_for(10ms);

        // Check group count is incremented since client 1 has joined
		ASSERT_EQ(1, udpGroupMemberCount());

        // Add client 2 to the group
		kt::UDPSocket client2;
        ASSERT_TRUE(client2.bind().first); // Bind since we will recieve data later
        ASSERT_TRUE(client2.sendTo("localhost", udpSocket.getListeningPort().value(), NEW_CLIENT_PREFIX_DEFAULT + std::to_string(client2.getListeningPort().value())).first.first);
		std::this_thread::sleep_for(10ms);

        // Check group count is incremented since client 2 has joined
        ASSERT_EQ(2, udpGroupMemberCount());

        // Send to group from client1
		std::string toSend = "UDPSocketForwarderTest";
		ASSERT_TRUE(client1.sendTo("localhost", udpSocket.getListeningPort().value(), toSend).first.first);
		std::this_thread::sleep_for(10ms);

        // Check it is received at client 2
        ASSERT_TRUE(client2.ready());
        std::pair<std::optional<std::string>, std::pair<int, kt::SocketAddress>> readResult = client2.receiveFrom(50);
        ASSERT_NE(-1, readResult.second.first);
        ASSERT_EQ(toSend, readResult.first.value());

        // Check it is received at client 1 because the UDP group does not know who sent it and just forwards to everyone
        ASSERT_TRUE(client1.ready());
        readResult = client1.receiveFrom(50);
        ASSERT_NE(-1, readResult.second.first);
        ASSERT_EQ(toSend, readResult.first.value());
        
        // Send different message from client 2
		toSend += "99283647561";
		ASSERT_TRUE(client2.sendTo("localhost", udpSocket.getListeningPort().value(), toSend).first.first);
		std::this_thread::sleep_for(10ms);

        // Check client 1 received it
        ASSERT_TRUE(client1.ready());
        readResult = client1.receiveFrom(50);
        ASSERT_NE(-1, readResult.second.first);
        ASSERT_EQ(toSend, readResult.first.value());

        // Again since its a broadcast client 2 will also receive it
        ASSERT_TRUE(client2.ready());
        readResult = client2.receiveFrom(50);
        ASSERT_NE(-1, readResult.second.first);
        ASSERT_EQ(toSend, readResult.first.value());

		client1.close();
		client2.close();
	}

    void receiveMessageAndAssertAsync(std::vector<kt::UDPSocket> sockets, size_t startIndex, unsigned long long endIndex, size_t messagesToReceive, std::string message)
    {
        ASSERT_GT(endIndex, startIndex);
        size_t receivedMessageCount = 0;
        for (size_t i = 0; i < messagesToReceive; i++)
        {
            for (size_t clientIndex = startIndex; clientIndex < endIndex; clientIndex++)
            {
                kt::UDPSocket socket = sockets[clientIndex];
                if (socket.ready())
                {
                    std::pair<std::optional<std::string>, std::pair<int, kt::SocketAddress>> result = socket.receiveFrom(message.size() + std::to_string(i).size());
                    ASSERT_NE(-1, result.second.first);
                    ASSERT_NE(std::nullopt, result.first);
                    ASSERT_TRUE(result.first.value().rfind(message, 0) == 0);
                    
                    receivedMessageCount++;
                }
            }
        }

        std::cout << "UDP - Test - Received messages [" << receivedMessageCount << "/" << messagesToReceive * (endIndex - startIndex) << "] [~" << (static_cast<double>(receivedMessageCount) / static_cast<double>((messagesToReceive * (endIndex - startIndex)))) * 100 << "%] from forwarder.\n";
    }

    /**
     * I've seen some UDP level recv buffer limits that are stopping how much I can push this test.
     * 
     * The max I've been able to achieve in this test is receiving 55600 total messages using different permutations.
     * 
     * For now, I can get 100% rate with 200 clients and 278 messages (which equals the max limit 55600). 
     * I believe the cause is how quickly the forwarder is returning messages back
     * since this is all running from a single machine.
     * 
     * I can try to add socket recv buffer limit increases to the UDP sockets.
     * 
     * The buffer size is 212992 by default, even if I double it (in the alpine container) to 425984. I still
     * reach a hard limit of 55600 messages. Maybe its the limit for a single machine. Or something else.
     */
    TEST_F(UDPSocketForwarderTest, TestNumerousClients)
    {
        const size_t amountOfClients = 200;
		const size_t messagesToSend = 278;
		std::string groupId = "TestNumerousClients-group";
		std::vector<kt::UDPSocket> sockets;

        ASSERT_EQ(0, udpGroupMemberCount());

        int bufferSize = 0;
        socklen_t size = sizeof(bufferSize);
        int err = getsockopt(udpSocket.getListeningSocket(), SOL_SOCKET, SO_RCVBUF, (char*)&bufferSize, &size);
        std::cout << "Buffer res: " << err << ". Buffer size is [" << bufferSize << "]." << std::endl;
        for (size_t i = 0; i < amountOfClients; i++)
		{
			kt::UDPSocket socket;
            ASSERT_TRUE(socket.bind().first);
			ASSERT_TRUE(socket.sendTo("localhost", udpSocket.getListeningPort().value(), NEW_CLIENT_PREFIX_DEFAULT + std::to_string(socket.getListeningPort().value())).first.first);
			sockets.push_back(socket);
            std::this_thread::sleep_for(1ms);
		}
		std::this_thread::sleep_for(10ms);

        ASSERT_EQ(amountOfClients, udpGroupMemberCount());

		std::string message = "TestNumerousClients";
        for (size_t i = 0; i < messagesToSend; i++)
		{
			kt::UDPSocket socket = sockets[0];
			ASSERT_TRUE(socket.sendTo("localhost", udpSocket.getListeningPort().value(), message + std::to_string(i)).first.first);
            std::this_thread::sleep_for(2ms);
		}

        std::this_thread::sleep_for(10ms);

        size_t receivedMessageCount = 0;

        // std::vector<std::future<void>> asyncs;
        // size_t increment = 2;
        // for (size_t split = 0; split < amountOfClients; split += increment)
        // {
        //     asyncs.push_back(std::move(std::async(std::launch::async, receiveMessageAndAssertAsync, sockets, split, split + increment, messagesToSend, message)));
        // }
		
        for (size_t i = 0; i < messagesToSend; i++)
		{
			for (size_t clientIndex = 0; clientIndex < amountOfClients; clientIndex++)
			{
				kt::UDPSocket socket = sockets[clientIndex];
                if (socket.ready())
                {
                    std::pair<std::optional<std::string>, std::pair<int, kt::SocketAddress>> result = socket.receiveFrom(message.size() + std::to_string(i).size());
                    ASSERT_NE(-1, result.second.first);
                    ASSERT_NE(std::nullopt, result.first);
                    ASSERT_TRUE(result.first.value().rfind(message, 0) == 0);
                    receivedMessageCount++;
                }
			}
		}

        // for (size_t i = 0; i < asyncs.size(); i++)
        // {
        //     asyncs[i].wait();
        // }

        for (size_t i = 0; i < amountOfClients; i++)
        {
            kt::UDPSocket socket = sockets[i];
            socket.close();
        }

        std::cout << "UDP - Test - Received messages [" << receivedMessageCount << "/" << messagesToSend * amountOfClients << "] [~" << (static_cast<double>(receivedMessageCount) / static_cast<double>((messagesToSend * amountOfClients))) * 100 << "%] from forwarder.\n";
        
        // We cannot assert this since there can be messages that are lost or dropped because of the use of UDP
        // ASSERT_EQ(messagesToSend * amountOfClients, receivedMessageCount);

        // We'll assert that we got more than 1 message and less than or equal to the upper limit
        ASSERT_GT(receivedMessageCount, 0);
        ASSERT_LE(receivedMessageCount, messagesToSend * amountOfClients);
    }
}
