cmake_minimum_required(VERSION 3.16)
project(PEWriterProduction VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_IDE_INTEGRATION "Build IDE integration components" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)

# Compiler flags
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Source files
set(CORE_SOURCES
    core/pe_structure_builder.cpp
    core/pe_validator.cpp
    core/error_handler.cpp
)

set(EMITTER_SOURCES
    emitter/code_emitter.cpp
)

set(STRUCTURES_SOURCES
    structures/import_resolver.cpp
    structures/relocation_manager.cpp
    structures/resource_manager.cpp
)

set(CONFIG_SOURCES
    config/config_parser.cpp
)

set(IDE_SOURCES
    ide_integration/ide_bridge.cpp
)

set(MAIN_SOURCES
    pe_writer.cpp
)

# Create main library
add_library(pewriter ${CORE_SOURCES} ${EMITTER_SOURCES} ${STRUCTURES_SOURCES} ${CONFIG_SOURCES} ${MAIN_SOURCES})

if(BUILD_IDE_INTEGRATION)
    add_library(pewriter_ide ${IDE_SOURCES})
    target_link_libraries(pewriter_ide pewriter)
endif()

# Tests
if(BUILD_TESTS)
    add_executable(pewriter_tests tests/test_main.cpp)
    target_link_libraries(pewriter_tests pewriter)

    if(BUILD_IDE_INTEGRATION)
        target_link_libraries(pewriter_tests pewriter_ide)
    endif()

    enable_testing()
    add_test(NAME PEWriterTests COMMAND pewriter_tests)
endif()

# Installation
install(TARGETS pewriter
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY .
    DESTINATION include/pewriter
    FILES_MATCHING PATTERN "*.h"
)

# Package configuration
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    PEWriterConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    cmake/PEWriterConfig.cmake.in
    PEWriterConfig.cmake
    INSTALL_DESTINATION lib/cmake/PEWriter
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/PEWriterConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/PEWriterConfigVersion.cmake
    DESTINATION lib/cmake/PEWriter
)