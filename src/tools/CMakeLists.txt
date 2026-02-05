# Tools - Command-line utilities for benchmarking and testing

# Simple GPU Test - For hotpatch testing framework
add_executable(simple_gpu_test
    simple_gpu_test.cpp
)

target_link_libraries(simple_gpu_test PRIVATE
    InferenceEngine
    ggml
    Qt${QT_VERSION_MAJOR}::Core
)

# Real Multi-Model Benchmark - For comprehensive model testing
add_executable(real_multi_model_benchmark
    real_multi_model_benchmark.cpp
)

target_link_libraries(real_multi_model_benchmark PRIVATE
    InferenceEngine
    ggml
    Qt${QT_VERSION_MAJOR}::Core
)

# GGUF API Server - Real HTTP API with GPU inference
add_executable(gguf_api_server
    ../gguf_api_server.cpp
)

target_link_libraries(gguf_api_server PRIVATE
    InferenceEngine
    ggml
    Qt${QT_VERSION_MAJOR}::Core
    ws2_32
)

target_include_directories(gguf_api_server PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

# Install targets
install(TARGETS simple_gpu_test real_multi_model_benchmark gguf_api_server
    RUNTIME DESTINATION bin
)
