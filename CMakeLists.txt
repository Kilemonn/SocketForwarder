cmake_minimum_required(VERSION 3.14)

include(ExternalProject)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(PROJECT_NAME SocketForwarder)

project(${PROJECT_NAME} VERSION 1.0)

ExternalProject_Add(CppSocketLibrary
    GIT_REPOSITORY https://github.com/Kilemonn/Cpp-SocketLibrary.git
    GIT_TAG v0.4.3
    INSTALL_COMMAND ""
    UPDATE_DISCONNECTED FALSE
    BUILD_IN_SOURCE TRUE
)

ExternalProject_Get_Property(CppSocketLibrary SOURCE_DIR)
set(SOCKET_LIB_SOURCE ${SOURCE_DIR})

set(FORWARDER_SOURCE
    socket-forwarder/main.cpp
    socket-forwarder/environment/Environment.cpp
    socket-forwarder/forwarder/Forwarder.cpp
    socket-forwarder/sockets/Sockets.cpp
)

add_executable(SocketForwarder ${FORWARDER_SOURCE})

target_include_directories(SocketForwarder
    PUBLIC ${SOCKET_LIB_SOURCE}/src
)

target_link_libraries(SocketForwarder 
    PUBLIC ${SOCKET_LIB_SOURCE}/libCppSocketLibrary.a
    pthread
    bluetooth
)

add_subdirectory(tests)
