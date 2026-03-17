# CMakeLists.txt for Pure MASM x64 Hotpatch System
# ZERO C/C++ DEPENDENCIES - Pure Assembly Only
# Links directly to kernel32.lib via ml64/link

cmake_minimum_required(VERSION 3.20)

# Check if MASM integration is enabled
if(NOT ENABLE_MASM_INTEGRATION)
    message(STATUS "MASM integration disabled - skipping MASM hotpatch system")
    return()
endif()

# Project is ASM_MASM ONLY - no C, no CXX
project(RawrXD_MASM_Hotpatch ASM_MASM)

# Enable MASM for x64
enable_language(ASM_MASM)

# Set compiler flags for MASM
# /I flag adds include directory path so ml64.exe can find .inc files
set(CMAKE_ASM_MASM_FLAGS "/nologo /Zi /c /Cp /W3 /I\"${CMAKE_CURRENT_SOURCE_DIR}/final-ide\"")

# Platform check - Windows only
if(NOT WIN32)
    message(FATAL_ERROR "MASM x64 hotpatch system requires Windows with MSVC toolchain")
endif()

# Verify x64 architecture
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "64-bit build required for MASM x64 implementation")
endif()

# If building C++ AVX2 kernels, ensure compilation enables AVX2.
# IMPORTANT: This subproject is primarily ASM_MASM; do not pass /arch:AVX2 to ml64.exe.
if(MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/arch:AVX2>)
    add_compile_definitions($<$<COMPILE_LANGUAGE:C,CXX>:__AVX2__=1>)
else()
    add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:-mavx2> $<$<COMPILE_LANGUAGE:C,CXX>:-march=native>)
endif()

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ==== MASM Runtime Foundation Library ====
set(MASM_RUNTIME_SOURCES
    asm_memory.asm
    asm_sync.asm
    asm_string.asm
    asm_events.asm
    asm_log.asm
    final-ide\\malloc_wrapper.asm
    final-ide\\masm_gpu_backend.asm
    final-ide\\cstring_wrappers.asm
    final-ide\\rawrxd_dualengine_support.asm
)

# Ensure Visual Studio treats these as MASM sources (not <None>)
set_source_files_properties(${MASM_RUNTIME_SOURCES} PROPERTIES LANGUAGE ASM_MASM)

# ==== Qt6 Foundation Layer (NEW) ====
# Pure MASM implementation of Qt6 foundation abstractions
# Provides: object model (VMT), memory pools, event queue, signal/slot binding
set(MASM_QT6_FOUNDATION_SOURCES
    final-ide\\qt6_foundation.asm
    final-ide\\qt6_main_window.asm
    final-ide\\qt6_text_editor.asm
    final-ide\\qt6_syntax_highlighter.asm
    final-ide\\qt6_statusbar.asm
)

# Ensure Visual Studio treats these as MASM sources (not <None>)
set_source_files_properties(${MASM_QT6_FOUNDATION_SOURCES} PROPERTIES LANGUAGE ASM_MASM)


# Check qt6_foundation.asm exists
foreach(src ${MASM_QT6_FOUNDATION_SOURCES})
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
        message(WARNING "Qt6 foundation source missing: ${src}")
    else()
        message(STATUS "Qt6 foundation found: ${src}")
    endif()
endforeach()

# Create object library (no linker involved yet)
add_library(masm_runtime_obj OBJECT ${MASM_RUNTIME_SOURCES})
add_library(masm_qt6_foundation_obj OBJECT ${MASM_QT6_FOUNDATION_SOURCES})

# ==== MASM Agentic System ====

# Create static library from objects
add_library(masm_runtime STATIC $<TARGET_OBJECTS:masm_runtime_obj>)
set_target_properties(masm_runtime PROPERTIES
    OUTPUT_NAME "masm_runtime"
)

# Qt6 Foundation Library
add_library(masm_qt6_foundation STATIC $<TARGET_OBJECTS:masm_qt6_foundation_obj>)
set_target_properties(masm_qt6_foundation PROPERTIES
    OUTPUT_NAME "masm_qt6_foundation"
)

