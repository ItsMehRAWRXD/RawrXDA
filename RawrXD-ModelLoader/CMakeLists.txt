cmake_minimum_required(VERSION 3.20)
project(RawrXD-ModelLoader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Detect compiler and set output directory suffix
if(MINGW)
    set(COMPILER_SUFFIX "mingw")
elseif(MSVC)
    set(COMPILER_SUFFIX "msvc")
else()
    set(COMPILER_SUFFIX "other")
endif()

# Build options
option(ENABLE_COPILOT "Enable Copilot streaming integration for Win32 IDE" OFF)

# Force Windows SDK version that ships all required headers
set(CMAKE_SYSTEM_VERSION "10.0.22621.0")

# Qt fully eliminated — pure C++20/Win32/MASM architecture

# Win32-based IDE (MSVC only)
if(FALSE AND MSVC) # Disabled until headers exist
    add_executable(RawrXD-Win32IDE 
        src/win32app/main_win32.cpp
        src/win32app/Win32IDE.cpp
        src/win32app/Win32IDE.h
        src/win32app/Win32IDE_Sidebar.cpp
        src/win32app/Win32IDE_VSCodeUI.cpp
        src/win32app/Win32IDE_PowerShellPanel.cpp
        src/win32app/Win32IDE_PowerShell.cpp
        src/win32app/Win32IDE_Logger.cpp
        src/win32app/TransparentRenderer.cpp
        src/win32app/TransparentRenderer.h
        src/win32app/Win32TerminalManager.cpp
        src/win32app/Win32TerminalManager.h
        src/win32app/Win32IDE_FileOps.cpp
        src/win32app/Win32IDE_Commands.cpp
        src/win32app/Win32IDE_Debugger.cpp
        src/win32app/Win32IDE_AgenticBridge.h
        src/win32app/Win32IDE_AgenticBridge.cpp
        src/win32app/Win32IDE_AgentCommands.cpp
        src/win32app/Win32IDE_Autonomy.h
        src/win32app/Win32IDE_Autonomy.cpp
        src/gguf_loader.cpp
        src/streaming_gguf_loader.cpp
        $<$<BOOL:${ENABLE_VULKAN}>:src/win32app/VulkanRenderer.cpp>
        $<$<BOOL:${ENABLE_COPILOT}>:src/win32app/Win32IDE_CopilotIntegration.cpp>
        $<$<BOOL:${ENABLE_COPILOT}>:src/win32app/CopilotStreaming.cpp>
    )
    
    target_link_libraries(RawrXD-Win32IDE PRIVATE
        comctl32
        d3d11
        dxgi
        dcomp
        dwmapi
        d2d1
        dwrite
        d3dcompiler
        winhttp
        shlwapi
        shell32
    )
    
    # Add include dir first to pick up our d3d10effect.h stub before system SDK
    target_include_directories(RawrXD-Win32IDE PRIVATE 
        ${CMAKE_SOURCE_DIR}/include
    )
    
    # Compiler options - MSVC only now
    target_compile_options(RawrXD-Win32IDE PRIVATE /EHsc /O2)
    
    set_target_properties(RawrXD-Win32IDE PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin-msvc"
        WIN32_EXECUTABLE TRUE
    )
    if(ENABLE_VULKAN)
        target_compile_definitions(RawrXD-Win32IDE PRIVATE ENABLE_VULKAN=1)
    endif()
    if(ENABLE_COPILOT)
        target_compile_definitions(RawrXD-Win32IDE PRIVATE ENABLE_COPILOT=1)
        message(STATUS "Copilot integration enabled for Win32 IDE")
    endif()
endif() # End MSVC-only Win32IDE block

# Terminal test executable (MSVC only, placed in bin-msvc)
if(FALSE AND MSVC AND EXISTS "${CMAKE_SOURCE_DIR}/src/win32app/test_terminal.cpp") # Disabled until headers exist
    add_executable(test_terminal
        src/win32app/test_terminal.cpp
        src/win32app/Win32TerminalManager.cpp
        src/win32app/Win32TerminalManager.h
    )
    set_target_properties(test_terminal PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin-msvc"
    )
endif()

# Chat application (MSVC only, taskbar integration)
if(FALSE AND MSVC AND ENABLE_COPILOT AND EXISTS "${CMAKE_SOURCE_DIR}/src/win32app/main_chat.cpp") # Disabled until headers exist
    add_executable(RawrXD-Chat
        src/win32app/main_chat.cpp
        src/win32app/Win32ChatApp.cpp
        src/win32app/Win32ChatApp.h
        src/win32app/ContextManager.h
    )

    target_link_libraries(RawrXD-Chat PRIVATE
        comctl32
        shell32
        winhttp
    )

    target_include_directories(RawrXD-Chat PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )

    target_compile_options(RawrXD-Chat PRIVATE /EHsc)

    set_target_properties(RawrXD-Chat PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin-msvc"
        WIN32_EXECUTABLE TRUE
    )
endif()

# Vulkan SDK (optional now; can be disabled if not installed)
option(BUILD_TESTS "Build unit tests (disables Vulkan requirement)" OFF)
option(ENABLE_VULKAN "Enable Vulkan support" OFF)
if(ENABLE_VULKAN)
    if(NOT BUILD_TESTS)
        find_package(Vulkan REQUIRED)
    else()
        find_package(Vulkan QUIET)
    endif()
else()
    message(STATUS "Vulkan disabled via ENABLE_VULKAN=OFF; skipping find_package")
endif()


# Compiler selection: do not force MinGW globally. Let the generator/toolchain decide.
# If you need MinGW, configure the build folder with `-G "MinGW Makefiles"`.

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

if(FALSE) # Disabled until headers exist
# Add main executable
add_executable(RawrXD-ModelLoader
    src/main.cpp
    src/gguf_loader.cpp
    src/vulkan_compute.cpp
    src/hf_downloader.cpp
    src/gui.cpp
    src/api_server.cpp
    src/settings.cpp
    src/telemetry.cpp
    src/telemetry/ai_metrics.cpp
    src/overclock_vendor.cpp
    src/overclock_governor.cpp
    src/baseline_profile.cpp
)

# Console-based IDE (non-Qt) interactive shell
add_executable(RawrXD-CLI
    src/rawrxd_cli.cpp
    src/gguf_loader.cpp
    src/vulkan_compute.cpp
    src/hf_downloader.cpp
    src/api_server.cpp
    src/settings.cpp
    src/telemetry.cpp
    src/overclock_vendor.cpp
    src/overclock_governor.cpp
    src/baseline_profile.cpp
)

target_link_libraries(RawrXD-CLI PRIVATE wbemuuid pdh)
if(Vulkan_FOUND)
    target_link_libraries(RawrXD-CLI PRIVATE Vulkan::Vulkan)
endif()
set_target_properties(RawrXD-CLI PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin-${COMPILER_SUFFIX}")

# Link libraries
target_link_libraries(RawrXD-ModelLoader PRIVATE
    ws2_32
    winmm
    imm32
    ole32
    oleaut32
    winspool
    advapi32
    shell32
    user32
    gdi32
    wbemuuid
    pdh
)

if(Vulkan_FOUND)
    target_link_libraries(RawrXD-ModelLoader PRIVATE Vulkan::Vulkan)
endif()

# Compiler flags
if(MSVC)
    target_compile_options(RawrXD-ModelLoader PRIVATE /O2 /EHsc)
else()
    target_compile_options(RawrXD-ModelLoader PRIVATE -O2 -fexceptions)
endif()
target_compile_definitions(RawrXD-ModelLoader PRIVATE _CRT_SECURE_NO_WARNINGS _WINDOWS)

# Output directory
set_target_properties(RawrXD-ModelLoader PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin-${COMPILER_SUFFIX}"
)
endif()

if(FALSE) # Disabled until headers exist
# Benchmark target
add_executable(model_loader_bench
    src/bench_main.cpp
    src/gguf_loader.cpp
    src/vulkan_compute.cpp
    src/settings.cpp
    src/telemetry.cpp
)

target_link_libraries(model_loader_bench PRIVATE wbemuuid pdh)
if(Vulkan_FOUND)
    target_link_libraries(model_loader_bench PRIVATE Vulkan::Vulkan)
endif()

# Stress test executable for overclock governor development
add_executable(rawrxd_stress
    src/oc_stress.cpp
    src/telemetry.cpp
)
target_link_libraries(rawrxd_stress PRIVATE wbemuuid pdh)
set_target_properties(rawrxd_stress PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin-${COMPILER_SUFFIX}")
if(MSVC)
    target_compile_options(model_loader_bench PRIVATE /O2 /EHsc)
else()
    target_compile_options(model_loader_bench PRIVATE -O2 -fexceptions)
endif()
set_target_properties(model_loader_bench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin-${COMPILER_SUFFIX}"
)
endif()

# Unit test targets (separate small executables) - DISABLED - files don't exist
# add_executable(rawrxd_test_baseline
#     tests/test_baseline_main.cpp
#     src/baseline_profile.cpp
#     src/settings.cpp
# )
# target_link_libraries(rawrxd_test_baseline PRIVATE wbemuuid pdh)
# set_target_properties(rawrxd_test_baseline PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# add_executable(rawrxd_test_pid
#     tests/test_pid_main.cpp
#     src/overclock_governor.cpp
#     src/overclock_vendor.cpp
#     src/settings.cpp
# )
# target_link_libraries(rawrxd_test_pid PRIVATE wbemuuid pdh)
# set_target_properties(rawrxd_test_pid PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(FALSE) # Disabled until headers exist
# Native Win32 IDE executable (no Qt dependencies)
add_executable(RawrXD-IDE WIN32
    src/ide_main.cpp
    src/ide_window.cpp
)

target_include_directories(RawrXD-IDE PRIVATE include)
target_link_libraries(RawrXD-IDE PRIVATE 
    comctl32 
    comdlg32 
    shell32
)

set_target_properties(RawrXD-IDE PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    WIN32_EXECUTABLE TRUE
)
endif()

# [REMOVED] RawrXD-SimpleIDE (legacy Qt/qtapp target — replaced by RawrXD-IDE Win32)

# Chromatic Wave Demo - Chameleon/Neon effects with D3D11
# Target: 540Hz @ 3840x2160 (4K UHD)
add_executable(RawrXD-Chromatic WIN32
    src/chromatic_main.cpp
    src/ui/chromatic_window.cpp
)

target_include_directories(RawrXD-Chromatic PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(RawrXD-Chromatic PRIVATE 
    user32 
    gdi32 
    d3d11
    dxgi
    d3dcompiler
)

set_target_properties(RawrXD-Chromatic PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    WIN32_EXECUTABLE TRUE
)

if(MSVC)
    target_compile_options(RawrXD-Chromatic PRIVATE /EHsc /O2)
else()
    target_compile_options(RawrXD-Chromatic PRIVATE -fexceptions -O2)
endif()
target_compile_definitions(RawrXD-Chromatic PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)

message(STATUS "RawrXD-ModelLoader configured successfully with Clang C++20")

if(FALSE) # Disabled until headers exist
# IDE Test Runner - Comprehensive function testing
add_executable(RawrXD-TestRunner
    src/win32app/test_runner.cpp
    src/win32app/Win32IDE.cpp
    src/win32app/Win32IDE_Commands.cpp
    src/win32app/Win32IDE_Debugger.cpp
    src/win32app/Win32IDE_FileOps.cpp
    src/win32app/Win32IDE_PowerShell.cpp
    src/win32app/Win32IDE_PowerShellPanel.cpp
    src/win32app/Win32IDE_Sidebar.cpp
    src/win32app/Win32IDE_VSCodeUI.cpp
    src/win32app/Win32IDE_AgenticBridge.cpp
    src/win32app/Win32IDE_AgentCommands.cpp
    src/win32app/Win32TerminalManager.cpp
    src/win32app/TransparentRenderer.cpp
    src/win32app/VulkanRenderer.cpp
    src/gguf_loader.cpp
    src/streaming_gguf_loader.cpp
)

target_include_directories(RawrXD-TestRunner PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/win32app
)

target_link_libraries(RawrXD-TestRunner PRIVATE
    user32 gdi32 comctl32 comdlg32 shell32 shlwapi
    d3d11 dxgi d3dcompiler
    d2d1 dwrite dcomp dwmapi
    winhttp
)

set_target_properties(RawrXD-TestRunner PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    WIN32_EXECUTABLE TRUE
)

target_compile_options(RawrXD-TestRunner PRIVATE -O2)
target_compile_definitions(RawrXD-TestRunner PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

# Q8_0 AVX2 end-to-end bench
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bench_q8_0_end2end.cpp")
    add_executable(bench_q8_0_end2end
        tests/bench_q8_0_end2end.cpp
        kernels/q8_0_avx2.cc
        kernels/matmul_kernel_avx2.cc
    )
    target_include_directories(bench_q8_0_end2end PRIVATE ${CMAKE_SOURCE_DIR})
    target_compile_options(bench_q8_0_end2end PRIVATE /arch:AVX2 /O2)
    target_compile_definitions(bench_q8_0_end2end PRIVATE __AVX2__=1)
    set_target_properties(bench_q8_0_end2end PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )
endif()

# Flash-Attention AVX2 bench
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bench_flash_attn.cpp")
    add_executable(bench_flash_attn
        tests/bench_flash_attn.cpp
        kernels/flash_attn_avx2.cc
        kernels/matmul_kernel_avx2.cc
    )
    target_include_directories(bench_flash_attn PRIVATE ${CMAKE_SOURCE_DIR})
    target_compile_options(bench_flash_attn PRIVATE /arch:AVX2 /O2)
    target_compile_definitions(bench_flash_attn PRIVATE __AVX2__=1)
    set_target_properties(bench_flash_attn PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )
endif()
# Flash-Attention All-Quant bench
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bench_flash_all_quant.cpp")
    add_executable(bench_flash_all_quant
        tests/bench_flash_all_quant.cpp
        kernels/flash_attn_avx2.cc
        kernels/matmul_kernel_avx2.cc
    )
    target_include_directories(bench_flash_all_quant PRIVATE ${CMAKE_SOURCE_DIR})
    target_compile_options(bench_flash_all_quant PRIVATE /arch:AVX2 /O2)
    target_compile_definitions(bench_flash_all_quant PRIVATE __AVX2__=1)
    set_target_properties(bench_flash_all_quant PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )
endif()

# Flash-Attention ASM Puppeteer (Phase 4 Final Form)
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bench_flash_asm_puppeteer.cpp")
    if(MSVC)
        enable_language(ASM_MASM)
        add_executable(bench_flash_asm_puppeteer
            tests/bench_flash_asm_puppeteer.cpp
            kernels/flash_attn_avx2.cc
            kernels/matmul_kernel_avx2.cc
            kernels/flash_attn_asm_avx2.asm
        )
        target_include_directories(bench_flash_asm_puppeteer PRIVATE ${CMAKE_SOURCE_DIR})
        target_compile_options(bench_flash_asm_puppeteer PRIVATE /arch:AVX2 /O2)
        target_compile_definitions(bench_flash_asm_puppeteer PRIVATE __AVX2__=1)
        set_target_properties(bench_flash_asm_puppeteer PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        )
    endif()
endif()

# MASM God-Mode deflate benchmark
if(MSVC)
    enable_language(ASM_MASM)
    
    if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bench_deflate_masm.cpp")
        add_executable(bench_deflate_godmode tests/bench_deflate_masm.cpp kernels/deflate_godmode_masm.asm)
        target_compile_definitions(bench_deflate_godmode PRIVATE NO_ZLIB_REF=1 DEFLATE_GODMODE=1)
        set_target_properties(bench_deflate_godmode PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    endif()

    # Brutal MASM benchmark
    if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bench_deflate_brutal.cpp")
        add_executable(bench_deflate_brutal tests/bench_deflate_brutal.cpp kernels/deflate_brutal_masm.asm)
        set_target_properties(bench_deflate_brutal PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    endif()

endif()

# ARM-NEON benchmark (cross-compile or native ARM64)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
    enable_language(ASM)
    # Existing generic NEON bench (if present)
    if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bench_deflate_neon.cpp" AND EXISTS "${CMAKE_SOURCE_DIR}/kernels/deflate_neon.S")
        add_executable(bench_deflate_neon tests/bench_deflate_neon.cpp kernels/deflate_neon.S)
        set_target_properties(bench_deflate_neon PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    endif()

    # Brutal stored-block ARM64 bench
    if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bench_deflate_brutal_arm64.cpp" AND EXISTS "${CMAKE_SOURCE_DIR}/kernels/deflate_brutal_neon.S")
        add_executable(bench_deflate_brutal_arm64
            tests/bench_deflate_brutal_arm64.cpp
            kernels/deflate_brutal_neon.S
        )
        set_target_properties(bench_deflate_brutal_arm64 PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    endif()
endif()

# Add benchmark for 50MB payload
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bench_deflate_50mb.cpp")
    add_executable(bench_deflate_50mb tests/bench_deflate_50mb.cpp kernels/deflate_brutal_masm.asm)
    set_target_properties(bench_deflate_50mb PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
endif()






# Brutal Gzip Library auto-selects MASM on x64 and NEON on ARM64
add_library(brutal_gzip STATIC)
target_include_directories(brutal_gzip PUBLIC include)
# Architecture-specific kernels get added below (MASM on x64, NEON on ARM64).

if(MSVC AND CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64")
    enable_language(ASM_MASM)
    target_sources(brutal_gzip PRIVATE kernels/deflate_brutal_masm.asm)
    target_compile_definitions(brutal_gzip PUBLIC HAS_BRUTAL_GZIP_MASM=1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
    enable_language(ASM)
    target_sources(brutal_gzip PRIVATE kernels/deflate_neon.S)
    target_compile_definitions(brutal_gzip PUBLIC HAS_BRUTAL_GZIP_NEON=1)
endif()

