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
# - sovereign_assembler_smoke

cmake_minimum_required(VERSION 3.20)

# ── GoogleTest (header-only fetch so gtest/gtest.h is available) ──────────────
include(FetchContent)
FetchContent_Declare(
    googletest
    URL "https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
# Don't install gtest alongside the project
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
# ─────────────────────────────────────────────────────────────────────────────

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

if(TARGET agent_workflow_orchestrator_smoke)
    add_test(NAME agent_workflow_orchestrator_smoke COMMAND agent_workflow_orchestrator_smoke)
    message(STATUS "  ✓ Registered agent_workflow_orchestrator_smoke")
endif()

# ============================================================
# NEW TESTS - Qt-free, C++20 native test framework
# ============================================================

# AI Completions Test
if(NOT TARGET test_ai_completions AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_ai_completions.cpp")
    add_executable(test_ai_completions
        test_ai_completions.cpp
        real_time_completion_engine_test_impl.cpp
    )
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

# Voice Processor Test intentionally disabled to remove microphone capture dependencies from builds.

# Zero Retention Manager Test
if(NOT TARGET test_zero_retention_manager AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_zero_retention_manager.cpp")
    add_executable(test_zero_retention_manager
        test_zero_retention_manager.cpp
        ${CMAKE_SOURCE_DIR}/src/terminal/zero_retention_manager.cpp
    )
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

# IDE-Linked Autonomy Smoke Test
if(NOT TARGET test_ide_model_autonomy AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/smoke/test_ide_model_autonomy.cpp")
    add_executable(test_ide_model_autonomy
        smoke/test_ide_model_autonomy.cpp
        smoke/ide_autonomy_shield_stubs.cpp
        ${CMAKE_SOURCE_DIR}/src/core/feature_handlers_model_load.cpp
    )
    target_include_directories(test_ide_model_autonomy PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(test_ide_model_autonomy PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        CXX_STANDARD 20
        # Global /LTCG can drop COMDATs from small helper TUs; keep this test a normal link.
        INTERPROCEDURAL_OPTIMIZATION OFF
    )
    if(MSVC)
        target_compile_options(test_ide_model_autonomy PRIVATE /GL-)
    endif()
    if(WIN32)
       target_link_libraries(test_ide_model_autonomy PRIVATE ws2_32 winhttp psapi)
    endif()
    add_test(NAME test_ide_model_autonomy COMMAND test_ide_model_autonomy)
    message(STATUS "  ✓ Added test_ide_model_autonomy")
endif()

# Extreme memory optimizer smoke test
if(NOT TARGET test_extreme_memory_optimizer AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/smoke/test_extreme_memory_optimizer.cpp")
    add_executable(test_extreme_memory_optimizer
        smoke/test_extreme_memory_optimizer.cpp
    )
    target_include_directories(test_extreme_memory_optimizer PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(test_extreme_memory_optimizer PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        CXX_STANDARD 20
    )
    add_test(NAME test_extreme_memory_optimizer COMMAND test_extreme_memory_optimizer)
    message(STATUS "  ✓ Added test_extreme_memory_optimizer")
endif()

# Quality corrector smoke test
if(NOT TARGET test_quality_corrector AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/smoke/test_quality_corrector.cpp")
    add_executable(test_quality_corrector
        smoke/test_quality_corrector.cpp
    )
    target_include_directories(test_quality_corrector PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(test_quality_corrector PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        CXX_STANDARD 20
    )
    add_test(NAME test_quality_corrector COMMAND test_quality_corrector)
    message(STATUS "  ✓ Added test_quality_corrector")
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

# Split-Answers Toggle Smoke Test
if(NOT TARGET smoke_split_answers_toggle AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/smoke_split_answers_toggle.cpp")
    add_executable(smoke_split_answers_toggle
        smoke_split_answers_toggle.cpp
        ${CMAKE_SOURCE_DIR}/src/backend/agentic_tools.cpp
        ${CMAKE_SOURCE_DIR}/tests/stubs/ollama_client_stub.cpp
        ${CMAKE_SOURCE_DIR}/tests/stubs/agent_tool_registry_stub.cpp
        ${CMAKE_SOURCE_DIR}/src/tools/git_client.cpp
        ${CMAKE_SOURCE_DIR}/src/tools/file_ops.cpp
    )
    target_include_directories(smoke_split_answers_toggle PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
    set_target_properties(smoke_split_answers_toggle PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" CXX_STANDARD 20)
    add_test(NAME smoke_split_answers_toggle COMMAND smoke_split_answers_toggle)
    message(STATUS "  ✓ Added smoke_split_answers_toggle")
endif()

# Agentic Repair Stress Test — Self-Healing hotpatch lifecycle
if(NOT TARGET agentic_repair_stress_test AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/agentic_repair_stress_test.cpp")
    add_executable(agentic_repair_stress_test agentic_repair_stress_test.cpp)
    target_include_directories(agentic_repair_stress_test PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(agentic_repair_stress_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" CXX_STANDARD 20)
    add_test(NAME agentic_repair_stress_test COMMAND agentic_repair_stress_test)
    message(STATUS "  ✓ Added agentic_repair_stress_test")
endif()

# Adaptive Credit Governor Test — Weight-Aware multi-modal tuning
if(NOT TARGET test_adaptive_credit_governor AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_adaptive_credit_governor.cpp")
    add_executable(test_adaptive_credit_governor
        test_adaptive_credit_governor.cpp
        ${CMAKE_SOURCE_DIR}/src/flow_control/adaptive_credit_governor.cpp
        ${CMAKE_SOURCE_DIR}/src/flow_control/credit_governor.cpp
        ${CMAKE_SOURCE_DIR}/src/flow_control/credit_based_flow_control.cpp
    )
    target_include_directories(test_adaptive_credit_governor PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(test_adaptive_credit_governor PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" CXX_STANDARD 20)
    add_test(NAME test_adaptive_credit_governor COMMAND test_adaptive_credit_governor)
    message(STATUS "  ✓ Added test_adaptive_credit_governor")
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
    target_link_libraries(test_gguf_integration PRIVATE GTest::gtest GTest::gtest_main)
    add_test(NAME test_gguf_integration COMMAND test_gguf_integration)
    message(STATUS "  ✓ Added test_gguf_integration")
endif()

if(NOT TARGET test_engine_revolver AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_engine_revolver.cpp")
    add_executable(test_engine_revolver test_engine_revolver.cpp ${CMAKE_SOURCE_DIR}/src/core/engine_revolver.cpp)
    target_include_directories(test_engine_revolver PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(test_engine_revolver PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" CXX_STANDARD 23)
    add_test(NAME test_engine_revolver COMMAND test_engine_revolver)
    message(STATUS "  ✓ Added test_engine_revolver")
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
        ${CMAKE_SOURCE_DIR}/src/logging/Logger.cpp
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
        ${CMAKE_SOURCE_DIR}/src/logging/Logger.cpp
    )
    target_include_directories(test_batch3_profiler_smoke PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(test_batch3_profiler_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" CXX_STANDARD 20)
    add_test(NAME test_batch3_profiler_smoke COMMAND test_batch3_profiler_smoke)
    message(STATUS "  ✓ Added test_batch3_profiler_smoke")
endif()

# State Subscription Engine Guarantees Smoke Test
if(NOT TARGET test_state_subscription_engine_smoke AND EXISTS "${CMAKE_SOURCE_DIR}/src/tests/state_subscription_engine_smoke.cpp")
    add_executable(test_state_subscription_engine_smoke
        ${CMAKE_SOURCE_DIR}/src/tests/state_subscription_engine_smoke.cpp
    )
    target_include_directories(test_state_subscription_engine_smoke PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    set_target_properties(test_state_subscription_engine_smoke PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        CXX_STANDARD 20
    )
    add_test(NAME test_state_subscription_engine_smoke COMMAND test_state_subscription_engine_smoke)
    message(STATUS "  ✓ Added test_state_subscription_engine_smoke")
endif()

# AI Execution Guardrails Smoke Test
if(NOT TARGET test_ai_execution_guardrails_smoke AND EXISTS "${CMAKE_SOURCE_DIR}/src/tests/ai_execution_guardrails_smoke.cpp")
    add_executable(test_ai_execution_guardrails_smoke
        ${CMAKE_SOURCE_DIR}/src/tests/ai_execution_guardrails_smoke.cpp
        ${CMAKE_SOURCE_DIR}/src/core/ai_execution_guardrails.cpp
    )
    target_include_directories(test_ai_execution_guardrails_smoke PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    set_target_properties(test_ai_execution_guardrails_smoke PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        CXX_STANDARD 20
    )
    add_test(NAME test_ai_execution_guardrails_smoke COMMAND test_ai_execution_guardrails_smoke)
    message(STATUS "  ✓ Added test_ai_execution_guardrails_smoke")
endif()

# AI Execution Guardrails Chaos Test
if(NOT TARGET test_ai_execution_guardrails_chaos AND EXISTS "${CMAKE_SOURCE_DIR}/src/tests/ai_execution_guardrails_chaos.cpp")
    add_executable(test_ai_execution_guardrails_chaos
        ${CMAKE_SOURCE_DIR}/src/tests/ai_execution_guardrails_chaos.cpp
        ${CMAKE_SOURCE_DIR}/src/core/ai_execution_guardrails.cpp
    )
    target_include_directories(test_ai_execution_guardrails_chaos PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    set_target_properties(test_ai_execution_guardrails_chaos PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        CXX_STANDARD 20
    )
    add_test(NAME test_ai_execution_guardrails_chaos COMMAND test_ai_execution_guardrails_chaos)
    message(STATUS "  ✓ Added test_ai_execution_guardrails_chaos")
endif()

# Safe Inference Session Integration Test
if(NOT TARGET test_safe_inference_session_integration AND EXISTS "${CMAKE_SOURCE_DIR}/src/tests/safe_inference_session_integration.cpp")
    add_executable(test_safe_inference_session_integration
        ${CMAKE_SOURCE_DIR}/src/tests/safe_inference_session_integration.cpp
        ${CMAKE_SOURCE_DIR}/src/core/safe_inference_session.cpp
        ${CMAKE_SOURCE_DIR}/src/core/ai_execution_guardrails.cpp
    )
    target_include_directories(test_safe_inference_session_integration PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    set_target_properties(test_safe_inference_session_integration PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        CXX_STANDARD 20
    )
    add_test(NAME test_safe_inference_session_integration COMMAND test_safe_inference_session_integration)
    message(STATUS "  ✓ Added test_safe_inference_session_integration")
endif()

# ============================================================================
# NEW TESTS — Phase 2 Extension Host + Sovereign Inference
# ============================================================================

# KV Cache Ownership Test
if(NOT TARGET test_kv_cache_ownership AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/inference/test_kv_cache_ownership.cpp")
    add_executable(test_kv_cache_ownership
        inference/test_kv_cache_ownership.cpp
        ${CMAKE_SOURCE_DIR}/src/inference/kv_cache_ownership.cpp
    )
    target_include_directories(test_kv_cache_ownership PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(test_kv_cache_ownership PRIVATE GTest::gtest GTest::gtest_main)
    set_target_properties(test_kv_cache_ownership PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        CXX_STANDARD 20
    )
    add_test(NAME test_kv_cache_ownership COMMAND test_kv_cache_ownership)
    message(STATUS "  ✓ Added test_kv_cache_ownership")
endif()

# Speculative Pipeline Test
if(NOT TARGET test_speculative_pipeline AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/inference/test_speculative_pipeline.cpp")
    add_executable(test_speculative_pipeline
        inference/test_speculative_pipeline.cpp
        ${CMAKE_SOURCE_DIR}/src/inference/speculative_pipeline.cpp
    )
    target_include_directories(test_speculative_pipeline PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(test_speculative_pipeline PRIVATE GTest::gtest GTest::gtest_main)
    set_target_properties(test_speculative_pipeline PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        CXX_STANDARD 20
    )
    add_test(NAME test_speculative_pipeline COMMAND test_speculative_pipeline)
    message(STATUS "  ✓ Added test_speculative_pipeline")
endif()

# Phase 3: INT4 KV Bridge Integration Test
if(NOT TARGET test_int4kv_bridge_integration AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_int4kv_bridge_integration.cpp")
    add_executable(test_int4kv_bridge_integration
        test_int4kv_bridge_integration.cpp
        ${CMAKE_SOURCE_DIR}/src/inference/inference_int4kv_bridge.cpp
        ${CMAKE_SOURCE_DIR}/src/compression/virtualalloc_reservation_manager.cpp
        ${CMAKE_SOURCE_DIR}/src/compression/int4_kv_dequant_kernel.cpp
        ${CMAKE_SOURCE_DIR}/src/compression/iq2m_dequant_kernel.cpp
        ${CMAKE_SOURCE_DIR}/src/engine/inference_kernels.cpp
    )
    target_include_directories(test_int4kv_bridge_integration PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(test_int4kv_bridge_integration PRIVATE)
    set_target_properties(test_int4kv_bridge_integration PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        CXX_STANDARD 20
    )
    if(WIN32)
        target_link_libraries(test_int4kv_bridge_integration PRIVATE kernel32)
    endif()
    add_test(NAME test_int4kv_bridge_integration COMMAND test_int4kv_bridge_integration)
    message(STATUS "  ✓ Added test_int4kv_bridge_integration (Phase 3 bridge validation)")
endif()

# Phase 4: IQ2_M 2.5-bit Dequantization Kernel Test
if(NOT TARGET test_iq2m_dequant_kernel AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_iq2m_dequant_kernel.cpp")
    add_executable(test_iq2m_dequant_kernel
        test_iq2m_dequant_kernel.cpp
        ${CMAKE_SOURCE_DIR}/src/compression/iq2m_dequant_kernel.cpp
    )
    target_include_directories(test_iq2m_dequant_kernel PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(test_iq2m_dequant_kernel PRIVATE)
    set_target_properties(test_iq2m_dequant_kernel PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        CXX_STANDARD 20
    )
    if(WIN32)
        target_link_libraries(test_iq2m_dequant_kernel PRIVATE kernel32)
    endif()
    add_test(NAME test_iq2m_dequant_kernel COMMAND test_iq2m_dequant_kernel)
    message(STATUS "  ✓ Added test_iq2m_dequant_kernel (Phase 4 IQ2_M kernel validation)")
endif()

# Extension Host Test
if(NOT TARGET test_extension_host AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/win32app/test_extension_host.cpp")
    add_executable(test_extension_host
        win32app/test_extension_host.cpp
        ${CMAKE_SOURCE_DIR}/src/win32app/ExtensionHost.cpp
        ${CMAKE_SOURCE_DIR}/src/win32app/ExtensionHost_Discovery.cpp
        ${CMAKE_SOURCE_DIR}/src/win32app/ExtensionHost_VSCodeAPIs.cpp
        ${CMAKE_SOURCE_DIR}/src/win32app/ExtensionAPI_VSCode.cpp
        ${CMAKE_SOURCE_DIR}/src/win32app/ExtensionHostProcess.cpp
        ${CMAKE_SOURCE_DIR}/src/win32app/ExtensionSandboxManager.cpp
        ${CMAKE_SOURCE_DIR}/src/win32app/IPC_Channel.cpp
    )
    target_include_directories(test_extension_host PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/win32app
        ${CMAKE_SOURCE_DIR}/src/extensions
    )
    target_link_libraries(test_extension_host PRIVATE GTest::gtest GTest::gtest_main)
    set_target_properties(test_extension_host PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        CXX_STANDARD 20
    )
    if(WIN32)
        target_link_libraries(test_extension_host PRIVATE shell32 ole32)
    endif()
    add_test(NAME test_extension_host COMMAND test_extension_host)
    message(STATUS "  ✓ Added test_extension_host")
endif()

# Extension Host Smoke Test (Phase 2 - process isolation + sandbox)
if(NOT TARGET extension_ipc_echo_stub AND EXISTS "${CMAKE_SOURCE_DIR}/src/tests/extension_ipc_echo_stub.cpp")
    add_executable(extension_ipc_echo_stub
        ${CMAKE_SOURCE_DIR}/src/tests/extension_ipc_echo_stub.cpp
    )
    set_target_properties(extension_ipc_echo_stub PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        CXX_STANDARD 20
    )
endif()

if(NOT TARGET extension_host_smoke_test AND EXISTS "${CMAKE_SOURCE_DIR}/src/tests/extension_host_smoke_test.cpp")
    add_executable(extension_host_smoke_test
        ${CMAKE_SOURCE_DIR}/src/tests/extension_host_smoke_test.cpp
        ${CMAKE_SOURCE_DIR}/src/extensions/process_broker.cpp
        ${CMAKE_SOURCE_DIR}/src/extensions/security_sandbox.cpp
        ${CMAKE_SOURCE_DIR}/src/extensions/vscode_api_bridge.cpp
        ${CMAKE_SOURCE_DIR}/src/win32app/ExtensionAPI_VSCode.cpp
    )
    target_include_directories(extension_host_smoke_test PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/extensions
    )
    set_target_properties(extension_host_smoke_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        CXX_STANDARD 20
    )
    if(WIN32)
        target_link_libraries(extension_host_smoke_test PRIVATE psapi)
    endif()
    if(TARGET extension_ipc_echo_stub)
        add_dependencies(extension_host_smoke_test extension_ipc_echo_stub)
    endif()
    add_test(NAME extension_host_smoke_test COMMAND extension_host_smoke_test)
    message(STATUS "  ✓ Added extension_host_smoke_test")
endif()

# ============================================================================
# Batch-4 smoke: IPC, Speculative Pipeline, AST Completions, Arbitration
# ============================================================================
if(NOT TARGET batch4_ipc_spec_ast_arb_smoke AND
   EXISTS "${CMAKE_SOURCE_DIR}/src/tests/batch4_ipc_spec_ast_arb_smoke.cpp")
    add_executable(batch4_ipc_spec_ast_arb_smoke
        ${CMAKE_SOURCE_DIR}/src/tests/batch4_ipc_spec_ast_arb_smoke.cpp
        ${CMAKE_SOURCE_DIR}/src/lsp/treesitter_parser.cpp
        ${CMAKE_SOURCE_DIR}/src/lsp/completion_provider.cpp
    )
    target_include_directories(batch4_ipc_spec_ast_arb_smoke PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
    )
    set_target_properties(batch4_ipc_spec_ast_arb_smoke PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )
    if(WIN32)
        target_link_libraries(batch4_ipc_spec_ast_arb_smoke PRIVATE kernel32)
    endif()
    add_test(NAME batch4_ipc_spec_ast_arb_smoke COMMAND batch4_ipc_spec_ast_arb_smoke)
    message(STATUS "  ✓ Added batch4_ipc_spec_ast_arb_smoke")
endif()

# ============================================================================
# Extension-kernel wire smoke (shared protocol + shm bi-channel roundtrip)
# ============================================================================
if(NOT TARGET autocomplete_kernel_wire_smoke AND
   EXISTS "${CMAKE_SOURCE_DIR}/src/tests/autocomplete_kernel_wire_smoke.cpp")
    add_executable(autocomplete_kernel_wire_smoke
        ${CMAKE_SOURCE_DIR}/src/tests/autocomplete_kernel_wire_smoke.cpp
    )
    target_include_directories(autocomplete_kernel_wire_smoke PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
    )
    set_target_properties(autocomplete_kernel_wire_smoke PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )
    if(WIN32)
        target_link_libraries(autocomplete_kernel_wire_smoke PRIVATE kernel32)
    endif()
    add_test(NAME autocomplete_kernel_wire_smoke COMMAND autocomplete_kernel_wire_smoke)
    message(STATUS "  ✓ Added autocomplete_kernel_wire_smoke")
endif()

# ============================================================================
# Compile-only guard: fused autocomplete kernel TU must compile in-tree
# ============================================================================
if(NOT TARGET autocomplete_kernel_compile_guard AND
   EXISTS "${CMAKE_SOURCE_DIR}/src/extension_kernel/autocomplete_kernel.cpp")
    add_library(autocomplete_kernel_compile_guard OBJECT
        ${CMAKE_SOURCE_DIR}/src/extension_kernel/autocomplete_kernel.cpp
    )
    target_include_directories(autocomplete_kernel_compile_guard PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
    )
    set_target_properties(autocomplete_kernel_compile_guard PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )
    message(STATUS "  ✓ Added autocomplete_kernel_compile_guard")
endif()

# ============================================================================
# Cross-repo router smoke: federated local indexes + bounded top-K routing
# ============================================================================
if(NOT TARGET cross_repo_router_smoke AND
   EXISTS "${CMAKE_SOURCE_DIR}/src/tests/cross_repo_router_smoke.cpp")
    add_executable(cross_repo_router_smoke
        ${CMAKE_SOURCE_DIR}/src/tests/cross_repo_router_smoke.cpp
    )
    target_include_directories(cross_repo_router_smoke PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
    )
    set_target_properties(cross_repo_router_smoke PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )
    add_test(NAME cross_repo_router_smoke COMMAND cross_repo_router_smoke)
    message(STATUS "  ✓ Added cross_repo_router_smoke")
endif()

# ============================================================================
# P95 latency harness: tail-latency gate for shared-memory completion path
# ============================================================================
if(NOT TARGET autocomplete_p95_latency_harness AND
   EXISTS "${CMAKE_SOURCE_DIR}/src/tests/autocomplete_p95_latency_harness.cpp")
    add_executable(autocomplete_p95_latency_harness
        ${CMAKE_SOURCE_DIR}/src/tests/autocomplete_p95_latency_harness.cpp
    )
    target_include_directories(autocomplete_p95_latency_harness PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
    )
    set_target_properties(autocomplete_p95_latency_harness PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )
    if(WIN32)
        target_link_libraries(autocomplete_p95_latency_harness PRIVATE kernel32)
    endif()
    add_test(NAME autocomplete_p95_latency_harness COMMAND autocomplete_p95_latency_harness)
    message(STATUS "  ✓ Added autocomplete_p95_latency_harness")
endif()

# ============================================================================
# Titan Rx hot-path smoke: submit -> step -> shared-channel block commit
# ============================================================================
if(TARGET RawrXD_TitanABI AND
   NOT TARGET titan_rx_hotpath_smoke AND
   EXISTS "${CMAKE_SOURCE_DIR}/src/tests/titan_rx_hotpath_smoke.cpp")
    add_executable(titan_rx_hotpath_smoke
        ${CMAKE_SOURCE_DIR}/src/tests/titan_rx_hotpath_smoke.cpp
    )
    target_include_directories(titan_rx_hotpath_smoke PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
    )
    target_link_libraries(titan_rx_hotpath_smoke PRIVATE RawrXD_TitanABI)
    set_target_properties(titan_rx_hotpath_smoke PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )
    add_test(NAME titan_rx_hotpath_smoke COMMAND titan_rx_hotpath_smoke)
    message(STATUS "  ✓ Added titan_rx_hotpath_smoke")
endif()

# ============================================================================
# Sovereign ASM Kernel Tests (GoogleTest v1.14.0)
# ============================================================================

# ASM Kernel Accuracy + Performance Test Suite
if(NOT TARGET test_sovereign_kernels AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/asm/test_sovereign_kernels.cpp")
    # ASM sources to compile alongside the test
    # NOTE: SovereignSpeculativeEngine.asm requires LlamaNativeBridge C++ symbols
    # which are not available in the test target. We compile only the standalone
    # engine and kernel functions for unit testing.
    set(TEST_ASM_SOURCES
        ${CMAKE_SOURCE_DIR}/src/asm/sovereign_kernels.asm
        ${CMAKE_SOURCE_DIR}/src/asm/SovereignStandaloneEngine.asm
    )
    if(RAWR_HAS_MASM)
        set_source_files_properties(${TEST_ASM_SOURCES} PROPERTIES LANGUAGE ASM_MASM)
    endif()
    
    add_executable(test_sovereign_kernels
        asm/test_sovereign_kernels.cpp
        ${TEST_ASM_SOURCES}
    )
    target_include_directories(test_sovereign_kernels PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/asm
    )
    target_link_libraries(test_sovereign_kernels PRIVATE
        GTest::gtest
        GTest::gtest_main
    )
    target_compile_options(test_sovereign_kernels PRIVATE
        /arch:AVX2
        /fp:precise
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:RelWithDebInfo>:/O2>
    )
    set_target_properties(test_sovereign_kernels PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )
    add_test(NAME test_sovereign_kernels COMMAND test_sovereign_kernels)
    message(STATUS "  ✓ Added test_sovereign_kernels")
endif()

# ASM Kernel Benchmark Target (separate, longer running)
if(NOT TARGET bench_sovereign_kernels AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/asm/test_sovereign_kernels.cpp")
    add_executable(bench_sovereign_kernels
        asm/test_sovereign_kernels.cpp
        ${TEST_ASM_SOURCES}
    )
    target_include_directories(bench_sovereign_kernels PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/asm
    )
    target_link_libraries(bench_sovereign_kernels PRIVATE
        GTest::gtest
        GTest::gtest_main
    )
    target_compile_options(bench_sovereign_kernels PRIVATE
        /arch:AVX2
        /fp:precise
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:RelWithDebInfo>:/O2>
    )
    target_compile_definitions(bench_sovereign_kernels PRIVATE
        SOVEREIGN_BENCHMARK_MODE=1
    )
    set_target_properties(bench_sovereign_kernels PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )
    add_test(NAME bench_sovereign_kernels COMMAND bench_sovereign_kernels)
    message(STATUS "  ✓ Added bench_sovereign_kernels")
endif()

# ============================================================
# Test Summary
# ============================================================

# Production Regression Test Suite (Qt-free, no external deps)
if(NOT TARGET test_production_regression AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_production_regression.cpp")
    add_executable(test_production_regression
        test_production_regression.cpp
        ${CMAKE_SOURCE_DIR}/src/agentic_observability.cpp
        ${CMAKE_SOURCE_DIR}/src/agentic_error_handler.cpp
        ${CMAKE_SOURCE_DIR}/src/agentic_loop_state.cpp
        ${CMAKE_SOURCE_DIR}/src/error_recovery_system.cpp
        ${CMAKE_SOURCE_DIR}/src/centralized_exception_handler.cpp
        ${CMAKE_SOURCE_DIR}/src/production_config_manager.cpp
    )
    target_include_directories(test_production_regression PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
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

if(TARGET sovereign_assembler_smoke)
    add_test(NAME sovereign_assembler_smoke COMMAND sovereign_assembler_smoke)
    message(STATUS "  ✓ Registered sovereign_assembler_smoke (SovereignAssembler PE smoke)")
endif()

# Lane B RawrEngine: headless Copilot parity (minimal GGUF + AgenticBridge + NativeAgent)
if(TARGET RawrEngine)
    add_test(
        NAME rawrengine_copilot_smoke
        COMMAND $<TARGET_FILE:RawrEngine> --copilot-smoke
    )
    set_tests_properties(rawrengine_copilot_smoke PROPERTIES LABELS "smoke;copilot;inference" TIMEOUT 600)
    message(STATUS "  ✓ Registered rawrengine_copilot_smoke (RawrEngine --copilot-smoke)")
endif()

# Win32 IDE: headless agentic smoke (tool registry + optional live Ollama); exits before main window when flag present.
if(TARGET RawrXD-Win32IDE)
    add_test(
        NAME win32ide_agentic_smoke
        COMMAND $<TARGET_FILE:RawrXD-Win32IDE> --agentic-smoke
    )
    set_tests_properties(win32ide_agentic_smoke PROPERTIES LABELS "smoke;agentic;win32ide" TIMEOUT 600)
    message(STATUS "  ✓ Registered win32ide_agentic_smoke (RawrXD-Win32IDE --agentic-smoke)")

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/smoke/win32ide_high_pressure_ui.ps1")
        add_test(
            NAME win32ide_high_pressure_ui
            COMMAND pwsh -NoProfile -ExecutionPolicy Bypass
                -File ${CMAKE_CURRENT_SOURCE_DIR}/smoke/win32ide_high_pressure_ui.ps1
                -ExePath $<TARGET_FILE:RawrXD-Win32IDE>
        )
        set_tests_properties(win32ide_high_pressure_ui PROPERTIES LABELS "smoke;stress;win32ide;lsp;model" TIMEOUT 300)
        message(STATUS "  ✓ Registered win32ide_high_pressure_ui (Win32IDE concurrent UI/LSP/model pressure)")
    endif()
endif()

# AgentOllamaClient RAWRXD_STREAM_* routing parity (no IDE binary; source assertions + env label matrix)
if(EXISTS "${CMAKE_SOURCE_DIR}/scripts/Test-OllamaStreamRoutingEnv.ps1")
    add_test(
        NAME ollama_stream_routing_env_smoke
        COMMAND pwsh -NoProfile -ExecutionPolicy Bypass
            -File ${CMAKE_SOURCE_DIR}/scripts/Test-OllamaStreamRoutingEnv.ps1
    )
    set_tests_properties(ollama_stream_routing_env_smoke PROPERTIES LABELS "smoke;agentic;ollama" TIMEOUT 120)
    message(STATUS "  ✓ Registered ollama_stream_routing_env_smoke (RAWRXD_STREAM_* routing)")
endif()

# 14-day expansion aggregate quality gate entrypoint for CTest-driven CI/task runs.
# This wires the existing self_test_gate target into a named CTest so callers can run:
#   ctest -R production_readiness_expansion_quality_gate
if(TARGET self_test_gate)
    add_test(
        NAME production_readiness_expansion_quality_gate
        COMMAND ${CMAKE_COMMAND}
            -D BINARY_DIR=${CMAKE_BINARY_DIR}
            -D CONFIG=$<CONFIG>
            -D BUILD_TARGET=self_test_gate
            -P ${CMAKE_CURRENT_SOURCE_DIR}/smoke/build_target_with_retry.cmake
    )
    set_tests_properties(production_readiness_expansion_quality_gate PROPERTIES
        LABELS "quality-gate;production;aggregate"
        TIMEOUT 1800
    )
    message(STATUS "  ✓ Registered production_readiness_expansion_quality_gate (retry-hardened build of self_test_gate)")
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
