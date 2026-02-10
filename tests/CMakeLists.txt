
find_package(GTest CONFIG QUIET)

include(FetchContent)

if (GTest_FOUND)
    message(STATUS "GTest found: ${GTest_VERSION}")
else()
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
    )
    if (WIN32)
        # For Windows: Prevent overriding the parent project's compiler/linker on Windows
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    endif()
    FetchContent_MakeAvailable(googletest)
endif()

find_package(slick-logger 1.0.4 CONFIG QUIET)
if (NOT slick-logger_FOUND)
    message(STATUS "Fetching slick-logger...")
    set(BUILD_SLICK_LOGGER_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_SLICK_LOGGER_TESTING OFF CACHE BOOL "" FORCE)
    set(BUILD_SLICK_LOGGER_BENCHMARKS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        slick-logger
        GIT_REPOSITORY https://github.com/SlickQuant/slick-logger.git
        GIT_TAG v1.0.4
    )
    FetchContent_MakeAvailable(slick-logger)
endif()

include(GoogleTest)

add_executable(coinbase_advance_tests rest_api_tests.cpp websocket_tests.cpp rest_awaitable_tests.cpp)
target_include_directories(coinbase_advance_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(coinbase_advance_tests PRIVATE coinbase-advanced-cpp slick::net GTest::gtest_main)

# Discover tests with increased timeout
gtest_discover_tests(coinbase_advance_tests DISCOVERY_TIMEOUT 30)

# Set test properties
set_target_properties(coinbase_advance_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)
