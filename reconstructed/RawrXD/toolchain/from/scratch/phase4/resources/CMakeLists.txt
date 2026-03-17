cmake_minimum_required(VERSION 3.10)
project(RawrXD_Phase4_Resources C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Add executable
add_executable(rawrxd_resource_test resource_test.c resource_builder.c)

# No external dependencies - pure C
target_compile_options(rawrxd_resource_test PRIVATE
    -Wall
    -Wextra
    -O2
)

message(STATUS "✓ Phase 4 Resources - CMake configuration complete - no external dependencies")