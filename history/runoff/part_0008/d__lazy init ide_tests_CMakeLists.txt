# ============================================================
# RawrXD Test Suite - CTest Registration
# ============================================================
# This file registers tests defined in the parent CMakeLists.txt with CTest
# and adds additional tests not already defined there.
#
# Tests ALREADY defined in parent CMakeLists.txt (do NOT redefine):
# - test_gguf_loader
# - test_gguf_loader_simple  
# - test_agent_coordinator
# - test_agent_coordinator_integration
# - quant_correctness_tests
# - quant_engine_test
# - quant_scalar_smoke
# - test_kv_cache
# - test_chat_streaming
# - test_terminal
# - test_ide_main
# - test_qmainwindow
# - test_header

cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# Qt DLL Deployment for Tests
# ============================================================
# Function to deploy Qt DLLs to test executables
find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin")

function(deploy_qt_for_test target_name)
    if(WIN32 AND WINDEPLOYQT_EXECUTABLE AND TARGET ${target_name})
        # Only deploy if VCINSTALLDIR is set (running in VS dev environment)
        if(DEFINED ENV{VCINSTALLDIR})
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                    --no-translations
                    --no-system-d3d-compiler
                    --no-opengl-sw
                    "$<TARGET_FILE:${target_name}>"
                COMMENT "Deploying Qt DLLs for ${target_name}"
                VERBATIM
            )
        else()
            # Fallback: copy Qt DLLs manually when outside VS environment
            get_target_property(QT_CORE_LOC Qt6::Core IMPORTED_LOCATION_RELEASE)
            if(QT_CORE_LOC)
                get_filename_component(QT_BIN_DIR "${QT_CORE_LOC}" DIRECTORY)
                set(_qt_extra_dlls "")
                if(EXISTS "${QT_BIN_DIR}/Qt6Multimedia.dll")
                    list(APPEND _qt_extra_dlls "${QT_BIN_DIR}/Qt6Multimedia.dll")
                endif()
                if(EXISTS "${QT_BIN_DIR}/Qt6MultimediaWidgets.dll")
                    list(APPEND _qt_extra_dlls "${QT_BIN_DIR}/Qt6MultimediaWidgets.dll")
                endif()
                add_custom_command(TARGET ${target_name} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${QT_BIN_DIR}/Qt6Core.dll"
                        "${QT_BIN_DIR}/Qt6Gui.dll"
                        "${QT_BIN_DIR}/Qt6Widgets.dll"
                        "${QT_BIN_DIR}/Qt6Test.dll"
                        "${QT_BIN_DIR}/Qt6Network.dll"
                        "${QT_BIN_DIR}/Qt6Sql.dll"
                        ${_qt_extra_dlls}
                        "$<TARGET_FILE_DIR:${target_name}>/"
                    COMMENT "Copying essential Qt DLLs for ${target_name}"
                    VERBATIM
                )
                # Create platforms directory and copy qwindows.dll
                add_custom_command(TARGET ${target_name} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target_name}>/platforms"
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${QT_BIN_DIR}/../plugins/platforms/qwindows.dll"
                        "$<TARGET_FILE_DIR:${target_name}>/platforms/"
                    COMMENT "Copying Qt platform plugin for ${target_name}"
                    VERBATIM
                )
                # Create sqldrivers directory and copy SQLite plugin
                add_custom_command(TARGET ${target_name} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target_name}>/sqldrivers"
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${QT_BIN_DIR}/../plugins/sqldrivers/qsqlite.dll"
                        "$<TARGET_FILE_DIR:${target_name}>/sqldrivers/"
                    COMMENT "Copying Qt SQLite driver for ${target_name}"
                    VERBATIM
                )
                set(_qt_multimedia_plugin_dir "${QT_BIN_DIR}/../plugins/multimedia")
                if(EXISTS "${_qt_multimedia_plugin_dir}")
                    add_custom_command(TARGET ${target_name} POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target_name}>/multimedia"
                        COMMAND ${CMAKE_COMMAND} -E copy_directory
                            "${_qt_multimedia_plugin_dir}"
                            "$<TARGET_FILE_DIR:${target_name}>/multimedia"
                        COMMENT "Copying Qt Multimedia plugins for ${target_name}"
                        VERBATIM
                    )
                endif()
            endif()
        endif()
    endif()
