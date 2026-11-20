#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <future>

#include "../../../socket-forwarder/environment/Environment.h"
#include "../../../socket-forwarder/forwarder/Forwarder.h"
#include "../../../socket-forwarder/logger/Logger.h"

#include <log4cxx/basicconfigurator.h>

using namespace std::chrono_literals;

namespace forwarder
{
	class UDPSocketForwarderTest : public ::testing::Test
	{
	protected:
        std::vector<ForwarderGroup> tcpGroups;
		std::vector<ForwarderGroup> udpGroups;

		std::optional<forwarder::Forwarder> forwarder = std::nullopt;
    protected:
        UDPSocketForwarderTest()
        {
            udpGroups = {
				{.receiverPort = 11115, .senderPort = 11116, .groupId = "udpGroup1"},
				{.receiverPort = 11117, .senderPort = 11118, .groupId = "udpGroup2"}
			};

            // Using IPV6 for UDP tests
			ForwarderConfig config(forwarder::HOST_IPV6_ADDRESS_DEFAULT, kt::InternetProtocolVersion::IPV6, forwarder::MAX_READ_IN_DEFAULT, forwarder::MAX_READ_IN_DEFAULT,
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

	TEST_F(UDPSocketForwarderTest, TestGeneralSendAndRecieve)
	{
        ForwarderGroup group1 = udpGroups.at(0);

        ASSERT_EQ(0, forwarder->udpGroupMemberCount(group1.groupId));

        const std::string message = "m";
        // Add client 1 to the group
		kt::UDPSocket client1;
        ASSERT_TRUE(client1.bind(kt::InternetProtocolVersion::IPV6)); // Bind to keep our recv and send address the same, so the forwarder will reply to the correct port
        ASSERT_EQ(message.size(), client1.sendTo("::1", group1.receiverPort, message).first);
		std::this_thread::sleep_for(10ms);

        // Check group count is incremented since client 1 has joined
		ASSERT_EQ(1, forwarder->udpGroupMemberCount(group1.groupId));

		kt::UDPSocket client2; // We don't need to bind since this is just a sender socket
        // Send to group from client2
		std::string toSend = "UDPSocketForwarderTest";
		ASSERT_EQ(toSend.size(), client2.sendTo("::1", group1.senderPort, toSend).first);
		std::this_thread::sleep_for(10ms);

        // Check group count is the same since it only includes receivers counts
        ASSERT_EQ(1, forwarder->udpGroupMemberCount(group1.groupId));

        // Check it is received at client 2
        ASSERT_TRUE(client1.ready());
        std::expected<std::pair<std::string, kt::SocketAddress>, int> readResult = client1.receiveFrom(50);
        ASSERT_TRUE(readResult);
        ASSERT_EQ(toSend, readResult.value().first);

        // Make sure client2 has no messages as they are only a sender
        ASSERT_FALSE(client2.ready());
        
        // Send different message from client 2
		toSend += "99283647561";
		ASSERT_EQ(toSend.size(), client2.sendTo("::1", group1.senderPort, toSend).first);
		std::this_thread::sleep_for(10ms);

        // Check client 1 received it
        ASSERT_TRUE(client1.ready());
        readResult = client1.receiveFrom(50);
        ASSERT_TRUE(readResult);
        ASSERT_EQ(toSend, readResult.value().first);

        ASSERT_FALSE(client2.ready());

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
                kt::UDPSocket& socket = sockets[clientIndex];
                if (socket.ready())
                {
                    std::expected<std::pair<std::string, kt::SocketAddress>, int> result = socket.receiveFrom(message.size() + std::to_string(i).size());
                    ASSERT_TRUE(result);
                    ASSERT_TRUE(result.value().first.rfind(message, 0) == 0);
                    
                    receivedMessageCount++;
                }
            }
        }

        std::cout << "UDP - Test - Received messages [" << receivedMessageCount << "/" << messagesToReceive * (endIndex - startIndex) << "] [~" << (static_cast<double>(receivedMessageCount) / static_cast<double>((messagesToReceive * (endIndex - startIndex)))) * 100 << "%] from forwarder." << std::endl;
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
        ForwarderGroup group = udpGroups.at(1);

        const size_t amountOfClients = 200;
		const size_t messagesToSend = 278;
		std::string groupId = "TestNumerousClients-group";
		std::vector<kt::UDPSocket> sockets;
        sockets.reserve(amountOfClients);

        ASSERT_EQ(0, forwarder->udpGroupMemberCount(group.groupId));

        // int bufferSize = 0;
        // socklen_t size = sizeof(bufferSize);
        // int err = getsockopt(udpSocket.getListeningSocket(), SOL_SOCKET, SO_RCVBUF, (char*)&bufferSize, &size);
        // std::cout << "Buffer res: " << err << ". Buffer size is [" << bufferSize << "]." << std::endl;
        for (size_t i = 0; i < amountOfClients; i++)
		{
			kt::UDPSocket socket;
            ASSERT_TRUE(socket.bind(kt::InternetProtocolVersion::IPV6));
			ASSERT_EQ(1, socket.sendTo("::1", group.receiverPort, "m").first);
			sockets.push_back(socket);
            std::this_thread::sleep_for(1ms);
		}
		std::this_thread::sleep_for(10ms);

        ASSERT_TRUE(forwarder->udpGroupMemberCount(group.groupId) > 0 && forwarder->udpGroupMemberCount(group.groupId) <= amountOfClients);

		std::string message = "TestNumerousClients";
        kt::UDPSocket sendSocket;
        for (size_t i = 0; i < messagesToSend; i++)
		{
			ASSERT_EQ(message.size() + std::to_string(i).size(), sendSocket.sendTo("::1", group.senderPort, message + std::to_string(i)).first);
            std::this_thread::sleep_for(2ms);
		}
        sendSocket.close();

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
				kt::UDPSocket& socket = sockets[clientIndex];
                if (socket.ready())
                {
                    std::expected<std::pair<std::string, kt::SocketAddress>, int> result = socket.receiveFrom(message.size() + std::to_string(i).size());
                    ASSERT_TRUE(result);
                    ASSERT_TRUE(result.value().first.rfind(message, 0) == 0);
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
            kt::UDPSocket& socket = sockets[i];
            socket.close();
        }

        std::cout << "UDP - Test - Received messages [" << receivedMessageCount << "/" << messagesToSend * amountOfClients << "] [~" << (static_cast<double>(receivedMessageCount) / static_cast<double>((messagesToSend * amountOfClients))) * 100 << "%] from forwarder." << std::endl;
        
        // We cannot assert this since there can be messages that are lost or dropped because of the use of UDP
        // ASSERT_EQ(messagesToSend * amountOfClients, receivedMessageCount);

        // We'll assert that we got more than 1 message and less than or equal to the upper limit
        ASSERT_GT(receivedMessageCount, 0);
        ASSERT_LE(receivedMessageCount, messagesToSend * amountOfClients);
    }
}
