# =============================================================================
# WebView2Container CMake Configuration
# =============================================================================

# Minimum CMake version
cmake_minimum_required(VERSION 3.16)

# Find WebView2 (optional - only needed for WebView2Container)
find_package(Microsoft.Web.WebView2 QUIET)

# WebView2Container library (only if WebView2 is found)
if(Microsoft.Web.WebView2_FOUND)
    set(WEBVIEW2_SOURCES
        WebView2Container_stubs.cpp
        WebView2Container.h
    )

    # Create the library
    add_library(WebView2Container STATIC ${WEBVIEW2_SOURCES})

    # Set C++17 standard (required for WebView2)
    target_compile_features(WebView2Container PUBLIC cxx_std_17)

    # Include directories
    target_include_directories(WebView2Container PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    # Link WebView2 and system libraries
    target_link_libraries(WebView2Container PUBLIC
        Microsoft.Web.WebView2::WebView2
        ole32
        oleaut32
        user32
        version
    )

    # Compiler definitions
    target_compile_definitions(WebView2Container PUBLIC
        WIN32_LEAN_AND_MEAN
        UNICODE
        _UNICODE
    )

    # For Visual Studio, set additional properties
    if(MSVC)
        target_compile_options(WebView2Container PRIVATE /W4)
        set_target_properties(WebView2Container PROPERTIES
            VS_GLOBAL_WindowsTargetPlatformVersion "10.0"
        )
    endif()
else()
    message(STATUS "[Core] WebView2 not found - WebView2Container disabled")
endif()

# =============================================================================
# Hotpatch Playback System CMake Configuration
# Part of RawrXD Progressive Layer Loading System
# =============================================================================

# Options
option(HOTPATCH_PLAYBACK_BUILD_TESTS "Build test suite" ON)
option(HOTPATCH_PLAYBACK_BUILD_DEMO "Build demo programs" ON)
option(HOTPATCH_PLAYBACK_BUILD_INTEGRATION "Build integration layer" ON)

# Source files
set(HOTPATCH_PLAYBACK_HEADERS
    hotpatch_playback.h
)

set(HOTPATCH_PLAYBACK_SOURCES
    hotpatch_playback.c
)

set(HOTPATCH_PLAYBACK_INTEGRATION_HEADERS
    hotpatch_playback_integration.h
)

set(HOTPATCH_PLAYBACK_INTEGRATION_SOURCES
    hotpatch_playback_integration.c
)

set(HOTPATCH_PLAYBACK_TEST_SOURCES
    hotpatch_playback_test.c
)

# Core playback library
add_library(hotpatch_playback STATIC
    ${HOTPATCH_PLAYBACK_HEADERS}
    ${HOTPATCH_PLAYBACK_SOURCES}
)

target_include_directories(hotpatch_playback PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(hotpatch_playback PRIVATE kernel32 user32)
else()
    target_link_libraries(hotpatch_playback PRIVATE m rt pthread)
endif()

# Integration library
if(HOTPATCH_PLAYBACK_BUILD_INTEGRATION)
    add_library(hotpatch_playback_integration STATIC
        ${HOTPATCH_PLAYBACK_INTEGRATION_HEADERS}
        ${HOTPATCH_PLAYBACK_INTEGRATION_SOURCES}
    )
    
    target_include_directories(hotpatch_playback_integration PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    target_link_libraries(hotpatch_playback_integration PUBLIC hotpatch_playback)
endif()

# Test executable
if(HOTPATCH_PLAYBACK_BUILD_TESTS)
    add_executable(hotpatch_playback_test ${HOTPATCH_PLAYBACK_TEST_SOURCES})
    target_link_libraries(hotpatch_playback_test PRIVATE hotpatch_playback)
    
    if(HOTPATCH_PLAYBACK_BUILD_INTEGRATION)
        target_link_libraries(hotpatch_playback_test PRIVATE hotpatch_playback_integration)
    endif()
    
    enable_testing()
    add_test(NAME HotpatchPlaybackTest COMMAND hotpatch_playback_test)
endif()

# Demo executables
if(HOTPATCH_PLAYBACK_BUILD_DEMO)
    add_executable(hotpatch_playback_demo hotpatch_playback.c)
    target_compile_definitions(hotpatch_playback_demo PRIVATE HOTPATCH_PLAYBACK_DEMO)
    target_link_libraries(hotpatch_playback_demo PRIVATE hotpatch_playback)
    
    if(HOTPATCH_PLAYBACK_BUILD_INTEGRATION)
        add_executable(hotpatch_playback_integration_demo hotpatch_playback_integration.c)
        target_compile_definitions(hotpatch_playback_integration_demo PRIVATE HOTPATCH_PLAYBACK_DEMO)
        target_link_libraries(hotpatch_playback_integration_demo PRIVATE hotpatch_playback_integration)
    endif()
endif()

# =============================================================================
# Inference Playback Bridge - Real Inference Hook
# =============================================================================

# MASM x64 GGUF Loader (Windows only)
if(WIN32)
    enable_language(ASM_MASM)
    
    set(INFERENCE_GGUF_LOADER_ASM
        inference_gguf_loader.asm
    )
    
    add_library(inference_gguf_loader STATIC
        ${INFERENCE_GGUF_LOADER_ASM}
    )
    
    # MASM compilation flags
    if(MSVC)
        target_compile_options(inference_gguf_loader PRIVATE
            /Zi
            /W3
        )
    endif()
else()
    # POSIX fallback - empty library, C implementation handles it
    add_library(inference_gguf_loader INTERFACE)
endif()

# Inference Bridge headers and sources
set(INFERENCE_PLAYBACK_BRIDGE_HEADERS
    inference_playback_bridge.h
)

set(INFERENCE_PLAYBACK_BRIDGE_SOURCES
    inference_playback_bridge.c
)

set(INFERENCE_PLAYBACK_BRIDGE_TEST_SOURCES
    inference_playback_bridge_test.c
)

# Inference Bridge library
add_library(inference_playback_bridge STATIC
    ${INFERENCE_PLAYBACK_BRIDGE_HEADERS}
    ${INFERENCE_PLAYBACK_BRIDGE_SOURCES}
)

target_include_directories(inference_playback_bridge PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(inference_playback_bridge PUBLIC
    hotpatch_playback
    inference_gguf_loader
)

# Platform-specific libraries for inference bridge
if(WIN32)
    target_link_libraries(inference_playback_bridge PRIVATE
        kernel32
        user32
    )
else()
    target_link_libraries(inference_playback_bridge PRIVATE
        m
        rt
        pthread
    )
endif()

# Inference Bridge test executable
if(HOTPATCH_PLAYBACK_BUILD_TESTS)
    add_executable(inference_playback_bridge_test
        ${INFERENCE_PLAYBACK_BRIDGE_TEST_SOURCES}
    )
    
    target_link_libraries(inference_playback_bridge_test PRIVATE
        inference_playback_bridge
        inference_gguf_loader
    )
    
    add_test(NAME InferencePlaybackBridgeTest COMMAND inference_playback_bridge_test)
endif()

# Inference Bridge demo
if(HOTPATCH_PLAYBACK_BUILD_DEMO)
    add_executable(inference_playback_bridge_demo
        inference_playback_bridge.c
    )
    target_compile_definitions(inference_playback_bridge_demo PRIVATE
        INFERENCE_PLAYBACK_BRIDGE_DEMO
    )
    target_link_libraries(inference_playback_bridge_demo PRIVATE
        inference_playback_bridge
        inference_gguf_loader
    )
endif()

# Unified Inference Engine - P0 Fix: Agentic SubmitInference BackendError
# Same backend for chat AND agent inference
# =============================================================================

set(UNIFIED_INFERENCE_ENGINE_HEADERS
    unified_inference_engine.h
)

set(UNIFIED_INFERENCE_ENGINE_SOURCES
    unified_inference_engine.c
)

set(UNIFIED_INFERENCE_ENGINE_TEST_SOURCES
    unified_inference_engine_test.c
)

# Unified Inference Engine library
add_library(unified_inference_engine STATIC
    ${UNIFIED_INFERENCE_ENGINE_HEADERS}
    ${UNIFIED_INFERENCE_ENGINE_SOURCES}
)

target_include_directories(unified_inference_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(unified_inference_engine PUBLIC
    inference_playback_bridge
    hotpatch_playback
)

# Platform-specific libraries for unified engine
if(WIN32)
    target_link_libraries(unified_inference_engine PRIVATE
        kernel32
        user32
        shell32
    )
else()
    target_link_libraries(unified_inference_engine PRIVATE
        m
        rt
        pthread
    )
endif()

# Unified Engine test executable
if(HOTPATCH_PLAYBACK_BUILD_TESTS)
    add_executable(unified_inference_engine_test
        ${UNIFIED_INFERENCE_ENGINE_TEST_SOURCES}
    )
    
    target_link_libraries(unified_inference_engine_test PRIVATE
        unified_inference_engine
        inference_playback_bridge
        inference_gguf_loader
    )
    
    add_test(NAME UnifiedInferenceEngineTest COMMAND unified_inference_engine_test)
endif()

# Unified Engine demo
if(HOTPATCH_PLAYBACK_BUILD_DEMO)
    add_executable(unified_inference_engine_demo
        unified_inference_engine.c
    )
    target_compile_definitions(unified_inference_engine_demo PRIVATE
        UNIFIED_INFERENCE_ENGINE_DEMO
    )
    target_link_libraries(unified_inference_engine_demo PRIVATE
        unified_inference_engine
        inference_playback_bridge
        inference_gguf_loader
    )
endif()

# Quantization & Hotswap Library
# =============================================================================

set(RAWRXD_QUANT_HEADERS
    quant_ops.h
    weight_hotswap.h
    gguf_format.h
)

set(RAWRXD_QUANT_SOURCES
    quant_ops.c
    weight_hotswap.c
)

add_library(rawrxd_quant STATIC
    ${RAWRXD_QUANT_HEADERS}
    ${RAWRXD_QUANT_SOURCES}
)

target_include_directories(rawrxd_quant PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(rawrxd_quant PRIVATE
        kernel32
        user32
    )
else()
    target_link_libraries(rawrxd_quant PRIVATE
        m
        rt
        pthread
    )
endif()

# Link against inference_gguf_loader for GGUF functions
target_link_libraries(rawrxd_quant PUBLIC inference_gguf_loader)

set_target_properties(rawrxd_quant PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# Quantization test executable
if(HOTPATCH_PLAYBACK_BUILD_TESTS)
    add_executable(quant_test quant_test.c)
    target_link_libraries(quant_test PRIVATE rawrxd_quant inference_gguf_loader)
    set_target_properties(quant_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    add_test(NAME QuantTest COMMAND quant_test)
endif()

# Hotswap demo executable
if(HOTPATCH_PLAYBACK_BUILD_DEMO)
    add_executable(hotswap_demo hotswap_demo.c)
    target_link_libraries(hotswap_demo PRIVATE rawrxd_quant inference_gguf_loader)
    set_target_properties(hotswap_demo PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endif()

# Installation
install(TARGETS rawrxd_quant
    ARCHIVE DESTINATION lib
)

install(FILES ${RAWRXD_QUANT_HEADERS}
    DESTINATION include/rawrxd/core
)

# Installation
install(TARGETS hotpatch_playback inference_playback_bridge inference_gguf_loader unified_inference_engine
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${HOTPATCH_PLAYBACK_HEADERS} ${INFERENCE_PLAYBACK_BRIDGE_HEADERS} ${UNIFIED_INFERENCE_ENGINE_HEADERS}
    DESTINATION include/rawrxd/core
)

if(HOTPATCH_PLAYBACK_BUILD_INTEGRATION)
    install(TARGETS hotpatch_playback_integration
        ARCHIVE DESTINATION lib
    )
    
    install(FILES ${HOTPATCH_PLAYBACK_INTEGRATION_HEADERS}
        DESTINATION include/rawrxd/core
    )
endif()

# =============================================================================
# Native IDE Tools Library
# 100+ Tools for IDE Integration — Zero External Dependencies
# Fixes P0 Blocker: 2GB+ File I/O via ChunkedIO
# NOTE: Files are in parent src/ directory, not src/core/
# =============================================================================

set(NATIVE_IDE_TOOLS_HEADERS
    ${CMAKE_SOURCE_DIR}/src/native_ide_tools.h
)

set(NATIVE_IDE_TOOLS_SOURCES
    ${CMAKE_SOURCE_DIR}/src/native_ide_tools.cpp
    ${CMAKE_SOURCE_DIR}/src/native_ide_tools_extended.cpp
)

add_library(native_ide_tools STATIC
    ${NATIVE_IDE_TOOLS_HEADERS}
    ${NATIVE_IDE_TOOLS_SOURCES}
)

target_compile_features(native_ide_tools PUBLIC cxx_std_17)

target_include_directories(native_ide_tools PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(native_ide_tools PRIVATE
        kernel32
        user32
        shell32
        ole32
        oleaut32
    )
    
    target_compile_definitions(native_ide_tools PUBLIC
        WIN32_LEAN_AND_MEAN
        UNICODE
        _UNICODE
        _CRT_SECURE_NO_WARNINGS
    )
else()
    target_link_libraries(native_ide_tools PRIVATE
        pthread
        dl
        rt
    )
    
    target_compile_definitions(native_ide_tools PUBLIC
        _GNU_SOURCE
        _XOPEN_SOURCE=600
    )
endif()

# Compiler-specific options
if(MSVC)
    target_compile_options(native_ide_tools PRIVATE
        /W4
        /MP
        /EHsc
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(native_ide_tools PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-function
        -fPIC
    )
endif()

# Link against hotpatch_playback for model analysis
target_link_libraries(native_ide_tools PUBLIC
    hotpatch_playback
)

# Optional: Link against inference components if available
if(TARGET inference_playback_bridge)
    target_link_libraries(native_ide_tools PUBLIC
        inference_playback_bridge
    )
    target_compile_definitions(native_ide_tools PUBLIC
        RAWRXD_INFERENCE_BRIDGE_ENABLED
    )
endif()

# Test executable
if(HOTPATCH_PLAYBACK_BUILD_TESTS)
    add_executable(native_ide_tools_test
        native_ide_tools_test.cpp
    )
    
    target_link_libraries(native_ide_tools_test PRIVATE
        native_ide_tools
    )
    
    add_test(NAME NativeIDEToolsTest COMMAND native_ide_tools_test)
endif()

# Demo executable
if(HOTPATCH_PLAYBACK_BUILD_DEMO)
    add_executable(native_ide_tools_demo
        native_ide_tools_demo.cpp
    )
    
    target_link_libraries(native_ide_tools_demo PRIVATE
        native_ide_tools
    )
    
    target_compile_definitions(native_ide_tools_demo PRIVATE
        NATIVE_IDE_TOOLS_DEMO
    )
endif()

# Installation
install(TARGETS native_ide_tools
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${NATIVE_IDE_TOOLS_HEADERS}
    DESTINATION include/rawrxd/core
)

# =============================================================================
# Model Loader Chunked Bridge — Fixes P0 Blocker for >2GB GGUF Files
# =============================================================================

set(MODEL_LOADER_CHUNKED_BRIDGE_SOURCES
    model_loader_chunked_bridge.cpp
)

add_library(model_loader_chunked_bridge STATIC
    ${MODEL_LOADER_CHUNKED_BRIDGE_SOURCES}
)

target_compile_features(model_loader_chunked_bridge PUBLIC cxx_std_17)

target_include_directories(model_loader_chunked_bridge PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/inference
)

target_link_libraries(model_loader_chunked_bridge PUBLIC
    native_ide_tools
)

# Installation
install(TARGETS model_loader_chunked_bridge
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

# =============================================================================
# RawrXD Weight Re-quantizer & Hot-Swap (rawrxd.h)
# Minimal complete implementation for quantization experiments
# =============================================================================

# Header-only library - just needs to be included
add_library(rawrxd INTERFACE)
target_include_directories(rawrxd INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Test executable
if(HOTPATCH_PLAYBACK_BUILD_TESTS)
    add_executable(rawrxd_test
        rawrxd_test.c
    )
    
    target_link_libraries(rawrxd_test PRIVATE
        rawrxd
        m
    )
    
    if(WIN32)
        target_link_libraries(rawrxd_test PRIVATE
            kernel32
        )
    endif()
    
    add_test(NAME RawrXDTest COMMAND rawrxd_test)
endif()

# Demo executable
if(HOTPATCH_PLAYBACK_BUILD_DEMO)
    add_executable(rawrxd_demo
        rawrxd_demo.c
    )
    
    target_link_libraries(rawrxd_demo PRIVATE
        rawrxd
        m
    )
    
    if(WIN32)
        target_link_libraries(rawrxd_demo PRIVATE
            kernel32
        )
    endif()
endif()

# Installation
install(FILES rawrxd.h
    DESTINATION include/rawrxd/core
)

# =============================================================================
# RAWRXD_CORE - Production Inference Engine
# Single file, <3k lines, no stubs, real code.
# Includes: GGUF, Quantization, Hotswap, Lockpick, Inference, Tools, LSP
# =============================================================================

# Header-only library for rawrxd_core
add_library(rawrxd_core INTERFACE)
target_include_directories(rawrxd_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# C11 standard required
target_compile_features(rawrxd_core INTERFACE c_std_11)

# Test executable for rawrxd_core
if(HOTPATCH_PLAYBACK_BUILD_TESTS)
    add_executable(rawrxd_core_test
        rawrxd_core_test.c
    )
    
    target_link_libraries(rawrxd_core_test PRIVATE
        rawrxd_core
        m
    )
    
    if(WIN32)
        target_link_libraries(rawrxd_core_test PRIVATE
            kernel32
        )
    else()
        target_link_libraries(rawrxd_core_test PRIVATE
            pthread
            rt
        )
    endif()
    
    add_test(NAME RawrXDCoreTest COMMAND rawrxd_core_test)
endif()

# Demo executable for rawrxd_core
if(HOTPATCH_PLAYBACK_BUILD_DEMO)
    add_executable(rawrxd_core_demo
        rawrxd_core_demo.c
    )
    
    target_link_libraries(rawrxd_core_demo PRIVATE
        rawrxd_core
        m
    )
    
    if(WIN32)
        target_link_libraries(rawrxd_core_demo PRIVATE
            kernel32
        )
    else()
        target_link_libraries(rawrxd_core_demo PRIVATE
            pthread
            rt
        )
    endif()
endif()

# Installation
install(FILES rawrxd_core.h
    DESTINATION include/rawrxd/core
)

# =============================================================================
# RAWRXD_SWARM - Multi-Agent Parallel Execution System
# Cursor-style 8-agent architecture with isolated worktrees
# =============================================================================

# Header-only library for rawrxd_swarm
add_library(rawrxd_swarm INTERFACE)
target_include_directories(rawrxd_swarm INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# C11 standard required
target_compile_features(rawrxd_swarm INTERFACE c_std_11)

# Test executable for rawrxd_swarm
if(HOTPATCH_PLAYBACK_BUILD_TESTS)
    add_executable(rawrxd_swarm_test
        rawrxd_swarm_test.c
    )
    
    target_link_libraries(rawrxd_swarm_test PRIVATE
        rawrxd_swarm
        rawrxd_core
    )
    
    if(WIN32)
        target_link_libraries(rawrxd_swarm_test PRIVATE
            kernel32
        )
    else()
        target_link_libraries(rawrxd_swarm_test PRIVATE
            m
            pthread
            rt
        )
    endif()
    
    add_test(NAME RawrXDSwarmTest COMMAND rawrxd_swarm_test)
endif()

# Demo executable for rawrxd_swarm
if(HOTPATCH_PLAYBACK_BUILD_DEMO)
    add_executable(rawrxd_swarm_demo
        rawrxd_swarm_demo.c
    )
    
    target_link_libraries(rawrxd_swarm_demo PRIVATE
        rawrxd_swarm
        rawrxd_core
    )
    
    if(WIN32)
        target_link_libraries(rawrxd_swarm_demo PRIVATE
            kernel32
        )
    else()
        target_link_libraries(rawrxd_swarm_demo PRIVATE
            m
            pthread
            rt
        )
    endif()
endif()

# Installation
install(FILES rawrxd_swarm.h
    DESTINATION include/rawrxd/core
)

# =============================================================================
# Example usage target (optional)
# =============================================================================
if(WEBVIEW2_BUILD_EXAMPLE)
    add_executable(WebView2Example example_usage.cpp)
    target_link_libraries(WebView2Example PRIVATE WebView2Container)
endif()

# =============================================================================
# Installation (optional)
# =============================================================================
if(WEBVIEW2_INSTALL)
    install(TARGETS WebView2Container
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
    )
    
    install(FILES WebView2Container.h
        DESTINATION include
    )
endif()

# =============================================================================
# Package configuration
# =============================================================================
include(CMakePackageConfigHelpers)

# Create version file
write_basic_package_version_file(
    WebView2ContainerConfigVersion.cmake
    VERSION 1.0.0
    COMPATIBILITY AnyNewerVersion
)

# Export targets (only if WebView2Container is built)
if(Microsoft.Web.WebView2_FOUND AND TARGET WebView2Container)
    export(TARGETS WebView2Container
        FILE WebView2ContainerTargets.cmake
        NAMESPACE WebView2Container::
    )
endif()