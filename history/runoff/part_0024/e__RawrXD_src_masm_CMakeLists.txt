# MASM-specific CMakeLists.txt
# This file should be included from the main project's CMakeLists.txt

# Add the source files for the MASM compiler library
set(MASM_LIB_SOURCES
    pe_writer.cpp
    elf_writer.cpp
    mach_o_writer.cpp
)

# Create a static library for the MASM writers
add_library(masm_writers STATIC ${MASM_LIB_SOURCES})

# Public include directory for the library
target_include_directories(masm_writers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
