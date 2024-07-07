#include <gtest/gtest.h>
#include <thread>
#include <chrono>

#include "../../socket-forwarder/environment/Environment.h"
#include "../../socket-forwarder/forwarder/Forwarder.h"

using namespace std::chrono_literals;

namespace forwarder
{
	class UDPSocketForwarderTest : public ::testing::Test
	{
	protected:
        kt::UDPSocket udpSocket;
		std::thread runningThread;
    protected:
        UDPSocketForwarderTest() : udpSocket() {}
        void SetUp() override
		{
            udpSocket.bind();
			runningThread = startUDPForwarder(udpSocket);
		}

        void TearDown() override
        {
			stopForwarder();
			runningThread.join();

			udpSocket.close();
        }
    };

	TEST_F(UDPSocketForwarderTest, TestGeneralSendAndRecieve)
	{
		std::string groupId = "1115121220";
		kt::UDPSocket client1;
		ASSERT_TRUE(client1.sendTo("localhost", udpSocket.getListeningPort().value(), NEW_CLIENT_PREFIX_DEFAULT + groupId).first);
		std::this_thread::sleep_for(10ms);

		ASSERT_TRUE(udpGroupWithIdExists(groupId));

		kt::UDPSocket client2;
        ASSERT_TRUE(client2.sendTo("localhost", udpSocket.getListeningPort().value(), NEW_CLIENT_PREFIX_DEFAULT + groupId).first);
		std::this_thread::sleep_for(10ms);

		std::string toSend = "UDPSocketForwarderTest";
		ASSERT_TRUE(client1.sendTo("localhost", udpSocket.getListeningPort().value(), toSend).first);
		std::this_thread::sleep_for(10ms);

		std::pair<std::optional<std::string>, std::pair<int, kt::SocketAddress>> result = client2.receiveFrom(50);
		ASSERT_NE(std::nullopt, result.first);
        ASSERT_EQ(toSend, result.first.value());

		std::string received = result.first.value() + "99283647561";
		ASSERT_TRUE(client2.sendTo("localhost", udpSocket.getListeningPort().value(), received).first);
		std::this_thread::sleep_for(10ms);

		result = client1.receiveFrom(50);
        ASSERT_NE(std::nullopt, result.first);
		ASSERT_EQ(result.first.value(), received);

		client1.close();
		client2.close();
	}
}
