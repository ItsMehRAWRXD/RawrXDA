cmake_minimum_required(VERSION 3.20)
project(RawrXD-Engine VERSION 7.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC specific flags for maximum performance
if(MSVC)
    add_compile_options(/O2 /arch:AVX2 /GL /W4 /EHsc /openmp)
    add_link_options(/LTCG)
else()
    add_compile_options(-O3 -march=native -Wall -fopenmp)
endif()

# Find dependencies
find_package(nlohmann_json CONFIG REQUIRED)
find_package(libzip CONFIG REQUIRED)
find_package(Threads REQUIRED)

# Source files
set(ENGINE_SOURCES
    src/main.cpp
    src/vsix_loader.cpp
    src/memory_core.cpp
    src/hot_patcher.cpp
    src/engine/gguf_core.cpp
    src/engine/inference_kernels.cpp
    src/engine/transformer.cpp
    src/engine/bpe_tokenizer.cpp
    src/engine/sampler.cpp
    src/engine/rawr_engine.cpp
    src/engine/universal_generator.cpp
    src/engine/react_ide_generator.cpp
    src/engine/core_generator.cpp
)

set(ENGINE_HEADERS
    src/vsix_loader.h
    src/memory_core.h
    src/hot_patcher.h
    src/engine/common_types.h
    src/engine/gguf_core.h
    src/engine/inference_kernels.h
    src/engine/transformer.h
    src/engine/bpe_tokenizer.h
    src/engine/sampler.h
    src/engine/rawr_engine.h
    src/engine/universal_generator.h
    src/engine/react_ide_generator.h
    src/engine/core_generator.h
)

# Main executable
add_executable(RawrEngine ${ENGINE_SOURCES} ${ENGINE_HEADERS})

# Include directories
target_include_directories(RawrEngine PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine
)

# Link libraries
target_link_libraries(RawrEngine PRIVATE
    nlohmann_json::nlohmann_json
    libzip::zip
    Threads::Threads
)

# Windows-specific libraries
if(WIN32)
    target_link_libraries(RawrEngine PRIVATE
        shlwapi
        psapi
        dbghelp
    )
endif()

# Define version macros
target_compile_definitions(RawrEngine PRIVATE
    RAWER_ENGINE_VERSION="7.0.0"
    RAWER_ENGINE_REV="7"
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

# Set output properties
set_target_properties(RawrEngine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME "RawrEngine"
)
