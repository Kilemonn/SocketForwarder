include(FetchContent)

FetchContent_Declare(
  googletest
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
)
FetchContent_MakeAvailable(googletest)

set(FORWARDER_TEST_SOURCE
    socket-forwarder/forwarder/TCPSocketForwarderTest.cpp
    socket-forwarder/forwarder/UDPSocketForwarderTest.cpp
    socket-forwarder/environment/EnvironmentTest.cpp
)

add_executable(SocketForwarderTests ${FORWARDER_TEST_SOURCE})

target_link_libraries(SocketForwarderTests PRIVATE
    SocketForwarder_lib   # brings includes, CppSocketLibrary, log4cxx, bluetooth, etc.
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(SocketForwarderTests)

add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS SocketForwarderTests
)
