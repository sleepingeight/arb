cmake_minimum_required(VERSION 3.28)

project(arb VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS system thread)
find_package(OpenSSL REQUIRED)

include(FetchContent)
FetchContent_Declare(
  simdjson
  GIT_REPOSITORY https://github.com/simdjson/simdjson.git
  GIT_TAG  tags/v3.10.0
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(simdjson)

FetchContent_Declare(
    websocketpp
    GIT_REPOSITORY https://github.com/zaphoyd/websocketpp.git
    GIT_TAG master
)
FetchContent_MakeAvailable(websocketpp)

add_executable(main src/main.cpp)
target_include_directories(main PRIVATE ${Boost_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR} ${websocketpp_SOURCE_DIR} ${simdjson_SOURCE_DIR})
target_include_directories(main PUBLIC src)
target_link_libraries(main PRIVATE Boost::system Boost::thread OpenSSL::SSL OpenSSL::Crypto)

