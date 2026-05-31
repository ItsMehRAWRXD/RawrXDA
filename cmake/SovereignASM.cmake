#=============================================================================
# CMake module for RawrXD Sovereign ASM Kernels
#=============================================================================
# Usage:
#   include(SovereignASM.cmake)
#   add_sovereign_kernels(target_name)
#=============================================================================

# Enable MASM for Windows
if(WIN32)
    enable_language(ASM_MASM)
    
    # Set MASM flags
    set(CMAKE_ASM_MASM_FLAGS "${CMAKE_ASM_MASM_FLAGS} /nologo /c /W3 /Zi")
    
    # AVX2 support
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(CMAKE_ASM_MASM_FLAGS "${CMAKE_ASM_MASM_FLAGS} /DWIN64 /DAVX2")
    endif()
endif()

#=============================================================================
# Sovereign ASM source files
#=============================================================================
set(SOVEREIGN_ASM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/asm/SovereignMatMul.asm
    ${CMAKE_CURRENT_SOURCE_DIR}/src/asm/SovereignAttention.asm
    ${CMAKE_CURRENT_SOURCE_DIR}/src/asm/SovereignTokenizer.asm
    ${CMAKE_CURRENT_SOURCE_DIR}/src/asm/SovereignForwardPass.asm
    ${CMAKE_CURRENT_SOURCE_DIR}/src/asm/SovereignKernels.asm
)

#=============================================================================
# Function to add sovereign kernels to a target
#=============================================================================
function(add_sovereign_kernels target_name)
    # Create object library for ASM files
    add_library(${target_name}_asm OBJECT ${SOVEREIGN_ASM_SOURCES})
    
    # Set properties for ASM files
    set_target_properties(${target_name}_asm PROPERTIES
        LINKER_LANGUAGE C
    )
    
    # Create static library
    add_library(${target_name} STATIC 
        $<TARGET_OBJECTS:${target_name}_asm>
    )
    
    # Set include directories
    target_include_directories(${target_name} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    
    # Link dependencies
    target_link_libraries(${target_name} PUBLIC
        # No external dependencies - sovereign!
    )
    
    # Set properties
    set_target_properties(${target_name} PROPERTIES
        POSITION_INDEPENDENT_CODE ON
    )
    
    # MSVC specific
    if(MSVC)
        target_compile_options(${target_name} PRIVATE
            /W4 /WX- /EHsc /MP
        )
    endif()
    
    message(STATUS "Sovereign ASM kernels added to target: ${target_name}")
endfunction()

#=============================================================================
# Function to add sovereign kernels as shared library
#=============================================================================
function(add_sovereign_kernels_shared target_name)
    # Create shared library directly from ASM sources
    add_library(${target_name} SHARED ${SOVEREIGN_ASM_SOURCES})
    
    # Set properties
    set_target_properties(${target_name} PROPERTIES
        LINKER_LANGUAGE C
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )
    
    # Set include directories
    target_include_directories(${target_name} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    
    # MSVC specific
    if(MSVC)
        target_compile_options(${target_name} PRIVATE
            /W4 /WX- /EHsc /MP
        )
        
        # Set module definition file for exports
        set_target_properties(${target_name} PROPERTIES
            LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/src/asm/SovereignASM.def"
        )
    endif()
    
    message(STATUS "Sovereign ASM kernels (shared) added: ${target_name}")
endfunction()

#=============================================================================
# Function to create test executable for kernels
#=============================================================================
function(add_sovereign_kernel_tests test_name)
    set(TEST_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_matmul.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_attention.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_tokenizer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_forward_pass.cpp
    )
    
    add_executable(${test_name} ${TEST_SOURCES})
    
    # Link with sovereign kernels
    target_link_libraries(${test_name} PRIVATE
        sovereign_kernels
    )
    
    # Include directories
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )
    
    # Add tests
    add_test(NAME ${test_name}_matmul COMMAND ${test_name} matmul)
    add_test(NAME ${test_name}_attention COMMAND ${test_name} attention)
    add_test(NAME ${test_name}_tokenizer COMMAND ${test_name} tokenizer)
    add_test(NAME ${test_name}_forward COMMAND ${test_name} forward)
    
    message(STATUS "Sovereign kernel tests added: ${test_name}")
endfunction()

#=============================================================================
# Print configuration
#=============================================================================
message(STATUS "Sovereign ASM Kernel Configuration:")
message(STATUS "  Sources: ${SOVEREIGN_ASM_SOURCES}")
message(STATUS "  ASM Flags: ${CMAKE_ASM_MASM_FLAGS}")
message(STATUS "  AVX2: Enabled")
message(STATUS "  External Dependencies: None (sovereign)")
