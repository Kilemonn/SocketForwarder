cmake_minimum_required(VERSION 3.20)
project(SocketForwarder VERSION 0.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(ExternalProject)

ExternalProject_Add(CppSocketLibrary
    GIT_REPOSITORY https://github.com/Kilemonn/Cpp-SocketLibrary.git
    GIT_TAG v0.7.1
    INSTALL_COMMAND ""
    UPDATE_DISCONNECTED FALSE
    BUILD_IN_SOURCE TRUE
)
ExternalProject_Get_Property(CppSocketLibrary SOURCE_DIR)
set(SOCKET_LIB_SOURCE ${SOURCE_DIR})

find_package(log4cxx REQUIRED)

set(FORWARDER_SOURCE
    socket-forwarder/environment/Environment.cpp
    socket-forwarder/forwarder/Forwarder.cpp
    socket-forwarder/forwarder/NetworkForwarder.cpp
    socket-forwarder/forwarder/TCPForwarder.cpp
    socket-forwarder/forwarder/UDPForwarder.cpp
    socket-forwarder/forwarder/ForwarderConfig.cpp
    socket-forwarder/logger/Logger.cpp
)

add_library(${PROJECT_NAME}_lib ${FORWARDER_SOURCE})

# Make sure the external project is cloned/built before compiling the lib
add_dependencies(${PROJECT_NAME}_lib CppSocketLibrary)

target_include_directories(${PROJECT_NAME}_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/socket-forwarder
    ${SOCKET_LIB_SOURCE}/src
)

target_link_libraries(${PROJECT_NAME}_lib PUBLIC
    ${SOCKET_LIB_SOURCE}/libCppSocketLibrary.a
    log4cxx
    pthread
    bluetooth
    uuid
)

# The executable is just main + the library
add_executable(${PROJECT_NAME} socket-forwarder/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_lib)

enable_testing()
add_subdirectory(tests)
