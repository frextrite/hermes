cmake_minimum_required(VERSION 4.1)
project(Hermes)

option(WS_BUILD_EXAMPLES "Build examples" ON)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

find_package(Boost 1.89 REQUIRED)
find_package(OpenSSL REQUIRED)

set(LIBRARY_SOURCES
    Implementation/WebSocketMessenger.cpp
    Implementation/Beast/Client/BeastClient.cpp
    Implementation/Beast/Connector/DirectConnector.cpp
    Implementation/Beast/Connector/ProxyConnector.cpp
)

add_library(hermes STATIC ${LIBRARY_SOURCES})
target_compile_features(hermes PUBLIC cxx_std_20)

target_link_libraries(hermes
    PUBLIC
        OpenSSL::SSL
        OpenSSL::Crypto
)

target_include_directories(hermes PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(hermes PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Include
    ${Boost_INCLUDE_DIRS}
)

if (WS_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()