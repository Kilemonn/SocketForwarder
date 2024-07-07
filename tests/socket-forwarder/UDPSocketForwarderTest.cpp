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
        ASSERT_EQ(0, udpGroupMemberCount());

		kt::UDPSocket client1;
        client1.bind();
		ASSERT_TRUE(client1.sendTo("localhost", udpSocket.getListeningPort().value(), NEW_CLIENT_PREFIX_DEFAULT + std::to_string(client1.getListeningPort().value())).first);
		std::this_thread::sleep_for(10ms);

		ASSERT_EQ(1, udpGroupMemberCount());

		kt::UDPSocket client2;
        client2.bind();
        ASSERT_TRUE(client2.sendTo("localhost", udpSocket.getListeningPort().value(), NEW_CLIENT_PREFIX_DEFAULT + std::to_string(client2.getListeningPort().value())).first);
		std::this_thread::sleep_for(10ms);

        ASSERT_EQ(2, udpGroupMemberCount());

		std::string toSend = "UDPSocketForwarderTest";
		ASSERT_TRUE(client1.sendTo("localhost", udpSocket.getListeningPort().value(), toSend).first);
		std::this_thread::sleep_for(1000ms);

        ASSERT_TRUE(client2.ready() || client1.ready());

        if (client2.ready())
        {
            std::pair<std::optional<std::string>, std::pair<int, kt::SocketAddress>> result = client2.receiveFrom(50);
            ASSERT_NE(-1, result.second.first);
            ASSERT_EQ(toSend, result.first.value());
        }

        if (client1.ready())
        {
            std::pair<std::optional<std::string>, std::pair<int, kt::SocketAddress>> result = client1.receiveFrom(50);
            ASSERT_NE(-1, result.second.first);
            ASSERT_EQ(toSend, result.first.value());
        }
        
		toSend += "99283647561";
		ASSERT_TRUE(client2.sendTo("localhost", udpSocket.getListeningPort().value(), toSend).first);
		std::this_thread::sleep_for(1000ms);

        ASSERT_TRUE(client2.ready() || client1.ready());

        if (client1.ready())
        {
            std::pair<std::optional<std::string>, std::pair<int, kt::SocketAddress>> result = client1.receiveFrom(50);
            ASSERT_NE(-1, result.second.first);
            ASSERT_EQ(toSend, result.first.value());
        }

        if (client2.ready())
        {
            std::pair<std::optional<std::string>, std::pair<int, kt::SocketAddress>> result = client2.receiveFrom(50);
            ASSERT_NE(-1, result.second.first);
            ASSERT_EQ(toSend, result.first.value());
        }

		client1.close();
		client2.close();
	}
}
