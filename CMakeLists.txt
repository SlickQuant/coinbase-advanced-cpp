cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)

project(coinbase-advanced-cpp
    VERSION 0.2.0
    LANGUAGES CXX)

if (CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-DDEBUG)
endif()

if (CMAKE_BUILD_TYPE MATCHES Release)
    add_definitions(-DNDEBUG)
endif()

find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenSSL CONFIG REQUIRED)
find_package(jwt-cpp CONFIG REQUIRED)

find_package(slick-net 2.0.0 CONFIG QUIET)
if (NOT slick-net_FOUND)
    message(STATUS "fetching slick-net...")
    include(FetchContent)
    FetchContent_Declare(
        slick-net
        GIT_REPOSITORY https://github.com/SlickQuant/slick-net.git
        GIT_TAG v2.0.0
    )
    FetchContent_MakeAvailable(slick-net)
else()
    message(STATUS "Found slick-net: ${slick-net_VERSION}")
endif()

add_library(coinbase-advanced-cpp STATIC
    src/auth.cpp
    src/rest.cpp
    src/rest_awaitable.cpp
    src/websocket.cpp
    src/utils.cpp
    src/logging.cpp
)
add_library(slick::coinbase-advanced-cpp ALIAS coinbase-advanced-cpp)
target_include_directories(coinbase-advanced-cpp PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(coinbase-advanced-cpp PUBLIC slick::net jwt-cpp::jwt-cpp)

# PRIVATE precompiled headers for faster static library compilation
target_precompile_headers(coinbase-advanced-cpp PRIVATE
    <nlohmann/json.hpp>
    <string>
    <vector>
    <memory>
    <coroutine>
)

if (MSVC)
    add_definitions(-D_WIN32_WINNT=0x0A00)
    set(CMAKE_SUPPRESS_REGENERATION true)   # supress zero_check
    set_target_properties(coinbase-advanced-cpp PROPERTIES LINK_INCREMENTAL ON)
    target_compile_options(coinbase-advanced-cpp PRIVATE
        /MP
        /FS
        /bigobj
        /wd4101
        /W4
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Release>:/GL>  # Whole program optimization
    )
    # Faster linking
    target_link_options(coinbase-advanced-cpp PRIVATE
        $<$<CONFIG:Debug>:/DEBUG:FASTLINK>
    )
else()
    target_compile_options(coinbase-advanced-cpp PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Release>:-O3>
    )
endif()

set(BUILD_COINBASE_ADVANCED_TESTS_DEFAULT ON)
if (NOT CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(BUILD_COINBASE_ADVANCED_TESTS_DEFAULT OFF)
endif()
option(BUILD_COINBASE_ADVANCED_TESTS "Build tests" ${BUILD_COINBASE_ADVANCED_TESTS_DEFAULT})
if (BUILD_COINBASE_ADVANCED_TESTS)
    message(STATUS "Building coinbase-advanced-cpp tests")
    enable_testing()
    add_subdirectory(tests)
else()
    message(STATUS "Skipping coinbase-advanced-cpp tests")
endif()

# Installation rules
install(DIRECTORY include/ DESTINATION include)

# Install CMake package configuration files for vcpkg
install(TARGETS coinbase-advanced-cpp EXPORT coinbase-advanced-cppTargets)

install(EXPORT coinbase-advanced-cppTargets
    FILE coinbase-advanced-cppTargets.cmake
    NAMESPACE slick::
    DESTINATION lib/cmake/coinbase-advanced-cpp
)

include(CMakePackageConfigHelpers)

# Generate the config file
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/coinbase-advanced-cppConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/coinbase-advanced-cppConfig.cmake
    INSTALL_DESTINATION lib/cmake/coinbase-advanced-cpp
)

# Generate version file
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/coinbase-advanced-cppConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install config files
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/coinbase-advanced-cppConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/coinbase-advanced-cppConfigVersion.cmake
    DESTINATION lib/cmake/coinbase-advanced-cpp
)

message(STATUS "${PROJECT_NAME}: ${PROJECT_VERSION}")
