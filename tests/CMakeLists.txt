cmake_minimum_required(VERSION 3.12)

# Find Google Test
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    # If GTest is not found, download and build it
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/v1.14.0.zip
        URL_HASH SHA256=1f357c27ca988c3f7c6b4bf68a9395005ac6761f034046e9dde0896e3aba00e4
        DOWNLOAD_EXTRACT_TIMESTAMP ON
    )
    FetchContent_MakeAvailable(googletest)
endif()

# Create test executable
set(TEST_SOURCES
    test_http_request.cpp
    test_http_response.cpp
    test_route_registry.cpp
    test_config.cpp
    test_debug_logger.cpp
    test_integration.cpp
    test_middleware.cpp
    test_middleware_pipeline.cpp
    test_middleware_config.cpp
    test_middleware_factory.cpp
    test_async_middleware.cpp
    test_auth_middleware.cpp
    test_authz_middleware.cpp
    test_rate_limit_middleware.cpp
    test_logging_middleware.cpp
    test_cors_middleware.cpp
    test_plugin_system.cpp
)

add_executable(cppSwitchboard_tests ${TEST_SOURCES})

target_link_libraries(cppSwitchboard_tests
    PRIVATE
        cppSwitchboard::cppSwitchboard
        gtest_main
        gtest
        gmock
)

target_include_directories(cppSwitchboard_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Add tests to CTest
include(GoogleTest)
gtest_discover_tests(cppSwitchboard_tests)

# Add individual test registration
add_test(NAME cppSwitchboard_unit_tests COMMAND cppSwitchboard_tests) 