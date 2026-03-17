cmake_minimum_required(VERSION 3.20)
project(GGUFVulkanHotpatch VERSION 1.0.0 LANGUAGES CXX)

# ════════════════════════════════════════════════════════════════════════════════
#  GGUF VULKAN HOTPATCH - CMake Build System
#  Integrates shader compilation, Vulkan linking, GGUF metadata resolver
# ════════════════════════════════════════════════════════════════════════════════

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─────────────────────────────────────────────────────────────────────────────
# OPTIONS
# ─────────────────────────────────────────────────────────────────────────────

option(BUILD_SHADER_SPIR_V "Compile GLSL to SPIR-V" ON)
option(BUILD_VULKAN_INTEGRATION "Build Vulkan integration example" ON)
option(BUILD_HOTPATCH_ENGINE "Build CLI hotpatch engine" ON)

# ─────────────────────────────────────────────────────────────────────────────
# VULKAN SDK DETECTION
# ─────────────────────────────────────────────────────────────────────────────

find_package(Vulkan REQUIRED)

if(NOT Vulkan_FOUND)
    message(FATAL_ERROR "Vulkan SDK not found. Install from https://vulkan.lunarg.com/sdk")
endif()

message(STATUS "Vulkan SDK found: ${Vulkan_INCLUDE_DIRS}")
message(STATUS "Vulkan libraries: ${Vulkan_LIBRARIES}")

# ─────────────────────────────────────────────────────────────────────────────
# SHADER COMPILATION
# ─────────────────────────────────────────────────────────────────────────────

if(BUILD_SHADER_SPIR_V)
    find_program(GLSLC glslc PATHS ${Vulkan_BIN_DIR})
    
    if(NOT GLSLC)
        message(FATAL_ERROR "glslc compiler not found. Install Vulkan SDK with tools.")
    endif()
    
    message(STATUS "Found glslc: ${GLSLC}")
    
    # Create build directory for shaders
    set(SHADER_BUILD_DIR "${CMAKE_BINARY_DIR}/shaders")
    file(MAKE_DIRECTORY "${SHADER_BUILD_DIR}")
    
    # Add shader compilation target
    set(SHADER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/gguf_metadata_adapter.comp")
    set(SHADER_SPIR_V "${SHADER_BUILD_DIR}/gguf_metadata_adapter.spv")
    
    add_custom_command(
        OUTPUT "${SHADER_SPIR_V}"
        COMMAND "${GLSLC}" -O3 -fshader-stage=compute "${SHADER_SRC}" -o "${SHADER_SPIR_V}"
        DEPENDS "${SHADER_SRC}"
        COMMENT "Compiling GLSL shader to SPIR-V..."
    )
    
    # Create shader target
    add_custom_target(
        ShaderCompilation ALL
        DEPENDS "${SHADER_SPIR_V}"
    )
    
    message(STATUS "Shader compilation enabled")
    message(STATUS "  Source: ${SHADER_SRC}")
    message(STATUS "  Output: ${SHADER_SPIR_V}")
endif()

# ─────────────────────────────────────────────────────────────────────────────
# VULKAN ADAPTER LIBRARY
# ─────────────────────────────────────────────────────────────────────────────

add_library(VulkanGGUFAdapter INTERFACE)

target_include_directories(VulkanGGUFAdapter
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
        ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(VulkanGGUFAdapter
    INTERFACE
        ${Vulkan_LIBRARIES}
)

# ─────────────────────────────────────────────────────────────────────────────
# VULKAN INTEGRATION EXECUTABLE
# ─────────────────────────────────────────────────────────────────────────────

if(BUILD_VULKAN_INTEGRATION)
    add_executable(vulkan_integration
        main_vulkan_integration.cpp
    )
    
    target_link_libraries(vulkan_integration
        PRIVATE
            VulkanGGUFAdapter
    )
    
    # Copy shader to output directory
    if(BUILD_SHADER_SPIR_V)
        add_dependencies(vulkan_integration ShaderCompilation)
        
        add_custom_command(TARGET vulkan_integration POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                "${SHADER_SPIR_V}"
                "$<TARGET_FILE_DIR:vulkan_integration>/gguf_metadata_adapter.spv"
            COMMENT "Copying SPIR-V shader to output directory..."
        )
    endif()
    
    message(STATUS "Building Vulkan integration example")
endif()

# ─────────────────────────────────────────────────────────────────────────────
# HOTPATCH ENGINE EXECUTABLE (MASM)
# ─────────────────────────────────────────────────────────────────────────────

if(BUILD_HOTPATCH_ENGINE AND MSVC)
    enable_language(ASM_MASM)
    
    add_executable(UniversalHotpatch
        UniversalHotpatch.asm
    )
    
    set_target_properties(UniversalHotpatch PROPERTIES
        LINKER_LANGUAGE CXX
    )
    
    target_link_libraries(UniversalHotpatch
        PRIVATE
            kernel32.lib
            msvcrt.lib
            user32.lib
    )
    
    message(STATUS "Building MASM hotpatch engine")
endif()

# ─────────────────────────────────────────────────────────────────────────────
# BUILD CONFIGURATION SUMMARY
# ─────────────────────────────────────────────────────────────────────────────

message(STATUS "")
message(STATUS "════════════════════════════════════════════════════════════════")
message(STATUS "  GGUF VULKAN HOTPATCH - BUILD CONFIGURATION")
message(STATUS "════════════════════════════════════════════════════════════════")
message(STATUS "")
message(STATUS "Build Type               : ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard             : ${CMAKE_CXX_STANDARD}")
message(STATUS "")
message(STATUS "Components:")
message(STATUS "  Shader Compilation     : ${BUILD_SHADER_SPIR_V}")
message(STATUS "  Vulkan Integration     : ${BUILD_VULKAN_INTEGRATION}")
message(STATUS "  Hotpatch Engine        : ${BUILD_HOTPATCH_ENGINE}")
message(STATUS "")
message(STATUS "Vulkan Configuration:")
message(STATUS "  SDK Version            : ${Vulkan_VERSION}")
message(STATUS "  Include                : ${Vulkan_INCLUDE_DIRS}")
message(STATUS "  Libraries              : ${Vulkan_LIBRARIES}")
message(STATUS "")
message(STATUS "════════════════════════════════════════════════════════════════")
message(STATUS "")

# ─────────────────────────────────────────────────────────────────────────────
# INSTALL TARGETS
# ─────────────────────────────────────────────────────────────────────────────

# Install shader
if(BUILD_SHADER_SPIR_V)
    install(FILES "${SHADER_SPIR_V}"
        DESTINATION bin
    )
endif()

# Install headers
install(FILES
    vulkan_gguf_adapter.hpp
    DESTINATION include
)

# Install executables
if(BUILD_VULKAN_INTEGRATION)
    install(TARGETS vulkan_integration
        DESTINATION bin
    )
endif()

if(BUILD_HOTPATCH_ENGINE AND MSVC)
    install(TARGETS UniversalHotpatch
        DESTINATION bin
    )
endif()
