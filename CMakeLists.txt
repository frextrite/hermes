cmake_minimum_required(VERSION 4.1)
project(Hermes)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

find_package(Boost 1.89 REQUIRED)

set(LIBRARY_SOURCES
    Implementation/WebSocketMessenger.cpp
    Implementation/Beast/Client/BeastClient.cpp
    Implementation/Beast/Connector/DirectConnector.cpp
    Implementation/Beast/Connector/ProxyConnector.cpp
)

add_library(hermes STATIC ${LIBRARY_SOURCES})
target_compile_features(hermes PRIVATE cxx_std_20)

target_include_directories(hermes PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(hermes PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Include
    ${Boost_INCLUDE_DIRS}
)