cmake_minimum_required(VERSION 3.12)
project(cppSwitchboard 
    VERSION 1.1.0
    DESCRIPTION "A high-performance HTTP middleware framework for C++ with comprehensive middleware pipeline support"
    LANGUAGES CXX
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include GNUInstallDirs for standard installation directories
include(GNUInstallDirs)

# Find required dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(NGHTTP2 REQUIRED libnghttp2)
find_package(Boost REQUIRED COMPONENTS system)
find_package(yaml-cpp QUIET)

# If yaml-cpp is not found, try pkg-config
if(NOT yaml-cpp_FOUND)
    pkg_check_modules(YAML_CPP REQUIRED yaml-cpp)
endif()

# Find OpenSSL
find_package(OpenSSL REQUIRED)

# Find nlohmann-json
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    # Try pkg-config for system package
    pkg_check_modules(NLOHMANN_JSON QUIET nlohmann_json)
endif()

# Library source files
set(CPPSWITCHBOARD_SOURCES
    src/config.cpp
    src/debug_logger.cpp
    src/http_handler.cpp
    src/http_request.cpp
    src/http_response.cpp
    src/http_server.cpp
    src/http2_server_impl.cpp
    src/route_registry.cpp
    src/middleware.cpp
    src/middleware_pipeline.cpp
    src/middleware_config.cpp
    src/middleware_factory.cpp
    src/async_middleware.cpp
    src/plugin_manager.cpp
    src/middleware_plugin.cpp
    src/middleware/auth_middleware.cpp
    src/middleware/authz_middleware.cpp
    src/middleware/rate_limit_middleware.cpp
    src/middleware/logging_middleware.cpp
    src/middleware/cors_middleware.cpp
)

# Library header files
set(CPPSWITCHBOARD_HEADERS
    include/cppSwitchboard/config.h
    include/cppSwitchboard/debug_logger.h
    include/cppSwitchboard/http_handler.h
    include/cppSwitchboard/http_request.h
    include/cppSwitchboard/http_response.h
    include/cppSwitchboard/http_server.h
    include/cppSwitchboard/http2_server_impl.h
    include/cppSwitchboard/route_registry.h
    include/cppSwitchboard/middleware.h
    include/cppSwitchboard/middleware_pipeline.h
    include/cppSwitchboard/middleware_config.h
    include/cppSwitchboard/middleware_factory.h
    include/cppSwitchboard/async_middleware.h
    include/cppSwitchboard/plugin_manager.h
    include/cppSwitchboard/middleware_plugin.h
    include/cppSwitchboard/middleware/auth_middleware.h
    include/cppSwitchboard/middleware/authz_middleware.h
    include/cppSwitchboard/middleware/rate_limit_middleware.h
    include/cppSwitchboard/middleware/logging_middleware.h
    include/cppSwitchboard/middleware/cors_middleware.h
)

# Create the library
add_library(cppSwitchboard ${CPPSWITCHBOARD_SOURCES})
add_library(cppSwitchboard::cppSwitchboard ALIAS cppSwitchboard)

# Set target properties
set_target_properties(cppSwitchboard PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "${CPPSWITCHBOARD_HEADERS}"
    EXPORT_NAME cppSwitchboard
)

# Include directories
target_include_directories(cppSwitchboard
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link dependencies
target_link_libraries(cppSwitchboard
    PUBLIC
        ${NGHTTP2_LIBRARIES}
        Boost::system
        OpenSSL::SSL
        OpenSSL::Crypto
    PRIVATE
        ${CMAKE_THREAD_LIBS_INIT}
        ${CMAKE_DL_LIBS}
)

if(yaml-cpp_FOUND)
    target_link_libraries(cppSwitchboard PUBLIC yaml-cpp)
else()
    target_link_libraries(cppSwitchboard PUBLIC ${YAML_CPP_LIBRARIES})
    target_include_directories(cppSwitchboard PUBLIC ${YAML_CPP_INCLUDE_DIRS})
endif()

# Link nlohmann-json (system package provides header-only library)
if(nlohmann_json_FOUND)
    target_link_libraries(cppSwitchboard PRIVATE nlohmann_json::nlohmann_json)
elseif(NLOHMANN_JSON_FOUND)
    target_include_directories(cppSwitchboard PRIVATE ${NLOHMANN_JSON_INCLUDE_DIRS})
else()
    # System package installed - use system include path
    target_include_directories(cppSwitchboard PRIVATE "/usr/include/nlohmann")
endif()

# Compiler flags
target_compile_definitions(cppSwitchboard PRIVATE ${NGHTTP2_CFLAGS_OTHER})
target_include_directories(cppSwitchboard PRIVATE ${NGHTTP2_INCLUDE_DIRS})

# Compiler options
target_compile_options(cppSwitchboard PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# Installation
install(TARGETS cppSwitchboard
    EXPORT cppSwitchboardTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cppSwitchboard
)

# Install middleware headers separately to preserve directory structure
install(DIRECTORY include/cppSwitchboard/middleware
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cppSwitchboard
    FILES_MATCHING PATTERN "*.h"
)

# Install the export set
install(EXPORT cppSwitchboardTargets
    FILE cppSwitchboardTargets.cmake
    NAMESPACE cppSwitchboard::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cppSwitchboard
)

# Create and install the config file
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_LIST_DIR}/cmake/cppSwitchboardConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cppSwitchboardConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cppSwitchboard
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/cppSwitchboardConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/cppSwitchboardConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cppSwitchboardConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cppSwitchboard
)

# Generate and install pkg-config file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/cppSwitchboard.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/cppSwitchboard.pc
    @ONLY
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/cppSwitchboard.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# Export for build tree
export(EXPORT cppSwitchboardTargets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/cppSwitchboardTargets.cmake
    NAMESPACE cppSwitchboard::
)

# ============================================================================
# CPack Configuration for Packaging
# ============================================================================

# Basic package information
set(CPACK_PACKAGE_NAME "cppSwitchboard")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "High-performance HTTP middleware framework for C++ with comprehensive middleware pipeline support")
set(CPACK_PACKAGE_DESCRIPTION "cppSwitchboard is a modern C++17 HTTP middleware framework supporting HTTP/1.1 and HTTP/2 protocols. Features include: comprehensive middleware pipeline system, built-in authentication/authorization, rate limiting, CORS support, async middleware support, configuration-driven middleware composition, and plugin system for dynamic middleware loading.")
set(CPACK_PACKAGE_VENDOR "cppSwitchboard Contributors")
set(CPACK_PACKAGE_CONTACT "team@cppswitchboard.dev")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/cppswitchboard/cppswitchboard")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_PACKAGE_RELOCATABLE TRUE)

