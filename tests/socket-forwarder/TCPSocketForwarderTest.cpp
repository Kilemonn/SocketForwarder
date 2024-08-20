#include <gtest/gtest.h>
#include <thread>
#include <chrono>

#include <csignal>

#include "../../socket-forwarder/environment/Environment.h"
#include "../../socket-forwarder/forwarder/Forwarder.h"

using namespace std::chrono_literals;

namespace forwarder
{
	class TCPSocketForwarderTest : public ::testing::Test
	{
	protected:
        kt::ServerSocket serverSocket;
		std::pair<std::thread, std::thread> runningThreads;
    protected:
        TCPSocketForwarderTest() : serverSocket(kt::SocketType::Wifi) {}
        void SetUp() override
		{
			runningThreads = startTCPForwarder(serverSocket, NEW_CLIENT_PREFIX_DEFAULT, MAX_READ_IN_DEFAULT);
		}

        void TearDown() override
        {
			stopForwarder();

			runningThreads.first.join();
			runningThreads.second.join();

			serverSocket.close();
        }
    };

	TEST_F(TCPSocketForwarderTest, TestSendToGroup_OtherGroupsDoNotReceive_SenderDoesNotReceiveOwnMessage)
	{
		std::string groupId = "98235123";
		ASSERT_FALSE(tcpGroupWithIdExists(groupId));
		
		kt::TCPSocket client1("localhost", serverSocket.getPort());
		ASSERT_TRUE(client1.send(NEW_CLIENT_PREFIX_DEFAULT + groupId).first);
		std::this_thread::sleep_for(10ms);

		ASSERT_TRUE(tcpGroupWithIdExists(groupId));
		ASSERT_EQ(1, tcpGroupMemberCount(groupId));

		kt::TCPSocket client2("localhost", serverSocket.getPort());
		ASSERT_TRUE(client2.send(NEW_CLIENT_PREFIX_DEFAULT + groupId).first);
		std::this_thread::sleep_for(10ms);

		ASSERT_EQ(2, tcpGroupMemberCount(groupId));

		std::string client3GroupId = "112343";
		ASSERT_NE(client3GroupId, groupId);
		ASSERT_FALSE(tcpGroupWithIdExists(client3GroupId));

		kt::TCPSocket client3("localhost", serverSocket.getPort());
		ASSERT_TRUE(client3.send(NEW_CLIENT_PREFIX_DEFAULT + client3GroupId).first);
		std::this_thread::sleep_for(10ms);

		ASSERT_EQ(1, tcpGroupMemberCount(client3GroupId));

		std::string toSend = "TCPSocketForwarderTest";
		ASSERT_TRUE(client1.send(toSend).first);
		std::this_thread::sleep_for(10ms);

		ASSERT_FALSE(client1.ready());
		ASSERT_TRUE(client2.ready());
		std::string received = client2.receiveAmount(50);
		ASSERT_EQ(toSend, received);
		// Make sure client 3 did not receive anything since its not in that group
		ASSERT_FALSE(client3.ready());

		received += "12738651239132434";
		ASSERT_TRUE(client2.send(received).first);
		std::this_thread::sleep_for(10ms);

		ASSERT_FALSE(client2.ready());
		ASSERT_TRUE(client1.ready());
		toSend = client1.receiveAmount(50);
		ASSERT_EQ(toSend, received);

		// Make sure client 3 did not receive anything since its not in that group
		ASSERT_FALSE(client3.ready());

		client1.close();
		client2.close();
		client3.close();
	}

	TEST_F(TCPSocketForwarderTest, TestClientDisconnects)
	{
		std::string groupId = "TestClientDisconnects-group";
		ASSERT_FALSE(tcpGroupWithIdExists(groupId));

		kt::TCPSocket client1("localhost", serverSocket.getPort());
		kt::TCPSocket client2("localhost", serverSocket.getPort());
		kt::TCPSocket client3("localhost", serverSocket.getPort());

		ASSERT_TRUE(client1.send(NEW_CLIENT_PREFIX_DEFAULT + groupId).first);
		ASSERT_TRUE(client2.send(NEW_CLIENT_PREFIX_DEFAULT + groupId).first);
		ASSERT_TRUE(client3.send(NEW_CLIENT_PREFIX_DEFAULT + groupId).first);
		std::this_thread::sleep_for(10ms);

		ASSERT_TRUE(tcpGroupWithIdExists(groupId));
		ASSERT_EQ(3, tcpGroupMemberCount(groupId));

		client3.close();
		std::this_thread::sleep_for(10ms);

		std::string content = "TestClientDisconnects-message";
		ASSERT_TRUE(client1.send(content).first);
		std::this_thread::sleep_for(10ms);

		ASSERT_TRUE(client2.ready());
		std::string recieved = client2.receiveAmount(50);

		ASSERT_EQ(content, recieved);

		ASSERT_TRUE(client1.send(content + content).first);
		std::this_thread::sleep_for(10ms);

		ASSERT_TRUE(client2.ready());
		recieved = client2.receiveAmount(100);
		ASSERT_EQ(content + content, recieved);

		ASSERT_EQ(2, tcpGroupMemberCount(groupId));

		client1.close();
		client2.close();
	}

	TEST_F(TCPSocketForwarderTest, TestNumerousClients)
	{
		const size_t amountOfClients = 200;
		std::string groupId = "TestNumerousClients-group";
		std::vector<kt::TCPSocket> sockets;

		ASSERT_FALSE(tcpGroupWithIdExists(groupId));

		for (size_t i = 0; i < amountOfClients; i++)
		{
			kt::TCPSocket socket("localhost", serverSocket.getPort());
			ASSERT_TRUE(socket.send(NEW_CLIENT_PREFIX_DEFAULT + groupId).first);
			sockets.push_back(socket);
		}
		std::this_thread::sleep_for(10ms);

		ASSERT_TRUE(tcpGroupWithIdExists(groupId));
		ASSERT_EQ(amountOfClients, tcpGroupMemberCount(groupId));

		const size_t sendFromIndex = 0;
		const size_t messagesToSend = 1000;
		std::string message = "TestNumerousClients";
		for (size_t i = 0; i < messagesToSend; i++)
		{
			kt::TCPSocket socket = sockets[sendFromIndex];
			ASSERT_TRUE(socket.send(message + std::to_string(i)).first);
		}
		std::this_thread::sleep_for(100ms);

		for (size_t i = 0; i < messagesToSend; i++)
		{
			for (size_t clientIndex = 1; clientIndex < amountOfClients; clientIndex++)
			{
				kt::TCPSocket socket = sockets[clientIndex];
				ASSERT_TRUE(socket.ready());
				std::string received = socket.receiveAmount(message.size() + std::to_string(i).size());
				ASSERT_EQ(message + std::to_string(i), received);
			}
		}
		ASSERT_FALSE(sockets[sendFromIndex].ready());

		for (size_t i = 0; i < amountOfClients; i++)
		{
			kt::TCPSocket socket = sockets[i];
			socket.close();
		}
	}
}
