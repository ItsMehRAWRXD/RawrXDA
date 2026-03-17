cmake_minimum_required(VERSION 3.15)
project(RawrXD LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================================
# Build Options
# ============================================================================

option(USE_GPU "Enable GPU acceleration with Vulkan" ON)
option(ENABLE_TIME_TRAVEL "Enable time-travel execution in polymorphic loader" ON)
option(USE_WIN32 "Build with Win32 API support" ON)
option(BUILD_TESTS "Build test executables" ON)
option(BUILD_QTAPP "Build the RawrXD-Shell application" ON)

# ============================================================================
# Dependencies
# ============================================================================

# GGML is optional — many components provide stub types when unavailable
find_package(GGML QUIET)
if(GGML_FOUND)
    message(STATUS "✓ GGML found: ${GGML_LIBRARY}")
else()
    message(STATUS "⚠ GGML not found — building with stub types")
    set(GGML_LIBRARY "")
    set(GGML_INCLUDE_DIR "")
endif()

if(USE_GPU)
    find_package(Vulkan QUIET)
    if(Vulkan_FOUND)
        message(STATUS "✓ Vulkan found")
    else()
        message(STATUS "⚠ Vulkan not found — GPU acceleration disabled")
        set(USE_GPU OFF)
    endif()
endif()

if(USE_WIN32)
    if(NOT WIN32)
        message(FATAL_ERROR "Win32 support requires Windows platform")
    endif()
endif()

# ============================================================================
# Compiler Settings
# ============================================================================

if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_definitions(_WINSOCK_DEPRECATION_NO_WARNINGS)
    # Suppress specific warnings for third-party patterns
    add_compile_options(/W3 /wd4244 /wd4267 /wd4996)
endif()

# ============================================================================
# Include Directories
# ============================================================================

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common
    ${CMAKE_CURRENT_SOURCE_DIR}/src/agent
    ${CMAKE_CURRENT_SOURCE_DIR}/src/qtapp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/qtapp/gguf
    ${CMAKE_CURRENT_SOURCE_DIR}/src/qtapp/ui
)

if(GGML_INCLUDE_DIR)
    include_directories(${GGML_INCLUDE_DIR})
endif()

if(USE_GPU AND Vulkan_FOUND)
    include_directories(${Vulkan_INCLUDE_DIRS})
endif()

# ============================================================================
# Source Files Grouped by Component
# ============================================================================

# Core inference library
set(INFERENCE_SOURCES
    src/ultra_fast_inference.cpp
    src/ultra_fast_inference.h
)

# Win32 agent tools
set(AGENT_TOOLS_SOURCES
    src/win32_agent_tools.h
    src/win32_agent_tools.cpp
)

# Blob parser
set(BLOB_SOURCES
    src/ollama_blob_parser.h
    src/ollama_blob_parser.cpp
)

# Polymorphic loader
set(POLYMORPHIC_SOURCES
    src/polymorphic_loader.cpp
    src/polymorphic_loader.h
)

# Agent framework sources
set(AGENT_FRAMEWORK_SOURCES
    src/agent/agent_hot_patcher.hpp
    src/agent/agent_hot_patcher.cpp
    src/agent/agent_memory.hpp
    src/agent/agent_memory.cpp
    src/agent/agent_self_reflection.hpp
    src/agent/agent_self_reflection.cpp
    src/agent/autonomous_decision_engine.hpp
    src/agent/autonomous_decision_engine.cpp
    src/agent/hierarchical_planner.hpp
    src/agent/hierarchical_planner.cpp
    src/agent/memory_hot_patcher.hpp
    src/agent/memory_hot_patcher.cpp
    src/agent/project_context.hpp
    src/agent/project_context.cpp
)

# Agentic tools and model interface sources (src/ root)
set(MODEL_SOURCES
    src/agentic_tools.hpp
    src/agentic_tools.cpp
    src/autonomous_feature_engine.h
    src/autonomous_feature_engine.cpp
    src/autonomous_model_manager.h
    src/autonomous_model_manager.cpp
    src/autonomous_widgets.h
    src/autonomous_widgets.cpp
    src/ide_main_window.h
    src/ide_main_window.cpp
    src/intelligent_codebase_engine.h
    src/intelligent_codebase_engine.cpp
    src/model_interface.h
    src/model_interface.cpp
    src/model_interface_examples.cpp
    src/model_trainer.h
    src/model_trainer.cpp
    src/model_trainer_enhancements.h
    src/universal_model_router.h
    src/universal_model_router.cpp
    src/validate_agentic_tools.cpp
)