# ==== MASM Hotpatch Core Library ====
set(MASM_HOTPATCH_SOURCES
    model_memory_hotpatch.asm
    RawrXD_DualEngineStreamer.asm
    RawrXD_RuntimePatcher.asm
    RawrXD_DualEngineManager.asm
    RawrXD_AgenticPatchOrchestrator.asm
    RawrXD_MathHotpatchEntry.asm
    RawrXD_ModelTrainer_PreParserSniper.asm
    interconnect/RawrXD_ModelTrainingEngine.asm
    final-ide\\masm_core_direct_io.asm
    final-ide\\masm_core_reversible_transforms.asm
    final-ide\\byte_level_hotpatcher_refactored.asm
    gguf_server_hotpatch.asm
    unified_hotpatch_manager.asm
    final-ide\\stub_completion_comprehensive_v2.asm
    final-ide\\stub_integration_bridges.asm
)

# Check which files exist and only include existing ones
foreach(src ${MASM_HOTPATCH_SOURCES})
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
        message(WARNING "MASM source file missing: ${src}")
        list(REMOVE_ITEM MASM_HOTPATCH_SOURCES ${src})
    endif()
endforeach()

if(MASM_HOTPATCH_SOURCES)
    set_source_files_properties(${MASM_HOTPATCH_SOURCES} PROPERTIES LANGUAGE ASM_MASM)
endif()

add_library(masm_hotpatch_core_obj OBJECT ${MASM_HOTPATCH_SOURCES})
add_library(masm_hotpatch_core STATIC $<TARGET_OBJECTS:masm_hotpatch_core_obj>)
set_target_properties(masm_hotpatch_core PROPERTIES
    OUTPUT_NAME "masm_hotpatch_core"
)

# ==== MASM Agentic Systems Library ====
# These will be stubs until implemented
set(MASM_AGENTIC_SOURCES)
foreach(src
    proxy_hotpatcher.asm
    agentic_failure_detector.asm
    agentic_puppeteer.asm
    final-ide\\agentic_puppeteer.asm
    final-ide\\lsp_client_fixed.asm
    final-ide\\agent_auto_bootstrap.asm
    final-ide\\masm_inference_engine.asm)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
        list(APPEND MASM_AGENTIC_SOURCES ${src})
    endif()
endforeach()

# Prefer top-level wrapper sources when available to prevent MSVC/MSBuild from
# synthesizing ObjectFileName values with forward-slash subpaths that break ml64
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/wrappers/wrap_agentic_puppeteer.asm")
    list(REMOVE_ITEM MASM_AGENTIC_SOURCES final-ide\\agentic_puppeteer.asm agentic_puppeteer.asm)
    list(APPEND MASM_AGENTIC_SOURCES wrappers\\wrap_agentic_puppeteer.asm)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/wrappers/wrap_lsp_client_fixed.asm")
    list(REMOVE_ITEM MASM_AGENTIC_SOURCES final-ide\\lsp_client_fixed.asm lsp_client_fixed.asm)
    list(APPEND MASM_AGENTIC_SOURCES wrappers\\wrap_lsp_client_fixed.asm)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/wrappers/wrap_agent_auto_bootstrap.asm")
    list(REMOVE_ITEM MASM_AGENTIC_SOURCES final-ide\\agent_auto_bootstrap.asm agent_auto_bootstrap.asm)
    list(APPEND MASM_AGENTIC_SOURCES wrappers\\wrap_agent_auto_bootstrap.asm)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/wrappers/wrap_masm_inference_engine.asm")
    list(REMOVE_ITEM MASM_AGENTIC_SOURCES final-ide\\masm_inference_engine.asm masm_inference_engine.asm)
    list(APPEND MASM_AGENTIC_SOURCES wrappers\\wrap_masm_inference_engine.asm)
endif()

