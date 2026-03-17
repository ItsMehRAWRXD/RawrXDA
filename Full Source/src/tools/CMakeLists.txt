# Tools - Command-line utilities for benchmarking and testing

# RawrXD KeyGen — RSA-4096 Key Authority & License Signer (unlock 800B kernel / enterprise)
# Build: standalone Win32 CLI; no Qt/engine deps. Use for --genkey, --hwid, --issue, --sign.
add_executable(RawrXD_KeyGen WIN32
    RawrXD_KeyGen.cpp
)
target_link_libraries(RawrXD_KeyGen PRIVATE advapi32 crypt32)
if(MSVC)
  target_compile_options(RawrXD_KeyGen PRIVATE /W4)
endif()

# Simple GPU Test - For hotpatch testing framework
add_executable(simple_gpu_test
    simple_gpu_test.cpp
)

# Qt-free: pure C++ with STL
target_link_libraries(simple_gpu_test PRIVATE
    InferenceEngine
    ggml
)

# Real Multi-Model Benchmark - For comprehensive model testing
add_executable(real_multi_model_benchmark
    real_multi_model_benchmark.cpp
)

target_link_libraries(real_multi_model_benchmark PRIVATE
    InferenceEngine
    ggml
)

# GGUF API Server - Real HTTP API with GPU inference
add_executable(gguf_api_server
    ../gguf_api_server.cpp
)

target_link_libraries(gguf_api_server PRIVATE
    InferenceEngine
    ggml
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