# qtapp sources (application layer)
set(QTAPP_SOURCES
    src/qtapp/tokenizer.hpp
    src/qtapp/unified_hotpatch_manager.hpp
    src/qtapp/unified_hotpatch_manager.cpp
    src/qtapp/transformer_inference.hpp
    src/qtapp/transformer_inference.cpp
    src/qtapp/inference_engine.hpp
    src/qtapp/inference_engine.cpp
    src/qtapp/health_check_server.hpp
    src/qtapp/health_check_server.cpp
    src/qtapp/HexMagConsole.h
    src/qtapp/HexMagConsole.cpp
    src/qtapp/MainWindow.h
    src/qtapp/MainWindow.cpp
    src/qtapp/MainWindow_AI_Integration.cpp
    src/qtapp/gguf/StreamingGGUFLoader.h
    src/qtapp/gguf/StreamingGGUFLoader.cpp
    src/qtapp/ui/OverlayWidget.h
    src/qtapp/ui/OverlayWidget.cpp
)

# Project context is already included in AGENT_FRAMEWORK_SOURCES
# (src/agent/project_context.hpp and src/agent/project_context.cpp)

# Test sources
set(TEST_SOURCES
    src/test_agentic_tools.cpp
    src/test_agent_hot_patcher.cpp
    src/test_agent_hot_patcher_integration.cpp
    src/test_agent_hot_patcher_qtest.cpp
    src/test_model_interface.cpp
    src/test_model_trainer_validation.cpp
)

# ============================================================================
# ultra_fast_inference Library
# ============================================================================

add_library(ultra_fast_inference_core STATIC ${INFERENCE_SOURCES})

if(GGML_LIBRARY)
    target_link_libraries(ultra_fast_inference_core PUBLIC ${GGML_LIBRARY})
endif()

if(USE_GPU AND Vulkan_FOUND)
    target_link_libraries(ultra_fast_inference_core PUBLIC Vulkan::Vulkan)
    target_compile_definitions(ultra_fast_inference_core PRIVATE ENABLE_GPU=1)
endif()

if(MSVC)
    target_compile_options(ultra_fast_inference_core PRIVATE /O2 /arch:AVX2)
endif()
set_property(TARGET ultra_fast_inference_core PROPERTY FOLDER "Core Libraries")

# ============================================================================
# win32_agent_tools Library
# ============================================================================

if(USE_WIN32)
    add_library(win32_agent_tools STATIC ${AGENT_TOOLS_SOURCES})

    target_link_libraries(win32_agent_tools
        PUBLIC kernel32 user32 advapi32 wininet psapi shlwapi ntdll
    )

    if(MSVC)
        target_compile_options(win32_agent_tools PRIVATE /O2)
    endif()
    set_property(TARGET win32_agent_tools PROPERTY FOLDER "Core Libraries")
endif()

# ============================================================================
# ollama_blob_parser Library
# ============================================================================

add_library(ollama_blob_parser STATIC ${BLOB_SOURCES})

target_link_libraries(ollama_blob_parser
    PUBLIC ultra_fast_inference_core
)

if(MSVC)
    target_compile_options(ollama_blob_parser PRIVATE /O2)
endif()
set_property(TARGET ollama_blob_parser PROPERTY FOLDER "Core Libraries")

# ============================================================================
# polymorphic_loader Library (Format-Agnostic Model Streaming)
# ============================================================================

add_library(polymorphic_loader STATIC ${POLYMORPHIC_SOURCES})

