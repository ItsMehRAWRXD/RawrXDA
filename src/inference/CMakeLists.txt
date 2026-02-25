# =============================================================================
# RawrXD-InferenceEngine — Standalone Inference Binary (Phase 6)
# =============================================================================
#
# Produces RawrXD-InferenceEngine.exe — the Qt-free, GUI-independent
# inference executable. This closes the self-hosting loop: the agent can
# reason about code without relying on any GUI infrastructure.
#
# Build:
#   cmake -S . -B build_inference -DRAWR_STANDALONE_INFERENCE=1
#   cmake --build build_inference --config Release --target RawrXD-InferenceEngine
#
# Or from the root build:
#   cmake --build . --config Release --target RawrXD-InferenceEngine
#
# =============================================================================

cmake_minimum_required(VERSION 3.20)

# Inherit parent project settings if built as subdirectory,
# otherwise configure as standalone project.
if(NOT CMAKE_PROJECT_NAME)
    project(RawrXD-InferenceEngine VERSION 1.0.0 LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -DNOMINMAX -DWIN32_LEAN_AND_MEAN)
endif()

# =============================================================================
# MASM64 Kernel Support (when building under MSVC)
# =============================================================================
if(MSVC AND NOT DEFINED RAWR_HAS_MASM)
    enable_language(ASM_MASM)
    set(RAWR_HAS_MASM TRUE)
    set(CMAKE_ASM_MASM_FLAGS "/c /Zi /Zd /I${CMAKE_CURRENT_SOURCE_DIR}/../asm")
endif()

# =============================================================================
# Core Inference Sources
# =============================================================================
set(INFERENCE_CORE_SOURCES
    ultra_fast_inference.cpp
    polymorphic_loader.cpp
    ollama_blob_parser.cpp
    inference_standalone_main.cpp
    RawrXD_LlamaNative.cpp              # Native llama.dll bridge (zero HTTP)
)

# =============================================================================
# Engine subsystem sources (shared with RawrEngine)
# =============================================================================
set(ENGINE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/gguf_core.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/inference_kernels.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/transformer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/bpe_tokenizer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/sampler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/rawr_engine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/core_generator.cpp
)

# =============================================================================
# Minimal support sources (stubs for non-inference subsystems)
# =============================================================================
set(SUPPORT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../gguf_loader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../gguf_vocab_resolver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../compression_interface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../streaming_gguf_loader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../stubs.cpp
)

# =============================================================================
# MASM Kernel Objects (linked directly for maximum throughput)
# =============================================================================
if(RAWR_HAS_MASM)
    set(INFERENCE_ASM_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/../asm/inference_core.asm
        ${CMAKE_CURRENT_SOURCE_DIR}/../asm/inference_kernels.asm
        ${CMAKE_CURRENT_SOURCE_DIR}/../asm/quant_avx2.asm
        ${CMAKE_CURRENT_SOURCE_DIR}/../asm/RawrXD_KQuant_Dequant.asm
        ${CMAKE_CURRENT_SOURCE_DIR}/../asm/FlashAttention_AVX512.asm
        ${CMAKE_CURRENT_SOURCE_DIR}/../asm/model_bridge_x64.asm
        ${CMAKE_CURRENT_SOURCE_DIR}/../asm/RawrXD_Telemetry_Kernel.asm
    )
    set_source_files_properties(${INFERENCE_ASM_SOURCES} PROPERTIES LANGUAGE ASM_MASM)
else()
    set(INFERENCE_ASM_SOURCES "")
endif()

# =============================================================================
# Build the standalone inference executable
# =============================================================================
add_executable(RawrXD-InferenceEngine
    ${INFERENCE_CORE_SOURCES}
    ${ENGINE_SOURCES}
    ${SUPPORT_SOURCES}
    ${INFERENCE_ASM_SOURCES}
)

target_compile_definitions(RawrXD-InferenceEngine PRIVATE
    RAWR_STANDALONE_INFERENCE=1
    WIN64
    _WIN64
    UNICODE
    _UNICODE
    _CRT_SECURE_NO_WARNINGS
    $<$<BOOL:${RAWR_HAS_MASM}>:RAWR_HAS_MASM=1>
    $<$<BOOL:${RAWR_HAS_MASM}>:RAWRXD_LINK_TELEMETRY_KERNEL_ASM=1>
    $<$<BOOL:${RAWR_HAS_MASM}>:RAWRXD_LINK_INFERENCE_KERNELS_ASM=1>
)

target_include_directories(RawrXD-InferenceEngine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine
    ${CMAKE_CURRENT_SOURCE_DIR}/../core
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# =============================================================================
# Link NanoQuant kernel if available
# =============================================================================
if(TARGET RawrXD_NanoQuant)
    target_link_libraries(RawrXD-InferenceEngine PRIVATE RawrXD_NanoQuant)
    target_compile_definitions(RawrXD-InferenceEngine PRIVATE RAWR_HAS_NANOQUANT=1)
endif()

# =============================================================================
# Platform libraries
# =============================================================================
find_package(Threads REQUIRED)
target_link_libraries(RawrXD-InferenceEngine PRIVATE Threads::Threads)

if(WIN32)
    target_link_libraries(RawrXD-InferenceEngine PRIVATE
        kernel32 advapi32 shlwapi psapi dbghelp ws2_32 winhttp ntdll
    )
endif()

# =============================================================================
# Vulkan SDK (optional GPU acceleration)
# =============================================================================
find_package(Vulkan QUIET)
if(Vulkan_FOUND)
    target_include_directories(RawrXD-InferenceEngine PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(RawrXD-InferenceEngine PRIVATE Vulkan::Vulkan)
    target_compile_definitions(RawrXD-InferenceEngine PRIVATE RAWR_HAS_VULKAN=1)
else()
    target_compile_definitions(RawrXD-InferenceEngine PRIVATE RAWR_HAS_VULKAN=0)
endif()

# =============================================================================
# Compiler options
# =============================================================================
if(MSVC)
    target_compile_options(RawrXD-InferenceEngine PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:/std:c++20>
        $<$<COMPILE_LANGUAGE:CXX>:/W3>
        $<$<COMPILE_LANGUAGE:CXX>:/permissive->
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Release>>:/O2>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Release>>:/Oi>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Release>>:/Ot>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Release>>:/GL>
    )
    target_link_options(RawrXD-InferenceEngine PRIVATE
        $<$<CONFIG:Release>:/LTCG>
    )
else()
    target_compile_options(RawrXD-InferenceEngine PRIVATE
        -O3 -march=native -mavx2 -mfma -fopenmp
    )
endif()

message(STATUS "[Inference] RawrXD-InferenceEngine standalone build configured")
message(STATUS "[Inference] MASM kernels: ${RAWR_HAS_MASM}")
