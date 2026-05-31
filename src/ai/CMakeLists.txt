# Minimal CMake configuration for testing streaming GGUF loader (C++20, Qt-free)

cmake_minimum_required(VERSION 3.16)
project(StreamingGGUFLoaderTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(AI_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Use parent src/ StreamingGGUFLoader (C++20, Qt-free)
add_executable(test_streaming_gguf_loader
    ${AI_DIR}/test_streaming_gguf_loader.cpp
    ${SRC_DIR}/streaming_gguf_loader.cpp
    ${SRC_DIR}/utils/Diagnostics.cpp
)
target_include_directories(test_streaming_gguf_loader PRIVATE ${AI_DIR} ${SRC_DIR})

# Link Windows libraries for memory mapping
if(WIN32)
    target_link_libraries(test_streaming_gguf_loader PRIVATE kernel32)
endif()

# Set output directory
set_target_properties(test_streaming_gguf_loader PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Benchmark runner executable
add_executable(benchmark_runner
    ${AI_DIR}/benchmark_runner.cpp
    ${AI_DIR}/final_production_pipeline.cpp
    ${AI_DIR}/production_pipeline.cpp
    ${AI_DIR}/model_residency.cpp
    ${AI_DIR}/kernel_switcher.cpp
    ${AI_DIR}/kv_cache_manager.cpp
    ${AI_DIR}/adaptive_debounce.cpp
    ${AI_DIR}/early_exit.cpp
    ${AI_DIR}/dual_stream_speculative.cpp
    ${AI_DIR}/prefix_pinning.cpp
    ${AI_DIR}/token_prefetch.cpp
    ${AI_DIR}/latency_profiler.cpp
    ${AI_DIR}/persistent_gpu_loop.cpp
    ${AI_DIR}/kv_paging.cpp
    ${AI_DIR}/async_overlap.cpp
    ${AI_DIR}/predictive_scheduler.cpp
    ${AI_DIR}/multi_model_arbitration.cpp
    ${AI_DIR}/context_heat_map.cpp
    ${AI_DIR}/stabilization_test_harness.cpp
    ${AI_DIR}/real_time_dashboard.cpp
    ${AI_DIR}/live_parameter_tuning.cpp
    ${AI_DIR}/streaming_inference_engine.cpp
    ${AI_DIR}/ide_completion_bridge.cpp
    ${AI_DIR}/copilot_pipeline.cpp
)
target_include_directories(benchmark_runner PRIVATE ${AI_DIR} ${SRC_DIR})
if(WIN32)
    target_link_libraries(benchmark_runner PRIVATE kernel32)
endif()
set_target_properties(benchmark_runner PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

message(STATUS "StreamingGGUFLoaderTest configured successfully")
message(STATUS "Build with: cmake --build build --config Release")
message(STATUS "Run with: ./build/bin/test_streaming_gguf_loader.exe <path-to-gguf>")