endfunction()

option(RAWRXD_FETCH_GTEST "Fetch GoogleTest if not found" ON)
find_package(GTest QUIET)
if(NOT GTest_FOUND AND RAWRXD_FETCH_GTEST)
    include(FetchContent)
    if(MSVC)
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    endif()
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(googletest)
endif()

if(TARGET gtest AND NOT TARGET GTest::gtest)
    add_library(GTest::gtest ALIAS gtest)
endif()
if(TARGET gtest_main AND NOT TARGET GTest::gtest_main)
    add_library(GTest::gtest_main ALIAS gtest_main)
endif()
if(TARGET gmock AND NOT TARGET GTest::gmock)
    add_library(GTest::gmock ALIAS gmock)
endif()
if(TARGET gmock_main AND NOT TARGET GTest::gmock_main)
    add_library(GTest::gmock_main ALIAS gmock_main)
endif()

# Enable testing
enable_testing()

message(STATUS "========================================")
message(STATUS "Registering Tests with CTest")
message(STATUS "========================================")

# Register existing tests from parent CMakeLists.txt with CTest
# These targets already exist, we just register them for ctest discovery

# GGUF loader tests require a model file argument
# Look for a test model or skip
set(GGUF_TEST_MODEL "" CACHE FILEPATH "Path to a GGUF model file for testing")
if(NOT GGUF_TEST_MODEL)
    # Try to find a model in common locations with actual content (>1MB)
    # The smallest valid GGUF model would be several MB
    file(GLOB GGUF_MODELS_OLLAMA "$ENV{USERPROFILE}/.ollama/models/*.gguf")
    file(GLOB GGUF_MODELS_D "D:/OllamaModels/*.gguf")
    
    # Check Ollama models first
    foreach(MODEL_FILE ${GGUF_MODELS_OLLAMA})
        if(EXISTS "${MODEL_FILE}")
            file(SIZE "${MODEL_FILE}" MODEL_SIZE)
            if(MODEL_SIZE GREATER 1000000)
                set(GGUF_TEST_MODEL "${MODEL_FILE}")
                break()
            endif()
        endif()
    endforeach()
    
    # Then check D:/OllamaModels
    if(NOT GGUF_TEST_MODEL)
        foreach(MODEL_FILE ${GGUF_MODELS_D})
            if(EXISTS "${MODEL_FILE}")
                file(SIZE "${MODEL_FILE}" MODEL_SIZE)
                if(MODEL_SIZE GREATER 1000000)
                    set(GGUF_TEST_MODEL "${MODEL_FILE}")
                    break()
                endif()
            endif()
        endforeach()
    endif()
endif()

if(TARGET test_gguf_loader)
    if(GGUF_TEST_MODEL AND EXISTS "${GGUF_TEST_MODEL}")
        add_test(NAME test_gguf_loader COMMAND test_gguf_loader "${GGUF_TEST_MODEL}")
        message(STATUS "  ✓ Registered test_gguf_loader with model: ${GGUF_TEST_MODEL}")
    else()
        # Register but expect to fail without a model (test will print usage and exit 1)
        add_test(NAME test_gguf_loader COMMAND test_gguf_loader)
        set_tests_properties(test_gguf_loader PROPERTIES WILL_FAIL TRUE)
        message(STATUS "  ⚠ Registered test_gguf_loader (WILL_FAIL - no model file)")
    endif()
endif()

if(TARGET test_gguf_loader_simple)
    if(GGUF_TEST_MODEL AND EXISTS "${GGUF_TEST_MODEL}")
        add_test(NAME test_gguf_loader_simple COMMAND test_gguf_loader_simple "${GGUF_TEST_MODEL}")
        message(STATUS "  ✓ Registered test_gguf_loader_simple with model: ${GGUF_TEST_MODEL}")
    else()
        add_test(NAME test_gguf_loader_simple COMMAND test_gguf_loader_simple)
        set_tests_properties(test_gguf_loader_simple PROPERTIES WILL_FAIL TRUE)
        message(STATUS "  ⚠ Registered test_gguf_loader_simple (WILL_FAIL - no model file)")
    endif()
endif()

if(TARGET test_agent_coordinator)
    add_test(NAME test_agent_coordinator COMMAND test_agent_coordinator)
    # Qt DLL deployment is handled in parent CMakeLists.txt
    message(STATUS "  ✓ Registered test_agent_coordinator")
