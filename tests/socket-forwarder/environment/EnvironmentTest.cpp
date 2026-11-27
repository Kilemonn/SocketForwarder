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

    TEST(EnvironmentTest, getVariablesWithPrefix)
    {
        std::string prefix = TCP_GROUP_PREFIX;
        std::string property1 = prefix + "my-group.sender.port";
        std::string property1AndValue = property1 + "=12345";

        std::vector<std::string> envProperties = { property1AndValue.data(), "banana=value", "cherry" };

        std::vector<char*> envp(envProperties.size());
        for (auto& s : envProperties) {
            envp.push_back(s.data());
        }
        envp.push_back(nullptr);
        std::vector<std::pair<std::string, std::string>> variables = getVariablesWithPrefix(prefix, envp.data());

        ASSERT_EQ(1, variables.size());
        auto [key, value] = variables.at(0);
        ASSERT_EQ(property1, key);
        ASSERT_EQ("12345", value);
    }

    TEST(EnvironmentTest, getGroupsWithPrefix)
    {
        std::string prefix = TCP_GROUP_PREFIX;
        std::string group1Property1 = prefix + "group1.property1";
        std::string group1Property1WithValue = group1Property1 + "=99999";

        std::string group1Property2 = prefix + "group1.property2";
        std::string group1Property2WithValue = group1Property2 + "=11111";

        std::string group2Property1 = prefix + "group2.property1";
        std::string group2Property1WithValue = group2Property1 + "=22222";

        std::string group3Property1 = prefix + "group3.property1";
        std::string group3Property1WithValue = group3Property1 + "=33333";

        std::vector<std::string> envProperties = { group1Property1WithValue.data(), group1Property2WithValue.data(), group2Property1WithValue.data(), 
            group3Property1WithValue.data(), "without-prefix=55555" };

        std::vector<char*> envp(envProperties.size());
        for (auto& s : envProperties) {
            envp.push_back(s.data());
        }
        envp.push_back(nullptr);
        
        std::vector<std::pair<std::string, std::string>> variables = getVariablesWithPrefix(prefix, envp.data());
        std::set<std::string> groups = getGroupsWithPrefix(prefix, variables);

        ASSERT_EQ(3, groups.size());
        ASSERT_NE(groups.find("group1"), groups.end());
        ASSERT_NE(groups.find("group2"), groups.end());
        ASSERT_NE(groups.find("group3"), groups.end());
    }
}
