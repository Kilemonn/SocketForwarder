#include <gtest/gtest.h>

#include "../../../socket-forwarder/sockets/Sockets.h"

namespace forwarder
{
    TEST(SocketsTest, SocketsSplit_withElements)
    {
        std::string input = "some-text-that-is-separated...";
        std::string delimiter = "-";

        std::vector<std::string> strings = split(input, delimiter);
        std::vector<std::string> expected = {"some", "text", "that", "is", "separated..."};

        ASSERT_EQ(expected.size(), strings.size());
        for (int i = 0; i < expected.size(); i++)
        {
            ASSERT_EQ(expected[i], strings[i]);
        }
    }

    TEST(SocketsTest, SocketsSplit_emptyInput)
    {
        std::string input = "";
        std::string delimiter = "-";

        std::vector<std::string> strings = split(input, delimiter);
        std::vector<std::string> expected = { "" };

        ASSERT_EQ(expected.size(), strings.size());
        for (int i = 0; i < expected.size(); i++)
        {
            ASSERT_EQ(expected[i], strings[i]);
        }
    }

    TEST(SocketsTest, SocketsSplit_delimiterDoesNotExist)
    {
        std::string input = "some-text-that-is-separated...";
        std::string delimiter = "!";

        std::vector<std::string> strings = split(input, delimiter);
        std::vector<std::string> expected = { "some-text-that-is-separated..." };

        ASSERT_EQ(expected.size(), strings.size());
        for (int i = 0; i < expected.size(); i++)
        {
            ASSERT_EQ(expected[i], strings[i]);
        }
    }

    TEST(SocketsTest, getPreconfiguredTCPAddresses)
    {
        std::string input = "group1:localhost:54321,group1:localhost:11223,group2:localhost:2255";
        std::unordered_map<std::string, std::vector<kt::SocketAddress>> address = getPreconfiguredTCPAddresses(input);
    
        auto group1 = address.find("group1");
        ASSERT_NE(address.end(), group1);
        ASSERT_EQ(2, group1->second.size());
        for (const kt::SocketAddress addr : group1->second)
        {
            unsigned short port = kt::getPortNumber(addr);
            ASSERT_TRUE(port == 54321 || port == 11223);
        }

        auto group2 = address.find("group2");
        ASSERT_EQ(1, group2->second.size());
        ASSERT_NE(address.end(), group2);
        for (const kt::SocketAddress addr : group2->second)
        {
            ASSERT_EQ(2255, kt::getPortNumber(addr));
        }

        ASSERT_EQ(address.end(), address.find("group3"));
    }

    TEST(SocketsTest, getPreconfiguredUDPAddresses)
    {
        std::string input = "localhost:33333,localhost:12345";
        std::vector<kt::SocketAddress> addresses = getPreconfiguredUDPAddresses(input);

        ASSERT_EQ(2, addresses.size());
        for (const kt::SocketAddress addr : addresses)
        {
            unsigned short port = kt::getPortNumber(addr);
            ASSERT_TRUE(port == 33333 || port == 12345);
        }
    }
}
