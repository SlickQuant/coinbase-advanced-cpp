cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)

project(coinbase_advanced_cpp
    VERSION 0.1.0
    LANGUAGES CXX)

message(STATUS "coinbase_advance_cpp: ${PROJECT_VERSION}")

if (CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-DDEBUG)
endif()

if (CMAKE_BUILD_TYPE MATCHES Release)
    add_definitions(-DNDEBUG)
endif()

find_package(nlohmann_json REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(jwt-cpp CONFIG REQUIRED)

include(FetchContent)
set(BUILD_SLICK_NET_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_SLICK_NET_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    slick_net
    GIT_REPOSITORY https://github.com/SlickQuant/slick_net.git
    GIT_TAG v1.2.1
)
FetchContent_MakeAvailable(slick_net)

message(STATUS "Slick Queue: ${slick_queue_SOURCE_DIR}")
message(STATUS "Slick Net: ${slick_net_SOURCE_DIR}")

add_library(coinbase_advanced_cpp INTERFACE)
target_include_directories(coinbase_advanced_cpp INTERFACE include)
target_link_libraries(coinbase_advanced_cpp INTERFACE slick_net #[[Boost::asio Boost::beast Boost::context OpenSSL::SSL OpenSSL::Crypto]])
target_precompile_headers(coinbase_advanced_cpp INTERFACE 
    <nlohmann/json.hpp>
    <atomic>
    <cstdlib>
    <functional>
    <unordered_map>
    <mutex>
    <csignal>
    <thread>
    # Boost headers
    <boost/beast/core.hpp>
    <boost/beast/websocket.hpp>
    <boost/beast/websocket/ssl.hpp>
    <boost/beast/http.hpp>
    <boost/asio/ssl.hpp>
    <boost/asio/strand.hpp>
    <boost/asio/awaitable.hpp>
    <boost/asio/co_spawn.hpp>
    <boost/asio/signal_set.hpp>
    <boost/asio/ip/tcp.hpp>
    <boost/asio/ssl/stream.hpp>
    <boost/asio/connect.hpp>
    <boost/asio/post.hpp>
    <boost/asio/as_tuple.hpp>
    # External dependencies
    <slick/queue.h>
)
if (MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "-O2")
    add_definitions(-D_WIN32_WINNT=0x0A00)
    set(CMAKE_SUPPRESS_REGENERATION true)   # supress zero_check
    set_target_properties(coinbase_advanced_cpp PROPERTIES LINK_INCREMENTAL ON)
    target_compile_options(coinbase_advanced_cpp INTERFACE "/MP" "/FS" "/bigobj" "/wd4101")
    # Faster linking
    add_link_options(/DEBUG:FASTLINK)
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-O3")
endif()

option(BUILD_COINBASE_ADVANCED_TESTS "Build tests" OFF)
if (BUILD_COINBASE_ADVANCED_TESTS)
    message(STATUS "Building coinbase_advanced_cpp tests")
    add_subdirectory(tests)
else()
    message(STATUS "Skipping coinbase_advanced_cpp tests")
endif()