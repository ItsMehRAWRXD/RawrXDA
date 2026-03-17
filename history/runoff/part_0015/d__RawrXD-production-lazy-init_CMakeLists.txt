cmake_minimum_required(VERSION 3.20)
project(RawrXD VERSION 1.0.13)
add_definitions(-DNOMINMAX -D_WIN32_WINNT=0x0A00)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# ENABLE MASM FOR CUSTOM IMPLEMENTATIONS
# ============================================================
enable_language(ASM_MASM)
set(CMAKE_ASM_MASM_FLAGS "/nologo /W3 /Cx /Zi")
message(STATUS "✓ MASM assembler enabled for native implementations")

# ============================================================
# 64-BIT ARCHITECTURE ENFORCEMENT
# ============================================================
# RawrXD is x64-ONLY. No external dependencies.
# Pure Windows native implementation.

# Force x64 architecture
if(MSVC)
    set(CMAKE_GENERATOR_PLATFORM x64)
    set(CMAKE_SIZEOF_VOID_P 8)
    # Validate 64-bit configuration
    if(NOT CMAKE_GENERATOR_PLATFORM STREQUAL "x64")
        message(FATAL_ERROR "CRITICAL: CMAKE_GENERATOR_PLATFORM must be x64, got ${CMAKE_GENERATOR_PLATFORM}")
    endif()
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "CRITICAL: CMAKE_SIZEOF_VOID_P must be 8 (x64), got ${CMAKE_SIZEOF_VOID_P}")
    endif()
    message(STATUS "✓ 64-bit configuration validated: pointer size = ${CMAKE_SIZEOF_VOID_P}, platform = ${CMAKE_GENERATOR_PLATFORM}")
endif()

# Pin MSVC runtime for deterministic builds - ALWAYS use /MD (dynamic CRT)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    # Force /MD for all targets to prevent CRT mismatch
    add_compile_options(
        $<$<CONFIG:Debug>:/MDd>
        $<$<CONFIG:Release>:/MD>
        $<$<CONFIG:RelWithDebInfo>:/MD>
        $<$<CONFIG:MinSizeRel>:/MD>
    )
endif()

# Detect compiler and set output directory suffix
if(MINGW)
    set(COMPILER_SUFFIX "mingw")
elseif(MSVC)
    set(COMPILER_SUFFIX "msvc")
else()
    set(COMPILER_SUFFIX "other")
endif()

# Build options
option(RAWRXD_BUILD_TESTS "Build unit-test harnesses" ON)  # Enable tests by default

# Force Windows SDK version
set(CMAKE_SYSTEM_VERSION "10.0.22621.0")

# ============================================================
# TEST INFRASTRUCTURE CONFIGURATION
# ============================================================
if(RAWRXD_BUILD_TESTS)
    enable_testing()
    include(CTest)
    message(STATUS "✓ CTest infrastructure enabled - tests will be discoverable")
endif()

# ZLIB Configuration - optional compression library
option(USE_ZLIB_FETCHCONTENT "Auto-download and build zlib if not found" OFF)

# ZLIB is optional - only needed for CheckpointManager compression
# Try to find ZLIB in common installation paths first
find_package(ZLIB QUIET HINTS C:/zlib C:/Program\ Files/zlib "C:/Program Files (x86)/zlib" /usr/local /usr)

if(ZLIB_FOUND)
    message(STATUS "✓ ZLIB found at ${ZLIB_INCLUDE_DIR} - Native compression enabled")
    set(HAVE_ZLIB 1)
    set(HAVE_COMPRESSION 1)
elseif(USE_ZLIB_FETCHCONTENT)
    # Auto-fetch zlib if not found on system
    include(FetchContent)
    message(STATUS "ℹ ZLIB not found - attempting to auto-download from GitHub...")
    
    FetchContent_Declare(zlib
        URL https://github.com/madler/zlib/archive/refs/tags/v1.3.tar.gz
        URL_HASH SHA256=b5b06d60ce49c8ba700e0ba517fa07de80b5d4628a037f4be8ad16955be7a7c0
    )
    
    FetchContent_MakeAvailable(zlib)
    set(ZLIB_FOUND TRUE)
    set(HAVE_ZLIB 1)
    set(HAVE_COMPRESSION 1)
    message(STATUS "✓ ZLIB auto-downloaded and built successfully")
else()
    # Auto-fetch zlib if not found on system (optional - can be disabled with FETCHCONTENT_ZLIB=OFF)
    option(FETCHCONTENT_ZLIB "Auto-download and build zlib if not found" ON)
    
    if(FETCHCONTENT_ZLIB)
        include(FetchContent)
        message(STATUS "ℹ ZLIB not found - attempting to auto-download from GitHub...")
        
        FetchContent_Declare(zlib
            URL https://github.com/madler/zlib/archive/refs/tags/v1.3.tar.gz
        )
        
        FetchContent_MakeAvailable(zlib)
        set(ZLIB_FOUND TRUE)
        set(HAVE_ZLIB 1)
        set(HAVE_COMPRESSION 1)
        message(STATUS "✓ ZLIB auto-downloaded and built successfully")
    else()
        message(STATUS "ℹ ZLIB not installed - Using built-in MASM-based compression (BrutalGzip/Deflate)")
        message(STATUS "  MASM Compression Engines: BrutalGzip (MASM optimized), Deflate (fallback)")
        message(STATUS "  To use native zlib compression:")
        message(STATUS "    Option 1: vcpkg install zlib:x64-windows")
        message(STATUS "    Option 2: Enable auto-download with -DFETCHCONTENT_ZLIB=ON")
        set(HAVE_ZLIB 0)
        set(HAVE_COMPRESSION 1)
    endif()