endif()

if(TARGET test_agent_coordinator_integration)
    add_test(NAME test_agent_coordinator_integration COMMAND test_agent_coordinator_integration)
    # Qt DLL deployment is handled in parent CMakeLists.txt
    message(STATUS "  ✓ Registered test_agent_coordinator_integration")
endif()

if(TARGET quant_correctness_tests)
    add_test(NAME quant_correctness_tests COMMAND quant_correctness_tests)
    message(STATUS "  ✓ Registered quant_correctness_tests")
endif()

if(TARGET quant_engine_test)
    add_test(NAME quant_engine_test COMMAND quant_engine_test)
    message(STATUS "  ✓ Registered quant_engine_test")
endif()

if(TARGET quant_scalar_smoke)
    add_test(NAME quant_scalar_smoke COMMAND quant_scalar_smoke)
    message(STATUS "  ✓ Registered quant_scalar_smoke")
endif()

if(TARGET test_kv_cache)
    add_test(NAME test_kv_cache COMMAND test_kv_cache)
    message(STATUS "  ✓ Registered test_kv_cache")
endif()

# ============================================================
# NEW TESTS - Only add tests not in parent CMakeLists.txt
# ============================================================

# Streaming GGUF loader budget regression (120B-class coverage)
if(NOT TARGET test_streaming_gguf_loader AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_streaming_gguf_loader.cpp")
    add_executable(test_streaming_gguf_loader
        test_streaming_gguf_loader.cpp
        ${CMAKE_SOURCE_DIR}/src/streaming_gguf_loader.cpp
        ${CMAKE_SOURCE_DIR}/src/utils/Diagnostics.cpp
    )
    target_include_directories(test_streaming_gguf_loader PRIVATE 
        ${CMAKE_SOURCE_DIR}/include 
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/3rdparty/ggml/include
        ${CMAKE_SOURCE_DIR}/3rdparty/ggml/src)
    target_link_libraries(test_streaming_gguf_loader PRIVATE GTest::gtest_main ggml)
    set_target_properties(test_streaming_gguf_loader PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_streaming_gguf_loader COMMAND test_streaming_gguf_loader)
    message(STATUS "  ✓ Added test_streaming_gguf_loader")
endif()

# Production benchmark verification (validate performance claims)
if(NOT TARGET benchmark_production_verification AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmark_production_verification.cpp")
    add_executable(benchmark_production_verification
        benchmark_production_verification.cpp
        ${CMAKE_SOURCE_DIR}/src/streaming_gguf_loader_enhanced.cpp
        ${CMAKE_SOURCE_DIR}/src/streaming_gguf_loader.cpp
        ${CMAKE_SOURCE_DIR}/src/utils/Diagnostics.cpp
    )
    target_include_directories(benchmark_production_verification PRIVATE 
        ${CMAKE_SOURCE_DIR}/include 
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/3rdparty/ggml/include
        ${CMAKE_SOURCE_DIR}/3rdparty/ggml/src)
    target_link_libraries(benchmark_production_verification PRIVATE ggml)
    target_compile_definitions(benchmark_production_verification PRIVATE _WIN32_WINNT=0x0A00)
    set_target_properties(benchmark_production_verification PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    message(STATUS "  ✓ Added benchmark_production_verification (NOT a test - run manually)")
endif()

# Zero-copy micro-benchmark (validates 50μs target)
if(NOT TARGET benchmark_zerocopy_microbench AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmark_zerocopy_microbench.cpp")
    add_executable(benchmark_zerocopy_microbench
        benchmark_zerocopy_microbench.cpp
        ${CMAKE_SOURCE_DIR}/src/streaming_gguf_loader_enhanced.cpp
        ${CMAKE_SOURCE_DIR}/src/streaming_gguf_loader.cpp
        ${CMAKE_SOURCE_DIR}/src/utils/Diagnostics.cpp
    )
    target_include_directories(benchmark_zerocopy_microbench PRIVATE 
        ${CMAKE_SOURCE_DIR}/include 
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/3rdparty/ggml/include
        ${CMAKE_SOURCE_DIR}/3rdparty/ggml/src)
    target_link_libraries(benchmark_zerocopy_microbench PRIVATE ggml)
    target_compile_definitions(benchmark_zerocopy_microbench PRIVATE _WIN32_WINNT=0x0A00)
    set_target_properties(benchmark_zerocopy_microbench PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    message(STATUS "  ✓ Added benchmark_zerocopy_microbench (50μs target validation)")
endif()

# IORing Unit Test (v1.1.0)
if(NOT TARGET test_ioring_loader AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_ioring_loader.cpp")
    add_executable(test_ioring_loader test_ioring_loader.cpp 
        ${CMAKE_SOURCE_DIR}/src/io/io_factory.cpp
    )
    target_link_libraries(test_ioring_loader PRIVATE onecoreuap.lib)
    target_include_directories(test_ioring_loader PRIVATE ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(test_ioring_loader PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_ioring_loader COMMAND test_ioring_loader)
    message(STATUS "  ✓ Added test_ioring_loader (v1.1.0 Kernel-Bypass)")
endif()

# IORing Performance Benchmark (v1.1.0)
if(NOT TARGET benchmark_ioring_performance AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmark_ioring_performance.cpp")
    add_executable(benchmark_ioring_performance benchmark_ioring_performance.cpp 
        ${CMAKE_SOURCE_DIR}/src/io/io_factory.cpp
    )
    target_link_libraries(benchmark_ioring_performance PRIVATE onecoreuap.lib)
    target_include_directories(benchmark_ioring_performance PRIVATE ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(benchmark_ioring_performance PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    message(STATUS "  ✓ Added benchmark_ioring_performance (v1.1.0)")
endif()

# AI Completions Test (full inference pipeline)
# DISABLED: Exit code 0xc000007b - Bad image format at runtime (investigate ggml linkage)
if(FALSE AND NOT TARGET test_ai_completions AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_ai_completions.cpp")
    add_executable(test_ai_completions 
        test_ai_completions.cpp
        # AI completion provider and engine
        ${CMAKE_SOURCE_DIR}/src/qtapp/ai_completion_provider.cpp
        ${CMAKE_SOURCE_DIR}/src/real_time_completion_engine.cpp
        ${CMAKE_SOURCE_DIR}/src/qtapp/inference_engine.cpp
        # GGUF loading (base class + Qt wrapper)
        ${CMAKE_SOURCE_DIR}/src/gguf_loader.cpp
        ${CMAKE_SOURCE_DIR}/src/qtapp/gguf_loader.cpp
        ${CMAKE_SOURCE_DIR}/src/gguf_vocab_resolver.cpp
        # Tokenizers
        ${CMAKE_SOURCE_DIR}/src/qtapp/bpe_tokenizer.cpp
        ${CMAKE_SOURCE_DIR}/src/qtapp/sentencepiece_tokenizer.cpp
        ${CMAKE_SOURCE_DIR}/src/qtapp/vocabulary_loader.cpp
        # Transformer inference
        ${CMAKE_SOURCE_DIR}/src/qtapp/transformer_inference.cpp
        # Quantization utils
        ${CMAKE_SOURCE_DIR}/src/qtapp/quant_utils.cpp
        # Codec stubs for compression (test only - no zlib dependency)
        ${CMAKE_SOURCE_DIR}/src/qtapp/codec_stub.cpp
        ${CMAKE_SOURCE_DIR}/src/qtapp/deflate_brutal_stub.cpp
    )
    target_link_libraries(test_ai_completions PRIVATE 
        Qt6::Core Qt6::Test Qt6::Network Qt6::Widgets Qt6::Concurrent
        $<TARGET_NAME_IF_EXISTS:ggml_interface>
        $<TARGET_NAME_IF_EXISTS:ggml>
    )
    target_include_directories(test_ai_completions PRIVATE 
        ${CMAKE_SOURCE_DIR}/include 
        ${CMAKE_SOURCE_DIR}/src 
        ${CMAKE_SOURCE_DIR}/src/qtapp
        ${CMAKE_SOURCE_DIR}/3rdparty/ggml/include
        ${CMAKE_SOURCE_DIR}/3rdparty/ggml/src
    )
    set_target_properties(test_ai_completions PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" AUTOMOC ON)
    add_test(NAME test_ai_completions COMMAND test_ai_completions)
    deploy_qt_for_test(test_ai_completions)
    message(STATUS "  ✓ Added test_ai_completions")
endif()

# LLM Connectivity Test - requires CURL and OpenSSL
# Disabled: These optional dependencies (CURL, OpenSSL) are not typically available
# To enable: install libcurl and openssl dev packages, then uncomment below
# if(NOT TARGET test_llm_connectivity AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_llm_connectivity.cpp" AND CURL_FOUND)
#     add_executable(test_llm_connectivity 
#         test_llm_connectivity.cpp
#         ${CMAKE_SOURCE_DIR}/src/llm_adapter/llm_http_client.cpp
#     )
#     target_link_libraries(test_llm_connectivity PRIVATE Qt6::Core Qt6::Test Qt6::Network CURL::libcurl)
#     target_include_directories(test_llm_connectivity PRIVATE 
#         ${CMAKE_SOURCE_DIR}/include 
#         ${CMAKE_SOURCE_DIR}/src 
#         ${CMAKE_SOURCE_DIR}/src/llm_adapter
#     )
#     set_target_properties(test_llm_connectivity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" AUTOMOC ON)
#     add_test(NAME test_llm_connectivity COMMAND test_llm_connectivity)
#     message(STATUS "  ✓ Added test_llm_connectivity")
# endif()
message(STATUS "  ⚠ Skipped test_llm_connectivity - requires CURL and OpenSSL")

# Tool Registry Test - standalone test with custom main()
if(NOT TARGET test_tool_registry AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_tool_registry.cpp")
    add_executable(test_tool_registry 
        test_tool_registry.cpp
        ${CMAKE_SOURCE_DIR}/src/tool_registry.cpp
    )
    target_include_directories(test_tool_registry PRIVATE 
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    if(WIN32)
        target_link_libraries(test_tool_registry PRIVATE Rpcrt4)
    endif()
    set_target_properties(test_tool_registry PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_tool_registry COMMAND test_tool_registry)
    message(STATUS "  ✓ Added test_tool_registry")
endif()

# Voice Processor Test - DISABLED: test uses wrong API (sampleSize not in Config struct)
# Requires Qt6::Multimedia component
find_package(Qt6 QUIET COMPONENTS Multimedia)
if(Qt6Multimedia_FOUND AND NOT TARGET test_voice_processor AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_voice_processor.cpp")
    message(STATUS "  ⚠ DISABLED test_voice_processor - freezes without audio hardware (CI only)")
endif()

# Zero Retention Manager Test - Google Test with Qt
if(NOT TARGET test_zero_retention_manager AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_zero_retention_manager.cpp")
    add_executable(test_zero_retention_manager 
        test_zero_retention_manager.cpp
        ${CMAKE_SOURCE_DIR}/src/terminal/zero_retention_manager.cpp
    )
    target_link_libraries(test_zero_retention_manager PRIVATE 
        Qt6::Core Qt6::Test Qt6::Widgets
        GTest::gtest GTest::gtest_main
    )
    target_include_directories(test_zero_retention_manager PRIVATE 
        ${CMAKE_SOURCE_DIR}/include 
        ${CMAKE_SOURCE_DIR}/src 
        ${CMAKE_SOURCE_DIR}/src/terminal
    )
    set_target_properties(test_zero_retention_manager PROPERTIES 
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" 
        AUTOMOC ON
    )
    add_test(NAME test_zero_retention_manager COMMAND test_zero_retention_manager)
    deploy_qt_for_test(test_zero_retention_manager)
    message(STATUS "  ✓ Added test_zero_retention_manager")
endif()

# Semantic Diff Analyzer Test
if(NOT TARGET test_semantic_diff_analyzer AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_semantic_diff_analyzer.cpp")
    add_executable(test_semantic_diff_analyzer 
        test_semantic_diff_analyzer.cpp
        ${CMAKE_SOURCE_DIR}/src/git/semantic_diff_analyzer.cpp
    )
    target_link_libraries(test_semantic_diff_analyzer PRIVATE Qt6::Core Qt6::Test Qt6::Concurrent Qt6::Network GTest::gtest_main)
    target_include_directories(test_semantic_diff_analyzer PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/git)
    set_target_properties(test_semantic_diff_analyzer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" AUTOMOC ON)
    add_test(NAME test_semantic_diff_analyzer COMMAND test_semantic_diff_analyzer)
    deploy_qt_for_test(test_semantic_diff_analyzer)
    message(STATUS "  ✓ Added test_semantic_diff_analyzer")
endif()

# AI Merge Resolver Test
if(NOT TARGET test_ai_merge_resolver AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_ai_merge_resolver.cpp")
    add_executable(test_ai_merge_resolver 
        test_ai_merge_resolver.cpp
        ${CMAKE_SOURCE_DIR}/src/git/ai_merge_resolver.cpp
    )
    target_link_libraries(test_ai_merge_resolver PRIVATE Qt6::Core Qt6::Test Qt6::Network Qt6::Concurrent GTest::gmock_main)
    target_include_directories(test_ai_merge_resolver PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/git)
    set_target_properties(test_ai_merge_resolver PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" AUTOMOC ON)
    add_test(NAME test_ai_merge_resolver COMMAND test_ai_merge_resolver)
    deploy_qt_for_test(test_ai_merge_resolver)
    message(STATUS "  ✓ Added test_ai_merge_resolver")
endif()

# Sandboxed Terminal Test
if(NOT TARGET test_sandboxed_terminal AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_sandboxed_terminal.cpp")
    add_executable(test_sandboxed_terminal 
        test_sandboxed_terminal.cpp
        ${CMAKE_SOURCE_DIR}/src/terminal/sandboxed_terminal.cpp
    )
    target_link_libraries(test_sandboxed_terminal PRIVATE Qt6::Core Qt6::Test Qt6::Widgets GTest::gtest_main)
    target_include_directories(test_sandboxed_terminal PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/terminal)
    set_target_properties(test_sandboxed_terminal PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" AUTOMOC ON)
    add_test(NAME test_sandboxed_terminal COMMAND test_sandboxed_terminal)
    deploy_qt_for_test(test_sandboxed_terminal)
    message(STATUS "  ✓ Added test_sandboxed_terminal")
endif()

# Agentic File Operations Test
# Includes MOC-enabled classes with Q_OBJECT macro and full signal/slot support
if(NOT TARGET test_agentic_file_operations AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_agentic_file_operations.cpp")
    # Enable AUTOMOC early so all Qt source files get processed
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    
    add_executable(test_agentic_file_operations 
        test_agentic_file_operations.cpp
        # Core file operations with approval dialogs
        ${CMAKE_SOURCE_DIR}/src/agentic_file_operations.cpp
        # Error handler with recovery strategies (requires observability + loop state)
        ${CMAKE_SOURCE_DIR}/src/agentic_error_handler.cpp
        # Observability system for structured logging/metrics/tracing
        ${CMAKE_SOURCE_DIR}/src/agentic_observability.cpp
        # Loop state management for error handler initialization
        ${CMAKE_SOURCE_DIR}/src/agentic_loop_state.cpp
        # Include headers to trigger MOC on Q_OBJECT classes
        ${CMAKE_SOURCE_DIR}/include/agentic_file_operations.h
        ${CMAKE_SOURCE_DIR}/include/agentic_error_handler.h
        ${CMAKE_SOURCE_DIR}/include/agentic_observability.h
    )
    target_link_libraries(test_agentic_file_operations PRIVATE 
        Qt6::Core Qt6::Test Qt6::Widgets Qt6::Gui
    )
    target_include_directories(test_agentic_file_operations PRIVATE 
        ${CMAKE_SOURCE_DIR}/include 
        ${CMAKE_SOURCE_DIR}/src
    )
    set_target_properties(test_agentic_file_operations PROPERTIES 
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" 
        AUTOMOC ON
        AUTOUIC ON
    )
    add_test(NAME test_agentic_file_operations COMMAND test_agentic_file_operations
             WORKING_DIRECTORY "$<TARGET_FILE_DIR:test_agentic_file_operations>")
    set_tests_properties(test_agentic_file_operations PROPERTIES 
        ENVIRONMENT "QT_QPA_PLATFORM=windows")
    deploy_qt_for_test(test_agentic_file_operations)
    message(STATUS "  ✓ Added test_agentic_file_operations")
endif()

# Model Registry Test
if(NOT TARGET model_registry_test AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/model_registry_test.cpp")
    add_executable(model_registry_test 
        model_registry_test.cpp
        ${CMAKE_SOURCE_DIR}/src/model_registry.cpp
    )
    target_link_libraries(model_registry_test PRIVATE Qt6::Core Qt6::Test Qt6::Widgets Qt6::Sql)
    target_include_directories(model_registry_test PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(model_registry_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" AUTOMOC ON)
    add_test(NAME model_registry_test COMMAND model_registry_test)
    deploy_qt_for_test(model_registry_test)
    message(STATUS "  ✓ Added model_registry_test")
endif()

# Qt AI Integration Test (full inference pipeline)
# DISABLED: Exit code 0xc000007b - Bad image format at runtime (investigate ggml linkage)
if(FALSE AND NOT TARGET test_qt_ai_integration AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_qt_ai_integration.cpp")
    add_executable(test_qt_ai_integration 
        test_qt_ai_integration.cpp
        # AI completion provider and engine
        ${CMAKE_SOURCE_DIR}/src/qtapp/ai_completion_provider.cpp
        ${CMAKE_SOURCE_DIR}/src/real_time_completion_engine.cpp
        ${CMAKE_SOURCE_DIR}/src/qtapp/inference_engine.cpp
        # GGUF loading (base class + Qt wrapper)
        ${CMAKE_SOURCE_DIR}/src/gguf_loader.cpp
        ${CMAKE_SOURCE_DIR}/src/qtapp/gguf_loader.cpp
        ${CMAKE_SOURCE_DIR}/src/gguf_vocab_resolver.cpp
        # Tokenizers
        ${CMAKE_SOURCE_DIR}/src/qtapp/bpe_tokenizer.cpp
        ${CMAKE_SOURCE_DIR}/src/qtapp/sentencepiece_tokenizer.cpp
        ${CMAKE_SOURCE_DIR}/src/qtapp/vocabulary_loader.cpp
        # Transformer inference
        ${CMAKE_SOURCE_DIR}/src/qtapp/transformer_inference.cpp
        # Quantization utils
        ${CMAKE_SOURCE_DIR}/src/qtapp/quant_utils.cpp
        # Codec stubs for compression (test only - no zlib dependency)
        ${CMAKE_SOURCE_DIR}/src/qtapp/codec_stub.cpp
        ${CMAKE_SOURCE_DIR}/src/qtapp/deflate_brutal_stub.cpp
    )
    # Link against Qt, ggml for transformer inference
    target_link_libraries(test_qt_ai_integration PRIVATE 
        Qt6::Core Qt6::Test Qt6::Widgets Qt6::Network Qt6::Concurrent
        $<TARGET_NAME_IF_EXISTS:ggml_interface>
        $<TARGET_NAME_IF_EXISTS:ggml>
    )
    target_include_directories(test_qt_ai_integration PRIVATE 
        ${CMAKE_SOURCE_DIR}/include 
        ${CMAKE_SOURCE_DIR}/src 
        ${CMAKE_SOURCE_DIR}/src/qtapp
        ${CMAKE_SOURCE_DIR}/src/ai
        ${CMAKE_SOURCE_DIR}/3rdparty/ggml/include
        ${CMAKE_SOURCE_DIR}/3rdparty/ggml/src
    )
    set_target_properties(test_qt_ai_integration PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" AUTOMOC ON)
    add_test(NAME test_qt_ai_integration COMMAND test_qt_ai_integration)
    deploy_qt_for_test(test_qt_ai_integration)
    message(STATUS "  ✓ Added test_qt_ai_integration")
endif()

# GGUF Integration Test
# NOTE: Disabled - GTest EXPECT_EQ doesn't work with QString; need custom printer
# if(NOT TARGET test_gguf_integration AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_gguf_integration.cpp")
#     add_executable(test_gguf_integration 
#         test_gguf_integration.cpp
#         ${CMAKE_SOURCE_DIR}/src/qtapp/inference_engine.cpp
#         ...
#     )
# endif()
message(STATUS "  ⚠ Skipped test_gguf_integration - GTest/QString issues")

# GPU Hybrid MASM/Vulkan shim test
if(NOT TARGET test_gpu_hybrid AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_gpu_hybrid.cpp")
    add_executable(test_gpu_hybrid
        test_gpu_hybrid.cpp
    )
    target_include_directories(test_gpu_hybrid PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/gpu_masm
    )
    target_link_libraries(test_gpu_hybrid PRIVATE GTest::gtest_main)
    set_target_properties(test_gpu_hybrid PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

    # Discover a usable matmul SPIR-V (optional GPU path). If none found, the test exercises CPU fallback.
    set(RAWRXD_MATMUL_SPV_PATH "")
    set(_matmul_spv_candidates
        "${CMAKE_SOURCE_DIR}/3rdparty/ggml/src/ggml-vulkan/vulkan-shaders.spv/mul_mm.spv"
        "${CMAKE_SOURCE_DIR}/3rdparty/ggml/src/ggml-vulkan/vulkan-shaders.spv/matmul_f16.spv"
        "${CMAKE_SOURCE_DIR}/3rdparty/ggml/src/ggml-vulkan/vulkan-shaders.spv/matmul.spv"
    )
    foreach(cand IN LISTS _matmul_spv_candidates)
        if(EXISTS "${cand}")
            set(RAWRXD_MATMUL_SPV_PATH "${cand}")
            break()
        endif()
    endforeach()

    add_test(NAME test_gpu_hybrid COMMAND test_gpu_hybrid)
    if(RAWRXD_MATMUL_SPV_PATH)
        set_tests_properties(test_gpu_hybrid PROPERTIES ENVIRONMENT "RAWRXD_MATMUL_SPV=${RAWRXD_MATMUL_SPV_PATH}")
        message(STATUS "  ✓ Added test_gpu_hybrid (GPU path enabled: ${RAWRXD_MATMUL_SPV_PATH})")
    else()
        message(STATUS "  ✓ Added test_gpu_hybrid (CPU fallback only - no SPIR-V found)")
    endif()
endif()

# ============================================================
# Quantum Burst Engine (v1.1.x)
# ============================================================
if(NOT TARGET rawrxd_burst)
    if(MSVC)
        enable_language(ASM_MASM)
    endif()

    add_library(rawrxd_burst STATIC
        ${CMAKE_SOURCE_DIR}/src/direct_io/direct_io_ring_win.cpp
        ${CMAKE_SOURCE_DIR}/src/direct_io/tensor_access_planner.cpp
        ${CMAKE_SOURCE_DIR}/src/direct_io/RAWRXD_BURSTCORE_X7.asm
    )
    
    if(MSVC)
        set_source_files_properties(
            ${CMAKE_SOURCE_DIR}/src/direct_io/RAWRXD_BURSTCORE_X7.asm
            PROPERTIES 
            LANGUAGE ASM_MASM
            COMPILE_DEFINITIONS ""
        )
    endif()

    target_include_directories(rawrxd_burst PUBLIC ${CMAKE_SOURCE_DIR}/src/direct_io)
    target_link_libraries(rawrxd_burst PRIVATE onecoreuap.lib)
endif()

# burstc patcher
if(NOT TARGET burstc AND EXISTS "${CMAKE_SOURCE_DIR}/src/direct_io/burstc_main.cpp")
    add_executable(burstc ${CMAKE_SOURCE_DIR}/src/direct_io/burstc_main.cpp)
    set_target_properties(burstc PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools")
endif()

# Cold Burst Benchmark
if(NOT TARGET benchmark_coldburst AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmark_coldburst.cpp")
    add_executable(benchmark_coldburst benchmark_coldburst.cpp)
    target_link_libraries(benchmark_coldburst PRIVATE rawrxd_burst)
    set_target_properties(benchmark_coldburst PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    message(STATUS "  ✓ Added benchmark_coldburst (v1.1.x Quantum)")
endif()

# Legacy IORing tests (updated to use rawrxd_burst)
if(NOT TARGET test_ioring_loader AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_ioring_loader.cpp")
    add_executable(test_ioring_loader test_ioring_loader.cpp)
    target_link_libraries(test_ioring_loader PRIVATE rawrxd_burst)
    set_target_properties(test_ioring_loader PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_ioring_loader COMMAND test_ioring_loader)
    message(STATUS "  ✓ Added test_ioring_loader (v1.1.0 Kernel-Bypass)")
endif()

# ============================================================
# Test Summary
# ============================================================
message(STATUS "========================================")
message(STATUS "Test Suite Registration Complete")
message(STATUS "Run tests: ctest --output-on-failure -C Release")
message(STATUS "Run specific: ctest -R <name> -V -C Release")
message(STATUS "========================================")