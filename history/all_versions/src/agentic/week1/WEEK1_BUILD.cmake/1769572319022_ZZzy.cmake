# WEEK 1 DELIVERABLE BUILD CONFIGURATION
# Background Thread Infrastructure
# Status: Production Ready

cmake_minimum_required(VERSION 3.15)

set(WEEK1_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}")

# Assembly files
set(WEEK1_ASM_SOURCES
    "${WEEK1_SOURCE_DIR}/WEEK1_DELIVERABLE.asm"
)

# Get MASM compiler
enable_language(ASM_MASM)

# Assembly compiler flags
set(CMAKE_ASM_MASM_FLAGS "/c /O2 /Zi /W3 /nologo")

# Compile assembly
set_source_files_properties(${WEEK1_ASM_SOURCES}
    PROPERTIES
    LANGUAGE ASM_MASM
    COMPILE_FLAGS "/c /O2 /Zi /W3 /nologo"
)

# Object files
set(WEEK1_OBJECTS)
foreach(asm_file ${WEEK1_ASM_SOURCES})
    get_filename_component(asm_name "${asm_file}" NAME_WE)
    set(obj_file "${CMAKE_CURRENT_BINARY_DIR}/${asm_name}.obj")
    list(APPEND WEEK1_OBJECTS "${obj_file}")
endforeach()

# Export for parent CMakeLists
set(WEEK1_ASM_OBJECTS ${WEEK1_OBJECTS} PARENT_SCOPE)
set(WEEK1_SOURCES ${WEEK1_ASM_SOURCES} PARENT_SCOPE)
