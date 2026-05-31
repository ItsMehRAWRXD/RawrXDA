cmake_minimum_required(VERSION 3.20)
project(RawrXDGenerator CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_RELEASE "/O2 /NDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "/Zi /Od")

# Source files
set(SOURCES
    generator.cpp
    ast.cpp
    code_builder.cpp
    file_emitter.cpp
    schema_parser.cpp
    main.cpp
)

set(HEADERS
    generator.hpp
    ast.hpp
    code_builder.hpp
    file_emitter.hpp
    schema_parser.hpp
)

# Create executable
add_executable(rawrxd_generator ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(rawrxd_generator PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Compiler-specific flags
if(MSVC)
    target_compile_options(rawrxd_generator PRIVATE /W4 /WX)
else()
    target_compile_options(rawrxd_generator PRIVATE -Wall -Wextra -Werror)
endif()

# Output directory
set_target_properties(rawrxd_generator PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Enable testing
enable_testing()

# Add custom command to run generator after build
add_custom_command(TARGET rawrxd_generator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Build complete: rawrxd_generator"
)