if(MASM_AGENTIC_SOURCES)
    set_source_files_properties(${MASM_AGENTIC_SOURCES} PROPERTIES LANGUAGE ASM_MASM)
    add_library(masm_agentic_obj OBJECT ${MASM_AGENTIC_SOURCES})
    add_library(masm_agentic STATIC $<TARGET_OBJECTS:masm_agentic_obj>)
    set_target_properties(masm_agentic PROPERTIES
        OUTPUT_NAME "masm_agentic"
    )
endif()

# ==== MASM AI Orchestration System (NEW) ====
# All orchestration subsystems now permanently included - no feature gating
set(MASM_ORCHESTRATION_SOURCES
    final-ide\\masm_orchestration_wrapper.asm
    simple_orchestration.asm
    ai_orchestration_coordinator.asm
    agentic_inference_stream.asm
    autonomous_task_executor.asm
    final-ide\\autonomous_task_executor_clean.asm
    final-ide\\ai_orchestration_glue.asm
    final-ide\\agent_orchestrator_main.asm
    agentic_failure_recovery.asm
)

# Check which files exist and only include existing ones
foreach(src ${MASM_ORCHESTRATION_SOURCES})
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
        message(WARNING "MASM source file missing: ${src}")
        list(REMOVE_ITEM MASM_ORCHESTRATION_SOURCES ${src})
    endif()
endforeach()

if(MASM_ORCHESTRATION_SOURCES)
    set_source_files_properties(${MASM_ORCHESTRATION_SOURCES} PROPERTIES LANGUAGE ASM_MASM)
endif()

add_library(masm_orchestration_obj OBJECT ${MASM_ORCHESTRATION_SOURCES})
add_library(masm_orchestration STATIC $<TARGET_OBJECTS:masm_orchestration_obj>)
set_target_properties(masm_orchestration PROPERTIES
    OUTPUT_NAME "masm_orchestration"
)

# ==== MASM UI Systems Library (NEW) ====
# Output pane logging, tab management, file tree, agent chat, menu handlers, layout persistence
set(MASM_UI_SOURCES)
foreach(src output_pane_logger.asm tab_manager.asm file_tree_driver.asm agent_chat_modes.asm menu_handlers.asm layout_persistence.asm)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
        list(APPEND MASM_UI_SOURCES ${src})
    endif()
endforeach()

if(MASM_UI_SOURCES)
    set_source_files_properties(${MASM_UI_SOURCES} PROPERTIES LANGUAGE ASM_MASM)
    add_library(masm_ui_obj OBJECT ${MASM_UI_SOURCES})
    add_library(masm_ui STATIC $<TARGET_OBJECTS:masm_ui_obj>)
    set_target_properties(masm_ui PROPERTIES
        OUTPUT_NAME "masm_ui"
    )
endif()

# ==== Unified MASM Hotpatch Library (All-in-One) ====
set(MASM_UNIFIED_SOURCES ${MASM_RUNTIME_SOURCES})
list(APPEND MASM_UNIFIED_SOURCES ${MASM_QT6_FOUNDATION_SOURCES})
list(APPEND MASM_UNIFIED_SOURCES ${MASM_HOTPATCH_SOURCES})
if(MASM_AGENTIC_SOURCES)
    list(APPEND MASM_UNIFIED_SOURCES ${MASM_AGENTIC_SOURCES})
endif()

add_library(masm_hotpatch_unified_obj OBJECT ${MASM_UNIFIED_SOURCES})
add_library(masm_hotpatch_unified STATIC $<TARGET_OBJECTS:masm_hotpatch_unified_obj>)
set_target_properties(masm_hotpatch_unified PROPERTIES
    OUTPUT_NAME "masm_hotpatch_unified"
)

