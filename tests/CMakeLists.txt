
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

set(BUILD_SLICK_LOGGER_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_SLICK_LOGGER_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_SLICK_LOGGER_BENCHMARKS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    slick_logger
    GIT_REPOSITORY https://github.com/SlickQuant/slick_logger.git
    GIT_TAG v1.0.1
)
FetchContent_MakeAvailable(slick_logger)

enable_testing()

include(GoogleTest)

add_executable(coinbase_advance_tests rest_api_tests.cpp websocket_tests.cpp)
target_include_directories(coinbase_advance_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(coinbase_advance_tests PRIVATE coinbase_advanced_cpp slick_net GTest::gtest_main)

# Discover tests with increased timeout
gtest_discover_tests(coinbase_advance_tests DISCOVERY_TIMEOUT 30)

# Set test properties
set_target_properties(coinbase_advance_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)