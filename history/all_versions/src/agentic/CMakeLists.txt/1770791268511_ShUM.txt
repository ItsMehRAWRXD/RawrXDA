# RawrXD Agentic Library
# Self-manifesting capability system with hotpatching, observability, and Vulkan/Neon fabric

cmake_minimum_required(VERSION 3.15)
project(RawrXD-Agentic)

# Enable MASM support (MSVC-only)
if(MSVC)
    enable_language(ASM_MASM)
endif()

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../
)

# Source files - Manifestor
set(MANIFESTOR_SOURCES
    manifestor/CapabilityManifest.cpp
    manifestor/PEParser.cpp
    manifestor/SelfManifestor.cpp
)

# Source files - Wiring
set(WIRING_SOURCES
    wiring/CapabilityRouter.cpp
    wiring/FeatureFlags.cpp
    wiring/DependencyGraph.cpp
)

# Source files - Hotpatch
set(HOTPATCH_SOURCES
    hotpatch/Detour.cpp
    hotpatch/ShadowPage.cpp
    hotpatch/Engine.cpp
)

# Source files - Observability
set(OBSERVABILITY_SOURCES
    observability/Logger.cpp
    observability/Metrics.cpp
    observability/Telemetry.cpp
)

# Source files - Vulkan/Neon
set(VULKAN_SOURCES
    vulkan/VulkanManager.cpp
    vulkan/NeonFabric.cpp
)

# Source files - Assembly (NEON Vulkan Fabric)
# Using stub while full production assembly is validated for ml64 compatibility
if(MSVC)
    set(VULKAN_ASM_SOURCES
        vulkan/NEON_VULKAN_FABRIC_STUB.asm
    )
else()
    set(VULKAN_ASM_SOURCES)
endif()

# Source files - Bridge
set(BRIDGE_SOURCES
    bridge/Win32IDEBridge.cpp
)

# Source files - Coordination (Phase 2)
set(COORDINATION_SOURCES
    coordination/AgentCoordinator.cpp
    coordination/ConflictResolver.cpp
)

# Source files - Planning (Phase 2)
set(PLANNING_SOURCES
    planning/ModelGuidedPlanner.cpp
)

# Source files - Agentic Agent Loop (Phase: Analysis→Agent Pivot)
set(AGENT_LOOP_SOURCES
    RawrXD_ToolRegistry.cpp
    AgentToolHandlers.cpp
    BoundedAgentLoop.cpp
    OllamaProvider.cpp
    DiffEngine.cpp
    RawrXD_AgentLoop.cpp
)

# Source files - X-Macro Tool Registry + Orchestrator (Phase: LLM Tool-Calling)
set(ORCHESTRATOR_SOURCES
    ToolRegistry.cpp
    AgentOllamaClient.cpp
    FIMPromptBuilder.cpp
    AgentOrchestrator.cpp
    OrchestratorBridge.cpp
    DiskRecoveryAgent.cpp
    DiskRecoveryToolHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../agent/DiskRecoveryAgent.cpp
)

# Source files - T4 Autonomous Recovery Orchestrator
set(T4_RECOVERY_SOURCES
    autonomous_recovery_orchestrator.cpp
    DeterministicReplayEngine.cpp
)

# ASM Kernel Sources — DiskRecoveryAgent SCSI hammer protocol
if(MSVC)
    set(DISK_RECOVERY_ASM_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/../asm/RawrXD_DiskRecoveryAgent.asm
        ${CMAKE_CURRENT_SOURCE_DIR}/../asm/disk_recovery_scsi.asm
    )
    # T4: SourceEdit Kernel — Atomic file replace + git command
    set(SOURCEEDIT_ASM_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/../asm/RawrXD_SourceEdit_Kernel.asm
    )
else()
    set(DISK_RECOVERY_ASM_SOURCES)
    set(SOURCEEDIT_ASM_SOURCES)
endif()

# Combine all sources
set(AGENTIC_SOURCES
    ${MANIFESTOR_SOURCES}
    ${WIRING_SOURCES}
    ${HOTPATCH_SOURCES}
    ${OBSERVABILITY_SOURCES}
    ${VULKAN_SOURCES}
    ${BRIDGE_SOURCES}
    ${COORDINATION_SOURCES}
    ${PLANNING_SOURCES}
    ${AGENT_LOOP_SOURCES}
    ${ORCHESTRATOR_SOURCES}
    ${T4_RECOVERY_SOURCES}
    ${VULKAN_ASM_SOURCES}
    ${DISK_RECOVERY_ASM_SOURCES}
    ${SOURCEEDIT_ASM_SOURCES}
)

# Create library
add_library(RawrXD-Agentic STATIC ${AGENTIC_SOURCES})

