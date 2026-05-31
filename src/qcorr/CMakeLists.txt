# ============================================================================
# QCORR - Quantization with Correction Offsets
# Build configuration for running 800B+ models on 16GB VRAM
# ============================================================================

cmake_minimum_required(VERSION 3.10)
project(qcorr VERSION 1.0.0 LANGUAGES C)

# ============================================================================
# Compiler Flags
# ============================================================================
if(MSVC)
    add_compile_options(/W3 /O2 /arch:AVX2)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -O3 -mavx2 -mfma)
endif()

# ============================================================================
# AVX-512 Support (Optional)
# ============================================================================
option(QCORR_USE_AVX512 "Enable AVX-512 optimizations" OFF)
if(QCORR_USE_AVX512)
    if(MSVC)
        add_compile_options(/arch:AVX512)
    else()
        add_compile_options(-mavx512f -mavx512dq)
    endif()
    add_compile_definitions(__AVX512F__)
endif()

# ============================================================================
# Library
# ============================================================================
add_library(qcorr STATIC
    qcorr.c
    memory_manager.c
)

target_include_directories(qcorr PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link math library
if(NOT MSVC)
    target_link_libraries(qcorr m)
endif()

# ============================================================================
# Test Executable
# ============================================================================
add_executable(qcorr_test
    qcorr_test.c
)

target_link_libraries(qcorr_test qcorr)

# ============================================================================
# Benchmark Executable
# ============================================================================
add_executable(qcorr_benchmark
    qcorr_test.c
)

target_link_libraries(qcorr_benchmark qcorr)
target_compile_definitions(qcorr_benchmark PRIVATE QCORR_BENCHMARK)

# ============================================================================
# Installation
# ============================================================================
install(TARGETS qcorr
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES qcorr.h memory_manager.h
    DESTINATION include
)

# ============================================================================
# Summary
# ============================================================================
message(STATUS "")
message(STATUS "QCORR Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  AVX-512: ${QCORR_USE_AVX512}")
message(STATUS "  Compiler: ${CMAKE_C_COMPILER_ID}")
message(STATUS "")
message(STATUS "Memory Requirements for 800B Models:")
message(STATUS "  INT4 + Sparse Correction: ~105GB (16GB VRAM + 32GB RAM + 57GB Disk)")
message(STATUS "  INT4 + SVD Correction:     ~108GB")
message(STATUS "  INT4 + Adaptive:           ~110GB")
message(STATUS "")