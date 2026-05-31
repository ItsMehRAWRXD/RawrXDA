# ============================================================================
# tests/asm/CMakeLists.txt — Sovereign ASM Kernel GoogleTest Suite
# ============================================================================
cmake_minimum_required(VERSION 3.20)

# ── GoogleTest (header-only fetch) ──────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    googletest
    URL "https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
# ─────────────────────────────────────────────────────────────────────────────

# ── Sovereign ASM Kernel Test Suite ───────────────────────────────────────────
add_executable(test_sovereign_kernels
    test_sovereign_kernels.cpp
)

target_include_directories(test_sovereign_kernels PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/asm
)

# Link against the ASM object files (produced by ml64.exe)
# These are built by the parent CMakeLists.txt via add_custom_command
target_link_libraries(test_sovereign_kernels
    gtest_main
    gmock
    ${CMAKE_SOURCE_DIR}/src/asm/sovereign_kernels.obj
    ${CMAKE_SOURCE_DIR}/src/asm/SovereignSpeculativeEngine.obj
    ${CMAKE_SOURCE_DIR}/src/asm/SovereignStandaloneEngine.obj
)

# Ensure AVX2 is enabled for the test runner itself
target_compile_options(test_sovereign_kernels PRIVATE
    /arch:AVX2
    /fp:precise
    /O2
)

set_target_properties(test_sovereign_kernels PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# ── CTest Registration ────────────────────────────────────────────────────────
add_test(NAME test_sovereign_kernels COMMAND test_sovereign_kernels)
message(STATUS "  ✓ Registered test_sovereign_kernels")

# ── Performance Benchmark Target (separate, longer running) ───────────────────
add_executable(bench_sovereign_kernels
    test_sovereign_kernels.cpp
)

target_include_directories(bench_sovereign_kernels PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/asm
)

target_link_libraries(bench_sovereign_kernels
    gtest_main
    gmock
    ${CMAKE_SOURCE_DIR}/src/asm/sovereign_kernels.obj
    ${CMAKE_SOURCE_DIR}/src/asm/SovereignSpeculativeEngine.obj
    ${CMAKE_SOURCE_DIR}/src/asm/SovereignStandaloneEngine.obj
)

target_compile_options(bench_sovereign_kernels PRIVATE
    /arch:AVX2
    /fp:precise
    /O2
)

target_compile_definitions(bench_sovereign_kernels PRIVATE
    SOVEREIGN_BENCHMARK_MODE=1
)

set_target_properties(bench_sovereign_kernels PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME bench_sovereign_kernels COMMAND bench_sovereign_kernels)
message(STATUS "  ✓ Registered bench_sovereign_kernels")