# Normalize MASM object file names for MSVC/masm to avoid
# creating ObjectFileName with forward-slashes (e.g. "$(IntDir)/final-ide/..."),
# which can be interpreted incorrectly by ml64.exe. Flatten names for files
# in subdirectories to simple filenames to avoid introducing '/'.
foreach(_src IN LISTS MASM_RUNTIME_SOURCES MASM_QT6_FOUNDATION_SOURCES MASM_HOTPATCH_SOURCES MASM_AGENTIC_SOURCES MASM_ORCHESTRATION_SOURCES MASM_UI_SOURCES)
    if(_src MATCHES "/")
        # Use just the filename as object name
        get_filename_component(_fname "${_src}" NAME)
        set_source_files_properties("${_src}" PROPERTIES OBJECT_FILE_NAME "${_fname}.obj")
    endif()
endforeach()

# ==== Pure MASM Test Harness (NO C/C++ CODE) ====
option(BUILD_MASM_TESTS "Build pure MASM hotpatch tests" ON)

if(BUILD_MASM_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/masm_test_main.asm")
    set_source_files_properties(masm_test_main.asm PROPERTIES LANGUAGE ASM_MASM)
    # Pure MASM x64 test harness - ZERO C/C++ dependencies
    add_executable(masm_hotpatch_test
        masm_test_main.asm
        $<TARGET_OBJECTS:masm_hotpatch_unified_obj>
    )
    
    set_target_properties(masm_hotpatch_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tests"
        LINK_FLAGS "/SUBSYSTEM:CONSOLE /ENTRY:main kernel32.lib user32.lib gdi32.lib dwmapi.lib uxtheme.lib d2d1.lib dwrite.lib"
    )
    
    # Add test
    enable_testing()
    add_test(NAME MASM_HotpatchSuite COMMAND masm_hotpatch_test)
    
    set_tests_properties(MASM_HotpatchSuite PROPERTIES
        PASS_REGULAR_EXPRESSION "Test Summary"
        FAIL_REGULAR_EXPRESSION "\\[FAIL\\]"
    )
endif()

# ==== Installation Targets ====
install(TARGETS masm_runtime masm_qt6_foundation masm_hotpatch_core masm_hotpatch_unified
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES masm_hotpatch.inc
    DESTINATION include/masm
)

# ==== Tools: MASM Header Generator (pure MASM) ====
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/masm_header_gen.asm")
    set_source_files_properties(tools/masm_header_gen.asm PROPERTIES LANGUAGE ASM_MASM)
    add_executable(masm_header_gen tools/masm_header_gen.asm)
    # Static link flags for a simple console app with kernel32
    set_target_properties(masm_header_gen PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        LINK_FLAGS "/SUBSYSTEM:CONSOLE /ENTRY:main kernel32.lib"
    )
endif()

# ==== Tools: MASM Source Stub Generator (pure MASM) ====
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/masm_source_stub_gen.asm")
    set_source_files_properties(tools/masm_source_stub_gen.asm PROPERTIES LANGUAGE ASM_MASM)
    add_executable(masm_source_stub_gen tools/masm_source_stub_gen.asm)
    set_target_properties(masm_source_stub_gen PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        LINK_FLAGS "/SUBSYSTEM:CONSOLE /ENTRY:main kernel32.lib"
    )
endif()

# ==== MASM Utility Library (Meyers Singleton, etc.) ====
# Thread-safe primitives and utility functions callable from C/C++
set(MASM_UTILITY_SOURCES)
foreach(src MeyersSingleton.asm)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
        list(APPEND MASM_UTILITY_SOURCES ${src})
    endif()
endforeach()

if(MASM_UTILITY_SOURCES)
    set_source_files_properties(${MASM_UTILITY_SOURCES} PROPERTIES LANGUAGE ASM_MASM)
    add_library(masm_utility_obj OBJECT ${MASM_UTILITY_SOURCES})
    add_library(masm_utility STATIC $<TARGET_OBJECTS:masm_utility_obj>)
    set_target_properties(masm_utility PROPERTIES
        OUTPUT_NAME "masm_utility"
    )
    
    # Install utility headers
    install(FILES meyers_singleton_bridge.hpp
        DESTINATION include/masm
    )
    
    # Add to unified library
    list(APPEND MASM_UNIFIED_SOURCES ${MASM_UTILITY_SOURCES})
endif()

# ==== MASM Core Services Library (OrchestraManager, RESTAPIServer) ====
# Production-ready MASM implementations of core services with C++ bridges
set(MASM_CORE_SOURCES)
foreach(src OrchestraManager.asm RESTAPIServer.asm)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
        list(APPEND MASM_CORE_SOURCES ${src})
        message(STATUS "MASM Core Service found: ${src}")
    endif()
endforeach()

if(MASM_CORE_SOURCES)
    set_source_files_properties(${MASM_CORE_SOURCES} PROPERTIES LANGUAGE ASM_MASM)
    add_library(masm_core_obj OBJECT ${MASM_CORE_SOURCES})
    add_library(masm_core STATIC $<TARGET_OBJECTS:masm_core_obj>)
    set_target_properties(masm_core PROPERTIES
        OUTPUT_NAME "masm_core"
    )
    
    # Link against WinSock2 for REST server
    target_link_libraries(masm_core INTERFACE ws2_32)
    
    # Install core service headers
    install(FILES 
        orchestra_manager_bridge.hpp
        rest_api_server_bridge.hpp
        DESTINATION include/masm
    )
    
    # Add to unified library
    list(APPEND MASM_UNIFIED_SOURCES ${MASM_CORE_SOURCES})
endif()

# ==== Build Summary ====
message(STATUS "========================================")
message(STATUS "MASM x64 Hotpatch System (PURE ASM)")
message(STATUS "========================================")
message(STATUS "Compiler: ${CMAKE_ASM_MASM_COMPILER}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Output: ${CMAKE_BINARY_DIR}/lib")
message(STATUS "")
message(STATUS "Libraries (ZERO C/C++ dependencies):")
message(STATUS "  - masm_runtime")
message(STATUS "  - masm_qt6_foundation (Qt6 object model, event queue, signal/slot)")
message(STATUS "  - masm_hotpatch_core")
message(STATUS "  - masm_hotpatch_unified")
message(STATUS "  - masm_utility (Meyers singleton, thread-safe primitives)")
message(STATUS "  - masm_core (OrchestraManager, RESTAPIServer - with C++ bridges)")
message(STATUS "")
message(STATUS "Build tests: ${BUILD_MASM_TESTS}")
message(STATUS "========================================")

# ==== Custom Build Targets ====
add_custom_target(masm_clean
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_BINARY_DIR}/lib"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/lib"
    COMMENT "Cleaning MASM build artifacts"
)