# Enable multiple package generators
set(CPACK_GENERATOR "TGZ;ZIP;DEB;RPM")
set(CPACK_SOURCE_GENERATOR "TGZ;ZIP")

# Package file naming
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-Source")

# Source package configuration
set(CPACK_SOURCE_IGNORE_FILES
    "/\\\\.git/"
    "/\\\\.github/"
    "/build/"
    "/\\\\.vscode/"
    "/\\\\.idea/"
    "\\\\.gitignore$"
    "\\\\.DS_Store$"
    "~$"
    "\\\\.swp$"
    "\\\\.tmp$"
)

# ============================================================================
# DEB Package Configuration
# ============================================================================

set(CPACK_DEBIAN_PACKAGE_NAME "libcppswitchboard")
set(CPACK_DEBIAN_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "cppSwitchboard Contributors <team@cppswitchboard.dev>")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION_SUMMARY}\n ${CPACK_PACKAGE_DESCRIPTION}")
set(CPACK_DEBIAN_PACKAGE_SECTION "libs")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${CPACK_PACKAGE_HOMEPAGE_URL}")

# Debian dependencies (adjust versions based on target distribution)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libnghttp2-14 (>= 1.43.0), libboost-system1.74.0 (>= 1.74.0), libyaml-cpp0.7 (>= 0.7.0), libssl3 (>= 3.0.0), libc6 (>= 2.34)")

# Development package dependencies  
set(CPACK_DEBIAN_PACKAGE_SUGGESTS "libcppswitchboard-dev, libnghttp2-dev, libboost-dev, libyaml-cpp-dev, libssl-dev")

# Enable automatic dependency detection
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS ON)

# ============================================================================
# RPM Package Configuration
# ============================================================================

set(CPACK_RPM_PACKAGE_NAME "cppSwitchboard")
set(CPACK_RPM_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")
set(CPACK_RPM_PACKAGE_SUMMARY "${CPACK_PACKAGE_DESCRIPTION_SUMMARY}")
set(CPACK_RPM_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION}")
set(CPACK_RPM_PACKAGE_GROUP "Development/Libraries")
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_URL "${CPACK_PACKAGE_HOMEPAGE_URL}")
set(CPACK_RPM_PACKAGE_VENDOR "${CPACK_PACKAGE_VENDOR}")

