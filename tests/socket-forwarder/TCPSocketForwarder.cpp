#include <gtest/gtest.h>
#include <thread>

#include "../../socket-forwarder/forwarder/Forwarder.h"

namespace forwarder
{
    TEST(SocketForwarderTest, Test1)
	{
		std::pair<std::thread, std::thread> runningThreads = startTCPForwarder();
	}
}