add_custom_target(masm_rebuild
    COMMAND ${CMAKE_COMMAND} --build . --target masm_clean
    COMMAND ${CMAKE_COMMAND} --build . --target masm_hotpatch_unified
    COMMENT "Rebuilding MASM hotpatch system"
)

# Target: masm_stats - Show build statistics
add_custom_target(masm_stats
    COMMAND powershell -Command "Get-ChildItem -Path src/masm/*.asm -Recurse | Measure-Object -Property Length -Sum | Select-Object Count, @{Name='TotalKB'; Expression={[math]::Round($_.Sum/1KB, 2)}}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "MASM source statistics"
)

# If the C++ AVX2 kernel file exists, compile it with AVX2 enabled so
# the runtime-backed intrinsics path is actually available on x86/x64
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/masm_kernels.cpp")
    message(STATUS "Found masm_kernels.cpp; enabling AVX2 compile flags on that source file")
    if(MSVC)
        # MSVC: use /arch:AVX2 and set compile-time macro so __AVX2__ checks pass
        set_source_files_properties(masm_kernels.cpp PROPERTIES COMPILE_FLAGS "/arch:AVX2")
        set_source_files_properties(masm_kernels.cpp PROPERTIES COMPILE_DEFINITIONS "__AVX2__=1")
    else()
        # GCC/Clang
        set_source_files_properties(masm_kernels.cpp PROPERTIES COMPILE_FLAGS "-mavx2 -march=native")
    endif()
endif()
