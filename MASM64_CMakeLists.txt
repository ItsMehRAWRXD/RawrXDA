# RawrXD Sidebar Core MASM64 Library
cmake_minimum_required(VERSION 3.20)
project(RawrXD_Sidebar_Core VERSION 1.0.0 LANGUAGES ASM_MASM)

# Enable MASM64
enable_language(ASM_MASM)

# Set the assembler
set(CMAKE_ASM_MASM_COMPILER ml64.exe)

# Source file
set(MASM_SOURCES
    Win32IDE_Sidebar_Core.asm
)

# Create static library
add_library(RawrXD_Sidebar_Core STATIC ${MASM_SOURCES})

# Set output properties
set_target_properties(RawrXD_Sidebar_Core PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    OUTPUT_NAME "RawrXD_Sidebar_Core"
)

# Include directories for header
target_include_directories(RawrXD_Sidebar_Core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)