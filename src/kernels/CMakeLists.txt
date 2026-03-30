# d:\rawrxd\src\kernels\CMakeLists.txt
# AVX-512 Semantic Search Kernels - MASM Assembly Layer

# Detect MASM assembler
if(NOT CMAKE_ASM_MASM_COMPILER)
    find_program(CMAKE_ASM_MASM_COMPILER ml64.exe)
    if(NOT CMAKE_ASM_MASM_COMPILER)
        message(WARNING "ml64.exe not found; MASM kernels will not be compiled")
        message(STATUS "To enable MASM kernels, set CMAKE_ASM_MASM_COMPILER to the path of ml64.exe")
    endif()
endif()

# Enable MASM language
enable_language(ASM_MASM OPTIONAL)

# ============================================================================
# MASM Kernel Object Compilation
# ============================================================================

option(RAWRXD_ENABLE_EXPERIMENTAL_AVX512_KERNELS "Build experimental AVX-512 MASM kernels" OFF)

if(CMAKE_ASM_MASM_COMPILER AND RAWRXD_ENABLE_EXPERIMENTAL_AVX512_KERNELS)
    set(ASM_KERNEL_SRC  "${CMAKE_CURRENT_SOURCE_DIR}/RawrXD_AVX512_Semantic.asm")
    set(ASM_KERNEL_OBJ  "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_AVX512_Semantic.obj")
    
    # MASM compiler flags
    set(ASM_MASM_FLAGS
        /c
        /Zi
        /W4
        /errorReport:queue
    )
    
    # Custom command: Assemble MASM to object file
    add_custom_command(
        OUTPUT ${ASM_KERNEL_OBJ}
        COMMAND ${CMAKE_ASM_MASM_COMPILER} ${ASM_MASM_FLAGS}
                /Fo${ASM_KERNEL_OBJ} ${ASM_KERNEL_SRC}
        DEPENDS ${ASM_KERNEL_SRC}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Assembling AVX-512 semantic search kernels (MASM)..."
        VERBATIM
    )
    
    # Create target for kernel assembly
    add_custom_target(
        AVX512_Kernels ALL
        DEPENDS ${ASM_KERNEL_OBJ}
        SOURCES ${ASM_KERNEL_SRC}
    )
    
    # Export object file path for parent to link
    set(AVX512_KERNEL_OBJECT ${ASM_KERNEL_OBJ} PARENT_SCOPE)
    set(AVX512_KERNELS_FOUND TRUE PARENT_SCOPE)
    
else()
    set(AVX512_KERNELS_FOUND FALSE PARENT_SCOPE)
    if(CMAKE_ASM_MASM_COMPILER)
        message(STATUS "AVX-512 MASM kernels disabled: RAWRXD_ENABLE_EXPERIMENTAL_AVX512_KERNELS=OFF")
    else()
        message(STATUS "AVX-512 MASM kernels disabled: ml64.exe not found")
    endif()
endif()

# ============================================================================
# Header-only bridge library
# ============================================================================

add_library(AVX512_Semantic_Bridge INTERFACE)
target_include_directories(AVX512_Semantic_Bridge INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Propagate findings
if(CMAKE_ASM_MASM_COMPILER)
    target_compile_definitions(AVX512_Semantic_Bridge INTERFACE
        RAWRXD_AVX512_MASM_KERNELS_AVAILABLE=1
    )
endif()