# Set MASM compiler flags (minimal - ml64 is very strict)
if(MSVC)
    set(CMAKE_ASM_MASM_FLAGS "/nologo /Zi /W3")

    # MASM source files - MUST NOT get C++ compiler flags
    set_source_files_properties(
        ${VULKAN_ASM_SOURCES}
        ${DISK_RECOVERY_ASM_SOURCES}
        ${SOURCEEDIT_ASM_SOURCES}
        PROPERTIES
        LANGUAGE ASM_MASM
        COMPILE_FLAGS "/DWIN64 /D_WIN64"
    )

    # DiskRecoveryAgent ASM needs its include path for RawrXD_Common.inc
    if(DISK_RECOVERY_ASM_SOURCES)
        set_source_files_properties(
            ${DISK_RECOVERY_ASM_SOURCES}
            PROPERTIES
            COMPILE_FLAGS "/DWIN64 /D_WIN64 /I${CMAKE_CURRENT_SOURCE_DIR}/../asm"
        )
    endif()

    # SourceEdit Kernel ASM include path
    if(SOURCEEDIT_ASM_SOURCES)
        set_source_files_properties(
            ${SOURCEEDIT_ASM_SOURCES}
            PROPERTIES
            COMPILE_FLAGS "/DWIN64 /D_WIN64 /I${CMAKE_CURRENT_SOURCE_DIR}/../asm"
        )
    endif()
endif()

# Target properties
set_target_properties(RawrXD-Agentic PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
if(MSVC)
    set_target_properties(RawrXD-Agentic PROPERTIES
        VS_PLATFORM_TOOLSET "v143"
    )
endif()

# Link dependencies
target_link_libraries(RawrXD-Agentic PUBLIC
    # Windows libraries
    Kernel32
    User32
    Advapi32
    Shell32
    Ws2_32
    WinHttp
    
    # External dependencies (if any)
    # minhook # Uncomment if using minhook for detours
)

# Include directories for targets
target_include_directories(RawrXD-Agentic PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/rawrxd/agentic>
)

# Compiler definitions
target_compile_definitions(RawrXD-Agentic PRIVATE
    WIN64
    _WIN64
    UNICODE
    _UNICODE
    _CRT_SECURE_NO_WARNINGS
    $<$<CONFIG:Debug>:AGENTIC_DEBUG=1>
    $<$<CONFIG:Debug>:AGENTIC_VERBOSE_LOGGING=1>
    $<$<CONFIG:Release>:AGENTIC_RELEASE=1>
    $<$<BOOL:${ENABLE_VULKAN}>:AGENTIC_VULKAN_ENABLED=1>
    $<$<BOOL:${ENABLE_HOTPATCH}>:AGENTIC_HOTPATCH_ENABLED=1>
)

# Compiler options (C++ only - not for MASM)
if(MSVC)
    target_compile_options(RawrXD-Agentic PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:/std:c++20>
        $<$<COMPILE_LANGUAGE:CXX>:/W4>
        $<$<COMPILE_LANGUAGE:CXX>:/WX->
        $<$<COMPILE_LANGUAGE:CXX>:/permissive->
        $<$<COMPILE_LANGUAGE:CXX>:/GS>
        $<$<COMPILE_LANGUAGE:CXX>:/guard:cf>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Debug>>:/Od>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Debug>>:/ZI>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Debug>>:/RTC1>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Release>>:/O2>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Release>>:/Oi>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Release>>:/Ot>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Release>>:/GL>
    )
else()
    target_compile_options(RawrXD-Agentic PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-Wall>
        $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
        $<$<COMPILE_LANGUAGE:CXX>:-Wpedantic>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Debug>>:-O0>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Debug>>:-g>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Release>>:-O2>
    )
endif()

# Linker options
if(MSVC)
    set_target_properties(RawrXD-Agentic PROPERTIES
        STATIC_LIBRARY_FLAGS "/LTCG"
    )
endif()

# Export headers for installation
install(DIRECTORY
    manifestor/ wiring/ hotpatch/ observability/ vulkan/ bridge/
    DESTINATION include/rawrxd/agentic
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)

# Feature flags
target_compile_definitions(RawrXD-Agentic PRIVATE
    $<$<BOOL:${ENABLE_VULKAN}>:AGENTIC_VULKAN=1>
    $<$<BOOL:${ENABLE_HOTPATCH}>:AGENTIC_HOTPATCH=1>
    $<$<BOOL:${ENABLE_OBSERVABILITY}>:AGENTIC_OBSERVABILITY=1>
)

# Installation
install(TARGETS RawrXD-Agentic
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/agentic
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)

# Tests (if enabled)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
