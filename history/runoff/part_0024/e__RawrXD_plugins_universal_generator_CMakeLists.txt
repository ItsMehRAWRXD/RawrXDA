cmake_minimum_required(VERSION 3.20)
project(universal_generator_plugin VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies
find_package(nlohmann_json CONFIG REQUIRED)

# Plugin source files
set(PLUGIN_SOURCES
    plugin.cpp
    ../../src/engine/universal_generator.cpp
)

set(PLUGIN_HEADERS
    ../../src/engine/universal_generator.h
    ../../src/engine/common_types.h
)

# Create plugin library
add_library(universal_generator SHARED ${PLUGIN_SOURCES} ${PLUGIN_HEADERS})

# Include directories
target_include_directories(universal_generator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/engine
)

# Link libraries
target_link_libraries(universal_generator PRIVATE
    nlohmann_json::nlohmann_json
)

# Set output properties
set_target_properties(universal_generator PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    OUTPUT_NAME "universal_generator"
    PREFIX ""  # No 'lib' prefix on Windows
    SUFFIX ".dll"
)

# Define plugin macros
target_compile_definitions(universal_generator PRIVATE
    UNIVERSAL_GENERATOR_VERSION="1.0.0"
    PLUGIN_EXPORTS
)

# Windows-specific libraries
if(WIN32)
    target_link_libraries(universal_generator PRIVATE
        shlwapi
    )
endif()

# Install rules
install(TARGETS universal_generator
    RUNTIME DESTINATION plugins
    LIBRARY DESTINATION plugins
)

# Create VSIX package
add_custom_target(vsix_package ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/vsix
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/manifest.json ${CMAKE_BINARY_DIR}/vsix/
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:universal_generator> ${CMAKE_BINARY_DIR}/vsix/
    COMMAND ${CMAKE_COMMAND} -E tar cf ${CMAKE_BINARY_DIR}/universal_generator.vsix --format=zip -- ${CMAKE_BINARY_DIR}/vsix/
    DEPENDS universal_generator
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Creating VSIX package..."
)