# RPM dependencies (adjust based on target distribution)
set(CPACK_RPM_PACKAGE_REQUIRES "libnghttp2 >= 1.43.0, boost-system >= 1.74.0, yaml-cpp >= 0.7.0, openssl-libs >= 3.0.0")

# Development package requirements
set(CPACK_RPM_PACKAGE_SUGGESTS "cppSwitchboard-devel, libnghttp2-devel, boost-devel, yaml-cpp-devel, openssl-devel")

# Enable automatic dependency detection  
set(CPACK_RPM_PACKAGE_AUTOREQ ON)
set(CPACK_RPM_PACKAGE_AUTOPROV ON)

# Post-install scripts for ldconfig
set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/rpm_post_install.sh")
set(CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/rpm_post_uninstall.sh")

# ============================================================================
# Archive Package Configuration (TGZ, ZIP)
# ============================================================================

# Include install files in archive packages
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)

include(CPack)

# Component definitions for fine-grained packaging
cpack_add_component(Runtime
    DISPLAY_NAME "Runtime Libraries"
    DESCRIPTION "Runtime libraries required to run applications using cppSwitchboard"
    REQUIRED
)

cpack_add_component(Development  
    DISPLAY_NAME "Development Files"
    DESCRIPTION "Headers and development files for building applications with cppSwitchboard"
    DEPENDS Runtime
)

cpack_add_component(Documentation
    DISPLAY_NAME "Documentation"
    DESCRIPTION "API documentation and tutorials"
    DEPENDS Runtime
)

# Package validation target
add_custom_target(package-validate
    COMMAND ${CMAKE_COMMAND} -E echo "=== Package Validation ==="
    COMMAND ${CMAKE_COMMAND} -E echo "Checking required files..."
    COMMAND test -f "${CMAKE_CURRENT_SOURCE_DIR}/README.md" || (echo "README.md missing" && exit 1)
    COMMAND test -f "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" || (echo "LICENSE missing" && exit 1)
    COMMAND test -f "${CMAKE_CURRENT_SOURCE_DIR}/CHANGELOG.md" || (echo "CHANGELOG.md missing" && exit 1)
    COMMAND ${CMAKE_COMMAND} -E echo "Package validation passed"
    COMMENT "Validating package requirements"
)

# Enable testing if building as main project or if explicitly enabled
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME OR BUILD_TESTING)
    enable_testing()
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests)
        add_subdirectory(tests)
    endif()
endif()

# ============================================================================
# Documentation Configuration
# ============================================================================

# Find documentation tools
find_package(Doxygen QUIET)
find_program(PANDOC_EXECUTABLE pandoc)
find_program(PDFLATEX_EXECUTABLE pdflatex)

# Documentation options
option(BUILD_DOCUMENTATION "Build documentation" ON)
option(BUILD_PDF_DOCS "Build PDF documentation (requires pandoc and pdflatex)" ON)
option(INSTALL_DOCUMENTATION "Install documentation" ON)

