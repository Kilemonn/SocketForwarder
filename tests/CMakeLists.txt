cmake_minimum_required(VERSION 3.14)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PROJECT_NAME SocketForwarderTests)
project(${PROJECT_NAME})

# Reference: https://github.com/google/googletest/tree/main/googletest#incorporating-into-an-existing-cmake-project
include(FetchContent)

FetchContent_Declare(
  googletest
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  # Specify the commit you depend on and update it regularly.
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

set(FORWARDER_TEST_SOURCE
    socket-forwarder/TCPSocketForwarderTest.cpp
)

# This is duplicated from the parent CMakeLists.txt, since these are needed to build the tests
set(FORWARDER_SOURCE_FOR_TEST
    ../socket-forwarder/environment/Environment.cpp
    ../socket-forwarder/forwarder/Forwarder.cpp
    ../socket-forwarder/sockets/Sockets.cpp
)

add_executable(${PROJECT_NAME} ${FORWARDER_TEST_SOURCE} ${FORWARDER_SOURCE_FOR_TEST})

target_include_directories(SocketForwarderTests
    PUBLIC ${SOCKET_LIB_SOURCE}/src
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    gtest_main
    gtest
    bluetooth
    pthread
    PUBLIC ${SOCKET_LIB_SOURCE}/libCppSocketLibrary.a
)

# Enable unit testing
enable_testing()

add_test(${PROJECT_NAME} ${PROJECT_NAME})

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} DEPENDS ${PROJECT_NAME})
