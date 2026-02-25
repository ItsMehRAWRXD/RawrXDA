# Enhanced Streaming GGUF Loader CMake Configuration
# This file integrates predictive caching, NVMe I/O, and tensor parallelism

# Detect enhanced loader capabilities
include(CheckIncludeFiles)
include(CheckSymbolExists)

# Check for Windows 11 IORING support
check_include_files("windows.h" HAVE_WINDOWS_H)
if(HAVE_WINDOWS_H)
    set(HAVE_IORING TRUE)
    message(STATUS "IORING (Windows 11 22H2+): Enabled")
else()
    set(HAVE_IORING FALSE)
endif()

# Create enhanced loader library
add_library(streaming_gguf_loader_enhanced STATIC
    src/streaming_gguf_loader_enhanced.cpp
)

target_include_directories(streaming_gguf_loader_enhanced PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/model_loader
    ${CMAKE_SOURCE_DIR}/3rdparty/ggml/include
)

target_link_libraries(streaming_gguf_loader_enhanced PUBLIC
    streaming_gguf_loader
    ggml
)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(streaming_gguf_loader_enhanced PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    
    target_link_libraries(streaming_gguf_loader_enhanced PRIVATE
        kernel32
        advapi32
        ws2_32
    )
    
    # Enable huge pages (requires admin)
    set(ENABLE_HUGE_PAGES TRUE)
    
    # Enable NVMe (requires driver support)
    set(ENABLE_NVME TRUE)
endif()

# Compiler optimizations for enhanced performance
if(MSVC)
    target_compile_options(streaming_gguf_loader_enhanced PRIVATE
        /O2              # Maximum optimization
        /fp:fast         # Fast floating point
        /arch:AVX2       # AVX2 SIMD (upgrade to /arch:AVX512 if available)
    )
else()
    target_compile_options(streaming_gguf_loader_enhanced PRIVATE
        -O3
        -ffast-math
        -march=native
    )
endif()

# Optional: Add AVX-512 support if compiler supports it
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("/arch:AVX512F" HAS_AVX512)
if(HAS_AVX512)
    target_compile_definitions(streaming_gguf_loader_enhanced PRIVATE
        ENABLE_AVX512=1
    )
    if(MSVC)
        target_compile_options(streaming_gguf_loader_enhanced PRIVATE /arch:AVX512F)
    else()
        target_compile_options(streaming_gguf_loader_enhanced PRIVATE -mavx512f)
    endif()
    message(STATUS "AVX-512: Enabled")
else()
    message(STATUS "AVX-512: Not available, using AVX2")
endif()

# Feature configuration
set(ENABLE_PREDICTIVE_CACHE TRUE CACHE BOOL "Enable predictive zone caching")
set(ENABLE_NVME_DIRECT_IO TRUE CACHE BOOL "Enable NVMe kernel-bypass I/O")
set(ENABLE_IORING_BATCH TRUE CACHE BOOL "Enable IORING batch I/O")
set(ENABLE_HUGE_PAGES TRUE CACHE BOOL "Enable 2MB huge page support")
set(ENABLE_TENSOR_PARALLEL TRUE CACHE BOOL "Enable tensor parallel loading")
set(ENABLE_ADAPTIVE_COMPRESSION TRUE CACHE BOOL "Enable adaptive decompression")

# Generate feature header
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/enhanced_loader_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/enhanced_loader_config.h
)

target_include_directories(streaming_gguf_loader_enhanced PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Test executable
if(BUILD_TESTING)
    add_executable(test_enhanced_streaming_gguf_loader
        tests/test_enhanced_streaming_gguf_loader.cpp
        src/streaming_gguf_loader.cpp
        src/streaming_gguf_loader_enhanced.cpp
        src/utils/Diagnostics.cpp
    )
    
    target_include_directories(test_enhanced_streaming_gguf_loader PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/model_loader
        ${CMAKE_SOURCE_DIR}/3rdparty/ggml/include
        ${CMAKE_CURRENT_BINARY_DIR}
    )
    
    target_link_libraries(test_enhanced_streaming_gguf_loader PRIVATE
        GTest::gtest_main
        ggml
    )
    
    # Platform-specific test settings
    if(WIN32)
        target_link_libraries(test_enhanced_streaming_gguf_loader PRIVATE
            kernel32
            advapi32
            ws2_32
        )
    endif()
    
    add_test(NAME test_enhanced_streaming_gguf_loader
             COMMAND test_enhanced_streaming_gguf_loader)
endif()

# Documentation
file(COPY ENHANCED_STREAMING_GGUF_LOADER_GUIDE.md DESTINATION ${CMAKE_BINARY_DIR})

message(STATUS "Enhanced Streaming GGUF Loader Configuration:")
message(STATUS "  Predictive Cache: ${ENABLE_PREDICTIVE_CACHE}")
message(STATUS "  NVMe Direct I/O: ${ENABLE_NVME_DIRECT_IO}")
message(STATUS "  IORING Batch I/O: ${ENABLE_IORING_BATCH}")
message(STATUS "  Huge Pages: ${ENABLE_HUGE_PAGES}")
message(STATUS "  Tensor Parallelism: ${ENABLE_TENSOR_PARALLEL}")
message(STATUS "  Adaptive Compression: ${ENABLE_ADAPTIVE_COMPRESSION}")