if(BUILD_DOCUMENTATION)
    # Create documentation directory
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/docs)
    
    if(DOXYGEN_FOUND)
        # Configure Doxygen
        set(DOXYGEN_INPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src")
        set(DOXYGEN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/docs")
        set(DOXYGEN_PROJECT_NAME "${PROJECT_NAME}")
        set(DOXYGEN_PROJECT_VERSION "${PROJECT_VERSION}")
        set(DOXYGEN_PROJECT_BRIEF "Modern C++ HTTP/1.1 and HTTP/2 Server Library")
        
        # Configure Doxyfile
        configure_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
            ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
            @ONLY
        )
        
        # Doxygen target
        add_custom_target(doxygen-cmake
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen (CMake)"
            VERBATIM
        )
        
        # HTML documentation target
        add_custom_target(docs-html
            DEPENDS doxygen-cmake
            COMMENT "HTML documentation generated"
        )
        
    else()
        message(WARNING "Doxygen not found. API documentation will not be generated.")
    endif()
    
    # Pandoc documentation targets
    if(PANDOC_EXECUTABLE AND PDFLATEX_EXECUTABLE AND BUILD_PDF_DOCS)
        # Markdown files for documentation
        set(DOC_MARKDOWN_FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/README.md
            ${CMAKE_CURRENT_SOURCE_DIR}/TESTING.md
            ${CMAKE_CURRENT_SOURCE_DIR}/docs/markdown/API_REFERENCE.md
            ${CMAKE_CURRENT_SOURCE_DIR}/docs/markdown/GETTING_STARTED.md
            ${CMAKE_CURRENT_SOURCE_DIR}/docs/markdown/TUTORIALS.md
            ${CMAKE_CURRENT_SOURCE_DIR}/docs/markdown/CONFIGURATION.md
            ${CMAKE_CURRENT_SOURCE_DIR}/docs/markdown/MIDDLEWARE.md
            ${CMAKE_CURRENT_SOURCE_DIR}/docs/markdown/ASYNC_PROGRAMMING.md
            ${CMAKE_CURRENT_SOURCE_DIR}/docs/markdown/DEPLOYMENT.md
            ${CMAKE_CURRENT_SOURCE_DIR}/docs/markdown/TROUBLESHOOTING.md
            ${CMAKE_CURRENT_SOURCE_DIR}/docs/markdown/ARCHITECTURE.md
            ${CMAKE_CURRENT_SOURCE_DIR}/docs/markdown/PERFORMANCE.md
            ${CMAKE_CURRENT_SOURCE_DIR}/docs/markdown/CONTRIBUTING.md
            ${CMAKE_CURRENT_SOURCE_DIR}/CHANGELOG.md
        )
        
        # Combined documentation target
        add_custom_target(docs-combine
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/docs/combined
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/docs/pdf
            COMMENT "Preparing combined documentation"
        )
        
        # Generate combined markdown
        add_custom_command(
            OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/docs/combined/main.md
            COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/generate_docs.sh
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            DEPENDS ${DOC_MARKDOWN_FILES} docs-combine
            COMMENT "Generating combined documentation"
        )
        
        # PDF documentation target
        add_custom_target(docs-pdf
            COMMAND ${PANDOC_EXECUTABLE} 
                ${CMAKE_CURRENT_SOURCE_DIR}/docs/combined/main.md
                --metadata-file=${CMAKE_CURRENT_SOURCE_DIR}/docs/pandoc.yaml
                --from=markdown 
                --to=pdf 
                --pdf-engine=pdflatex
                --toc
                --output=${CMAKE_CURRENT_BINARY_DIR}/docs/pdf/cppSwitchboard-Documentation.pdf
            COMMAND ${PANDOC_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/docs/markdown/API_REFERENCE.md
                --metadata title="cppSwitchboard API Reference"
                --metadata author="cppSwitchboard Development Team"
                --metadata date="${CMAKE_CURRENT_DATE}"
                --from=markdown
                --to=pdf
                --pdf-engine=pdflatex
                --toc
                --output=${CMAKE_CURRENT_BINARY_DIR}/docs/pdf/cppSwitchboard-API-Reference.pdf
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/docs/combined/main.md
            COMMENT "Generating PDF documentation"
        )
        
        # API Reference PDF target
        add_custom_target(docs-api-pdf
            COMMAND ${PANDOC_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/docs/markdown/API_REFERENCE.md
                --metadata title="cppSwitchboard API Reference"
                --metadata author="cppSwitchboard Development Team"
                --from=markdown
                --to=pdf
                --pdf-engine=pdflatex
                --toc
                --output=${CMAKE_CURRENT_BINARY_DIR}/docs/pdf/cppSwitchboard-API-Reference.pdf
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/docs/markdown/API_REFERENCE.md
            COMMENT "Generating API Reference PDF"
        )
        
    else()
        if(BUILD_PDF_DOCS)
            message(WARNING "Pandoc or pdflatex not found. PDF documentation will not be generated.")
        endif()
    endif()
    
    # Main documentation target
    if(DOXYGEN_FOUND AND PANDOC_EXECUTABLE AND PDFLATEX_EXECUTABLE AND BUILD_PDF_DOCS)
        add_custom_target(docs
            DEPENDS doxygen-cmake docs-pdf
            COMMENT "Building complete documentation (HTML + PDF)"
        )
    elseif(DOXYGEN_FOUND)
        add_custom_target(docs
            DEPENDS doxygen-cmake
            COMMENT "Building HTML documentation"
        )
    else()
        add_custom_target(docs
            COMMAND ${CMAKE_COMMAND} -E echo "Documentation tools not available"
            COMMENT "Documentation tools not found"
        )
    endif()
    
    # Documentation validation target
    add_custom_target(docs-validate
        COMMAND ${CMAKE_COMMAND} -E echo "Validating documentation..."
        COMMAND test -f ${CMAKE_CURRENT_BINARY_DIR}/docs/html/index.html || (echo "HTML docs missing" && exit 1)
        COMMAND test -f ${CMAKE_CURRENT_BINARY_DIR}/docs/pdf/cppSwitchboard-Documentation.pdf || echo "PDF docs missing (optional)"
        COMMENT "Validating generated documentation"
    )
    
    # Documentation cleanup target
    add_custom_target(docs-clean
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_CURRENT_BINARY_DIR}/docs
        COMMENT "Cleaning documentation"
    )
    
    # Install documentation
    if(INSTALL_DOCUMENTATION)
        if(DOXYGEN_FOUND)
            install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/docs/html
                DESTINATION ${CMAKE_INSTALL_DOCDIR}
                OPTIONAL
            )
        endif()
        
        if(PANDOC_EXECUTABLE AND PDFLATEX_EXECUTABLE AND BUILD_PDF_DOCS)
            install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/docs/pdf
                DESTINATION ${CMAKE_INSTALL_DOCDIR}
                OPTIONAL
            )
        endif()
        
        # Install markdown documentation
        install(FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/README.md
            ${CMAKE_CURRENT_SOURCE_DIR}/TESTING.md
            ${CMAKE_CURRENT_SOURCE_DIR}/CHANGELOG.md
            DESTINATION ${CMAKE_INSTALL_DOCDIR}
            OPTIONAL
        )
    endif()
    
    # Documentation status target
    add_custom_target(docs-status
        COMMAND ${CMAKE_COMMAND} -E echo "=== Documentation Status ==="
        COMMAND ${CMAKE_COMMAND} -E echo "Doxygen: ${DOXYGEN_FOUND}"
        COMMAND ${CMAKE_COMMAND} -E echo "Pandoc: ${PANDOC_EXECUTABLE}"
        COMMAND ${CMAKE_COMMAND} -E echo "PDFLaTeX: ${PDFLATEX_EXECUTABLE}"
        COMMAND ${CMAKE_COMMAND} -E echo "Build Documentation: ${BUILD_DOCUMENTATION}"
        COMMAND ${CMAKE_COMMAND} -E echo "Build PDF Docs: ${BUILD_PDF_DOCS}"
        COMMAND ${CMAKE_COMMAND} -E echo "Install Documentation: ${INSTALL_DOCUMENTATION}"
        COMMENT "Showing documentation configuration status"
    )
    
