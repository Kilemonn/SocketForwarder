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
			stopTCPForwarder();

			runningThreads.first.join();
			runningThreads.second.join();
        }
    };

	TEST_F(TCPSocketForwarderTest, Test1)
	{
		std::string groupId = "98235123";
		kt::TCPSocket client1("localhost", serverSocket.getPort());
		ASSERT_TRUE(client1.send(NEW_CLIENT_PREFIX_DEFAULT + groupId));

		std::this_thread::sleep_for(100ms);
		ASSERT_TRUE(tcpGroupWithIdExists(groupId));

		kt::TCPSocket client2("localhost", serverSocket.getPort());
		ASSERT_TRUE(client2.send(NEW_CLIENT_PREFIX_DEFAULT + groupId));
		

		client1.close();
		client2.close();
	}
}
