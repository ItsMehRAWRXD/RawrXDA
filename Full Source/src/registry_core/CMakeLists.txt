cmake_minimum_required(VERSION 3.20)
project(RawrXD_Registry_Core)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/main.cpp
    src/registry_manager.cpp
    src/extension_installer.cpp
    src/database_manager.cpp
    src/http_server.cpp
    src/process_bridge.cpp
)

# Header files
set(HEADERS
    include/registry_manager.hpp
    include/extension_installer.hpp
    include/database_manager.hpp
    include/http_server.hpp
    include/process_bridge.hpp
    include/types.hpp
)

# Create executable
add_executable(registry_core ${SOURCES} ${HEADERS})

# Platform-specific settings
if(WIN32)
    target_compile_definitions(registry_core PRIVATE _WIN32)
    # Link with Windows libraries if needed
    target_link_libraries(registry_core ws2_32)
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(registry_core PRIVATE /W4 /WX)
else()
    target_compile_options(registry_core PRIVATE -Wall -Wextra -Werror)
endif()

# Output directory
set_target_properties(registry_core PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Installation
install(TARGETS registry_core
    RUNTIME DESTINATION bin
)
