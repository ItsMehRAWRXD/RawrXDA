cmake_minimum_required(VERSION 3.20)

# RawrXD_CLI is now built in parent CMakeLists with full agentic parity (chat + tools + Ollama).
# This subdir provides StressTest and other utilities only.

# Stress Test Executable  
add_executable(RawrXD_StressTest stress_test.cpp)
target_include_directories(RawrXD_StressTest PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(RawrXD_StressTest PRIVATE ws2_32)

# Install targets
install(TARGETS RawrXD_StressTest RUNTIME DESTINATION bin)
