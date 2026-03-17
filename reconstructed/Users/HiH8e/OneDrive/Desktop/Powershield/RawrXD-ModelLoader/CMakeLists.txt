cmake_minimum_required(VERSION 3.20)
project(RawrXD-ModelLoader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# AVX2 optimization flag (ON for avx2 preset, OFF for scalar)
option(GGUF_ENABLE_AVX2 "Enable AVX2 SIMD kernels for Q4_0/RoPE" OFF)

# Detect compiler and set output directory suffix
if(MINGW)
    set(COMPILER_SUFFIX "mingw")
elseif(MSVC)
    set(COMPILER_SUFFIX "msvc")
else()
    set(COMPILER_SUFFIX "other")
endif()

# Build options
option(ENABLE_COPILOT "Enable Copilot streaming integration for Win32 IDE" ON)

# Force Windows SDK version that ships all required headers
set(CMAKE_SYSTEM_VERSION "10.0.22621.0")

# Set Qt path
# Qt path: prefer environment-provided Qt6_DIR (e.g. C:/Qt/6.7.3/msvc2022_64/lib/cmake/Qt6)
# or CMAKE_PREFIX_PATH (e.g. C:/Qt/6.7.3/msvc2022_64)
if(DEFINED ENV{Qt6_DIR})
    message(STATUS "Using Qt6 from ENV Qt6_DIR=$ENV{Qt6_DIR}")
    # Qt6_DIR should point to .../lib/cmake/Qt6, so use parent for PREFIX_PATH
    get_filename_component(QT_CMAKE_DIR "$ENV{Qt6_DIR}" DIRECTORY)
    get_filename_component(QT_LIB_DIR "${QT_CMAKE_DIR}" DIRECTORY)
    get_filename_component(QT_ROOT_DIR "${QT_LIB_DIR}" DIRECTORY)
    set(CMAKE_PREFIX_PATH "${QT_ROOT_DIR}")
    message(STATUS "Set CMAKE_PREFIX_PATH to ${CMAKE_PREFIX_PATH}")
endif()

# Pure-ASM compact wire bench (NASM). Optionally compares to Qt qCompress if Qt6::Core is available.
add_executable(bench_compact_asm
    RawrXD-ModelLoader/tests/bench_compact_asm.cpp
)

# Assemble NASM file via custom command to avoid MSBuild flags leakage
set(COMPACT_ASM_SRC ${CMAKE_SOURCE_DIR}/RawrXD-ModelLoader/kernels/compact_wire_asm.asm)
set(COMPACT_ASM_OBJ ${CMAKE_CURRENT_BINARY_DIR}/compact_wire_asm.obj)
add_custom_command(
    OUTPUT ${COMPACT_ASM_OBJ}
    COMMAND nasm -f win64 -o ${COMPACT_ASM_OBJ} ${COMPACT_ASM_SRC}
    DEPENDS ${COMPACT_ASM_SRC}
    COMMENT "Assembling compact_wire_asm.asm with nasm"
)
add_custom_target(compact_wire_asm_obj DEPENDS ${COMPACT_ASM_OBJ})
add_dependencies(bench_compact_asm compact_wire_asm_obj)
target_sources(bench_compact_asm PRIVATE ${COMPACT_ASM_OBJ})

find_package(Qt6 COMPONENTS Core QUIET)
if(Qt6Core_FOUND)
    target_link_libraries(bench_compact_asm PRIVATE Qt6::Core)
    target_compile_definitions(bench_compact_asm PRIVATE QT_CORE_LIB)
endif()

if(MSVC)
    target_compile_options(bench_compact_asm PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/O2 /EHsc>)
else()
    target_compile_options(bench_compact_asm PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-O3>)
endif()
set_target_properties(bench_compact_asm PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")

# Qt-based IDE shell (separate executable, no Vulkan)
if(MSVC)
    find_package(Qt6 COMPONENTS Widgets Network WebSockets QUIET)
    if(Qt6_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/TelemetryDialog.cpp") # ENABLED only if sources exist
        message(STATUS "Building Qt Shell with Chat Workspace - Qt version: ${Qt6_VERSION}")
        
        # Enable Qt MOC for QObject classes
        set(CMAKE_AUTOMOC ON)

        # Create Qt target with new chat workspace integration
        add_executable(RawrXD-QtShell 
            src/qtapp/main_qt.cpp
            src/qtapp/MainWindow.cpp
            src/qtapp/MainWindow.h
            src/qtapp/TelemetryDialog.cpp
            src/qtapp/TelemetryDialog.h
            src/chat_workspace.cpp
            src/file_browser_tree.cpp
            src/multi_tab_editor.cpp
            src/terminal_pool.cpp
            src/agentic_chat.cpp
            src/scalar_server.cpp
            src/chat_interface.cpp
            src/file_browser.cpp
            src/agentic_engine.cpp
            src/inference_engine.cpp
            src/gguf_loader.cpp
            src/vulkan_compute.cpp
            src/transformer_block_scalar.cpp
            src/settings.cpp
            src/telemetry.cpp
        )

        # Ensure QtShell links all required QObject implementations
        target_sources(RawrXD-QtShell PRIVATE
            src/qtapp/ui/AISuggestionOverlay.cpp
            src/qtapp/StreamerClient.cpp
            src/qtapp/ui/TaskProposalWidget.cpp
            src/llm_adapter/GGUFRunner.cpp
            src/llm_adapter/GGUFRunner.h
            src/qtapp/utils/ModelCompilerUtils.cpp
            src/qtapp/utils/ModelCompilerUtils.h
            src/orchestrator/TaskNode.cpp
            src/orchestrator/AgentOrchestrator.cpp
            src/llm_adapter/matmul_avx2.cpp
            src/qtapp/widgets/CloudRunnerWidget.cpp
            src/qtapp/widgets/CloudRunnerWidget.h
            src/qtapp/widgets/LanguageClientHost.cpp
            src/qtapp/widgets/LanguageClientHost.h
            src/qtapp/widgets/InlineChatWidget.cpp
            src/qtapp/widgets/InlineChatWidget.h
            src/qtapp/widgets/PluginManagerWidget.cpp
            src/qtapp/widgets/PluginManagerWidget.h
            src/llm_adapter/gguf_loader.cpp
            src/llm_adapter/gguf_loader.h
        )
        
        # Conditional AVX2 kernels
        if(GGUF_ENABLE_AVX2)
            target_sources(RawrXD-QtShell PRIVATE
                src/llm_adapter/avx2_kernels.cpp
                src/llm_adapter/avx2_kernels.h
            )
            target_compile_definitions(RawrXD-QtShell PRIVATE GGUF_ENABLE_AVX2)
            message(STATUS "AVX2 kernels ENABLED (Q4_0/RoPE optimized)")
        else()
            message(STATUS "AVX2 kernels DISABLED (scalar fallback)")
        endif()
        
        # SCALAR-ONLY: No assembly kernels
        # if(MSVC)
        #     enable_language(ASM_MASM)
        #     target_sources(RawrXD-QtShell PRIVATE
        #         src/llm_adapter/gguf_kernels.asm
        #     )
        # endif()
        
        target_link_libraries(RawrXD-QtShell PRIVATE 
            Qt6::Widgets Qt6::Network Qt6::WebSockets Qt6::Core
            ws2_32 winhttp wbemuuid pdh
            user32 gdi32 comctl32 comdlg32 shell32 shlwapi
        )
        target_include_directories(RawrXD-QtShell PRIVATE include ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/qtapp)
        
        set_target_properties(RawrXD-QtShell PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin-${COMPILER_SUFFIX}"
            WIN32_EXECUTABLE TRUE
        )

        # Compiler-specific optimizations
        if(MSVC)
            if(GGUF_ENABLE_AVX2)
                # AVX2 preset: /arch:AVX2 /fp:fast already set via CMAKE_CXX_FLAGS_RELEASE
                target_compile_options(RawrXD-QtShell PRIVATE /EHsc)
            else()
                # Scalar preset: portable x64 baseline
                target_compile_options(RawrXD-QtShell PRIVATE /EHsc /O2)
            endif()
        else()
            target_compile_options(RawrXD-QtShell PRIVATE -fexceptions -O2)
        endif()
        
        target_compile_definitions(RawrXD-QtShell PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
    else()
        message(STATUS "Qt6 not found, building Win32 IDE with terminal integration")
    endif()
else()
    message(STATUS "MSVC not detected; skipping Qt6 targets to avoid MinGW/MSVC mismatch")
endif()

# Win32-based IDE (MSVC only to match Qt)
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
    src/inference_engine.cpp
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
    src/inference_engine.cpp
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

if(Qt6Widgets_FOUND)
    message(STATUS "Qt6Widgets already configured above")
endif()

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

# Flash-attention ASM puppeteer benchmark (MSVC + AVX2)
if(MSVC)
    enable_language(ASM_MASM)
    add_executable(bench_flash_asm_puppeteer
        RawrXD-ModelLoader/tests/bench_flash_asm_puppeteer.cpp
        RawrXD-ModelLoader/kernels/q8_0_avx2.cc
        RawrXD-ModelLoader/kernels/flash_attn_avx2.cc
        RawrXD-ModelLoader/kernels/flash_attn_puppeteer_avx2.asm
    )
    # Force AVX2 for this target
    target_compile_options(bench_flash_asm_puppeteer PRIVATE /O2 /arch:AVX2 /EHsc)
    target_compile_definitions(bench_flash_asm_puppeteer PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(bench_flash_asm_puppeteer PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>"
    )
else()
    add_executable(bench_flash_asm_puppeteer
        RawrXD-ModelLoader/tests/bench_flash_asm_puppeteer.cpp
        RawrXD-ModelLoader/kernels/q8_0_avx2.cc
        RawrXD-ModelLoader/kernels/flash_attn_avx2.cc
    )
    target_compile_options(bench_flash_asm_puppeteer PRIVATE -O3 -mavx2)
    set_target_properties(bench_flash_asm_puppeteer PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )
endif()

# Scaled (8Kx128) flash-attention bench
if(MSVC)
    enable_language(ASM_MASM)
    add_executable(bench_flash_asm_scaled
        RawrXD-ModelLoader/tests/bench_flash_asm_scaled.cpp
        RawrXD-ModelLoader/kernels/q8_0_avx2.cc
        RawrXD-ModelLoader/kernels/flash_attn_avx2.cc
        RawrXD-ModelLoader/kernels/flash_attn_puppeteer_avx2.asm
    )
    target_compile_options(bench_flash_asm_scaled PRIVATE /O2 /arch:AVX2 /EHsc)
    target_compile_definitions(bench_flash_asm_scaled PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(bench_flash_asm_scaled PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>"
    )
else()
    add_executable(bench_flash_asm_scaled
        RawrXD-ModelLoader/tests/bench_flash_asm_scaled.cpp
        RawrXD-ModelLoader/kernels/q8_0_avx2.cc
        RawrXD-ModelLoader/kernels/flash_attn_avx2.cc
    )
    target_compile_options(bench_flash_asm_scaled PRIVATE -O3 -mavx2)
    set_target_properties(bench_flash_asm_scaled PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )
endif()

# Ultimate-scale (16Kx256) flash-attention bench
if(MSVC)
    enable_language(ASM_MASM)
    add_executable(bench_flash_asm_ultimate
        RawrXD-ModelLoader/tests/bench_flash_asm_ultimate.cpp
        RawrXD-ModelLoader/kernels/q8_0_avx2.cc
        RawrXD-ModelLoader/kernels/flash_attn_avx2.cc
        RawrXD-ModelLoader/kernels/flash_attn_puppeteer_avx2.asm
    )
    target_compile_options(bench_flash_asm_ultimate PRIVATE /O2 /arch:AVX2 /EHsc)
    target_compile_definitions(bench_flash_asm_ultimate PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(bench_flash_asm_ultimate PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>"
    )
else()
    add_executable(bench_flash_asm_ultimate
        RawrXD-ModelLoader/tests/bench_flash_asm_ultimate.cpp
        RawrXD-ModelLoader/kernels/q8_0_avx2.cc
        RawrXD-ModelLoader/kernels/flash_attn_avx2.cc
    )
    target_compile_options(bench_flash_asm_ultimate PRIVATE -O3 -mavx2)
    set_target_properties(bench_flash_asm_ultimate PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )
endif()

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

if(FALSE) # Disabled until headers exist
# Simple C++ IDE with advanced features stubs
add_executable(RawrXD-SimpleIDE
    src/qtapp/main_simple.cpp
    src/qtapp/MainWindowSimple.cpp
    src/qtapp/MainWindowSimple.h
    src/editor_buffer.cpp
    src/syntax_engine.cpp
    src/overclock_vendor.cpp
    src/overclock_governor.cpp
    src/settings.cpp
    src/telemetry.cpp
    src/baseline_profile.cpp
    src/gguf_loader.cpp
    src/telemetry/ai_metrics.cpp
    src/session/ai_session.cpp
    src/backend/ollama_client.cpp
    src/backend/websocket_server.cpp
    src/backend/agentic_tools.cpp
    src/tools/file_ops.cpp
    src/tools/git_client.cpp
    src/context/indexer.cpp
    src/context/semantic_store.cpp
    src/ui/split_layout.cpp
    src/ui/chat_panel.cpp
)

target_include_directories(RawrXD-SimpleIDE PRIVATE include)
target_link_libraries(RawrXD-SimpleIDE PRIVATE 
    user32 
    gdi32 
    wbemuuid 
    pdh
    winhttp
    ws2_32
)

set_target_properties(RawrXD-SimpleIDE PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Enable standard exception semantics and optimization for SimpleIDE
if(MSVC)
    target_compile_options(RawrXD-SimpleIDE PRIVATE /EHsc /O2)
else()
    target_compile_options(RawrXD-SimpleIDE PRIVATE -fexceptions -O2)
endif()
target_compile_definitions(RawrXD-SimpleIDE PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Chromatic Wave Demo - Chameleon/Neon effects with D3D11
# Target: 540Hz @ 3840x2160 (4K UHD)
if(EXISTS "${CMAKE_SOURCE_DIR}/src/chromatic_main.cpp")
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
endif()

# Hand-written assembly kernel library (pure NASM, no C++ flags)
if(MSVC AND EXISTS "${CMAKE_SOURCE_DIR}/kernels/gemm_kernel.asm")
    enable_language(ASM_NASM)
    add_library(kernels OBJECT kernels/gemm_kernel.asm)
    set_source_files_properties(kernels/gemm_kernel.asm
                                PROPERTIES LANGUAGE ASM_NASM
                                           COMPILE_FLAGS "")
endif()

# Standalone API Server with Inference Engine
if(EXISTS "${CMAKE_SOURCE_DIR}/src/server_main.cpp")
add_executable(RawrXD-Server
    src/server_main.cpp
    src/api_server.cpp
    src/inference_engine.cpp
    src/gguf_loader.cpp
    src/vulkan_compute.cpp
    src/settings.cpp
    src/telemetry.cpp
    src/transformer_block_scalar.cpp
)

# Link hand-written assembly kernel
if(MSVC)
    target_link_libraries(RawrXD-Server PRIVATE kernels)
endif()

target_include_directories(RawrXD-Server PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(RawrXD-Server PRIVATE 
    ws2_32
    winhttp
    wbemuuid
    pdh
)

if(Vulkan_FOUND)
    target_link_libraries(RawrXD-Server PRIVATE Vulkan::Vulkan)
endif()

if(MSVC)
    target_compile_options(RawrXD-Server PRIVATE /EHsc /O2)
else()
    target_compile_options(RawrXD-Server PRIVATE -fexceptions -O2)
endif()

target_compile_definitions(RawrXD-Server PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)

set_target_properties(RawrXD-Server PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
endif()

# Agentic IDE with all scalar components integrated
if(EXISTS "${CMAKE_SOURCE_DIR}/src/agentic_ide_main.cpp")
add_executable(RawrXD-AgenticIDE WIN32
    src/agentic_ide_main.cpp
    src/agentic_ide.cpp
    src/scalar_server.cpp
    src/file_browser_tree.cpp
    src/agentic_chat.cpp
    src/chat_workspace.cpp
    src/multi_tab_editor.cpp
    src/terminal_pool.cpp
    src/agentic_engine.cpp
    src/chat_interface.cpp
    src/file_browser.cpp
    src/inference_engine.cpp
    src/gguf_loader.cpp
    src/vulkan_compute.cpp
    src/transformer_block_scalar.cpp
    src/settings.cpp
    src/telemetry.cpp
)

target_include_directories(RawrXD-AgenticIDE PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(RawrXD-AgenticIDE PRIVATE 
    user32 gdi32 comctl32 comdlg32 shell32 shlwapi
    ws2_32 winhttp wbemuuid pdh
)

if(Vulkan_FOUND)
    target_link_libraries(RawrXD-AgenticIDE PRIVATE Vulkan::Vulkan)
endif()

if(MSVC)
    target_compile_options(RawrXD-AgenticIDE PRIVATE /EHsc /O2)
else()
    target_compile_options(RawrXD-AgenticIDE PRIVATE -fexceptions -O2)
endif()

target_compile_definitions(RawrXD-AgenticIDE PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)

set_target_properties(RawrXD-AgenticIDE PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    WIN32_EXECUTABLE TRUE
)
endif()

message(STATUS "RawrXD-ModelLoader configured successfully with Clang C++20")

# ------------------------
# Q4_0 unpack unit test
# ------------------------
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/test_q4_0_unpack.cpp")
find_package(Threads)
add_executable(test_q4_0_unpack
    tests/test_q4_0_unpack.cpp
    kernels/q4_0_unpack_avx2.cc
)
if(MSVC)
    target_compile_options(test_q4_0_unpack PRIVATE /EHsc /O2)
else()
    target_compile_options(test_q4_0_unpack PRIVATE -fexceptions -O2)
endif()
if(GGUF_ENABLE_AVX2)
    target_compile_definitions(test_q4_0_unpack PRIVATE __AVX2__)
endif()
target_link_libraries(test_q4_0_unpack PRIVATE ${CMAKE_THREAD_LIBS_INIT})
set_target_properties(test_q4_0_unpack PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)
endif()

# ------------------------
# Q4_0 end-to-end benchmark
# ------------------------
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bench_q4_0_end2end.cpp")
add_executable(bench_q4_0_end2end
    tests/bench_q4_0_end2end.cpp
    kernels/q4_0_unpack_avx2.cc
    kernels/q4_0_gemm_avx2.cc
    src/llm_adapter/matmul_avx2.cpp
)
if(MSVC)
    target_compile_options(bench_q4_0_end2end PRIVATE /EHsc /O2)
else()
    target_compile_options(bench_q4_0_end2end PRIVATE -fexceptions -O2)
endif()
if(GGUF_ENABLE_AVX2)
    target_compile_definitions(bench_q4_0_end2end PRIVATE __AVX2__)
endif()
target_link_libraries(bench_q4_0_end2end PRIVATE ${CMAKE_THREAD_LIBS_INIT})
set_target_properties(bench_q4_0_end2end PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)
endif()

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
        kernels/q8_0_gemm_avx2.cc
        kernels/q8_0_unpack_avx2.cc
        kernels/matmul_kernel_avx2.cc
        kernels/q8_0_avx2.cc
    )
    target_include_directories(bench_q8_0_end2end PRIVATE ${CMAKE_SOURCE_DIR})
    target_compile_options(bench_q8_0_end2end PRIVATE /arch:AVX2 /O2)
    target_compile_definitions(bench_q8_0_end2end PRIVATE __AVX2__=1)
    set_target_properties(bench_q8_0_end2end PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )
endif()

