# tests/native/CMakeLists.txt
cmake_minimum_required(VERSION 3.20)

# Native loader tests
add_executable(test_native_loader test_gguf_loader.cpp)
target_link_libraries(test_native_loader PRIVATE RawrXD_Native)
add_test(NAME NativeLoaderTest COMMAND test_native_loader)

# Quantization tests
add_executable(test_quantization test_quantization.cpp)
target_link_libraries(test_quantization PRIVATE RawrXD_Native)
add_test(NAME QuantizationTest COMMAND test_quantization)

# Metrics server tests
add_executable(test_metrics test_metrics_server.cpp)
target_link_libraries(test_metrics PRIVATE RawrXD_Native)
add_test(NAME MetricsTest COMMAND test_metrics)