cmake_minimum_required(VERSION 3.20)
project(RawrEngine VERSION 7.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-D_CRT_SECURE_NO_WARNINGS -DNOMINMAX -DWIN32_LEAN_AND_MEAN)

find_package(Threads REQUIRED)

set(SOURCES 
    src/main.cpp
    src/stubs.cpp
    src/memory_core.cpp
    src/hot_patcher.cpp
    src/cpu_inference_engine.cpp
    src/complete_server.cpp
    src/agentic_engine.cpp
    src/subagent_core.cpp
    src/agent_history.cpp
    src/agent_policy.cpp
    src/agent_explainability.cpp
    src/streaming_gguf_loader.cpp
    src/gguf_loader.cpp
    src/gguf_vocab_resolver.cpp
    src/compression_interface.cpp
    
    src/engine/gguf_core.cpp
    src/engine/inference_kernels.cpp
    src/engine/transformer.cpp
    src/engine/bpe_tokenizer.cpp
    src/engine/sampler.cpp
    src/engine/rawr_engine.cpp
    src/engine/core_generator.cpp
    
    src/codec/compression.cpp
    src/codec/brutal_gzip.cpp
)

add_executable(RawrEngine ${SOURCES})

target_include_directories(RawrEngine PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(RawrEngine PRIVATE Threads::Threads)

if(WIN32)
    target_link_libraries(RawrEngine PRIVATE shlwapi psapi dbghelp comctl32 comdlg32 shell32 ole32 oleaut32 uuid winhttp ws2_32 gomp)
endif()

if(NOT MSVC)
    target_compile_options(RawrEngine PRIVATE -march=native -O3 -w -mavx2 -mfma -fopenmp)
endif()

add_executable(rawrxd-monaco-gen
    src/monaco_gen.cpp
    src/engine/react_ide_generator.cpp
)

target_include_directories(rawrxd-monaco-gen PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(rawrxd-monaco-gen PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY bin
    OUTPUT_NAME rawrxd-monaco-gen
)

# Win32 GUI IDE
if(WIN32)
    set(WIN32IDE_SOURCES
        src/win32app/main_win32.cpp
        src/win32app/Win32IDE.cpp
        src/win32app/Win32IDE.h
        src/win32app/Win32IDE_Core.cpp
        src/win32app/Win32IDE_Sidebar.cpp
        src/win32app/Win32IDE_VSCodeUI.cpp
        src/win32app/Win32IDE_PowerShellPanel.cpp
        src/win32app/Win32IDE_PowerShell.cpp
        src/win32app/Win32IDE_Logger.cpp
        src/win32app/IDELogger.h
        src/win32app/Win32IDE_FileOps.cpp
        src/win32app/Win32IDE_Commands.cpp
        src/win32app/Win32IDE_Debugger.cpp
        src/win32app/Win32IDE_AgenticBridge.h
        src/win32app/Win32IDE_AgenticBridge.cpp
        src/win32app/Win32IDE_AgentCommands.cpp
        src/win32app/Win32IDE_Autonomy.h
        src/win32app/Win32IDE_Autonomy.cpp
        src/win32app/Win32IDE_SyntaxHighlight.cpp
        src/win32app/Win32IDE_Themes.cpp
        src/win32app/Win32IDE_Annotations.cpp
        src/win32app/Win32IDE_Session.cpp
        src/win32app/Win32IDE_StreamingUX.cpp
        src/win32app/Win32IDE_ReverseEngineering.cpp
        src/win32app/Win32IDE_GhostText.cpp
        src/win32app/Win32IDE_PlanExecutor.cpp
        src/win32app/Win32IDE_FailureDetector.cpp
        src/win32app/Win32IDE_FailureIntelligence.cpp
        src/win32app/Win32IDE_Settings.cpp
        src/win32app/Win32IDE_LocalServer.cpp
        src/win32app/Win32IDE_BackendSwitcher.cpp
        src/win32app/Win32IDE_LLMRouter.cpp
        src/win32app/Win32IDE_LSPClient.cpp
        src/win32app/Win32IDE_SubAgent.h
        src/win32app/Win32IDE_SubAgent.cpp
        src/win32app/Win32IDE_AgentHistory.cpp
        src/win32app/Win32TerminalManager.cpp
        src/win32app/Win32TerminalManager.h
        src/win32app/TransparentRenderer.cpp
        src/win32app/TransparentRenderer.h
        src/win32app/ModelConnection.h
        src/win32app/ContextManager.h
        
        # Configuration management
        src/config/IDEConfig.cpp
        
        # Core engine files needed by IDE
        src/stubs.cpp
        src/agentic_engine.cpp
        src/subagent_core.cpp
        src/agent_history.cpp
        src/agent_policy.cpp
        src/agent_explainability.cpp
        src/utils/ErrorReporter.cpp
        src/modules/engine_manager.cpp
        src/cpu_inference_engine.cpp
        src/memory_core.cpp
        src/hot_patcher.cpp
        src/streaming_gguf_loader.cpp
        src/gguf_loader.cpp
        src/gguf_vocab_resolver.cpp
        src/model_source_resolver.cpp
        src/compression_interface.cpp
        src/engine/gguf_core.cpp
        src/engine/inference_kernels.cpp
        src/engine/transformer.cpp
        src/engine/bpe_tokenizer.cpp
        src/engine/sampler.cpp
        src/engine/rawr_engine.cpp
        src/engine/core_generator.cpp
        src/codec/compression.cpp
        src/codec/brutal_gzip.cpp
        
        # Streaming engine registry (Phase 9)
        src/core/streaming_engine_registry.cpp
        
        # Module implementations
        src/modules/vsix_loader_win32.cpp
        src/modules/codex_ultimate.cpp
    )
    
    add_executable(RawrXD-Win32IDE WIN32 ${WIN32IDE_SOURCES})
    
    target_link_libraries(RawrXD-Win32IDE PRIVATE
        Threads::Threads
        comctl32 comdlg32 shell32 ole32 oleaut32 uuid
        shlwapi psapi dbghelp winhttp ws2_32 winmm gdi32 user32
        d2d1 dwrite d3d11 dcomp d3dcompiler dwmapi
        gomp
    )
    
    target_include_directories(RawrXD-Win32IDE PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/engine
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core
        ${CMAKE_CURRENT_SOURCE_DIR}/src/win32app
        ${CMAKE_CURRENT_SOURCE_DIR}/src/modules
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
        ${CMAKE_CURRENT_SOURCE_DIR}/src/config
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    if(MSVC)
        target_compile_options(RawrXD-Win32IDE PRIVATE /EHsc /O2 /W0 /arch:AVX2)
    else()
        target_compile_options(RawrXD-Win32IDE PRIVATE -O3 -w -mavx2 -mfma -march=native -fopenmp)
    endif()

    set_target_properties(RawrXD-Win32IDE PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY bin
        OUTPUT_NAME RawrXD-Win32IDE
    )
endif()