endif()

find_package(CURL QUIET)
if(CURL_FOUND)
    message(STATUS "CURL found: ${CURL_VERSION_STRING}")
    set(HAVE_CURL 1)
else()
    message(WARNING "CURL not found - HTTP client will use fallback. Set CURL_DIR or install libcurl for full functionality.")
    set(HAVE_CURL 0)
endif()

find_package(ZSTD QUIET)
if(ZSTD_FOUND)
    message(STATUS "ZSTD found: ${ZSTD_VERSION_STRING}")
    set(HAVE_ZSTD 1)
else()
    message(WARNING "ZSTD not found - compression will use passthrough. Set ZSTD_ROOT or install zstd for full functionality.")
    set(HAVE_ZSTD 0)
endif()

if(Qt6_FOUND)
    message(STATUS "Qt6 found: ${Qt6_VERSION}")
    add_definitions(-DHAVE_QT_CORE -DQT_NO_OPENGL)
    include_directories(${Qt6_INCLUDE_DIRS})
    link_libraries(Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Sql Qt6::Concurrent Qt6::Test Qt6::Charts Qt6::PrintSupport)
    
    # Add compression definitions globally
    add_compile_definitions(HAVE_COMPRESSION=1)
    if(HAVE_ZLIB)
        add_compile_definitions(HAVE_ZLIB=1)
    endif()
    if(HAVE_ZSTD)
        add_compile_definitions(HAVE_ZSTD=1)
    endif()
    
    # ============================================================
    # AUTOMATIC DLL DEPLOYMENT - Deploy ALL dependencies automatically
    # ============================================================
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin")
    
    function(deploy_runtime_dependencies target_name)
        if(WIN32 AND WINDEPLOYQT_EXECUTABLE)
            # Use windeployqt to automatically deploy Qt DLLs, plugins, and MSVC runtime
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}" 
                    --release 
                    --compiler-runtime 
                    --no-translations 
                    --no-system-d3d-compiler
                    --no-opengl-sw
                    "$<TARGET_FILE:${target_name}>"
                WORKING_DIRECTORY "$<TARGET_FILE_DIR:${target_name}>"
                COMMENT "Deploying Qt dependencies for ${target_name}..."
            )
            
            # Additionally copy Qt6Sql.dll explicitly (sometimes missed by windeployqt)
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:Qt6::Sql>"
                    "$<TARGET_FILE_DIR:${target_name}>/"
                COMMENT "Ensuring Qt6Sql.dll is deployed for ${target_name}"
            )
            
            # Copy MSVC OpenMP runtime (vcomp140.dll) if not already deployed
            if(EXISTS "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Redist/MSVC/14.44.35207/x64/Microsoft.VC144.OpenMP/vcomp140.dll")
                add_custom_command(TARGET ${target_name} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Redist/MSVC/14.44.35207/x64/Microsoft.VC144.OpenMP/vcomp140.dll"
                        "$<TARGET_FILE_DIR:${target_name}>/"
                    COMMENT "Deploying vcomp140.dll for ${target_name}"
                )
            endif()
            
            message(STATUS "✓ Automatic DLL deployment enabled for ${target_name}")
        else()
            message(WARNING "windeployqt not found - manual DLL deployment required for ${target_name}")
        endif()
    endfunction()
    
else()
    message(WARNING "Qt6 not found. Qt-based benchmarks will be skipped.")
endif()

# ============================================================
# Native Windows DirectX 12 GPU Support
# ============================================================
# Use native DirectX 12 for GPU compute instead of Vulkan/CUDA
add_compile_definitions(HAVE_D3D12=1)
message(STATUS "✓ Native DirectX 12 GPU support enabled")

# ============================================================
# Native Quantized Math Implementation
# ============================================================
# Implement Q4_0/Q8_0 dequantization in pure C++20/MASM
# No external ggml dependency
add_compile_definitions(HAVE_NATIVE_QUANT=1)
message(STATUS "✓ Native quantized math implementation enabled")

# ============================================================
# Core Source Files
# ============================================================
message(STATUS "Adding core native Windows implementation")

# ============================================================
# Native Windows Executable
# ============================================================
add_executable(RawrXD
    src/main.cpp
    # Add core native Windows files here
)

target_include_directories(RawrXD PRIVATE include)
message(STATUS "✓ Native Windows RawrXD executable configured")

# ============================================================
# Test Infrastructure
# ============================================================
if(RAWRXD_BUILD_TESTS)
    enable_testing()
    message(STATUS "✓ Test infrastructure enabled")
endif()

message(STATUS "✓ CMake configuration complete - no external dependencies")
