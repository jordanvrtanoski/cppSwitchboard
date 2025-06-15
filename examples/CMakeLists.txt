cmake_minimum_required(VERSION 3.12)
project(cppSwitchboard_examples)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find the cppSwitchboard library
find_package(cppSwitchboard REQUIRED)

# Examples
add_executable(basic_server basic_server.cpp)
target_link_libraries(basic_server cppSwitchboard)

add_executable(middleware_example middleware_example.cpp)
target_link_libraries(middleware_example cppSwitchboard)

add_executable(auth_example auth_example.cpp)
target_link_libraries(auth_example cppSwitchboard)

add_executable(custom_middleware custom_middleware.cpp)
target_link_libraries(custom_middleware cppSwitchboard)

# Install examples
install(TARGETS basic_server middleware_example auth_example custom_middleware
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/examples
)

# Copy to parent build directory for easy access
set_target_properties(basic_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
) 