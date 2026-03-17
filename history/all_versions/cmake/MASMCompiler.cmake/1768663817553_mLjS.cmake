# ============================================================================
# MASM Self-Compiling Compiler Suite - CMake Integration
# Add this to the main E:\RawrXD\CMakeLists.txt file
# ============================================================================

cmake_minimum_required(VERSION 3.15)

# ============================================================================
# MASM Compiler Suite Configuration
# ============================================================================

message(STATUS "Configuring MASM Self-Compiling Compiler Suite...")

# Find NASM assembler
find_program(NASM_EXECUTABLE nasm)
if(NOT NASM_EXECUTABLE)
    message(WARNING "NASM not found - MASM Solo compiler will not be built")
    message(WARNING "Install NASM from https://www.nasm.us/")
else()
    message(STATUS "Found NASM: ${NASM_EXECUTABLE}")
endif()

# ============================================================================
# 1. MASM Solo Compiler (Pure Assembly - Zero Dependencies)
# ============================================================================

if(NASM_EXECUTABLE)
    set(MASM_SOLO_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/masm/masm_solo_compiler.asm)
    set(MASM_SOLO_OBJ ${CMAKE_CURRENT_BINARY_DIR}/masm_solo_compiler.obj)
    
    add_custom_command(
        OUTPUT ${MASM_SOLO_OBJ}
        COMMAND ${NASM_EXECUTABLE} -f win64 
                ${MASM_SOLO_SOURCE}
                -o ${MASM_SOLO_OBJ}
        DEPENDS ${MASM_SOLO_SOURCE}
        COMMENT "Assembling MASM Solo Compiler (pure NASM assembly)"
        VERBATIM
    )
    
    add_executable(masm_solo_compiler ${MASM_SOLO_OBJ})
    
    # Link with minimal Windows API
    target_link_libraries(masm_solo_compiler PRIVATE kernel32 user32)
    
    # Set linker properties for raw assembly
    set_target_properties(masm_solo_compiler PROPERTIES
        LINKER_LANGUAGE C
        LINK_FLAGS "/ENTRY:main /SUBSYSTEM:CONSOLE /NODEFAULTLIB"
    )
    
    install(TARGETS masm_solo_compiler DESTINATION bin)
    
    message(STATUS "MASM Solo Compiler: ENABLED")
else()
    message(STATUS "MASM Solo Compiler: DISABLED (NASM not found)")
endif()

# ============================================================================
# 2. MASM CLI Compiler (C++ Command-Line Interface)
# ============================================================================

add_executable(masm_cli_compiler
    src/masm/masm_cli_compiler.cpp
)

# Require C++17 for filesystem support
target_compile_features(masm_cli_compiler PRIVATE cxx_std_17)

# Compiler warnings
target_compile_options(masm_cli_compiler PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX->
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -pedantic>
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -pedantic>
)

# Link with filesystem library for GCC < 9.0
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(masm_cli_compiler PRIVATE stdc++fs)
endif()

