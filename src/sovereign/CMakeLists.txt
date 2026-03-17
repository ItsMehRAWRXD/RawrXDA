#=============================================================================
# CMakeLists.txt for RawrXD Sovereign Core Integration
# Builds C++ wrappers and IDE bridge for autonomous agentic core
#=============================================================================

cmake_minimum_required(VERSION 3.16)
project(RawrXD_Sovereign)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#=============================================================================
# MASM Assembly Objects (compiled separately via ml64.exe)
#=============================================================================
# Pre-built: d:\rawrxd\build_out\sovereign_core.obj
# (Build via Build_Amphibious.ps1 first)

set(SOVEREIGN_CORE_OBJ 
    "d:/rawrxd/build_out/sovereign_core.obj"
)

if(NOT EXISTS "${SOVEREIGN_CORE_OBJ}")
    message(WARNING "sovereign_core.obj not found. Run Build_Amphibious.ps1 first.")
endif()

#=============================================================================
# C++ Wrapper Sources
#=============================================================================
set(SOVEREIGN_WRAPPER_SOURCES
    SovereignCoreWrapper.hpp
    SovereignCoreWrapper.cpp
    AgenticEngineSovereignHook.h
    AgenticEngineSovereignHook.cpp
    RawrXD_SovereignStatusPanel.h
    RawrXD_SovereignStatusPanel.cpp
)

#=============================================================================
# Library: sovereign_core (static lib with MASM object)
#=============================================================================
add_library(sovereign_core STATIC
    ${SOVEREIGN_WRAPPER_SOURCES}
)

target_include_directories(sovereign_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Link MASM object file
if(EXISTS "${SOVEREIGN_CORE_OBJ}")
    target_sources(sovereign_core PRIVATE
        ${SOVEREIGN_CORE_OBJ}
    )
endif()

# Link required Win32 libraries
target_link_libraries(sovereign_core PUBLIC
    kernel32.lib
    user32.lib
    gdi32.lib
)

#=============================================================================
# Export: Include dirs for IDE integration
#=============================================================================
target_compile_options(sovereign_core PRIVATE /W4)

if(MSVC)
    target_compile_options(sovereign_core PRIVATE /permissive- /utf-8)
endif()

#=============================================================================
# Optional: Self-contained test executable
#=============================================================================
option(BUILD_SOVEREIGN_TEST "Build standalone sovereign test" OFF)

if(BUILD_SOVEREIGN_TEST)
    add_executable(sovereign_test
        test/SovereignCoreTest.cpp
    )
    target_link_libraries(sovereign_test PUBLIC
        sovereign_core
    )
endif()

#=============================================================================
# Installation / IDE Integration Hook
#=============================================================================
if(TARGET RawrXD_IDE_Win32)
    # If building full IDE, link sovereign core to IDE
    target_link_libraries(RawrXD_IDE_Win32 PRIVATE
        sovereign_core
    )
    target_compile_definitions(RawrXD_IDE_Win32 PRIVATE
        RAWRXD_WITH_SOVEREIGN
    )
endif()
