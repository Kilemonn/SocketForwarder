#include <gtest/gtest.h>
#include <thread>
#include <chrono>

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
			runningThreads = startTCPForwarder(serverSocket);
		}

        void TearDown() override
        {
			stopForwarder();

			runningThreads.first.join();
			runningThreads.second.join();

			serverSocket.close();
        }
    };

	TEST_F(TCPSocketForwarderTest, TestGeneralSendAndRecieve)
	{
		std::string groupId = "98235123";
		kt::TCPSocket client1("localhost", serverSocket.getPort());
		ASSERT_TRUE(client1.send(NEW_CLIENT_PREFIX_DEFAULT + groupId));
		std::this_thread::sleep_for(10ms);

		ASSERT_TRUE(tcpGroupWithIdExists(groupId));

		kt::TCPSocket client2("localhost", serverSocket.getPort());
		ASSERT_TRUE(client2.send(NEW_CLIENT_PREFIX_DEFAULT + groupId));
		std::this_thread::sleep_for(10ms);

		std::string client3GroupId = "112343";
		ASSERT_NE(client3GroupId, groupId);
		kt::TCPSocket client3("localhost", serverSocket.getPort());
		ASSERT_TRUE(client3.send(NEW_CLIENT_PREFIX_DEFAULT + client3GroupId));
		std::this_thread::sleep_for(10ms);

		std::string toSend = "TCPSocketForwarderTest";
		ASSERT_TRUE(client1.send(toSend));
		std::this_thread::sleep_for(10ms);

		std::string received = client2.receiveAmount(50);
		ASSERT_EQ(toSend, received);
		// Make sure client 3 did not receive anything since its not in that group
		ASSERT_FALSE(client3.ready());

		received += "12738651239132434";
		ASSERT_TRUE(client2.send(received));
		std::this_thread::sleep_for(10ms);

		toSend = client1.receiveAmount(50);
		ASSERT_EQ(toSend, received);
		// Make sure client 3 did not receive anything since its not in that group
		ASSERT_FALSE(client3.ready());

		client1.close();
		client2.close();
		client3.close();
	}
}
