#include <gtest/gtest.h>
#include "../../../socket-forwarder/environment/Environment.h"

namespace forwarder
{
    TEST(EnvironmentTest, split_withElements)
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

    TEST(EnvironmentTest, split_emptyInput)
    {
        std::string input = "";
        std::string delimiter = "-";

        std::vector<std::string> strings = forwarder::split(input, delimiter);
        std::vector<std::string> expected = { "" };

        ASSERT_EQ(expected.size(), strings.size());
        for (int i = 0; i < expected.size(); i++)
        {
            ASSERT_EQ(expected[i], strings[i]);
        }
    }

    TEST(EnvironmentTest, split_delimiterDoesNotExist)
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
}