# Windows specific
if(WIN32)
    target_compile_definitions(masm_cli_compiler PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

install(TARGETS masm_cli_compiler DESTINATION bin)

message(STATUS "MASM CLI Compiler: ENABLED")

# ============================================================================
# 3. MASM Qt IDE Integration
# ============================================================================

if(Qt6_FOUND OR Qt5_FOUND)
    set(MASM_QT_SOURCES
        src/masm/MASMCompilerWidget.cpp
        src/masm/MASMCompilerWidget.h
    )
    
    # Add to main Qt application
    if(TARGET ${PROJECT_NAME})
        target_sources(${PROJECT_NAME} PRIVATE ${MASM_QT_SOURCES})
        
        # Add MASM include directory
        target_include_directories(${PROJECT_NAME} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/masm
        )
        
        message(STATUS "MASM Qt IDE Integration: ENABLED")
    else()
        message(WARNING "Main Qt project target not found - Qt integration not added")
    endif()
else()
    message(STATUS "MASM Qt IDE Integration: DISABLED (Qt not found)")
endif()

# ============================================================================
# Test Programs
# ============================================================================

enable_testing()

set(MASM_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests/masm)
file(MAKE_DIRECTORY ${MASM_TEST_DIR})

# Create test programs if they don't exist
if(NOT EXISTS ${MASM_TEST_DIR}/hello.asm)
    file(WRITE ${MASM_TEST_DIR}/hello.asm 
"; Hello World Test Program
; Demonstrates basic MASM compilation

.data
    message db 'Hello from MASM!', 13, 10, 0

.code
    extern ExitProcess
    extern GetStdHandle
    extern WriteFile

main proc
    ; Get stdout
    mov rcx, -11
    call GetStdHandle
    mov rbx, rax
    
    ; Write message
    mov rcx, rbx
    lea rdx, [message]
    mov r8, 18
    sub rsp, 40
    lea r9, [rsp+32]
    mov qword ptr [rsp+32], 0
    call WriteFile
    add rsp, 40
    
    ; Exit
    xor rcx, rcx
    call ExitProcess
main endp

end main
")
endif()

if(NOT EXISTS ${MASM_TEST_DIR}/factorial.asm)
    file(WRITE ${MASM_TEST_DIR}/factorial.asm
"; Factorial Calculation Test
; Tests arithmetic and procedure calls

.data
    result dq 0

.code
    extern ExitProcess

factorial proc
    cmp rcx, 1
    jle .base
    
    push rcx
    dec rcx
    call factorial
    pop rcx
    imul rax, rcx
    ret
    
.base:
    mov rax, 1
    ret
factorial endp

main proc
    mov rcx, 10
    call factorial
    mov [result], rax
    
    xor rcx, rcx
    call ExitProcess
main endp

end main
")
endif()

# Add tests
if(NASM_EXECUTABLE AND TARGET masm_solo_compiler)
    add_test(NAME masm_solo_hello
             COMMAND masm_solo_compiler ${MASM_TEST_DIR}/hello.asm ${CMAKE_CURRENT_BINARY_DIR}/hello_solo.exe
             WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    
    add_test(NAME masm_solo_factorial
             COMMAND masm_solo_compiler ${MASM_TEST_DIR}/factorial.asm ${CMAKE_CURRENT_BINARY_DIR}/factorial_solo.exe
             WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    
    set_tests_properties(masm_solo_hello masm_solo_factorial PROPERTIES
        LABELS "masm;solo"
        TIMEOUT 30)
endif()

if(TARGET masm_cli_compiler)
    add_test(NAME masm_cli_hello
             COMMAND masm_cli_compiler --verbose ${MASM_TEST_DIR}/hello.asm -o ${CMAKE_CURRENT_BINARY_DIR}/hello_cli.exe
             WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    
    add_test(NAME masm_cli_factorial
             COMMAND masm_cli_compiler -O2 ${MASM_TEST_DIR}/factorial.asm -o ${CMAKE_CURRENT_BINARY_DIR}/factorial_cli.exe
             WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    
    add_test(NAME masm_cli_multifile
             COMMAND masm_cli_compiler --verbose -g ${MASM_TEST_DIR}/hello.asm ${MASM_TEST_DIR}/factorial.asm -o ${CMAKE_CURRENT_BINARY_DIR}/combined.exe
             WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    
    set_tests_properties(masm_cli_hello masm_cli_factorial masm_cli_multifile PROPERTIES
        LABELS "masm;cli"
        TIMEOUT 30)
endif()

# ============================================================================
# Documentation Target
# ============================================================================

add_custom_target(masm_docs
    COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
    COMMAND ${CMAKE_COMMAND} -E echo "MASM Compiler Suite Documentation"
    COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Three complete MASM compiler implementations:"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "1. Solo Compiler (Pure Assembly)"
    COMMAND ${CMAKE_COMMAND} -E echo "   Location: ${CMAKE_INSTALL_PREFIX}/bin/masm_solo_compiler.exe"
    COMMAND ${CMAKE_COMMAND} -E echo "   Features: Zero dependencies, self-compiling"
    COMMAND ${CMAKE_COMMAND} -E echo "   Usage:    masm_solo_compiler input.asm output.exe"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "2. CLI Compiler (C++ Implementation)"
    COMMAND ${CMAKE_COMMAND} -E echo "   Location: ${CMAKE_INSTALL_PREFIX}/bin/masm_cli_compiler.exe"
    COMMAND ${CMAKE_COMMAND} -E echo "   Features: Multi-file, optimization, verbose output"
    COMMAND ${CMAKE_COMMAND} -E echo "   Usage:    masm_cli_compiler [options] source.asm"
    COMMAND ${CMAKE_COMMAND} -E echo "   Help:     masm_cli_compiler --help"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "3. Qt IDE Integration"
    COMMAND ${CMAKE_COMMAND} -E echo "   Location: Integrated into main Qt application"
    COMMAND ${CMAKE_COMMAND} -E echo "   Features: Syntax highlighting, error markers, debugger"
    COMMAND ${CMAKE_COMMAND} -E echo "   Access:   MASM menu in main application"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Build Commands:"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build . --target masm_solo_compiler"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build . --target masm_cli_compiler"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build . --config Release"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Test Commands:"
    COMMAND ${CMAKE_COMMAND} -E echo "  ctest -R masm --verbose"
    COMMAND ${CMAKE_COMMAND} -E echo "  ctest -L solo --verbose"
    COMMAND ${CMAKE_COMMAND} -E echo "  ctest -L cli --verbose"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Install:"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake --install . --prefix ./install"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
)

# ============================================================================
# Install Documentation
# ============================================================================

install(FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/MASM_COMPILER_SUITE_COMPLETE.md
    DESTINATION docs
)

# ============================================================================
# Summary
# ============================================================================

message(STATUS "========================================")
message(STATUS "MASM Compiler Suite Configuration Summary")
message(STATUS "========================================")
if(NASM_EXECUTABLE)
    message(STATUS "  Solo Compiler:      ENABLED")
else()
    message(STATUS "  Solo Compiler:      DISABLED")
endif()
message(STATUS "  CLI Compiler:       ENABLED")
if(Qt6_FOUND OR Qt5_FOUND)
    message(STATUS "  Qt IDE Integration: ENABLED")
else()
    message(STATUS "  Qt IDE Integration: DISABLED")
endif()
message(STATUS "  Test Programs:      ${MASM_TEST_DIR}")
message(STATUS "========================================")
