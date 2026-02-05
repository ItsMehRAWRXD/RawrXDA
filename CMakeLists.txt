cmake_minimum_required(VERSION 3.20)
project(RawrEngine VERSION 7.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-D_CRT_SECURE_NO_WARNINGS -DNOMINMAX -DWIN32_LEAN_AND_MEAN)

find_package(Threads REQUIRED)

set(SOURCES 
    src/main.cpp
    src/stubs.cpp
    src/memory_core.cpp
    src/hot_patcher.cpp
    src/cpu_inference_engine.cpp
    src/complete_server.cpp
    src/streaming_gguf_loader.cpp
    src/gguf_loader.cpp
    src/gguf_vocab_resolver.cpp
    src/compression_interface.cpp
    
    src/engine/gguf_core.cpp
    src/engine/transformer.cpp
    src/engine/bpe_tokenizer.cpp
    src/engine/sampler.cpp
    src/engine/rawr_engine.cpp
    src/engine/core_generator.cpp
    
    src/codec/compression.cpp
    src/codec/brutal_gzip.cpp
)

add_executable(RawrEngine ${SOURCES})

target_include_directories(RawrEngine PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(RawrEngine PRIVATE Threads::Threads)

if(WIN32)
    target_link_libraries(RawrEngine PRIVATE shlwapi psapi dbghelp comctl32 comdlg32 shell32 ole32 oleaut32 uuid winhttp ws2_32)
endif()

if(NOT MSVC)
    target_compile_options(RawrEngine PRIVATE -march=native -O3 -w)
endif()

add_executable(rawrxd-monaco-gen
    src/monaco_gen.cpp
    src/engine/react_ide_generator.cpp
)

target_include_directories(rawrxd-monaco-gen PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(rawrxd-monaco-gen PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY bin
    OUTPUT_NAME rawrxd-monaco-gen
)