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
# - test_header

cmake_minimum_required(VERSION 3.20)

# Enable testing
enable_testing()

message(STATUS "========================================")
message(STATUS "Registering Tests with CTest")
message(STATUS "========================================")

# Register existing tests from parent CMakeLists.txt with CTest
# These targets already exist, we just register them for ctest discovery
if(TARGET test_gguf_loader)
    add_test(NAME test_gguf_loader COMMAND test_gguf_loader)
    message(STATUS "  ✓ Registered test_gguf_loader")
endif()

if(TARGET test_gguf_loader_simple)
    add_test(NAME test_gguf_loader_simple COMMAND test_gguf_loader_simple)
    message(STATUS "  ✓ Registered test_gguf_loader_simple")
endif()

if(TARGET test_agent_coordinator)
    add_test(NAME test_agent_coordinator COMMAND test_agent_coordinator)
    message(STATUS "  ✓ Registered test_agent_coordinator")
endif()

if(TARGET test_agent_coordinator_integration)
    add_test(NAME test_agent_coordinator_integration COMMAND test_agent_coordinator_integration)
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
# NEW TESTS - Qt-free, C++20 native test framework
# ============================================================

# AI Completions Test
if(NOT TARGET test_ai_completions AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_ai_completions.cpp")
    add_executable(test_ai_completions test_ai_completions.cpp)
    target_include_directories(test_ai_completions PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(test_ai_completions PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" CXX_STANDARD 20)
    add_test(NAME test_ai_completions COMMAND test_ai_completions)
    message(STATUS "  ✓ Added test_ai_completions")
endif()

# LLM Connectivity Test
if(NOT TARGET test_llm_connectivity AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_llm_connectivity.cpp")
    add_executable(test_llm_connectivity test_llm_connectivity.cpp)
    target_include_directories(test_llm_connectivity PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(test_llm_connectivity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" CXX_STANDARD 20)
    if(WIN32)
        target_link_libraries(test_llm_connectivity PRIVATE ws2_32 winhttp)
    endif()
    add_test(NAME test_llm_connectivity COMMAND test_llm_connectivity)
    message(STATUS "  ✓ Added test_llm_connectivity")
endif()

# Tool Registry Test
if(NOT TARGET test_tool_registry AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_tool_registry.cpp")
    add_executable(test_tool_registry
        test_tool_registry.cpp
        tool_registry_full_impl.cpp
    )
    target_include_directories(test_tool_registry PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(test_tool_registry PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" CXX_STANDARD 20)
    add_test(NAME test_tool_registry COMMAND test_tool_registry)
    message(STATUS "  ✓ Added test_tool_registry")
endif()

# Voice Processor Test
if(NOT TARGET test_voice_processor AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_voice_processor.cpp")
    add_executable(test_voice_processor test_voice_processor.cpp)
    target_include_directories(test_voice_processor PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(test_voice_processor PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" CXX_STANDARD 20)
    add_test(NAME test_voice_processor COMMAND test_voice_processor)
    message(STATUS "  ✓ Added test_voice_processor")
endif()

# Zero Retention Manager Test
if(NOT TARGET test_zero_retention_manager AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_zero_retention_manager.cpp")
    add_executable(test_zero_retention_manager test_zero_retention_manager.cpp)
    target_include_directories(test_zero_retention_manager PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(test_zero_retention_manager PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" CXX_STANDARD 20)
    add_test(NAME test_zero_retention_manager COMMAND test_zero_retention_manager)
    message(STATUS "  ✓ Added test_zero_retention_manager")
endif()

# Semantic Diff Analyzer Test
if(NOT TARGET test_semantic_diff_analyzer AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_semantic_diff_analyzer.cpp")
    add_executable(test_semantic_diff_analyzer test_semantic_diff_analyzer.cpp)
    target_include_directories(test_semantic_diff_analyzer PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(test_semantic_diff_analyzer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" CXX_STANDARD 20)
    add_test(NAME test_semantic_diff_analyzer COMMAND test_semantic_diff_analyzer)
    message(STATUS "  ✓ Added test_semantic_diff_analyzer")
endif()

# AI Merge Resolver Test
if(NOT TARGET test_ai_merge_resolver AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_ai_merge_resolver.cpp")
    add_executable(test_ai_merge_resolver test_ai_merge_resolver.cpp)
    target_include_directories(test_ai_merge_resolver PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(test_ai_merge_resolver PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" CXX_STANDARD 20)
    if(WIN32)
        target_link_libraries(test_ai_merge_resolver PRIVATE ws2_32 winhttp)
    endif()
    add_test(NAME test_ai_merge_resolver COMMAND test_ai_merge_resolver)
    message(STATUS "  ✓ Added test_ai_merge_resolver")
endif()

# Sandboxed Terminal Test
if(NOT TARGET test_sandboxed_terminal AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_sandboxed_terminal.cpp")
    add_executable(test_sandboxed_terminal test_sandboxed_terminal.cpp)
    target_include_directories(test_sandboxed_terminal PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(test_sandboxed_terminal PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" CXX_STANDARD 20)
    add_test(NAME test_sandboxed_terminal COMMAND test_sandboxed_terminal)
    message(STATUS "  ✓ Added test_sandboxed_terminal")
endif()

# Agentic File Operations Test
if(NOT TARGET test_agentic_file_operations AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_agentic_file_operations.cpp")
    add_executable(test_agentic_file_operations test_agentic_file_operations.cpp)
    target_include_directories(test_agentic_file_operations PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(test_agentic_file_operations PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" CXX_STANDARD 20)
    add_test(NAME test_agentic_file_operations COMMAND test_agentic_file_operations)
    message(STATUS "  ✓ Added test_agentic_file_operations")
endif()

# Model Registry Test
if(NOT TARGET model_registry_test AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/model_registry_test.cpp")
    add_executable(model_registry_test model_registry_test.cpp)
    target_include_directories(model_registry_test PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(model_registry_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" CXX_STANDARD 20)
    add_test(NAME model_registry_test COMMAND model_registry_test)
    message(STATUS "  ✓ Added model_registry_test")
endif()

# GGUF Integration Test
if(NOT TARGET test_gguf_integration AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_gguf_integration.cpp")
    add_executable(test_gguf_integration test_gguf_integration.cpp)
    target_include_directories(test_gguf_integration PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(test_gguf_integration PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" CXX_STANDARD 20)
    add_test(NAME test_gguf_integration COMMAND test_gguf_integration)
    message(STATUS "  ✓ Added test_gguf_integration")
endif()

if(NOT TARGET test_swarm_scheduler_pin AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_swarm_scheduler_pin.cpp")
    add_executable(test_swarm_scheduler_pin
        test_swarm_scheduler_pin.cpp
        swarm_scheduler_rawr_loader_link_stub.cpp
        ${CMAKE_SOURCE_DIR}/src/core/swarm_scheduler.cpp
        ${CMAKE_SOURCE_DIR}/src/logging/Logger.cpp
    )
    target_include_directories(test_swarm_scheduler_pin PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(test_swarm_scheduler_pin PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" CXX_STANDARD 23)
    add_test(NAME test_swarm_scheduler_pin COMMAND test_swarm_scheduler_pin)
    message(STATUS "  ✓ Added test_swarm_scheduler_pin")
endif()

if(NOT TARGET test_swarm_expert_heatmap_stress AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_swarm_expert_heatmap_stress.cpp")
    add_executable(test_swarm_expert_heatmap_stress
        test_swarm_expert_heatmap_stress.cpp
        swarm_scheduler_rawr_loader_link_stub.cpp
        ${CMAKE_SOURCE_DIR}/src/core/swarm_scheduler.cpp
        ${CMAKE_SOURCE_DIR}/src/logging/Logger.cpp
    )
    target_include_directories(test_swarm_expert_heatmap_stress PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(test_swarm_expert_heatmap_stress PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" CXX_STANDARD 23)
    add_test(NAME test_swarm_expert_heatmap_stress COMMAND test_swarm_expert_heatmap_stress)
    message(STATUS "  ✓ Added test_swarm_expert_heatmap_stress")
endif()

if(NOT TARGET test_moe_mixture_reference AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_moe_mixture_reference.cpp")
    add_executable(test_moe_mixture_reference test_moe_mixture_reference.cpp)
    target_include_directories(test_moe_mixture_reference PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(test_moe_mixture_reference PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" CXX_STANDARD 20)
    add_test(NAME test_moe_mixture_reference COMMAND test_moe_mixture_reference)
    message(STATUS "  ✓ Added test_moe_mixture_reference")
endif()

if(NOT TARGET test_moe_down_project_policy AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_moe_down_project_policy.cpp")
    add_executable(test_moe_down_project_policy test_moe_down_project_policy.cpp)
    target_include_directories(test_moe_down_project_policy PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(test_moe_down_project_policy PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" CXX_STANDARD 20)
    add_test(NAME test_moe_down_project_policy COMMAND test_moe_down_project_policy)
    message(STATUS "  ✓ Added test_moe_down_project_policy")
endif()

if(NOT TARGET test_moe_plan_row_mixture_pack_cache AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_moe_plan_row_mixture_pack_cache.cpp")
    add_executable(test_moe_plan_row_mixture_pack_cache test_moe_plan_row_mixture_pack_cache.cpp)
    target_include_directories(test_moe_plan_row_mixture_pack_cache PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(test_moe_plan_row_mixture_pack_cache PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" CXX_STANDARD 20)
    add_test(NAME test_moe_plan_row_mixture_pack_cache COMMAND test_moe_plan_row_mixture_pack_cache)
    message(STATUS "  ✓ Added test_moe_plan_row_mixture_pack_cache")
endif()

if(NOT TARGET test_moe_batched_down_project AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_moe_batched_down_project.cpp")
    add_executable(test_moe_batched_down_project test_moe_batched_down_project.cpp)
    target_include_directories(test_moe_batched_down_project PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(test_moe_batched_down_project PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" CXX_STANDARD 20)
    add_test(NAME test_moe_batched_down_project COMMAND test_moe_batched_down_project)
    message(STATUS "  ✓ Added test_moe_batched_down_project")
endif()

if(NOT TARGET test_moe_pack_hud_metrics AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_moe_pack_hud_metrics.cpp")
    add_executable(test_moe_pack_hud_metrics test_moe_pack_hud_metrics.cpp)
    target_include_directories(test_moe_pack_hud_metrics PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/core)
    set_target_properties(test_moe_pack_hud_metrics PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" CXX_STANDARD 23)
    add_test(NAME test_moe_pack_hud_metrics COMMAND test_moe_pack_hud_metrics)
    message(STATUS "  ✓ Added test_moe_pack_hud_metrics")
endif()

if(NOT TARGET moe_grouped_gemm_bench AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/moe_grouped_gemm_bench.cpp")
    add_executable(moe_grouped_gemm_bench moe_grouped_gemm_bench.cpp)
    target_include_directories(moe_grouped_gemm_bench PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(moe_grouped_gemm_bench PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" CXX_STANDARD 20)
    message(STATUS "  ✓ Added moe_grouped_gemm_bench (manual / CI microbench; not registered in CTest)")
endif()

if(NOT TARGET test_basic_profiler AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/performance/test_basic_profiler.cpp")
    add_executable(test_basic_profiler
        performance/test_basic_profiler.cpp
        ${CMAKE_SOURCE_DIR}/src/profiler.cpp
        ${CMAKE_SOURCE_DIR}/src/profiling/AdvancedPerformanceProfiler.cpp
    )
    target_include_directories(test_basic_profiler PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(test_basic_profiler PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" CXX_STANDARD 20)
    add_test(NAME test_basic_profiler COMMAND test_basic_profiler)
    message(STATUS "  ✓ Added test_basic_profiler")
endif()

if(NOT TARGET test_batch3_profiler_smoke AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/performance/test_batch3_profiler_smoke.cpp")
    add_executable(test_batch3_profiler_smoke
        performance/test_batch3_profiler_smoke.cpp
        ${CMAKE_SOURCE_DIR}/src/profiler.cpp
        ${CMAKE_SOURCE_DIR}/src/profiling/AdvancedPerformanceProfiler.cpp
    )
    target_include_directories(test_batch3_profiler_smoke PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(test_batch3_profiler_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" CXX_STANDARD 20)
    add_test(NAME test_batch3_profiler_smoke COMMAND test_batch3_profiler_smoke)
    message(STATUS "  ✓ Added test_batch3_profiler_smoke")
endif()

# ============================================================
# Test Summary
# ============================================================

# Production Regression Test Suite (Qt-free, no external deps)
if(NOT TARGET test_production_regression AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_production_regression.cpp")
    add_executable(test_production_regression test_production_regression.cpp)
    target_include_directories(test_production_regression PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/3rdparty
    )
    # Link against the core library when RawrEngine is built as a library target.
    if(TARGET RawrEngine)
        get_target_property(_rawrengine_target_type RawrEngine TYPE)
    endif()
    if(TARGET RawrEngine AND NOT _rawrengine_target_type STREQUAL "EXECUTABLE")
        target_link_libraries(test_production_regression PRIVATE RawrEngine)
    endif()
    set_target_properties(test_production_regression PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        CXX_STANDARD 20
    )
    add_test(NAME test_production_regression COMMAND test_production_regression)
    message(STATUS "  ✓ Added test_production_regression (Qt-free regression suite)")
endif()

# Instructions Provider Test (getAllContent aggregation, priority sort, hot-reload watcher)
if(NOT TARGET test_instructions_provider AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_instructions_provider.cpp")
    add_executable(test_instructions_provider
        test_instructions_provider.cpp
        ${CMAKE_SOURCE_DIR}/src/core/instructions_provider.cpp
    )
    target_include_directories(test_instructions_provider PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    set_target_properties(test_instructions_provider PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        CXX_STANDARD 20
    )
    if(WIN32)
        target_link_libraries(test_instructions_provider PRIVATE shell32 ole32)
    endif()
    add_test(NAME test_instructions_provider COMMAND test_instructions_provider)
    message(STATUS "  ✓ Added test_instructions_provider")
endif()

message(STATUS "========================================")
message(STATUS "Test Suite Registration Complete")
message(STATUS "Run tests: ctest --output-on-failure -C Release")
message(STATUS "Run specific: ctest -R <name> -V -C Release")
message(STATUS "========================================")

# Model loader/streamer smoke test (no external model required)
if(NOT TARGET test_model_loader_streamer_smoke AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_model_loader_streamer_smoke.cpp")
    add_executable(test_model_loader_streamer_smoke
        test_model_loader_streamer_smoke.cpp
        gguf_link_stubs.cpp
        ${CMAKE_SOURCE_DIR}/src/gguf_loader.cpp
        ${CMAKE_SOURCE_DIR}/src/gguf_vocab_resolver.cpp
        ${CMAKE_SOURCE_DIR}/src/streaming_gguf_loader.cpp
        ${CMAKE_SOURCE_DIR}/src/utils/Diagnostics.cpp
    )
    target_include_directories(test_model_loader_streamer_smoke PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/3rdparty
    )
    set_target_properties(test_model_loader_streamer_smoke PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        CXX_STANDARD 20
    )
    add_test(NAME test_model_loader_streamer_smoke COMMAND test_model_loader_streamer_smoke)
    message(STATUS "  ✓ Added test_model_loader_streamer_smoke")
endif()
