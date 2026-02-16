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

message(STATUS "StreamingGGUFLoaderTest configured successfully")
message(STATUS "Build with: cmake --build build --config Release")
message(STATUS "Run with: ./build/bin/test_streaming_gguf_loader.exe <path-to-gguf>")