endif() # BUILD_DOCUMENTATION

# ============================================================================
# Development and Maintenance Targets
# ============================================================================

# Code formatting target (if clang-format is available)
find_program(CLANG_FORMAT_EXECUTABLE clang-format)
if(CLANG_FORMAT_EXECUTABLE)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXECUTABLE} -i ${CPPSWITCHBOARD_SOURCES} ${CPPSWITCHBOARD_HEADERS}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Formatting source code with clang-format"
    )
endif()

# Static analysis target (if cppcheck is available)
find_program(CPPCHECK_EXECUTABLE cppcheck)
if(CPPCHECK_EXECUTABLE)
    add_custom_target(cppcheck
        COMMAND ${CPPCHECK_EXECUTABLE} 
            --enable=all 
            --std=c++17 
            --verbose 
            --quiet
            --error-exitcode=1
            -I ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
        COMMENT "Running static analysis with cppcheck"
    )
endif()

# Coverage target (if gcov/lcov is available)
find_program(GCOV_EXECUTABLE gcov)
find_program(LCOV_EXECUTABLE lcov)
if(GCOV_EXECUTABLE AND LCOV_EXECUTABLE)
    add_custom_target(coverage
        COMMAND ${LCOV_EXECUTABLE} --directory . --capture --output-file coverage.info
        COMMAND ${LCOV_EXECUTABLE} --remove coverage.info '/usr/*' --output-file coverage.info
        COMMAND ${LCOV_EXECUTABLE} --list coverage.info
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating code coverage report"
    )
endif()

# All-in-one development target
add_custom_target(dev-all
    DEPENDS docs
    COMMENT "Running all development tasks"
)

if(CLANG_FORMAT_EXECUTABLE)
    add_dependencies(dev-all format)
endif()

if(CPPCHECK_EXECUTABLE)
    add_dependencies(dev-all cppcheck)
endif()

# Print configuration summary
message(STATUS "=== cppSwitchboard Configuration Summary ===")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Documentation: ${BUILD_DOCUMENTATION}")
message(STATUS "PDF Documentation: ${BUILD_PDF_DOCS}")
message(STATUS "Doxygen found: ${DOXYGEN_FOUND}")
message(STATUS "Pandoc found: ${PANDOC_EXECUTABLE}")
message(STATUS "PDFLaTeX found: ${PDFLATEX_EXECUTABLE}")
message(STATUS "=============================================") 