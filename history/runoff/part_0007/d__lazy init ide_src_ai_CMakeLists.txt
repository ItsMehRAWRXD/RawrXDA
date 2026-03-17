# Minimal CMake configuration for testing streaming GGUF loader

cmake_minimum_required(VERSION 3.16)
project(StreamingGGUFLoaderTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add source files
add_executable(test_streaming_gguf_loader
    memory_mapped_file.cpp
    streaming_gguf_loader_qt.cpp
    test_streaming_gguf_loader.cpp
)

# Include current directory for headers
target_include_directories(test_streaming_gguf_loader PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# --- RawrXD Digestion Engine ---
find_package(Qt6 REQUIRED COMPONENTS Sql Concurrent Core)

set(DIGESTION_SOURCES
    digestion_engine.cpp
    digestion_engine.h
)

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    enable_language(ASM_MASM)
    list(APPEND DIGESTION_SOURCES digestion_scanner.asm)
    set_source_files_properties(digestion_scanner.asm 
        PROPERTIES COMPILE_FLAGS "/arch:AVX512")
endif()

add_library(RawrXDDigestion STATIC ${DIGESTION_SOURCES})
target_link_libraries(RawrXDDigestion 
    PUBLIC
    Qt6::Sql 
    Qt6::Concurrent
    Qt6::Core
)

target_include_directories(RawrXDDigestion PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

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
