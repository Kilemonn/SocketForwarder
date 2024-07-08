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
			runningThread = startUDPForwarder(udpSocket, NEW_CLIENT_PREFIX_DEFAULT, MAX_READ_IN_DEFAULT);
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

        // Add client 1 to the group
		kt::UDPSocket client1;
        client1.bind(); // Bind since we will recieve data later
        ASSERT_TRUE(client1.sendTo("127.0.0.1", udpSocket.getListeningPort().value(), NEW_CLIENT_PREFIX_DEFAULT + std::to_string(client1.getListeningPort().value())).first);
		std::this_thread::sleep_for(10ms);

        // Check group count is incremented since client 1 has joined
		ASSERT_EQ(1, udpGroupMemberCount());

        // Add client 2 to the group
		kt::UDPSocket client2;
        client2.bind(); // Bind since we will recieve data later
        ASSERT_TRUE(client2.sendTo("127.0.0.1", udpSocket.getListeningPort().value(), NEW_CLIENT_PREFIX_DEFAULT + std::to_string(client2.getListeningPort().value())).first);
		std::this_thread::sleep_for(10ms);

        // Check group count is incremented since client 2 has joined
        ASSERT_EQ(2, udpGroupMemberCount());

        // Send to group from client1
		std::string toSend = "UDPSocketForwarderTest";
		ASSERT_TRUE(client1.sendTo("127.0.0.1", udpSocket.getListeningPort().value(), toSend).first);
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
		ASSERT_TRUE(client2.sendTo("127.0.0.1", udpSocket.getListeningPort().value(), toSend).first);
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
}
