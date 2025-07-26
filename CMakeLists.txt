cmake_minimum_required(VERSION 3.28)
project(arb VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS system thread)
find_package(OpenSSL REQUIRED)

include(FetchContent)
FetchContent_Declare(
  websocketpp
  GIT_REPOSITORY https://github.com/zaphoyd/websocketpp.git
  GIT_TAG develop
)
FetchContent_MakeAvailable(websocketpp)

FetchContent_Declare(
  simdjson
  GIT_REPOSITORY https://github.com/simdjson/simdjson.git
  GIT_TAG tags/v3.13.0
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(simdjson)

set(SOURCES
    src/main.cpp
    src/ws_client.cpp
    src/utils.cpp
    src/orderbook.cpp
    src/sqlite3.c
)

add_executable(arb ${SOURCES})

option(FAKE "Enable fake mode" OFF)
if(FAKE)
    target_compile_definitions(arb PRIVATE FAKE)
endif()

target_include_directories(arb
    PRIVATE
        ${Boost_INCLUDE_DIRS}
        ${OPENSSL_INCLUDE_DIR}
        ${websocketpp_SOURCE_DIR}
        ${simdjson_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(arb
    PRIVATE
        Boost::system
        Boost::thread
        OpenSSL::SSL
        OpenSSL::Crypto
        simdjson::simdjson
)
