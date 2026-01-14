cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)

project(coinbase-advanced-cpp
    VERSION 0.1.0
    LANGUAGES CXX)

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
    slick-net
    GIT_REPOSITORY https://github.com/SlickQuant/slick-net.git
    GIT_TAG v1.2.2
)
FetchContent_MakeAvailable(slick-net)

add_library(coinbase-advanced-cpp INTERFACE)
target_include_directories(coinbase-advanced-cpp INTERFACE include)
target_link_libraries(coinbase-advanced-cpp INTERFACE slick::net jwt-cpp::jwt-cpp)
target_precompile_headers(coinbase-advanced-cpp INTERFACE 
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
    set_target_properties(coinbase-advanced-cpp PROPERTIES LINK_INCREMENTAL ON)
    target_compile_options(coinbase-advanced-cpp INTERFACE "/MP" "/FS" "/bigobj" "/wd4101")
    # Faster linking
    add_link_options(/DEBUG:FASTLINK)
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-O3")
endif()

option(BUILD_COINBASE_ADVANCED_TESTS "Build tests" ON)
if (BUILD_COINBASE_ADVANCED_TESTS)
    message(STATUS "Building coinbase-advanced-cpp tests")
    enable_testing()
    add_subdirectory(tests)
else()
    message(STATUS "Skipping coinbase-advanced-cpp tests")
endif()

message(STATUS "coinbase-advance-cpp: ${PROJECT_VERSION}")