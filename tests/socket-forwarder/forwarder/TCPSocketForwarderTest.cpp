#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <optional>

#include <csignal>

#include "../../../socket-forwarder/environment/Environment.h"
#include "../../../socket-forwarder/forwarder/Forwarder.h"
#include "../../../socket-forwarder/logger/Logger.h"

#include <log4cxx/basicconfigurator.h>

using namespace std::chrono_literals;

namespace forwarder
{
	class TCPSocketForwarderTest : public ::testing::Test
	{
	protected:
		std::vector<ForwarderGroup> tcpGroups;
		std::vector<ForwarderGroup> udpGroups;
		std::optional<forwarder::Forwarder> forwarder;

    protected:
        TCPSocketForwarderTest()
		{
			tcpGroups = {
				{.receiverPort = 11111, .senderPort = 11112, .groupId = "tcpGroup1"},
				{.receiverPort = 11113, .senderPort = 11114, .groupId = "tcpGroup2"}
			};

			// Using IPV4 for TCP tests
			ForwarderConfig config(forwarder::HOST_IPV4_ADDRESS_DEFAULT, kt::InternetProtocolVersion::IPV4, forwarder::MAX_READ_IN_DEFAULT, forwarder::MAX_READ_IN_DEFAULT,
				tcpGroups, udpGroups);

			forwarder = Forwarder(config);
		}
        
		static void SetUpTestCase()
		{
			log4cxx::BasicConfigurator::resetConfiguration();
			forwarder::initialiseConsoleLogger();
			log4cxx::Logger::getRootLogger()->setLevel(log4cxx::Level::getInfo());
		}
		
		void SetUp() override
		{
			ASSERT_NE(forwarder, std::nullopt);
			forwarder->start();
		}

        void TearDown() override
        {
			forwarder->stop();
			forwarder->join();
        }
    };

	TEST_F(TCPSocketForwarderTest, TestSendToGroup_OtherGroupsDoNotReceive_SenderDoesNotReceiveOwnMessage)
	{
		ForwarderGroup group1 = tcpGroups.at(0);
		ASSERT_FALSE(forwarder->tcpGroupWithIdExists(group1.groupId));
		
		kt::TCPSocket client1("localhost", group1.senderPort);
		std::this_thread::sleep_for(50ms);

		ASSERT_TRUE(forwarder->tcpGroupWithIdExists(group1.groupId));
		ASSERT_EQ(1, forwarder->tcpGroupMemberCount(group1.groupId));

		kt::TCPSocket client2("localhost", group1.receiverPort);
		std::this_thread::sleep_for(50ms);

		ASSERT_EQ(2, forwarder->tcpGroupMemberCount(group1.groupId));

		ForwarderGroup group2 = tcpGroups.at(1);
		ASSERT_NE(group2.groupId, group1.groupId);
		ASSERT_FALSE(forwarder->tcpGroupWithIdExists(group2.groupId));

		kt::TCPSocket client3("localhost", group2.receiverPort);
		std::this_thread::sleep_for(50ms);

		ASSERT_EQ(1, forwarder->tcpGroupMemberCount(group2.groupId));

		std::string toSend = "TCPSocketForwarderTest";
		ASSERT_EQ(toSend.size(), client1.send(toSend));
		std::this_thread::sleep_for(10ms);

		ASSERT_FALSE(client1.ready());
		ASSERT_TRUE(client2.ready());
		std::string received = client2.receiveAmount(50);
		ASSERT_EQ(toSend, received);
		// Make sure client 3 did not receive anything since its not in that group
		ASSERT_FALSE(client3.ready());

		received += "12738651239132434";

		// Make sure anything sent by client2 is not detected by client 1, since client2 is connected to the receiver port and cannot send into the forwarder
		ASSERT_EQ(received.size(), client2.send(received));
		std::this_thread::sleep_for(10ms);

		ASSERT_FALSE(client2.ready());
		ASSERT_FALSE(client1.ready());

		// Make sure client 3 did not receive anything since its in a separate group
		ASSERT_FALSE(client3.ready());

		client1.close();
		client2.close();
		client3.close();
	}

	TEST_F(TCPSocketForwarderTest, TestClientDisconnects)
	{
		ForwarderGroup group1 = tcpGroups.at(0);
		ASSERT_FALSE(forwarder->tcpGroupWithIdExists(group1.groupId));

		kt::TCPSocket client1("localhost", group1.senderPort);
		kt::TCPSocket client2("localhost", group1.receiverPort);
		kt::TCPSocket client3("localhost", group1.receiverPort);

		std::this_thread::sleep_for(10ms);

		ASSERT_TRUE(forwarder->tcpGroupWithIdExists(group1.groupId));
		ASSERT_EQ(3, forwarder->tcpGroupMemberCount(group1.groupId));

		client3.close();
		std::this_thread::sleep_for(50ms);

		std::string content = "TestClientDisconnects-message";
		ASSERT_EQ(content.size(), client1.send(content));
		std::this_thread::sleep_for(10ms);

		ASSERT_TRUE(client2.ready());
		std::string recieved = client2.receiveAmount(50);

		ASSERT_EQ(content, recieved);

		ASSERT_EQ(content.size() + content.size(), client1.send(content + content));
		std::this_thread::sleep_for(10ms);

		ASSERT_TRUE(client2.ready());
		recieved = client2.receiveAmount(100);
		ASSERT_EQ(content + content, recieved);

		ASSERT_EQ(2, forwarder->tcpGroupMemberCount(group1.groupId));

		client1.close();
		client2.close();
	}

	TEST_F(TCPSocketForwarderTest, TestNumerousClients)
	{
		ForwarderGroup group1 = tcpGroups.at(0);

		const size_t amountOfClients = 200;
		kt::TCPSocket sendSocket("localhost", group1.senderPort);
		std::vector<kt::TCPSocket> receiveSockets;
		receiveSockets.reserve(amountOfClients);

		ASSERT_FALSE(forwarder->tcpGroupWithIdExists(group1.groupId));

		std::this_thread::sleep_for(10ms);

		for (size_t i = 0; i < amountOfClients; i++)
		{
			kt::TCPSocket receiverSocket("localhost", group1.receiverPort);
			receiveSockets.push_back(receiverSocket);
		}
		std::this_thread::sleep_for(100ms);

		ASSERT_TRUE(forwarder->tcpGroupWithIdExists(group1.groupId));
		ASSERT_EQ(amountOfClients + 1, forwarder->tcpGroupMemberCount(group1.groupId));

		const size_t messagesToSend = 1000;
		std::string message = "TestNumerousClients";
		for (size_t i = 0; i < messagesToSend; i++)
		{
			ASSERT_EQ(message.size() + std::to_string(i).size(), sendSocket.send(message + std::to_string(i)));
		}
		std::this_thread::sleep_for(100ms);

		for (size_t i = 0; i < messagesToSend; i++)
		{
			for (kt::TCPSocket& receiverSocket : receiveSockets)
			{
				ASSERT_TRUE(receiverSocket.ready());
				std::string received = receiverSocket.receiveAmount(message.size() + std::to_string(i).size());
				ASSERT_EQ(message + std::to_string(i), received);
			}
		}
		ASSERT_FALSE(sendSocket.ready());

		sendSocket.close();
		for (kt::TCPSocket& receiverSocket : receiveSockets)
		{
			receiverSocket.close();
		}
	}
}
