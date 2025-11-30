cmake_minimum_required(VERSION 3.20)
project(RawrXD-ModelLoader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build options
option(ENABLE_COPILOT "Enable Copilot streaming integration for Win32 IDE" OFF)

# Force Windows SDK version that ships all required headers
set(CMAKE_SYSTEM_VERSION "10.0.22621.0")

# Set Qt path
# Qt path: prefer environment-provided Qt6_DIR (e.g. C:/Qt/6.7.2/msvc2022_64/lib/cmake/Qt6)
if(DEFINED ENV{Qt6_DIR})
    message(STATUS "Using Qt6 from ENV Qt6_DIR=$ENV{Qt6_DIR}")
    set(CMAKE_PREFIX_PATH "$ENV{Qt6_DIR}")
endif()

# Qt-based IDE shell (separate executable, no Vulkan)
find_package(Qt6 COMPONENTS Widgets QUIET)

if(Qt6_FOUND)
    message(STATUS "Building Qt Shell with Qt version: ${Qt6_VERSION}")
    
    # Create a separate Qt target that doesn't depend on problematic files
    add_executable(RawrXD-QtShell 
        src/qtapp/main_qt.cpp
        src/qtapp/MainWindow.cpp
        src/qtapp/MainWindow.h
    )
    
    target_link_libraries(RawrXD-QtShell PRIVATE Qt6::Widgets)
    target_include_directories(RawrXD-QtShell PRIVATE include)
    
    set_target_properties(RawrXD-QtShell PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        WIN32_EXECUTABLE TRUE
    )
else()
    message(STATUS "Qt6 not found, building Win32 IDE with terminal integration")
    
    # Win32-based IDE with terminal support
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
    
    # Compiler options - use appropriate flags for different compilers
    if(MSVC)
        target_compile_options(RawrXD-Win32IDE PRIVATE /EHsc)
    else()
        target_compile_options(RawrXD-Win32IDE PRIVATE -fexceptions)
    endif()
    
    set_target_properties(RawrXD-Win32IDE PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        WIN32_EXECUTABLE TRUE
    )
    if(ENABLE_VULKAN)
        target_compile_definitions(RawrXD-Win32IDE PRIVATE ENABLE_VULKAN=1)
    endif()
    if(ENABLE_COPILOT)
        target_compile_definitions(RawrXD-Win32IDE PRIVATE ENABLE_COPILOT=1)
        message(STATUS "Copilot integration enabled for Win32 IDE")
    endif()

    # Terminal test executable (only if source exists)
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/win32app/test_terminal.cpp")
        add_executable(test_terminal
            src/win32app/test_terminal.cpp
            src/win32app/Win32TerminalManager.cpp
            src/win32app/Win32TerminalManager.h
        )
        set_target_properties(test_terminal PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        )
    endif()

    # Chat application (taskbar integration) - behind ENABLE_COPILOT and file guard
    option(ENABLE_COPILOT "Enable Copilot chat integration" ON)
    if(ENABLE_COPILOT AND EXISTS "${CMAKE_SOURCE_DIR}/src/win32app/main_chat.cpp")
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
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
            WIN32_EXECUTABLE TRUE
        )
    endif()
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


# Use MinGW
set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

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
set_target_properties(RawrXD-CLI PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

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
target_compile_options(RawrXD-ModelLoader PRIVATE /O2 /EHsc)
target_compile_definitions(RawrXD-ModelLoader PRIVATE _CRT_SECURE_NO_WARNINGS _WINDOWS)

# Output directory
set_target_properties(RawrXD-ModelLoader PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

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
set_target_properties(rawrxd_stress PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_compile_options(model_loader_bench PRIVATE /O2 /EHsc)
set_target_properties(model_loader_bench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

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
target_compile_options(RawrXD-SimpleIDE PRIVATE /EHsc /O2)
target_compile_definitions(RawrXD-SimpleIDE PRIVATE _CRT_SECURE_NO_WARNINGS)

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

target_compile_options(RawrXD-Chromatic PRIVATE /EHsc /O2)
target_compile_definitions(RawrXD-Chromatic PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)

message(STATUS "RawrXD-ModelLoader configured successfully with Clang C++20")

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

