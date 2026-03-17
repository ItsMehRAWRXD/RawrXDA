#==============================================================================
# WEEK 1 DELIVERABLE BUILD CONFIGURATION
# Background Thread Infrastructure - Complete Implementation
# RawrXD AI IDE - Pure x64 MASM Assembly
# Status: Production Ready
#==============================================================================

cmake_minimum_required(VERSION 3.15)

#------------------------------------------------------------------------------
# Configuration
#------------------------------------------------------------------------------
set(WEEK1_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(WEEK1_MODULE_NAME "week1_infrastructure")

# Feature flags
option(WEEK1_ENABLE_DEBUG "Enable debug output" ON)
option(WEEK1_ENABLE_STATISTICS "Enable statistics collection" ON)
option(WEEK1_USE_COMPLETE_IMPL "Use WEEK1_COMPLETE.asm (2100+ LOC)" ON)

#------------------------------------------------------------------------------
# Source files
#------------------------------------------------------------------------------
if(WEEK1_USE_COMPLETE_IMPL)
    set(WEEK1_ASM_SOURCES
        "${WEEK1_SOURCE_DIR}/WEEK1_COMPLETE.asm"
    )
else()
    set(WEEK1_ASM_SOURCES
        "${WEEK1_SOURCE_DIR}/WEEK1_DELIVERABLE.asm"
    )
endif()

#------------------------------------------------------------------------------
# MASM Compiler Setup
#------------------------------------------------------------------------------
enable_language(ASM_MASM)

# MASM flags for x64 Windows
# /c       - Compile only (no link)
# /O2      - Maximum optimizations
# /Zi      - Generate debug info
# /W3      - Warning level 3
# /nologo  - Suppress banner
# /Cp      - Preserve case of user identifiers
# /Cx      - Preserve case in publics/externs
set(WEEK1_MASM_FLAGS "/c /O2 /Zi /W3 /nologo /Cp /Cx")

# Add debug defines
if(WEEK1_ENABLE_DEBUG)
    set(WEEK1_MASM_FLAGS "${WEEK1_MASM_FLAGS} /DDEBUG=1")
endif()

if(WEEK1_ENABLE_STATISTICS)
    set(WEEK1_MASM_FLAGS "${WEEK1_MASM_FLAGS} /DENABLE_STATS=1")
endif()

set(CMAKE_ASM_MASM_FLAGS "${WEEK1_MASM_FLAGS}")

#------------------------------------------------------------------------------
# Set assembly properties
#------------------------------------------------------------------------------
set_source_files_properties(${WEEK1_ASM_SOURCES}
    PROPERTIES
    LANGUAGE ASM_MASM
    COMPILE_FLAGS "${WEEK1_MASM_FLAGS}"
)

#------------------------------------------------------------------------------
# Build object files
#------------------------------------------------------------------------------
set(WEEK1_OBJECTS)
foreach(asm_file ${WEEK1_ASM_SOURCES})
    get_filename_component(asm_name "${asm_file}" NAME_WE)
    set(obj_file "${CMAKE_CURRENT_BINARY_DIR}/${asm_name}.obj")
    list(APPEND WEEK1_OBJECTS "${obj_file}")
    
    # Custom command for explicit control
    add_custom_command(
        OUTPUT ${obj_file}
        COMMAND ml64 ${WEEK1_MASM_FLAGS} /Fo${obj_file} ${asm_file}
        DEPENDS ${asm_file}
        COMMENT "Assembling ${asm_name}.asm"
        VERBATIM
    )
endforeach()

# Create custom target for objects
add_custom_target(${WEEK1_MODULE_NAME}_objects ALL
    DEPENDS ${WEEK1_OBJECTS}
)

#------------------------------------------------------------------------------
# Static library target
#------------------------------------------------------------------------------
add_library(${WEEK1_MODULE_NAME} STATIC ${WEEK1_ASM_SOURCES})

target_link_libraries(${WEEK1_MODULE_NAME}
    kernel32
    ntdll
)

# Set library properties
set_target_properties(${WEEK1_MODULE_NAME} PROPERTIES
    OUTPUT_NAME "week1"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LINKER_LANGUAGE C
)

#------------------------------------------------------------------------------
# Export for parent CMakeLists
#------------------------------------------------------------------------------
set(WEEK1_ASM_OBJECTS ${WEEK1_OBJECTS} PARENT_SCOPE)
set(WEEK1_SOURCES ${WEEK1_ASM_SOURCES} PARENT_SCOPE)
set(WEEK1_LIBRARY ${WEEK1_MODULE_NAME} PARENT_SCOPE)
set(WEEK1_INCLUDE_DIR "${WEEK1_SOURCE_DIR}" PARENT_SCOPE)

#------------------------------------------------------------------------------
# Installation rules
#------------------------------------------------------------------------------
install(TARGETS ${WEEK1_MODULE_NAME}
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES "${WEEK1_SOURCE_DIR}/Week1_API.h"
    DESTINATION include
)

#------------------------------------------------------------------------------
# Print configuration summary
#------------------------------------------------------------------------------
message(STATUS "Week 1 Infrastructure Configuration:")
message(STATUS "  Source: ${WEEK1_ASM_SOURCES}")
message(STATUS "  Debug: ${WEEK1_ENABLE_DEBUG}")
message(STATUS "  Statistics: ${WEEK1_ENABLE_STATISTICS}")
message(STATUS "  Complete Implementation: ${WEEK1_USE_COMPLETE_IMPL}")