target_include_directories(polymorphic_loader PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(GGML_LIBRARY)
    target_link_libraries(polymorphic_loader PUBLIC ${GGML_LIBRARY})
endif()
target_link_libraries(polymorphic_loader PUBLIC ultra_fast_inference_core)

if(ENABLE_TIME_TRAVEL)
    target_compile_definitions(polymorphic_loader PUBLIC ENABLE_TIME_TRAVEL=1)
endif()

if(MSVC)
    target_compile_options(polymorphic_loader PRIVATE /O2 /arch:AVX2)
endif()
set_property(TARGET polymorphic_loader PROPERTY FOLDER "Core Libraries")

message(STATUS "✓ Polymorphic loader configured (70B-700B+ model streaming)")

# ============================================================================
# Agent Framework Library
# ============================================================================

add_library(agent_framework STATIC
    ${AGENT_FRAMEWORK_SOURCES}
    ${MODEL_SOURCES}
)

target_include_directories(agent_framework PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common
    ${CMAKE_CURRENT_SOURCE_DIR}/src/agent
)

if(USE_WIN32)
    target_link_libraries(agent_framework
        PUBLIC kernel32 user32 advapi32 wininet psapi shlwapi
    )
endif()

if(GGML_LIBRARY)
    target_link_libraries(agent_framework PUBLIC ${GGML_LIBRARY})
endif()

if(MSVC)
    target_compile_options(agent_framework PRIVATE /O2)
endif()
set_property(TARGET agent_framework PROPERTY FOLDER "Agent Framework")

message(STATUS "✓ Agent framework configured (12 agent files + 12 model files)")

# ============================================================================
# RawrXD-Shell Application (qtapp)
# ============================================================================

if(BUILD_QTAPP)
    add_executable(RawrXD-Shell
        src/qtapp/main.cpp
        ${QTAPP_SOURCES}
    )

    target_include_directories(RawrXD-Shell PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common
        ${CMAKE_CURRENT_SOURCE_DIR}/src/qtapp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/qtapp/gguf
        ${CMAKE_CURRENT_SOURCE_DIR}/src/qtapp/ui
    )

    target_link_libraries(RawrXD-Shell
        PRIVATE agent_framework
        PRIVATE ultra_fast_inference_core
        PRIVATE ollama_blob_parser
        PRIVATE polymorphic_loader
    )

    if(USE_WIN32)
        target_link_libraries(RawrXD-Shell
            PRIVATE win32_agent_tools
            PRIVATE ws2_32       # Winsock2 for health_check_server
            PRIVATE wininet      # WinINet for HTTP
            PRIVATE kernel32
            PRIVATE user32
            PRIVATE advapi32
            PRIVATE psapi
            PRIVATE shlwapi
        )
    endif()

    if(GGML_LIBRARY)
        target_link_libraries(RawrXD-Shell PRIVATE ${GGML_LIBRARY})
    endif()

    if(USE_GPU AND Vulkan_FOUND)
        target_link_libraries(RawrXD-Shell PRIVATE Vulkan::Vulkan)
    endif()

    if(MSVC)
        target_compile_options(RawrXD-Shell PRIVATE /O2 /arch:AVX2)
    endif()
    set_property(TARGET RawrXD-Shell PROPERTY FOLDER "Applications")

    message(STATUS "✓ RawrXD-Shell application configured")
endif()

# ============================================================================
# Test Executables
# ============================================================================

if(BUILD_TESTS)
    # Test: Agentic Tools
    add_executable(test_agentic_tools src/test_agentic_tools.cpp)
    target_link_libraries(test_agentic_tools PRIVATE agent_framework)
    if(USE_WIN32)
        target_link_libraries(test_agentic_tools PRIVATE win32_agent_tools kernel32 user32 advapi32 psapi shlwapi)
    endif()
    set_property(TARGET test_agentic_tools PROPERTY FOLDER "Test Executables")

    # Test: Agent Hot Patcher
    add_executable(test_agent_hot_patcher src/test_agent_hot_patcher.cpp)
    target_link_libraries(test_agent_hot_patcher PRIVATE agent_framework)
    if(USE_WIN32)
        target_link_libraries(test_agent_hot_patcher PRIVATE kernel32)
    endif()
    set_property(TARGET test_agent_hot_patcher PROPERTY FOLDER "Test Executables")

    # Test: Agent Hot Patcher Integration
    add_executable(test_agent_hot_patcher_integration src/test_agent_hot_patcher_integration.cpp)
    target_link_libraries(test_agent_hot_patcher_integration PRIVATE agent_framework)
    if(USE_WIN32)
        target_link_libraries(test_agent_hot_patcher_integration PRIVATE kernel32)
    endif()
    set_property(TARGET test_agent_hot_patcher_integration PROPERTY FOLDER "Test Executables")

    # Test: Agent Hot Patcher QTest (converted)
    add_executable(test_agent_hot_patcher_qtest src/test_agent_hot_patcher_qtest.cpp)
    target_link_libraries(test_agent_hot_patcher_qtest PRIVATE agent_framework)
    if(USE_WIN32)
        target_link_libraries(test_agent_hot_patcher_qtest PRIVATE kernel32)
    endif()
    set_property(TARGET test_agent_hot_patcher_qtest PROPERTY FOLDER "Test Executables")

    # Test: Model Interface
    add_executable(test_model_interface src/test_model_interface.cpp)
    target_link_libraries(test_model_interface PRIVATE agent_framework)
    if(USE_WIN32)
        target_link_libraries(test_model_interface PRIVATE kernel32 psapi)
    endif()
    set_property(TARGET test_model_interface PROPERTY FOLDER "Test Executables")

    # Test: Model Trainer Validation
    add_executable(test_model_trainer_validation src/test_model_trainer_validation.cpp)
    target_link_libraries(test_model_trainer_validation PRIVATE agent_framework)
    if(USE_WIN32)
        target_link_libraries(test_model_trainer_validation PRIVATE kernel32)
    endif()
    set_property(TARGET test_model_trainer_validation PROPERTY FOLDER "Test Executables")

    message(STATUS "✓ Test executables configured (6 test suites)")
endif()

# ============================================================================
# Installation Configuration
# ============================================================================

install(TARGETS ultra_fast_inference_core
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

if(USE_WIN32)
    install(TARGETS win32_agent_tools
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
    )
endif()

install(TARGETS ollama_blob_parser
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(TARGETS polymorphic_loader
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(TARGETS agent_framework
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

if(BUILD_QTAPP)
    install(TARGETS RawrXD-Shell
        RUNTIME DESTINATION bin
    )
endif()

# ============================================================================
# Build Summary
# ============================================================================

message(STATUS "")
message(STATUS "╔════════════════════════════════════════════════════════════╗")
message(STATUS "║           RawrXD Build Configuration                       ║")
message(STATUS "╠════════════════════════════════════════════════════════════╣")
message(STATUS "║ Configuration:                                              ║")
message(STATUS "║   GPU Support (Vulkan):       ${USE_GPU}")
message(STATUS "║   Win32 Agent Tools:          ${USE_WIN32}")
message(STATUS "║   Time-Travel Execution:      ${ENABLE_TIME_TRAVEL}")
message(STATUS "║   Build Tests:                ${BUILD_TESTS}")
message(STATUS "║   Build RawrXD-Shell:         ${BUILD_QTAPP}")
message(STATUS "║                                                             ║")
message(STATUS "║ Libraries:                                                  ║")
message(STATUS "║   ✓ ultra_fast_inference_core (tensor ops, hotpatching)   ║")
message(STATUS "║   ✓ win32_agent_tools         (autonomous Win32 bridge)   ║")
message(STATUS "║   ✓ ollama_blob_parser        (GGUF blob extraction)      ║")
message(STATUS "║   ✓ polymorphic_loader        (70B-700B+ streaming)       ║")
message(STATUS "║   ✓ agent_framework           (agent + model interface)   ║")
message(STATUS "║   ✓ RawrXD-Shell              (headless app + server)     ║")
message(STATUS "║                                                             ║")
message(STATUS "║ Build Instructions:                                         ║")
message(STATUS "║   cmake -G \"Visual Studio 17 2022\" -DUSE_GPU=ON -B build  ║")
message(STATUS "║   cmake --build build --config Release -j 8               ║")
message(STATUS "║                                                             ║")
message(STATUS "╚════════════════════════════════════════════════════════════╝")
message(STATUS "")
