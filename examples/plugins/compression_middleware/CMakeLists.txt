# Compression Middleware Plugin CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# Project name and version
project(CompressionMiddlewarePlugin VERSION 1.0.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZLIB REQUIRED zlib)

# Include directories for cppSwitchboard
set(CPPSWITCH_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../include")

# Plugin source files
set(PLUGIN_SOURCES
    compression_middleware.cpp
    compression_middleware.h
)

# Create shared library for the plugin
add_library(compression_middleware SHARED ${PLUGIN_SOURCES})

# Set library properties
set_target_properties(compression_middleware PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PREFIX ""  # Remove 'lib' prefix for plugins
)

# Include directories
target_include_directories(compression_middleware PRIVATE
    ${CPPSWITCH_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link libraries
target_link_libraries(compression_middleware
    ${ZLIB_LIBRARIES}
)

# Compiler flags
target_compile_options(compression_middleware PRIVATE
    ${ZLIB_CFLAGS_OTHER}
    -fPIC
    -fvisibility=hidden  # Hide symbols by default, export only what's needed
)

# Install plugin to appropriate directory
install(TARGETS compression_middleware
    LIBRARY DESTINATION lib/cppSwitchboard/plugins
    RUNTIME DESTINATION lib/cppSwitchboard/plugins
)

# Install plugin header for development
install(FILES compression_middleware.h
    DESTINATION include/cppSwitchboard/plugins
)

# Create plugin configuration example
set(PLUGIN_CONFIG_EXAMPLE "${CMAKE_CURRENT_BINARY_DIR}/compression_plugin_config.yaml")
file(WRITE ${PLUGIN_CONFIG_EXAMPLE}
"# Compression Middleware Plugin Configuration Example
middleware:
  - name: compression
    enabled: true
    config:
      min_size: 1024
      compression_level: 6
      compression_types:
        - text/html
        - text/css
        - text/javascript
        - text/plain
        - application/json
        - application/xml
        - application/javascript
      excluded_paths:
        - /api/binary-data
        - /images/
")

install(FILES ${PLUGIN_CONFIG_EXAMPLE}
    DESTINATION share/cppSwitchboard/examples
)

# Add target for plugin development testing
if(BUILD_TESTING)
    # Simple test to verify plugin loads correctly
    add_executable(test_compression_plugin test_plugin.cpp)
    target_include_directories(test_compression_plugin PRIVATE
        ${CPPSWITCH_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(test_compression_plugin
        ${CMAKE_DL_LIBS}  # For dlopen/dlclose
    )
    
    add_test(NAME CompressionPluginLoadTest COMMAND test_compression_plugin)
endif()

# Plugin development help target
add_custom_target(plugin_help
    COMMAND ${CMAKE_COMMAND} -E echo "Compression Middleware Plugin Build Instructions:"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "To build the plugin:"
    COMMAND ${CMAKE_COMMAND} -E echo "  mkdir build && cd build"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake .."
    COMMAND ${CMAKE_COMMAND} -E echo "  make"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "To install the plugin:"
    COMMAND ${CMAKE_COMMAND} -E echo "  make install"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "To test the plugin:"
    COMMAND ${CMAKE_COMMAND} -E echo "  make test"
    COMMAND ${CMAKE_COMMAND} -E echo ""
)

# Package configuration
set(CPACK_PACKAGE_NAME "cppSwitchboard-compression-plugin")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "HTTP compression middleware plugin for cppSwitchboard")
set(CPACK_PACKAGE_VENDOR "Jordan Vrtanoski")
set(CPACK_GENERATOR "TGZ;DEB")

include(CPack) 