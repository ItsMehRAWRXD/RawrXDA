# CMakeLists_native.txt - Pure Native RawrXD Build
cmake_minimum_required(VERSION 3.20)

# Phase 46: Enable Vulkan for dual GPU testing
option(RAWR_ENABLE_VULKAN "Enable Vulkan SDK for GPU compute (dual GPU testing)" ON)

# Force Windows SDK version BEFORE any project() or compiler checks
# This MUST be set before project() to affect the VS generator
if(WIN32)
    set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION "10.0.22621.0" CACHE STRING "Windows SDK version for VS generator" FORCE)
    set(CMAKE_SYSTEM_VERSION "10.0.22621.0" CACHE STRING "Windows SDK version" FORCE)
endif()

# =============================================================================
# Windows SDK & MSVC CRT Path Resolution (Phase 41 Build Fix)
# =============================================================================
# MUST run before project() because CMake's compiler test needs these paths.
# The environment on this machine has partial SDK installs:
#   10.0.22621.0 — has ucrt, shared, um includes BUT um/x64 libs are separate
#   10.0.26100.0 — has um/shared includes and um/x64 libs BUT no ucrt
# We combine them to form a complete toolchain.
if(WIN32 AND NOT DEFINED ENV{VSCMD_ARG_TGT_ARCH})
    # Not running inside a VS Developer Prompt — inject paths manually
    # Auto-detect Windows SDK: try D: first, fall back to C:
    if(EXISTS "D:/Program Files (x86)/Windows Kits/10")
        set(_WIN10_SDK_ROOT "D:/Program Files (x86)/Windows Kits/10")
    else()
        set(_WIN10_SDK_ROOT "C:/Program Files (x86)/Windows Kits/10")
    endif()
    set(_SDK_UCRT_VER "10.0.22621.0")    # Has ucrt
    
    # Force use of 22621.0 since 26100.0 is not registered properly for MSBuild
    set(_SDK_UM_VER "10.0.22621.0")
    
    # Fallback: if 10.0.26100.0 doesn't exist for um, try 10.0.22621.0 for everything
    if(NOT EXISTS "${_WIN10_SDK_ROOT}/Include/${_SDK_UM_VER}/um/windows.h")
        set(_SDK_UM_VER "${_SDK_UCRT_VER}")
    endif()
    if(NOT EXISTS "${_WIN10_SDK_ROOT}/Include/${_SDK_UCRT_VER}/ucrt")
        set(_SDK_UCRT_VER "${_SDK_UM_VER}")
    endif()

    # Auto-detect MSVC toolchain: try D: first, fall back to C:
    # Verify that cl.exe exists in the detected path before using it
    set(_MSVC_ROOT "")
    foreach(_MSVC_CANDIDATE_ROOT
        "C:/Program Files/Microsoft Visual Studio/18/Enterprise/VC/Tools/MSVC"
        "D:/VS2022Enterprise/VC/Tools/MSVC"
        "C:/VS2022Enterprise/VC/Tools/MSVC"
        "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC"
        "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC"
        "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC"
        "C:/Program Files (x86)/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC"
        "C:/Program Files (x86)/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC"
    )
        if(EXISTS "${_MSVC_CANDIDATE_ROOT}")
            file(GLOB _MSVC_CANDIDATE_VERS "${_MSVC_CANDIDATE_ROOT}/*")
            if(_MSVC_CANDIDATE_VERS)
                list(SORT _MSVC_CANDIDATE_VERS)
                list(GET _MSVC_CANDIDATE_VERS -1 _MSVC_CANDIDATE_VER)
                # Verify cl.exe exists before accepting this path
                if(EXISTS "${_MSVC_CANDIDATE_VER}/bin/Hostx64/x64/cl.exe")
                    set(_MSVC_ROOT "${_MSVC_CANDIDATE_VER}")
                    message(STATUS "[SDK Fix] Found MSVC with cl.exe at: ${_MSVC_ROOT}")
                    break()
                endif()
            endif()
        endif()
    endforeach()
    
    if(NOT _MSVC_ROOT)
        message(FATAL_ERROR "[SDK Fix] Could not find a valid MSVC installation with cl.exe. Please install Visual Studio Build Tools or run from a Developer Command Prompt.")
    endif()
    
    # SDK bin tools (rc.exe, mt.exe)
    set(_SDK_BIN "${_WIN10_SDK_ROOT}/bin/${_SDK_UCRT_VER}/x64")
    
    # Build INCLUDE and LIB environment strings
    set(ENV{INCLUDE} "${_MSVC_ROOT}/include;${_WIN10_SDK_ROOT}/Include/${_SDK_UCRT_VER}/ucrt;${_WIN10_SDK_ROOT}/Include/${_SDK_UM_VER}/shared;${_WIN10_SDK_ROOT}/Include/${_SDK_UM_VER}/um")
    # MSVC 14.50+ ships CRT in lib/onecore/x64 instead of lib/x64, include both
    set(ENV{LIB} "${_MSVC_ROOT}/lib/x64;${_MSVC_ROOT}/lib/onecore/x64;${_WIN10_SDK_ROOT}/Lib/${_SDK_UCRT_VER}/ucrt/x64;${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64")
    set(ENV{PATH} "$ENV{PATH};${_SDK_BIN};${_MSVC_ROOT}/bin/Hostx64/x64")
    
    # Tell CMake where the SDK tools are
    set(CMAKE_MT "${_SDK_BIN}/mt.exe" CACHE FILEPATH "Manifest Tool" FORCE)
    set(CMAKE_RC_COMPILER "${_SDK_BIN}/rc.exe" CACHE FILEPATH "Resource Compiler" FORCE)
    set(CMAKE_C_COMPILER "${_MSVC_ROOT}/bin/Hostx64/x64/cl.exe" CACHE FILEPATH "C Compiler" FORCE)
    set(CMAKE_CXX_COMPILER "${_MSVC_ROOT}/bin/Hostx64/x64/cl.exe" CACHE FILEPATH "C++ Compiler" FORCE)
    set(CMAKE_LINKER "${_MSVC_ROOT}/bin/Hostx64/x64/link.exe" CACHE FILEPATH "Linker" FORCE)
    set(CMAKE_ASM_MASM_COMPILER "${_MSVC_ROOT}/bin/Hostx64/x64/ml64.exe" CACHE FILEPATH "MASM64" FORCE)

    # Makefile generators need an explicit build tool path when not in a Dev Prompt.
    if(CMAKE_GENERATOR STREQUAL "NMake Makefiles" AND EXISTS "${_MSVC_ROOT}/bin/Hostx64/x64/nmake.exe")
        set(CMAKE_MAKE_PROGRAM "${_MSVC_ROOT}/bin/Hostx64/x64/nmake.exe" CACHE FILEPATH "NMake" FORCE)
    endif()
     
    message(STATUS "[SDK Fix] INCLUDE = $ENV{INCLUDE}")
    message(STATUS "[SDK Fix] LIB     = $ENV{LIB}")
    message(STATUS "[SDK Fix] MT      = ${_SDK_BIN}/mt.exe")
endif()

# Provide explicit library search paths for all targets when not in a Dev Prompt.
if(MSVC)
    set(_RAWR_MSVC_LIBPATHS "")
    if(DEFINED _MSVC_ROOT AND EXISTS "${_MSVC_ROOT}/lib/x64")
        list(APPEND _RAWR_MSVC_LIBPATHS "${_MSVC_ROOT}/lib/x64")
    endif()
    if(DEFINED _MSVC_ROOT AND EXISTS "${_MSVC_ROOT}/lib/onecore/x64")
        list(APPEND _RAWR_MSVC_LIBPATHS "${_MSVC_ROOT}/lib/onecore/x64")
    endif()
    if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UCRT_VER AND EXISTS "${_WIN10_SDK_ROOT}/Lib/${_SDK_UCRT_VER}/ucrt/x64")
        list(APPEND _RAWR_MSVC_LIBPATHS "${_WIN10_SDK_ROOT}/Lib/${_SDK_UCRT_VER}/ucrt/x64")
    endif()
    if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UM_VER AND EXISTS "${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64")
        list(APPEND _RAWR_MSVC_LIBPATHS "${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64")
    endif()
    if(_RAWR_MSVC_LIBPATHS)
        link_directories(${_RAWR_MSVC_LIBPATHS})
    endif()
endif()

# ============================================================================
# Scaffold/Placeholder Enforcement Target
# ============================================================================
if(WIN32)
    add_custom_target(enforce_no_scaffold
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass
                -File ${CMAKE_SOURCE_DIR}/tools/enforce_no_scaffold.ps1
                -Root ${CMAKE_SOURCE_DIR}
                -FailOnMatch
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Enforcing zero scaffold/placeholder language in source tree"
        VERBATIM
    )
endif()

# ============================================================================
# Vulkan SDK Detection (Common - must run before targets are defined)
# ============================================================================
if(RAWR_ENABLE_VULKAN)
    # Try manual path first — D: first, then C:
    file(GLOB _VK_SDK_DIRS "D:/VulkanSDK/1.*")
    if(NOT _VK_SDK_DIRS)
        file(GLOB _VK_SDK_DIRS "C:/VulkanSDK/1.*")
    endif()
    if(_VK_SDK_DIRS)
        list(SORT _VK_SDK_DIRS)
        list(GET _VK_SDK_DIRS -1 _VK_SDK_DIR)
        message(STATUS "[Vulkan] Common SDK: ${_VK_SDK_DIR}")
        set(RAWR_VULKAN_SDK_ROOT "${_VK_SDK_DIR}" CACHE PATH "Vulkan SDK root")
        set(RAWR_VULKAN_INCLUDE "${_VK_SDK_DIR}/Include" CACHE PATH "Vulkan include dir")
        set(RAWR_VULKAN_LIB "${_VK_SDK_DIR}/Lib/vulkan-1.lib" CACHE FILEPATH "Vulkan library")
    endif()
endif()

# RawrXD - Zero Bloat Horizon (Phase 14.7.3+)
project(RawrXD VERSION 14.7.3 LANGUAGES C CXX ASM_MASM)

# ============================================================================
# nlohmann/json via FetchContent (fixes Deep2Discovery.cpp json/json.hpp)
# ============================================================================
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# MSVC 19.51 has a known <ranges> crash at C++23 (/std:c++latest).
# Pin to C++20 for build stability. std::expected is available via
# <expected> polyfill or 3rdparty if needed.
if(MSVC)
    set(CMAKE_CXX_STANDARD 20)
else()
    set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# One compile_commands.json entry per translation unit helps MSVC/clangd-based tools avoid
# invalid merged command lines (D8036 / drv_out_file_argument_with_multiple_sources: /Fo + multiple sources).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile_commands.json for IDE/clangd")

# v14.7.3 Performance/throughput flags
# Keep Debug free of /O2 and /GL to avoid /RTC1 conflicts on single-config generators.
# Note: Do not add /MP globally. Ninja (and VS) already parallelize across TUs; CMake emits one
# source per cl invocation for Ninja, so /MP is redundant and some MSVC language services mishandle
# /Fo together with /MP when a tool synthesizes a multi-source command line.
if(MSVC)
    add_compile_options(
        $<$<COMPILE_LANGUAGE:C,CXX>:/GS->
        $<$<COMPILE_LANGUAGE:C,CXX>:/sdl->
    )
    
    # AddressSanitizer configuration (MSVC 2019 16.9+)
    if(RAWRXD_ENABLE_ASAN)
        message(STATUS "[ASan] AddressSanitizer enabled for C++ components")
        add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/fsanitize=address>)
        add_link_options(/fsanitize=address)
        # Disable incremental linking with ASan
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /INCREMENTAL:NO")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /INCREMENTAL:NO")
    endif()
else()
    add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:-O3>)
endif()
# Keep LTCG globally, but avoid executable-only/defaultlib overrides here.
# /NODEFAULTLIB + /ENTRY + /SUBSYSTEM must not leak into DLL targets.
if(MSVC)
    add_link_options(/LTCG)
endif()

# WebView2 and WIL are expected under this optional local SDK path.
set(RAWRXD_LOCAL_WIN32_SDK_DIR "d:/rawrxd/include")
if(EXISTS "${RAWRXD_LOCAL_WIN32_SDK_DIR}")
    include_directories("${RAWRXD_LOCAL_WIN32_SDK_DIR}")
endif()

option(RAWRXD_BUILD_WIN32IDE "Build the legacy Win32IDE target" OFF)
option(RAWRXD_BUILD_CLI "Build the rawrxd CLI console target" OFF)
option(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES "Strip stub/shim/mock source units from production targets" OFF)
option(RAWRXD_INCLUDE_STRESS_AND_REPLAY_SOURCES "Include stress/replay harness sources in production targets" OFF)
option(RAWRXD_ENABLE_MISSING_HANDLER_STUBS "Allow missing_handler_stubs.cpp fallback lane for Win32IDE wiring" OFF)
option(RAWRXD_ENABLE_ASAN "Enable AddressSanitizer for C++ components (MSVC only)" OFF)
option(RAWRXD_ENABLE_VALIDATION "Enable RawrXD runtime validation instrumentation (tensor dumping for llama.cpp parity)" OFF)
option(RAWR_ENGINE_LANE_B "Build RawrEngine in minimal Lane B mode (no debugger/ollama)" OFF)
if(RAWRXD_BUILD_CLI)
    if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
        message(FATAL_ERROR "[ProductionLane] RAWRXD_BUILD_CLI=ON is incompatible with RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON (rawrxd CLI links compatibility shims).")
    endif()
    file(GLOB UI_SOURCES "src/ui/*.cpp")
    file(GLOB ASM_SOURCES "src/asm/RawrXD_IPC_Dispatcher.asm" "src/asm/RawrXD_Disasm_Kernel.asm")

    add_executable(rawrxd
        src/ui/entry_point.cpp
        src/ui/RawrXD_MainLoop.cpp
        src/asm/RawrXD_IPC_Dispatcher.asm
        src/asm/RawrXD_Disasm_Kernel.asm
        src/asm/rawr_cpu_features.asm
        src/cli/rawrxd_cli_link_shims.cpp
        src/res/Resource.rc
        src/rawrxd_link_stubs.cpp
        # Core inference components for dual GPU testing
        src/rawrxd_inference.cpp
        src/rawrxd_model_loader.cpp
        src/rawrxd_tokenizer.cpp
        src/rawrxd_transformer.cpp
        src/rawrxd_sampler.cpp
        src/gguf_loader.cpp
        src/gguf_vocab_resolver.cpp
        src/core/swarm_scheduler.cpp
        src/core/link_stubs.cpp
        src/core/gguf_swarm_plan_builder.cpp
        src/core/inference_witness.cpp
        src/logging/Logger.cpp
        # Generation Bouncer - unified entry point
        src/runtime/GenerationBouncer.cpp
        src/runtime/TensorExecutionRouter.cpp
        src/runtime/StreamRouterAdapter.cpp
        src/runtime/Sovereign_KernelBridge_CPP.cpp
        src/runtime/Deep2ExecutionTelemetry.cpp
        src/backend/VulkanGemmDispatcher.cpp
        src/runtime/gguf_tensor_loader.cpp
        # Elastic runtime for predictive tensor residency
        src/runtime/elastic/ElasticEngine.cpp
        src/runtime/elastic/ElasticGGUFIndex.cpp
        src/runtime/elastic/ElasticResidencyManager.cpp
        src/runtime/elastic/VulkanTensorResidencyBackend.cpp
        src/runtime/elastic/ElasticDecoder.cpp
        # Shared model runtime
        src/runtime/shared/SharedModelRuntime.cpp
        src/runtime/shared/BP1BraidIndex.cpp
        src/runtime/shared/BP1BraidStreamer.cpp
        src/runtime/governance/HardwareCapabilityProbe.cpp
        src/runtime/governance/CapabilityTierMatrix.cpp
        src/runtime/governance/HardwareGovernor.cpp
        src/runtime/governance/PressedStateEngine.cpp
        src/runtime/governance/Ensemble58Governor.cpp
        src/runtime/governance/TLSInstructionGate.cpp
        src/runtime/governance/UnifiedTriggerOrchestrator.cpp
        src/runtime/memory/CapacityManager.cpp
        src/runtime/memory/PlacementPolicy.cpp
        src/runtime/memory/ResidencyTracker.cpp
        src/runtime/memory/TensorPlacementManager.cpp
        src/runtime/memory/TransferScheduler.cpp
        src/runtime/memory/PredictiveMemoryManager.cpp
        src/runtime/memory/WorkingSetPredictor.cpp
        src/inference/NegativeSpaceProfiler.cpp
        B014/build/b014_profiler.cpp
        # Agentic components for CLI Ollama testing - TEMPORARILY DISABLED
        # src/agentic/OrchestratorBridge.cpp
        # src/agentic/AgentOllamaClient.cpp
        src/agentic/FIMPromptBuilder.cpp
        src/agentic/RobustOllamaParser.cpp
    )
    set_target_properties(rawrxd PROPERTIES EXCLUDE_FROM_ALL TRUE EXCLUDE_FROM_DEFAULT_BUILD TRUE)
    if(MSVC)
        target_link_directories(rawrxd PRIVATE ${_RAWR_MSVC_LIBPATHS})
        set(_RAWR_CLI_LIBPATHS "")
        foreach(_rawr_lib IN LISTS _RAWR_MSVC_LIBPATHS)
            if(EXISTS "${_rawr_lib}")
                target_link_options(rawrxd PRIVATE "/LIBPATH:\"${_rawr_lib}\"")
                list(APPEND _RAWR_CLI_LIBPATHS "/LIBPATH:\"${_rawr_lib}\"")
            endif()
        endforeach()
        if(_RAWR_CLI_LIBPATHS)
            string(REPLACE ";" " " _RAWR_CLI_LIBPATHS_STR "${_RAWR_CLI_LIBPATHS}")
            set_property(TARGET rawrxd APPEND_STRING PROPERTY LINK_FLAGS " ${_RAWR_CLI_LIBPATHS_STR}")
        endif()
    endif()

    # Add LIBPATH flags for Windows SDK, MSVC runtime, and UCRT
    target_link_options(rawrxd PRIVATE
        "/LIBPATH:C:/Program Files (x86)/Windows Kits/10/Lib/10.0.22621.0/um/x64"
        "/LIBPATH:C:/Program Files (x86)/Windows Kits/10/Lib/10.0.22621.0/ucrt/x64"
        "/LIBPATH:C:/Program Files/Microsoft Visual Studio/18/Enterprise/VC/Tools/MSVC/14.51.36231/lib/x64"
        "/LIBPATH:C:/Program Files/Microsoft Visual Studio/18/Enterprise/VC/Tools/MSVC/14.51.36231/lib/onecore/x64"
    )
    
    # Explicit absolute library paths ensure linking succeeds even if LIBPATH is dropped
    target_link_libraries(rawrxd PRIVATE
        kernel32.lib
        user32.lib
        gdi32.lib
        winspool.lib
        shell32.lib
        ole32.lib
        oleaut32.lib
        uuid.lib
        comdlg32.lib
        advapi32.lib
        version.lib
        ws2_32.lib
        dxgi.lib
    )
    
    # Vulkan SDK support for rawrxd
    # Phase 46: Auto-detect Vulkan SDK for dual GPU testing
    if(RAWR_ENABLE_VULKAN AND DEFINED RAWR_VULKAN_SDK_ROOT)
        message(STATUS "[rawrxd] Vulkan SDK: ${RAWR_VULKAN_SDK_ROOT}")
        target_include_directories(rawrxd PRIVATE "${RAWR_VULKAN_INCLUDE}")
        target_link_directories(rawrxd PRIVATE "${RAWR_VULKAN_SDK_ROOT}/Lib")
        target_link_libraries(rawrxd PRIVATE "${RAWR_VULKAN_LIB}" dxgi)
        target_compile_definitions(rawrxd PRIVATE RAWR_HAS_VULKAN=1 RAWR_VULKAN_AVAILABLE=1)
    elseif(RAWR_ENABLE_VULKAN AND DEFINED VULKAN_INCLUDE_DIR)
        target_include_directories(rawrxd PRIVATE ${VULKAN_INCLUDE_DIR})
        target_link_libraries(rawrxd PRIVATE dxgi)
        target_compile_definitions(rawrxd PRIVATE RAWR_HAS_VULKAN=1)
    elseif(RAWR_ENABLE_VULKAN AND Vulkan_FOUND)
        target_include_directories(rawrxd PRIVATE ${Vulkan_INCLUDE_DIRS})
        target_link_libraries(rawrxd PRIVATE Vulkan::Vulkan dxgi)
        target_compile_definitions(rawrxd PRIVATE RAWR_HAS_VULKAN=1)
    elseif(RAWR_ENABLE_VULKAN)
        message(STATUS "[rawrxd] Vulkan SDK not found — GPU compute uses CPU fallback")
        target_compile_definitions(rawrxd PRIVATE RAWR_HAS_VULKAN=0)
    endif()

    # =============================================================================
    # RawrXD Serve — Ollama-compatible HTTP server with shared model runtime
    # =============================================================================
    add_executable(rawrxd-serve
        src/serve/rawrxd_serve_main.cpp
        src/serve/rawrxd_serve.cpp
        src/serve/rawrxd_model_registry.cpp
        src/serve/rawrxd_serve_inference_plugin.cpp
        src/serve/inference_plugin_backend.cpp
        src/serve/rawrxd_pipe_server.cpp
        src/serve/rawrxd_pipe_server_stubs.cpp
        src/runtime/GenerationBouncer.cpp
        src/runtime/shared/SharedModelRuntime.cpp
        src/runtime/shared/BP1BraidIndex.cpp
        src/runtime/shared/BP1BraidStreamer.cpp
        src/runtime/TensorExecutionRouter.cpp
        src/runtime/StreamRouterAdapter.cpp
        src/runtime/Sovereign_KernelBridge_CPP.cpp
        src/runtime/Deep2ExecutionTelemetry.cpp
        src/backend/VulkanGemmDispatcher.cpp
        src/runtime/governance/HardwareCapabilityProbe.cpp
        src/runtime/governance/CapabilityTierMatrix.cpp
        src/runtime/governance/HardwareGovernor.cpp
        src/runtime/governance/PressedStateEngine.cpp
        src/runtime/governance/Ensemble58Governor.cpp
        src/runtime/governance/TLSInstructionGate.cpp
        src/runtime/governance/UnifiedTriggerOrchestrator.cpp
        src/runtime/memory/CapacityManager.cpp
        src/runtime/memory/PlacementPolicy.cpp
        src/runtime/memory/ResidencyTracker.cpp
        src/runtime/memory/TensorPlacementManager.cpp
        src/runtime/memory/TransferScheduler.cpp
        src/runtime/memory/PredictiveMemoryManager.cpp
        src/runtime/memory/WorkingSetPredictor.cpp
        src/gpu_enforcement.cpp
    )
    set_target_properties(rawrxd-serve PROPERTIES
        EXCLUDE_FROM_ALL TRUE
        EXCLUDE_FROM_DEFAULT_BUILD TRUE
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
    target_include_directories(rawrxd-serve PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/inference
        ${CMAKE_SOURCE_DIR}/src/engine
        ${CMAKE_SOURCE_DIR}/src/core
        ${CMAKE_SOURCE_DIR}/src/codec
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/masm
        ${CMAKE_SOURCE_DIR}/src/runtime/governance
        ${RAWR_VULKAN_INCLUDE}
    )
    if(MSVC)
        target_link_directories(rawrxd-serve PRIVATE ${_RAWR_MSVC_LIBPATHS})
        foreach(_rawr_lib IN LISTS _RAWR_MSVC_LIBPATHS)
            if(EXISTS "${_rawr_lib}")
                target_link_options(rawrxd-serve PRIVATE "/LIBPATH:\"${_rawr_lib}\"")
            endif()
        endforeach()
    endif()
    target_link_libraries(rawrxd-serve PRIVATE
        InferenceEngine
        httpapi.lib
        ws2_32.lib
        kernel32.lib
        user32.lib
        gdi32.lib
        shell32.lib
        ole32.lib
        oleaut32.lib
        uuid.lib
        comdlg32.lib
        advapi32.lib
        version.lib
    )
    if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UM_VER)
        set_property(TARGET rawrxd-serve APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64\"")
    endif()
    if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UCRT_VER)
        set_property(TARGET rawrxd-serve APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UCRT_VER}/ucrt/x64\"")
    endif()
    if(DEFINED _MSVC_ROOT)
        set_property(TARGET rawrxd-serve APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_MSVC_ROOT}/lib/x64\"")
    endif()
    message(STATUS "rawrxd-serve: Ollama-compatible HTTP server target configured")

    # =============================================================================
    # BP1 Triple-Braid Smoke Test
    # Validates: index build, block addressing, all 3 braid paths,
    #            decompression (identity/RLE/LZ4-lite), corruption rejection,
    #            dequantization correctness, workspace bounds, memory stability
    # =============================================================================
    add_executable(bp1_smoke_test
        tests/bp1_smoke_test.cpp
        src/runtime/shared/SharedModelRuntime.cpp
        src/runtime/shared/BP1BraidIndex.cpp
        src/runtime/shared/BP1BraidStreamer.cpp
        src/runtime/memory/CapacityManager.cpp
        src/runtime/memory/PlacementPolicy.cpp
        src/runtime/memory/ResidencyTracker.cpp
        src/runtime/memory/TensorPlacementManager.cpp
        src/runtime/memory/TransferScheduler.cpp
        src/runtime/memory/PredictiveMemoryManager.cpp
        src/runtime/memory/WorkingSetPredictor.cpp
    )
    set_target_properties(bp1_smoke_test PROPERTIES
        EXCLUDE_FROM_ALL TRUE
        EXCLUDE_FROM_DEFAULT_BUILD TRUE
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    target_include_directories(bp1_smoke_test PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
    )
    if(MSVC)
        target_link_directories(bp1_smoke_test PRIVATE ${_RAWR_MSVC_LIBPATHS})
        foreach(_rawr_lib IN LISTS _RAWR_MSVC_LIBPATHS)
            if(EXISTS "${_rawr_lib}")
                target_link_options(bp1_smoke_test PRIVATE "/LIBPATH:\"${_rawr_lib}\"")
            endif()
        endforeach()
    endif()
    target_link_libraries(bp1_smoke_test PRIVATE
        kernel32.lib user32.lib gdi32.lib shell32.lib ole32.lib oleaut32.lib
        uuid.lib comdlg32.lib advapi32.lib version.lib psapi.lib
    )
    if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UM_VER)
        set_property(TARGET bp1_smoke_test APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64\"")
    endif()
    if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UCRT_VER)
        set_property(TARGET bp1_smoke_test APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UCRT_VER}/ucrt/x64\"")
    endif()
    if(DEFINED _MSVC_ROOT)
        set_property(TARGET bp1_smoke_test APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_MSVC_ROOT}/lib/x64\"")
    endif()
    message(STATUS "bp1_smoke_test: BP1 triple-braid smoke test configured")

    # =========================================================================
    # Vulkan Compute Diagnostic Test
    # Minimal proof: buffer allocation → transfer → compute fill → readback
    # =========================================================================
    if(RAWR_ENABLE_VULKAN AND DEFINED RAWR_VULKAN_SDK_ROOT)
        add_executable(vulkan_compute_diagnostic
            tests/vulkan_compute_diagnostic.cpp
        )
        set_target_properties(vulkan_compute_diagnostic PROPERTIES
            EXCLUDE_FROM_ALL TRUE
            EXCLUDE_FROM_DEFAULT_BUILD TRUE
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )
        target_include_directories(vulkan_compute_diagnostic PRIVATE
            ${RAWR_VULKAN_INCLUDE}
        )
        target_link_libraries(vulkan_compute_diagnostic PRIVATE
            ${RAWR_VULKAN_LIB}
            kernel32.lib user32.lib gdi32.lib
        )
        if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UM_VER)
            set_property(TARGET vulkan_compute_diagnostic APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64\"")
        endif()
        if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UCRT_VER)
            set_property(TARGET vulkan_compute_diagnostic APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UCRT_VER}/ucrt/x64\"")
        endif()
        if(DEFINED _MSVC_ROOT)
            set_property(TARGET vulkan_compute_diagnostic APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_MSVC_ROOT}/lib/x64\"")
        endif()
        message(STATUS "vulkan_compute_diagnostic: Vulkan compute diagnostic test configured")

        # =====================================================================
        # Vulkan Tensor Upload + Readback Verification Test
        # Proves: staging buffer → device-local buffer → readback → verification
        # =====================================================================
        add_executable(vulkan_tensor_upload_test
            tests/vulkan_tensor_upload_test.cpp
        )
        set_target_properties(vulkan_tensor_upload_test PROPERTIES
            EXCLUDE_FROM_ALL TRUE
            EXCLUDE_FROM_DEFAULT_BUILD TRUE
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )
        target_include_directories(vulkan_tensor_upload_test PRIVATE
            ${RAWR_VULKAN_INCLUDE}
        )
        target_link_libraries(vulkan_tensor_upload_test PRIVATE
            ${RAWR_VULKAN_LIB}
            kernel32.lib user32.lib gdi32.lib
        )
        if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UM_VER)
            set_property(TARGET vulkan_tensor_upload_test APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64\"")
        endif()
        if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UCRT_VER)
            set_property(TARGET vulkan_tensor_upload_test APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UCRT_VER}/ucrt/x64\"")
        endif()
        if(DEFINED _MSVC_ROOT)
            set_property(TARGET vulkan_tensor_upload_test APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_MSVC_ROOT}/lib/x64\"")
        endif()
        message(STATUS "vulkan_tensor_upload_test: Vulkan tensor upload test configured")

        # =====================================================================
        # Vulkan GEMM Verification Test
        # Proves: compute shader dispatch → GPU GEMM → readback → CPU comparison
        # =====================================================================
        add_executable(vulkan_gemm_test
            tests/vulkan_gemm_test.cpp
        )
        set_target_properties(vulkan_gemm_test PROPERTIES
            EXCLUDE_FROM_ALL TRUE
            EXCLUDE_FROM_DEFAULT_BUILD TRUE
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )
        target_include_directories(vulkan_gemm_test PRIVATE
            ${RAWR_VULKAN_INCLUDE}
        )
        target_link_libraries(vulkan_gemm_test PRIVATE
            ${RAWR_VULKAN_LIB}
            kernel32.lib user32.lib gdi32.lib
        )
        if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UM_VER)
            set_property(TARGET vulkan_gemm_test APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64\"")
        endif()
        if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UCRT_VER)
            set_property(TARGET vulkan_gemm_test APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UCRT_VER}/ucrt/x64\"")
        endif()
        if(DEFINED _MSVC_ROOT)
            set_property(TARGET vulkan_gemm_test APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_MSVC_ROOT}/lib/x64\"")
        endif()
        message(STATUS "vulkan_gemm_test: Vulkan GEMM verification test configured")

        # =====================================================================
        # Vulkan Real-Dimension GEMM Test
        # Proves: compute shader dispatch at transformer-scale dimensions
        #         (e.g. 4096x4096x4096) with no RawrXD dependencies.
        # =====================================================================
        add_executable(vulkan_real_gemm_test
            tests/vulkan_real_gemm_test.cpp
        )
        set_target_properties(vulkan_real_gemm_test PROPERTIES
            EXCLUDE_FROM_ALL TRUE
            EXCLUDE_FROM_DEFAULT_BUILD TRUE
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )
        target_include_directories(vulkan_real_gemm_test PRIVATE
            ${RAWR_VULKAN_INCLUDE}
        )
        target_link_libraries(vulkan_real_gemm_test PRIVATE
            ${RAWR_VULKAN_LIB}
            kernel32.lib user32.lib gdi32.lib
        )
        if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UM_VER)
            set_property(TARGET vulkan_real_gemm_test APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64\"")
        endif()
        if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UCRT_VER)
            set_property(TARGET vulkan_real_gemm_test APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UCRT_VER}/ucrt/x64\"")
        endif()
        if(DEFINED _MSVC_ROOT)
            set_property(TARGET vulkan_real_gemm_test APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_MSVC_ROOT}/lib/x64\"")
        endif()
        message(STATUS "vulkan_real_gemm_test: Real-dimension GEMM test configured")

        # =====================================================================
        # Vulkan Transformer GEMM Integration Test
        # Proves: real Qwen GGUF → RawrXDInference → model loader → GPU-resident
        #         weight → Vulkan GEMM dispatch → verified output.
        # =====================================================================
        add_executable(vulkan_transformer_gemm_test
            tests/vulkan_transformer_gemm_test.cpp
        )
        set_target_properties(vulkan_transformer_gemm_test PROPERTIES
            EXCLUDE_FROM_ALL TRUE
            EXCLUDE_FROM_DEFAULT_BUILD TRUE
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_include_directories(vulkan_transformer_gemm_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/src/inference
            ${CMAKE_SOURCE_DIR}/src/engine
            ${CMAKE_SOURCE_DIR}/src/core
            ${CMAKE_SOURCE_DIR}/src/codec
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/masm
            ${RAWR_VULKAN_INCLUDE}
        )
        if(MSVC)
            target_link_directories(vulkan_transformer_gemm_test PRIVATE ${_RAWR_MSVC_LIBPATHS})
            foreach(_rawr_lib IN LISTS _RAWR_MSVC_LIBPATHS)
                if(EXISTS "${_rawr_lib}")
                    target_link_options(vulkan_transformer_gemm_test PRIVATE "/LIBPATH:\"${_rawr_lib}\"")
                endif()
            endforeach()
        endif()
        target_link_libraries(vulkan_transformer_gemm_test PRIVATE
            InferenceEngine
            ${RAWR_VULKAN_LIB}
            kernel32.lib
            user32.lib
            gdi32.lib
            shell32.lib
            ole32.lib
            oleaut32.lib
            uuid.lib
            comdlg32.lib
            advapi32.lib
            version.lib
        )
        if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UM_VER)
            set_property(TARGET vulkan_transformer_gemm_test APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64\"")
        endif()
        if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UCRT_VER)
            set_property(TARGET vulkan_transformer_gemm_test APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UCRT_VER}/ucrt/x64\"")
        endif()
        if(DEFINED _MSVC_ROOT)
            set_property(TARGET vulkan_transformer_gemm_test APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_MSVC_ROOT}/lib/x64\"")
        endif()
        message(STATUS "vulkan_transformer_gemm_test: Transformer GEMM integration test configured")

        # =====================================================================
        # Elastic Path Validation Test
        # Proves: ElasticEngine residency path is reached during transformer
        #         forward pass on a real model.
        # =====================================================================
        add_executable(elastic_path_validation_test
            tests/elastic_path_validation_test.cpp
        )
        set_target_properties(elastic_path_validation_test PROPERTIES
            EXCLUDE_FROM_ALL TRUE
            EXCLUDE_FROM_DEFAULT_BUILD TRUE
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_include_directories(elastic_path_validation_test PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/src/inference
            ${CMAKE_SOURCE_DIR}/src/engine
            ${CMAKE_SOURCE_DIR}/src/core
            ${CMAKE_SOURCE_DIR}/src/codec
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/masm
            ${RAWR_VULKAN_INCLUDE}
        )
        if(MSVC)
            target_link_directories(elastic_path_validation_test PRIVATE ${_RAWR_MSVC_LIBPATHS})
            foreach(_rawr_lib IN LISTS _RAWR_MSVC_LIBPATHS)
                if(EXISTS "${_rawr_lib}")
                    target_link_options(elastic_path_validation_test PRIVATE "/LIBPATH:\"${_rawr_lib}\"")
                endif()
            endforeach()
        endif()
        target_link_libraries(elastic_path_validation_test PRIVATE
            InferenceEngine
            ${RAWR_VULKAN_LIB}
            kernel32.lib
            user32.lib
            gdi32.lib
            shell32.lib
            ole32.lib
            oleaut32.lib
            uuid.lib
            comdlg32.lib
            advapi32.lib
            version.lib
        )
        if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UM_VER)
            set_property(TARGET elastic_path_validation_test APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64\"")
        endif()
        if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UCRT_VER)
            set_property(TARGET elastic_path_validation_test APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UCRT_VER}/ucrt/x64\"")
        endif()
        if(DEFINED _MSVC_ROOT)
            set_property(TARGET elastic_path_validation_test APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_MSVC_ROOT}/lib/x64\"")
        endif()
        message(STATUS "elastic_path_validation_test: Elastic path validation test configured")

        # =====================================================================
        # fused_qkv_gguf_validation: Full GGUF parse → elastic index → fused
        # QKV execution gate. Exercises all new code paths.
        # =====================================================================
        add_executable(fused_qkv_gguf_validation
            tests/fused_qkv_gguf_validation.cpp
        )
        set_target_properties(fused_qkv_gguf_validation PROPERTIES
            EXCLUDE_FROM_ALL TRUE
            EXCLUDE_FROM_DEFAULT_BUILD TRUE
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_include_directories(fused_qkv_gguf_validation PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/src/inference
            ${CMAKE_SOURCE_DIR}/src/engine
            ${CMAKE_SOURCE_DIR}/src/core
            ${CMAKE_SOURCE_DIR}/src/codec
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src/masm
            ${RAWR_VULKAN_INCLUDE}
        )
        if(MSVC)
            target_link_directories(fused_qkv_gguf_validation PRIVATE ${_RAWR_MSVC_LIBPATHS})
            foreach(_rawr_lib IN LISTS _RAWR_MSVC_LIBPATHS)
                if(EXISTS "${_rawr_lib}")
                    target_link_options(fused_qkv_gguf_validation PRIVATE "/LIBPATH:\"${_rawr_lib}\"")
                endif()
            endforeach()
        endif()
        target_link_libraries(fused_qkv_gguf_validation PRIVATE
            InferenceEngine
            ${RAWR_VULKAN_LIB}
            kernel32.lib
            user32.lib
            gdi32.lib
            shell32.lib
            ole32.lib
            oleaut32.lib
            uuid.lib
            comdlg32.lib
            advapi32.lib
            version.lib
        )
        if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UM_VER)
            set_property(TARGET fused_qkv_gguf_validation APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64\"")
        endif()
        if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UCRT_VER)
            set_property(TARGET fused_qkv_gguf_validation APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UCRT_VER}/ucrt/x64\"")
        endif()
        if(DEFINED _MSVC_ROOT)
            set_property(TARGET fused_qkv_gguf_validation APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_MSVC_ROOT}/lib/x64\"")
        endif()
        message(STATUS "fused_qkv_gguf_validation: Fused QKV + GGUF validation test configured")
    endif()

endif()

# =============================================================================
# VAL-051.2.A: Real Token Proof Harness
# =============================================================================
# Standalone test executable that validates the full RawrXDInference
# component chain produces real tokens from a GGUF model.
if(WIN32)
    add_executable(val_051_2_a_real_token
        tests/val_051_2_a_real_token.cpp
        src/rawrxd_inference.cpp
        src/rawrxd_model_loader.cpp
        src/rawrxd_tokenizer.cpp
        src/rawrxd_transformer.cpp
        src/rawrxd_sampler.cpp
        src/gguf_loader.cpp
        src/gguf_vocab_resolver.cpp
        src/core/inference_witness.cpp
        src/core/swarm_scheduler.cpp
        src/core/memory_stubs.cpp
        src/core/link_stubs.cpp
        src/core/gguf_swarm_plan_builder.cpp
        src/logging/Logger.cpp
    )
    set_target_properties(val_051_2_a_real_token PROPERTIES 
        EXCLUDE_FROM_ALL TRUE 
        EXCLUDE_FROM_DEFAULT_BUILD TRUE
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    target_include_directories(val_051_2_a_real_token PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include/core
    )
    # Vulkan SDK support for val_051_2_a_real_token (shares sources with rawrxd)
    if(RAWR_ENABLE_VULKAN AND RAWR_VULKAN_SDK_ROOT)
        target_include_directories(val_051_2_a_real_token PRIVATE "${RAWR_VULKAN_INCLUDE}")
        target_link_libraries(val_051_2_a_real_token PRIVATE "${RAWR_VULKAN_LIB}" dxgi)
        target_compile_definitions(val_051_2_a_real_token PRIVATE RAWR_HAS_VULKAN=1)
    elseif(RAWR_ENABLE_VULKAN AND VULKAN_INCLUDE_DIR)
        target_include_directories(val_051_2_a_real_token PRIVATE ${VULKAN_INCLUDE_DIR})
        target_link_libraries(val_051_2_a_real_token PRIVATE dxgi)
        target_compile_definitions(val_051_2_a_real_token PRIVATE RAWR_HAS_VULKAN=1)
    elseif(RAWR_ENABLE_VULKAN AND Vulkan_FOUND)
        target_include_directories(val_051_2_a_real_token PRIVATE ${Vulkan_INCLUDE_DIRS})
        target_link_libraries(val_051_2_a_real_token PRIVATE Vulkan::Vulkan dxgi)
        target_compile_definitions(val_051_2_a_real_token PRIVATE RAWR_HAS_VULKAN=1)
    elseif(RAWR_ENABLE_VULKAN)
        target_compile_definitions(val_051_2_a_real_token PRIVATE RAWR_HAS_VULKAN=0)
        target_link_libraries(val_051_2_a_real_token PRIVATE dxgi)
    else()
        target_link_libraries(val_051_2_a_real_token PRIVATE dxgi)
    endif()
    # Add LIBPATH directly using the SDK paths defined at top of file
    if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UM_VER)
        set_property(TARGET val_051_2_a_real_token APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64\"")
    endif()
    if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UCRT_VER)
        set_property(TARGET val_051_2_a_real_token APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UCRT_VER}/ucrt/x64\"")
    endif()
    if(DEFINED _MSVC_ROOT)
        set_property(TARGET val_051_2_a_real_token APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_MSVC_ROOT}/lib/x64\"")
    endif()
    # Use short library names - LIBPATH above provides the paths
    target_link_libraries(val_051_2_a_real_token PRIVATE
        kernel32 user32 gdi32 winspool shell32 ole32 oleaut32 uuid comdlg32 advapi32 version ws2_32
    )
    message(STATUS "VAL-051.2.A: Real Token Proof Harness configured")
endif()

# =============================================================================
# Dual GPU Smoke Test
# Validates both GPUs are detected and usable
# =============================================================================
if(WIN32)
    add_executable(dual_gpu_smoke_test
        tests/dual_gpu_smoke_test.cpp
    )
    set_target_properties(dual_gpu_smoke_test PROPERTIES 
        EXCLUDE_FROM_ALL TRUE 
        EXCLUDE_FROM_DEFAULT_BUILD TRUE
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    target_include_directories(dual_gpu_smoke_test PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UM_VER)
        set_property(TARGET dual_gpu_smoke_test APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64\"")
    endif()
    if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UCRT_VER)
        set_property(TARGET dual_gpu_smoke_test APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UCRT_VER}/ucrt/x64\"")
    endif()
    if(DEFINED _MSVC_ROOT)
        set_property(TARGET dual_gpu_smoke_test APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_MSVC_ROOT}/lib/x64\"")
    endif()
    # Vulkan SDK support for dual_gpu_smoke_test
    if(RAWR_ENABLE_VULKAN AND RAWR_VULKAN_SDK_ROOT)
        target_include_directories(dual_gpu_smoke_test PRIVATE "${RAWR_VULKAN_INCLUDE}")
        target_link_directories(dual_gpu_smoke_test PRIVATE "${RAWR_VULKAN_SDK_ROOT}/Lib")
        target_link_libraries(dual_gpu_smoke_test PRIVATE "${RAWR_VULKAN_LIB}" dxgi)
        target_compile_definitions(dual_gpu_smoke_test PRIVATE RAWR_HAS_VULKAN=1)
    elseif(RAWR_ENABLE_VULKAN AND VULKAN_INCLUDE_DIR)
        target_include_directories(dual_gpu_smoke_test PRIVATE ${VULKAN_INCLUDE_DIR})
        target_link_libraries(dual_gpu_smoke_test PRIVATE dxgi)
        target_compile_definitions(dual_gpu_smoke_test PRIVATE RAWR_HAS_VULKAN=1)
    elseif(RAWR_ENABLE_VULKAN AND Vulkan_FOUND)
        target_include_directories(dual_gpu_smoke_test PRIVATE ${Vulkan_INCLUDE_DIRS})
        target_link_libraries(dual_gpu_smoke_test PRIVATE Vulkan::Vulkan dxgi)
        target_compile_definitions(dual_gpu_smoke_test PRIVATE RAWR_HAS_VULKAN=1)
    else()
        target_compile_definitions(dual_gpu_smoke_test PRIVATE RAWR_HAS_VULKAN=0)
        target_link_libraries(dual_gpu_smoke_test PRIVATE dxgi)
    endif()
    target_link_libraries(dual_gpu_smoke_test PRIVATE
        kernel32 user32 gdi32 winspool shell32 ole32 oleaut32 uuid comdlg32 advapi32 version ws2_32
    )
    message(STATUS "Dual GPU Smoke Test configured")
endif()

# =============================================================================
# GGUF Inspector Tool
# Deep model architecture analysis for IDE integration
# =============================================================================
if(WIN32)
    add_executable(gguf_inspector
        tools/gguf_inspector.cpp
    )
    set_target_properties(gguf_inspector PROPERTIES 
        EXCLUDE_FROM_ALL TRUE 
        EXCLUDE_FROM_DEFAULT_BUILD TRUE
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    target_compile_features(gguf_inspector PRIVATE cxx_std_20)
    message(STATUS "GGUF Inspector tool configured")
endif()

# =============================================================================
# Comprehensive Dual GPU Test
# Full validation of multi-GPU inference pipeline
# =============================================================================
if(WIN32)
    add_executable(comprehensive_dual_gpu_test
        tests/comprehensive_dual_gpu_test.cpp
    )
    set_target_properties(comprehensive_dual_gpu_test PROPERTIES 
        EXCLUDE_FROM_ALL TRUE 
        EXCLUDE_FROM_DEFAULT_BUILD TRUE
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    target_include_directories(comprehensive_dual_gpu_test PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UM_VER)
        set_property(TARGET comprehensive_dual_gpu_test APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64\"")
    endif()
    if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UCRT_VER)
        set_property(TARGET comprehensive_dual_gpu_test APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UCRT_VER}/ucrt/x64\"")
    endif()
    if(DEFINED _MSVC_ROOT)
        set_property(TARGET comprehensive_dual_gpu_test APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_MSVC_ROOT}/lib/x64\"")
    endif()
    # Vulkan SDK support for comprehensive_dual_gpu_test
    if(RAWR_ENABLE_VULKAN AND RAWR_VULKAN_SDK_ROOT)
        target_include_directories(comprehensive_dual_gpu_test PRIVATE "${RAWR_VULKAN_INCLUDE}")
        target_link_directories(comprehensive_dual_gpu_test PRIVATE "${RAWR_VULKAN_SDK_ROOT}/Lib")
        target_link_libraries(comprehensive_dual_gpu_test PRIVATE "${RAWR_VULKAN_LIB}" dxgi)
        target_compile_definitions(comprehensive_dual_gpu_test PRIVATE RAWR_HAS_VULKAN=1)
    elseif(RAWR_ENABLE_VULKAN AND VULKAN_INCLUDE_DIR)
        target_include_directories(comprehensive_dual_gpu_test PRIVATE ${VULKAN_INCLUDE_DIR})
        target_link_libraries(comprehensive_dual_gpu_test PRIVATE dxgi)
        target_compile_definitions(comprehensive_dual_gpu_test PRIVATE RAWR_HAS_VULKAN=1)
    elseif(RAWR_ENABLE_VULKAN AND Vulkan_FOUND)
        target_include_directories(comprehensive_dual_gpu_test PRIVATE ${Vulkan_INCLUDE_DIRS})
        target_link_libraries(comprehensive_dual_gpu_test PRIVATE Vulkan::Vulkan dxgi)
        target_compile_definitions(comprehensive_dual_gpu_test PRIVATE RAWR_HAS_VULKAN=1)
    else()
        target_compile_definitions(comprehensive_dual_gpu_test PRIVATE RAWR_HAS_VULKAN=0)
        target_link_libraries(comprehensive_dual_gpu_test PRIVATE dxgi)
    endif()
    target_link_libraries(comprehensive_dual_gpu_test PRIVATE
        kernel32 user32 gdi32 winspool shell32 ole32 oleaut32 uuid comdlg32 advapi32 version ws2_32
    )
    message(STATUS "Comprehensive Dual GPU Test configured")
endif()

# =============================================================================
# OMEGA-1 Bridge Test Harness
# Validates IAT slots 64-75 for self-mutating engine integration
# =============================================================================
if(WIN32)
    add_executable(test_omega1_bridge
        tests/test_omega1_bridge.cpp
        src/omega1_modules/OmegaPowerShellBridge.cpp
    )
    set_target_properties(test_omega1_bridge PROPERTIES EXCLUDE_FROM_ALL TRUE EXCLUDE_FROM_DEFAULT_BUILD TRUE)
    target_include_directories(test_omega1_bridge PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/omega1_modules
    )
    if(MSVC)
        target_link_directories(test_omega1_bridge PRIVATE ${_RAWR_MSVC_LIBPATHS})
        foreach(_rawr_lib IN LISTS _RAWR_MSVC_LIBPATHS)
            if(EXISTS "${_rawr_lib}")
                target_link_options(test_omega1_bridge PRIVATE "/LIBPATH:\"${_rawr_lib}\"")
            endif()
        endforeach()
    endif()
    target_link_libraries(test_omega1_bridge PRIVATE
        Omega1Engine
        kernel32
        user32
        gdi32
        winspool
        shell32
        ole32
        oleaut32
        uuid
        comdlg32
        advapi32
        version
        ws2_32
    )
    if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UM_VER)
        target_link_libraries(test_omega1_bridge PRIVATE
            "${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64/kernel32.lib"
            "${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64/user32.lib"
            "${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64/gdi32.lib"
            "${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64/winspool.lib"
            "${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64/shell32.lib"
            "${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64/ole32.lib"
            "${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64/oleaut32.lib"
            "${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64/uuid.lib"
            "${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64/comdlg32.lib"
            "${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64/advapi32.lib"
            "${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64/version.lib"
            "${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64/ws2_32.lib"
        )
    endif()
    message(STATUS "OMEGA-1 Bridge Test Harness configured")
endif()

# =============================================================================
# K2 Router Injection Smoke Test
# Validates per-layer MoE router weight injection from real K2 GGUF tensors.
# Must pass before generation wiring.
# =============================================================================
if(WIN32)
    add_executable(Deep2Engine_RouterSmokeTest
        src/deep2/Deep2Engine_RouterSmokeTest.cpp
        src/deep2/GGUFLoader.cpp
        src/deep2/MoERouter.cpp
    )
    set_target_properties(Deep2Engine_RouterSmokeTest PROPERTIES
        EXCLUDE_FROM_ALL TRUE
        EXCLUDE_FROM_DEFAULT_BUILD TRUE
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    target_include_directories(Deep2Engine_RouterSmokeTest PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/deep2
        ${CMAKE_SOURCE_DIR}/include
    )
    if(MSVC)
        target_link_directories(Deep2Engine_RouterSmokeTest PRIVATE ${_RAWR_MSVC_LIBPATHS})
        foreach(_rawr_lib IN LISTS _RAWR_MSVC_LIBPATHS)
            if(EXISTS "${_rawr_lib}")
                target_link_options(Deep2Engine_RouterSmokeTest PRIVATE "/LIBPATH:\"${_rawr_lib}\"")
            endif()
        endforeach()
    endif()
    if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UM_VER)
        set_property(TARGET Deep2Engine_RouterSmokeTest APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64\"")
    endif()
    if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UCRT_VER)
        set_property(TARGET Deep2Engine_RouterSmokeTest APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UCRT_VER}/ucrt/x64\"")
    endif()
    if(DEFINED _MSVC_ROOT)
        set_property(TARGET Deep2Engine_RouterSmokeTest APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_MSVC_ROOT}/lib/x64\"")
    endif()
    target_link_libraries(Deep2Engine_RouterSmokeTest PRIVATE
        kernel32 user32 gdi32 winspool shell32 ole32 oleaut32 uuid comdlg32 advapi32 version ws2_32
    )
    message(STATUS "K2 Router Injection Smoke Test configured")
endif()

# Resource compiler for icon/version (Windows only)
if(WIN32)
    enable_language(RC)
endif()

message(STATUS "RawrXD v14.7.3 Target Configured - Zero Bloat Mode")

# Default to Release builds (prevents /RTC1 + /O2 clash on Ninja single-config)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    message(STATUS "[Build] Defaulting CMAKE_BUILD_TYPE to Release")
endif()

# CXX standard set above (MSVC 23, others 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static link MSVC runtime — required for RawrEngine and Gold builds
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)

# =============================================================================
# Persistent SDK Include/Lib Paths (Phase 41 Build Fix)
# =============================================================================
# set(ENV{...}) only works during configure. These persist into build commands.
if(WIN32 AND MSVC)
    # Auto-detect SDK root: try D: first, fall back to C:
    if(EXISTS "D:/Program Files (x86)/Windows Kits/10")
        set(_WIN10_SDK_ROOT "D:/Program Files (x86)/Windows Kits/10")
    else()
        set(_WIN10_SDK_ROOT "C:/Program Files (x86)/Windows Kits/10")
    endif()
    set(_SDK_VER "10.0.22621.0")
    # Auto-detect MSVC Build Tools root
    if(DEFINED _MSVC_ROOT AND EXISTS "${_MSVC_ROOT}")
        set(_MSVC_BT_ROOT "${_MSVC_ROOT}")
    elseif(EXISTS "D:/VS2022Enterprise/VC/Tools/MSVC")
        file(GLOB _BT_VERS "D:/VS2022Enterprise/VC/Tools/MSVC/*")
        list(SORT _BT_VERS)
        list(GET _BT_VERS -1 _MSVC_BT_ROOT)
    elseif(EXISTS "C:/VS2022Enterprise/VC/Tools/MSVC")
        file(GLOB _BT_VERS "C:/VS2022Enterprise/VC/Tools/MSVC/*")
        list(SORT _BT_VERS)
        list(GET _BT_VERS -1 _MSVC_BT_ROOT)
    else()
        set(_MSVC_BT_ROOT "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.44.35207")
    endif()

    include_directories(SYSTEM
        "${_MSVC_BT_ROOT}/include"
        "${_WIN10_SDK_ROOT}/Include/${_SDK_VER}/ucrt"
        "${_WIN10_SDK_ROOT}/Include/${_SDK_VER}/shared"
        "${_WIN10_SDK_ROOT}/Include/${_SDK_VER}/um"
        "${_WIN10_SDK_ROOT}/Include/${_SDK_VER}/winrt"
        "D:/rawrxd/Ship/webview2/build/native/include"
    )
    link_directories(
        "${_MSVC_BT_ROOT}/lib/x64"
        "${_MSVC_BT_ROOT}/lib/onecore/x64"
        "${_WIN10_SDK_ROOT}/Lib/${_SDK_VER}/ucrt/x64"
        "${_WIN10_SDK_ROOT}/Lib/${_SDK_VER}/um/x64"
        "D:/rawrxd/Ship/webview2/build/native/x64"
    )
    message(STATUS "[Build Paths] MSVC CRT: ${_MSVC_BT_ROOT}")
endif()

# =============================================================================
# Intent Guardrails System - Toggleable Features
# =============================================================================
include(cmake/IntentGuardrails.cmake)

# =============================================================================
# MASM64 Assembly Kernel Support
# =============================================================================
# Enable ASM_MASM only on MSVC (ml64.exe required).
# MinGW builds skip ASM kernels and use C++ fallbacks.
if(MSVC)
    enable_language(ASM_MASM)
    set(CMAKE_ASM_MASM_FLAGS "/c /Zi /Zd /I${CMAKE_CURRENT_SOURCE_DIR}/src/asm")
    set(RAWR_HAS_MASM TRUE)
    # These two ASM modules currently rely on `rawrxd_win64.inc` sovereign resolver blocks
    # that do not assemble cleanly under ml64; Win32IDE uses C++ stubs for now.
    message(STATUS "MASM64 enabled: ASM kernels will be compiled")
else()
    set(RAWR_HAS_MASM FALSE)
    message(STATUS "MASM64 not available (non-MSVC): using C++ fallbacks")
endif()
if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
    if(NOT WIN32)
        message(FATAL_ERROR "[ProductionLane] RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON requires WIN32 toolchain/targets.")
    endif()
    if(NOT MSVC)
        message(FATAL_ERROR "[ProductionLane] RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON requires MSVC for strict production parity.")
    endif()
    if(NOT RAWR_HAS_MASM)
        message(FATAL_ERROR "[ProductionLane] RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON requires RAWR_HAS_MASM=TRUE.")
    endif()
    if(NOT CMAKE_CONFIGURATION_TYPES)
        if(NOT CMAKE_BUILD_TYPE)
            message(FATAL_ERROR "[ProductionLane] CMAKE_BUILD_TYPE must be set to Release when RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON.")
        endif()
        string(TOUPPER "${CMAKE_BUILD_TYPE}" _RAWR_BUILD_TYPE_UPPER)
        if(NOT _RAWR_BUILD_TYPE_UPPER STREQUAL "RELEASE")
            message(FATAL_ERROR "[ProductionLane] CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} is incompatible with RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON. Use Release.")
        endif()
    else()
        message(FATAL_ERROR "[ProductionLane] RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON requires a single-config generator with CMAKE_BUILD_TYPE=Release.")
    endif()
endif()

# ASM kernel sources (shared between RawrEngine and Win32IDE)
if(RAWR_HAS_MASM)
    set(ASM_KERNEL_SOURCES
        src/asm/rawr_globals.asm
        src/asm/rawr_cpu_features.asm
        src/asm/inference_core.asm
        src/asm/FlashAttention_AVX512.asm
        src/asm/quant_avx2.asm
        src/asm/RawrXD_KQuant_Dequant.asm
        src/asm/RawrXD_AVX512_Dequant_BPE.asm
        src/asm/RawrXD_PDBKernel_v3.asm
        src/asm/RawrXD_ModuleEngine.asm
        src/asm/request_patch.asm
        src/asm/inference_kernels.asm
        src/asm/model_bridge_x64.asm
        src/asm/model_streamer_x64.asm
        src/asm/RawrXD_DualAgent_Orchestrator.asm
        src/asm/RawrXD_DiskRecoveryAgent.asm
        src/asm/disk_recovery_scsi.asm
        # src/asm/RawrXD-AnalyzerDistiller.asm      # excluded: ml64 include errors (rawrxd_win64.inc segment nesting)
        # src/asm/RawrXD-StreamingOrchestrator.asm  # excluded: ml64 include errors (rawrxd_win64.inc segment nesting)
        src/asm/RawrXD_Telemetry_Kernel.asm
        src/asm/RawrXD_Prometheus_Exporter.asm
        src/asm/RawrXD_SelfPatch_Agent.asm
        src/asm/RawrXD_SourceEdit_Kernel.asm
        src/asm/feature_dispatch_bridge.asm
        src/asm/gui_dispatch_bridge.asm
        src/asm/VulkanKernel_DispatchRaw.asm
        # src/asm/RawrXD_CopilotGapCloser.asm   # excluded: scaffold/invalid ml64 syntax (__real@*, missing .ENDPROLOG)
        # src/asm/native_speed_kernels.asm      # excluded: scaffold/invalid ml64 syntax (__real@*, missing .ENDPROLOG)
        # src/asm/DirectML_Bridge.asm           # excluded: scaffold/invalid ml64 syntax (__real@*, missing .ENDPROLOG)
        # src/asm/RawrXD_Hotpatch_Kernel.asm    # handled separately below
        # src/asm/RawrXD_Snapshot.asm           # handled separately below
        # src/asm/RawrXD_Pyre_Compute.asm       # handled separately below
        # Pure MASM inference kernels (zero dependencies, no GGML)
        src/masm/rawrxd_math_masm.asm
        src/masm/rawrxd_transformer_masm_fixed.asm
        src/masm/rawrxd_transformer_full.asm
        # Sovereign ABI bridge — stable C++/MASM execution plane seam
        src/asm/Sovereign_KernelBridge_MASM.asm
        # src/asm/vision_projection_kernel.asm  # excluded: scaffold/invalid ml64 syntax (__real@*, missing .ENDPROLOG)
        # src/asm/quantum_beaconism_backend.asm        # excluded: MASM build errors (undefined symbols / unwind prolog issues)
        # src/asm/RawrXD_DualEngine_QuantumBeacon.asm  # excluded: MASM build errors (undefined symbols / unwind prolog issues)
        # src/asm/RawrXD_DualEngine_Pure.asm    # excluded: ml64 errors (operand size mismatch)
        src/asm/RawrXD_OverclockGov_Pure.asm
        src/asm/RawrXD_Emitter_v1.asm
        src/asm/RawrXD_IPC_Dispatcher.asm
        src/asm/RawrXD_AuditSystem_Pure.asm
        src/asm/Advanced_Code_Deobfuscator.asm
        src/asm/Code_Pattern_Reconstructor.asm
        src/asm/agentic_deep_thinking_kernels.asm
        # Deep2 MoE kernels (2K TPS path)
        src/deep2/sovereign_deep2_kernels.asm
        src/deep2/sovereign_q4k_gemv.asm
        src/deep2/sovereign_moe_fused.asm
        # Sovereign Puppeteer Architecture - Self-Modification System
        src/sovereign/puppeteer/Puppeteer_CaptureState.asm
        # src/asm/RawrXD_MissingSymbols.asm  # excluded: duplicates symbols from gold_link_stubs.cpp
        # src/asm/byte_search.asm  # excluded: duplicate symbols in rawr_engine_link_shims.cpp
        # src/asm/memory_patch.asm  # excluded: duplicate symbols in rawr_engine_link_shims.cpp
        # src/x64/rawrxd_quantum_beaconism.asm  # excluded: MASM syntax errors (scaffold file)
        # src/ai/digestion_scanner.asm  # excluded: MASM syntax errors (scaffold file)
        # Previously broken MASM64 syntax now portable C++20 with extern "C" ABI compatibility.

        
        # Keep C++ reverse_engineered_stubs.cpp as single source of truth for these symbols.
        # ASM unresolved stubs duplicate AllocateDMABuffer and cause LNK2005 in Win32IDE.
        # src/unresolved_asm_stubs.asm
        # User-provided production ASM (need ML64 syntax fixes: .endprolog, REAL4, scale): agentic_deep_thinking_kernels.asm, ai_completion_provider_masm.asm, agentic_puppeteer_byte_ops.asm
        # src/RawrXD_Complete_ReverseEngineered.asm  # excluded: fragment missing struct defs/includes
    )
    list(REMOVE_DUPLICATES ASM_KERNEL_SOURCES)
    # Mark ASM files with MASM language property
    set_source_files_properties(${ASM_KERNEL_SOURCES} PROPERTIES LANGUAGE ASM_MASM)
else()
    set(ASM_KERNEL_SOURCES "")
endif()

# Manual validation target: compile a tiny compute shader to SPIR-V for raw dispatch ABI testing.
set(RAWRXD_TEST_SHADER_SRC "${CMAKE_SOURCE_DIR}/src/shaders/test_vec_add.comp")
set(RAWRXD_TEST_SHADER_DIR "${CMAKE_BINARY_DIR}/shaders")
set(RAWRXD_TEST_SHADER_SPV "${RAWRXD_TEST_SHADER_DIR}/test_vec_add.spv")

add_custom_command(
    OUTPUT ${RAWRXD_TEST_SHADER_SPV}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${RAWRXD_TEST_SHADER_DIR}
    COMMAND glslc -fshader-stage=compute --target-env=vulkan1.2 -O
            ${RAWRXD_TEST_SHADER_SRC}
            -o ${RAWRXD_TEST_SHADER_SPV}
    DEPENDS ${RAWRXD_TEST_SHADER_SRC}
    COMMENT "Generating test SPIR-V shader: test_vec_add.spv"
    VERBATIM
)

add_custom_target(rawrxd-test-spv DEPENDS ${RAWRXD_TEST_SHADER_SPV})

# MatMul benchmark target: compile production GEMM compute shader to SPIR-V.
set(RAWRXD_MATMUL_SHADER_SRC "${CMAKE_SOURCE_DIR}/src/gpu/shader_matmul.comp")
set(RAWRXD_MATMUL_SHADER_SPV "${RAWRXD_TEST_SHADER_DIR}/shader_matmul.spv")

add_custom_command(
    OUTPUT ${RAWRXD_MATMUL_SHADER_SPV}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${RAWRXD_TEST_SHADER_DIR}
    COMMAND glslc -fshader-stage=compute --target-env=vulkan1.2 -O
            ${RAWRXD_MATMUL_SHADER_SRC}
            -o ${RAWRXD_MATMUL_SHADER_SPV}
    DEPENDS ${RAWRXD_MATMUL_SHADER_SRC}
    COMMENT "Generating MatMul SPIR-V shader: shader_matmul.spv"
    VERBATIM
)

add_custom_target(rawrxd-matmul-spv DEPENDS ${RAWRXD_MATMUL_SHADER_SPV})

set(_RAWR_ASM_FEATURE_DEFINITIONS
    RAWR_HAS_MASM=$<IF:$<BOOL:${RAWR_HAS_MASM}>,1,0>
    RAWRXD_LINK_QUADBUFFER_ASM=$<IF:$<BOOL:${RAWR_HAS_MASM}>,1,0>
    RAWRXD_LINK_INFERENCE_KERNELS_ASM=$<IF:$<BOOL:${RAWR_HAS_MASM}>,1,0>
    RAWRXD_LINK_ANALYZER_DISTILLER_ASM=0
    RAWRXD_LINK_STREAMING_ORCHESTRATOR_ASM=0
    RAWRXD_LINK_TELEMETRY_KERNEL_ASM=$<IF:$<BOOL:${RAWR_HAS_MASM}>,1,0>
    RAWRXD_LINK_PROMETHEUS_EXPORTER_ASM=$<IF:$<BOOL:${RAWR_HAS_MASM}>,1,0>
    RAWRXD_LINK_SELFPATCH_AGENT_ASM=$<IF:$<BOOL:${RAWR_HAS_MASM}>,1,0>
    RAWRXD_LINK_SOURCEEDIT_KERNEL_ASM=$<IF:$<BOOL:${RAWR_HAS_MASM}>,1,0>
    RAWRXD_LINK_VISION_PROJECTION_ASM=0
    RAWRXD_LINK_REVERSE_ENGINEERED_ASM=$<IF:$<BOOL:${RAWR_HAS_MASM}>,1,0>
    RAWRXD_LINK_DUAL_ENGINE_QUANTUM_BEACON_ASM=0
    RAWRXD_LINK_UNIFIED_OVERCLOCK_GOVERNOR_ASM=0
    RAWRXD_LINK_CODEBASE_AUDIT_SYSTEM_ASM=0
    RAWRXD_LINK_ADVANCED_DEOBFUSCATOR_ASM=$<IF:$<BOOL:${RAWR_HAS_MASM}>,1,0>
    RAWRXD_LINK_PATTERN_RECONSTRUCTOR_ASM=$<IF:$<BOOL:${RAWR_HAS_MASM}>,1,0>
)
if(TARGET rawrxd)
    target_compile_definitions(rawrxd PRIVATE ${_RAWR_ASM_FEATURE_DEFINITIONS})
endif()

find_package(Threads REQUIRED)

set(SOURCES 
    src/main.cpp
    src/memory_core.cpp
    src/hot_patcher.cpp
    src/cpu_inference_engine.cpp
    src/rawrxd_inference.cpp
    src/rawrxd_model_loader.cpp
    src/rawrxd_transformer.cpp
    src/runtime/TensorExecutionRouter.cpp
    src/runtime/RawrRuntime.cpp
    src/runtime/memory/PlacementPolicy.cpp
    src/runtime/memory/ResidencyTracker.cpp
    src/runtime/memory/WorkingSetPredictor.cpp
    src/runtime/memory/TransferScheduler.cpp
    src/runtime/memory/TensorPlacementManager.cpp
    src/runtime/memory/PredictiveMemoryManager.cpp
    src/runtime/memory/CapacityManager.cpp
    src/runtime/StreamRouterAdapter.cpp
    src/runtime/Deep2ExecutionTelemetry.cpp
    src/runtime/gguf_tensor_loader.cpp
    src/runtime/elastic/ElasticEngine.cpp
    src/runtime/elastic/ElasticGGUFIndex.cpp
    src/runtime/elastic/ElasticResidencyManager.cpp
    src/runtime/elastic/VulkanTensorResidencyBackend.cpp
    src/runtime/elastic/ElasticDecoder.cpp
    src/runtime/governance/HardwareCapabilityProbe.cpp
    src/runtime/governance/CapabilityTierMatrix.cpp
    src/runtime/governance/HardwareGovernor.cpp
    src/runtime/governance/PressedStateEngine.cpp
    src/runtime/governance/Ensemble58Governor.cpp
    src/runtime/governance/TLSInstructionGate.cpp
    src/runtime/governance/UnifiedTriggerOrchestrator.cpp
    B014/build/b014_profiler.cpp
    src/rawrxd_tokenizer.cpp
    src/rawrxd_sampler.cpp
    src/complete_server.cpp
    src/agentic_engine.cpp
    src/subagent_core.cpp
    src/agent_history.cpp
    src/agent_policy.cpp
    src/agent_explainability.cpp
    src/streaming_gguf_loader.cpp
    src/utils/Diagnostics.cpp
    src/gguf_loader.cpp
    src/gguf_vocab_resolver.cpp
    src/compression_interface.cpp
    src/core/memory_production.cpp
    src/core/rawrxd_subsystem_api.cpp
    src/core/rawrxd_linker_closure.cpp
    # src/infinite/InfinitePerfectionEngine.cpp  # excluded: 100+ compilation errors
    # src/swarm/SovereignSwarm.cpp  # excluded: large file, use linker stubs instead
    # src/core/rawrxd_subsystem_api_impl.cpp  # excluded: compilation errors, use rawrxd_subsystem_api.cpp instead
    # src/core/link_stubs_production.cpp  # excluded: duplicate symbols with rawrxd_subsystem_api.cpp
    src/vulkan_kernel_stubs.cpp
    src/reverse/ReverseEngine.cpp
    src/reverse/ReverseModelLoader.cpp
    src/compression/zlib_runtime_loader.cpp
    src/deep2/MASMKernelStubs.cpp
    src/inference/NegativeSpaceProfiler.cpp
    
    src/engine/gguf_core.cpp
    src/engine/inference_kernels.cpp
    src/engine/transformer.cpp
    src/engine/bpe_tokenizer.cpp
    src/engine/sampler.cpp
    src/engine/rawr_engine.cpp
    src/engine/core_generator.cpp
    src/engine/pyre_compute.cpp
    src/engine/sovereign_engines.cpp
    
    src/core/execution_scheduler.cpp
    
    # Agentic Decision Tree + CLI Autonomy (Phase 19)
    src/cli/agentic_decision_tree.cpp
    src/cli/cli_autonomy_loop.cpp
    src/cli/deep_iteration_engine.cpp
    
    # Headless CLI Systems (Phase 20: Safety, Confidence, Replay, Governor, Multi-Response, History, Policy, Explainability)
    src/cli/cli_headless_systems.cpp
    src/core/agent_safety_contract.cpp
    src/core/confidence_gate.cpp
    src/core/deterministic_replay.cpp
    src/core/execution_governor.cpp
    # src/core/multi_response_engine.cpp  # excluded: methods defined inline in win32app/multi_response_engine.h
    src/core/streaming_engine_registry.cpp
    src/core/flash_attention.cpp
    src/core/gpu_backend_bridge.cpp
    
    # Native speed layer + local AI core (zero-dependency inference)
    src/core/native_speed_layer.cpp
    src/core/local_ai_core.cpp
    src/core/native_inference_pipeline.cpp
    
    # Layer Offload Manager — RAM streaming for 74B+ models on 16GB VRAM
    src/core/layer_offload_manager.cpp
    
    # Three-layer hotpatch system (needed by decision tree)
    src/core/model_memory_hotpatch.cpp
    # src/core/byte_level_hotpatcher.cpp  # excluded: patch_bytes and search_and_patch_bytes defined in link_stubs_production.cpp
    src/core/unified_hotpatch_manager.cpp
    src/core/proxy_hotpatcher.cpp
    src/core/model_bruteforce_engine.cpp
    src/server/gguf_server_hotpatch.cpp
    src/agent/agentic_hotpatch_orchestrator.cpp

    # Singleton subsystems referenced by UnifiedHotpatchManager
    src/core/agentic_task_graph.cpp
    src/core/embedding_engine.cpp
    src/core/vision_encoder.cpp

    # Vision Pipeline Extensions (Token Gating, Embedding Cache, KV Isolation, Quantized Encoder, GPU Staging)
    src/core/vision_token_gate.cpp
    src/core/vision_embedding_cache.cpp
    src/core/vision_kv_isolation.cpp
    src/core/vision_quantized_encoder.cpp
    src/core/vision_gpu_staging.cpp

    src/marketplace/extension_marketplace.cpp
    src/auth/rbac_engine.cpp

    # Shadow-Page Detour + Sentinel Watchdog (Layer 6)
    src/core/shadow_page_detour.cpp
    src/core/sentinel_watchdog.cpp

    # Agentic Loop State (iterative reasoning state machine)
    src/agentic_loop_state.cpp

    # Autonomous SubAgents — Bulk Fix Orchestration for models/swarms
    src/agent/autonomous_subagent.cpp
    src/agent/agentic_failure_detector.cpp
    src/agent/agentic_puppeteer.cpp

    # ═══════════════════════════════════════════════════════════════════
    # Sovereign Puppeteer Architecture — Self-Modification System
    # ═══════════════════════════════════════════════════════════════════
    src/sovereign/introspection/SymbolTableGenerator.cpp
    src/sovereign/puppeteer/PuppeteerAPI.cpp
    src/sovereign/puppeteer/VEH_Watchdog.cpp

    # ═══════════════════════════════════════════════════════════════════
    # Intent-to-Execution Guardrails — Model Control Plane
    # ═══════════════════════════════════════════════════════════════════
    src/intent/intent_config.cpp
    src/intent/intent_abi.cpp
    src/guardrails/capability_policy.cpp
    src/guardrails/patch_firewall.cpp
    src/hotpatch/patch_transaction.cpp

    # ═══════════════════════════════════════════════════════════════════
    # Sovereign Agent Kernel — Orchestration Layer
    # ═══════════════════════════════════════════════════════════════════
    src/kernel/AgentKernel.cpp
    src/kernel/TelemetryInjector.cpp
    src/kernel/IntentExecutionPipeline.cpp
    src/kernel/IntentReplayEngine.cpp

    # HTTP Client — WinHTTP/curl async with future retention
    src/agent/llm_http_client.cpp
    src/agent/model_invoker.cpp
    src/agent/planner.cpp
    src/agent/eval_framework.cpp
    # src/ollama_client.cpp  # REMOVED: Ollama backend replaced with native GGUF loader

    # LocalReasoningEngine — API-free static analysis + pattern detection (no Qt, no API key)
    src/agent/local_reasoning_engine.cpp

    # Agent Self-Repair (MASM64 self-patching kernel bridge)
    src/agent/agent_self_repair.cpp
    src/agent/agent_self_healing_orchestrator.cpp

    # T3-D: Deterministic Replay Engine (DivergenceEvent + TelemetrySnapshot)
    src/agentic/DeterministicReplayEngine.cpp
    src/agentic/AgentToolHandlers.cpp

    # T4: Autonomous Recovery Orchestrator (FailureClass → recovery → verify → commit)
    src/agentic/autonomous_recovery_orchestrator.cpp

    # CoT Fallback System (required by unified_hotpatch_manager.cpp)
    src/core/cot_fallback_system.cpp
    
    # T4.5: Agentic Deep Thinking Engine — Autonomous monitoring, recovery, and orchestration
    src/agentic/AgenticDeepThinkingEngine.cpp

    # T4.6: Validation Fault Injection and Recovery Telemetry (Mission 2.1-2.3)
    validation/fault_injection/FaultInjector.cpp
    validation/fault_injection/WorkerCrashInjector.cpp
    validation/fault_injection/MemoryPressureInjector.cpp
    validation/fault_injection/ServiceKillInjector.cpp
    validation/fault_injection/StateCorruptionInjector.cpp
    validation/fault_injection/ExceptionStormInjector.cpp
    validation/recovery_telemetry/RecoveryTelemetry.cpp

    # T5: Agent Orchestrator + Bridge + Navigation (agentic brain wiring)
    src/agentic/AgentOrchestrator.cpp
    src/agentic/OrchestratorBridge.cpp
    src/agentic/AgenticNavigator.cpp
    src/agentic/RawrXD_ToolRegistry.cpp
    src/agentic/ToolRegistry.cpp
    src/agentic/RawrXD_AgentLoop.cpp
    src/agentic/agentic_executor.cpp

    # T6: Autonomous Repair Orchestrator + Agent Memory Indexer
    src/core/auto_repair_orchestrator.cpp
    src/core/agent_memory_indexer.cpp

    # Disk Recovery Agent (C++ wrapper for ASM SCSI kernel)
    src/agent/DiskRecoveryAgent.cpp
    
    # ═══════════════════════════════════════════════════════════════════
    # IDE Subsystems — Repository Intelligence, Build, Diagnostics, etc.
    # ═══════════════════════════════════════════════════════════════════
    src/repo/FileIndex.cpp
    src/repo/SymbolTable.cpp
    src/repo/ASTCache.cpp
    src/repo/TypeDatabase.cpp
    src/repo/CallGraph.cpp
    src/repo/IncludeGraph.cpp
    src/repo/SearchIndex.cpp
    src/repo/EmbeddingIndex.cpp
    src/build/BuildIntelligence.cpp
    src/diagnostics/DiagnosticsEngine.cpp
    src/refactoring/RefactoringEngine.cpp
    src/validation/PatchValidator.cpp
    src/ide/IDEShell.cpp
    
    # Phase 21: Distributed Swarm Bridge + Universal Model Hotpatcher
    src/core/swarm_decision_bridge.cpp
    src/core/universal_model_hotpatcher.cpp
    
    # Phase 22: Production Release Engineering
    src/core/production_release.cpp
    
    # Phase 20: WebRTC P2P Signaling
    src/core/webrtc_signaling.cpp
    
    # Phase 23: GPU Kernel Auto-Tuner
    src/core/gpu_kernel_autotuner.cpp
    
    # Phase 24: Windows Sandbox Integration
    src/core/sandbox_integration.cpp
    
    # Phase 25: AMD GPU Acceleration (RX 7800 XT - actual hardware)
    src/core/amd_gpu_accelerator.cpp
    src/core/unified_memory_executor.cpp
    src/core/model_runtime_gate.cpp
    src/runtime_surface/GenerationStopwatch.cpp
    
    # Accelerator implementations (required by accelerator_router.cpp)
    src/core/intel_gpu_accelerator.cpp
    src/core/arm64_gpu_accelerator.cpp
    src/core/cerebras_wse_accelerator.cpp
    
    # Phase 30: Unified Accelerator Router
    src/core/accelerator_router.cpp
    
    # Phase 29.2: GSI Hash Table + TPI Type Parser
    src/core/pdb_gsi_hash.cpp
    
    # Phase 29: Native PDB Parser (required by pdb_reference_provider)
    src/core/pdb_native.cpp
    
    # Phase 29.2: Multi-Reference Provider + Tools/Instructions Hook
    src/core/pdb_reference_provider.cpp
    
    # Missing link dependencies (enterprise license, swarm, flash attn stubs, tool registry)
    src/core/enterprise_license.cpp
    src/core/enterprise_licensev2_impl.cpp
    src/core/camellia256_bridge.cpp
    src/core/swarm_coordinator.cpp
    src/core/swarm_worker.cpp
    src/tool_registry.cpp

    # License enforcement + feature manifest (LicenseEnforcer, g_FeatureManifest, featureName) — required by complete_server, hotpatch, perf_telemetry, enterprise_licensev2_impl
    src/license_enforcement.cpp
    src/feature_flags_runtime.cpp
    src/tools/license_validator_manifest.cpp

    # Agentic config + enterprise feature manager + model name utils — required by complete_server, subagent_core
    src/core/agentic_autonomous_config.cpp
    src/core/enterprise_feature_manager.cpp
    src/model_name_utils.cpp
    src/model_config.cpp

    # Multi-GPU manager (RawrXD::Enterprise::MultiGPUManager) — required by complete_server, execution_scheduler, local_ai_core
    src/core/multi_gpu.cpp

    # Engine registry (register_engine) — required by sovereign_engines
    src/core/engine_registry.cpp

    # Telemetry + observability — required by model_bruteforce_engine, auto_feature_registry, AgentOrchestrator
    src/agent/telemetry_collector.cpp
    src/agentic_observability.cpp

    # BoundedAgentLoop + FIMPromptBuilder — required by agentic_task_graph, OrchestratorBridge, auto_feature_registry, AgentOrchestrator
    src/agentic/BoundedAgentLoop.cpp
    src/agentic/FIMPromptBuilder.cpp

    # Subsystem registry — required by AgentOrchestrator, ToolRegistry, feature_handlers
    src/core/rawrxd_subsystem_api.cpp

    # IDEConfig — required by OrchestratorBridge
    src/config/IDEConfig.cpp

    # Disk recovery ASM agent + tool handler — required by ToolRegistry
    src/agentic/DiskRecoveryAgent.cpp
    src/agentic/DiskRecoveryToolHandler.cpp

    # Agentic bridge (ExecuteAgentCommand) — headless stub for RawrEngine
    src/win32app/agentic_bridge_headless.cpp

    # Agent failure detect SIMD implementation (masm_agent_failure_detect_simd) — required by agentic_failure_detector
    src/core/ai_agent_masm_core_impl.cpp

    # Context deterioration hotpatch + LSP hotpatch + symbol provider — required by feature_handlers, auto_feature_registry
    src/core/context_deterioration_hotpatch.cpp
    src/lsp/lsp_hotpatch_bridge.cpp
    src/lsp/hotpatch_symbol_provider.cpp

    # License anti-tampering (computeHMAC_SHA256, verifyLicenseKeyIntegrity) — required by enterprise_licensev2_impl
    src/core/license_anti_tampering.cpp

    # Reverse-engineered bridge stubs (Heartbeat_Shutdown, GPU_*, GetHighResTick, TicksTo*, CalculateCRC32, AllocateDMABuffer, Tensor_QuantizedMatMul)
    # stub file removed

    # Support tier manager (SupportTierManager::Instance, GenerateStatusReport) — required by main, complete_server
    src/core/support_tier.cpp

    # Subsystem mode stubs (CompileMode, EncryptMode, InjectMode, etc.) — required by rawrxd_subsystem_api
    # removed stub unit

    # Feature registry (FeatureRegistry::instance, registerFeature) — required by enterprise_feature_manager
    src/core/feature_registry.cpp
    # Menu auditor (RawrXD::Audit::verifyCommandInMenu) — required by feature_registry
    src/core/menu_auditor.cpp

    # GGUF diagnostic provider + LSP server — required by lsp_hotpatch_bridge
    src/lsp/gguf_diagnostic_provider.cpp
    src/lsp/RawrXD_LSPServer.cpp

    # Extension polyfill engine + VSCode API — required by js_extension_host
    src/core/extension_polyfill_engine.cpp
    src/modules/vscode_extension_api.cpp

    # JS extension host — required by ssot_handlers_ext
    src/core/js_extension_host.cpp

    # Model source resolver — required by auto_feature_registry
    src/model_source_resolver.cpp

    # ReverseEngineering (NativeDisassembler, BinaryAnalyzer, RECodex, NativeCompiler) — required by auto_feature_registry
    src/modules/ReverseEngineering.cpp

    # AgenticDeepThinkingEngine full implementation
    src/agent/agentic_deep_thinking_engine.cpp

    # Quantum Autonomous Systems - Real implementations (required by agentic_deep_thinking_engine)
    src/agent/quantum_autonomous_todo_system.cpp
    src/agent/quantum_multi_model_agent_cycling.cpp
    src/agent/quantum_dynamic_time_manager.cpp
    src/agent/quantum_missing_impl.cpp
    src/agent/quantum_production_orchestrator.cpp
    src/agent/quantum_agent_orchestrator.cpp

    # Win32IDE::logMessage fallback (for RawrEngine / Gold when linking TranscendencePanel without full IDE)
    src/win32app/Win32IDE_logMessage.cpp
    # Win32IDE output/copilot/tree stubs (for RawrEngine / Gold when linking vscode_extension_api without full IDE)

    # Stubgen: AnalyzerDistiller + StreamingOrchestrator (C++ fallbacks, guarded by RAWR_HAS_MASM)

    # PT Driver Contract — Page Table substrate (VEH watchpoints, COW snapshots, ASLR, large pages)
    src/core/pt_driver_contract.cpp

    # Phase 21: Distributed Swarm Inference Orchestrator
    src/cli/swarm_orchestrator.cpp
    src/swarm_orchestrator.cpp

    # Phase 33: Voice Chat Engine
    src/core/voice_chat.cpp

    # Phase 44: Voice Automation — Toggleable TTS Response Reader
    src/core/voice_automation.cpp

    # DirectML GPU Inference Engine — D3D12 + DML operators for transformer forward pass
    src/core/directml_compute.cpp
    src/core/gguf_dml_bridge.cpp
    src/core/dml_streaming_integration.cpp
    src/core/dml_asm_fallback.cpp
    src/dml_inference_engine.cpp

    # Agentic Watchdog — .text Section Integrity Verification Service
    src/core/watchdog_service.cpp

    # Phase 33: Quick-Win Ports
    src/core/shortcut_manager.cpp
    src/core/backup_manager.cpp
    src/core/alert_system.cpp

    # Shared Feature Dispatch System (CLI + GUI unified command routing)
    # NOTE: shared_feature_dispatch.cpp REMOVED — its 4 extern "C" symbols are
    # superseded by unified_command_dispatch.cpp (superset with AutoRegistrar)
    src/core/feature_registration.cpp
    src/core/feature_handlers.cpp
    src/core/auto_feature_registry.cpp
    src/core/intent_engine.cpp
    src/core/model_training_pipeline.cpp
    src/core/ssot_handlers.cpp
    src/core/ssot_beacon.cpp
    src/core/ssot_handlers_ext_runtime_minimal.cpp
    src/core/ssot_validation.cpp

    # Unified Command Dispatch — Zero-drift SSOT dispatch (replaces manual registration)
    src/core/unified_command_dispatch.cpp

    # Phase 32A: Chain-of-Thought Multi-Model Review Engine
    src/core/chain_of_thought_engine.cpp

    # Phase 32B: Tunable Reasoning Pipeline (P-Settings Orchestrator)
    src/core/reasoning_profile.cpp
    src/core/reasoning_pipeline_orchestrator.cpp
    src/core/reasoning_cot_bridge.cpp

    # Phase 36: Cross-Process MMF State Synchronization (shared with Win32IDE)
    src/core/rawrxd_state_mmf.cpp

    # Phase 46: VulkanKernel — Full GPU Compute Engine (SPIR-V, Flash Attn v2, Fused MLP)
    src/vulkan_compute.cpp
    src/backend/VulkanGemmDispatcher.cpp

    # Phase 46: Live Binary Patcher — Layer 5 Hotpatch (trampolines, code-page swap, RVA reloc)
    src/core/live_binary_patcher.cpp

    # Phase 37: Iterative Partial Inference with Adaptive Tensor Traversal
    src/core/traversal_strategy.cpp
    src/core/layer_contribution_scorer.cpp
    src/core/semantic_delta_tracker.cpp
    src/core/cross_run_tensor_cache.cpp
    src/core/convergence_controller.cpp
    src/core/iterative_tensor_traversal.cpp

    # ═══════════════════════════════════════════════════════════════════
    # Cursor-Parity Gaps — META prerequisites + GAP 1-10
    # ═══════════════════════════════════════════════════════════════════

    # META 1: SQLite RAII Wrapper + Real Amalgamation
    src/core/sqlite_wrapper.cpp
    src/core/sqlite3.c           # Real sqlite3 amalgamation (sqlite.org 3.47.2)

    # META 2: Work-Stealing Thread Pool
    src/core/thread_pool.cpp

    # META 3: Memory Pressure Handler
    src/core/memory_pressure_handler.cpp

    # META 4: JSON-RPC 2.0 Parser
    src/core/jsonrpc_parser.cpp

    # GAP 1: Multi-File Atomic Transactions + WAL Journal
    src/agentic/multi_file_transaction.cpp
    src/core/transaction_journal.cpp

    # GAP 1.5: Agentic Transaction Layer (Cursor-Composer-class orchestration)
    src/agentic/agentic_transaction.cpp

    # P6: Agent Workflow Orchestrator (DAG-based plan→execute→validate→commit)
    src/agentic/agent_workflow_orchestrator.cpp

    # GAP 2: HNSW Vector Index
    src/core/vector_index.cpp

    # GAP 3: Model Cascade Router (Thompson Sampling)
    src/agentic/model_cascade.cpp

    # GAP 4: Ghost-Text Context Assembler
    src/agentic/context_assembler.cpp

    # GAP 5: Chat Message Renderer (Markdown → HTML)
    src/ui/chat_message_renderer.cpp
    src/ui/tool_action_status.cpp
    src/ui/chat_panel.cpp

    # Cathedral Hardening (5.0 Readiness) - REAL IMPLEMENTATIONS ONLY
    src/core/crash_containment.cpp
    src/core/patch_rollback_ledger.cpp
    src/core/quant_hysteresis.cpp
    src/core/perf_telemetry.cpp

    # GAP 6: Git Context Provider (CLI subprocess)
    src/git/git_context.cpp

    # GAP 7: LSP Diagnostic Consumer + Quick-Fix
    src/lsp/diagnostic_consumer.cpp

    # GAP 8: Jinja2-Style Prompt Template Engine
    src/core/prompt_template_engine.cpp

    # GAP 9: Speculative Decoder v2 (Draft-Verify-Accept)
    src/gpu/speculative_decoder_v2.cpp

    # VAL-032: ISA-Optimized Tree Attention Kernels for Speculative Decoding
    src/ai/streaming_inference_engine.cpp
    src/ai/speculative_tree_attention_bridge.cpp
    src/kernels/tree_attention_dispatch.cpp
    src/kernels/tree_attention_scalar.cpp
    src/kernels/tree_attention_avx2.cpp
    src/kernels/tree_attention_avx512_intrinsics.cpp

    # Autonomous inference engine (required by speculative_decoder.cpp for draft token generation)
    src/inference/ultra_fast_inference.cpp

    # GAP 10: Completion Feedback (Online Logistic Regression)
    src/telemetry/completion_feedback.cpp

    # T3-D: Replay + Telemetry Fusion Engine
    src/telemetry/replay_telemetry_fusion.cpp

    # T3-D: Hotpatch Telemetry Safety Layer
    src/telemetry/hotpatch_telemetry_safety.cpp

    # Unified Telemetry Core — single entry point for all telemetry layers
    src/telemetry/UnifiedTelemetryCore.cpp

    # ═══════════════════════════════════════════════════════════════════
    # GPU + Agentic + UI Integration (STL/Win32 native)
    # ═══════════════════════════════════════════════════════════════════

    # GPU Backend — Vulkan/CUDA/CPU fallback chain (fprintf logging)
    src/gpu/gpu_backend.cpp

    # KV-Cache Optimizer — sliding-window eviction, std::chrono timing
    src/gpu/kv_cache_optimizer.cpp

    # Speculative Decoder v1 — draft/verify with std::mt19937 RNG
    src/gpu/speculative_decoder.cpp

    # Agentic Controller — callback-based event loop (no QObject/signals)
    src/agentic_controller.cpp

    # Phase 2 UI Integration — Win32 MessageBox, std::filesystem, std::function callbacks
    src/ui/phase2_integration_example.cpp

    # Multi-Model Benchmark Tool — has its own main(), built as separate target
    # src/tools/multi_model_benchmark.cpp  # EXCLUDED: duplicate main() symbol

    # Agentic Agent Coordinator — multi-agent lifecycle (ctor, dtor, createAgent, synchronize)
    src/agentic_agent_coordinator.cpp

    # Production Config Manager — singleton config loader (instance, loadConfig, value)
    src/production_config_manager.cpp

    # ═══════════════════════════════════════════════════════════════════
    # Core Production Subsystems (Verified Working)
    # ═══════════════════════════════════════════════════════════════════

    # Deterministic Swarm Reproducibility (same input + seed = same result)
    src/core/deterministic_swarm.cpp
    src/core/swarm_scheduler.cpp
    src/core/gguf_swarm_plan_builder.cpp

    # Input Guard with Backend Slicing (OOM protection, chunk routing, budget)
    src/core/input_guard_slicer.cpp

    # ═══════════════════════════════════════════════════════════════════
    # Autonomous-Fix Replay Harness (Deterministic Validation Layer)
    # ═══════════════════════════════════════════════════════════════════
    src/test_harness/replay_fixture.cpp
    src/test_harness/replay_oracle.cpp
    src/test_harness/replay_mock_inference.cpp
    src/test_harness/replay_harness.cpp
    src/test_harness/replay_reporter.cpp

    # Phase 34: Instructions Context Provider — Read tools.instructions.md (All Lines)
    src/core/instructions_provider.cpp

    # Phase 13: Distributed Pipeline Orchestrator — DAG scheduling, work-stealing
    src/core/distributed_pipeline_orchestrator.cpp

    # Phase 14: Hotpatch Control Plane — Lifecycle, transactions, dependency graphs
    src/core/hotpatch_control_plane.cpp

    # Phase 15: Static Analysis Engine — CFG/SSA, dominators, optimizations
    src/core/static_analysis_engine.cpp

    # Top-50: Unified Problems (LSP + SAST + SCA + Secrets)
    src/core/problems_aggregator.cpp

    # Top-50: Secrets scanning (API keys, high-entropy) — Security
    src/security/secrets_scanner.cpp

    # Top-50: In-house SCA (manifest parsing, no external deps)
    src/security/dependency_audit.cpp

    # Top-50: SBOM export (CycloneDX 1.4, in-house)
    src/security/sbom_export.cpp

    # Top-50: In-house SAST (regex/source rules, no external SAST lib)
    src/security/sast_rule_engine.cpp

    # Google Dork Scanner + bug-safe SQLi (maxIterations, replaceSuffix, boolean payloads)
    src/security/RawrXD_GoogleDork_Scanner.cpp

    # Universal Dorker — LDOAGTIAC, XOR obfuscation, hotpatch, result reversal
    src/security/RawrXD_Universal_Dorker.cpp

    # Security hardening (AuditLog, SecurityManager, InputValidator)
    src/security/SecurityHardening.cpp

    # Phase 15b: Code Linter — Real-time syntax/semantic linting (C++, ASM, Python, JS)
    src/core/code_linter.cpp

    # Phase 16: Semantic Code Intelligence — Cross-refs, type inference, indexing
    src/core/semantic_code_intelligence.cpp

    # Phase 17: Enterprise Telemetry & Compliance — OTLP tracing, audit, GDPR
    src/core/enterprise_telemetry_compliance.cpp

    # Core workflow/reasoning engines (required by unified_hotpatch_manager, etc.)
    src/core/autonomous_workflow_engine.cpp
    src/core/workspace_reasoning_profiles.cpp
    src/core/reasoning_schema_versioning.cpp
    src/core/safe_refactor_engine.cpp

    # Unlinked symbols batches (required for production link closure)
    src/core/unlinked_symbols_batch_001.cpp
    # src/core/unlinked_symbols_batch_002.cpp  # excluded: duplicate symbols with RawrXD_Pyre_Compute.asm (asm_pyre_*)
    src/core/unlinked_symbols_batch_003.cpp
    # src/core/unlinked_symbols_batch_004.cpp  # excluded: duplicate symbols with RawrXD_Hotpatch_Kernel.asm and RawrXD_Snapshot.asm
    src/core/unlinked_symbols_batch_005.cpp
    src/core/unlinked_symbols_batch_006.cpp
    src/core/unlinked_symbols_batch_007.cpp
    src/core/unlinked_symbols_batch_008.cpp
    # src/core/unlinked_symbols_batch_009.cpp  # excluded: duplicate symbols with rawrxd_subsystem_api.cpp
    # src/core/unlinked_symbols_batch_010.cpp  # excluded: duplicate symbols with rawrxd_subsystem_api.cpp
    # src/core/unlinked_symbols_batch_011.cpp  # excluded: duplicate symbols with rawrxd_subsystem_api.cpp
    src/core/unlinked_symbols_batch_012.cpp
    # src/core/unlinked_symbols_batch_022.cpp  # moved to WIN32IDE_SOURCES
    # src/core/unlinked_symbols_batch_013.cpp  # excluded: duplicate symbols with RawrXD_AgenticOrchestrator.asm
    
    # Batch 018-020: AgentOllamaClient, Win32IDE methods, Sovereign subsystem, Camellia256, Watchdog, Performance
    # src/core/unlinked_symbols_batch_018.cpp  # excluded: duplicate symbols with unlinked_symbols_batch_001.cpp
    # src/core/unlinked_symbols_batch_019.cpp  # excluded: duplicate symbols with unlinked_symbols_batch_005.cpp and unlinked_symbols_batch_011.cpp
    # src/core/unlinked_symbols_batch_020.cpp  # excluded: duplicate symbols with unlinked_symbols_batch_001.cpp

    # Streaming orchestrator and analyzer distiller removed - symbols provided by unlinked_symbols_batch_*.cpp
    # src/core/streaming_orchestrator.cpp
    # src/core/analyzer_distiller.cpp

    # Link shims for Win32IDE symbols (required by vscode_extension_api when Win32IDE is not built)
    # NOTE: rawrengine_missing_stubs.cpp removed - duplicates symbols from unlinked_symbols_batch_*.cpp
    # src/core/rawrengine_missing_stubs.cpp

    # Phase 50: Enterprise Hardening — Auto-Update, Swarm Reconciliation, Plugin Sandbox/Signature, Stress Tests
    src/core/update_signature.cpp
    src/core/cursor_github_parity_bridge.cpp
    src/core/local_parity_bridge.cpp
    src/core/swarm_reconciliation.cpp
    src/core/quickjs_sandbox.cpp
    src/core/plugin_signature.cpp
    src/core/enterprise_stress_tests.cpp
    src/core/auto_update_system.cpp

    # ═══════════════════════════════════════════════════════════════════
    # Autonomy Gap Closure — Phase A-D + Tier 1.4 + Tier 2.3
    # ═══════════════════════════════════════════════════════════════════

    # Phase A: Closed-Loop Verification (Write → Compile → Test → Fix)
    src/agentic/autonomous_verification_loop.cpp

    # Phase B: Continuous Background Daemon (proactive refactor, doc-sync, security)
    src/agentic/autonomous_background_daemon.cpp

    # Phase C: Long-Term Knowledge Graph (cross-session learning, decision memory)
    src/core/knowledge_graph_core.cpp

    # Phase D: Multi-Agent Conflict Resolution (merge negotiation, consensus)
    src/core/swarm_conflict_resolver.cpp

    # Tier 1.4: Autonomous Debugging & Root Cause Analysis (agent-driven breakpoints)
    src/core/autonomous_debugger.cpp

    # Tier 2.3: Autonomous Communication (standup reports, approval requests)
    src/agentic/autonomous_communicator.cpp

    # ═══════════════════════════════════════════════════════════════════
    # Cursor/JB-Parity Feature Modules (telemetry, composer, @mention,
    # vision, refactoring, language, semantic index, resource gen)
    # ═══════════════════════════════════════════════════════════════════
    src/telemetry/telemetry_export.cpp
    src/agentic/agentic_composer_ux.cpp
    src/context/context_mention_parser.cpp
    src/multimodal_engine/vision_encoder.cpp
    src/ide/refactoring_plugin.cpp
    src/ide/language_plugin.cpp
    src/context/semantic_index.cpp
    src/ide/resource_generator.cpp

    # P0: Codebase RAG (in-house TF-IDF)
    src/ai/codebase_rag.cpp

    # P1: Symbol Graph Indexer (cross-file navigation, Copilot/Cursor parity)
    src/ai/symbol_graph_indexer.cpp

    # P2: Semantic Code Search (TF-IDF + fuzzy ranking)
    src/ai/semantic_code_search.cpp

    # P3: Multi-File Reasoning (cross-file context & dependency analysis)
    src/ai/multi_file_reasoning.cpp

    # P4: Workspace Embeddings (LSH vector index, incremental)
    src/ai/workspace_embeddings.cpp

    # P5: Repo-Wide Refactor Engine (rename, extract, move, change-sig)
    src/ai/repo_refactor_engine.cpp

    # VAL-032: ISA-Optimized Tree Attention Kernels for Speculative Decoding
    src/ai/streaming_inference_engine.cpp
    src/ai/speculative_tree_attention_bridge.cpp
    src/kernels/tree_attention_dispatch.cpp
    src/kernels/tree_attention_scalar.cpp
    src/kernels/tree_attention_avx2.cpp
    src/kernels/tree_attention_avx512_intrinsics.cpp

    # NOTE: Removed Transcendence fiction (self_host_engine, hardware_synthesizer,
    # mesh_brain, speciator_engine, neural_bridge, omega_orchestrator, transcendence_coordinator)
    # These were valuation theater - not implementable.
    # 
    # NOTE: Removed Quantum Production Orchestrator files - marketing fiction without
    # coherent implementation. Real scheduler is swarm_scheduler.cpp above.

    # NOTE: Removed Transcendence IDE Panel - part of valuation theater fiction

    # ═══════════════════════════════════════════════════════════════════
    # Cognitive Architecture — Autonomous Reverse Engineering OS
    # ═══════════════════════════════════════════════════════════════════
    src/agentic/cognitive/cognitive_types.cpp
    src/agentic/cognitive/enhanced_blackboard.cpp
    src/agentic/cognitive/mission_director.cpp
    src/agentic/cognitive/reflection_agent.cpp
    src/agentic/cognitive/dynamic_planner.cpp
    src/agentic/cognitive/autonomous_orchestrator.cpp

    # ═══════════════════════════════════════════════════════════════════
    # TrailBrake / HotPatcher / AntiPatcher Safety System
    # ═══════════════════════════════════════════════════════════════════
    src/deep2/TrailBrake.cpp
    src/deep2/HotPatcher.cpp
    src/deep2/HotPatcherSafety.cpp
    src/deep2/AntiPatcher.cpp
    src/deep2/GoalSystem.cpp
    src/deep2/PatchCache.cpp
    src/deep2/BottleTTL.cpp
    src/deep2/CPUFrequency.cpp
    # Dual GPU / Reverse Recovery / Streaming
    src/deep2/DualGPUHook.cpp
    src/deep2/ReverseTensorRecovery.cpp
    src/deep2/StreamEngine.cpp
    src/deep2/ShardRouterIntegration.cpp
    src/deep2/Deep2IDEIntegration.cpp
    src/deep2/FabricTensorTable.cpp
    src/deep2/Deep2Engine.cpp
    src/deep2/KimiK2Config.cpp
    src/deep2/K2MLAWeights.cpp
    src/deep2/K2MoEWeights.cpp
    src/deep2/K2GlobalTensorIndex.cpp
    src/deep2/Deep2Bridge.cpp
    src/deep2/InferenceSession.cpp
    src/deep2/QuantKernelRegistry.cpp
    src/deep2/ThreadPool.cpp
    src/deep2/KVCache.cpp
    src/deep2/GGUFLoader.cpp
    src/deep2/ReverseIntegration.cpp
    src/deep2/ReverseHotpatchEngine.cpp
    src/deep2/MoERouter.cpp
    src/deep2/MoEWeightProxy.cpp
    src/deep2/MoEWeightsLoader.cpp
    src/deep2/MoEArchitectureParser.cpp
    src/deep2/TensorHop.cpp
    src/deep2/DeepSeekMoELoader.cpp
    src/deep2/Deep2ExecutionGraph.cpp
    src/deep2/StreamRouter.cpp
    src/deep2/FusedInferenceKernel.cpp
    src/deep2/UniversalModelLoader.cpp
    src/deep2/ModelLoader.cpp
    src/deep2/ModelRegistry.cpp
    src/deep2/RuntimePlanner.cpp
    src/deep2/WarmupScheduler.cpp
    src/deep2/NUFusedPacker.cpp
    src/deep2/CompressedKVCache.cpp
    src/deep2/NVMeStream.cpp
    src/deep2/SlidingWindowEngine.cpp
    src/deep2/MedusaDecoder.cpp
    src/deep2/RawrXDInferenceAdapter.cpp
    src/inference/RawrXD_LlamaNative.cpp
    src/deep2/mars/VRAMManager.cpp
    src/deep2/mars/TensorHotpatch.cpp
    src/deep2/mars/DualGPUBackend.cpp
    src/deep2/mars/MARSController.cpp
    src/sampling/advanced_sampler.cpp
    src/deep2/deep2_link_stubs.cpp

    # Sovereign ABI runtime — stable C++/MASM execution plane seam
    src/runtime/Sovereign_RingTelemetry.cpp

# NOTE: Do not compile unresolved_stubs_all.cpp into production targets.
# It provides fallback symbol placeholders and can mask real feature gaps.
# Keep real subsystem implementations as the single source of truth.
)

# =============================================================================
# Controlled Stub Replacement Pass (subsystem-scoped)
# =============================================================================
# Keep default behavior conservative; enable per subsystem during targeted builds.
option(RAWR_REPLACE_SSOT_HANDLERS "Replace SSOT minimal/isolated lane with full ssot_handlers_ext.cpp" OFF)
option(RAWR_REPLACE_MASM_FALLBACKS "Remove MASM-related C++ fallback stubs (and optionally re-enable blocked ASM units)" OFF)
set(RAWR_SSOT_PROVIDER "AUTO" CACHE STRING "SSOT owner for RawrEngine: CORE, EXT, AUTO, STUBS, FEATURES")
set_property(CACHE RAWR_SSOT_PROVIDER PROPERTY STRINGS CORE EXT AUTO STUBS FEATURES)

if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES AND RAWRXD_INCLUDE_STRESS_AND_REPLAY_SOURCES)
    message(FATAL_ERROR "[ProductionLane] RAWRXD_INCLUDE_STRESS_AND_REPLAY_SOURCES=ON is incompatible with RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON.")
endif()
if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES AND RAWR_REPLACE_SSOT_HANDLERS)
    message(FATAL_ERROR "[ProductionLane] RAWR_REPLACE_SSOT_HANDLERS=ON is incompatible with RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON.")
endif()
if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES AND RAWR_REPLACE_MASM_FALLBACKS)
    message(FATAL_ERROR "[ProductionLane] RAWR_REPLACE_MASM_FALLBACKS=ON is incompatible with RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON.")
endif()

if(NOT RAWRXD_INCLUDE_STRESS_AND_REPLAY_SOURCES)
    list(REMOVE_ITEM SOURCES
        src/core/masm_stress_harness.cpp
        src/core/convergence_stress_harness.cpp
        src/core/enterprise_stress_tests.cpp
        src/test_harness/replay_fixture.cpp
        src/test_harness/replay_oracle.cpp
        src/test_harness/replay_mock_inference.cpp
        src/test_harness/replay_harness.cpp
        src/test_harness/replay_reporter.cpp
    )
endif()
if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
    if(DEFINED ENV{RAWRXD_EMERGENCY_STUB_LINK})
        message(FATAL_ERROR "[ProductionLane] RAWRXD_EMERGENCY_STUB_LINK must be unset when RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON.")
    endif()
    list(REMOVE_ITEM SOURCES
        src/core/cot_fallback_system.cpp
        src/win32app/Win32IDE_logMessage.cpp
    )
    # NOTE: agentic_bridge_headless.cpp is NOT a stub - it provides headless AgenticBridge implementation
    # Required by RawrXD_AgentLoop.cpp when Win32IDE is not built
    # NOTE: dml_asm_fallback.cpp is NOT a stub - it contains real DML ASM function implementations
    # The name matches the pattern but it's actually the implementation, not a fallback
    list(FILTER SOURCES EXCLUDE REGEX ".*/.*_fallback[^/]*\\.cpp$")
    list(FILTER SOURCES EXCLUDE REGEX ".*/.*_link_shims?\\.cpp$")
    list(FILTER SOURCES EXCLUDE REGEX ".*/.*_shims?\\.cpp$")
    list(FILTER SOURCES EXCLUDE REGEX ".*/stubs\\.cpp$")
    list(FILTER SOURCES EXCLUDE REGEX ".*/.*_stub\\.cpp$")
    list(FILTER SOURCES EXCLUDE REGEX ".*/stub_.*\\.cpp$")
    list(FILTER SOURCES EXCLUDE REGEX ".*/shim_.*\\.cpp$")
    list(FILTER SOURCES EXCLUDE REGEX ".*/.*_(mock|fake)\\.cpp$")
    list(FILTER SOURCES EXCLUDE REGEX ".*/.*(mock_|fake_).*\\.cpp$")
    list(FILTER SOURCES EXCLUDE REGEX ".*/.*_stubs\\.asm$")
    list(FILTER SOURCES EXCLUDE REGEX ".*/stubs\\.asm$")
    list(FILTER SOURCES EXCLUDE REGEX ".*/.*_stub\\.asm$")
    list(FILTER SOURCES EXCLUDE REGEX ".*/stub_.*\\.asm$")
    list(FILTER SOURCES EXCLUDE REGEX ".*/.*_shims?\\.asm$")
    list(FILTER SOURCES EXCLUDE REGEX ".*/shim_.*\\.asm$")
    list(FILTER SOURCES EXCLUDE REGEX ".*/.*_link_shims?\\.asm$")
    list(FILTER SOURCES EXCLUDE REGEX ".*/.*_fallback[^/]*\\.asm$")
    list(FILTER SOURCES EXCLUDE REGEX ".*/.*_(mock|fake)\\.asm$")
    list(FILTER SOURCES EXCLUDE REGEX ".*/.*(mock_|fake_).*\\.asm$")
    list(FILTER SOURCES EXCLUDE REGEX ".*/link_stubs_.*\\.cpp$")
    list(FILTER SOURCES EXCLUDE REGEX ".*/link_stubs_.*\\.asm$")
    # Re-add files that were incorrectly filtered - they contain real implementations, not stubs
    list(APPEND SOURCES
        src/core/cot_fallback_system.cpp
        src/core/dml_asm_fallback.cpp
    )
    if(RAWRXD_ENABLE_MISSING_HANDLER_STUBS)
        message(FATAL_ERROR "[ProductionLane] RAWRXD_ENABLE_MISSING_HANDLER_STUBS=ON is incompatible with RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON.")
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/core/missing_handler_stubs.cpp")
        file(READ "${CMAKE_SOURCE_DIR}/src/core/missing_handler_stubs.cpp" _RAWR_MISSING_HANDLER_STUBS_SRC)
        if(_RAWR_MISSING_HANDLER_STUBS_SRC MATCHES "history/reconstructed")
            message(FATAL_ERROR "[ProductionLane] src/core/missing_handler_stubs.cpp is history-indirected. Promote canonical ownership under src/core before strict production strip builds.")
        endif()
    endif()
endif()

if(RAWR_REPLACE_SSOT_HANDLERS)
    list(REMOVE_ITEM SOURCES
        src/core/ssot_handlers_ext_runtime_minimal.cpp
        src/core/ssot_handlers_ext_isolated.cpp
    )
    list(APPEND SOURCES
        src/core/ssot_handlers_ext.cpp
    )
    message(STATUS "[ReplacementPass] SSOT: using full src/core/ssot_handlers_ext.cpp")
endif()

if(RAWR_REPLACE_MASM_FALLBACKS)
    if(RAWR_HAS_MASM)
        list(APPEND ASM_KERNEL_SOURCES
            src/asm/memory_patch.asm
            src/asm/byte_search.asm
        )
        set_source_files_properties(
            src/asm/memory_patch.asm
            src/asm/byte_search.asm
            PROPERTIES LANGUAGE ASM_MASM
        )
        message(STATUS "[ReplacementPass] MASM: promoted memory_patch/byte_search ASM; analyzer/streaming remain on C++ fallbacks pending asm include fixes")
    else()
        message(WARNING "[ReplacementPass] MASM replacement requested but RAWR_HAS_MASM=FALSE; fallback stubs removed without ASM replacements")
    endif()
endif()

# Build target-scoped SSOT ownership partition for RawrEngine only.
set(RAWR_ENGINE_SOURCES ${SOURCES})
list(REMOVE_ITEM RAWR_ENGINE_SOURCES
    src/core/ssot_validation.cpp
    src/core/rawrxd_subsystem_api.cpp
    src/core/auto_feature_registry.cpp
    src/lsp/RawrXD_LSPServer.cpp
    src/lsp/lsp_hotpatch_bridge.cpp
    # Swarm files that reference symbols not available in RawrEngine target
    src/swarm/InfinitePerfectionTelemetry.cpp
    src/swarm/InfinitePerfectionTelemetrySQLite.cpp
    src/swarm/TelemetryDashboardServer.cpp
    src/swarm/IDEIntegration.cpp
    src/swarm/LearningSimulator.cpp
    src/agentic_agent_coordinator.cpp
    # Remove streaming_orchestrator.cpp and analyzer_distiller.cpp - symbols provided by unlinked_symbols_batch_*.cpp
    src/core/streaming_orchestrator.cpp
    src/core/analyzer_distiller.cpp
    # Remove streaming_inference_engine.cpp - references KernelArbiter and Vulkan APIs not available in RawrEngine
    src/ai/streaming_inference_engine.cpp
    src/ai/speculative_tree_attention_bridge.cpp
    # Remove unified_command_dispatch.cpp and agentic_transaction.cpp - reference voice command handlers not in RawrEngine
    src/core/unified_command_dispatch.cpp
    src/agentic/agentic_transaction.cpp
    # Remove files referencing SubsystemRegistry (not available in RawrEngine)
    src/agentic/ToolRegistry.cpp
    src/core/feature_handlers.cpp
    src/agentic/AgentOrchestrator.cpp
    # Remove model_config.cpp - references OllamaClient which is not linked in RawrEngine
    src/model_config.cpp
    # Remove LSP providers referencing direct_read (byte_level_hotpatcher.cpp excluded)
    src/lsp/hotpatch_symbol_provider.cpp
    src/lsp/gguf_diagnostic_provider.cpp
    # Remove quantum_agent_orchestrator.cpp - references GPUDispatchGate which has no implementation
    src/agent/quantum_agent_orchestrator.cpp
    # Remove unified_hotpatch_manager.cpp - references patch_bytes/search_and_patch_bytes from excluded byte_level_hotpatcher.cpp
    src/core/unified_hotpatch_manager.cpp
    # Remove RawrXD_AgentLoop.cpp and DiskRecoveryToolHandler.cpp - reference AgentToolRegistry from excluded ToolRegistry.cpp
    src/agentic/RawrXD_AgentLoop.cpp
    src/agentic/DiskRecoveryToolHandler.cpp
    # Remove files referencing UnifiedHotpatchManager (unified_hotpatch_manager.cpp excluded above)
    src/core/iterative_tensor_traversal.cpp
    src/core/execution_scheduler.cpp
    src/cli/agentic_decision_tree.cpp
    src/core/agentic_task_graph.cpp
    src/core/auto_repair_orchestrator.cpp
    # Remove cli_autonomy_loop.cpp - references AgenticDecisionTree from excluded agentic_decision_tree.cpp
    src/cli/cli_autonomy_loop.cpp
)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/agent/quantum_missing_impl.cpp")
    # RawrEngine currently relies on quantum_missing_impl.cpp for additional
    # QuantumAutonomousTodoSystem/QuantumMultiModelAgentCycling definitions.
    # Exclude the duplicate time-manager impl translation unit in this lane.
    list(REMOVE_ITEM RAWR_ENGINE_SOURCES src/agent/quantum_dynamic_time_manager_impl.cpp)
endif()
string(TOUPPER "${RAWR_SSOT_PROVIDER}" RAWR_SSOT_PROVIDER_UPPER)
set(_RAWR_VALID_SSOT_PROVIDERS CORE EXT AUTO STUBS FEATURES)
list(FIND _RAWR_VALID_SSOT_PROVIDERS "${RAWR_SSOT_PROVIDER_UPPER}" _rawwr_ssot_provider_idx)
if(_rawwr_ssot_provider_idx EQUAL -1)
    message(FATAL_ERROR "Invalid RAWR_SSOT_PROVIDER='${RAWR_SSOT_PROVIDER}'. Valid values: CORE, EXT, AUTO, STUBS, FEATURES")
endif()
if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES AND NOT RAWR_SSOT_PROVIDER_UPPER STREQUAL "AUTO")
    message(FATAL_ERROR "[ProductionLane] RAWR_SSOT_PROVIDER=${RAWR_SSOT_PROVIDER_UPPER} is incompatible with RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON. Use RAWR_SSOT_PROVIDER=AUTO.")
endif()

set(_RAWR_SSOT_CORE 0)
set(_RAWR_SSOT_EXT 0)
set(_RAWR_SSOT_AUTO 0)
set(_RAWR_SSOT_STUBS 0)
set(_RAWR_SSOT_FEATURES 0)
set(_RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS 0)

if(RAWR_SSOT_PROVIDER_UPPER STREQUAL "CORE")
    set(_RAWR_SSOT_CORE 1)
    list(REMOVE_ITEM RAWR_ENGINE_SOURCES
    src/core/ssot_handlers_ext_runtime_minimal.cpp
    src/core/ssot_handlers_ext_isolated.cpp
    src/core/ssot_handlers_ext.cpp
        src/core/auto_feature_registry.cpp
        src/core/feature_handlers.cpp
    )
elseif(RAWR_SSOT_PROVIDER_UPPER STREQUAL "EXT")
    set(_RAWR_SSOT_EXT 1)
    list(REMOVE_ITEM RAWR_ENGINE_SOURCES
        src/core/ssot_handlers.cpp
        src/core/auto_feature_registry.cpp
        src/core/ssot_handlers_ext_runtime_minimal.cpp
        src/core/ssot_handlers_ext_isolated.cpp
    )
elseif(RAWR_SSOT_PROVIDER_UPPER STREQUAL "AUTO")
    set(_RAWR_SSOT_AUTO 1)
    set(_RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS 1)
    list(REMOVE_ITEM RAWR_ENGINE_SOURCES
        src/core/ssot_handlers_ext_runtime_minimal.cpp
        src/core/ssot_handlers_ext_isolated.cpp
        src/core/ssot_handlers.cpp
    )
    # AUTO lane uses `auto_feature_registry.cpp` + unified dispatch; do not also link EXT handlers or ssot_handlers.cpp.
    list(REMOVE_ITEM RAWR_ENGINE_SOURCES
        src/core/ssot_handlers_ext.cpp
        src/core/ssot_handlers_ext_dedicated.cpp
    )
elseif(RAWR_SSOT_PROVIDER_UPPER STREQUAL "STUBS")
    set(_RAWR_SSOT_STUBS 1)
    list(REMOVE_ITEM RAWR_ENGINE_SOURCES
        src/core/ssot_handlers.cpp
        src/core/auto_feature_registry.cpp
        src/core/feature_handlers.cpp
        src/core/ssot_handlers_ext_runtime_minimal.cpp
        src/core/ssot_handlers_ext_isolated.cpp
        src/core/ssot_handlers_ext.cpp
    )
elseif(RAWR_SSOT_PROVIDER_UPPER STREQUAL "FEATURES")
    set(_RAWR_SSOT_FEATURES 1)
    list(REMOVE_ITEM RAWR_ENGINE_SOURCES
        src/core/ssot_handlers.cpp
        src/core/auto_feature_registry.cpp
        src/core/ssot_handlers_ext_runtime_minimal.cpp
        src/core/ssot_handlers_ext_isolated.cpp
        src/core/ssot_handlers_ext.cpp
    )
endif()
if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
    list(FILTER RAWR_ENGINE_SOURCES EXCLUDE REGEX ".*/.*_link_shims?\\.cpp$")
    list(FILTER RAWR_ENGINE_SOURCES EXCLUDE REGEX ".*/.*_shims?\\.cpp$")
    list(FILTER RAWR_ENGINE_SOURCES EXCLUDE REGEX ".*/.*_fallback[^/]*\\.cpp$")
    list(FILTER RAWR_ENGINE_SOURCES EXCLUDE REGEX ".*/stubs\\.cpp$")
    list(FILTER RAWR_ENGINE_SOURCES EXCLUDE REGEX ".*/.*_stub\\.cpp$")
    list(FILTER RAWR_ENGINE_SOURCES EXCLUDE REGEX ".*/stub_.*\\.cpp$")
    list(FILTER RAWR_ENGINE_SOURCES EXCLUDE REGEX ".*/shim_.*\\.cpp$")
    list(FILTER RAWR_ENGINE_SOURCES EXCLUDE REGEX ".*/.*_(mock|fake)\\.cpp$")
    list(FILTER RAWR_ENGINE_SOURCES EXCLUDE REGEX ".*/.*(mock_|fake_).*\\.cpp$")
    list(FILTER RAWR_ENGINE_SOURCES EXCLUDE REGEX ".*/.*_stubs\\.asm$")
    list(FILTER RAWR_ENGINE_SOURCES EXCLUDE REGEX ".*/stubs\\.asm$")
    list(FILTER RAWR_ENGINE_SOURCES EXCLUDE REGEX ".*/.*_stub\\.asm$")
    list(FILTER RAWR_ENGINE_SOURCES EXCLUDE REGEX ".*/stub_.*\\.asm$")
    list(FILTER RAWR_ENGINE_SOURCES EXCLUDE REGEX ".*/.*_shims?\\.asm$")
    list(FILTER RAWR_ENGINE_SOURCES EXCLUDE REGEX ".*/shim_.*\\.asm$")
    list(FILTER RAWR_ENGINE_SOURCES EXCLUDE REGEX ".*/.*_link_shims?\\.asm$")
    list(FILTER RAWR_ENGINE_SOURCES EXCLUDE REGEX ".*/.*_fallback[^/]*\\.asm$")
    list(FILTER RAWR_ENGINE_SOURCES EXCLUDE REGEX ".*/.*_(mock|fake)\\.asm$")
    list(FILTER RAWR_ENGINE_SOURCES EXCLUDE REGEX ".*/.*(mock_|fake_).*\\.asm$")
    list(FILTER RAWR_ENGINE_SOURCES EXCLUDE REGEX ".*/link_stubs_.*\\.cpp$")
    list(FILTER RAWR_ENGINE_SOURCES EXCLUDE REGEX ".*/link_stubs_.*\\.asm$")
endif()

# RawrEngine requires the core observability/safety singletons and the shared
# feature handlers used by SSOT/unified dispatch. Ensure they are linked even
# in headless mode.
list(APPEND RAWR_ENGINE_SOURCES
    src/logging/Logger.cpp
    src/security/InputSanitizer.cpp
    src/security/SecurityHardening.cpp
    src/core/ConfigurationValidator.cpp
    src/inference/PerformanceMonitor.cpp
    src/agentic/ErrorRecoveryManager.cpp
    # src/core/rawrengine_missing_stubs.cpp  # Removed - duplicates symbols from unlinked_symbols_batch_*.cpp
    # REMOVED: src/link_stubs/rawrengine_link_closure.cpp - stub file deleted

    # NOTE: rawrengine_command_handlers.cpp removed - depends on SelfHostEngine,
    # SpeciatorEngine, TranscendenceCoordinator which are excluded from RawrEngine
    # src/core/rawrengine_command_handlers.cpp
)

list(REMOVE_DUPLICATES RAWR_ENGINE_SOURCES)
message(STATUS "[SSOT Ownership] RawrEngine provider=${RAWR_SSOT_PROVIDER_UPPER} (CORE=${_RAWR_SSOT_CORE} EXT=${_RAWR_SSOT_EXT} AUTO=${_RAWR_SSOT_AUTO} STUBS=${_RAWR_SSOT_STUBS} FEATURES=${_RAWR_SSOT_FEATURES})")

# RawrEngine is headless; don't hard-depend on the Win32IDE VSCode API bridge.
list(REMOVE_ITEM RAWR_ENGINE_SOURCES
    src/modules/vscode_extension_api.cpp
    src/win32app/Win32IDE_VSCodeExtAPI.cpp
    src/core/js_extension_host.cpp
    src/core/extension_polyfill_engine.cpp
    src/modules/quickjs_extension_host.cpp
    src/modules/quickjs_vscode_bindings.cpp
    src/engine/pyre_compute.cpp
)

# RawrEngine is headless; exclude Win32IDE GUI translation units.
list(FILTER RAWR_ENGINE_SOURCES EXCLUDE REGEX "^src/win32app/Win32IDE_.*\\.cpp$")
list(REMOVE_ITEM RAWR_ENGINE_SOURCES
    src/win32app/Win32IDE.cpp
    src/win32app/main_win32.cpp
)

list(APPEND RAWR_ENGINE_SOURCES
    src/BackendOrchestrator.cpp
    src/agentic/AdvancedAgentCoordinator.cpp
    src/agentic/hotpatch/Engine.cpp
    src/InferenceProfiler.cpp
    src/core/js_extension_host_headless_impl.cpp
    src/core/shadow_page_detour.cpp
    src/core/camellia256_bridge.cpp
    src/core/cli_autonomy_loop_stub.cpp
    # src/core/rawrxd_native_log_impl.cpp  # Duplicate - RawrXD_Native_Log defined in unlinked_symbols_batch_012.cpp

    # ═══════════════════════════════════════════════════════════════════
    # Kimi K2.6 300-Agent Swarm Architecture (subset — excluded files
    # reference symbols not available in RawrEngine target)
    # ═══════════════════════════════════════════════════════════════════
    src/swarm/SwarmOrchestrator.cpp
    src/swarm/KimiSwarmOrchestrator.cpp
    src/swarm/ArchitectAgent.cpp
    src/swarm/FrontendSquad.cpp
    src/swarm/BackendCore.cpp
    src/swarm/QAHive.cpp
    src/swarm/ReviewerAgents.cpp
    src/swarm/CinematicVibeEngine.cpp
    src/swarm/DeepContextManager.cpp
    src/swarm/OpenClawBridge.cpp
    src/swarm/LegacyRefactorModule.cpp
    # src/swarm/IDEIntegration.cpp           # excluded: references ReviewerAgents methods not in RawrEngine
    # src/swarm/InfinitePerfectionTelemetry.cpp    # excluded: references InfinitePerfectionEngine not in RawrEngine
    # src/swarm/InfinitePerfectionTelemetrySQLite.cpp # excluded: same
    # src/swarm/TelemetryDashboardServer.cpp       # excluded: same
    # src/swarm/LearningSimulator.cpp              # excluded: references Sovereign::SelfModelRegistry not in RawrEngine
)

# =============================================================================
# Command Table Audit — Build-time coverage validation targets
# =============================================================================
include(cmake/AutoFeatureRegistry.cmake)

if(WIN32)
    add_custom_target(regen-ssot-beacons
        COMMAND powershell -ExecutionPolicy Bypass -File ${CMAKE_SOURCE_DIR}/tools/sync_ssot_beacon_symbols.ps1 -RepoRoot ${CMAKE_SOURCE_DIR}
        COMMAND powershell -ExecutionPolicy Bypass -File ${CMAKE_SOURCE_DIR}/tools/generate_ssot_beacon_table.ps1 -RepoRoot ${CMAKE_SOURCE_DIR}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Sync + regenerate SSOT beacon ownership artifacts"
        VERBATIM
    )

    add_custom_target(verify-ssot-ext-ownership
        COMMAND powershell -ExecutionPolicy Bypass -File ${CMAKE_SOURCE_DIR}/tools/verify_ssot_ext_ownership.ps1 -RepoRoot ${CMAKE_SOURCE_DIR}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Verify SSOT EXT ownership artifacts are consistent"
        VERBATIM
    )
endif()

# =============================================================================
# OMEGA-1 Self-Mutating Engine + PowerShell Bridge
# IAT Slots 64-75: Self-mutation, reflective execution, PowerShell integration
# =============================================================================
set(OMEGA1_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/omega1_modules")

set(OMEGA1_SOURCES
    ${OMEGA1_DIR}/OmegaPowerShellBridge.cpp
)

set(OMEGA1_HEADERS
    ${OMEGA1_DIR}/OmegaPowerShellBridge.h
)

# OMEGA-1 Engine as static library
add_library(Omega1Engine STATIC ${OMEGA1_SOURCES} ${OMEGA1_HEADERS})

target_include_directories(Omega1Engine PUBLIC
    ${OMEGA1_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(Omega1Engine PRIVATE
    OMEGA1_BUILDING=1
    RAWRXD_OMEGA1_ENABLED=1
)

set_property(TARGET Omega1Engine PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# =============================================================================
# RawrXD Compression Library (Zero-Dependency Runtime ZLIB Loader)
# Replaces ZLIB for checkpoint compression - no build dependencies
# =============================================================================
set(RAWRXD_COMPRESSION_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/compression")

# Runtime ZLIB loader sources (zero build dependencies)
set(RAWRXD_COMPRESSION_SOURCES
    ${RAWRXD_COMPRESSION_DIR}/zlib_runtime_loader.cpp
)

set(RAWRXD_COMPRESSION_HEADERS
    ${RAWRXD_COMPRESSION_DIR}/zlib_runtime_loader.hpp
)

# Create compression library
add_library(RawrXD_Compression STATIC ${RAWRXD_COMPRESSION_SOURCES})
target_include_directories(RawrXD_Compression PUBLIC ${RAWRXD_COMPRESSION_DIR})

# Link Windows libraries only (no ZLIB dependency)
target_link_libraries(RawrXD_Compression PUBLIC
    kernel32
    user32
)

# Also create the MASM compression if available
set(RAWRXD_COMPRESSION_ASM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/asm")
if(EXISTS ${RAWRXD_COMPRESSION_ASM_DIR}/RawrXD_Compression.asm)
    # Build MASM compression as additional option
    set(RAWRXD_COMPRESSION_ASM ${RAWRXD_COMPRESSION_ASM_DIR}/RawrXD_Compression.asm)
    set(RAWRXD_COMPRESSION_OBJ ${CMAKE_BINARY_DIR}/RawrXD_Compression_MASM.obj)
    
    add_custom_command(
        OUTPUT ${RAWRXD_COMPRESSION_OBJ}
        COMMAND ${CMAKE_ASM_MASM_COMPILER} /c /W3 /nologo /Fo${RAWRXD_COMPRESSION_OBJ} ${RAWRXD_COMPRESSION_ASM}
        DEPENDS ${RAWRXD_COMPRESSION_ASM}
        COMMENT "Assembling RawrXD_Compression.asm (MASM)"
        VERBATIM
    )
    
    add_custom_target(RawrXD_Compression_MASM DEPENDS ${RAWRXD_COMPRESSION_OBJ})
    add_dependencies(RawrXD_Compression RawrXD_Compression_MASM)
    
    target_compile_definitions(RawrXD_Compression PRIVATE HAS_MASM_COMPRESSION=1)
endif()

message(STATUS "✓ RawrXD_Compression configured (runtime ZLIB loader, zero build deps)")

# =============================================================================
# Reverse Engineering Suite (sub-module)
# =============================================================================
add_subdirectory(src/reverse_engineering)

# =============================================================================
# CEO Agent Module — Autonomous Software Engineering Layer
# =============================================================================
add_subdirectory(src/ceo)

# =============================================================================
# Repository Intelligence Module — Deep Code Understanding
# =============================================================================
add_subdirectory(src/repository)
add_subdirectory(src/generation)

# RawrEngine needs specific ASM kernels for Deep2 and Sovereign Q4K
set(RAWR_ENGINE_ASM_SOURCES
    src/deep2/sovereign_deep2_kernels.asm
    src/deep2/sovereign_q4k_gemv.asm
)
set_source_files_properties(${RAWR_ENGINE_ASM_SOURCES} PROPERTIES LANGUAGE ASM_MASM)

add_executable(RawrEngine ${RAWR_ENGINE_SOURCES} ${RAWR_ENGINE_ASM_SOURCES})

if(WIN32)
    add_dependencies(RawrEngine regen-ssot-beacons verify-ssot-ext-ownership)
endif()

# ═════════════════════════════════════════════════════════════════════════════
# OMEGA-1 Post-Build Manifest Generation
# ═════════════════════════════════════════════════════════════════════════════
if(WIN32)
    add_custom_command(TARGET RawrEngine POST_BUILD
        COMMAND powershell -ExecutionPolicy Bypass -File ${OMEGA1_DIR}/Generate-Manifest.ps1
                -BinaryPath $<TARGET_FILE:RawrEngine>
                -OutputPath ${CMAKE_BINARY_DIR}/omega1_manifest.json
        COMMENT "Generating OMEGA-1 manifest for RawrEngine"
        VERBATIM
    )
endif()

# Lane B: curated sources already include required stubs.

# Link RE library and OMEGA-1 Engine into RawrEngine
target_link_libraries(RawrEngine PRIVATE RawrXD-RE-Library Omega1Engine)

target_include_directories(RawrEngine PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/security
    ${CMAKE_CURRENT_SOURCE_DIR}/src/server
    ${CMAKE_CURRENT_SOURCE_DIR}/src/agent
    ${CMAKE_CURRENT_SOURCE_DIR}/src/agentic
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cli
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/git
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lsp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gpu
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tools
    ${CMAKE_CURRENT_SOURCE_DIR}/src/telemetry
    ${CMAKE_CURRENT_SOURCE_DIR}/src/reverse_engineering
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sovereign
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sovereign/puppeteer
    ${CMAKE_CURRENT_SOURCE_DIR}/src/intent
    ${CMAKE_CURRENT_SOURCE_DIR}/src/repo
    ${CMAKE_CURRENT_SOURCE_DIR}/src/build
    ${CMAKE_CURRENT_SOURCE_DIR}/src/diagnostics
    ${CMAKE_CURRENT_SOURCE_DIR}/src/refactoring
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ide
    ${CMAKE_CURRENT_SOURCE_DIR}/src/validation
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guardrails
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hotpatch
    ${CMAKE_CURRENT_SOURCE_DIR}/src/kernel
    ${CMAKE_CURRENT_SOURCE_DIR}/src/swarm
    ${CMAKE_CURRENT_SOURCE_DIR}/src/omega1_modules
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/gpu
)

target_link_libraries(RawrEngine PRIVATE Threads::Threads)

# Avoid duplicate register_sovereign_engines (real impl in sovereign_engines.cpp)
target_compile_definitions(RawrEngine PRIVATE RAWR_HAS_SOVEREIGN_ENGINES=1)
target_compile_definitions(RawrEngine PRIVATE
    RAWRXD_STRICT_PRODUCTION_PROFILE=$<IF:$<BOOL:${RAWRXD_PRODUCTION_STRIP_STUB_SOURCES}>,1,0>
    RAWRXD_HEADLESS_ONLY=1
    RAWR_SSOT_CORE=${_RAWR_SSOT_CORE}
    RAWR_SSOT_EXT=${_RAWR_SSOT_EXT}
    RAWR_SSOT_AUTO=${_RAWR_SSOT_AUTO}
    RAWR_SSOT_STUBS=${_RAWR_SSOT_STUBS}
    RAWR_SSOT_FEATURES=${_RAWR_SSOT_FEATURES}
    RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=$<IF:$<BOOL:${RAWRXD_PRODUCTION_STRIP_STUB_SOURCES}>,1,0>
    # Enable the console entry point in src/main.cpp for RawrEngine.exe.
    RAWRXD_STANDALONE_MAIN=1
    ${_RAWR_ASM_FEATURE_DEFINITIONS}
    $<$<BOOL:${_RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS}>:RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS=1>
    RAWRXD_OMEGA1_ENABLED=1
)

# Lane B: keep RawrEngine minimal (no debugger/ollama shims).
# Sources used by missing_handler_stubs: debugger engine + symbols + Ollama client
# Lane B is deliberately minimal; do not pull these subsystems in.
if(NOT RAWR_ENGINE_LANE_B)
    target_sources(RawrEngine PRIVATE
        src/core/native_debugger_engine.cpp
        src/core/native_debugger_symbols.cpp
        src/agentic/AgentOllamaClient.cpp
        src/core/command_handlers_lsp.cpp
        src/core/command_handlers_asm.cpp
        src/core/command_handlers_hybrid.cpp
        src/core/command_handlers_comprehensive.cpp
        src/inference/NegativeSpaceProfiler.cpp
        # src/core/asm_symbols_stub.cpp  # Removed - has compile errors and duplicates symbols
    )
    # rawrengine_missing_stubs.cpp removed - duplicates symbols from unlinked_symbols_batch_*.cpp
    if(RAWR_SSOT_PROVIDER_UPPER STREQUAL "EXT" OR RAWR_SSOT_PROVIDER_UPPER STREQUAL "STUBS")
        target_sources(RawrEngine PRIVATE src/core/auto_feature_stub_impl.cpp)
    endif()
endif()

# Set MSVC runtime library for RawrEngine
set_property(TARGET RawrEngine PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Win32 system libraries (same as Gold)
if(WIN32)
    target_link_libraries(RawrEngine PRIVATE
        shlwapi psapi dbghelp comctl32 comdlg32 shell32 ole32 oleaut32 uuid
        winhttp ws2_32 ntdll advapi32 crypt32 opengl32 wininet gdi32 user32 bcrypt
        d3d12 dxgi dxcore
    )
    # Link static CRT libraries explicitly for static runtime
    target_link_libraries(RawrEngine PRIVATE
        libcmt.lib libcpmt.lib libvcruntime.lib libucrt.lib
    )
    if(NOT MSVC)
        target_link_libraries(RawrEngine PRIVATE gomp)
    endif()
endif()

# =============================================================================
# Standalone Gold Deployment: RawrXD_Gold.exe
# All MASM modules linked, zero unresolved externals, static CRT
# =============================================================================
set(GOLD_UNDERSCORE_SOURCES ${SOURCES})
list(REMOVE_ITEM GOLD_UNDERSCORE_SOURCES
    src/core/rawrengine_command_handlers.cpp
    src/core/ssot_validation.cpp
)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/agent/quantum_missing_impl.cpp")
    # Keep Gold and RawrEngine on the same non-duplicated quantum time manager lane.
    list(REMOVE_ITEM GOLD_UNDERSCORE_SOURCES src/agent/quantum_dynamic_time_manager_impl.cpp)
endif()
list(APPEND GOLD_UNDERSCORE_SOURCES src/core/ignite_800b.cpp)
# Transcendence coordinator required by rawrengine_command_handlers.cpp
list(APPEND GOLD_UNDERSCORE_SOURCES src/core/transcendence_coordinator.cpp)
# CoT fallback system required by unified_hotpatch_manager.cpp
list(APPEND GOLD_UNDERSCORE_SOURCES src/core/cot_fallback_system.cpp)
# Minimal Gold main entry point
list(APPEND GOLD_UNDERSCORE_SOURCES src/core/minimal_gold_main.cpp)
# rawr_engine_link_shims.cpp removed - it duplicates symbols from streaming_orchestrator.cpp
# and analyzer_distiller.cpp which are already included in SOURCES
# Remove unlinked_symbols_batch files that duplicate symbols from streaming_orchestrator.cpp
# and analyzer_distiller.cpp to avoid LNK2005 errors
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/unlinked_symbols_batch_001\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/unlinked_symbols_batch_002\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/unlinked_symbols_batch_003\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/unlinked_symbols_batch_004\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/unlinked_symbols_batch_005\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/unlinked_symbols_batch_006\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/unlinked_symbols_batch_007\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/unlinked_symbols_batch_008\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/unlinked_symbols_batch_009\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/unlinked_symbols_batch_010\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/unlinked_symbols_batch_011\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/unlinked_symbols_batch_012\\.cpp$")
# Remove RawrXD_LSPServer.cpp - it has its own main() function and is built as a separate executable
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/RawrXD_LSPServer\\.cpp$")
# Remove lsp_hotpatch_bridge.cpp - it depends on RawrXD_LSPServer symbols
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/lsp_hotpatch_bridge\\.cpp$")
# Remove rawr_engine_link_shims.cpp - it duplicates symbols from streaming_orchestrator.cpp
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/rawr_engine_link_shims\\.cpp$")
# Remove C++ files that depend on empty ASM implementations
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/mesh_brain\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/speciator_engine\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/hardware_synthesizer\\.cpp$")
# Additional files with unresolved ASM externals (pyre_compute, hotpatcher, etc.)
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/pyre_compute\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/byte_level_hotpatcher\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/shadow_page_detour\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/auto_repair_orchestrator\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/agent_self_healing_orchestrator\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/rawrxd_linker_closure\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/camellia256_bridge\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/rawrxd_subsystem_api\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/perf_telemetry\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/update_signature\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/neural_bridge\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/omega_orchestrator\\.cpp$")
# Additional exclusions for files that depend on the above
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/unified_hotpatch_manager\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/sentinel_watchdog\\.cpp$")
# More exclusions for files referencing missing implementations
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/hotpatch_symbol_provider\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/gguf_diagnostic_provider\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/feature_handlers\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/iterative_tensor_traversal\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/execution_scheduler\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/agentic_decision_tree\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/agentic_task_graph\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/agent_self_repair\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/model_bruteforce_engine\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/rawrengine_command_handlers\\.cpp$")
# Additional exclusions for remaining unresolved symbols
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/cli_autonomy_loop\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/autonomous_recovery_orchestrator\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/AgentOrchestrator\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/ToolRegistry\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/agentic_deep_thinking_engine\\.cpp$")
# Exclude quantum files that reference AgenticDeepThinkingEngine (unresolved externals)
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/quantum_autonomous_todo_system\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/quantum_production_orchestrator\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/quantum_multi_model_agent_cycling\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/quantum_missing_impl\\.cpp$")
# Exclude streaming_inference_engine.cpp that references KernelArbiter (unresolved externals)
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/streaming_inference_engine\\.cpp$")
# Exclude link_stubs_production.cpp from Gold - ASM files provide these symbols
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/link_stubs_production\\.cpp$")
# Exclude memory_production.cpp from Gold - ASM files provide these symbols (rawr_cpu_has_avx512, etc.)
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/memory_production\\.cpp$")
# Exclude unified_command_dispatch.cpp and agentic_transaction.cpp - they need many command handlers
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/unified_command_dispatch\\.cpp$")
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/agentic_transaction\\.cpp$")
if(RAWR_SSOT_PROVIDER_UPPER STREQUAL "AUTO")
    # Production lane: prefer full handlers over stub providers.
    list(REMOVE_ITEM GOLD_UNDERSCORE_SOURCES
        src/core/missing_handler_stubs.cpp
        src/core/ssot_handlers_ext_runtime_minimal.cpp
        src/core/ssot_handlers_ext_isolated.cpp
    )
    list(APPEND GOLD_UNDERSCORE_SOURCES src/core/ssot_handlers_ext.cpp)
    set(_RAWR_GOLD_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS 1)
else()
    # Legacy lanes: retain fallback ownership to preserve link closure.
    list(REMOVE_ITEM GOLD_UNDERSCORE_SOURCES
        src/core/auto_feature_registry.cpp
        src/core/ssot_handlers.cpp
        src/core/ssot_handlers_ext_runtime_minimal.cpp
        src/core/ssot_handlers_ext_isolated.cpp
    )
    list(APPEND GOLD_UNDERSCORE_SOURCES
        src/core/auto_feature_stub_impl.cpp
        src/core/missing_handler_stubs.cpp
    )
    set(_RAWR_GOLD_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS 0)
endif()
if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
    list(REMOVE_ITEM GOLD_UNDERSCORE_SOURCES src/core/rawrengine_missing_stubs.cpp)
    # Gold strict mode: enforce single-owner policy for SO_/AD_ symbol families.
    list(REMOVE_ITEM GOLD_UNDERSCORE_SOURCES
        src/core/streaming_orchestrator.cpp
        src/core/analyzer_distiller.cpp
    )
    list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/.*_link_shims?\\.cpp$")
    list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/.*_shims?\\.cpp$")
    list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/.*_fallback[^/]*\\.cpp$")
    list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/stubs\\.cpp$")
    list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/.*_stub\\.cpp$")
    list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/stub_.*\\.cpp$")
    list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/shim_.*\\.cpp$")
    list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/.*_(mock|fake)\\.cpp$")
    list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/.*(mock_|fake_).*\\.cpp$")
    list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/.*_stubs\\.asm$")
    list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/stubs\\.asm$")
    list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/.*_stub\\.asm$")
    list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/stub_.*\\.asm$")
    list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/.*_shims?\\.asm$")
    list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/shim_.*\\.asm$")
    list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/.*_link_shims?\\.asm$")
    # Exclude problematic AI experimental ASM files
    list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/ai/.*_scanner\\.asm$")
    list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/ai/.*_kernel\\.asm$")
    list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/.*_fallback[^/]*\\.asm$")
    list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/.*_(mock|fake)\\.asm$")
    list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/.*(mock_|fake_).*\\.asm$")
    list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/link_stubs_.*\\.cpp$")
    list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/link_stubs_.*\\.asm$")
    # Re-add only files that are safe and non-duplicative in Gold strict lane.
    list(APPEND GOLD_UNDERSCORE_SOURCES
        src/core/cot_fallback_system.cpp
        src/core/dml_asm_fallback.cpp
    )
endif()
# Gold: MultiResponseEngine methods are defined inline in win32app/multi_response_engine.h; the
# standalone .cpp duplicates those symbols. AUTO SSOT lane links ssot_handlers_ext (superset)—drop
# ssot_handlers + auto_feature_registry to avoid LNK2005 duplicate CommandResult handlers.
list(REMOVE_ITEM GOLD_UNDERSCORE_SOURCES src/core/multi_response_engine.cpp)
if(RAWR_SSOT_PROVIDER_UPPER STREQUAL "AUTO")
    list(REMOVE_ITEM GOLD_UNDERSCORE_SOURCES
        src/core/ssot_handlers.cpp
        src/core/auto_feature_registry.cpp
    )
endif()
list(REMOVE_DUPLICATES GOLD_UNDERSCORE_SOURCES)
list(REMOVE_ITEM GOLD_UNDERSCORE_SOURCES
    src/complete_server.cpp
    src/modules/vscode_extension_api.cpp
    src/core/js_extension_host.cpp
    src/ui/entry_point.cpp
)

# Add Gold stub implementations for excluded files
# REMOVED: src/gold_stubs/gold_link_stubs.cpp - stub file deleted
# Add Gold-specific stubs for symbols not provided by ASM files
list(APPEND GOLD_UNDERSCORE_SOURCES
    src/core/link_stubs_gold.cpp
    src/reverse/ReverseEngine.cpp
    src/reverse/ReverseModelLoader.cpp
    src/compression/zlib_runtime_loader.cpp
    src/deep2/MASMKernelStubs.cpp
    B014/build/b014_profiler.cpp
    src/runtime/StreamRouterAdapter.cpp
    src/runtime/memory/CapacityManager.cpp
    src/vulkan_kernel_stubs.cpp
    src/runtime/elastic/ElasticEngine.cpp
    src/runtime/elastic/ElasticGGUFIndex.cpp
    src/runtime/elastic/ElasticResidencyManager.cpp
    src/runtime/elastic/VulkanTensorResidencyBackend.cpp
    src/runtime/elastic/ElasticDecoder.cpp
    src/runtime/governance/UnifiedTriggerOrchestrator.cpp
    src/runtime/governance/HardwareCapabilityProbe.cpp
    src/runtime/governance/CapabilityTierMatrix.cpp
    src/runtime/governance/HardwareGovernor.cpp
    src/runtime/governance/PressedStateEngine.cpp
    src/runtime/governance/Ensemble58Governor.cpp
    src/runtime/governance/TLSInstructionGate.cpp
    src/runtime/gguf_tensor_loader.cpp
    src/runtime/Deep2ExecutionTelemetry.cpp
)
# Remove duplicate stub files that are already in SOURCES or have real implementations
list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/deep2_link_stubs\\.cpp$")
# NOTE: vulkan_kernel_stubs.cpp is REQUIRED for RawrXD_Gold because
# VulkanKernel_DispatchRaw.asm references VulkanKernel_DispatchRaw_Impl
# which is defined in vulkan_kernel_stubs.cpp. Do NOT exclude it.

add_executable(RawrXD_Gold ${GOLD_UNDERSCORE_SOURCES} ${ASM_KERNEL_SOURCES})

if(WIN32)
    add_dependencies(RawrXD_Gold regen-ssot-beacons)
endif()

# ═════════════════════════════════════════════════════════════════════════════
# OMEGA-1 Post-Build Manifest Generation for Gold
# ═════════════════════════════════════════════════════════════════════════════
if(WIN32)
    add_custom_command(TARGET RawrXD_Gold POST_BUILD
        COMMAND powershell -ExecutionPolicy Bypass -File ${OMEGA1_DIR}/Generate-Manifest.ps1
                -BinaryPath $<TARGET_FILE:RawrXD_Gold>
                -OutputPath ${CMAKE_BINARY_DIR}/gold/omega1_manifest.json
        COMMENT "Generating OMEGA-1 manifest for RawrXD_Gold"
        VERBATIM
    )
endif()

target_link_libraries(RawrXD_Gold PRIVATE RawrXD-RE-Library Omega1Engine)

target_include_directories(RawrXD_Gold PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/server
    ${CMAKE_CURRENT_SOURCE_DIR}/src/agent
    ${CMAKE_CURRENT_SOURCE_DIR}/src/agentic
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cli
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/git
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lsp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gpu
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tools
    ${CMAKE_CURRENT_SOURCE_DIR}/src/telemetry
    ${CMAKE_CURRENT_SOURCE_DIR}/src/reverse_engineering
    ${CMAKE_CURRENT_SOURCE_DIR}/src/win32app
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sovereign
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sovereign/puppeteer
    ${CMAKE_CURRENT_SOURCE_DIR}/src/intent
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guardrails
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hotpatch
    ${CMAKE_CURRENT_SOURCE_DIR}/src/kernel
    ${CMAKE_CURRENT_SOURCE_DIR}/src/omega1_modules
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/gpu
)

if(RAWRXD_INCLUDE_STRESS_AND_REPLAY_SOURCES)
    target_include_directories(RawrEngine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/test_harness)
    target_include_directories(RawrXD_Gold PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/test_harness)
endif()

target_link_libraries(RawrXD_Gold PRIVATE Threads::Threads)

target_compile_definitions(RawrXD_Gold PRIVATE
    RAWRXD_STRICT_PRODUCTION_PROFILE=$<IF:$<BOOL:${RAWRXD_PRODUCTION_STRIP_STUB_SOURCES}>,1,0>
    RAWRXD_GOLD_BUILD=1
    RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS=${_RAWR_GOLD_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS}
    RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=$<IF:$<BOOL:${RAWRXD_PRODUCTION_STRIP_STUB_SOURCES}>,1,0>
    ${_RAWR_ASM_FEATURE_DEFINITIONS}
    RAWRXD_OMEGA1_ENABLED=1
)
if(RAWR_HAS_MASM)
    target_compile_definitions(RawrXD_Gold PRIVATE HAS_BRUTAL_GZIP_MASM=1)
endif()

if(MSVC)
    target_compile_options(RawrXD_Gold PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:/EHsc /W0 /arch:AVX512>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CONFIG:Debug>>>:/O2>
    )
    target_link_options(RawrXD_Gold PRIVATE /LARGEADDRESSAWARE:NO)
    # Set proper entry point for Windows subsystem application with wWinMain
    target_link_options(RawrXD_Gold PRIVATE /ENTRY:wWinMainCRTStartup)
    if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
        set(_GOLD_MAP_PATH "${CMAKE_BINARY_DIR}/gold/RawrXD_Gold.map")
        set(_GOLD_MAP_AUDIT_PATH "${CMAKE_BINARY_DIR}/RawrXD_Gold.map")
        target_link_options(RawrXD_Gold PRIVATE
            "/MAP:${_GOLD_MAP_PATH}"
            /MAPINFO:EXPORTS
        )
        add_custom_command(TARGET RawrXD_Gold POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_GOLD_MAP_PATH}" "${_GOLD_MAP_AUDIT_PATH}"
            VERBATIM
        )
    endif()
else()
    target_compile_options(RawrXD_Gold PRIVATE -march=native -O3 -w -mavx2 -mfma -mavx512f -mavx512bw -mavx512dq -mavx512vl -fopenmp)
endif()

set_target_properties(RawrXD_Gold PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/gold"
    OUTPUT_NAME RawrXD_Gold
)

# Win32 system libraries
if(WIN32)
    target_link_libraries(RawrXD_Gold PRIVATE
        shlwapi psapi dbghelp comctl32 comdlg32 shell32 ole32 oleaut32 uuid
        winhttp ws2_32 ntdll advapi32 crypt32 opengl32 wininet gdi32 user32 bcrypt
        d3d12 dxgi dxcore
    )
    if(MSVC)
        # Use default CRT linking - CMake handles this
        # Do not explicitly link CRT libraries to avoid conflicts
    endif()
    if(NOT MSVC)
        target_link_libraries(RawrXD_Gold PRIVATE gomp)
    endif()
endif()

# Gold-only sources not present in the main SOURCES aggregation lane.
target_sources(RawrXD_Gold PRIVATE
    # src/core/gold_patch_symbol_stubs.cpp  # Removed - symbols now in rawr_engine_link_shims.cpp
    src/core/js_extension_host_headless_impl.cpp
    src/core/native_debugger_engine.cpp
    src/core/native_debugger_symbols.cpp
    src/agentic/AgentOllamaClient.cpp
    src/logging/Logger.cpp
    src/security/InputSanitizer.cpp
    src/core/ConfigurationValidator.cpp
    src/inference/PerformanceMonitor.cpp
    src/agentic/ErrorRecoveryManager.cpp
    # Link closure for unified/ssot lanes: implementations live outside main SOURCES or were stripped.
    tests/gguf_link_impl.cpp
    src/core/rawrengine_command_handlers.cpp
    src/agent/quantum_dynamic_time_manager_impl.cpp
    src/core/native_gguf_loader_link_impl.cpp
    src/agentic/AdvancedAgentCoordinator.cpp
    src/BackendOrchestrator.cpp
    src/core/gold_inference_profiler_minimal.cpp
    src/agentic/hotpatch/Engine.cpp
    src/core/gold_enterprise_devunlock_impl.cpp
    src/core/gold_beacon_handlers.cpp
    src/core/neural_bridge.cpp
    src/core/omega_orchestrator.cpp
    src/core/mesh_brain.cpp
    src/core/speciator_engine.cpp
    src/core/self_host_engine.cpp
    src/core/hardware_synthesizer.cpp
    # src/ui/entry_point.cpp  # Removed - conflicts with minimal_gold_main.cpp
    src/ui/webview2_bridge.cpp
    src/ui/RawrXD_MainLoop.cpp
    src/ui/debugger_core.cpp
    src/inference/gpu_dispatch_gate.cpp
    src/gguf_d3d12_bridge.cpp
    src/asm/RawrXD_PE_Exporter.asm
)

# Vulkan SDK for Gold build — linked later after find_package(Vulkan)

message(STATUS "[Gold] Standalone deployment target: RawrXD_Gold.exe (all MASM linked)")

# ═══════════════════════════════════════════════════════════════════
# MASM x64 Assembly — Phase 20/21 AVX-512 Requantization + Swarm Streaming
# ═══════════════════════════════════════════════════════════════════
if(MSVC)
    # Prefer prebuilt MASM objects when available (some checked-in .asm files are placeholders).
    set(RAWRXD_PREBUILT_OBJ_DIR "${CMAKE_CURRENT_SOURCE_DIR}/obj")
    set(RAWRXD_PREBUILT_GENESIS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build_genesis")
    set(RAWRXD_PREBUILT_SRCASM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/asm")

    # Phase 20: AVX-512 Q4_K_M → Q2_K requantization kernel
    set(ASM_REQUANTIZE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/requantize_q4km_to_q2k_avx512.asm")
    set(ASM_REQUANTIZE_OBJ "${CMAKE_CURRENT_BINARY_DIR}/requantize_q4km_to_q2k_avx512.obj")
    add_custom_command(
        OUTPUT ${ASM_REQUANTIZE_OBJ}
        COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /Fo${ASM_REQUANTIZE_OBJ} ${ASM_REQUANTIZE_SRC}
        DEPENDS ${ASM_REQUANTIZE_SRC}
        COMMENT "[MASM] Assembling requantize_q4km_to_q2k_avx512.asm (AVX-512 DQ/BW)"
        VERBATIM
    )

    # Phase 20: ROCm GPU requantization dispatch stubs
    set(ASM_GPU_REQUANT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/gpu_requantize_rocm.asm")
    set(ASM_GPU_REQUANT_OBJ "${CMAKE_CURRENT_BINARY_DIR}/gpu_requantize_rocm.obj")
    add_custom_command(
        OUTPUT ${ASM_GPU_REQUANT_OBJ}
        COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /Fo${ASM_GPU_REQUANT_OBJ} ${ASM_GPU_REQUANT_SRC}
        DEPENDS ${ASM_GPU_REQUANT_SRC}
        COMMENT "[MASM] Assembling gpu_requantize_rocm.asm (HIP dispatch)"
        VERBATIM
    )

    # Phase 21: Zero-copy tensor network streaming
    set(ASM_SWARM_TENSOR_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/swarm_tensor_stream.asm")
    set(ASM_SWARM_TENSOR_OBJ "${CMAKE_CURRENT_BINARY_DIR}/swarm_tensor_stream.obj")
    add_custom_command(
        OUTPUT ${ASM_SWARM_TENSOR_OBJ}
        COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /Fo${ASM_SWARM_TENSOR_OBJ} ${ASM_SWARM_TENSOR_SRC}
        DEPENDS ${ASM_SWARM_TENSOR_SRC}
        COMMENT "[MASM] Assembling swarm_tensor_stream.asm (CRC32C, RLE, chunks)"
        VERBATIM
    )

    # Phase 28: MonacoCore Native Editor Engine — Gap Buffer + Tokenizer
    set(ASM_MONACOCORE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_MonacoCore.asm")
    set(ASM_MONACOCORE_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_MonacoCore.obj")
    add_custom_command(
        OUTPUT ${ASM_MONACOCORE_OBJ}
        COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /Fo${ASM_MONACOCORE_OBJ} ${ASM_MONACOCORE_SRC}
        DEPENDS ${ASM_MONACOCORE_SRC}
        COMMENT "[MASM] Assembling RawrXD_MonacoCore.asm (Gap Buffer, Tokenizer)"
        VERBATIM
    )

    # Phase 29: PDB Symbol Server — Native MSF v7.00 Parser Kernel
    set(ASM_PDBKERNEL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_PDBKernel.asm")
    set(ASM_PDBKERNEL_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_PDBKernel.obj")
    add_custom_command(
        OUTPUT ${ASM_PDBKERNEL_OBJ}
        COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /Fo${ASM_PDBKERNEL_OBJ} ${ASM_PDBKERNEL_SRC}
        DEPENDS ${ASM_PDBKERNEL_SRC}
        COMMENT "[MASM] Assembling RawrXD_PDBKernel.asm (MSF magic, PubSym scan, GUID hex)"
        VERBATIM
    )

    # Phase 30: Accelerator Router — MASM Fast-Path Bridge
    set(ASM_ROUTERBRIDGE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_RouterBridge.asm")
    set(ASM_ROUTERBRIDGE_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_RouterBridge.obj")
    add_custom_command(
        OUTPUT ${ASM_ROUTERBRIDGE_OBJ}
        COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /Fo${ASM_ROUTERBRIDGE_OBJ} ${ASM_ROUTERBRIDGE_SRC}
        DEPENDS ${ASM_ROUTERBRIDGE_SRC}
        COMMENT "[MASM] Assembling RawrXD_RouterBridge.asm (Fast-Path Router Dispatch)"
        VERBATIM
    )

    # Phase 29.2: GSI Hash — MASM Accelerated Hash + Bucket Probe
    set(ASM_GSIHASH_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_GSIHash.asm")
    set(ASM_GSIHASH_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_GSIHash.obj")
    add_custom_command(
        OUTPUT ${ASM_GSIHASH_OBJ}
        COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /Fo${ASM_GSIHASH_OBJ} ${ASM_GSIHASH_SRC}
        DEPENDS ${ASM_GSIHASH_SRC}
        COMMENT "[MASM] Assembling RawrXD_GSIHash.asm (PDB hash, GSI bucket probe)"
        VERBATIM
    )

    # Phase 29.2: Reference Provider — FNV-1a Hash + Pattern Match
    set(ASM_REFPROVIDER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_RefProvider.asm")
    set(ASM_REFPROVIDER_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_RefProvider.obj")
    add_custom_command(
        OUTPUT ${ASM_REFPROVIDER_OBJ}
        COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /Fo${ASM_REFPROVIDER_OBJ} ${ASM_REFPROVIDER_SRC}
        DEPENDS ${ASM_REFPROVIDER_SRC}
        COMMENT "[MASM] Assembling RawrXD_RefProvider.asm (Reference hash, pattern match)"
        VERBATIM
    )

    # Phase 31: IDE Self-Audit — Stub Detection Kernel
    set(ASM_STUBDETECTOR_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_StubDetector.asm")
    set(ASM_STUBDETECTOR_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_StubDetector.obj")
    add_custom_command(
        OUTPUT ${ASM_STUBDETECTOR_OBJ}
        COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /Fo${ASM_STUBDETECTOR_OBJ} ${ASM_STUBDETECTOR_SRC}
        DEPENDS ${ASM_STUBDETECTOR_SRC}
        COMMENT "[MASM] Assembling RawrXD_StubDetector.asm (Stub Pattern Scanner)"
        VERBATIM
    )

    # Phase 37: Chain-of-Thought Engine — Pure MASM CoT DLL Core
    set(ASM_COT_ENGINE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/rawrxd_cot_engine.asm")
    set(ASM_COT_ENGINE_OBJ "${CMAKE_CURRENT_BINARY_DIR}/rawrxd_cot_engine.obj")
    add_custom_command(
        OUTPUT ${ASM_COT_ENGINE_OBJ}
        COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /Fo${ASM_COT_ENGINE_OBJ} ${ASM_COT_ENGINE_SRC}
        DEPENDS ${ASM_COT_ENGINE_SRC}
        COMMENT "[MASM] Assembling rawrxd_cot_engine.asm (CoT Engine: 12 roles, 6 presets, 1GB arena, SEH)"
        VERBATIM
    )

    # Phase 37: Chain-of-Thought Engine — DLL Entry + SRW Lock
    set(ASM_COT_DLL_ENTRY_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/rawrxd_cot_dll_entry.asm")
    set(ASM_COT_DLL_ENTRY_OBJ "${CMAKE_CURRENT_BINARY_DIR}/rawrxd_cot_dll_entry.obj")
    add_custom_command(
        OUTPUT ${ASM_COT_DLL_ENTRY_OBJ}
        COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /Fo${ASM_COT_DLL_ENTRY_OBJ} ${ASM_COT_DLL_ENTRY_SRC}
        DEPENDS ${ASM_COT_DLL_ENTRY_SRC}
        COMMENT "[MASM] Assembling rawrxd_cot_dll_entry.asm (DllMain, SRW Lock, Thread Tracking)"
        VERBATIM
    )

    # Phase 39–42: CoT Enterprise Finishers — Snapshot, Telemetry, Copy Engine, Security
    set(ASM_COT_PHASE39_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/rawrxd_cot_phase39.asm")
    set(ASM_COT_PHASE39_OBJ "${CMAKE_CURRENT_BINARY_DIR}/rawrxd_cot_phase39.obj")
    add_custom_command(
        OUTPUT ${ASM_COT_PHASE39_OBJ}
        COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /Fo${ASM_COT_PHASE39_OBJ} ${ASM_COT_PHASE39_SRC}
        DEPENDS ${ASM_COT_PHASE39_SRC}
        COMMENT "[MASM] Assembling rawrxd_cot_phase39.asm (Snapshot, Telemetry, CopyEngine, Seal, Quotas)"
        VERBATIM
    )

    # LSP Bridge: MASM64 Symbol Index Accelerator (binary search, batch FNV-1a, prefix scan)
    set(ASM_LSP_SYMINDEX_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_LSP_SymbolIndex.asm")
    set(ASM_LSP_SYMINDEX_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_LSP_SymbolIndex.obj")
    add_custom_command(
        OUTPUT ${ASM_LSP_SYMINDEX_OBJ}
        COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /Fo${ASM_LSP_SYMINDEX_OBJ} ${ASM_LSP_SYMINDEX_SRC}
        DEPENDS ${ASM_LSP_SYMINDEX_SRC}
        COMMENT "[MASM] Assembling RawrXD_LSP_SymbolIndex.asm (Hash Lookup, Batch FNV-1a, Prefix Scan)"
        VERBATIM
    )

    # Phase 45: RawrCodex — Agentic Decision Tree Core (295KB, 21 procs)
    set(ASM_RAWRCODEX_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrCodex.asm")
    set(ASM_RAWRCODEX_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrCodex.obj")
    add_custom_command(
        OUTPUT ${ASM_RAWRCODEX_OBJ}
        COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /Fo${ASM_RAWRCODEX_OBJ} ${ASM_RAWRCODEX_SRC}
        DEPENDS ${ASM_RAWRCODEX_SRC}
        COMMENT "[MASM] Assembling RawrCodex.asm (Agentic Decision Tree, Pattern Engine)"
        VERBATIM
    )

    # Phase 45: Debug Engine — Native Debugger Kernel (51KB, DynTrace symbolic resolution)
    set(ASM_DEBUG_ENGINE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_Debug_Engine.asm")
    set(ASM_DEBUG_ENGINE_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_Debug_Engine.obj")
    add_custom_command(
        OUTPUT ${ASM_DEBUG_ENGINE_OBJ}
        COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /Fo${ASM_DEBUG_ENGINE_OBJ} ${ASM_DEBUG_ENGINE_SRC}
        DEPENDS ${ASM_DEBUG_ENGINE_SRC}
        COMMENT "[MASM] Assembling RawrXD_Debug_Engine.asm (Breakpoints, Watchpoints, Symbolic DynTrace)"
        VERBATIM
    )

    # Phase 45: QuadBuffer Streamer — High-Performance Memory Streaming (52KB)
    set(ASM_QUADBUFFER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_QuadBuffer_Streamer.asm")
    set(ASM_QUADBUFFER_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_QuadBuffer_Streamer.obj")
    add_custom_command(
        OUTPUT ${ASM_QUADBUFFER_OBJ}
        COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /Fo${ASM_QUADBUFFER_OBJ} ${ASM_QUADBUFFER_SRC}
        DEPENDS ${ASM_QUADBUFFER_SRC}
        COMMENT "[MASM] Assembling RawrXD_QuadBuffer_Streamer.asm (4-Buffer Ring, Zero-Copy Streaming)"
        VERBATIM
    )

    # Phase 45: Swarm Network — Distributed Composition Protocol (36KB)
    set(ASM_SWARM_NETWORK_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_Swarm_Network.asm")
    set(ASM_SWARM_NETWORK_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_Swarm_Network.obj")
    add_custom_command(
        OUTPUT ${ASM_SWARM_NETWORK_OBJ}
        COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /Fo${ASM_SWARM_NETWORK_OBJ} ${ASM_SWARM_NETWORK_SRC}
        DEPENDS ${ASM_SWARM_NETWORK_SRC}
        COMMENT "[MASM] Assembling RawrXD_Swarm_Network.asm (P2P Discovery, Task Distribution)"
        VERBATIM
    )

    # Phase 45: Enterprise License — Feature Gate + Hardware Fingerprint (31KB)
    set(ASM_ENTERPRISE_LIC_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_EnterpriseLicense.asm")
    set(ASM_ENTERPRISE_LIC_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_EnterpriseLicense.obj")
    add_custom_command(
        OUTPUT ${ASM_ENTERPRISE_LIC_OBJ}
        COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /Fo${ASM_ENTERPRISE_LIC_OBJ} ${ASM_ENTERPRISE_LIC_SRC}
        DEPENDS ${ASM_ENTERPRISE_LIC_SRC}
        COMMENT "[MASM] Assembling RawrXD_EnterpriseLicense.asm (License Validation, HWID, Feature Gates)"
        VERBATIM
    )

    # Phase 45: License Shield — Anti-Tamper + Integrity Guard (34KB)
    set(ASM_LICENSE_SHIELD_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_License_Shield.asm")
    set(ASM_LICENSE_SHIELD_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_License_Shield.obj")
    add_custom_command(
        OUTPUT ${ASM_LICENSE_SHIELD_OBJ}
        COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /Fo${ASM_LICENSE_SHIELD_OBJ} ${ASM_LICENSE_SHIELD_SRC}
        DEPENDS ${ASM_LICENSE_SHIELD_SRC}
        COMMENT "[MASM] Assembling RawrXD_License_Shield.asm (CRC32 Integrity, Anti-Debug, Tamper Detection)"
        VERBATIM
    )

    # Phase 46: VulkanBridge — MASM64 GPU Dispatch Shim (VkBridge_Init, MatMul, FlashAttn, Hotswap)
    set(ASM_VULKANBRIDGE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_VulkanBridge.asm")
    set(ASM_VULKANBRIDGE_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_VulkanBridge.obj")
    add_custom_command(
        OUTPUT ${ASM_VULKANBRIDGE_OBJ}
        COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /Fo${ASM_VULKANBRIDGE_OBJ} ${ASM_VULKANBRIDGE_SRC}
        DEPENDS ${ASM_VULKANBRIDGE_SRC}
        COMMENT "[MASM] Assembling RawrXD_VulkanBridge.asm (GPU Dispatch Shim: Init, MatMul, FlashAttn, Hotswap)"
        VERBATIM
    )

    # Phase 49: SocketResponder — MASM64 IOCP Zero-Copy HTTP Server
    set(ASM_SOCKETRESP_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_SocketResponder.asm")
    set(_ASM_SOCKETRESP_PREBUILT "${RAWRXD_PREBUILT_GENESIS_DIR}/RawrXD_SocketResponder.obj")
    if(EXISTS "${_ASM_SOCKETRESP_PREBUILT}")
        set(ASM_SOCKETRESP_OBJ "${_ASM_SOCKETRESP_PREBUILT}")
        message(STATUS "[MASM] Using prebuilt RawrXD_SocketResponder.obj")
    else()
        set(ASM_SOCKETRESP_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_SocketResponder.obj")
        add_custom_command(
            OUTPUT ${ASM_SOCKETRESP_OBJ}
            COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /I "${CMAKE_CURRENT_SOURCE_DIR}/src/asm" /Fo${ASM_SOCKETRESP_OBJ} ${ASM_SOCKETRESP_SRC}
            DEPENDS ${ASM_SOCKETRESP_SRC}
            COMMENT "[MASM] Assembling RawrXD_SocketResponder.asm (IOCP HTTP: /health /status /models sub-0.5ms)"
            VERBATIM
        )
    endif()

    # Phase 49: TridentBeacon — MASM64 COM Trident Host + Beacon JS Bridge
    set(ASM_TRIDENTBEACON_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_TridentBeacon.asm")
    set(_ASM_TRIDENTBEACON_PREBUILT "${RAWRXD_PREBUILT_GENESIS_DIR}/RawrXD_TridentBeacon.obj")
    if(EXISTS "${_ASM_TRIDENTBEACON_PREBUILT}")
        set(ASM_TRIDENTBEACON_OBJ "${_ASM_TRIDENTBEACON_PREBUILT}")
        message(STATUS "[MASM] Using prebuilt RawrXD_TridentBeacon.obj")
    else()
        set(ASM_TRIDENTBEACON_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_TridentBeacon.obj")
        add_custom_command(
            OUTPUT ${ASM_TRIDENTBEACON_OBJ}
            COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /I "${CMAKE_CURRENT_SOURCE_DIR}/src/asm" /Fo${ASM_TRIDENTBEACON_OBJ} ${ASM_TRIDENTBEACON_SRC}
            DEPENDS ${ASM_TRIDENTBEACON_SRC}
            COMMENT "[MASM] Assembling RawrXD_TridentBeacon.asm (COM Trident Host, Beacon IDispatch, Ollama Bridge)"
            VERBATIM
        )
    endif()

    # Phase 49: TridentBeacon GPU — MASM64 GPU Acceleration + WebGL Bridge
    set(ASM_TRIDENTGPU_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_TridentBeacon_GPU.asm")
    set(_ASM_TRIDENTGPU_PREBUILT "${RAWRXD_PREBUILT_GENESIS_DIR}/RawrXD_TridentBeacon_GPU.obj")
    if(EXISTS "${_ASM_TRIDENTGPU_PREBUILT}")
        set(ASM_TRIDENTGPU_OBJ "${_ASM_TRIDENTGPU_PREBUILT}")
        message(STATUS "[MASM] Using prebuilt RawrXD_TridentBeacon_GPU.obj")
    else()
        set(ASM_TRIDENTGPU_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_TridentBeacon_GPU.obj")
        add_custom_command(
            OUTPUT ${ASM_TRIDENTGPU_OBJ}
            COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /I "${CMAKE_CURRENT_SOURCE_DIR}/src/asm" /Fo${ASM_TRIDENTGPU_OBJ} ${ASM_TRIDENTGPU_SRC}
            DEPENDS ${ASM_TRIDENTGPU_SRC}
            COMMENT "[MASM] Assembling RawrXD_TridentBeacon_GPU.asm (IE11 Feature Control, WebGL FBO, OpenGL Bridge)"
            VERBATIM
        )
    endif()

    # Phase 50: AgenticOrchestrator — VTable Dispatch + SPSC Task Queue
    set(ASM_AGENTIC_ORCH_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_AgenticOrchestrator.asm")
    set(_ASM_AGENTIC_ORCH_PREBUILT1 "${RAWRXD_PREBUILT_SRCASM_DIR}/RawrXD_AgenticOrchestrator.obj")
    set(_ASM_AGENTIC_ORCH_PREBUILT2 "${RAWRXD_PREBUILT_GENESIS_DIR}/RawrXD_AgenticOrchestrator.obj")
    if(EXISTS "${_ASM_AGENTIC_ORCH_PREBUILT1}")
        set(ASM_AGENTIC_ORCH_OBJ "${_ASM_AGENTIC_ORCH_PREBUILT1}")
        message(STATUS "[MASM] Using prebuilt RawrXD_AgenticOrchestrator.obj")
    elseif(EXISTS "${_ASM_AGENTIC_ORCH_PREBUILT2}")
        set(ASM_AGENTIC_ORCH_OBJ "${_ASM_AGENTIC_ORCH_PREBUILT2}")
        message(STATUS "[MASM] Using prebuilt RawrXD_AgenticOrchestrator.obj (genesis)")
    else()
        set(ASM_AGENTIC_ORCH_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_AgenticOrchestrator.obj")
        add_custom_command(
            OUTPUT ${ASM_AGENTIC_ORCH_OBJ}
            COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /I "${CMAKE_CURRENT_SOURCE_DIR}/src/asm" /Fo${ASM_AGENTIC_ORCH_OBJ} ${ASM_AGENTIC_ORCH_SRC}
            DEPENDS ${ASM_AGENTIC_ORCH_SRC}
            COMMENT "[MASM] Assembling RawrXD_AgenticOrchestrator.asm (23-Handler VTable, SPSC Queue, Metrics)"
            VERBATIM
        )
    endif()

    # Phase 50: GGUF Vulkan Loader — Memory-Mapped Parser + Staging Buffer
    set(ASM_GGUF_VKLOAD_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_GGUF_Vulkan_Loader.asm")
    set(_ASM_GGUF_VKLOAD_PREBUILT1 "${RAWRXD_PREBUILT_OBJ_DIR}/asm_RawrXD_GGUF_Vulkan_Loader.obj")
    set(_ASM_GGUF_VKLOAD_PREBUILT2 "${RAWRXD_PREBUILT_GENESIS_DIR}/RawrXD_GGUF_Vulkan_Loader.obj")
    if(EXISTS "${_ASM_GGUF_VKLOAD_PREBUILT1}")
        set(ASM_GGUF_VKLOAD_OBJ "${_ASM_GGUF_VKLOAD_PREBUILT1}")
        message(STATUS "[MASM] Using prebuilt RawrXD_GGUF_Vulkan_Loader.obj")
    elseif(EXISTS "${_ASM_GGUF_VKLOAD_PREBUILT2}")
        set(ASM_GGUF_VKLOAD_OBJ "${_ASM_GGUF_VKLOAD_PREBUILT2}")
        message(STATUS "[MASM] Using prebuilt RawrXD_GGUF_Vulkan_Loader.obj (genesis)")
    else()
        set(ASM_GGUF_VKLOAD_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_GGUF_Vulkan_Loader.obj")
        add_custom_command(
            OUTPUT ${ASM_GGUF_VKLOAD_OBJ}
            COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /I "${CMAKE_CURRENT_SOURCE_DIR}/src/asm" /Fo${ASM_GGUF_VKLOAD_OBJ} ${ASM_GGUF_VKLOAD_SRC}
            DEPENDS ${ASM_GGUF_VKLOAD_SRC}
            COMMENT "[MASM] Assembling RawrXD_GGUF_Vulkan_Loader.asm (GGUF v3, FNV-1a, Tensor Staging, GGML Types)"
            VERBATIM
        )
    endif()

    # Phase 50: LSP-AI Bridge — SymbolIndex ↔ ContextAnalyzer Fusion
    set(ASM_LSP_BRIDGE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_LSP_AI_Bridge.asm")
    set(_ASM_LSP_BRIDGE_PREBUILT1 "${RAWRXD_PREBUILT_OBJ_DIR}/asm_RawrXD_LSP_AI_Bridge.obj")
    set(_ASM_LSP_BRIDGE_PREBUILT2 "${RAWRXD_PREBUILT_GENESIS_DIR}/RawrXD_LSP_AI_Bridge.obj")
    if(EXISTS "${_ASM_LSP_BRIDGE_PREBUILT1}")
        set(ASM_LSP_BRIDGE_OBJ "${_ASM_LSP_BRIDGE_PREBUILT1}")
        message(STATUS "[MASM] Using prebuilt RawrXD_LSP_AI_Bridge.obj")
    elseif(EXISTS "${_ASM_LSP_BRIDGE_PREBUILT2}")
        set(ASM_LSP_BRIDGE_OBJ "${_ASM_LSP_BRIDGE_PREBUILT2}")
        message(STATUS "[MASM] Using prebuilt RawrXD_LSP_AI_Bridge.obj (genesis)")
    else()
        set(ASM_LSP_BRIDGE_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_LSP_AI_Bridge.obj")
        add_custom_command(
            OUTPUT ${ASM_LSP_BRIDGE_OBJ}
            COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /I "${CMAKE_CURRENT_SOURCE_DIR}/src/asm" /Fo${ASM_LSP_BRIDGE_OBJ} ${ASM_LSP_BRIDGE_SRC}
            DEPENDS ${ASM_LSP_BRIDGE_SRC}
            COMMENT "[MASM] Assembling RawrXD_LSP_AI_Bridge.asm (Symbol Ranking, Weight Fusion, Kind Boost)"
            VERBATIM
        )
    endif()

    # Phase 50: Streaming QuadBuffer — HWND Token Rendering + GDI Double-Buffer
    set(ASM_STREAM_QBUF_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_Streaming_QuadBuffer.asm")
    set(_ASM_STREAM_QBUF_PREBUILT "${RAWRXD_PREBUILT_GENESIS_DIR}/RawrXD_Streaming_QuadBuffer.obj")

    # Batch 2 kernels are intentionally not manually assembled here.
    # Keep manual MASM object generation limited to entries wired into MASM_OBJECTS.

    if(EXISTS "${_ASM_STREAM_QBUF_PREBUILT}")
        set(ASM_STREAM_QBUF_OBJ "${_ASM_STREAM_QBUF_PREBUILT}")
        message(STATUS "[MASM] Using prebuilt RawrXD_Streaming_QuadBuffer.obj")
    else()
        set(ASM_STREAM_QBUF_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_Streaming_QuadBuffer.obj")
        add_custom_command(
            OUTPUT ${ASM_STREAM_QBUF_OBJ}
            COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /I "${CMAKE_CURRENT_SOURCE_DIR}/src/asm" /Fo${ASM_STREAM_QBUF_OBJ} ${ASM_STREAM_QBUF_SRC}
            DEPENDS ${ASM_STREAM_QBUF_SRC}
            COMMENT "[MASM] Assembling RawrXD_Streaming_QuadBuffer.asm (SPSC Ring, GDI BitBlt, 60 FPS Render)"
            VERBATIM
        )
    endif()

    # Phase 50: SelfPatch Engine — Runtime Code Morphing + CPU Optimization
    set(ASM_SPENGINE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_SelfPatch_Engine.asm")
    set(_ASM_SPENGINE_PREBUILT "${RAWRXD_PREBUILT_GENESIS_DIR}/RawrXD_SelfPatch_Engine.obj")
    if(EXISTS "${_ASM_SPENGINE_PREBUILT}")
        set(ASM_SPENGINE_OBJ "${_ASM_SPENGINE_PREBUILT}")
        message(STATUS "[MASM] Using prebuilt RawrXD_SelfPatch_Engine.obj")
    else()
        set(ASM_SPENGINE_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_SelfPatch_Engine.obj")
        add_custom_command(
            OUTPUT ${ASM_SPENGINE_OBJ}
            COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /I "${CMAKE_CURRENT_SOURCE_DIR}/src/asm" /Fo${ASM_SPENGINE_OBJ} ${ASM_SPENGINE_SRC}
            DEPENDS ${ASM_SPENGINE_SRC}
            COMMENT "[MASM] Assembling RawrXD_SelfPatch_Engine.asm (Hotpatch Table, Code Caves, CPUID Optimize, Quant Switch)"
            VERBATIM
        )
    endif()

    # Phase RDNA3: RX 7800 XT GPU Kernels — Q4MatMul, KV Attention, Tile Streamer
    # Prebuilt kernel object with embedded RDNA3 ISA (gfx1101)
    # Note: Kernels are in d:/rawrxd/src/kernels/rdna3/, not in ci-bootstrap
    set(ASM_RDNA3_KERNELS_SRC "D:/rawrxd/src/kernels/rdna3/RDNA3_Full_Test.asm")
    set(_ASM_RDNA3_KERNELS_PREBUILT "D:/rawrxd/src/kernels/rdna3/obj/RDNA3_Full_Test.obj")
    if(EXISTS "${_ASM_RDNA3_KERNELS_PREBUILT}")
        set(ASM_RDNA3_KERNELS_OBJ "${_ASM_RDNA3_KERNELS_PREBUILT}")
        message(STATUS "[MASM] Using prebuilt RDNA3_Full_Test.obj (RX 7800 XT kernels)")
    elseif(EXISTS "D:/rawrxd/src/kernels/rdna3/RDNA3_Full_Test.asm")
        set(ASM_RDNA3_KERNELS_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RDNA3_Full_Test.obj")
        add_custom_command(
            OUTPUT ${ASM_RDNA3_KERNELS_OBJ}
            COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /W3 /nologo /Zi /Fo${ASM_RDNA3_KERNELS_OBJ} "D:/rawrxd/src/kernels/rdna3/RDNA3_Full_Test.asm"
            DEPENDS "D:/rawrxd/src/kernels/rdna3/RDNA3_Full_Test.asm"
            COMMENT "[MASM] Assembling RDNA3_Full_Test.asm (Q4MatMul, KV Attention, Tile Streamer for gfx1101)"
            VERBATIM
        )
    else()
        message(WARNING "[MASM] RDNA3 kernels not found. GPU acceleration disabled.")
        set(ASM_RDNA3_KERNELS_OBJ "")
    endif()

    # Phase Camellia: Camellia-256 Encryption Engine — MASM x64 CTR-mode workspace encryption
    set(ASM_CAMELLIA256_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_Camellia256.asm")
    set(_ASM_CAMELLIA256_PREBUILT1 "${RAWRXD_PREBUILT_SRCASM_DIR}/RawrXD_Camellia256.obj")
    set(_ASM_CAMELLIA256_PREBUILT2 "${RAWRXD_PREBUILT_GENESIS_DIR}/RawrXD_Camellia256.obj")
    if(EXISTS "${_ASM_CAMELLIA256_PREBUILT1}")
        set(ASM_CAMELLIA256_OBJ "${_ASM_CAMELLIA256_PREBUILT1}")
        message(STATUS "[MASM] Using prebuilt RawrXD_Camellia256.obj")
    elseif(EXISTS "${_ASM_CAMELLIA256_PREBUILT2}")
        set(ASM_CAMELLIA256_OBJ "${_ASM_CAMELLIA256_PREBUILT2}")
        message(STATUS "[MASM] Using prebuilt RawrXD_Camellia256.obj (genesis)")
    else()
        set(ASM_CAMELLIA256_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_Camellia256.obj")
        add_custom_command(
            OUTPUT ${ASM_CAMELLIA256_OBJ}
            COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /I "${CMAKE_CURRENT_SOURCE_DIR}/src/asm" /Fo${ASM_CAMELLIA256_OBJ} ${ASM_CAMELLIA256_SRC}
            DEPENDS ${ASM_CAMELLIA256_SRC}
            COMMENT "[MASM] Assembling RawrXD_Camellia256.asm (Camellia-256 CTR Encrypt, 24-Round Feistel, HWID Key)"
            VERBATIM
        )
    endif()

    # Phase Camellia-Auth: Authenticated Encrypt/Decrypt (Encrypt-then-MAC, HMAC-SHA256, RCM2 Format)
    set(ASM_CAMELLIA256_AUTH_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_Camellia256_Auth.asm")
    set(_ASM_CAMELLIA256_AUTH_PREBUILT1 "${RAWRXD_PREBUILT_OBJ_DIR}/asm_RawrXD_Camellia256_Auth.obj")
    set(_ASM_CAMELLIA256_AUTH_PREBUILT2 "${RAWRXD_PREBUILT_GENESIS_DIR}/RawrXD_Camellia256_Auth.obj")
    if(EXISTS "${_ASM_CAMELLIA256_AUTH_PREBUILT1}")
        set(ASM_CAMELLIA256_AUTH_OBJ "${_ASM_CAMELLIA256_AUTH_PREBUILT1}")
        message(STATUS "[MASM] Using prebuilt RawrXD_Camellia256_Auth.obj")
    elseif(EXISTS "${_ASM_CAMELLIA256_AUTH_PREBUILT2}")
        set(ASM_CAMELLIA256_AUTH_OBJ "${_ASM_CAMELLIA256_AUTH_PREBUILT2}")
        message(STATUS "[MASM] Using prebuilt RawrXD_Camellia256_Auth.obj (genesis)")
    else()
        set(ASM_CAMELLIA256_AUTH_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_Camellia256_Auth.obj")
        add_custom_command(
            OUTPUT ${ASM_CAMELLIA256_AUTH_OBJ}
            COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /I "${CMAKE_CURRENT_SOURCE_DIR}/src/asm" /Fo${ASM_CAMELLIA256_AUTH_OBJ} ${ASM_CAMELLIA256_AUTH_SRC}
            DEPENDS ${ASM_CAMELLIA256_AUTH_SRC}
            COMMENT "[MASM] Assembling RawrXD_Camellia256_Auth.asm (Encrypt-then-MAC, HMAC-SHA256, RCM2 Format)"
            VERBATIM
        )
    endif()

    # Phase KQ: Q4_K Bare-Metal Dequantization Kernel (AVX2 + AVX-512, CPUID Dispatch)
    set(ASM_KQUANT_KERNEL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_KQuant_Kernel.asm")
    set(_ASM_KQUANT_KERNEL_PREBUILT "${RAWRXD_PREBUILT_GENESIS_DIR}/RawrXD_KQuant_Kernel.obj")
    if(EXISTS "${_ASM_KQUANT_KERNEL_PREBUILT}")
        set(ASM_KQUANT_KERNEL_OBJ "${_ASM_KQUANT_KERNEL_PREBUILT}")
        message(STATUS "[MASM] Using prebuilt RawrXD_KQuant_Kernel.obj")
    else()
        set(ASM_KQUANT_KERNEL_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_KQuant_Kernel.obj")
        add_custom_command(
            OUTPUT ${ASM_KQUANT_KERNEL_OBJ}
            COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /I "${CMAKE_CURRENT_SOURCE_DIR}/src/asm" /Fo${ASM_KQUANT_KERNEL_OBJ} ${ASM_KQUANT_KERNEL_SRC}
            DEPENDS ${ASM_KQUANT_KERNEL_SRC}
            COMMENT "[MASM] Assembling RawrXD_KQuant_Kernel.asm (Q4_K AVX2+AVX-512 Dequant, CPUID Dispatch)"
            VERBATIM
        )
    endif()

    # Phase WD: Agentic Watchdog — .text Section Integrity Verification (HMAC-SHA256)
    set(ASM_WATCHDOG_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_Watchdog.asm")
    set(_ASM_WATCHDOG_PREBUILT "${RAWRXD_PREBUILT_GENESIS_DIR}/RawrXD_Watchdog.obj")
    if(EXISTS "${_ASM_WATCHDOG_PREBUILT}")
        set(ASM_WATCHDOG_OBJ "${_ASM_WATCHDOG_PREBUILT}")
        message(STATUS "[MASM] Using prebuilt RawrXD_Watchdog.obj")
    else()
        set(ASM_WATCHDOG_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_Watchdog.obj")
        add_custom_command(
            OUTPUT ${ASM_WATCHDOG_OBJ}
            COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /I "${CMAKE_CURRENT_SOURCE_DIR}/src/asm" /Fo${ASM_WATCHDOG_OBJ} ${ASM_WATCHDOG_SRC}
            DEPENDS ${ASM_WATCHDOG_SRC}
            COMMENT "[MASM] Assembling RawrXD_Watchdog.asm (.text HMAC-SHA256 Integrity, PE Parser, Tamper Detect)"
            VERBATIM
        )
    endif()

    # Phase E: SelfHost Engine — Recursive Self-Compilation
    set(ASM_SELFHOST_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_SelfHost_Engine.asm")
    set(_ASM_SELFHOST_PREBUILT1 "${RAWRXD_PREBUILT_SRCASM_DIR}/RawrXD_SelfHost_Engine.obj")
    set(_ASM_SELFHOST_PREBUILT2 "${RAWRXD_PREBUILT_GENESIS_DIR}/RawrXD_SelfHost_Engine.obj")
    if(EXISTS "${_ASM_SELFHOST_PREBUILT1}")
        set(ASM_SELFHOST_OBJ "${_ASM_SELFHOST_PREBUILT1}")
        message(STATUS "[MASM] Using prebuilt RawrXD_SelfHost_Engine.obj")
    elseif(EXISTS "${_ASM_SELFHOST_PREBUILT2}")
        set(ASM_SELFHOST_OBJ "${_ASM_SELFHOST_PREBUILT2}")
        message(STATUS "[MASM] Using prebuilt RawrXD_SelfHost_Engine.obj (genesis)")
    else()
        set(ASM_SELFHOST_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_SelfHost_Engine.obj")
        add_custom_command(
            OUTPUT ${ASM_SELFHOST_OBJ}
            COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /I "${CMAKE_CURRENT_SOURCE_DIR}/src/asm" /Fo${ASM_SELFHOST_OBJ} ${ASM_SELFHOST_SRC}
            DEPENDS ${ASM_SELFHOST_SRC}
            COMMENT "[MASM] Assembling RawrXD_SelfHost_Engine.asm (Self-Compile, uASM, Atomic Swap)"
            VERBATIM
        )
    endif()

    # Phase F: Hardware Synthesizer — FPGA/ASIC Acceleration
    set(ASM_HWSYNTH_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_HardwareSynthesizer.asm")
    set(_ASM_HWSYNTH_PREBUILT "${RAWRXD_PREBUILT_GENESIS_DIR}/RawrXD_HardwareSynthesizer.obj")
    if(EXISTS "${_ASM_HWSYNTH_PREBUILT}")
        set(ASM_HWSYNTH_OBJ "${_ASM_HWSYNTH_PREBUILT}")
        message(STATUS "[MASM] Using prebuilt RawrXD_HardwareSynthesizer.obj")
    else()
        set(ASM_HWSYNTH_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_HardwareSynthesizer.obj")
        add_custom_command(
            OUTPUT ${ASM_HWSYNTH_OBJ}
            COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /I "${CMAKE_CURRENT_SOURCE_DIR}/src/asm" /Fo${ASM_HWSYNTH_OBJ} ${ASM_HWSYNTH_SRC}
            DEPENDS ${ASM_HWSYNTH_SRC}
            COMMENT "[MASM] Assembling RawrXD_HardwareSynthesizer.asm (Dataflow, GEMM Spec, Verilog Gen)"
            VERBATIM
        )
    endif()

    # Phase G: Mesh Brain — Distributed Consciousness P2P Mesh
    set(ASM_MESHBRAIN_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_MeshBrain.asm")
    set(_ASM_MESHBRAIN_PREBUILT "${RAWRXD_PREBUILT_GENESIS_DIR}/RawrXD_MeshBrain.obj")
    if(EXISTS "${_ASM_MESHBRAIN_PREBUILT}")
        set(ASM_MESHBRAIN_OBJ "${_ASM_MESHBRAIN_PREBUILT}")
        message(STATUS "[MASM] Using prebuilt RawrXD_MeshBrain.obj")
    else()
        set(ASM_MESHBRAIN_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_MeshBrain.obj")
        add_custom_command(
            OUTPUT ${ASM_MESHBRAIN_OBJ}
            COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /I "${CMAKE_CURRENT_SOURCE_DIR}/src/asm" /Fo${ASM_MESHBRAIN_OBJ} ${ASM_MESHBRAIN_SRC}
            DEPENDS ${ASM_MESHBRAIN_SRC}
            COMMENT "[MASM] Assembling RawrXD_MeshBrain.asm (CRDT, ZKP, DHT, FedAvg, Gossip)"
            VERBATIM
        )
    endif()

    # Phase H: Speciator Engine — Metamorphic Evolution
    set(ASM_SPECIATOR_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_Speciator.asm")
    set(_ASM_SPECIATOR_PREBUILT "${RAWRXD_PREBUILT_GENESIS_DIR}/RawrXD_Speciator.obj")
    if(EXISTS "${_ASM_SPECIATOR_PREBUILT}")
        set(ASM_SPECIATOR_OBJ "${_ASM_SPECIATOR_PREBUILT}")
        message(STATUS "[MASM] Using prebuilt RawrXD_Speciator.obj")
    else()
        set(ASM_SPECIATOR_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_Speciator.obj")
        add_custom_command(
            OUTPUT ${ASM_SPECIATOR_OBJ}
            COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /I "${CMAKE_CURRENT_SOURCE_DIR}/src/asm" /Fo${ASM_SPECIATOR_OBJ} ${ASM_SPECIATOR_SRC}
            DEPENDS ${ASM_SPECIATOR_SRC}
            COMMENT "[MASM] Assembling RawrXD_Speciator.asm (Genome, Crossover, Mutation, Speciation)"
            VERBATIM
        )
    endif()

    # Phase I: Neural Bridge — Direct Cortex Interface
    set(ASM_NEURALBRIDGE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_NeuralBridge.asm")
    set(_ASM_NEURALBRIDGE_PREBUILT "${RAWRXD_PREBUILT_GENESIS_DIR}/RawrXD_NeuralBridge.obj")
    if(EXISTS "${_ASM_NEURALBRIDGE_PREBUILT}")
        set(ASM_NEURALBRIDGE_OBJ "${_ASM_NEURALBRIDGE_PREBUILT}")
        message(STATUS "[MASM] Using prebuilt RawrXD_NeuralBridge.obj")
    else()
        set(ASM_NEURALBRIDGE_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_NeuralBridge.obj")
        add_custom_command(
            OUTPUT ${ASM_NEURALBRIDGE_OBJ}
            COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /I "${CMAKE_CURRENT_SOURCE_DIR}/src/asm" /Fo${ASM_NEURALBRIDGE_OBJ} ${ASM_NEURALBRIDGE_SRC}
            DEPENDS ${ASM_NEURALBRIDGE_SRC}
            COMMENT "[MASM] Assembling RawrXD_NeuralBridge.asm (EEG FFT, Intent Classify, Phosphene HUD)"
            VERBATIM
        )
    endif()

    # Phase Ω: Omega Orchestrator — Autonomous Development Pipeline
    set(ASM_OMEGA_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_OmegaOrchestrator.asm")
    set(_ASM_OMEGA_PREBUILT "${RAWRXD_PREBUILT_GENESIS_DIR}/RawrXD_OmegaOrchestrator.obj")
    if(EXISTS "${_ASM_OMEGA_PREBUILT}")
        set(ASM_OMEGA_OBJ "${_ASM_OMEGA_PREBUILT}")
        message(STATUS "[MASM] Using prebuilt RawrXD_OmegaOrchestrator.obj")
    else()
        set(ASM_OMEGA_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_OmegaOrchestrator.obj")
        add_custom_command(
            OUTPUT ${ASM_OMEGA_OBJ}
            COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /I "${CMAKE_CURRENT_SOURCE_DIR}/src/asm" /Fo${ASM_OMEGA_OBJ} ${ASM_OMEGA_SRC}
            DEPENDS ${ASM_OMEGA_SRC}
            COMMENT "[MASM] Assembling RawrXD_OmegaOrchestrator.asm (Task DAG, Agents, Pipeline, Self-Improve)"
            VERBATIM
        )
    endif()

    # Phase ∞: PerfCounters — RDTSC Per-Kernel Cycle Histogram
    set(ASM_PERFCOUNTERS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_PerfCounters.asm")
    set(_ASM_PERFCOUNTERS_PREBUILT1 "${RAWRXD_PREBUILT_OBJ_DIR}/asm_RawrXD_PerfCounters.obj")
    set(_ASM_PERFCOUNTERS_PREBUILT2 "${RAWRXD_PREBUILT_GENESIS_DIR}/RawrXD_PerfCounters.obj")
    if(EXISTS "${_ASM_PERFCOUNTERS_PREBUILT1}")
        set(ASM_PERFCOUNTERS_OBJ "${_ASM_PERFCOUNTERS_PREBUILT1}")
        message(STATUS "[MASM] Using prebuilt RawrXD_PerfCounters.obj")
    elseif(EXISTS "${_ASM_PERFCOUNTERS_PREBUILT2}")
        set(ASM_PERFCOUNTERS_OBJ "${_ASM_PERFCOUNTERS_PREBUILT2}")
        message(STATUS "[MASM] Using prebuilt RawrXD_PerfCounters.obj (genesis)")
    else()
        set(ASM_PERFCOUNTERS_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_PerfCounters.obj")
        add_custom_command(
            OUTPUT ${ASM_PERFCOUNTERS_OBJ}
            COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /I "${CMAKE_CURRENT_SOURCE_DIR}/src/asm" /Fo${ASM_PERFCOUNTERS_OBJ} ${ASM_PERFCOUNTERS_SRC}
            DEPENDS ${ASM_PERFCOUNTERS_SRC}
            COMMENT "[MASM] Assembling RawrXD_PerfCounters.asm (RDTSC Cycle Histograms, 64 Slots)"
            VERBATIM
        )
    endif()

    # Prebuilt-only MASM modules: these .asm sources are intentionally excluded from
    # ASM_KERNEL_SOURCES due to known ml64 incompatibilities in checked-in scaffolds.
    # For IDE/link correctness, prefer the vetted prebuilt .obj blobs when present.
    #
    # Phase 14.2: Hotpatch kernel (trampolines, shadow buffers, I-cache flush)
    set(ASM_HOTPATCH_KERNEL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_Hotpatch_Kernel.asm")
    set(_ASM_HOTPATCH_KERNEL_PREBUILT1 "${RAWRXD_PREBUILT_OBJ_DIR}/asm_RawrXD_Hotpatch_Kernel.obj")
    set(_ASM_HOTPATCH_KERNEL_PREBUILT2 "${RAWRXD_PREBUILT_GENESIS_DIR}/RawrXD_Hotpatch_Kernel.obj")
    if(EXISTS "${_ASM_HOTPATCH_KERNEL_PREBUILT1}")
        set(ASM_HOTPATCH_KERNEL_OBJ "${_ASM_HOTPATCH_KERNEL_PREBUILT1}")
        message(STATUS "[MASM] Using prebuilt RawrXD_Hotpatch_Kernel.obj")
    elseif(EXISTS "${_ASM_HOTPATCH_KERNEL_PREBUILT2}")
        set(ASM_HOTPATCH_KERNEL_OBJ "${_ASM_HOTPATCH_KERNEL_PREBUILT2}")
        message(STATUS "[MASM] Using prebuilt RawrXD_Hotpatch_Kernel.obj (genesis)")
    else()
        set(ASM_HOTPATCH_KERNEL_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_Hotpatch_Kernel.obj")
        add_custom_command(
            OUTPUT ${ASM_HOTPATCH_KERNEL_OBJ}
            COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /I "${CMAKE_CURRENT_SOURCE_DIR}/src/asm" /Fo${ASM_HOTPATCH_KERNEL_OBJ} ${ASM_HOTPATCH_KERNEL_SRC}
            DEPENDS ${ASM_HOTPATCH_KERNEL_SRC}
            COMMENT "[MASM] Assembling RawrXD_Hotpatch_Kernel.asm"
            VERBATIM
        )
    endif()

    # Phase 14.3: Snapshot kernel (capture/restore/verify/discard)
    set(ASM_SNAPSHOT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_Snapshot.asm")
    set(_ASM_SNAPSHOT_PREBUILT1 "${RAWRXD_PREBUILT_OBJ_DIR}/asm_RawrXD_Snapshot.obj")
    set(_ASM_SNAPSHOT_PREBUILT2 "${RAWRXD_PREBUILT_GENESIS_DIR}/RawrXD_Snapshot.obj")
    if(EXISTS "${_ASM_SNAPSHOT_PREBUILT1}")
        set(ASM_SNAPSHOT_OBJ "${_ASM_SNAPSHOT_PREBUILT1}")
        message(STATUS "[MASM] Using prebuilt RawrXD_Snapshot.obj")
    elseif(EXISTS "${_ASM_SNAPSHOT_PREBUILT2}")
        set(ASM_SNAPSHOT_OBJ "${_ASM_SNAPSHOT_PREBUILT2}")
        message(STATUS "[MASM] Using prebuilt RawrXD_Snapshot.obj (genesis)")
    else()
        set(ASM_SNAPSHOT_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_Snapshot.obj")
        add_custom_command(
            OUTPUT ${ASM_SNAPSHOT_OBJ}
            COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /I "${CMAKE_CURRENT_SOURCE_DIR}/src/asm" /Fo${ASM_SNAPSHOT_OBJ} ${ASM_SNAPSHOT_SRC}
            DEPENDS ${ASM_SNAPSHOT_SRC}
            COMMENT "[MASM] Assembling RawrXD_Snapshot.asm"
            VERBATIM
        )
    endif()

    # Phase 46: Pyre compute kernels (sgemm/gemv/norm/rope)
    set(ASM_PYRE_COMPUTE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_Pyre_Compute.asm")
    set(_ASM_PYRE_COMPUTE_PREBUILT1 "${RAWRXD_PREBUILT_OBJ_DIR}/asm_RawrXD_Pyre_Compute.obj")
    set(_ASM_PYRE_COMPUTE_PREBUILT2 "${RAWRXD_PREBUILT_GENESIS_DIR}/RawrXD_Pyre_Compute.obj")
    if(EXISTS "${_ASM_PYRE_COMPUTE_PREBUILT1}")
        set(ASM_PYRE_COMPUTE_OBJ "${_ASM_PYRE_COMPUTE_PREBUILT1}")
        message(STATUS "[MASM] Using prebuilt RawrXD_Pyre_Compute.obj")
    elseif(EXISTS "${_ASM_PYRE_COMPUTE_PREBUILT2}")
        set(ASM_PYRE_COMPUTE_OBJ "${_ASM_PYRE_COMPUTE_PREBUILT2}")
        message(STATUS "[MASM] Using prebuilt RawrXD_Pyre_Compute.obj (genesis)")
    else()
        set(ASM_PYRE_COMPUTE_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_Pyre_Compute.obj")
        add_custom_command(
            OUTPUT ${ASM_PYRE_COMPUTE_OBJ}
            COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /I "${CMAKE_CURRENT_SOURCE_DIR}/src/asm" /Fo${ASM_PYRE_COMPUTE_OBJ} ${ASM_PYRE_COMPUTE_SRC}
            DEPENDS ${ASM_PYRE_COMPUTE_SRC}
            COMMENT "[MASM] Assembling RawrXD_Pyre_Compute.asm"
            VERBATIM
        )
    endif()

    # Phase 49: Copilot gap closer (VecDB/Composer/CRDT/Git context)
    set(ASM_COPILOT_GAPCLOSER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_CopilotGapCloser.asm")
    set(_ASM_COPILOT_GAPCLOSER_PREBUILT1 "${RAWRXD_PREBUILT_OBJ_DIR}/asm_RawrXD_CopilotGapCloser.obj")
    set(_ASM_COPILOT_GAPCLOSER_PREBUILT2 "${RAWRXD_PREBUILT_GENESIS_DIR}/RawrXD_CopilotGapCloser.obj")
    if(EXISTS "${_ASM_COPILOT_GAPCLOSER_PREBUILT1}")
        set(ASM_COPILOT_GAPCLOSER_OBJ "${_ASM_COPILOT_GAPCLOSER_PREBUILT1}")
        message(STATUS "[MASM] Using prebuilt RawrXD_CopilotGapCloser.obj")
    elseif(EXISTS "${_ASM_COPILOT_GAPCLOSER_PREBUILT2}")
        set(ASM_COPILOT_GAPCLOSER_OBJ "${_ASM_COPILOT_GAPCLOSER_PREBUILT2}")
        message(STATUS "[MASM] Using prebuilt RawrXD_CopilotGapCloser.obj (genesis)")
    else()
        set(ASM_COPILOT_GAPCLOSER_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_CopilotGapCloser.obj")
        add_custom_command(
            OUTPUT ${ASM_COPILOT_GAPCLOSER_OBJ}
            COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /I "${CMAKE_CURRENT_SOURCE_DIR}/src/asm" /Fo${ASM_COPILOT_GAPCLOSER_OBJ} ${ASM_COPILOT_GAPCLOSER_SRC}
            DEPENDS ${ASM_COPILOT_GAPCLOSER_SRC}
            COMMENT "[MASM] Assembling RawrXD_CopilotGapCloser.asm"
            VERBATIM
        )
    endif()

    # NativeSpeed kernels (SIMD fast-paths)
    set(ASM_NATIVE_SPEED_KERNELS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/native_speed_kernels.asm")
    set(_ASM_NATIVE_SPEED_KERNELS_PREBUILT "${RAWRXD_PREBUILT_GENESIS_DIR}/native_speed_kernels.obj")
    if(EXISTS "${_ASM_NATIVE_SPEED_KERNELS_PREBUILT}")
        set(ASM_NATIVE_SPEED_KERNELS_OBJ "${_ASM_NATIVE_SPEED_KERNELS_PREBUILT}")
        message(STATUS "[MASM] Using prebuilt native_speed_kernels.obj (genesis)")
    else()
        set(ASM_NATIVE_SPEED_KERNELS_OBJ "${CMAKE_CURRENT_BINARY_DIR}/native_speed_kernels.obj")
        add_custom_command(
            OUTPUT ${ASM_NATIVE_SPEED_KERNELS_OBJ}
            COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /I "${CMAKE_CURRENT_SOURCE_DIR}/src/asm" /Fo${ASM_NATIVE_SPEED_KERNELS_OBJ} ${ASM_NATIVE_SPEED_KERNELS_SRC}
            DEPENDS ${ASM_NATIVE_SPEED_KERNELS_SRC}
            COMMENT "[MASM] Assembling native_speed_kernels.asm"
            VERBATIM
        )
    endif()

    # Brutal gzip deflate — stored DEFLATE blocks + gzip wrap (deflate_brutal_masm)
    set(ASM_DEFLATE_BRUTAL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/gpu_masm/deflate_brutal_masm.asm")
    set(ASM_DEFLATE_BRUTAL_OBJ "${CMAKE_CURRENT_BINARY_DIR}/deflate_brutal_masm.obj")
    add_custom_command(
        OUTPUT ${ASM_DEFLATE_BRUTAL_OBJ}
        COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /I "${CMAKE_CURRENT_SOURCE_DIR}/src/asm" /I "${CMAKE_CURRENT_SOURCE_DIR}/include" /Fo${ASM_DEFLATE_BRUTAL_OBJ} ${ASM_DEFLATE_BRUTAL_SRC}
        DEPENDS ${ASM_DEFLATE_BRUTAL_SRC}
        COMMENT "[MASM] Assembling deflate_brutal_masm.asm"
        VERBATIM
    )

    # Collect all MASM objects
    set(MASM_OBJECTS
        ${ASM_REQUANTIZE_OBJ}
        ${ASM_GPU_REQUANT_OBJ}
        ${ASM_SWARM_TENSOR_OBJ}
        ${ASM_MONACOCORE_OBJ}
        ${ASM_PDBKERNEL_OBJ}
        ${ASM_ROUTERBRIDGE_OBJ}
        ${ASM_GSIHASH_OBJ}
        ${ASM_REFPROVIDER_OBJ}
        ${ASM_STUBDETECTOR_OBJ}
        ${ASM_COT_ENGINE_OBJ}
        ${ASM_COT_DLL_ENTRY_OBJ}
        ${ASM_COT_PHASE39_OBJ}
        ${ASM_LSP_SYMINDEX_OBJ}
        ${ASM_RAWRCODEX_OBJ}
        ${ASM_DEBUG_ENGINE_OBJ}
        ${ASM_QUADBUFFER_OBJ}
        ${ASM_SWARM_NETWORK_OBJ}
        ${ASM_ENTERPRISE_LIC_OBJ}
        ${ASM_LICENSE_SHIELD_OBJ}
        ${ASM_VULKANBRIDGE_OBJ}
        ${ASM_SOCKETRESP_OBJ}
        ${ASM_TRIDENTBEACON_OBJ}
        ${ASM_TRIDENTGPU_OBJ}
        ${ASM_AGENTIC_ORCH_OBJ}
        ${ASM_GGUF_VKLOAD_OBJ}
        ${ASM_LSP_BRIDGE_OBJ}
        ${ASM_STREAM_QBUF_OBJ}
        ${ASM_SPENGINE_OBJ}
        ${ASM_CAMELLIA256_OBJ}
        ${ASM_CAMELLIA256_AUTH_OBJ}
        ${ASM_KQUANT_KERNEL_OBJ}
        ${ASM_WATCHDOG_OBJ}
        ${ASM_SELFHOST_OBJ}
        ${ASM_HWSYNTH_OBJ}
        ${ASM_MESHBRAIN_OBJ}
        ${ASM_SPECIATOR_OBJ}
        ${ASM_NEURALBRIDGE_OBJ}
        ${ASM_OMEGA_OBJ}
        ${ASM_PERFCOUNTERS_OBJ}
        ${ASM_HOTPATCH_KERNEL_OBJ}
        ${ASM_SNAPSHOT_OBJ}
        ${ASM_PYRE_COMPUTE_OBJ}
        ${ASM_COPILOT_GAPCLOSER_OBJ}
        ${ASM_NATIVE_SPEED_KERNELS_OBJ}
        ${ASM_DEFLATE_BRUTAL_OBJ}
        ${ASM_RDNA3_KERNELS_OBJ}
    )

    # Link MASM objects into RawrEngine (disabled in Lane B headless minimal).
    if(NOT RAWR_ENGINE_LANE_B)
        target_link_libraries(RawrEngine PRIVATE ${MASM_OBJECTS})
        add_dependencies(RawrEngine masm_kernels)
    endif()

    # Link MASM objects into RawrXD_Gold
    target_link_libraries(RawrXD_Gold PRIVATE ${MASM_OBJECTS})

    # Ensure MASM objects are built before linking
    add_custom_target(masm_kernels DEPENDS ${MASM_OBJECTS})
    add_dependencies(RawrXD_Gold masm_kernels)

    # =========================================================================
    # NanoQuant Engine — Sub-1-Bit Quantization Static Library
    # MASM64 kernel (ADMM, AVX2/AVX-512) + C++ bridge + GGML type registration
    # Disabled by default — ASM kernel requires ml64 port fixes (A2008/A2083).
    # Enable with: cmake -DRAWR_ENABLE_NANOQUANT=ON
    # =========================================================================
    option(RAWR_ENABLE_NANOQUANT "Enable NanoQuant sub-1-bit quantization engine (requires working NanoQuant ASM)" OFF)
    if(RAWR_ENABLE_NANOQUANT)
    set(ASM_NANOQUANT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_NanoQuant_Engine.asm")
    set(ASM_NANOQUANT_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_NanoQuant_Engine.obj")
    add_custom_command(
        OUTPUT ${ASM_NANOQUANT_OBJ}
        COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /Fo${ASM_NANOQUANT_OBJ} ${ASM_NANOQUANT_SRC}
        DEPENDS ${ASM_NANOQUANT_SRC}
        COMMENT "[MASM] Assembling RawrXD_NanoQuant_Engine.asm (ADMM, AVX2/512, NQ_1/NQ_R4)"
        VERBATIM
    )

    add_library(RawrXD_NanoQuant STATIC
        src/ggml/ggml_nanoquant.cpp
        src/agentic/agent_tool_quantize.cpp
    )
    # Set MSVC runtime library to match RawrEngine (static CRT)
    set_property(TARGET RawrXD_NanoQuant PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_include_directories(RawrXD_NanoQuant PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(RawrXD_NanoQuant PRIVATE ${ASM_NANOQUANT_OBJ})
    add_custom_target(nanoquant_asm DEPENDS ${ASM_NANOQUANT_OBJ})
    add_dependencies(RawrXD_NanoQuant nanoquant_asm)

    # Link NanoQuant into main targets
    target_link_libraries(RawrEngine PRIVATE RawrXD_NanoQuant)
    target_compile_definitions(RawrEngine PRIVATE RAWR_HAS_NANOQUANT=1)
    target_link_libraries(RawrXD_Gold PRIVATE RawrXD_NanoQuant)
    target_compile_definitions(RawrXD_Gold PRIVATE RAWR_HAS_NANOQUANT=1)

    message(STATUS "[NanoQuant] Sub-1-bit quantization engine enabled (NQ_1 + NQ_R4)")
    else()
    message(STATUS "[NanoQuant] DISABLED — enable with -DRAWR_ENABLE_NANOQUANT=ON")
    endif()
endif()

# =============================================================================
# Vulkan SDK (optional — GPU compute acceleration)
# =============================================================================
# Phase 4A: Apply Vulkan detection fix for Windows
include(${CMAKE_SOURCE_DIR}/cmake/RawrXD-Vulkan-Fix.cmake OPTIONAL)

find_package(Vulkan QUIET)
if(Vulkan_FOUND)
    message(STATUS "[Vulkan] Found SDK: ${Vulkan_INCLUDE_DIRS}")
    target_include_directories(RawrEngine PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(RawrEngine PRIVATE Vulkan::Vulkan)
    target_compile_definitions(RawrEngine PRIVATE RAWR_HAS_VULKAN=1)
    # Gold target
    target_include_directories(RawrXD_Gold PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(RawrXD_Gold PRIVATE Vulkan::Vulkan)
    target_compile_definitions(RawrXD_Gold PRIVATE RAWR_HAS_VULKAN=1)
else()
    # Try manual path — D: first, then C:
    file(GLOB _VK_SDK_DIRS "D:/VulkanSDK/1.*")
    if(NOT _VK_SDK_DIRS)
        file(GLOB _VK_SDK_DIRS "C:/VulkanSDK/1.*")
    endif()
    if(_VK_SDK_DIRS)
        list(SORT _VK_SDK_DIRS)
        list(GET _VK_SDK_DIRS -1 _VK_SDK_DIR)
        message(STATUS "[Vulkan] Manual SDK: ${_VK_SDK_DIR}")
        target_include_directories(RawrEngine PRIVATE "${_VK_SDK_DIR}/Include")
        target_link_directories(RawrEngine PRIVATE "${_VK_SDK_DIR}/Lib")
        target_link_libraries(RawrEngine PRIVATE vulkan-1)
        target_compile_definitions(RawrEngine PRIVATE RAWR_HAS_VULKAN=1)
        # Gold target
        target_include_directories(RawrXD_Gold PRIVATE "${_VK_SDK_DIR}/Include")
        target_link_directories(RawrXD_Gold PRIVATE "${_VK_SDK_DIR}/Lib")
        target_link_libraries(RawrXD_Gold PRIVATE vulkan-1)
        target_compile_definitions(RawrXD_Gold PRIVATE RAWR_HAS_VULKAN=1)
    else()
        message(STATUS "[Vulkan] SDK not found — GPU compute uses CPU fallback")
        target_compile_definitions(RawrEngine PRIVATE RAWR_HAS_VULKAN=0)
        target_compile_definitions(RawrXD_Gold PRIVATE RAWR_HAS_VULKAN=0)
    endif()
endif()

# RawrCodex.asm uses ADDR32 relocations — require /LARGEADDRESSAWARE:NO
if(MSVC)
    target_link_options(RawrEngine PRIVATE /LARGEADDRESSAWARE:NO)
    # Do NOT set /ENTRY - let CMake/MSVC use default entry point
    # /ENTRY bypasses CRT initialization which causes _purecall unresolved
    if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
        set(_RAWR_ENGINE_MAP_PATH "${CMAKE_BINARY_DIR}/bin/RawrEngine.map")
        set(_RAWR_ENGINE_MAP_AUDIT_PATH "${CMAKE_BINARY_DIR}/RawrEngine.map")
        target_link_options(RawrEngine PRIVATE
            "/MAP:${_RAWR_ENGINE_MAP_PATH}"
            /MAPINFO:EXPORTS
        )
        add_custom_command(TARGET RawrEngine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_RAWR_ENGINE_MAP_PATH}" "${_RAWR_ENGINE_MAP_AUDIT_PATH}"
            VERBATIM
        )
    endif()
endif()

if(NOT MSVC)
    target_compile_options(RawrEngine PRIVATE -march=native -O3 -w -mavx2 -mfma -fopenmp)
endif()

# =============================================================================
# Phase 6: Standalone Inference Engine Executable (T4)
# =============================================================================
# Produces RawrXD-InferenceEngine.exe — loads GGUF and emits tokens with no IDE.
# This is the self-hosting blocker: inference must run independently.
# T4 integration: the AutonomousRecoveryOrchestrator invokes this for reasoning.
# =============================================================================
set(INFERENCE_ENGINE_SOURCES
    src/inference/inference_standalone_main.cpp
    src/inference/ultra_fast_inference.cpp
    src/inference/polymorphic_loader.cpp
    src/inference/ollama_blob_parser.cpp
    src/cpu_inference_engine.cpp
    src/rawrxd_inference.cpp
    src/rawrxd_model_loader.cpp
    src/rawrxd_transformer.cpp
    src/rawrxd_transformer_forwardbatch.cpp
    src/rawrxd_tokenizer.cpp
    src/rawrxd_sampler.cpp
    src/gguf_loader.cpp
    src/gguf_vocab_resolver.cpp
    src/streaming_gguf_loader.cpp
    src/compression_interface.cpp
    src/memory_core.cpp
    src/license_enforcement.cpp
    src/feature_flags_runtime.cpp
    src/core/enterprise_licensev2_impl.cpp
    src/core/license_anti_tampering.cpp
    src/tools/license_validator_manifest.cpp
    src/core/ignite_800b.cpp
    src/core/ConfigurationValidator.cpp
    src/engine/gguf_core.cpp
    src/engine/inference_kernels.cpp
    src/engine/transformer.cpp
    src/engine/bpe_tokenizer.cpp
    src/engine/sampler.cpp
    src/engine/rawr_engine.cpp
    src/engine/core_generator.cpp
    src/engine/pyre_compute.cpp
    src/engine/sovereign_engines.cpp
    src/core/layer_offload_manager.cpp
    src/core/engine_registry.cpp
    src/utils/Diagnostics.cpp
    src/inference/TitanLoaderDiagnostics.cpp
    src/inference/MemoryPressureGuard.cpp
    src/agentic/ErrorRecoveryManager.cpp
    src/inference/PerformanceMonitor.cpp
    src/inference/NegativeSpaceProfiler.cpp
    src/core/swarm_scheduler.cpp
    src/core/gguf_swarm_plan_builder.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp
    src/runtime/TensorExecutionRouter.cpp
    src/runtime/Deep2ExecutionTelemetry.cpp
    src/backend/VulkanGemmDispatcher.cpp
    src/runtime/memory/TransferScheduler.cpp
    src/runtime/memory/PredictiveMemoryManager.cpp
    src/runtime/memory/PlacementPolicy.cpp
    src/runtime/memory/ResidencyTracker.cpp
    src/runtime/memory/WorkingSetPredictor.cpp
    src/runtime/memory/TensorPlacementManager.cpp
    src/runtime/memory/CapacityManager.cpp
    src/runtime/memory/RouterPredictiveMemoryBridge.cpp
    src/runtime/GenerationBouncer.cpp
    src/runtime/StreamRouterAdapter.cpp
    src/runtime/Sovereign_KernelBridge_CPP.cpp
    src/logging/Logger.cpp
    src/sampling/advanced_sampler.cpp
    src/runtime/RawrRuntime.cpp
    src/runtime/governance/HardwareCapabilityProbe.cpp
    src/runtime/governance/CapabilityTierMatrix.cpp
    src/runtime/governance/HardwareGovernor.cpp
    src/runtime/governance/PressedStateEngine.cpp
    src/runtime/governance/Ensemble58Governor.cpp
    src/runtime/governance/TLSInstructionGate.cpp
    src/runtime/governance/UnifiedTriggerOrchestrator.cpp
    src/deep2/K2TimeLimitedServing.cpp
    src/runtime/elastic/ElasticGGUFIndex.cpp
    src/runtime/elastic/ElasticResidencyManager.cpp
    src/runtime/elastic/ElasticDecoder.cpp
    src/runtime/elastic/ElasticEngine.cpp
    src/runtime/elastic/ElasticArchitectureDetector.cpp
    src/runtime/elastic/VulkanTensorResidencyBackend.cpp
    src/runtime/gguf_tensor_loader.cpp
    tests/b009/b009b_batched_gemm.cpp
    B014/build/b014_profiler.cpp
)
list(REMOVE_DUPLICATES INFERENCE_ENGINE_SOURCES)
# REMOVED: inference_standalone_link_shims.cpp reference - stub file deleted
# if(NOT RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
#     list(APPEND INFERENCE_ENGINE_SOURCES src/inference/inference_standalone_link_shims.cpp)
# endif()
if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
    list(FILTER INFERENCE_ENGINE_SOURCES EXCLUDE REGEX ".*/.*_link_shims?\\.cpp$")
    list(FILTER INFERENCE_ENGINE_SOURCES EXCLUDE REGEX ".*/.*_shims?\\.cpp$")
    list(FILTER INFERENCE_ENGINE_SOURCES EXCLUDE REGEX ".*/.*_fallback[^/]*\\.cpp$")
    list(FILTER INFERENCE_ENGINE_SOURCES EXCLUDE REGEX ".*/stubs\\.cpp$")
    list(FILTER INFERENCE_ENGINE_SOURCES EXCLUDE REGEX ".*/.*_stub\\.cpp$")
    list(FILTER INFERENCE_ENGINE_SOURCES EXCLUDE REGEX ".*/stub_.*\\.cpp$")
    list(FILTER INFERENCE_ENGINE_SOURCES EXCLUDE REGEX ".*/shim_.*\\.cpp$")
    list(FILTER INFERENCE_ENGINE_SOURCES EXCLUDE REGEX ".*/.*_(mock|fake)\\.cpp$")
    list(FILTER INFERENCE_ENGINE_SOURCES EXCLUDE REGEX ".*/.*(mock_|fake_).*\\.cpp$")
    list(FILTER INFERENCE_ENGINE_SOURCES EXCLUDE REGEX ".*/.*_stubs\\.asm$")
    list(FILTER INFERENCE_ENGINE_SOURCES EXCLUDE REGEX ".*/stubs\\.asm$")
    list(FILTER INFERENCE_ENGINE_SOURCES EXCLUDE REGEX ".*/.*_stub\\.asm$")
    list(FILTER INFERENCE_ENGINE_SOURCES EXCLUDE REGEX ".*/stub_.*\\.asm$")
    list(FILTER INFERENCE_ENGINE_SOURCES EXCLUDE REGEX ".*/.*_shims?\\.asm$")
    list(FILTER INFERENCE_ENGINE_SOURCES EXCLUDE REGEX ".*/shim_.*\\.asm$")
    list(FILTER INFERENCE_ENGINE_SOURCES EXCLUDE REGEX ".*/.*_link_shims?\\.asm$")
    list(FILTER INFERENCE_ENGINE_SOURCES EXCLUDE REGEX ".*/.*_fallback[^/]*\\.asm$")
    list(FILTER INFERENCE_ENGINE_SOURCES EXCLUDE REGEX ".*/.*_(mock|fake)\\.asm$")
    list(FILTER INFERENCE_ENGINE_SOURCES EXCLUDE REGEX ".*/.*(mock_|fake_).*\\.asm$")
    list(FILTER INFERENCE_ENGINE_SOURCES EXCLUDE REGEX ".*/link_stubs_.*\\.cpp$")
    list(FILTER INFERENCE_ENGINE_SOURCES EXCLUDE REGEX ".*/link_stubs_.*\\.asm$")
endif()

set(INFERENCE_ENGINE_LIBRARY_SOURCES ${INFERENCE_ENGINE_SOURCES})
list(REMOVE_ITEM INFERENCE_ENGINE_LIBRARY_SOURCES
    src/inference/inference_standalone_main.cpp
)

add_library(InferenceEngine STATIC ${INFERENCE_ENGINE_LIBRARY_SOURCES})
# Set MSVC runtime library to match RawrEngine (static CRT)
set_property(TARGET InferenceEngine PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(RawrXD-InferenceEngine EXCLUDE_FROM_ALL
    src/inference/inference_standalone_main.cpp
)
target_link_libraries(RawrXD-InferenceEngine PRIVATE InferenceEngine)
if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
    set_target_properties(RawrXD-InferenceEngine PROPERTIES
        EXCLUDE_FROM_ALL FALSE
        EXCLUDE_FROM_DEFAULT_BUILD FALSE
    )
endif()

# Unified Generation CLI - uses GenerationBouncer
add_executable(RawrXD-Generate EXCLUDE_FROM_ALL
    src/cli/generate_cli.cpp
    src/runtime/GenerationBouncer.cpp
)
target_link_libraries(RawrXD-Generate PRIVATE
    InferenceEngine
    Threads::Threads
    kernel32
    user32
)
target_include_directories(RawrXD-Generate PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime
    ${CMAKE_CURRENT_SOURCE_DIR}/src/inference
)
set_target_properties(RawrXD-Generate PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME RawrXD-Generate
)
if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
    set_target_properties(RawrXD-Generate PROPERTIES
        EXCLUDE_FROM_ALL FALSE
        EXCLUDE_FROM_DEFAULT_BUILD FALSE
    )
endif()

target_compile_definitions(InferenceEngine PRIVATE
    RAWRXD_STRICT_PRODUCTION_PROFILE=$<IF:$<BOOL:${RAWRXD_PRODUCTION_STRIP_STUB_SOURCES}>,1,0>
    RAWR_STANDALONE_INFERENCE=1
    RAWR_HAS_SOVEREIGN_ENGINES=1
    RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=$<IF:$<BOOL:${RAWRXD_PRODUCTION_STRIP_STUB_SOURCES}>,1,0>
    ${_RAWR_ASM_FEATURE_DEFINITIONS}
)
target_compile_definitions(RawrXD-InferenceEngine PRIVATE
    RAWRXD_STRICT_PRODUCTION_PROFILE=$<IF:$<BOOL:${RAWRXD_PRODUCTION_STRIP_STUB_SOURCES}>,1,0>
    RAWR_STANDALONE_INFERENCE=1
    RAWR_HAS_SOVEREIGN_ENGINES=1
    RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=$<IF:$<BOOL:${RAWRXD_PRODUCTION_STRIP_STUB_SOURCES}>,1,0>
)

target_include_directories(InferenceEngine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/inference
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/codec
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/masm
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/governance
    ${RAWR_VULKAN_INCLUDE}
)

target_link_libraries(InferenceEngine PUBLIC Threads::Threads)

if(WIN32)
    target_link_libraries(InferenceEngine PUBLIC
        shlwapi psapi dbghelp winhttp bcrypt advapi32 crypt32 dxgi
    )
endif()

# Vulkan SDK link for InferenceEngine (symbols referenced by rawrxd_model_loader / vulkan_compute)
if(RAWR_ENABLE_VULKAN AND DEFINED RAWR_VULKAN_LIB)
    target_link_libraries(InferenceEngine PUBLIC "${RAWR_VULKAN_LIB}")
    target_compile_definitions(InferenceEngine PUBLIC RAWR_HAS_VULKAN=1)
endif()

    if(RAWR_HAS_MASM)
        # Link quantization + telemetry kernels into inference engine
        set(INFERENCE_ASM_SOURCES
            src/asm/rawr_globals.asm
            src/asm/rawr_cpu_features.asm
            src/asm/inference_core.asm
            src/asm/inference_kernels.asm
            src/asm/quant_avx2.asm
            src/asm/RawrXD_KQuant_Dequant.asm
            src/asm/RawrXD_KQuant_Kernel.asm
            src/asm/FlashAttention_AVX512.asm
            src/asm/RawrXD_Telemetry_Kernel.asm
            # Pure MASM inference kernels (zero dependencies, no GGML)
            src/masm/rawrxd_math_masm.asm
            src/masm/rawrxd_transformer_masm_fixed.asm
            # Known invalid ml64 scaffold in repository copy; keep C++ pyre_compute.cpp path active.
            # src/asm/RawrXD_Pyre_Compute.asm
        )
    set_source_files_properties(${INFERENCE_ASM_SOURCES} PROPERTIES LANGUAGE ASM_MASM)
    target_sources(InferenceEngine PRIVATE ${INFERENCE_ASM_SOURCES})

    target_link_libraries(InferenceEngine PRIVATE ${ASM_DEFLATE_BRUTAL_OBJ})
    add_dependencies(InferenceEngine masm_kernels)
    add_dependencies(RawrXD-InferenceEngine masm_kernels)
    target_compile_definitions(InferenceEngine PRIVATE HAS_BRUTAL_GZIP_MASM=1)

    # NanoQuant integration (optional)
    if(TARGET RawrXD_NanoQuant)
        target_link_libraries(InferenceEngine PRIVATE RawrXD_NanoQuant)
        target_compile_definitions(InferenceEngine PRIVATE RAWR_HAS_NANOQUANT=1)
    endif()
endif()

if(MSVC)
    target_compile_options(InferenceEngine PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:/EHsc /W0 /arch:AVX512>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CONFIG:Debug>>>:/O2>
    )
else()
    target_compile_options(InferenceEngine PRIVATE -O3 -w -mavx2 -mfma -mavx512f -mavx512bw -mavx512dq -mavx512vl -march=native)
endif()

set_target_properties(RawrXD-InferenceEngine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY bin
    OUTPUT_NAME RawrXD-InferenceEngine
)

# Telemetry_Kernel.asm uses ADDR32 relocations — require /LARGEADDRESSAWARE:NO
if(MSVC)
    target_link_options(RawrXD-InferenceEngine PRIVATE /LARGEADDRESSAWARE:NO)
    if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
        set(_RAWR_INFERENCE_MAP_PATH "${CMAKE_BINARY_DIR}/bin/RawrXD-InferenceEngine.map")
        set(_RAWR_INFERENCE_MAP_AUDIT_PATH "${CMAKE_BINARY_DIR}/RawrXD-InferenceEngine.map")
        target_link_options(RawrXD-InferenceEngine PRIVATE
            "/MAP:${_RAWR_INFERENCE_MAP_PATH}"
            /MAPINFO:EXPORTS
        )
        add_custom_command(TARGET RawrXD-InferenceEngine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_RAWR_INFERENCE_MAP_PATH}" "${_RAWR_INFERENCE_MAP_AUDIT_PATH}"
            VERBATIM
        )
    endif()
endif()

message(STATUS "[Phase 6] Standalone inference engine target: RawrXD-InferenceEngine")

add_executable(rawrxd-monaco-gen
    src/monaco_gen.cpp
    src/engine/react_ide_generator.cpp
)

target_include_directories(rawrxd-monaco-gen PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(rawrxd-monaco-gen PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY bin
    OUTPUT_NAME rawrxd-monaco-gen
)

# Win32 GUI IDE - Zero Bloat Track B (v14.6.0)
# Entry: main_win32.cpp (WinMain) so full IDE createWindow/showWindow/layout and all modules run.
# Previously entry_point.cpp (wWinMain) created a minimal window and could leave it hidden (nCmdShow=SW_HIDE).
if(WIN32 AND RAWRXD_BUILD_WIN32IDE)
    set(WIN32IDE_SOURCES
        src/win32app/main_win32.cpp
        src/win32app/HeadlessIDE.cpp
        src/agent/quantum_agent_orchestrator.cpp
        src/agent/quantum_autonomous_todo_system.cpp
        src/agent/quantum_multi_model_agent_cycling.cpp
        src/agent/quantum_dynamic_time_manager.cpp
        src/agent/quantum_missing_impl.cpp
        src/agent/quantum_production_orchestrator.cpp
        src/ui/RawrXD_MainLoop.cpp
        src/ui/webview2_bridge.cpp
        src/ui/debugger_core.cpp
        src/ui/agentic_bridge.cpp
        src/win32app/RawrXD-Win32IDE.rc
        src/bridge/Win32SwarmBridge.cpp
        src/bridge/SwarmIATRegistration.cpp
        src/bridge/UnifiedModelMetadata.cpp
        src/subagent_core.cpp
        src/agentic/SubAgentManager.cpp
        src/agentic/RobustOllamaParser.cpp
        src/agentic/ErrorRecoveryManager.cpp
        src/gpu/speculative_decoder_v2.cpp
        src/inference/MemoryPressureGuard.cpp
        src/inference/gpu_dispatch_gate.cpp
        src/inference/ai_model_caller_real.cpp
        src/inference/inference_engine_real.cpp
        src/gguf_masm_weight_bridge.cpp
        src/codec/brutal_gzip.cpp
        src/codec/gzip_brutal_inflate.cpp
    )

    # AUTO provider: auto_feature_registry.cpp provides the real handlers
    if(RAWR_SSOT_PROVIDER_UPPER STREQUAL "AUTO")
        list(APPEND WIN32IDE_SOURCES src/core/auto_feature_registry.cpp)
        # ReverseEngineering (NativeDisassembler, BinaryAnalyzer, RECodex, NativeCompiler) — required by auto_feature_registry
        list(APPEND WIN32IDE_SOURCES src/modules/ReverseEngineering.cpp)
    endif()

    # MASM64 UI Dispatcher (MSVC/MASM-only).
    if(RAWR_HAS_MASM)
        set(WEBVIEW2_DISPATCHER src/asm/webview2_masm64_dispatcher.asm)
        set_source_files_properties(${WEBVIEW2_DISPATCHER} PROPERTIES LANGUAGE ASM_MASM)
        list(APPEND WIN32IDE_SOURCES ${WEBVIEW2_DISPATCHER})
    endif()

    # MASM64 ViewState - Enterprise-grade lock-free UI state management
    if(RAWR_HAS_MASM)
        set(VIEWSTATE_MASM src/asm/view_state_x64.asm)
        set_source_files_properties(${VIEWSTATE_MASM} PROPERTIES LANGUAGE ASM_MASM)
        list(APPEND WIN32IDE_SOURCES ${VIEWSTATE_MASM})
    endif()

    # MASM64 Tensor Operations - Pure x64 assembly inference kernels
    if(RAWR_HAS_MASM)
        set(TENSOR_OPS_MASM src/asm/tensor_masm.asm)
        set_source_files_properties(${TENSOR_OPS_MASM} PROPERTIES LANGUAGE ASM_MASM)
        list(APPEND WIN32IDE_SOURCES ${TENSOR_OPS_MASM})
    endif()

    # NOTE:
    # Win32IDE target creation/linking is performed in the canonical block below
    # (single add_executable + full source/link configuration).

    list(APPEND WIN32IDE_SOURCES
        # Core Win32IDE implementation units (required for real GUI command/menu behavior)
        src/win32app/Win32IDE.cpp
        src/win32app/Win32IDE_Core.cpp
        src/win32app/Win32IDE_CoreRuntimeSpine.cpp
        src/win32app/Win32IDE_Commands.cpp
        src/win32app/Win32IDE_VSCodeUI.cpp
        src/win32app/Win32IDE_Window.cpp
        src/win32app/Win32IDE_Accelerators.cpp
        src/win32app/IV280Bridge.cpp
        src/win32app/IMultiFileSearchWidget.cpp
        src/win32app/CICDSettings.cpp
        src/win32app/BenchmarkMenu.cpp
            src/win32app/WindowManager.cpp
            src/win32app/EditorOperations.cpp
            src/win32app/RouterOperations.cpp
                    src/win32app/Win32IDE_Watchdog.cpp
        src/win32app/Win32IDE_SyntaxHighlight.cpp
        src/win32app/Win32IDE_VisionEncoder.cpp
        src/win32app/Win32IDE_Themes.cpp
        src/win32app/Win32IDE_StreamingUX.cpp
        src/win32app/Win32IDE_Sidebar.cpp
        src/win32app/Win32IDE_Debugger.cpp
        src/win32app/Win32IDE_AutonomousDebugger.cpp
        src/win32app/Win32IDE_Annotations.cpp
        src/win32app/Win32IDE_ReverseEngineering.cpp
        src/win32app/Win32IDE_Logger.cpp
        src/win32app/Win32IDE_AgenticBridge.cpp
        src/full_agentic_ide/FullAgenticIDE.cpp
        src/full_agentic_ide/AgenticPlanningOrchestrator.cpp
        src/full_agentic_ide/AgenticIOCPBridge.cpp
        src/win32app/Win32IDE_AgentCommands.cpp
        src/win32app/Win32IDE_TabManager.cpp
        src/win32app/Win32IDE_Settings.cpp
        src/win32app/Win32IDE_AutonomousCommunicator.cpp
        src/win32app/Win32IDE_AgenticComposerUX.cpp
        src/win32app/Win32IDE_ContextMentionParser.cpp
        src/win32app/Win32IDE_UnifiedTelemetry.cpp
        src/win32app/Win32IDE_Autonomy.cpp
        src/win32app/Win32IDE_PowerShell.cpp
        src/win32app/Win32IDE_PowerShellPanel.cpp
        src/win32app/Win32IDE_Session.cpp
        src/win32app/Win32IDE_GGUFInspector.cpp
        src/win32app/Win32IDE_FileOps.cpp
        src/win32app/Win32IDE_DecompilerView.cpp
        src/win32app/Win32IDE_NativePipeline.cpp
        src/win32app/Win32IDE_ModelAnatomy.cpp
        src/sampling/advanced_sampler.cpp
        src/runtime/RawrRuntime.cpp
        src/compression/zlib_runtime_loader.cpp
        src/deep2/Deep2APIServer.cpp
        src/deep2/production/Deep2ProductionRuntime.cpp
        src/reverse/ReverseEngine.cpp
        src/reverse/ReverseModelLoader.cpp
        src/inference/NegativeSpaceProfiler.cpp
        src/core/unlinked_symbols_batch_022.cpp
        Ship/RawrXD_AutonomousAgenticPipeline.cpp
        Ship/RawrXD_AgentCoordinator.cpp

        src/win32app/Win32IDE_FeatureManifest.cpp
        src/win32app/Win32IDE_GhostText.cpp
        # Deep2 completion provider and bridge — real local inference for ghost text
        src/ai_completion_real.cpp
        src/ide/Deep2Bridge.cpp
        src/ide/SovereignSharedMemoryBridge.cpp
        src/ide/ANSIColorParser.cpp
        src/ide/FindReplaceDialog.cpp
        src/ide/GitCommitDialog.cpp
        src/ide/GitDiffViewer.cpp
        src/win32app/Win32IDE_OutlinePanel.cpp
        src/win32app/Win32IDE_RenamePreview.cpp
        src/win32app/Win32IDE_Tier2Cosmetics.cpp
        src/win32app/Win32IDE_Tier1Cosmetics.cpp
        src/win32app/Win32IDE_Breadcrumbs.cpp
        src/win32app/Win32IDE_PlanExecutor.cpp
        src/agentic/agentic_planning_orchestrator.cpp
        src/agentic/failure_intelligence_orchestrator.cpp
        src/agentic/agentic_orchestrator_integration.cpp
        src/agentic/agentic_audit_sink.cpp
        src/agentic/change_impact_analyzer.cpp

        src/win32app/Win32IDE_AgenticPlanningPanel.cpp
        src/win32app/Win32IDE_FailureDetector.cpp
        src/win32app/Win32IDE_FailureIntelligence.cpp
        src/win32app/Win32IDE_Settings.cpp
        src/win32app/Win32IDE_LocalServer.cpp
        src/win32app/Win32IDE_BackendSwitcher.cpp
        src/win32app/Win32IDE_AIBackend.cpp
        src/win32app/Win32IDE_LLMRouter.cpp
        src/win32app/Win32IDE_LSPClient.cpp
        src/win32app/Win32IDE_PeekOverlay.cpp
        src/win32app/Win32IDE_CaretAnimation.cpp
        # src/win32app/Win32IDE_AgentOllamaClient.cpp  # REMOVED: Ollama backend replaced with native GGUF loader
        src/win32app/Win32IDE_ModelDiscovery.cpp
        src/win32app/Win32IDE_SwarmModelSelector.cpp
        src/win32app/Win32IDE_AsmSemantic.cpp
        src/win32app/Win32IDE_LSP_AI_Bridge.cpp
        src/win32app/Win32IDE_MultiResponse.cpp
        src/win32app/Win32IDE_ExecutionGovernor.cpp
        src/win32app/Win32IDE_LayerEviction.cpp
        src/win32app/Win32IDE_SubAgent.cpp
        src/win32app/Win32IDE_AgentHistory.cpp
        src/win32app/Win32IDE_AgentPanel.cpp
        src/win32app/Win32IDE_DiskRecovery.cpp
        src/win32app/Win32TerminalManager.cpp
        src/win32app/TransparentRenderer.cpp

        # Plugin System — Phase 43: Native Win32 DLL loading (C ABI)
        src/plugin_system/win32_plugin_loader.cpp
        src/win32app/Win32IDE_Plugins.cpp
        src/win32app/Win32IDE_HandleExtensionCommand.cpp

        # Configuration management
        src/config/IDEConfig.cpp

        # LLM adapter + Auth (Win32/STL only; Ship/StdReplacements.hpp + Ship/RawrCompatIo.hpp when needed)
        src/llm_adapter/QuantBackend.cpp
        src/llm_adapter/GGUFRunner.cpp
        src/llm_adapter/gguf_k_quants.cpp
        src/auth/QuantumAuthUI.cpp
        
        # Core engine files needed by IDE
        src/agentic_engine.cpp
        src/subagent_core.cpp

        # Bounded Agent Loop + FIM Provider (Agent→IDE Pivot)
        src/agentic/AgentToolHandlers.cpp
        src/agentic/BoundedAgentLoop.cpp
        src/agentic/OllamaProvider.cpp
        src/agentic/DiffEngine.cpp

        # T3-D: Deterministic Replay Engine (DivergenceEvent + TelemetrySnapshot)
        src/agentic/DeterministicReplayEngine.cpp

        # T4: Autonomous Recovery Orchestrator (FailureClass → recovery → verify → commit)
        src/agentic/autonomous_recovery_orchestrator.cpp
        
        # T4.5: Agentic Deep Thinking Engine — Autonomous monitoring, recovery, and orchestration
        src/agentic/AgenticDeepThinkingEngine.cpp

        # T5: Agent Orchestrator + Bridge + Navigation (agentic brain wiring)
        src/agentic/AgentOrchestrator.cpp
        src/agentic/OrchestratorBridge.cpp
        src/agentic/AgenticNavigator.cpp
        src/agentic/ToolRegistry.cpp
        src/agentic/RawrXD_AgentLoop.cpp
        src/agentic/agentic_executor.cpp
        src/agentic/FIMPromptBuilder.cpp
        src/agentic/AgentOllamaClient.cpp

        # Unlinked symbols batches - provide real implementations for link closure
        src/core/unlinked_symbols_batch_001.cpp
        src/core/unlinked_symbols_batch_002.cpp  # ADDED: Provides matmul_kernel_avx2, ggml_gemm_q4_0
        src/core/unlinked_symbols_batch_003.cpp
        src/core/unlinked_symbols_batch_004.cpp  # ADDED: Provides asm_watchdog_init
        src/core/unlinked_symbols_batch_005.cpp
        src/core/unlinked_symbols_batch_006.cpp  # ADDED: Provides asm_mesh_init, asm_mesh_quorum_vote, asm_mesh_crdt_merge
        src/core/unlinked_symbols_batch_007.cpp  # ADDED: Provides asm_mesh_fedavg_aggregate, asm_mesh_zkp_generate, asm_speciator_*
        src/core/unlinked_symbols_batch_008.cpp  # ADDED: Provides asm_neural_*, asm_hwsynth_*
        src/core/unlinked_symbols_batch_009.cpp  # ADDED: Provides asm_hwsynth_profile_dataflow, asm_hwsynth_predict_perf
        src/core/unlinked_symbols_batch_010.cpp  # ADDED: Provides SO_InitializeVulkan
        src/core/unlinked_symbols_batch_011.cpp
        src/core/unlinked_symbols_batch_012.cpp
        src/core/unlinked_symbols_batch_018.cpp  # ADDED: Provides asm_gguf_loader_close, asm_lsp_bridge_shutdown, asm_spengine_cpu_optimize
        src/core/unlinked_symbols_batch_019.cpp
        src/core/unlinked_symbols_batch_020.cpp
        src/core/unlinked_symbols_batch_021.cpp  # ADDED: Provides AnnotationOverlay, ANSIParser, SettingsManager, AgentCoordinator, Deep2_SwiGLU, BraidedLoader, ModelConversionDialog, handleAIStopGeneration
        src/core/vulkan_kernel_stubs.cpp  # ADDED: Provides VulkanKernel_* symbols for RawrXD_VulkanBridge.asm

        # ASM link stubs for missing symbols
        # src/core/asm_link_stubs.cpp  # REMOVED: Real implementations in unlinked_symbols_batch_001.cpp (asm_lsp_bridge_shutdown, asm_gguf_loader_close)
        src/gguf_d3d12_bridge.cpp  # ADDED: Real implementation (was gguf_d3d12_bridge_stub.cpp)
        # src/core/rtp_stubs.cpp  # REMOVED: Real implementations in unlinked_symbols_batch_003.cpp (RTP_* functions)
        src/core/rdna3_stubs.cpp  # KEPT: No real implementations found for RDNA3_* functions
        # src/core/ollama_provider_stub.cpp  # REMOVED: Real implementation in src/agentic/OllamaProvider.cpp
        # src/core/gguf_runner_stub.cpp  # REMOVED: Real implementations in unlinked_symbols_batch_002.cpp (matmul_kernel_avx2, ggml_gemm_q4_0)
        src/llm_adapter/rawrxd_ggufrunner_signals.cpp
        # src/core/subsystem_api_stubs.cpp  # REMOVED: Real implementations in unlinked_symbols_batch_011.cpp (AD_ProcessGGUF, SO_LoadExecFile), batch_010.cpp (SO_InitializeVulkan), batch_019.cpp
        # src/core/crypto_stubs.cpp  # REMOVED: Real implementations in unlinked_symbols_batch_005.cpp and batch_019.cpp (asm_camellia256_auth_*)
        # src/core/watchdog_stubs.cpp  # REMOVED: Real implementations in unlinked_symbols_batch_001.cpp, batch_004.cpp, batch_005.cpp, batch_019.cpp
        # src/core/pattern_stubs.cpp  # REMOVED: Real implementation in unlinked_symbols_batch_019.cpp (find_pattern_asm)
        # src/core/perf_telemetry_stubs.cpp  # REMOVED: Real implementations in unlinked_symbols_batch_001.cpp (asm_perf_init, asm_perf_begin, asm_perf_end), batch_020.cpp
        # src/core/mesh_brain_stubs.cpp  # REMOVED: Real implementations in unlinked_symbols_batch_006.cpp, batch_007.cpp
        # src/core/native_log_stub.cpp  # REMOVED: Real implementation in unlinked_symbols_batch_012.cpp
        # src/core/speciator_stubs.cpp  # REMOVED: Real implementations in unlinked_symbols_batch_007.cpp
        # src/core/neural_bridge_stubs.cpp  # REMOVED: Real implementations in unlinked_symbols_batch_008.cpp
        # src/core/hwsynth_stubs.cpp  # REMOVED: Real implementations in unlinked_symbols_batch_008.cpp, batch_009.cpp
        # src/core/spengine_stubs.cpp  # REMOVED: Real implementation in unlinked_symbols_batch_012.cpp (asm_spengine_cpu_optimize)
        # src/core/collab_stubs.cpp  # REMOVED: Real implementations in src/collab/
        src/collab/cursor_widget.cpp
        src/collab/crdt_buffer.cpp
        # src/core/mesh_brain_stubs.cpp  # REMOVED: Real implementations in unlinked_symbols_batch_006.cpp, batch_007.cpp
        # src/core/speciator_stubs.cpp  # REMOVED: Real implementations in unlinked_symbols_batch_007.cpp
        # src/core/neural_bridge_stubs.cpp  # REMOVED: Real implementations in unlinked_symbols_batch_008.cpp
        # src/core/hwsynth_stubs.cpp  # REMOVED: Real implementations in unlinked_symbols_batch_008.cpp, batch_009.cpp
        src/core/spengine_stubs.cpp  # KEPT: Provides asm_apply_memory_patch (not in any batch file)
        # src/core/model_router_stubs.cpp  # REMOVED: Real implementations in unlinked_symbols_batch_003.cpp (LoadModel, ModelLoaderInit) and unlinked_symbols_batch_012.cpp (HotSwapModel)
        # src/win32app/Win32IDE_SemanticIndex.cpp  # SKIPPED: File has syntax errors at end
        src/win32app/Win32IDE_private_stubs.cpp  # KEPT: Stubs for Win32IDE private methods
        
        # SDMA Phase 1: Coordinator (burst scheduling bridge/thread management)
        src/core/sdma/sdma_coordinator.cpp
        
        src/agentic_observability.cpp
        src/core/rawrxd_subsystem_api.cpp
        src/runtime_core.cpp
        src/tool_registry.cpp
        src/agent/telemetry_collector.cpp

        # T6: Autonomous Repair Orchestrator + Agent Memory Indexer
        src/core/auto_repair_orchestrator.cpp
        src/core/agent_memory_indexer.cpp

        # T5: Agentic Transaction Layer (multi-step command execute/rollback)
        src/agentic/agentic_transaction.cpp

        # P6: Agent Workflow Orchestrator (DAG-based plan→execute→validate→commit)
        src/agentic/agent_workflow_orchestrator.cpp

        # P1-P5: AI Developer Tooling (Copilot/Cursor parity modules)
        src/ai/symbol_graph_indexer.cpp
        src/ai/semantic_code_search.cpp
        src/ai/multi_file_reasoning.cpp
        src/ai/workspace_embeddings.cpp
        src/ai/repo_refactor_engine.cpp

        # AgenticObservability + SubsystemRegistry (required by AgentOrchestrator, ToolRegistry, feature_handlers)
        # already added above in this block; keep single provider per file

        # Disk Recovery Agent (C++ implementation) + Tool handler + ASM wrapper
        src/agent/DiskRecoveryAgent.cpp
        src/agentic/DiskRecoveryAgent.cpp
        src/agentic/DiskRecoveryToolHandler.cpp
        src/agent_history.cpp
        src/agent_policy.cpp
        src/agent_explainability.cpp
        src/utils/ErrorReporter.cpp
        # Observability + validation + perf telemetry (link-time required)
        src/logging/Logger.cpp
        src/security/InputSanitizer.cpp
        src/core/ConfigurationValidator.cpp
        src/inference/PerformanceMonitor.cpp
        src/rawrxd_inference.cpp
        src/rawrxd_model_loader.cpp
        src/rawrxd_transformer.cpp
        src/rawrxd_tokenizer.cpp
        src/rawrxd_sampler.cpp
        src/modules/engine_manager.cpp
        src/cpu_inference_engine.cpp
        src/core/swarm_scheduler.cpp
        src/core/gguf_swarm_plan_builder.cpp
        src/memory_core.cpp
        src/hot_patcher.cpp
        src/streaming_gguf_loader.cpp
        src/utils/Diagnostics.cpp
        src/gguf_loader.cpp
        src/gguf_vocab_resolver.cpp
        src/model_source_resolver.cpp
        src/compression_interface.cpp
        src/engine/gguf_core.cpp
        src/engine/inference_kernels.cpp
        src/engine/transformer.cpp
        src/engine/bpe_tokenizer.cpp
        src/engine/sampler.cpp
        src/engine/rawr_engine.cpp
        src/engine/core_generator.cpp
        src/engine/pyre_compute.cpp
        src/engine/sovereign_engines.cpp
        # Streaming engine registry (Phase 9)
        src/core/streaming_engine_registry.cpp
        src/core/gpu_backend_bridge.cpp
        src/core/enterprise_license.cpp
        src/core/enterprise_licensev2_impl.cpp
        src/core/enterprise_devunlock_bridge.cpp
        src/core/monolithic_heap_globals.cpp
        src/tools/license_validator_manifest.cpp
        src/core/license_anti_tampering.cpp
        src/core/camellia256_bridge.cpp
        src/core/watchdog_service.cpp
        # Win32IDE uses the win32app multi-response integration unit.
        # Avoid duplicate symbol emission from parallel multi_response_engine defs.
        src/core/flash_attention.cpp
        src/core/execution_scheduler.cpp
        src/core/execution_governor.cpp
        src/core/multi_gpu.cpp
        src/core/agent_safety_contract.cpp
        src/core/deterministic_replay.cpp
        src/core/confidence_gate.cpp
        
        # Layer Offload Manager (required by pyre_compute)
        src/core/layer_offload_manager.cpp

        # Native speed layer + local AI core (zero-dependency inference)
        src/core/native_speed_layer.cpp
        src/core/local_ai_core.cpp
        src/core/native_inference_pipeline.cpp
        
        # 70B GGUF Hotpatch System
        src/core/70b_gguf_hotpatch.cpp
        
        # Governor/Throttling System
        src/core/governor_throttling.cpp
        
        # Distributed Swarm Compilation (Phase 11)
        src/core/swarm_coordinator.cpp
        src/core/swarm_worker.cpp
        src/win32app/Win32IDE_SwarmPanel.cpp
        
        # Dual-Agent Orchestrator (Phase 41)
        src/win32app/Win32IDE_DualAgentPanel.cpp
        
        # Native Debugger Engine (Phase 12) + P0 source-level symbols (dbghelp)
        src/core/native_debugger_engine.cpp
        src/core/native_debugger_symbols.cpp
        src/win32app/Win32IDE_NativeDebugPanel.cpp
        
        # Three-Layer Hotpatch System (Phase 14.2)
        src/core/model_memory_hotpatch.cpp
        src/core/byte_level_hotpatcher.cpp
        src/core/unified_hotpatch_manager.cpp
        src/core/proxy_hotpatcher.cpp
        src/core/model_bruteforce_engine.cpp
        src/server/gguf_server_hotpatch.cpp
        src/core/pt_driver_contract.cpp
        src/core/shadow_page_detour.cpp
        src/core/sentinel_watchdog.cpp
        src/win32app/Win32IDE_HotpatchPanel.cpp
        
        # Agent-Driven Hotpatch Orchestration (Phase 18)
        src/agent/agentic_hotpatch_orchestrator.cpp

        # HTTP Client — WinHTTP async with future retention (CLI+GUI)
        src/agent/llm_http_client.cpp
        src/agent/model_invoker.cpp
        src/agent/planner.cpp
        src/agent/meta_planner.cpp
        src/agent/eval_framework.cpp
        src/agent/autonomous_subagent.cpp
        src/agent/agentic_failure_detector.cpp
        src/agent/agentic_deep_thinking_engine.cpp
        
        # Quantum Autonomous Systems - Real implementations (DUPLICATED - see main SOURCES)
        # src/agent/quantum_autonomous_todo_system.cpp
        # src/agent/quantum_multi_model_agent_cycling.cpp
        # src/agent/quantum_dynamic_time_manager.cpp
        # src/agent/quantum_missing_impl.cpp
        # src/agent/quantum_production_orchestrator.cpp
        # src/agent/quantum_agent_orchestrator.cpp
        
        src/core/ai_agent_masm_core_impl.cpp
        src/agent/agentic_puppeteer.cpp
        src/cloud_api_client.cpp

        # LocalReasoningEngine — API-free static analysis + pattern detection
        src/agent/local_reasoning_engine.cpp

        # Agent Self-Repair — MASM64 self-patching kernel bridge
        src/agent/agent_self_repair.cpp
        src/agent/agent_self_healing_orchestrator.cpp
        
        # Module implementations
        src/modules/vsix_loader_win32.cpp
        src/modules/codex_ultimate.cpp

        # Phase 29: VS Code Extension API Compatibility Layer
        src/modules/vscode_extension_api.cpp
        src/win32app/Win32IDE_VSCodeExtAPI.cpp
        
        # Phase 21: Distributed Swarm Bridge + Universal Model Hotpatcher
        src/core/swarm_decision_bridge.cpp
        src/core/universal_model_hotpatcher.cpp
        
        # Phase 22: Production Release Engineering
        src/core/production_release.cpp
        
        # Phase 22: Quantum-Safe Transport, Adaptive Pipeline, Universal Model Merger
        src/core/quantum_safe_transport.cpp
        src/core/adaptive_pipeline_parallel.cpp
        src/core/universal_model_merger.cpp
        
        # Phase 20: WebRTC P2P Signaling
        src/core/webrtc_signaling.cpp
        
        # Phase 23: GPU Kernel Auto-Tuner
        src/core/gpu_kernel_autotuner.cpp
        
        # Phase 24: Windows Sandbox Integration
        src/core/sandbox_integration.cpp
        
        # Phase 25: AMD GPU Acceleration (RX 7800 XT - actual hardware)
        src/core/amd_gpu_accelerator.cpp
        src/core/unified_memory_executor.cpp
        src/core/model_runtime_gate.cpp
        
        # Intel/ARM64/Cerebras accelerator implementations
        src/core/intel_gpu_accelerator.cpp
        src/core/arm64_gpu_accelerator.cpp
        src/core/cerebras_wse_accelerator.cpp
        
        # Phase 30: Unified Accelerator Router
        src/core/accelerator_router.cpp
        
        # Phase 29.2: GSI Hash Table + TPI Type Parser
        src/core/pdb_gsi_hash.cpp
        
        # Phase 29.2: Multi-Reference Provider + Tools/Instructions Hook
        src/core/pdb_reference_provider.cpp
        
        # Phase 26: WebView2 + Monaco Editor Integration
        src/win32app/Win32IDE_WebView2.cpp
        src/win32app/AgenticBrowserLayer.cpp
        src/win32app/Win32IDE_AgenticBrowser.cpp
        src/win32app/Win32IDE_MonacoThemes.cpp

        # Phase 27: Embedded LSP Server (JSON-RPC 2.0)
        src/lsp/RawrXD_LSPServer.cpp
        src/win32app/Win32IDE_LSPServer.cpp

        # Phase 27b: LSP ↔ Hotpatch Bridge (Symbol Export, Diagnostics, JSON-RPC Extension)
        src/lsp/lsp_hotpatch_bridge.cpp
        src/lsp/hotpatch_symbol_provider.cpp
        src/lsp/gguf_diagnostic_provider.cpp

        # Phase 28: MonacoCore Native Editor Engine — Toggleable Engine System
        src/core/MonacoCoreEngine.cpp
        src/core/WebView2EditorEngine.cpp
        src/core/RichEditEditorEngine.cpp
        src/core/EditorEngineFactory.cpp
        src/win32app/Win32IDE_EditorEngine.cpp

        # Phase 29: Native PDB Symbol Server — MSF v7.00 Parser + Symbol Cache
        src/core/pdb_native.cpp
        src/core/pdb_lsp_bridge.cpp
        src/win32app/Win32IDE_PDBSymbols.cpp

        # Phase 31: IDE Self-Audit & Verification System
        src/core/feature_registry.cpp
        src/core/menu_auditor.cpp
        src/core/auto_discovery.cpp
        src/win32app/Win32IDE_AuditDashboard.cpp

        # Phase 32: The Final Gauntlet — Pre-Packaging Runtime Verification
        src/core/final_gauntlet.cpp
        src/win32app/Win32IDE_Gauntlet.cpp

        # Phase 33: Voice Chat Engine
        src/core/voice_chat.cpp
        src/core/voice_automation.cpp
        src/win32app/Win32IDE_VoiceChat.cpp
        src/win32app/Win32IDE_VoiceAutomation.cpp

        # Phase 33: Quick-Win Ports
        src/core/shortcut_manager.cpp
        src/core/backup_manager.cpp
        src/core/alert_system.cpp
        src/win32app/Win32IDE_QuickWins.cpp

        # Shared Feature Dispatch System (CLI + GUI unified command routing)
        # NOTE: shared_feature_dispatch.cpp REMOVED — duplicated by unified_command_dispatch.cpp
        src/core/feature_registration.cpp
        src/core/feature_handlers.cpp
        # auto_feature_registry excluded for Win32 IDE — use ssot_handlers (lightweight stubs)
        src/core/intent_engine.cpp
        src/core/model_training_pipeline.cpp
        src/model_trainer.cpp
        # REMOVED: src/agentic/AgentOllamaClient.cpp - Ollama backend replaced with native GGUF loader
        # REMOVED: src/agentic/AgentOllamaClient_stubs.cpp - stub file deleted
        src/agentic/hotpatch/Engine.cpp
        src/agentic/observability/Logger.cpp
        src/agentic/AdvancedAgentCoordinator.cpp
        src/modules/autonomous_agentic_orchestrator.cpp
        src/core/native_gguf_loader_link_impl.cpp
        src/codec/compression.cpp
        src/codec/brutal_gzip.cpp
        src/codec/gzip_brutal_inflate.cpp
        src/core/ssot_handlers.cpp
        src/core/ssot_handlers_ext_isolated.cpp
        # src/core/beacon_link_stub.cpp  # REMOVED: File does not exist
        src/win32app/v280_link_bridge.cpp
        src/core/ssot_validation.cpp

        # Unified Command Dispatch — Zero-drift SSOT dispatch (replaces manual registration)
        src/core/unified_command_dispatch.cpp
        src/core/startup_phase_registry.cpp

        # RawrEngine Command Handlers (required for Win32IDE link closure)
        src/core/rawrengine_command_handlers.cpp
        src/core/jsonrpc_parser.cpp

        # Phase 21: Distributed Swarm Inference Orchestrator
        src/cli/swarm_orchestrator.cpp

        # Phase 32A: Chain-of-Thought Multi-Model Review Engine
        src/core/chain_of_thought_engine.cpp

        # Phase 34: Telemetry Export
        src/win32app/Win32IDE_Telemetry.cpp

        # Phase 36: MCP Integration — Model Context Protocol Server + Client
        src/mcp_integration.cpp
        src/win32app/Win32IDE_MCP.cpp

        # Phase 36: Flight Recorder — memory-mapped binary ring-buffer logger
        src/win32app/Win32IDE_FlightRecorder.cpp

        # Phase 36: QuickJS VSIX JavaScript Extension Host
        src/modules/quickjs_extension_host.cpp
        src/modules/quickjs_vscode_bindings.cpp

        # Phase 36: Cross-Process MMF State Synchronization
        src/core/rawrxd_state_mmf.cpp

        # Phase 37: JS Extension Host + Auto-Polyfill Engine
        src/core/js_extension_host.cpp
        src/core/extension_polyfill_engine.cpp

        # Phase 46: VulkanKernel — Full GPU Compute Engine
        src/vulkan_compute.cpp

        # Phase 46: Live Binary Patcher — Layer 5 Hotpatch
        src/core/live_binary_patcher.cpp

        # ═══════════════════════════════════════════════════════════════
        # Core Production Subsystems (Verified Working)
        # ═══════════════════════════════════════════════════════════════
        src/core/deterministic_swarm.cpp
        src/core/swarm_scheduler.cpp
        src/core/gguf_swarm_plan_builder.cpp
        src/core/input_guard_slicer.cpp
        src/agentic_agent_coordinator.cpp
        src/production_config_manager.cpp

        # Singleton subsystems referenced by UnifiedHotpatchManager
        src/core/agentic_task_graph.cpp
        src/core/embedding_engine.cpp
        src/core/vector_index.cpp
        src/core/vision_encoder.cpp

        # Vision Pipeline Extensions (Token Gating, Embedding Cache, KV Isolation, Quantized Encoder, GPU Staging)
        src/core/vision_token_gate.cpp
        src/core/vision_embedding_cache.cpp
        src/core/vision_kv_isolation.cpp
        src/core/vision_quantized_encoder.cpp
        src/core/vision_gpu_staging.cpp

        src/marketplace/extension_marketplace.cpp
        src/auth/rbac_engine.cpp

        # Agentic Loop State (iterative reasoning state machine)
        src/agentic_loop_state.cpp

        # Transaction Journal (WAL journal for atomic file operations)
        src/core/transaction_journal.cpp

        # Instructions Provider — unified *.instructions.md loader (HTTP+CLI+GUI)
        src/core/instructions_provider.cpp
        src/win32app/Win32IDE_Instructions.cpp

        # Phase 45: Game Engine Integration (Unity + Unreal)
        src/modules/unity_engine_integration.cpp
        src/modules/unreal_engine_integration.cpp
        src/modules/game_engine_manager.cpp
        src/win32app/Win32IDE_GameEnginePanel.cpp

        # Phase 48: The Final Crucible (Unified Stress-Test Harness)
        src/modules/crucible_engine.cpp
        src/win32app/Win32IDE_CruciblePanel.cpp

        # Phase 49: Copilot Gap Closer (HNSW + Composer + CRDT + Git Context)
        src/modules/copilot_gap_closer.cpp
        src/win32app/Win32IDE_CopilotGapPanel.cpp

        # Phase 13: Distributed Pipeline Orchestrator
        src/core/distributed_pipeline_orchestrator.cpp
        src/win32app/Win32IDE_PipelinePanel.cpp

        # Phase 14: Hotpatch Control Plane
        src/core/hotpatch_control_plane.cpp
        src/win32app/Win32IDE_HotpatchCtrlPanel.cpp

        # Phase 15: Static Analysis Engine
        src/core/static_analysis_engine.cpp
        src/core/code_linter.cpp
        src/lsp/diagnostic_consumer.cpp
        src/win32app/Win32IDE_StaticAnalysisPanel.cpp

        # P0: Unified Problems Panel (LSP, SAST, SCA, Secrets, Build)
        src/win32app/Win32IDE_ProblemsPanel.cpp
        src/win32app/Win32IDE_BuildRunner.cpp
        src/win32app/Win32IDE_SecurityScans.cpp
        src/core/problems_panel_bridge.cpp
        src/core/problems_aggregator.cpp
        src/security/secrets_scanner.cpp
        src/security/sast_rule_engine.cpp
        src/security/dependency_audit.cpp
        src/win32app/Win32IDE_AgentStreamingBridge.cpp

        # Phase 16: Semantic Code Intelligence
        src/core/semantic_code_intelligence.cpp
        src/win32app/Win32IDE_SemanticPanel.cpp

        # Phase 17: Enterprise Telemetry & Compliance
        src/core/enterprise_telemetry_compliance.cpp
        src/win32app/Win32IDE_TelemetryPanel.cpp
        src/win32app/Win32IDE_Tier3Polish.cpp
        src/win32app/Win32IDE_Tier3Cosmetics.cpp
        src/win32app/Win32IDE_Tier5Cosmetics.cpp
        src/win32app/Win32IDE_CrashReporter.cpp
        src/win32app/Win32IDE_ColorPicker.cpp
        src/win32app/Win32IDE_EmojiSupport.cpp
        src/win32app/Win32IDE_ShortcutEditor.cpp
        src/win32app/Win32IDE_TelemetryDashboard.cpp
        src/win32app/Win32IDE_MarketplacePanel.cpp
        src/win32app/Win32IDE_Collab.cpp

        # Phase 51: mIRC Control Bridge — IRC bot for remote IDE control
        src/win32app/Win32IDE_IRCBridge.cpp
        src/win32app/Win32IDE_IRCBridgeCommands.cpp
        src/collab/websocket_hub.cpp
        src/win32app/Win32IDE_TestExplorerTree.cpp
        src/win32app/Win32IDE_NetworkPanel.cpp
        # NOTE: Experimental features removed from production build
        src/core/omega_orchestrator.cpp
        src/core/omega_asm_native_kernel.cpp
        src/core/mesh_brain.cpp
        src/core/speciator_engine.cpp
        src/core/neural_bridge.cpp
        src/core/self_host_engine.cpp
        src/core/hardware_synthesizer.cpp
        src/core/transcendence_coordinator.cpp
        src/core/integrated_runtime.cpp
        # src/core/safe_refactor_engine.cpp      # MOVED: to main SOURCES (required by unified_hotpatch_manager)
        # src/core/workspace_reasoning_profiles.cpp # MOVED: to main SOURCES
        # src/core/reasoning_schema_versioning.cpp  # MOVED: to main SOURCES
        # src/core/autonomous_workflow_engine.cpp   # MOVED: to main SOURCES
        src/core/reasoning_profile.cpp
        # src/core/win32ide_link_stubs.cpp  # Conflicts with existing fallbacks
        # src/win32app/Win32IDE_TranscendencePanel.cpp
        src/win32app/VulkanRenderer.cpp
        src/win32app/OSExplorerInterceptor.cpp
        src/win32app/Win32IDE_MCPHooks.cpp
        src/win32app/TodoManager.cpp
        src/win32app/IocpFileWatcher.cpp
        src/win32app/IDEDiagnosticAutoHealer.cpp
        src/win32app/ConsentPrompt.cpp
        src/win32app/AutonomousAgent.cpp

        # UI Rendering — Chat message renderer + tool action status + chat panel
        src/ui/chat_message_renderer.cpp
        src/ui/tool_action_status.cpp
        src/ui/chat_panel.cpp

        # Cathedral Hardening (5.0 Readiness)
        src/core/crash_containment.cpp
        src/core/patch_rollback_ledger.cpp
        src/core/quant_hysteresis.cpp
        src/core/masm_stress_harness.cpp
        src/core/perf_telemetry.cpp
        src/core/convergence_stress_harness.cpp

        # ASM Stub Implementations - REMOVED: All stub files deleted, real implementations only

        # Phase 50: Enterprise Hardening — Auto-Update, Swarm Reconciliation, Plugin Sandbox/Signature, Stress Tests
        src/core/update_signature.cpp
        src/core/cursor_github_parity_bridge.cpp
        src/core/swarm_reconciliation.cpp
        src/core/quickjs_sandbox.cpp
        src/core/plugin_signature.cpp
        src/core/enterprise_stress_tests.cpp
        src/core/auto_update_system.cpp

        # ═══════════════════════════════════════════════════════════════
        # Autonomy Gap Closure — Phase A-D + Tier 1.4 + Tier 2.3
        # ═══════════════════════════════════════════════════════════════

        # Phase A: Closed-Loop Verification (Write → Compile → Test → Fix)
        src/agentic/autonomous_verification_loop.cpp

        # Phase B: Continuous Background Daemon (proactive refactor, doc-sync, security)
        src/agentic/autonomous_background_daemon.cpp

        # Phase C: Long-Term Knowledge Graph (cross-session learning, decision memory)
        src/core/sqlite3.c
        src/core/knowledge_graph_core.cpp

        # Phase D: Multi-Agent Conflict Resolution (merge negotiation, consensus)
        src/core/swarm_conflict_resolver.cpp

        # Tier 1.4: Autonomous Debugging & Root Cause Analysis
        src/core/autonomous_debugger.cpp

        # Tier 2.3: Autonomous Communication (standup reports, approval requests)
        src/agentic/autonomous_communicator.cpp

        # Unified Telemetry Core
        src/telemetry/UnifiedTelemetryCore.cpp

        # ═══════════════════════════════════════════════════════════════
        # Cursor/JB-Parity Feature Modules
        # ═══════════════════════════════════════════════════════════════
        src/telemetry/telemetry_export.cpp
        src/agentic/agentic_composer_ux.cpp
        src/context/context_mention_parser.cpp
        src/multimodal_engine/vision_encoder.cpp
        src/ide/refactoring_plugin.cpp
        src/ide/language_plugin.cpp
        src/context/semantic_index.cpp
        src/ide/resource_generator.cpp
        src/win32app/Win32IDE_CursorParity.cpp
        src/win32app/Win32IDE_GUILayoutHotpatch.cpp

        # ═══════════════════════════════════════════════════════════════
        # Compile-Debt Resolved: IDE Feature Modules (Tasks, Git, Search,
        # Fuzzy Search, Refactor, CallStack, Multi-File Search)
        # ═══════════════════════════════════════════════════════════════
        src/win32app/Win32IDE_Tasks.cpp
        src/win32app/Win32IDE_GitPanel.cpp
        src/win32app/Win32IDE_Refactor.cpp
        src/win32app/Win32IDE_SearchPanel.cpp
        src/win32app/Win32IDE_FuzzySearch.cpp

        # ═══════════════════════════════════════════════════════════════
        # Compile-Debt Resolved: IDE Cosmetics & Intelligence Layer
        # (Minimap, DiffView, SettingsGUI, CodeLens, InlayHints, Hover)
        # ═══════════════════════════════════════════════════════════════
        src/win32app/Win32IDE_Minimap.cpp
        src/win32app/Win32IDE_DiffView.cpp
        src/win32app/Win32IDE_SettingsGUI.cpp
        src/win32app/Win32IDE_CodeLens.cpp
        src/win32app/Win32IDE_InlayHints.cpp
        src/win32app/Win32IDE_HoverTooltips.cpp

        # ═══════════════════════════════════════════════════════════════
        # Flagship Product Pillars (Provable Agent, AI RE, Airgapped)
        # ═══════════════════════════════════════════════════════════════
        src/win32app/Win32IDE_ProvableAgent.cpp
        src/win32app/Win32IDE_AIReverseEngineering.cpp
        src/win32app/Win32IDE_AirgappedEnterprise.cpp
        src/win32app/Win32IDE_FlagshipFeatures.cpp

        # Linker deps: ProjectContext, UniversalModelRouter, ModelRegistry, CheckpointManager
        src/project_context.cpp
        src/universal_model_router.cpp
        src/model_registry.cpp
        src/model_name_utils.cpp
        src/core/checkpoint_manager.cpp
        # AgenticAutonomousConfig, EnterpriseFeatureManager, ContextDeteriorationHotpatch
        src/core/agentic_autonomous_config.cpp
        src/core/enterprise_feature_manager.cpp
        src/core/context_deterioration_hotpatch.cpp
        # src/core/model_loader_asm_stubs.cpp  # Conflicts with model_loader_fallbacks.cpp
        # Win32 panels and dialogs
        src/ui/interpretability_panel.cpp
        # Advanced feature modules (src scope)
        src/real_time_completion_engine.cpp
        src/LanguageServerIntegration.cpp
        src/interpretability_panel_enhanced.cpp
        src/feature_registry_panel.cpp
        src/autonomous_feature_engine.cpp
        src/autonomous_intelligence_orchestrator.cpp
        # Advanced feature module deps
        src/plan_orchestrator.cpp
        src/tool_registry_advanced.cpp
        src/lsp_client.cpp
        src/intelligent_codebase_engine.cpp
        src/ai_model_caller.cpp
        src/hybrid_cloud_manager.cpp
        # Benchmarks (real implementation; replaces earlier IDE stubs)
        src/benchmark_menu_widget.cpp
        src/benchmark_runner.cpp
        src/win32app/feature_registry_panel.cpp
        src/ui/monaco_settings_dialog.cpp
        src/thermal/RAWRXD_ThermalDashboard.cpp
        src/win32app/VSCodeMarketplaceAPI.cpp
        # Multi-file search (real implementation; replaces earlier IDE stub)
        src/multi_file_search.cpp
        # Legacy support unit wiring removed from production lane.
        # Completed modules previously documented as shipped
        src/core/license_manager_panel.cpp
        # Qt-based logger disabled (project is Qt-free; Win32IDE_Logger.cpp provides logging)
        # src/telemetry/logger.cpp
        # License Creator + Feature Registry dialogs (showLicenseCreatorDialog, showFeatureRegistryDialog)
        src/win32app/Win32IDE_LicenseCreator.cpp
        # License enforcement + manifest (g_FeatureManifest + featureName from license_validator_manifest.cpp; LicenseEnforcer from license_enforcement.cpp)
        src/license_enforcement.cpp
        src/feature_flags_runtime.cpp

        # ═══════════════════════════════════════════════════════════════
        # 10x Dual Engine + Unified Overclock + Quantum Beaconism Fusion
        # ═══════════════════════════════════════════════════════════════

        # ═══════════════════════════════════════════════════════════════
        # Batch 2 Parity Enhancements (Peek, AutoSave, TermSplit, Snippets, MultiCursor)
        # ═══════════════════════════════════════════════════════════════
        src/win32app/Win32IDE_PeekView.cpp
        src/win32app/Win32IDE_AutoSave.cpp
        src/win32app/Win32IDE_TerminalSplit.cpp
        src/win32app/Win32IDE_SnippetEngine.cpp
        src/win32app/Win32IDE_MultiCursor.cpp

        # ═══════════════════════════════════════════════════════════════
        # Cognitive Architecture — Autonomous Reverse Engineering OS
        # ═══════════════════════════════════════════════════════════════
        src/agentic/cognitive/cognitive_types.cpp
        src/agentic/cognitive/enhanced_blackboard.cpp
        src/agentic/cognitive/mission_director.cpp
        src/agentic/cognitive/reflection_agent.cpp
        src/agentic/cognitive/dynamic_planner.cpp
        src/agentic/cognitive/autonomous_orchestrator.cpp

        # ═══════════════════════════════════════════════════════════════
        # TrailBrake / HotPatcher / AntiPatcher Safety System
        # ═══════════════════════════════════════════════════════════════
        src/deep2/TrailBrake.cpp
        src/deep2/HotPatcher.cpp
        src/deep2/HotPatcherSafety.cpp
        src/deep2/AntiPatcher.cpp
        src/deep2/GoalSystem.cpp
        src/deep2/PatchCache.cpp
        src/deep2/BottleTTL.cpp
        src/deep2/CPUFrequency.cpp
        # Dual GPU / Reverse Recovery / Streaming
        src/deep2/DualGPUHook.cpp
        src/deep2/ReverseTensorRecovery.cpp
        src/deep2/StreamEngine.cpp
        src/deep2/ShardRouterIntegration.cpp
        src/deep2/Deep2IDEIntegration.cpp
        src/deep2/FabricTensorTable.cpp
        src/deep2/FabricTensorTable.cpp
    )

    if(NOT RAWRXD_INCLUDE_STRESS_AND_REPLAY_SOURCES)
        list(REMOVE_ITEM WIN32IDE_SOURCES
            src/core/masm_stress_harness.cpp
            src/core/convergence_stress_harness.cpp
            src/core/enterprise_stress_tests.cpp
        )
    endif()

    # DbgEng COM interfaces used by the native debugger engine are MSVC/SDK-specific.
    # For non-MSVC toolchains (e.g., MinGW cross-builds), keep the rest of Win32IDE
    # buildable by excluding these units.
    if(NOT MSVC)
        list(REMOVE_ITEM WIN32IDE_SOURCES
            src/core/native_debugger_engine.cpp
            src/core/native_debugger_symbols.cpp
            src/win32app/Win32IDE_NativeDebugPanel.cpp
            src/core/shadow_page_detour.cpp
            src/agent/agentic_deep_thinking_engine.cpp
            # NOTE: Quantum fiction files already removed from SOURCES
            src/vulkan_compute.cpp
            src/runtime/TensorExecutionRouter.cpp
            src/core/embedding_engine.cpp
            src/core/vector_index.cpp
            src/core/agentic_task_graph.cpp
            src/core/vision_encoder.cpp
            src/win32app/Win32IDE_Minimap.cpp
            src/win32app/Win32IDE_SettingsGUI.cpp
            src/win32app/VulkanRenderer.cpp
            src/win32app/OSExplorerInterceptor.cpp
        )
    endif()
     
    # =========================================================================
    # MultiWindow Task Scheduler Kernel (MASM64 DLL)
    # =========================================================================
    set(RAWRXD_HAS_MW_KERNEL_FLAG 0)
    if(RAWR_HAS_MASM)
        set(MW_KERNEL_ASM src/asm/RawrXD_MultiWindow_Kernel.asm)
        set_source_files_properties(${MW_KERNEL_ASM} PROPERTIES LANGUAGE ASM_MASM)

        add_library(RawrXD_MultiWindow_Kernel SHARED
            ${MW_KERNEL_ASM}
            src/core/multiwindow_scheduler.cpp
        )
        target_include_directories(RawrXD_MultiWindow_Kernel PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src/core
        )
        set_target_properties(RawrXD_MultiWindow_Kernel PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY bin
            OUTPUT_NAME RawrXD_MultiWindow_Kernel
            LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/src/asm/MultiWindowKernel.def"
        )
        target_link_libraries(RawrXD_MultiWindow_Kernel PRIVATE
            kernel32 user32 winhttp
        )
        target_compile_definitions(RawrXD_MultiWindow_Kernel PRIVATE
            RAWRXD_MW_KERNEL_EXPORTS=1
        )
        set(RAWRXD_HAS_MW_KERNEL_FLAG 1)
        message(STATUS "MultiWindow Kernel DLL target enabled")
    else()
        message(STATUS "MultiWindow Kernel DLL skipped (no MASM)")
    endif()

    # =========================================================================
    # Dynamic Prompt Engine DLL (MASM64 kernel + C glue)
    # Unity/Unreal compatible SHARED library
    # =========================================================================
    set(RAWRXD_HAS_PROMPT_ENGINE_FLAG 0)
    if(RAWR_HAS_MASM)
        # Use the stub file - GPU_DMA_COMPLETE_IMPLEMENTATIONS.asm has 32-bit directives
        # incompatible with ml64.exe. The stub provides the exports declared in the .def file.
        set(PROMPT_ENGINE_ASM src/asm/RawrXD_DynamicPromptEngine.asm)
        set_source_files_properties(${PROMPT_ENGINE_ASM} PROPERTIES LANGUAGE ASM_MASM)

        # Single target definition with conditional EXCLUDE_FROM_ALL
        if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
            set(_RAWRXD_PROMPT_ENGINE_EXCLUDE "")
        else()
            set(_RAWRXD_PROMPT_ENGINE_EXCLUDE "EXCLUDE_FROM_ALL")
        endif()
        add_library(RawrXD_DynamicPromptEngine SHARED ${_RAWRXD_PROMPT_ENGINE_EXCLUDE}
            ${PROMPT_ENGINE_ASM}
            src/core/dynamic_prompt_engine_glue.cpp
        )
        target_include_directories(RawrXD_DynamicPromptEngine PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src/core
        )
        set_target_properties(RawrXD_DynamicPromptEngine PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY bin
            OUTPUT_NAME RawrXD_DynamicPromptEngine
            LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_DynamicPromptEngine.def"
        )
        target_link_libraries(RawrXD_DynamicPromptEngine PRIVATE
            kernel32 user32
        )
        target_compile_definitions(RawrXD_DynamicPromptEngine PRIVATE
            RAWRXD_PROMPT_ENGINE_EXPORTS=1
        )
        set(RAWRXD_HAS_PROMPT_ENGINE_FLAG 1)
        message(STATUS "DynamicPromptEngine DLL target enabled (Unity/Unreal compatible)")
    else()
        message(STATUS "DynamicPromptEngine DLL skipped (no MASM)")
    endif()
    if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
        if(NOT RAWRXD_HAS_MW_KERNEL_FLAG)
            message(FATAL_ERROR "[ProductionLane] RawrXD_MultiWindow_Kernel must be enabled when RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON.")
        endif()
        if(NOT RAWRXD_HAS_PROMPT_ENGINE_FLAG)
            message(FATAL_ERROR "[ProductionLane] RawrXD_DynamicPromptEngine must be enabled when RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON.")
        endif()
        if(NOT TARGET RawrXD_MultiWindow_Kernel)
            message(FATAL_ERROR "[ProductionLane] RawrXD_MultiWindow_Kernel target is required in strict production strip mode.")
        endif()
        if(NOT TARGET RawrXD_DynamicPromptEngine)
            message(FATAL_ERROR "[ProductionLane] RawrXD_DynamicPromptEngine target is required in strict production strip mode.")
        endif()
    endif()

    target_compile_definitions(RawrEngine PRIVATE
        RAWRXD_HAS_MW_KERNEL=${RAWRXD_HAS_MW_KERNEL_FLAG}
        RAWRXD_HAS_PROMPT_ENGINE=${RAWRXD_HAS_PROMPT_ENGINE_FLAG}
        RAWRXD_DISABLE_DUPLICATE_SHIMS=1
    )
    target_compile_definitions(RawrXD_Gold PRIVATE
        RAWRXD_HAS_MW_KERNEL=${RAWRXD_HAS_MW_KERNEL_FLAG}
        RAWRXD_HAS_PROMPT_ENGINE=${RAWRXD_HAS_PROMPT_ENGINE_FLAG}
    )
    target_compile_definitions(InferenceEngine PRIVATE
        RAWRXD_HAS_MW_KERNEL=${RAWRXD_HAS_MW_KERNEL_FLAG}
        RAWRXD_HAS_PROMPT_ENGINE=${RAWRXD_HAS_PROMPT_ENGINE_FLAG}
    )
    target_compile_definitions(RawrXD-InferenceEngine PRIVATE
        RAWRXD_HAS_MW_KERNEL=${RAWRXD_HAS_MW_KERNEL_FLAG}
        RAWRXD_HAS_PROMPT_ENGINE=${RAWRXD_HAS_PROMPT_ENGINE_FLAG}
    )

    if(RAWRXD_ENABLE_MISSING_HANDLER_STUBS)
        list(REMOVE_ITEM WIN32IDE_SOURCES
            src/core/ssot_handlers.cpp
            src/core/ssot_handlers_ext_isolated.cpp
            src/core/ssot_auto_missing_handlers.cpp
            src/core/ssot_linker_gap_handlers.cpp
            src/core/win32ide_missing_handlers.cpp
        )
        list(APPEND WIN32IDE_SOURCES
            src/core/ssot_missing_handlers_provider.cpp
        )
        message(STATUS "[SSOT] Win32IDE: STUBS lane enabled (excluding ssot_handlers.cpp + ssot_handlers_ext_isolated.cpp; adding ssot_missing_handlers_provider.cpp)")
    else()
        list(REMOVE_ITEM WIN32IDE_SOURCES
            src/core/ssot_auto_missing_handlers.cpp
            src/core/ssot_linker_gap_handlers.cpp
        )
        # Real lane - keep isolated handlers and runtime menu/command handlers.
        # DO NOT remove ssot_handlers_ext_isolated.cpp - it has working VSCEXT handlers 10000-10009
        # DO NOT add ssot_handlers_ext.cpp - it has unresolved symbols we haven't fixed
        # Fallback TUs (below) + rawr_engine_link_shims when linked; no win32_ide_link_stubs.cpp (removed from tree).
        # only in explicit non-strict MSVC compatibility lanes.
        message(STATUS "Win32IDE: Real lane - using isolated handlers + runtime handlers")
    endif()

    # AgenticExecutor real implementation now provides these members.
    # Keep the link stub out of Win32IDE to avoid duplicate definitions.
    list(REMOVE_ITEM WIN32IDE_SOURCES
        src/core/agentic_executor_link_stub.cpp
        src/core/beacon_link_stub.cpp
    )

    # AUTO provider: ssot_handlers.cpp and auto_feature_registry.cpp both define
    # some command handlers. Use /FORCE:MULTIPLE on the linker to tolerate the
    # duplicates (auto_feature_registry takes priority due to link order).
    # We keep ssot_handlers.cpp because it also provides unique handlers
    # (handleTier1AutoUpdateCheck, handleTier1UpdateDismiss, isBeaconFullActive, etc.)
    # that auto_feature_registry.cpp does not define.

    if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
        list(REMOVE_ITEM WIN32IDE_SOURCES
            src/core/subsystem_mode_fallbacks.cpp
            src/core/model_loader_fallbacks.cpp
            src/core/rawrxd_native_log_impl.cpp
            src/llm_adapter/ggufrunner_link_fallbacks.cpp
            src/win32app/collab_cursor_fallbacks.cpp
        )
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/.*_fallback[^/]*\\.cpp$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/.*_link_shims?\\.cpp$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/.*_shims?\\.cpp$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/stubs\\.cpp$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/.*_stub\\.cpp$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/stub_.*\\.cpp$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/shim_.*\\.cpp$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/.*_(mock|fake)\\.cpp$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/.*(mock_|fake_).*\\.cpp$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/.*_stubs\\.asm$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/stubs\\.asm$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/.*_stub\\.asm$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/stub_.*\\.asm$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/.*_shims?\\.asm$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/shim_.*\\.asm$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/.*_link_shims?\\.asm$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/.*_fallback[^/]*\\.asm$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/.*_(mock|fake)\\.asm$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/.*(mock_|fake_).*\\.asm$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/link_stubs_.*\\.cpp$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/link_stubs_.*\\.asm$")
        message(STATUS "[ProductionLane] Win32IDE: stripped ssot/fallback/shim compatibility units")
    endif()

    # Avoid mixing parallel GGUF engine stacks in this lane.
    list(REMOVE_ITEM WIN32IDE_SOURCES
        src/engine/sovereign_engines.cpp
        src/engine/rawr_engine.cpp
        src/engine/gguf_core.cpp
    )
    # Re-wire providers needed by this target's link graph.
    list(APPEND WIN32IDE_SOURCES src/core/local_parity_bridge.cpp)
    if(NOT RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
        if(MSVC)
            # Link closure: fallbacks for Win32IDE (rawrengine_missing_stubs.cpp is already in RawrEngine)
            set(_RAWRXD_WIN32IDE_FALLBACK_UNITS
                # src/core/asm_symbols_stub.cpp  # Removed - duplicates symbols from unlinked_symbols_batch_*.cpp
                src/core/json_instantiations.cpp
                src/core/cot_fallback_system.cpp
                # REMOVED: src/core/model_loader_fallbacks.cpp - stub file deleted
                # REMOVED: src/core/rawrxd_native_log_impl.cpp - stub file deleted
                # REMOVED: src/llm_adapter/ggufrunner_link_fallbacks.cpp - stub file deleted
                # REMOVED: src/inference/gguf_d3d12_bridge_link_fallback.cpp - stub file deleted
                # REMOVED: src/win32app/collab_cursor_fallbacks.cpp - stub file deleted
                # REMOVED: src/win32app/rtp_protocol_fallback.cpp - stub file deleted
                src/modules/quickjs_node_shims.cpp
            )
            foreach(_rawr_fallback IN LISTS _RAWRXD_WIN32IDE_FALLBACK_UNITS)
                if(EXISTS "${CMAKE_SOURCE_DIR}/${_rawr_fallback}")
                    list(APPEND WIN32IDE_SOURCES "${_rawr_fallback}")
                else()
                    message(WARNING "[Win32IDE] Optional fallback unit missing, skipping: ${_rawr_fallback}")
                endif()
            endforeach()
        else()
            message(STATUS "[Win32IDE] Non-MSVC fallback append skipped")
        endif()
    else()
        message(STATUS "[ProductionLane] Win32IDE: fallback provider append skipped")
    endif()

    list(APPEND WIN32IDE_SOURCES
        src/win32app/ai_workers_queue.cpp
        # v14.7.0 Native Track B Modules
        src/ui/ImportBridge.cpp
        src/ui/ExportBridge.cpp
        src/terminal/embedded_terminal.cpp
        # REMOVED: src/win32app/sovereign_gpu_link_stubs.cpp - stub file deleted
    )

    set(WIN32IDE_EXTRA_ASM "")
    if(RAWR_HAS_MASM)
        set(WIN32IDE_EXTRA_ASM
            src/asm/RawrXD_PE_Importer.asm
            src/asm/RawrXD_PE_Exporter.asm
            src/asm/ghost_text_ranker.asm
            src/asm/diff_engine.asm
        )

        # NOTE: Removed fraudulent GPU sovereign modules
        # These claimed capabilities that are physically impossible:
        # - RDNA3_MMIO_Recon.asm: Cannot reconstruct AMD MMIO without signed firmware/PSP
        # - Silicon_PUF_Signer.asm: PUF requires silicon fab access
        # - Silicon_Authenticator.asm: Same
        # - Neural_Entropy_Shield.asm: Marketing word salad
        # All src/asm/gpu/RawrXD_RDNA3_*.asm files removed from build.
        set(RAWRXD_HAS_SOVEREIGN_GPU_ASM 0)
        message(STATUS "[Win32IDE] Sovereign GPU ASM disabled — fraudulent claims removed")

        set_source_files_properties(${WIN32IDE_EXTRA_ASM} PROPERTIES LANGUAGE ASM_MASM)
    endif()

    # Gold Lane: prefer real implementations for Win32IDE agentic/autonomy path.
    # Default ON: without fallback TUs, Win32IDE hits 100+ LNK2001s (asm_*, RTP_*, GGUF stubs, etc.).
    # Production/CI can force -DRAWRXD_ALLOW_AGENTIC_STUB_FALLBACK=OFF when the full ASM graph is linked.
    option(RAWRXD_ALLOW_AGENTIC_STUB_FALLBACK
        "Allow stub/link-fallback units in RawrXD-Win32IDE agentic path"
        ON
    )
    if(NOT RAWRXD_ALLOW_AGENTIC_STUB_FALLBACK)
        message(WARNING
            "[Win32IDE] RAWRXD_ALLOW_AGENTIC_STUB_FALLBACK=OFF strips fallback/shim TUs (strict lane). "
            "The IDE usually will not link unless every ASM symbol is provided elsewhere. "
            "For a normal dev link: -DRAWRXD_ALLOW_AGENTIC_STUB_FALLBACK=ON or remove that cache entry.")
    endif()
    if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES AND RAWRXD_ALLOW_AGENTIC_STUB_FALLBACK)
        message(FATAL_ERROR "[ProductionLane] RAWRXD_ALLOW_AGENTIC_STUB_FALLBACK=ON is incompatible with RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON.")
    endif()
    if(RAWRXD_ALLOW_AGENTIC_STUB_FALLBACK)
        message(STATUS "[Win32IDE] Agentic stub fallback enabled by RAWRXD_ALLOW_AGENTIC_STUB_FALLBACK=ON")
    else()
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*_stubs\\.cpp$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/stubs\\.cpp$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*_stub\\.cpp$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/stub_.*\\.cpp$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/shim_.*\\.cpp$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/.*_(mock|fake)\\.cpp$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/.*(mock_|fake_).*\\.cpp$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*_stubs\\.asm$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/stubs\\.asm$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*_stub\\.asm$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/stub_.*\\.asm$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/.*_(mock|fake)\\.asm$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/.*(mock_|fake_).*\\.asm$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/.*_shims?\\.cpp$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/shim_.*\\.asm$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/.*_shims?\\.asm$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/reverse_engineered_stubs\\.cpp$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/link_stubs_.*\\.cpp$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/link_stubs_.*\\.asm$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/.*_link_shims?\\.cpp$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/.*_link_shims?\\.asm$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/.*_fallback[^/]*\\.cpp$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/.*_fallback[^/]*\\.asm$")

        list(REMOVE_ITEM WIN32IDE_SOURCES
            src/win32app/agentic_bridge_headless.cpp
            src/win32app/Win32IDE_headless_stubs.cpp
            src/core/ignite_800b.cpp
            src/stubs.cpp
            src/core/stubs.cpp
            src/core/universal_stub.cpp
            src/core/link_stubs_final.cpp
            src/core/link_stubs_remaining_classes.cpp
        )
        message(STATUS "[Win32IDE] Agentic real lane enforced (stub/link-fallback units removed)")
    endif()

    option(RAWRXD_STRICT_AGENTIC_REALITY
        "Fail CMake configure if Win32IDE sources still include stub/link-fallback units"
        ON
    )
    if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES AND NOT RAWRXD_STRICT_AGENTIC_REALITY)
        message(FATAL_ERROR "[ProductionLane] RAWRXD_STRICT_AGENTIC_REALITY=OFF is incompatible with RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON.")
    endif()
    if(RAWRXD_STRICT_AGENTIC_REALITY AND NOT RAWRXD_ALLOW_AGENTIC_STUB_FALLBACK)
        set(_RAWRXD_FORBIDDEN_WIN32IDE_SOURCES "")
        foreach(_rawrxd_src IN LISTS WIN32IDE_SOURCES)
            # ai_agent_masm_core_impl.cpp is required (masm_agent_failure_detect_simd), not a fallback
            if(NOT _rawrxd_src MATCHES "ai_agent_masm_core_impl\\.cpp$" AND _rawrxd_src MATCHES "(_stubs\\.cpp$|/stubs\\.cpp$|_stubs\\.asm$|/stubs\\.asm$|/.*_stub\\.cpp$|/.*_stub\\.asm$|/stub_.*\\.cpp$|/stub_.*\\.asm$|/shim_.*\\.(cpp|asm)$|/.*_(mock|fake)\\.(cpp|asm)$|/.*(mock_|fake_).*(\\.cpp|\\.asm)$|/.*_shims?\\.(cpp|asm)$|/link_stubs_.*\\.(cpp|asm)$|/.*_link_shims?\\.(cpp|asm)$|/.*_fallback[^/]*\\.(cpp|asm)$|/agentic_bridge_headless\\.cpp$|/Win32IDE_headless_stubs\\.cpp$)")
                list(APPEND _RAWRXD_FORBIDDEN_WIN32IDE_SOURCES "${_rawrxd_src}")
            endif()
        endforeach()
        if(_RAWRXD_FORBIDDEN_WIN32IDE_SOURCES)
            list(JOIN _RAWRXD_FORBIDDEN_WIN32IDE_SOURCES "\n  - " _RAWRXD_FORBIDDEN_LIST)
            message(FATAL_ERROR
                "[Win32IDE] STRICT AGENTIC REALITY gate failed. Forbidden fallback units detected:\n"
                "  - ${_RAWRXD_FORBIDDEN_LIST}\n"
                "Set RAWRXD_ALLOW_AGENTIC_STUB_FALLBACK=ON only for explicit fallback/debug builds."
            )
        endif()
    endif()

    # =========================================================================
    # Policy: Production builds MUST NOT link stub implementations
    # 
    # Strict enforcement: FATAL_ERROR unless emergency override set.
    # No stubs permitted; ai_agent_masm_core_impl.cpp is the only explicit exception
    # because it provides required MASM bridge symbols.
    # =========================================================================
    
    function(EnforceNoStubs target_name)
        get_target_property(SOURCES ${target_name} SOURCES)
        if(NOT SOURCES OR SOURCES STREQUAL "SOURCES-NOTFOUND")
            message(FATAL_ERROR "[ProductionLane] ${target_name} has no resolvable SOURCES property for stub enforcement.")
        endif()
        set(STUB_VIOLATIONS "")
        set(STUB_COUNT 0)
        
        foreach(src ${SOURCES})
            # Detect stub files
            if(src MATCHES "(_stubs\\.cpp$|/stubs\\.cpp$|_stub\\.cpp$|/stub_.*\\.cpp$|_stubs\\.asm$|/stubs\\.asm$|_stub\\.asm$|/stub_.*\\.asm$|/shim_.*\\.(cpp|asm)$|/missing_handler|/link_stubs_.*\\.(cpp|asm)$|_link_shims?\\.(cpp|asm)$|_fallback[^/]*\\.(cpp|asm)$|_(mock|fake)\\.(cpp|asm)$)" OR
               src MATCHES "(stub_|_stub\\.(cpp|asm)$|shim_|_shims?\\.(cpp|asm)$|shims?\\.(cpp|asm)$|/stubs\\.(cpp|asm)$)" OR
               src MATCHES "(mock_|fake_|_(mock|fake)\\.(cpp|asm)$)")
                                # ai_agent_masm_core_impl.cpp provides required MASM symbols — exempt
                                if(src MATCHES "ai_agent_masm_core_impl\\.cpp$")
                                    continue()
                                endif()
                list(APPEND STUB_VIOLATIONS "${src}")
                math(EXPR STUB_COUNT "${STUB_COUNT} + 1")
            endif()
        endforeach()
        
        # Enforce: No stubs in production
        if(STUB_COUNT GREATER 0)
            if(NOT DEFINED ENV{RAWRXD_EMERGENCY_STUB_LINK})
                list(REMOVE_DUPLICATES STUB_VIOLATIONS)
                list(JOIN STUB_VIOLATIONS "\n  - " STUB_LIST)
                message(FATAL_ERROR
                    "\\n===============================================================\\n"
                    "[PRODUCTION POLICY VIOLATION] ${target_name}\\n"
                    "Found ${STUB_COUNT} stub/shim/mock files:\\n"
                    "  - ${STUB_LIST}\\n"
                    "\\n"
                    "POLICY: Production builds must NOT link stubs.\\n"
                    "All functionality must have real implementations.\\n"
                    "\\n"
                    "OPTIONS:\\n"
                    "  1. Implement the missing functionality (preferred)\\n"
                    "  2. Exclude the feature from build (if not ready)\\n"
                    "  3. Emergency override: set RAWRXD_EMERGENCY_STUB_LINK=1\\n"
                    "     WARNING: CI will REJECT builds with this flag set\\n"
                    "===============================================================\\n"
                )
            else()
                message(WARNING "[EMERGENCY OVERRIDE] Stub files linked to ${target_name} (CI will reject)")
            endif()
        else()
            message(STATUS "✓ ${target_name}: No stub policy violations")
        endif()
    endfunction()
    
    # Verify link_stubs_gate.cpp size (must shrink as real implementations replace stubs)
    function(EnforceLinkStubsGateSizeLimit)
        set(RAWRXD_STUB_GATE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/core/link_stubs_gate.cpp")
        set(RAWRXD_STUB_GATE_MAX_LINES 400)
        
        if(EXISTS "${RAWRXD_STUB_GATE_FILE}")
            file(STRINGS "${RAWRXD_STUB_GATE_FILE}" STUB_GATE_LINES)
            list(LENGTH STUB_GATE_LINES STUB_GATE_LINE_COUNT)
            
            if(STUB_GATE_LINE_COUNT GREATER RAWRXD_STUB_GATE_MAX_LINES)
                message(FATAL_ERROR
                    "\\nlink_stubs_gate.cpp has grown to ${STUB_GATE_LINE_COUNT} lines.\\n"
                    "Maximum allowed: ${RAWRXD_STUB_GATE_MAX_LINES} lines\\n"
                    "\\n"
                    "POLICY: link_stubs_gate.cpp is temporary. Lines must shrink\\n"
                    "as real implementations replace stubs.\\n"
                    "\\n"
                    "Action: Replace stubs with real code or remove features.\\n"
                )
            endif()
        endif()
    endfunction()
    
    # Mandatory symbol providers for Win32IDE link closure.
    # When RAWRXD_ALLOW_AGENTIC_STUB_FALLBACK=ON, keep *_fallback*.cpp / link shims so the IDE links.
    # When OFF, strip generic stub/fallback units (strict lane).
    if(NOT RAWRXD_ALLOW_AGENTIC_STUB_FALLBACK)
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX "(_stubs\\.cpp$|/stubs\\.cpp$|_stub\\.cpp$|/stub_.*\\.cpp$|_stubs\\.asm$|/stubs\\.asm$|_stub\\.asm$|/stub_.*\\.asm$|/shim_.*\\.(cpp|asm)$|missing_handler|link_stubs_.*\\.(cpp|asm)$|_shims?\\.(cpp|asm)$|_link_shims?\\.(cpp|asm)$|_fallback[^/]*\\.(cpp|asm)$|_(mock|fake)\\.(cpp|asm)$|stub_|shim_|mock_|fake_)")
    endif()
    # Keep required MASM bridge provider after broad stub filtering.
    if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES AND NOT EXISTS "${CMAKE_SOURCE_DIR}/src/core/ai_agent_masm_core_impl.cpp")
        message(FATAL_ERROR "[ProductionLane] Required MASM bridge provider src/core/ai_agent_masm_core_impl.cpp is missing.")
    endif()
    list(REMOVE_ITEM WIN32IDE_SOURCES src/core/ai_agent_masm_core_impl.cpp)
    list(APPEND WIN32IDE_SOURCES src/core/ai_agent_masm_core_impl.cpp)


    # Pre-append strict guard: Verify no stubs were auto-added earlier
    if(RAWRXD_STRICT_AGENTIC_REALITY AND NOT RAWRXD_ALLOW_AGENTIC_STUB_FALLBACK)
        set(_RAWRXD_FORBIDDEN_WIN32IDE_SOURCES_POST "")
        foreach(_rawrxd_src IN LISTS WIN32IDE_SOURCES)
            if(_rawrxd_src MATCHES "(_stubs\\.cpp$|/stubs\\.cpp$|_stubs\\.asm$|/stubs\\.asm$|/.*_stub\\.(cpp|asm)$|/stub_.*\\.(cpp|asm)$|/shim_.*\\.(cpp|asm)$|/.*_(mock|fake)\\.(cpp|asm)$|/.*(mock_|fake_).*(\\.cpp|\\.asm)$|/link_stubs_.*\\.(cpp|asm)$|/.*_shims?\\.(cpp|asm)$|/.*_link_shims?\\.(cpp|asm)$|/.*_fallback[^/]*\\.(cpp|asm)$)" OR
               _rawrxd_src MATCHES "(missing_handler|/stubs\\.(cpp|asm)$|_stub\\.(cpp|asm)$|stub_|shim_|shims?\\.(cpp|asm)$|mock_|fake_|_(mock|fake)\\.(cpp|asm)$)")
                if(NOT _rawrxd_src MATCHES "ai_agent_masm_core_impl\\.cpp$")
                    list(APPEND _RAWRXD_FORBIDDEN_WIN32IDE_SOURCES_POST "${_rawrxd_src}")
                endif()
            endif()
        endforeach()
        if(_RAWRXD_FORBIDDEN_WIN32IDE_SOURCES_POST)
            list(JOIN _RAWRXD_FORBIDDEN_WIN32IDE_SOURCES_POST "\n  - " _RAWRXD_FORBIDDEN_LIST_POST)
            message(FATAL_ERROR
                "[Win32IDE] STRICT AGENTIC REALITY gate failed. Stubs detected:\\n"
                "  - ${_RAWRXD_FORBIDDEN_LIST_POST}\\n"
                "No stub/shim/mock files are allowed in production."
            )
        endif()
    endif()

    # v14.7.6 Batch 2 parity files — compilation fixes applied, re-enabled
    # (PeekView, AutoSave, TerminalSplit now compile cleanly)

    list(REMOVE_DUPLICATES WIN32IDE_SOURCES)

    # NOTE: Removed Transcendence fiction handlers (batch 36-42)
    # These were valuation theater - not implementable:
    # - Win32IDE_OmegaOrchestrator.cpp - "Autonomous development pipeline" - just a scheduler
    # - Win32IDE_MeshBrain.cpp - "Distributed consciousness" - word salad
    # - Win32IDE_SpeciatorEngine.cpp - "Metamorphic evolution" - genetic algo wrapper at best
    # - Win32IDE_NeuralBridge.cpp - "Direct cortex interface" - no Neuralink hardware
    # - Win32IDE_SelfHostEngine.cpp - "Recursive self-compilation" - needs working compiler first
    # - Win32IDE_HardwareSynthesizer.cpp - FPGA/ASIC codegen - no fab access
    # - Win32IDE_TranscendenceCoordinator.cpp - Orchestrates the above fiction
    # NOTE: Command handlers are in rawrengine_command_handlers.cpp
    # Removing duplicate handler files to avoid LNK2005 errors
    list(APPEND WIN32IDE_SOURCES
        # src/win32app/Win32IDE_CursorParityBridge.cpp  # Handlers in rawrengine_command_handlers.cpp
        # src/win32app/Win32IDE_OmegaOrchestrator.cpp   # Handlers in rawrengine_command_handlers.cpp
        # src/win32app/Win32IDE_MeshBrain.cpp           # Handlers in rawrengine_command_handlers.cpp
        # src/win32app/Win32IDE_SpeciatorEngine.cpp     # Handlers in rawrengine_command_handlers.cpp
        # src/win32app/Win32IDE_NeuralBridge.cpp        # Handlers in rawrengine_command_handlers.cpp
        # src/win32app/Win32IDE_SelfHostEngine.cpp      # Handlers in rawrengine_command_handlers.cpp
        # src/win32app/Win32IDE_HardwareSynthesizer.cpp # Handlers in rawrengine_command_handlers.cpp
        # src/win32app/Win32IDE_TranscendenceCoordinator.cpp # Handlers in rawrengine_command_handlers.cpp
        src/win32app/Win32IDE_TranscendencePanel.cpp
    )

    # Add UI & infrastructure handler files for batch 50-56
    list(APPEND WIN32IDE_SOURCES
        # NOTE: Win32IDE_CommandHandlers.cpp removed - CommandResult handlers already in rawrengine_command_handlers.cpp
        src/win32app/Win32IDE_AutonomousAgent.cpp
        src/win32app/Win32IDE_ChatMessageRenderer.cpp
        src/win32app/Win32IDE_ToolActionStatus.cpp
        src/win32app/Win32IDE_ChatPanel.cpp
        src/win32app/Win32IDE_ChatPanel_Ollama.cpp  # RESTORED: Required for HandleCopilotSend_Ollama and initializeChatPanelOllama
        src/win32app/Win32IDE_VulkanRenderer.cpp
        src/win32app/Win32IDE_OSExplorerInterceptor.cpp
        src/win32app/Win32IDE_IOCPFileWatcher.cpp
        src/win32app/Win32IDE_IDEDiagnosticAutoHealer.cpp
        src/win32app/Win32IDE_ConsentPrompt.cpp
        src/win32app/Win32IDE_PerfTelemetry.cpp
        src/win32app/Win32IDE_UpdateSignature.cpp
        src/win32app/Win32IDE_PluginSignature.cpp
        src/win32app/Win32IDE_EnterpriseStressTests.cpp
        src/win32app/Win32IDE_SQLite3Core.cpp
        src/win32app/Win32IDE_TelemetryExport.cpp
        src/win32app/Win32IDE_RefactoringPlugin.cpp
        src/win32app/Win32IDE_LanguagePlugin.cpp
        src/win32app/Win32IDE_ResourceGenerator.cpp
    )

    # Runtime surface — unity module (single TU) + GenerationStopwatch (separate TU: shared with model_runtime_gate)
    list(APPEND WIN32IDE_SOURCES
        src/runtime_surface/RuntimeSurfaceModule.cpp
        src/runtime_surface/GenerationStopwatch.cpp
    )

    # ── Performance-optimization batches 3–6 ───────────────────────────────────
    # Batch 3: Unified tiered memory pool (CPU / GPU / disk / compressed)
    # Batch 4: Work-stealing ExecutionScheduler (resolves friend-class declaration)
    # Batch 5: InferenceProfiler — per-layer profiling + Prometheus export
    # Batch 6: BackendOrchestrator — dynamic switching, sharding, semantic cache
    list(APPEND WIN32IDE_SOURCES
        src/memory/UnifiedMemoryPool.cpp
        src/memory/SequentialBlowoffValve.cpp
        src/inference/OutOfCoreScheduler.cpp
        src/inference/DualGpuPipeline.cpp
        src/inference/Deep2Engine.cpp
        src/kernels/VulkanComputeKernels.cpp
        src/InferenceProfiler.cpp
        src/BackendOrchestrator.cpp
        # --- Missing sources causing LNK2001 (added 2026-08-10) ---
        src/runtime/TensorExecutionRouter.cpp
        src/runtime/StreamRouterAdapter.cpp
        src/runtime/memory/TransferScheduler.cpp
        src/runtime/memory/PredictiveMemoryManager.cpp
        src/runtime/memory/ResidencyTracker.cpp
        src/runtime/memory/WorkingSetPredictor.cpp
        src/runtime/memory/CapacityManager.cpp
        src/runtime/memory/TensorPlacementManager.cpp
        B014/build/b014_profiler.cpp
        src/deep2/Deep2Engine.cpp
        src/deep2/KimiK2Config.cpp
        src/deep2/K2MLAWeights.cpp
        src/deep2/K2MoEWeights.cpp
        src/deep2/K2GlobalTensorIndex.cpp
        # Deep2 component files required by Deep2Engine.cpp
        src/deep2/ReverseIntegration.cpp
        src/deep2/ReverseHotpatchEngine.cpp
        src/deep2/mars/MARSController.cpp
        src/deep2/mars/TensorHotpatch.cpp
        src/deep2/mars/DualGPUBackend.cpp
        src/deep2/mars/VRAMManager.cpp
        src/deep2/KVCache.cpp
        src/deep2/GGUFLoader.cpp
        src/deep2/MoERouter.cpp
        src/deep2/MoEWeightProxy.cpp
        src/deep2/MoEWeightsLoader.cpp
        src/deep2/MedusaDecoder.cpp
        src/deep2/NUFusedPacker.cpp
        src/deep2/WarmupScheduler.cpp
        src/deep2/CompressedKVCache.cpp
        src/deep2/NVMeStream.cpp
        src/deep2/SlidingWindowEngine.cpp
        src/deep2/QuantKernelRegistry.cpp
        src/deep2/ThreadPool.cpp
        src/deep2/StreamRouter.cpp
        src/deep2/Deep2Bridge.cpp
        src/deep2/Deep2ExecutionGraph.cpp
        src/deep2/FusedInferenceKernel.cpp
        src/deep2/ShardRouterIntegration.cpp
        src/deep2/StreamEngine.cpp
        src/deep2/RuntimePlanner.cpp
        src/deep2/ModelLoader.cpp
        src/deep2/ModelRegistry.cpp
        src/deep2/DeepSeekMoELoader.cpp
        src/deep2/UniversalModelLoader.cpp
        src/deep2/RawrXDInferenceAdapter.cpp
        src/inference/RawrXD_LlamaNative.cpp
        src/deep2/InferenceSession.cpp
        src/deep2/TensorHop.cpp
        src/deep2/FabricTensorTable.cpp
        src/deep2/PatchCache.cpp
        src/deep2/TrailBrake.cpp
        src/deep2/HotPatcher.cpp
        src/deep2/HotPatcherSafety.cpp
        src/deep2/AntiPatcher.cpp
        src/deep2/GoalSystem.cpp
        src/deep2/BottleTTL.cpp
        src/deep2/CPUFrequency.cpp
        src/deep2/DualGPUHook.cpp
        src/deep2/ReverseTensorRecovery.cpp
        src/deep2/ScaleQuality.cpp
        src/deep2/MoEArchitectureParser.cpp
        src/deep2/TheDualityExample.cpp
        src/deep2/Deep2Discovery.cpp
        src/deep2/Deep2Benchmark.cpp
        src/deep2/Deep2InferenceEndpoint.cpp
        src/deep2/Deep2InferenceGateway.cpp
        src/deep2/Deep2Integration.cpp
        src/deep2/Deep2LocalServer.cpp
        src/deep2/Deep2Server_Minimal.cpp
        src/deep2/Deep2Server_Sovereign.cpp
        src/deep2/Deep2APIServer.cpp
        src/deep2/Deep2APIServer_Complete.cpp
        src/deep2/GGUFDiagnostics.cpp
        src/deep2/GGUFLoader_Fixed.cpp
        src/deep2/GGUFVerifier.cpp
        src/deep2/IQQuantKernels.cpp
        src/deep2/MASMKernelStubs.cpp
        src/deep2/mcp_bridge.cpp
        src/deep2/gpu/Deep2GPUBackend.cpp
        src/deep2/MultiGPU/GPUDeviceRegistry.cpp
        src/deep2/MultiGPU/MultiGPUScheduler.cpp
        src/deep2/MultiGPU/VRAMAllocator.cpp
        src/deep2/production/Deep2ProductionRuntime.cpp
        src/deep2/dump_tensors.cpp
        src/deep2/deep2_http_gateway.cpp
        src/deep2/Deep2Engine_KernelTest.cpp
        src/deep2/moe_microbench.cpp
        src/deep2/moe_simple_bench.cpp
        src/deep2/moe_test.cpp
        src/deep2/moe_validation_test.cpp
        src/deep2/router_bench.cpp
        src/deep2/router_latency_test.cpp
        src/deep2/test_api_server.cpp
        src/deep2/test_real_gguf_load.cpp
        src/deep2/test_real_gguf_validate.cpp
        src/deep2/test_tool_limit_hotpatch.cpp
        src/deep2/VAL063_Deep2Certification.cpp
        src/deep2/VAL038_Benchmark_Harness.cpp
        src/deep2/deep2_end_to_end_bench.cpp
        src/deep2/deep2_moe_bench_standalone.cpp
        src/deep2/deep2_benchmark_main.cpp
        src/deep2/deep2_server_main.cpp
        src/deep2/Deep2APIServer_Complete.cpp
        src/deep2/ModelRegistry/GGUFModelRegistry.cpp
    )

    # Deep2Engine Integration Test
    add_executable(Deep2Engine_IntegrationTest
        src/tests/Deep2Engine_IntegrationTest_Minimal.cpp
        src/memory/SequentialBlowoffValve.cpp
        src/inference/OutOfCoreScheduler.cpp
        src/inference/DualGpuPipeline.cpp
        src/inference/Deep2Engine.cpp
        src/kernels/VulkanComputeKernels.cpp
    )
    target_include_directories(Deep2Engine_IntegrationTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/memory
        ${CMAKE_CURRENT_SOURCE_DIR}/src/inference
        ${CMAKE_CURRENT_SOURCE_DIR}/src/kernels
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    if(RAWR_ENABLE_VULKAN AND RAWR_VULKAN_SDK_ROOT)
        target_include_directories(Deep2Engine_IntegrationTest PRIVATE ${RAWR_VULKAN_SDK_ROOT}/Include)
        target_link_directories(Deep2Engine_IntegrationTest PRIVATE ${RAWR_VULKAN_SDK_ROOT}/Lib)
        target_link_libraries(Deep2Engine_IntegrationTest PRIVATE vulkan-1)
    endif()

    # ============================================================================
    # DeepSeek2 Metadata Regression Test (K2-001)
    # Loads real K2 GGUF metadata-only and asserts ground-truth values.
    # Must pass before K2-002 bounded tensor execution.
    # ============================================================================
    add_executable(Deep2Engine_MetadataRegressionTest
        src/deep2/Deep2Engine_MetadataRegressionTest.cpp
        src/deep2/GGUFLoader.cpp
    )
    target_include_directories(Deep2Engine_MetadataRegressionTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/deep2
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_compile_features(Deep2Engine_MetadataRegressionTest PRIVATE cxx_std_17)
    set_target_properties(Deep2Engine_MetadataRegressionTest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    message(STATUS "Deep2Engine Metadata Regression Test configured")

    # ── SDMA GPU Scheduler (C++ replacement for MASM) ───────────────────────
    # Replaced broken MASM syntax with portable C++ implementations
    # sdma_scheduler: GPU DMA event loop, burst coalescing, TSC-based scheduling
    # sdma_ring_allocator: O(1) tensor slot bitmap allocator with spinlock
    list(APPEND WIN32IDE_SOURCES
        src/core/sdma/sdma_scheduler.cpp
        src/core/sdma/sdma_ring_allocator.cpp
    )

    # Core workflow/reasoning engines required by unified_hotpatch_manager and agentic_task_graph
    list(APPEND WIN32IDE_SOURCES
        src/core/autonomous_workflow_engine.cpp
        src/core/workspace_reasoning_profiles.cpp
        src/core/reasoning_schema_versioning.cpp
        src/core/safe_refactor_engine.cpp
    )

    # Sovereign Coordination System - 10 primitives + Execution Capsule
    list(APPEND WIN32IDE_SOURCES
        src/sovereign/spine/ExecutionSpine.cpp
        src/sovereign/build/BuildStateGraph.cpp
        src/sovereign/ide/SovereignIDEIntegration.cpp
        src/sovereign/terminal/TerminalOwnership.cpp
        src/sovereign/bus/CapabilityBus.cpp
        src/sovereign/bus/BeaconBus.cpp
        src/sovereign/awareness/SystemAwareness.cpp
        src/sovereign/validator/RealityValidator.cpp
        src/sovereign/recovery/AutonomousRecovery.cpp
        src/sovereign/control/SovereignControlPlane.cpp
        src/sovereign/compression/IntentCompression.cpp
        src/sovereign/capsule/ExecutionCapsule.cpp
    )

    # Sovereign Autonomy Stack - Blackboard, TaskGraph, MissionPlanner, Reflection, Critic, Runtime
    list(APPEND WIN32IDE_SOURCES
        src/sovereign_autonomy/SovereignBlackboard.cpp
        src/sovereign_autonomy/SovereignTaskGraph.cpp
        src/sovereign_autonomy/SovereignMissionPlanner.cpp
        src/sovereign_autonomy/SovereignReflectionEngine.cpp
        src/sovereign_autonomy/SovereignCritic.cpp
        src/sovereign_autonomy/SovereignAgentRuntime.cpp
    )

    # ═══════════════════════════════════════════════════════════════
    # OMEGA-1 v2.0 IDE Integration - IPC Layer for Ghost Text & Telemetry
    # ═══════════════════════════════════════════════════════════════
    list(APPEND WIN32IDE_SOURCES
        src/win32ide/Omega1IPCClient.cpp
        src/win32ide/GhostTextRenderer.cpp
        src/win32ide/StatusBarTelemetry.cpp
        src/win32ide/Omega1IDEIntegration.cpp
        src/win32ide/Win32IDE_Omega1Integration.cpp
    )
    
    # ═══════════════════════════════════════════════════════════════
    # Elastic Engine — Predictive tensor residency + governance
    # ═══════════════════════════════════════════════════════════════
    list(APPEND WIN32IDE_SOURCES
        src/runtime/elastic/ElasticEngine.cpp
        src/runtime/elastic/ElasticGGUFIndex.cpp
        src/runtime/elastic/ElasticResidencyManager.cpp
        src/runtime/elastic/VulkanTensorResidencyBackend.cpp
        src/runtime/elastic/ElasticDecoder.cpp
        src/runtime/elastic/ElasticArchitectureDetector.cpp
        src/runtime/shared/SharedModelRuntime.cpp
        src/runtime/shared/BP1BraidIndex.cpp
        src/runtime/shared/BP1BraidStreamer.cpp
        src/runtime/governance/HardwareCapabilityProbe.cpp
        src/runtime/governance/CapabilityTierMatrix.cpp
        src/runtime/governance/HardwareGovernor.cpp
        src/runtime/governance/PressedStateEngine.cpp
        src/runtime/governance/Ensemble58Governor.cpp
        src/runtime/governance/TLSInstructionGate.cpp
        src/runtime/governance/UnifiedTriggerOrchestrator.cpp
        src/runtime/memory/CapacityManager.cpp
        src/runtime/memory/PlacementPolicy.cpp
        src/runtime/memory/ResidencyTracker.cpp
        src/runtime/memory/TensorPlacementManager.cpp
        src/runtime/memory/TransferScheduler.cpp
        src/runtime/memory/PredictiveMemoryManager.cpp
        src/runtime/memory/WorkingSetPredictor.cpp
        src/runtime/Deep2ExecutionTelemetry.cpp
        src/backend/VulkanGemmDispatcher.cpp
        src/runtime/gguf_tensor_loader.cpp
        src/runtime/GenerationBouncer.cpp
        src/runtime/TensorExecutionRouter.cpp
        src/runtime/StreamRouterAdapter.cpp
        src/runtime/Sovereign_KernelBridge_CPP.cpp
    )

    # ═══════════════════════════════════════════════════════════════
    # OMEGA-1 Engine Server - IPC Server for IDE integration
    # ═══════════════════════════════════════════════════════════════
    list(APPEND RAWR_ENGINE_SOURCES
        src/engine/Omega1Engine_Server.cpp
    )

    add_executable(RawrXD-Win32IDE WIN32 ${WIN32IDE_SOURCES} ${ASM_KERNEL_SOURCES} ${WIN32IDE_EXTRA_ASM})

    # Set MSVC runtime library for Win32IDE (static CRT to match RawrEngine)
    set_property(TARGET RawrXD-Win32IDE PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    # GPU sovereign ASM link state (UI/runtime detection)
    if(DEFINED RAWRXD_HAS_SOVEREIGN_GPU_ASM)
        target_compile_definitions(RawrXD-Win32IDE PRIVATE RAWRXD_HAS_SOVEREIGN_GPU_ASM=${RAWRXD_HAS_SOVEREIGN_GPU_ASM})
    else()
        target_compile_definitions(RawrXD-Win32IDE PRIVATE RAWRXD_HAS_SOVEREIGN_GPU_ASM=0)
    endif()

    # Ensure link output dir exists; clear prior exe/pdb before link (mitigates MSVC LNK1104 when
    # a stale handle, indexer, or crashed link left the binary busy — harmless if files are absent).
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    # Use CMake-native removal to avoid shell quoting issues in generated link commands.
    if(MSVC)
        add_custom_command(TARGET RawrXD-Win32IDE PRE_LINK
            COMMAND ${CMAKE_COMMAND} -E rm -f "$<TARGET_FILE:RawrXD-Win32IDE>" "$<TARGET_PDB_FILE:RawrXD-Win32IDE>"
            COMMENT "RawrXD-Win32IDE: PRE_LINK optional del exe/pdb if unlocked"
            VERBATIM
        )
    endif()

    # =========================================================================
    # PRODUCTION POLICY ENFORCEMENT
    # =========================================================================
    if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
        EnforceNoStubs(RawrEngine)
        EnforceNoStubs(RawrXD_Gold)
        EnforceNoStubs(RawrXD-InferenceEngine)
    endif()
    # Enforce both: (1) No stubs linked, (2) link_stubs_gate.cpp shrinking.
    # Explicit fallback builds may retain compatibility units by design.
    if(RAWRXD_ALLOW_AGENTIC_STUB_FALLBACK)
        message(STATUS "[Win32IDE] Stub policy enforcement relaxed (RAWRXD_ALLOW_AGENTIC_STUB_FALLBACK=ON)")
    else()
        EnforceNoStubs(RawrXD-Win32IDE)
        EnforceLinkStubsGateSizeLimit()
    endif()

    # External infra pack is mirrored into the project tree under D: and wired
    # as include/lib providers for Win32IDE.
    set(RAWRXD_INFRA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/rawr_infra")
    target_include_directories(RawrXD-Win32IDE PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Ship"
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/masm"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/sovereign_autonomy"
    )
    if(EXISTS "${RAWRXD_INFRA_ROOT}/Headers")
        target_include_directories(RawrXD-Win32IDE PRIVATE
            "${RAWRXD_INFRA_ROOT}/Headers"
            "${RAWRXD_INFRA_ROOT}/Source"
        )
        target_compile_definitions(RawrXD-Win32IDE PRIVATE RAWRXD_INFRA_AVAILABLE=1)
        if(EXISTS "${RAWRXD_INFRA_ROOT}/Libraries")
            target_link_directories(RawrXD-Win32IDE PRIVATE "${RAWRXD_INFRA_ROOT}/Libraries")
        endif()
        message(STATUS "[Infra] Win32IDE wired to ${RAWRXD_INFRA_ROOT}")
    else()
        target_compile_definitions(RawrXD-Win32IDE PRIVATE RAWRXD_INFRA_AVAILABLE=0)
    endif()

    # MAP output for link graph verification
    if(MSVC)
        set(_IDE_MAP_PATH "${CMAKE_BINARY_DIR}/bin/RawrXD-Win32IDE.map")
        set(_IDE_MAP_AUDIT_PATH "${CMAKE_BINARY_DIR}/RawrXD-Win32IDE.map")
        target_link_options(RawrXD-Win32IDE PRIVATE
            /LARGEADDRESSAWARE:NO
            /DEBUG:FULL
            "/MAP:${_IDE_MAP_PATH}"
            /MAPINFO:EXPORTS
            /MANIFEST:NO
            /FORCE:MULTIPLE
        )
        # Mirror the map at the build root for audit tools that scan ${CMAKE_BINARY_DIR}.
        add_custom_command(TARGET RawrXD-Win32IDE POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_IDE_MAP_PATH}" "${_IDE_MAP_AUDIT_PATH}"
            VERBATIM
        )
    endif()

    # Runtime feature gates for this target's source graph.
    target_compile_definitions(RawrXD-Win32IDE PRIVATE
        UNICODE
        _UNICODE
        RAWRXD_STRICT_PRODUCTION_PROFILE=$<IF:$<BOOL:${RAWRXD_PRODUCTION_STRIP_STUB_SOURCES}>,1,0>
        RAWR_HAS_SOVEREIGN_ENGINES=0
        RAWR_HAS_RAWR_INFERENCE=0
        RAWRXD_HAS_MW_KERNEL=${RAWRXD_HAS_MW_KERNEL_FLAG}
        RAWRXD_HAS_PROMPT_ENGINE=${RAWRXD_HAS_PROMPT_ENGINE_FLAG}
        RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=$<IF:$<BOOL:${RAWRXD_PRODUCTION_STRIP_STUB_SOURCES}>,1,0>
        ${_RAWR_ASM_FEATURE_DEFINITIONS}
        RAWRXD_USE_DEDICATED_CODELENS=1
        RAWRXD_USE_DEDICATED_INLAY=1
        RAWRXD_USE_DEDICATED_HOVER=1
        RAWRXD_USE_DEDICATED_MINIMAP=1
        RAWRXD_DISABLE_DUPLICATE_SHIMS=1
    )
    if(RAWR_HAS_MASM)
        target_compile_definitions(RawrXD-Win32IDE PRIVATE HAS_BRUTAL_GZIP_MASM=1)
        target_compile_definitions(RawrXD-Win32IDE PRIVATE RAWRXD_MASM_GLOBALS_LINKED=1)
    endif()

    # =========================================================================
    # Phase 36: QuickJS Static Library (embedded JS runtime)
    # =========================================================================
    # QuickJS sources should be placed in 3rdparty/quickjs/
    # If not present, create stub target that allows compilation without QuickJS
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/quickjs_ng/quickjs.c")
        set(QUICKJS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/quickjs_ng)
    else()
        set(QUICKJS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/quickjs)
    endif()
    if(EXISTS "${QUICKJS_DIR}/quickjs.c")
        set(QUICKJS_SOURCES
            ${QUICKJS_DIR}/quickjs.c
            ${QUICKJS_DIR}/libregexp.c
            ${QUICKJS_DIR}/libunicode.c
            ${QUICKJS_DIR}/quickjs-libc.c
        )
        if(EXISTS "${QUICKJS_DIR}/cutils.c")
            list(APPEND QUICKJS_SOURCES ${QUICKJS_DIR}/cutils.c)
        endif()
        set(QUICKJS_HAS_LIBBF OFF)
        if(EXISTS "${QUICKJS_DIR}/libbf.c")
            list(APPEND QUICKJS_SOURCES ${QUICKJS_DIR}/libbf.c)
            set(QUICKJS_HAS_LIBBF ON)
        elseif(EXISTS "${QUICKJS_DIR}/dtoa.c")
            list(APPEND QUICKJS_SOURCES ${QUICKJS_DIR}/dtoa.c)
        endif()

        add_library(quickjs_static STATIC ${QUICKJS_SOURCES})
        # Set MSVC runtime library to match RawrEngine (static CRT)
        set_property(TARGET quickjs_static PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        set_target_properties(quickjs_static PROPERTIES
            C_STANDARD 11
            C_STANDARD_REQUIRED ON
        )
        target_include_directories(quickjs_static PUBLIC ${QUICKJS_DIR})
        if(MSVC)
            target_compile_options(quickjs_static PRIVATE
                /W0
                /experimental:c11atomics
                $<$<NOT:$<CONFIG:Debug>>:/O2>
            )
            if(QUICKJS_HAS_LIBBF)
                target_compile_definitions(quickjs_static PRIVATE
                    _CRT_SECURE_NO_WARNINGS
                    CONFIG_BIGNUM
                    CONFIG_VERSION="2024.01.13"
                )
            else()
                target_compile_definitions(quickjs_static PRIVATE
                    _CRT_SECURE_NO_WARNINGS
                    CONFIG_VERSION="2024.01.13"
                )
            endif()
        else()
            target_compile_options(quickjs_static PRIVATE -w -O2)
            if(QUICKJS_HAS_LIBBF)
                target_compile_definitions(quickjs_static PRIVATE
                    CONFIG_BIGNUM
                    CONFIG_VERSION="2024.01.13"
                    _GNU_SOURCE
                )
            else()
                target_compile_definitions(quickjs_static PRIVATE
                    CONFIG_VERSION="2024.01.13"
                    _GNU_SOURCE
                )
            endif()
        endif()
        target_link_libraries(RawrXD-Win32IDE PRIVATE quickjs_static)
        if(TARGET RawrEngine)
            target_link_libraries(RawrEngine PRIVATE quickjs_static)
            target_include_directories(RawrEngine PRIVATE ${QUICKJS_DIR}/..)
        endif()
        if(TARGET RawrXD_Gold)
            target_link_libraries(RawrXD_Gold PRIVATE quickjs_static)
            target_include_directories(RawrXD_Gold PRIVATE ${QUICKJS_DIR}/..)
        endif()
        if(TARGET InferenceEngine)
            target_link_libraries(InferenceEngine PRIVATE quickjs_static)
            target_include_directories(InferenceEngine PRIVATE ${QUICKJS_DIR}/..)
        endif()
        if(TARGET RawrXD-InferenceEngine)
            target_link_libraries(RawrXD-InferenceEngine PRIVATE quickjs_static)
            target_include_directories(RawrXD-InferenceEngine PRIVATE ${QUICKJS_DIR}/..)
        endif()
        target_compile_definitions(RawrXD-Win32IDE PRIVATE RAWR_HAS_QUICKJS=1)
        if(TARGET RawrEngine)
            target_compile_definitions(RawrEngine PRIVATE RAWR_HAS_QUICKJS=1)
        endif()
        if(TARGET RawrXD_Gold)
            target_compile_definitions(RawrXD_Gold PRIVATE RAWR_HAS_QUICKJS=1)
        endif()
        if(TARGET InferenceEngine)
            target_compile_definitions(InferenceEngine PRIVATE RAWR_HAS_QUICKJS=1)
        endif()
        if(TARGET RawrXD-InferenceEngine)
            target_compile_definitions(RawrXD-InferenceEngine PRIVATE RAWR_HAS_QUICKJS=1)
        endif()
        target_compile_definitions(RawrXD-Win32IDE PRIVATE RAWR_QUICKJS_STUB=0)
        if(TARGET RawrEngine)
            target_compile_definitions(RawrEngine PRIVATE RAWR_QUICKJS_STUB=0)
        endif()
        if(TARGET RawrXD_Gold)
            target_compile_definitions(RawrXD_Gold PRIVATE RAWR_QUICKJS_STUB=0)
        endif()
        if(TARGET InferenceEngine)
            target_compile_definitions(InferenceEngine PRIVATE RAWR_QUICKJS_STUB=0)
        endif()
        if(TARGET RawrXD-InferenceEngine)
            target_compile_definitions(RawrXD-InferenceEngine PRIVATE RAWR_QUICKJS_STUB=0)
        endif()
        message(STATUS "QuickJS found: JS extension host enabled")
    else()
        if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
            message(FATAL_ERROR "[ProductionLane] QuickJS not found at ${QUICKJS_DIR} and Win32IDE would require QuickJS stubs; strict production strip requires a real QuickJS provider.")
        endif()
        message(STATUS "QuickJS not found at ${QUICKJS_DIR} — JS extension host uses stubs")
        # Create stub include directory so headers compile
        target_compile_definitions(RawrXD-Win32IDE PRIVATE RAWR_QUICKJS_STUB=1)
        target_compile_definitions(RawrXD-Win32IDE PRIVATE RAWR_HAS_QUICKJS=0)
        if(TARGET RawrEngine)
            target_compile_definitions(RawrEngine PRIVATE RAWR_HAS_QUICKJS=0)
            target_compile_definitions(RawrEngine PRIVATE RAWR_QUICKJS_STUB=1)
        endif()
        if(TARGET RawrXD_Gold)
            target_compile_definitions(RawrXD_Gold PRIVATE RAWR_HAS_QUICKJS=0)
            target_compile_definitions(RawrXD_Gold PRIVATE RAWR_QUICKJS_STUB=1)
        endif()
        if(TARGET InferenceEngine)
            target_compile_definitions(InferenceEngine PRIVATE RAWR_HAS_QUICKJS=0)
            target_compile_definitions(InferenceEngine PRIVATE RAWR_QUICKJS_STUB=1)
        endif()
        if(TARGET RawrXD-InferenceEngine)
            target_compile_definitions(RawrXD-InferenceEngine PRIVATE RAWR_HAS_QUICKJS=0)
            target_compile_definitions(RawrXD-InferenceEngine PRIVATE RAWR_QUICKJS_STUB=1)
        endif()
    endif()
    
    # Link RE library into Win32IDE
    target_link_libraries(RawrXD-Win32IDE PRIVATE RawrXD-RE-Library)
    
    # Link MultiWindow Kernel into Win32IDE
    if(TARGET RawrXD_MultiWindow_Kernel)
        target_link_libraries(RawrXD-Win32IDE PRIVATE RawrXD_MultiWindow_Kernel)
        target_compile_definitions(RawrXD-Win32IDE PRIVATE RAWRXD_HAS_MW_KERNEL=1)
    endif()
    
    # Link NanoQuant into Win32IDE
    if(TARGET RawrXD_NanoQuant)
        target_link_libraries(RawrXD-Win32IDE PRIVATE RawrXD_NanoQuant)
        target_compile_definitions(RawrXD-Win32IDE PRIVATE RAWR_HAS_NANOQUANT=1)
    endif()
    
    # Vulkan SDK for Win32IDE
    if(Vulkan_FOUND)
        target_include_directories(RawrXD-Win32IDE PRIVATE ${Vulkan_INCLUDE_DIRS})
        target_link_libraries(RawrXD-Win32IDE PRIVATE Vulkan::Vulkan)
        target_compile_definitions(RawrXD-Win32IDE PRIVATE RAWR_HAS_VULKAN=1)
    elseif(_VK_SDK_DIRS)
        target_include_directories(RawrXD-Win32IDE PRIVATE "${_VK_SDK_DIR}/Include")
        target_link_directories(RawrXD-Win32IDE PRIVATE "${_VK_SDK_DIR}/Lib")
        target_link_libraries(RawrXD-Win32IDE PRIVATE vulkan-1)
        target_compile_definitions(RawrXD-Win32IDE PRIVATE RAWR_HAS_VULKAN=1)
    else()
        target_compile_definitions(RawrXD-Win32IDE PRIVATE RAWR_HAS_VULKAN=0)
    endif()

    option(RAWRXD_LINK_WIN32IDE_MONOLITHIC_OBJS
        "Link optional obj/asm_monolithic_*.obj blobs into Win32IDE (advanced/dev only; may require extra providers)"
        OFF
    )

    set(_WIN32IDE_LINKED_MONOLITHIC_OBJS "")
    # Monolithic obj packs overlap with C++ fallback providers and can easily cause
    # duplicate symbols / missing-provider symbols. Only honor the option when
    # the agentic stub fallback lane is disabled.
    set(_WIN32IDE_USE_MONOLITHIC_OBJS ${RAWRXD_LINK_WIN32IDE_MONOLITHIC_OBJS})
    if(RAWRXD_ALLOW_AGENTIC_STUB_FALLBACK AND _WIN32IDE_USE_MONOLITHIC_OBJS)
        message(WARNING "[Win32IDE] Ignoring RAWRXD_LINK_WIN32IDE_MONOLITHIC_OBJS=ON because RAWRXD_ALLOW_AGENTIC_STUB_FALLBACK=ON (would cause duplicate symbols). Disable stub fallback to use monolithic objs.")
        set(_WIN32IDE_USE_MONOLITHIC_OBJS OFF)
    endif()

    if(_WIN32IDE_USE_MONOLITHIC_OBJS)
        set(_WIN32IDE_OPTIONAL_MONOLITHIC_OBJS
            "${CMAKE_CURRENT_SOURCE_DIR}/obj/asm_monolithic_beacon.obj"
            "${CMAKE_CURRENT_SOURCE_DIR}/obj/asm_monolithic_model_loader.obj"
            "${CMAKE_CURRENT_SOURCE_DIR}/obj/asm_monolithic_inference.obj"
        )
        foreach(_obj IN LISTS _WIN32IDE_OPTIONAL_MONOLITHIC_OBJS)
            if(EXISTS "${_obj}")
                list(APPEND _WIN32IDE_LINKED_MONOLITHIC_OBJS "${_obj}")
            else()
                message(STATUS "Optional Win32IDE link object missing, skipping: ${_obj}")
            endif()
        endforeach()

        # If monolithic objects are linked, avoid also compiling/linking the overlapping C++ fallback provider.
        if(_WIN32IDE_LINKED_MONOLITHIC_OBJS)
            list(REMOVE_ITEM WIN32IDE_SOURCES src/core/model_loader_fallbacks.cpp)
            message(STATUS "[Win32IDE] Monolithic ASM objects linked — removing src/core/model_loader_fallbacks.cpp to avoid duplicate symbols")
        endif()
    else()
        message(STATUS "[Win32IDE] Monolithic ASM obj pack disabled (RAWRXD_LINK_WIN32IDE_MONOLITHIC_OBJS=OFF)")
    endif()

    target_link_libraries(RawrXD-Win32IDE PRIVATE
        Threads::Threads
        comctl32 comdlg32 shell32 ole32 oleaut32 uuid
        shlwapi psapi dbghelp winhttp ws2_32 winmm gdi32 user32 ntdll
        d2d1 dwrite d3d11 d3d12 dcomp d3dcompiler dwmapi
        dxgi
        advapi32 crypt32 bcrypt wintrust
        opengl32 wininet
        pdh
        ${MASM_OBJECTS}
        ${_WIN32IDE_LINKED_MONOLITHIC_OBJS}
        nlohmann_json::nlohmann_json
    )

    # Explicit nlohmann/json include for multi-header FetchContent layout
    target_include_directories(RawrXD-Win32IDE PRIVATE
        "${nlohmann_json_SOURCE_DIR}/include"
    )

    if(NOT MSVC)
        target_link_libraries(RawrXD-Win32IDE PRIVATE gomp)
    endif()

    # Ensure MASM kernels build before Win32IDE
    if(TARGET masm_kernels)
        add_dependencies(RawrXD-Win32IDE masm_kernels)
    endif()
    
    # Link RE deobfuscator MASM objects if available
    if(TARGET re_deobfuscator)
        add_dependencies(RawrXD-Win32IDE re_deobfuscator)
        target_link_libraries(RawrXD-Win32IDE PRIVATE ${RE_DEOBF_OBJECTS})
    endif()
    # Skip RE_MODEL_OBJECTS (model_llm_harvester.asm) — it defines main() and conflicts with WinMain
    # if(TARGET re_model_reverse)
    #     add_dependencies(RawrXD-Win32IDE re_model_reverse)
    #     target_link_libraries(RawrXD-Win32IDE PRIVATE ${RE_MODEL_OBJECTS})
    # endif()
    
    target_include_directories(RawrXD-Win32IDE PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/engine
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core
        ${CMAKE_CURRENT_SOURCE_DIR}/src/server
        ${CMAKE_CURRENT_SOURCE_DIR}/src/agent
        ${CMAKE_CURRENT_SOURCE_DIR}/src/agentic
        ${CMAKE_CURRENT_SOURCE_DIR}/src/cli
        ${CMAKE_CURRENT_SOURCE_DIR}/src/win32app
        ${CMAKE_CURRENT_SOURCE_DIR}/src/modules
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
        ${CMAKE_CURRENT_SOURCE_DIR}/src/config
        ${CMAKE_CURRENT_SOURCE_DIR}/src/asm
        ${CMAKE_CURRENT_SOURCE_DIR}/src/lsp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/plugin_system
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ide
        ${CMAKE_CURRENT_SOURCE_DIR}/src/llm_adapter
        ${CMAKE_CURRENT_SOURCE_DIR}/src/auth
        ${CMAKE_CURRENT_SOURCE_DIR}/src/context
        ${CMAKE_CURRENT_SOURCE_DIR}/src/multimodal_engine
        ${CMAKE_CURRENT_SOURCE_DIR}/src/telemetry
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
        ${CMAKE_CURRENT_SOURCE_DIR}/src/git
        ${CMAKE_CURRENT_SOURCE_DIR}/src/sovereign
        ${CMAKE_CURRENT_SOURCE_DIR}/src/sovereign/spine
        ${CMAKE_CURRENT_SOURCE_DIR}/src/sovereign/terminal
        ${CMAKE_CURRENT_SOURCE_DIR}/src/sovereign/build
        ${CMAKE_CURRENT_SOURCE_DIR}/src/sovereign/agent
        ${CMAKE_CURRENT_SOURCE_DIR}/src/sovereign/bus
        ${CMAKE_CURRENT_SOURCE_DIR}/src/sovereign/compression
        ${CMAKE_CURRENT_SOURCE_DIR}/src/sovereign/awareness
        ${CMAKE_CURRENT_SOURCE_DIR}/src/sovereign/validator
        ${CMAKE_CURRENT_SOURCE_DIR}/src/sovereign/recovery
        ${CMAKE_CURRENT_SOURCE_DIR}/src/sovereign/control
        ${CMAKE_CURRENT_SOURCE_DIR}/src/sovereign/capsule
        ${CMAKE_CURRENT_SOURCE_DIR}/src/sovereign/ide
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    if(MSVC)
        target_compile_options(RawrXD-Win32IDE PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:/EHsc /W0 /arch:AVX2 /Zi>
            # Avoid /RTC1 + /O2 conflict in Debug (CMake injects /RTC1 for Debug by default).
            $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CONFIG:Debug>>>:/O2>
        )
    else()
        target_compile_options(RawrXD-Win32IDE PRIVATE -O3 -w -mavx2 -mfma -mavx512f -mavx512bw -mavx512dq -mavx512vl -march=native -fopenmp)
    endif()

    set_target_properties(RawrXD-Win32IDE PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY bin
        OUTPUT_NAME RawrXD-Win32IDE
        PDB_NAME RawrXD-Win32IDE
        PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        COMPILE_PDB_NAME RawrXD-Win32IDE
        COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
    )
    
    # Increase stack size to 4MB to prevent stack overflow during window creation
    # The default 1MB stack is insufficient for the deep call chains in onCreate
    if(MSVC)
        target_link_options(RawrXD-Win32IDE PRIVATE "/STACK:4194304")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_link_options(RawrXD-Win32IDE PRIVATE "-Wl,--stack,4194304")
    endif()
    # RawrCodex.asm uses ADDR32 relocations — /LARGEADDRESSAWARE:NO already set in MAP block above

    # Final link-line hygiene: remove duplicate library/object entries that
    # otherwise trigger LNK4042 warnings for repeated prebuilt MASM objects.
    foreach(_rawrxd_link_target RawrEngine RawrXD_Gold RawrXD-Win32IDE)
        if(TARGET ${_rawrxd_link_target})
            get_target_property(_rawrxd_link_libs ${_rawrxd_link_target} LINK_LIBRARIES)
            if(_rawrxd_link_libs)
                list(REMOVE_DUPLICATES _rawrxd_link_libs)
                set_target_properties(${_rawrxd_link_target} PROPERTIES
                    LINK_LIBRARIES "${_rawrxd_link_libs}"
                )
            endif()
        endif()
    endforeach()
endif()

# =============================================================================
# RawrXD-Crypto Shared Library
# =============================================================================
#
# NOTE: We intentionally do NOT link/build any UAC bypass implementation here.
# This target is kept buildable and export-stable (Encrypt/Decrypt/UACBypass),
# but UACBypass is a safe stub unless the project owners wire in their own impl.
#
# Build fix:
#   CMake cannot generate an ASM_MASM-only SHARED library for some generators
#   (missing CMAKE_ASM_MASM_CREATE_SHARED_LIBRARY). Ensure C++ is the linker
#   language by adding a tiny C++ TU and setting LINKER_LANGUAGE.
if(WIN32 AND MSVC AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/security/uac_bypass_impl_stub.cpp" AND NOT RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
    add_library(RawrXD-Crypto SHARED
        src/crypto_masm.asm
        src/security/uac_bypass_impl_stub.cpp
    )
    set_source_files_properties(
        src/crypto_masm.asm
        PROPERTIES LANGUAGE ASM_MASM
    )
    target_link_options(RawrXD-Crypto PRIVATE
        "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/src/security/RawrXD-Crypto.def"
    )
    target_link_libraries(RawrXD-Crypto PRIVATE
        kernel32
        advapi32
    )
    set_target_properties(RawrXD-Crypto PROPERTIES
        LINKER_LANGUAGE CXX
        RUNTIME_OUTPUT_DIRECTORY bin
        OUTPUT_NAME RawrXD-Crypto
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )
else()
    if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
        message(STATUS "RawrXD-Crypto skipped in strict production strip mode (uses uac_bypass_impl_stub.cpp)")
    else()
        message(STATUS "RawrXD-Crypto skipped (requires WIN32 + MSVC + MASM + uac_bypass_impl_stub.cpp)")
    endif()
endif()

# NOTE:
# Duplicate/partial RawrXD_Gold source block removed here.
# Canonical RawrXD_Gold target is defined in the standalone Gold section above.
    # ============================================================================
    # CI Gating Targets — Release / Debug / AddressSanitizer
    # ============================================================================
# Usage:
#   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release           (production)
#   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug             (dev)
#   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DRAWR_ASAN=ON  (ASAN)
#
# ASAN (MSVC): /fsanitize=address — detects heap overflows, use-after-free
option(RAWR_ASAN "Enable AddressSanitizer (MSVC /fsanitize=address)" OFF)

# PMASSA: bye Ollama — prefer local GGUF / inference; Ollama code can be excluded when ON
# See docs/PMASSA_BYE_OLLAMA.md
option(RAWRXD_PMASSA "PMASSA: phase out Ollama (prefer local GGUF; define RAWRXD_PMASSA)" OFF)
if(RAWRXD_PMASSA)
    add_compile_definitions(RAWRXD_PMASSA=1)
    message(STATUS "[PMASSA] Bye Ollama — RAWRXD_PMASSA=ON")
endif()

if(RAWR_ASAN AND MSVC)
    message(STATUS "[CI] AddressSanitizer ENABLED (/fsanitize=address)")
    add_compile_options(/fsanitize=address)
    # ASAN requires /Zi debug info and dynamic CRT
    add_compile_options(/Zi)
elseif(RAWR_ASAN AND NOT MSVC)
    message(STATUS "[CI] AddressSanitizer ENABLED (-fsanitize=address)")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()
if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES AND RAWR_ASAN)
    message(FATAL_ERROR "[ProductionLane] RAWR_ASAN=ON is incompatible with RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON.")
endif()

# Build SSOT stub-lane ownership guard only when explicitly enabled.
set(RAWRXD_HAS_SSOT_STUB_LANE_GUARD OFF)
if(RAWRXD_ENABLE_MISSING_HANDLER_STUBS AND EXISTS "${CMAKE_SOURCE_DIR}/src/core/missing_handler_stubs.cpp")
    add_library(ssot_stub_lane_objs OBJECT
        ${CMAKE_SOURCE_DIR}/src/core/missing_handler_stubs.cpp
        ${CMAKE_SOURCE_DIR}/src/core/ssot_missing_handlers_provider.cpp
    )
    target_compile_definitions(ssot_stub_lane_objs PRIVATE
        RAWRXD_ENABLE_MISSING_HANDLER_STUBS=1
    )
    target_include_directories(ssot_stub_lane_objs PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/core
    )

    add_custom_target(ssot_stub_lane_guard
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File ${CMAKE_SOURCE_DIR}/tools/verify_stub_lane_handler_ownership.ps1
            -RepoRoot ${CMAKE_SOURCE_DIR}
            -BuildDir ${CMAKE_BINARY_DIR}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "[CI Gate] Verifying SSOT stub-lane handler ownership"
        VERBATIM
    )
    add_dependencies(ssot_stub_lane_guard ssot_stub_lane_objs)
    set(RAWRXD_HAS_SSOT_STUB_LANE_GUARD ON)
else()
    message(STATUS "[SSOT] Stub lane guard disabled (set RAWRXD_ENABLE_MISSING_HANDLER_STUBS=ON to enable).")
endif()

option(RAWRXD_ENABLE_HEAVY_GATES "Run heavy ownership/compiled-symbol guards as part of self_test_gate" ON)
if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES AND NOT RAWRXD_ENABLE_HEAVY_GATES)
    message(FATAL_ERROR "[ProductionLane] RAWRXD_ENABLE_HEAVY_GATES=OFF is incompatible with RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON.")
endif()
if(RAWRXD_ENABLE_HEAVY_GATES)
    if(RAWRXD_HAS_SSOT_STUB_LANE_GUARD)
        if(TARGET RawrXD-Win32IDE)
            add_custom_target(self_test_gate
                COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config $<CONFIG> --target ssot_stub_lane_guard
                COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config $<CONFIG> --target RawrXD-Win32IDE
                COMMENT "[CI Gate] Ownership guard + RawrXD-Win32IDE"
                VERBATIM
            )
        else()
            add_custom_target(self_test_gate
                COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config $<CONFIG> --target ssot_stub_lane_guard
                COMMENT "[CI Gate] Ownership guard (Win32IDE not enabled)"
                VERBATIM
            )
        endif()
    else()
        if(TARGET RawrXD-Win32IDE)
            add_custom_target(self_test_gate
                COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config $<CONFIG> --target RawrXD-Win32IDE
                COMMENT "[CI Gate] RawrXD-Win32IDE (stub ownership guard disabled)"
                VERBATIM
            )
        else()
            add_custom_target(self_test_gate
                COMMAND ${CMAKE_COMMAND} -E echo "[CI Gate] self_test_gate: Win32IDE not enabled, nothing to build"
                COMMENT "[CI Gate] No-op (Win32IDE not enabled)"
                VERBATIM
            )
        endif()
    endif()
else()
    if(TARGET RawrXD-Win32IDE)
        add_custom_target(self_test_gate
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config $<CONFIG> --target RawrXD-Win32IDE
            COMMENT "[CI Gate] RawrXD-Win32IDE (heavy gates disabled)"
            VERBATIM
        )
    else()
        add_custom_target(self_test_gate
            COMMAND ${CMAKE_COMMAND} -E echo "[CI Gate] self_test_gate: Win32IDE not enabled, nothing to build"
            COMMENT "[CI Gate] No-op (Win32IDE not enabled)"
            VERBATIM
        )
    endif()
endif()
if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
    # Only add dependencies for targets that actually exist
    set(SELF_TEST_DEPS "")
    if(TARGET RawrXD-Win32IDE)
        list(APPEND SELF_TEST_DEPS RawrXD-Win32IDE)
    endif()
    if(TARGET RawrEngine)
        list(APPEND SELF_TEST_DEPS RawrEngine)
    endif()
    if(TARGET RawrXD_Gold)
        list(APPEND SELF_TEST_DEPS RawrXD_Gold)
    endif()
    if(TARGET RawrXD-InferenceEngine)
        list(APPEND SELF_TEST_DEPS RawrXD-InferenceEngine)
    endif()
    if(TARGET RawrXD_MultiWindow_Kernel)
        list(APPEND SELF_TEST_DEPS RawrXD_MultiWindow_Kernel)
    endif()
    if(TARGET RawrXD_DynamicPromptEngine)
        list(APPEND SELF_TEST_DEPS RawrXD_DynamicPromptEngine)
    endif()
    if(SELF_TEST_DEPS)
        add_dependencies(self_test_gate ${SELF_TEST_DEPS})
    endif()
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# SovereignTest_Suite — Pre-Build CI/CD Gate (beaconism pattern)
# Validates: atomic rollback, cache coherency, patch correctness
# ═══════════════════════════════════════════════════════════════════════════════
option(BUILD_SOVEREIGN_TEST_SUITE "Build SovereignTest_Suite pre-build gate" ON)
if(BUILD_SOVEREIGN_TEST_SUITE)
    add_executable(SovereignTest_Suite EXCLUDE_FROM_ALL
        src/sovereign/tests/SovereignTest_Main.cpp
        src/sovereign/tests/SovereignTest_AtomicRollback.cpp
        src/sovereign/tests/SovereignTest_CacheCoherency.cpp
        src/sovereign/tests/SovereignTest_Correctness.cpp
    )
    if(MSVC)
        target_compile_options(SovereignTest_Suite PRIVATE /O2 /arch:AVX512 /EHsc /W4 /std:c++20)
    endif()
    target_link_libraries(SovereignTest_Suite PRIVATE
        kernel32
        user32
    )
    set_target_properties(SovereignTest_Suite PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME SovereignTest_Suite
    )
    message(STATUS "[Sovereign] Test suite: SovereignTest_Suite (beaconism pattern)")

    # Add as dependency to self_test_gate
    if(TARGET self_test_gate)
        add_dependencies(self_test_gate SovereignTest_Suite)
    endif()
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# SovereignTest_VAL038_E2E — Integration E2E Harness for VAL-038 Kernel
# Live-fire validation: Cold-start -> Hot-patch -> Inference -> Hash validation
# ═══════════════════════════════════════════════════════════════════════════════
option(BUILD_SOVEREIGN_VAL038_E2E "Build VAL-038 E2E integration test" ON)
if(BUILD_SOVEREIGN_VAL038_E2E)
    add_executable(SovereignTest_VAL038_E2E EXCLUDE_FROM_ALL
        src/sovereign/tests/SovereignTest_VAL038_E2E.cpp
    )
    if(MSVC)
        target_compile_options(SovereignTest_VAL038_E2E PRIVATE /O2 /arch:AVX512 /EHsc /W4 /std:c++20)
    endif()
    target_link_libraries(SovereignTest_VAL038_E2E PRIVATE
        kernel32
        user32
        psapi
    )
    set_target_properties(SovereignTest_VAL038_E2E PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME SovereignTest_VAL038_E2E
    )
    message(STATUS "[Sovereign] VAL-038 E2E test: SovereignTest_VAL038_E2E")

    # Beaconism Gate: E2E test runs as part of the validation pipeline
    if(TARGET SovereignTest_Suite)
        add_dependencies(SovereignTest_Suite SovereignTest_VAL038_E2E)
    endif()
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# Deep2_Batch_Test — Model Stress-Test Harness with PageFaultMonitor
# Discovery-Load-Benchmark-Validation sequence for all models
# ═══════════════════════════════════════════════════════════════════════════════
option(BUILD_DEEP2_BATCH_TEST "Build Deep2 batch model stress-test harness" ON)
if(BUILD_DEEP2_BATCH_TEST)
    add_executable(Deep2_Batch_Test EXCLUDE_FROM_ALL
        src/sovereign/tests/SovereignTest_Deep2_Batch.cpp
    )
    if(MSVC)
        target_compile_options(Deep2_Batch_Test PRIVATE /O2 /arch:AVX512 /EHsc /W4 /std:c++20)
    endif()
    target_link_libraries(Deep2_Batch_Test PRIVATE
        kernel32
        user32
        psapi
    )
    set_target_properties(Deep2_Batch_Test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME Deep2_Batch_Test
    )
    message(STATUS "[Sovereign] Deep2 batch test: Deep2_Batch_Test")

    # Beaconism Gate: Batch test runs as part of the validation pipeline
    if(TARGET SovereignTest_Suite)
        add_dependencies(SovereignTest_Suite Deep2_Batch_Test)
    endif()
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# Deep2_Production_Bench — Production TPS Benchmark
# Tests: ThreadPool + KVCache + Full Transformer Stack + All Quantization Kernels
# ═══════════════════════════════════════════════════════════════════════════════
option(BUILD_DEEP2_PRODUCTION_BENCH "Build Deep2 production TPS benchmark" ON)
if(BUILD_DEEP2_PRODUCTION_BENCH)
    add_executable(Deep2_Production_Bench EXCLUDE_FROM_ALL
        # Core engine (verified working)
        src/deep2/deep2_end_to_end_bench.cpp
        src/deep2/Deep2Engine.cpp
        src/deep2/GGUFLoader.cpp
        src/deep2/ThreadPool.cpp
        src/deep2/KVCache.cpp
        src/deep2/MoERouter.cpp
        src/deep2/MoEWeightProxy.cpp
        src/deep2/MoEWeightsLoader.cpp
        src/deep2/DeepSeekMoELoader.cpp
        src/deep2/TensorHop.cpp
        src/deep2/MoEArchitectureParser.cpp
        src/deep2/QuantKernelRegistry.cpp
        src/deep2/Deep2ExecutionGraph.cpp
        src/deep2/ReverseHotpatchEngine.cpp
        src/deep2/ReverseIntegration.cpp
        src/deep2/DualGPUHook.cpp
        src/deep2/ReverseTensorRecovery.cpp
        src/deep2/StreamEngine.cpp
        src/deep2/StreamRouter.cpp
        src/deep2/FusedInferenceKernel.cpp
        # MARS dual-GPU VRAM orchestration
        src/deep2/mars/VRAMManager.cpp
        src/deep2/mars/TensorHotpatch.cpp
        src/deep2/mars/DualGPUBackend.cpp
        src/deep2/mars/MARSController.cpp
        # VAL-000 Components
        src/deep2/ReverseIntegration.cpp
        src/reverse/ReverseEngine.cpp
        src/reverse/ReverseModelLoader.cpp
        src/deep2/MedusaDecoder.cpp
        src/deep2/WarmupScheduler.cpp
        src/deep2/NUFusedPacker.cpp
        src/deep2/CompressedKVCache.cpp
        src/deep2/NVMeStream.cpp
        src/deep2/SlidingWindowEngine.cpp
        # Sampling
        src/sampling/advanced_sampler.cpp
        # The Bottle + The Antidote + TrailBrake (Safe Runtime Patching)
        src/deep2/HotPatcher.cpp
        src/deep2/HotPatcherSafety.cpp
        src/deep2/AntiPatcher.cpp
        src/deep2/TrailBrake.cpp
        src/deep2/PatchCache.cpp
        src/deep2/BottleTTL.cpp
        src/deep2/GoalSystem.cpp
        src/deep2/CPUFrequency.cpp
        # ASM kernels - Deep2 production kernels
        src/deep2/sovereign_deep2_kernels.asm
        src/deep2/sovereign_q4k_gemv.asm
        src/deep2/sovereign_moe_fused.asm
    )
    if(MSVC)
        target_compile_options(Deep2_Production_Bench PRIVATE /O2 /arch:AVX512 /EHsc /W4 /std:c++20)
    endif()
    target_include_directories(Deep2_Production_Bench PRIVATE
        ${CMAKE_SOURCE_DIR}/src/deep2
        ${CMAKE_SOURCE_DIR}/include
    )
    target_link_libraries(Deep2_Production_Bench PRIVATE
        kernel32
        user32
    )
    set_target_properties(Deep2_Production_Bench PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME Deep2_Production_Bench
    )
    message(STATUS "[Deep2] Production TPS benchmark: Deep2_Production_Bench (+ quantization kernels)")
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# SovereignTest_PatchRegistry — PatchRegistry Abstraction Layer Tests
# Validates CI/CD mock path and prepares for production hot-patching
# ═══════════════════════════════════════════════════════════════════════════════
option(BUILD_PATCHREGISTRY_TEST "Build PatchRegistry abstraction tests" ON)
if(BUILD_PATCHREGISTRY_TEST)
    add_executable(SovereignTest_PatchRegistry EXCLUDE_FROM_ALL
        src/sovereign/tests/SovereignTest_PatchRegistry.cpp
    )
    if(MSVC)
        target_compile_options(SovereignTest_PatchRegistry PRIVATE /O2 /EHsc /W4 /std:c++20)
    endif()
    target_include_directories(SovereignTest_PatchRegistry PRIVATE
        ${CMAKE_SOURCE_DIR}/src/sovereign/patcher
    )
    target_link_libraries(SovereignTest_PatchRegistry PRIVATE
        kernel32
        user32
    )
    set_target_properties(SovereignTest_PatchRegistry PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME SovereignTest_PatchRegistry
    )
    message(STATUS "[Sovereign] PatchRegistry test: SovereignTest_PatchRegistry")
    
    # Beaconism Gate: PatchRegistry test runs as part of validation pipeline
    if(TARGET SovereignTest_Suite)
        add_dependencies(SovereignTest_Suite SovereignTest_PatchRegistry)
    endif()
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# SovereignTest_HotPatcher — Production Live Binary Patching Tests
# Validates VirtualProtectEx, WriteProcessMemory, FlushInstructionCache
# ═══════════════════════════════════════════════════════════════════════════════
option(BUILD_HOTPATCHER_TEST "Build HotPatcher production tests" ON)
if(BUILD_HOTPATCHER_TEST)
    add_executable(SovereignTest_HotPatcher EXCLUDE_FROM_ALL
        src/sovereign/patcher/HotPatcher.cpp
        src/sovereign/tests/SovereignTest_HotPatcher.cpp
    )
    if(MSVC)
        target_compile_options(SovereignTest_HotPatcher PRIVATE /O2 /EHsc /W4 /std:c++20)
    endif()
    target_include_directories(SovereignTest_HotPatcher PRIVATE
        ${CMAKE_SOURCE_DIR}/src/sovereign/patcher
    )
    target_link_libraries(SovereignTest_HotPatcher PRIVATE
        kernel32
        user32
    )
    set_target_properties(SovereignTest_HotPatcher PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME SovereignTest_HotPatcher
    )
    message(STATUS "[Sovereign] HotPatcher test: SovereignTest_HotPatcher")
    
    # Beaconism Gate: HotPatcher test runs as part of validation pipeline
    if(TARGET SovereignTest_Suite)
        add_dependencies(SovereignTest_Suite SovereignTest_HotPatcher)
    endif()
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# SovereignTest_AutonomousAgent — End-to-End Autonomous Agent Integration Test
# Validates: SessionStore -> AgentGraphRuntime -> ToolRegistry -> AutonomousAgent
# ═══════════════════════════════════════════════════════════════════════════════
option(BUILD_AUTONOMOUS_AGENT_TEST "Build Autonomous Agent integration tests" ON)
if(BUILD_AUTONOMOUS_AGENT_TEST)
    add_executable(SovereignTest_AutonomousAgent EXCLUDE_FROM_ALL
        src/sovereign/session/SessionStore.cpp
        src/sovereign/agent/AgentGraphRuntime.cpp
        src/sovereign/agent/AutonomousAgent.cpp
        src/sovereign/tool/ToolRegistry.cpp
        src/sovereign/tests/SovereignTest_AutonomousAgent.cpp
    )
    if(MSVC)
        target_compile_options(SovereignTest_AutonomousAgent PRIVATE /O2 /EHsc /W4 /std:c++20)
    endif()
    target_include_directories(SovereignTest_AutonomousAgent PRIVATE
        ${CMAKE_SOURCE_DIR}/src/sovereign/session
        ${CMAKE_SOURCE_DIR}/src/sovereign/agent
        ${CMAKE_SOURCE_DIR}/src/sovereign/tool
    )
    target_link_libraries(SovereignTest_AutonomousAgent PRIVATE
        kernel32
        user32
    )
    set_target_properties(SovereignTest_AutonomousAgent PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME SovereignTest_AutonomousAgent
    )
    message(STATUS "[Sovereign] AutonomousAgent test: SovereignTest_AutonomousAgent")
    
    # Beaconism Gate: AutonomousAgent test runs as part of validation pipeline
    if(TARGET SovereignTest_Suite)
        add_dependencies(SovereignTest_Suite SovereignTest_AutonomousAgent)
    endif()
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# AntiHallucinationHotPatcher — MASM Anti-Hallucination Guard
# Validates patches before application to prevent logic drift
# ═══════════════════════════════════════════════════════════════════════════════
option(BUILD_ANTI_HALLUCINATION "Build Anti-Hallucination MASM patcher" ON)
if(BUILD_ANTI_HALLUCINATION AND MSVC)
    # Assemble the MASM file
    set(ASM_SOURCE ${CMAKE_SOURCE_DIR}/src/sovereign/patcher/AntiHallucinationHotPatcher.asm)
    set(ASM_OBJECT ${CMAKE_BINARY_DIR}/AntiHallucinationHotPatcher.obj)
    
    add_custom_command(
        OUTPUT ${ASM_OBJECT}
        COMMAND ${CMAKE_ASM_MASM_COMPILER} /c /Fo ${ASM_OBJECT} ${ASM_SOURCE}
        DEPENDS ${ASM_SOURCE}
        COMMENT "Assembling AntiHallucinationHotPatcher.asm"
    )
    
    add_custom_target(AntiHallucinationASM DEPENDS ${ASM_OBJECT})
    
    add_executable(SovereignTest_AntiHallucination EXCLUDE_FROM_ALL
        src/sovereign/tests/SovereignTest_AntiHallucination.cpp
        ${ASM_OBJECT}
    )
    add_dependencies(SovereignTest_AntiHallucination AntiHallucinationASM)
    
    if(MSVC)
        target_compile_options(SovereignTest_AntiHallucination PRIVATE /O2 /EHsc /W4 /std:c++20)
    endif()
    target_include_directories(SovereignTest_AntiHallucination PRIVATE
        ${CMAKE_SOURCE_DIR}/src/sovereign/patcher
    )
    target_link_libraries(SovereignTest_AntiHallucination PRIVATE
        kernel32
        user32
    )
    set_target_properties(SovereignTest_AntiHallucination PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME SovereignTest_AntiHallucination
    )
    message(STATUS "[Sovereign] AntiHallucination test: SovereignTest_AntiHallucination")
    
    # Beaconism Gate: AntiHallucination test runs as part of validation pipeline
    if(TARGET SovereignTest_Suite)
        add_dependencies(SovereignTest_Suite SovereignTest_AntiHallucination)
    endif()
endif()
# ═══════════════════════════════════════════════════════════════════════════════

# CI gate: stress_test — build and note availability
message(STATUS "[CI] self_test_gate target available (cmake --build . --target self_test_gate)")
message(STATUS "[CI] RAWR_ASAN=${RAWR_ASAN}")
if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
    message(STATUS "[ProductionLane] Strict production profile enabled (stubs/shims/fallbacks stripped; heavy gates required)")
endif()


find_package(Python3 COMPONENTS Interpreter QUIET)
if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES AND NOT Python3_FOUND)
    message(FATAL_ERROR "[ProductionLane] Python3 interpreter is required when RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON (validate_registry_strict dependency).")
endif()
if(Python3_FOUND)
    add_custom_target(validate_registry
        COMMAND ${Python3_EXECUTABLE} 
            ${CMAKE_SOURCE_DIR}/tools/validate_command_registry.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Validating command registry integrity (SSOT check)..."
        VERBATIM
    )
    
    add_custom_target(validate_registry_strict
        COMMAND ${Python3_EXECUTABLE} 
            ${CMAKE_SOURCE_DIR}/tools/validate_command_registry.py --strict
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Validating command registry (STRICT — all IDM_* must be in COMMAND_TABLE)..."
        VERBATIM
    )
    if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
        add_dependencies(self_test_gate validate_registry)
        add_dependencies(self_test_gate validate_registry_strict)
    endif()
endif()

# ============================================================================
# Inference Routing Test - Minimal validation of local vs Ollama path selection
# ============================================================================
# This test validates the core routing logic WITHOUT full IDE linkage
# Build: cmake --build . --target RawrXD-InferenceRoutingTest
# Run:   .\bin\RawrXD-InferenceRoutingTest.exe
add_executable(RawrXD-InferenceRoutingTest
    src/tests/inference_routing_test.cpp
)
set_target_properties(RawrXD-InferenceRoutingTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY bin
    OUTPUT_NAME RawrXD-InferenceRoutingTest
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
target_include_directories(RawrXD-InferenceRoutingTest PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)
message(STATUS "[Test] RawrXD-InferenceRoutingTest target added (validates local vs Ollama routing)")

# Autonomous Agentic Pipeline unit test (Task 5 — buildable)
if(WIN32)
    add_executable(test_autonomous_pipeline
        tests/test_autonomous_pipeline.cpp
        Ship/RawrXD_AutonomousAgenticPipeline.cpp
    )
    target_include_directories(test_autonomous_pipeline PRIVATE
        ${CMAKE_SOURCE_DIR}/Ship
        ${CMAKE_SOURCE_DIR}
    )
    target_link_libraries(test_autonomous_pipeline PRIVATE kernel32)
    set_target_properties(test_autonomous_pipeline PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    if(EXISTS "${CMAKE_SOURCE_DIR}/src/cpu_inference_engine_autofix_stub.cpp" AND NOT RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
        add_executable(RawrXD-AutoFixCLI
            src/app/cli_entrypoint.cpp
            # NOTE: Removed quantum_agent_orchestrator.cpp - marketing fiction
            src/agentic/AgentOllamaClient.cpp
            src/inference/gpu_dispatch_gate.cpp
            src/cpu_inference_engine_autofix_stub.cpp
            src/gguf_d3d12_bridge.cpp
        )
        target_include_directories(RawrXD-AutoFixCLI PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/include
        )
        target_link_libraries(RawrXD-AutoFixCLI PRIVATE
            kernel32
            user32
            winhttp
            d3d12
            dxgi
            d3dcompiler
        )
        set_target_properties(RawrXD-AutoFixCLI PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )
    elseif(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
        message(STATUS "RawrXD-AutoFixCLI skipped in strict production strip mode (uses cpu_inference_engine_autofix_stub.cpp)")
    else()
        message(STATUS "RawrXD-AutoFixCLI skipped (cpu_inference_engine_autofix_stub.cpp missing)")
    endif()

    # ═══════════════════════════════════════════════════════════════════
    # GPU Inference Benchmark — v16.2-GPU-HYBRID Phase C
    # ═══════════════════════════════════════════════════════════════════
    add_executable(RawrXD-Benchmark EXCLUDE_FROM_ALL
        src/tools/gpu_benchmark.cpp
        src/gguf_d3d12_bridge.cpp
        src/kernels/flash_attention.cpp
    )
    target_include_directories(RawrXD-Benchmark PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
    )
    # Vulkan SDK support for RawrXD-Benchmark
    if(RAWR_ENABLE_VULKAN AND RAWR_VULKAN_SDK_ROOT)
        target_include_directories(RawrXD-Benchmark PRIVATE "${RAWR_VULKAN_INCLUDE}")
        target_link_libraries(RawrXD-Benchmark PRIVATE "${RAWR_VULKAN_LIB}")
        target_compile_definitions(RawrXD-Benchmark PRIVATE RAWR_HAS_VULKAN=1)
    elseif(RAWR_ENABLE_VULKAN AND VULKAN_INCLUDE_DIR)
        target_include_directories(RawrXD-Benchmark PRIVATE ${VULKAN_INCLUDE_DIR})
        target_compile_definitions(RawrXD-Benchmark PRIVATE RAWR_HAS_VULKAN=1)
    elseif(RAWR_ENABLE_VULKAN AND Vulkan_FOUND)
        target_include_directories(RawrXD-Benchmark PRIVATE ${Vulkan_INCLUDE_DIRS})
        target_link_libraries(RawrXD-Benchmark PRIVATE Vulkan::Vulkan)
        target_compile_definitions(RawrXD-Benchmark PRIVATE RAWR_HAS_VULKAN=1)
    endif()
    target_compile_definitions(RawrXD-Benchmark PRIVATE
        RAWR_BENCHMARK=1
    )
    if(MSVC)
        target_compile_options(RawrXD-Benchmark PRIVATE /EHsc /W0 /O2)
    endif()
    target_link_libraries(RawrXD-Benchmark PRIVATE
        kernel32
        user32
        d3d12
        dxgi
        d3dcompiler
        ole32
    )
    set_target_properties(RawrXD-Benchmark PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME RawrXD-Benchmark
    )
    message(STATUS "[Phase C] GPU Benchmark target: RawrXD-Benchmark")

    # ═══════════════════════════════════════════════════════════════════
    # Fused Layer Benchmark — v16.3 Phase D Layer Fusion
    # ═══════════════════════════════════════════════════════════════════
    add_executable(RawrXD-FusedBenchmark EXCLUDE_FROM_ALL
        src/tools/fused_layer_benchmark.cpp
        src/gguf_d3d12_bridge.cpp
    )
    target_include_directories(RawrXD-FusedBenchmark PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
    )
    target_compile_definitions(RawrXD-FusedBenchmark PRIVATE
        RAWR_BENCHMARK=1
    )
    if(MSVC)
        target_compile_options(RawrXD-FusedBenchmark PRIVATE /EHsc /W0 /O2)
    endif()
    target_link_libraries(RawrXD-FusedBenchmark PRIVATE
        kernel32
        user32
        d3d12
        dxgi
        d3dcompiler
        ole32
    )
    set_target_properties(RawrXD-FusedBenchmark PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME RawrXD-FusedBenchmark
    )
    message(STATUS "[Phase D] Fused Layer Benchmark target: RawrXD-FusedBenchmark")

    # ═══════════════════════════════════════════════════════════════════
    # KV Cache Benchmark — v16.4 Phase E GPU-Resident KV Cache
    # ═══════════════════════════════════════════════════════════════════
    add_executable(RawrXD-KVBenchmark EXCLUDE_FROM_ALL
        src/tools/kv_cache_benchmark.cpp
        src/gguf_d3d12_bridge.cpp
    )
    target_include_directories(RawrXD-KVBenchmark PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
    )
    target_compile_definitions(RawrXD-KVBenchmark PRIVATE
        RAWR_BENCHMARK=1
    )
    if(MSVC)
        target_compile_options(RawrXD-KVBenchmark PRIVATE /EHsc /W0 /O2)
    endif()
    target_link_libraries(RawrXD-KVBenchmark PRIVATE
        kernel32
        user32
        d3d12
        dxgi
        d3dcompiler
        ole32
    )
    set_target_properties(RawrXD-KVBenchmark PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME RawrXD-KVBenchmark
    )
    message(STATUS "[Phase E] KV Cache Benchmark target: RawrXD-KVBenchmark")

    # ═══════════════════════════════════════════════════════════════════
    # Fix #3 NHWC Layout Benchmark — REMOVED: stub file deleted
    # ═══════════════════════════════════════════════════════════════════
    # add_executable(Fix3_NHWC_Benchmark EXCLUDE_FROM_ALL ...)
    # TARGET REMOVED: src/benchmark/Fix3_NHWC_Benchmark_Stub.cpp was deleted

    # ═══════════════════════════════════════════════════════════════════
    # Fix #4 Flash Attention v2 Benchmark — Memory-efficient attention
    # ═══════════════════════════════════════════════════════════════════
    add_executable(Fix4_FlashAttention_Benchmark EXCLUDE_FROM_ALL
        src/benchmark/Fix4_FlashAttention_Benchmark.cpp
    )
    target_include_directories(Fix4_FlashAttention_Benchmark PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/memory
        ${CMAKE_SOURCE_DIR}/include
    )
    target_compile_definitions(Fix4_FlashAttention_Benchmark PRIVATE
        RAWR_BENCHMARK=1
    )
    if(MSVC)
        target_compile_options(Fix4_FlashAttention_Benchmark PRIVATE /EHsc /W4 /O2 /arch:AVX512)
    endif()
    target_link_libraries(Fix4_FlashAttention_Benchmark PRIVATE
        kernel32
        user32
    )
    set_target_properties(Fix4_FlashAttention_Benchmark PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME Fix4_FlashAttention_Benchmark
    )
    message(STATUS "[Fix #4] Flash Attention v2 Benchmark target: Fix4_FlashAttention_Benchmark")

    # ═══════════════════════════════════════════════════════════════════
    # Fix #5 Fused Quantized Kernels — Q4_0/Q8_0 fused dequantize+GEMM
    # ═══════════════════════════════════════════════════════════════════
    add_executable(Fix5_FusedQuantized_Benchmark EXCLUDE_FROM_ALL
        src/benchmark/Fix5_FusedQuantized_Benchmark.cpp
    )
    target_include_directories(Fix5_FusedQuantized_Benchmark PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/memory
        ${CMAKE_SOURCE_DIR}/include
    )
    target_compile_definitions(Fix5_FusedQuantized_Benchmark PRIVATE
        RAWR_BENCHMARK=1
    )
    if(MSVC)
        target_compile_options(Fix5_FusedQuantized_Benchmark PRIVATE /EHsc /W4 /O2 /arch:AVX512)
    endif()
    target_link_libraries(Fix5_FusedQuantized_Benchmark PRIVATE
        kernel32
        user32
    )
    set_target_properties(Fix5_FusedQuantized_Benchmark PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME Fix5_FusedQuantized_Benchmark
    )
    message(STATUS "[Fix #5] Fused Quantized Kernels Benchmark target: Fix5_FusedQuantized_Benchmark")

    # ═══════════════════════════════════════════════════════════════════
    # VAL-032: Speculative Decoding (Medusa-style) Benchmark
    # ═══════════════════════════════════════════════════════════════════
    add_executable(VAL032_SpeculativeDecoding_Benchmark EXCLUDE_FROM_ALL
        src/benchmark/VAL032_SpeculativeDecoding_Benchmark.cpp
    )
    target_include_directories(VAL032_SpeculativeDecoding_Benchmark PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/memory
        ${CMAKE_SOURCE_DIR}/include
    )
    target_compile_definitions(VAL032_SpeculativeDecoding_Benchmark PRIVATE
        RAWR_BENCHMARK=1
    )
    if(MSVC)
        target_compile_options(VAL032_SpeculativeDecoding_Benchmark PRIVATE /EHsc /W4 /O2 /arch:AVX512)
    endif()
    target_link_libraries(VAL032_SpeculativeDecoding_Benchmark PRIVATE
        kernel32
        user32
    )
    set_target_properties(VAL032_SpeculativeDecoding_Benchmark PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME VAL032_SpeculativeDecoding_Benchmark
    )
    message(STATUS "[VAL-032] Speculative Decoding Benchmark target: VAL032_SpeculativeDecoding_Benchmark")

    # ═══════════════════════════════════════════════════════════════════
    # VAL-032: AVX-512 Assembly Kernel Benchmark
    # ═══════════════════════════════════════════════════════════════════
    add_executable(VAL032_AVX512_Benchmark EXCLUDE_FROM_ALL
        src/benchmark/VAL032_AVX512_Benchmark_Clean.cpp
    )
    target_include_directories(VAL032_AVX512_Benchmark PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/memory
        ${CMAKE_SOURCE_DIR}/include
    )
    target_compile_definitions(VAL032_AVX512_Benchmark PRIVATE
        RAWR_BENCHMARK=1
    )
    if(MSVC)
        target_compile_options(VAL032_AVX512_Benchmark PRIVATE /EHsc /W4 /O2 /arch:AVX512)
    endif()
    target_link_libraries(VAL032_AVX512_Benchmark PRIVATE
        kernel32
        user32
        telemetry_masm_kernels
    )
    set_target_properties(VAL032_AVX512_Benchmark PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME VAL032_AVX512_Benchmark
    )
    message(STATUS "[VAL-032] AVX-512 Benchmark target: VAL032_AVX512_Benchmark")

    # ═══════════════════════════════════════════════════════════════════
    # VAL-032 Simple Intrinsics Benchmark — Direct AVX-512 performance measurement
    # ═══════════════════════════════════════════════════════════════════
    add_executable(tree_attention_bench_simple EXCLUDE_FROM_ALL
        src/benchmark/tree_attention_avx512_intrinsics.cpp
        src/benchmark/tree_attention_bench_simple.cpp
    )
    target_include_directories(tree_attention_bench_simple PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    if(MSVC)
        target_compile_options(tree_attention_bench_simple PRIVATE /O2 /arch:AVX512 /EHsc /W4)
    endif()
    target_link_libraries(tree_attention_bench_simple PRIVATE
        kernel32
        user32
    )
    set_target_properties(tree_attention_bench_simple PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME tree_attention_bench_simple
    )
    message(STATUS "[VAL-032] Simple intrinsics benchmark: tree_attention_bench_simple")

    # ═══════════════════════════════════════════════════════════════════
    # VAL-033 Profiling Instrumentation — Per-stage cycle-accurate timing
    # ═══════════════════════════════════════════════════════════════════
    add_executable(tree_attention_profiled EXCLUDE_FROM_ALL
        src/benchmark/tree_attention_profiled.cpp
    )
    target_include_directories(tree_attention_profiled PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    if(MSVC)
        target_compile_options(tree_attention_profiled PRIVATE /O2 /arch:AVX512 /EHsc /W4)
    endif()
    target_link_libraries(tree_attention_profiled PRIVATE
        kernel32
        user32
    )
    set_target_properties(tree_attention_profiled PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME tree_attention_profiled
    )
    message(STATUS "[VAL-033] Profiling instrumentation: tree_attention_profiled")

    # ═══════════════════════════════════════════════════════════════════
    # VAL-034 Q@K^T Alignment Optimization — 64-byte aligned loads
    # ═══════════════════════════════════════════════════════════════════
    add_executable(tree_attention_aligned EXCLUDE_FROM_ALL
        src/benchmark/tree_attention_aligned.cpp
    )
    target_include_directories(tree_attention_aligned PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    if(MSVC)
        target_compile_options(tree_attention_aligned PRIVATE /O2 /arch:AVX512 /EHsc /W4)
    endif()
    target_link_libraries(tree_attention_aligned PRIVATE
        kernel32
        user32
    )
    set_target_properties(tree_attention_aligned PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME tree_attention_aligned
    )
    message(STATUS "[VAL-034] Q@K^T alignment optimization: tree_attention_aligned")

    # ═══════════════════════════════════════════════════════════════════
    # VAL-035 Tiled Q@K^T — Register-resident Q, streaming K
    # ═══════════════════════════════════════════════════════════════════
    add_executable(tree_attention_tiled EXCLUDE_FROM_ALL
        src/benchmark/tree_attention_tiled.cpp
    )
    target_include_directories(tree_attention_tiled PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    if(MSVC)
        target_compile_options(tree_attention_tiled PRIVATE /O2 /arch:AVX512 /EHsc /W4)
    endif()
    target_link_libraries(tree_attention_tiled PRIVATE
        kernel32
        user32
    )
    set_target_properties(tree_attention_tiled PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME tree_attention_tiled
    )
    message(STATUS "[VAL-035] Tiled Q@K^T: tree_attention_tiled")

    # ═══════════════════════════════════════════════════════════════════
    # VAL-036 Vectorized Softmax — AVX-512 polynomial exp approximation
    # ═══════════════════════════════════════════════════════════════════
    add_executable(tree_attention_vectorized EXCLUDE_FROM_ALL
        src/benchmark/tree_attention_vectorized.cpp
    )
    target_include_directories(tree_attention_vectorized PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    if(MSVC)
        target_compile_options(tree_attention_vectorized PRIVATE /O2 /arch:AVX512 /EHsc /W4)
    endif()
    target_link_libraries(tree_attention_vectorized PRIVATE
        kernel32
        user32
    )
    set_target_properties(tree_attention_vectorized PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME tree_attention_vectorized
    )
    message(STATUS "[VAL-036] Vectorized softmax: tree_attention_vectorized")

    # ═══════════════════════════════════════════════════════════════════
    # VAL-037 Tree Sparsity — Exploit tree structure to reduce computation
    # ═══════════════════════════════════════════════════════════════════
    add_executable(tree_attention_sparse EXCLUDE_FROM_ALL
        src/benchmark/tree_attention_sparse.cpp
    )
    target_include_directories(tree_attention_sparse PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    if(MSVC)
        target_compile_options(tree_attention_sparse PRIVATE /O2 /arch:AVX512 /EHsc /W4)
    endif()
    target_link_libraries(tree_attention_sparse PRIVATE
        kernel32
        user32
    )
    set_target_properties(tree_attention_sparse PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME tree_attention_sparse
    )
    message(STATUS "[VAL-037] Tree sparsity: tree_attention_sparse")

    # ═══════════════════════════════════════════════════════════════════
    # VAL-037.1 Fused Online Softmax + Block-Sparse Attention
    # ═══════════════════════════════════════════════════════════════════
    add_executable(tree_attention_fused_sparse EXCLUDE_FROM_ALL
        src/benchmark/tree_attention_fused_sparse.cpp
    )
    target_include_directories(tree_attention_fused_sparse PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    if(MSVC)
        target_compile_options(tree_attention_fused_sparse PRIVATE /O2 /arch:AVX512 /EHsc /W4)
    endif()
    target_link_libraries(tree_attention_fused_sparse PRIVATE
        kernel32
        user32
    )
    set_target_properties(tree_attention_fused_sparse PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME tree_attention_fused_sparse
    )
    message(STATUS "[VAL-037.1] Fused online softmax: tree_attention_fused_sparse")

    # ═══════════════════════════════════════════════════════════════════
    # VAL-037.2 Tree-Specialized Fused Attention — Exploit 4x4 tree topology
    # ═══════════════════════════════════════════════════════════════════
    add_executable(tree_attention_tree_specialized EXCLUDE_FROM_ALL
        src/benchmark/tree_attention_tree_specialized.cpp
    )
    target_include_directories(tree_attention_tree_specialized PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    if(MSVC)
        target_compile_options(tree_attention_tree_specialized PRIVATE /O2 /arch:AVX512 /EHsc /W4)
    endif()
    target_link_libraries(tree_attention_tree_specialized PRIVATE
        kernel32
        user32
    )
    set_target_properties(tree_attention_tree_specialized PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME tree_attention_tree_specialized
    )
    message(STATUS "[VAL-037.2] Tree-specialized: tree_attention_tree_specialized")

    # ═══════════════════════════════════════════════════════════════════
    # VAL-038.1 MASM Mechanical Translation — Same algorithm, optimal registers
    # ═══════════════════════════════════════════════════════════════════
    # Create the executable with C++ wrapper and ASM source
    add_executable(tree_attention_masm_val038 EXCLUDE_FROM_ALL
        src/benchmark/tree_attention_masm_val038_wrapper.cpp
        src/benchmark/tree_attention_masm_val038.asm
    )
    target_include_directories(tree_attention_masm_val038 PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    if(MSVC)
        target_compile_options(tree_attention_masm_val038 PRIVATE /O2 /arch:AVX512 /EHsc /W4)
        # Enable MASM for the .asm file
        set_source_files_properties(src/benchmark/tree_attention_masm_val038.asm 
            PROPERTIES LANGUAGE ASM_MASM)
    endif()
    target_link_libraries(tree_attention_masm_val038 PRIVATE
        kernel32
        user32
    )
    set_target_properties(tree_attention_masm_val038 PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME tree_attention_masm_val038
    )
    message(STATUS "[VAL-038.1] MASM mechanical: tree_attention_masm_val038")

    # ═══════════════════════════════════════════════════════════════════
    # VAL-038 Fused Attention — Q@K^T → Softmax → A@V in single kernel
    # ═══════════════════════════════════════════════════════════════════
    add_executable(VAL038_FusedAttention_Benchmark EXCLUDE_FROM_ALL
        src/benchmark/VAL038_FusedAttention_Benchmark.cpp
    )
    target_include_directories(VAL038_FusedAttention_Benchmark PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/validation/kernels
    )
    if(MSVC)
        target_compile_options(VAL038_FusedAttention_Benchmark PRIVATE /O2 /arch:AVX512 /EHsc /W4)
    endif()
    target_link_libraries(VAL038_FusedAttention_Benchmark PRIVATE
        kernel32
        user32
        ${CMAKE_BINARY_DIR}/lib/TreeAttention_Fused_VAL038.lib
    )
    set_target_properties(VAL038_FusedAttention_Benchmark PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME VAL038_FusedAttention_Benchmark
    )
    message(STATUS "[VAL-038] Fused attention benchmark: VAL038_FusedAttention_Benchmark")

    # ═══════════════════════════════════════════════════════════════════
    # VAL-038 Validation Benchmark (B → C → A)
    # Step B: Control flow validation with debug counters
    # Step C: Numerical correctness vs scalar reference
    # ═══════════════════════════════════════════════════════════════════
    add_executable(VAL038_Validation_Benchmark EXCLUDE_FROM_ALL
        src/benchmark/VAL038_Validation_Benchmark.cpp
    )
    target_include_directories(VAL038_Validation_Benchmark PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/validation/kernels
    )
    if(MSVC)
        target_compile_options(VAL038_Validation_Benchmark PRIVATE /O2 /EHsc /W4)
    endif()
    target_link_libraries(VAL038_Validation_Benchmark PRIVATE
        kernel32
        user32
        ${CMAKE_BINARY_DIR}/lib/TreeAttention_Fused_VAL038_Debug.lib
    )
    set_target_properties(VAL038_Validation_Benchmark PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME VAL038_Validation_Benchmark
    )
    message(STATUS "[VAL-038] Validation benchmark (B→C→A): VAL038_Validation_Benchmark")

    # ═══════════════════════════════════════════════════════════════════
    # TPS smoke — GGUFRunner + model_runtime_gate + runtime_surface unity + decode TPS vs ref (default 239)
    # ═══════════════════════════════════════════════════════════════════
    add_executable(RawrXD-TpsSmoke EXCLUDE_FROM_ALL
        src/tools/RawrXD_TpsSmoke.cpp
        src/llm_adapter/GGUFRunner.cpp
        src/llm_adapter/gguf_k_quants.cpp
        src/llm_adapter/QuantBackend.cpp
        src/llm_adapter/ggufrunner_link_kernels.cpp
        src/core/unified_memory_executor.cpp
        src/core/model_runtime_gate.cpp
        src/runtime_surface/GenerationStopwatch.cpp
        src/runtime_surface/RuntimeSurfaceModule.cpp
        src/logging/Logger.cpp
    )
    target_include_directories(RawrXD-TpsSmoke PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/llm_adapter
        ${CMAKE_SOURCE_DIR}/src/core
        ${CMAKE_SOURCE_DIR}/src/codec
        ${CMAKE_SOURCE_DIR}/include
    )
    if(MSVC)
        target_compile_options(RawrXD-TpsSmoke PRIVATE /EHsc /W2 /O2 /arch:AVX2)
    endif()
    target_link_libraries(RawrXD-TpsSmoke PRIVATE kernel32)
    set_target_properties(RawrXD-TpsSmoke PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME RawrXD-TpsSmoke
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
    )
    message(STATUS "[Smoke] RawrXD-TpsSmoke target (Run-TpsSmoke.ps1)")

    # ═══════════════════════════════════════════════════════════════════
    # Model analysis — shared lib (Win32IDE + CLI) + CLI tool (docs/ANALYSIS_QUICKSTART.md)
    # Note: gguf_k_quants.cpp stays out of the lib — Win32IDE already links one copy.
    # ═══════════════════════════════════════════════════════════════════
    add_library(RawrXD-ModelAnalysisLib STATIC EXCLUDE_FROM_ALL
        src/core/model_anatomy.cpp
        src/core/neurological_diff.cpp
    )
    target_include_directories(RawrXD-ModelAnalysisLib PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/llm_adapter
    )
    if(MSVC)
        target_compile_options(RawrXD-ModelAnalysisLib PRIVATE /EHsc /W2)
        target_compile_options(RawrXD-ModelAnalysisLib PRIVATE
            $<$<CONFIG:Release>:/O2>
            $<$<CONFIG:RelWithDebInfo>:/O2>
        )
    endif()
    set_target_properties(RawrXD-ModelAnalysisLib PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
    )

    add_executable(RawrXD-ModelAnalysis EXCLUDE_FROM_ALL
        src/tools/model_analysis_cli.cpp
        src/llm_adapter/gguf_k_quants.cpp
    )
    target_include_directories(RawrXD-ModelAnalysis PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/llm_adapter
    )
    if(MSVC)
        target_compile_options(RawrXD-ModelAnalysis PRIVATE /EHsc /W2)
        target_compile_options(RawrXD-ModelAnalysis PRIVATE
            $<$<CONFIG:Release>:/O2>
            $<$<CONFIG:RelWithDebInfo>:/O2>
        )
    endif()
    target_link_libraries(RawrXD-ModelAnalysis PRIVATE RawrXD-ModelAnalysisLib kernel32)
    set_target_properties(RawrXD-ModelAnalysis PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME RawrXD-ModelAnalysis
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
    )
    message(STATUS "[Analysis] RawrXD-ModelAnalysis target (Implant_Model_Analysis.ps1 / ANALYSIS_QUICKSTART.md)")

    if(TARGET RawrXD-Win32IDE AND TARGET RawrXD-ModelAnalysisLib)
        target_link_libraries(RawrXD-Win32IDE PRIVATE RawrXD-ModelAnalysisLib)
        target_compile_definitions(RawrXD-Win32IDE PRIVATE RAWR_HAS_MODEL_ANATOMY=1)
        message(STATUS "[Analysis] Win32IDE linked to RawrXD-ModelAnalysisLib (RAWR_HAS_MODEL_ANATOMY)")
    endif()
endif()

# Swarm smoke test (manual)
if(NOT RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
    add_executable(SwarmSmokeTest EXCLUDE_FROM_ALL
        src/tests/SwarmSmokeTest.cpp
        src/bridge/Win32SwarmBridge.cpp
        src/bridge/SwarmIATRegistration.cpp
        src/agentic/SubAgentManager.cpp
        src/agentic/RobustOllamaParser.cpp
        src/runtime_patcher.cpp
        # REMOVED: src/tests/swarm_smoke_stubs.cpp - stub file deleted
    )
    target_include_directories(SwarmSmokeTest PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
    )
    if(WIN32)
        target_link_libraries(SwarmSmokeTest PRIVATE user32 gdi32 shell32 ole32 oleaut32 uuid)
    endif()
else()
    message(STATUS "SwarmSmokeTest skipped in strict production strip mode (uses swarm_smoke_stubs.cpp)")
endif()

# BackendOrchestrator model sharding smoke test (manual)
add_executable(BackendOrchestratorShardSmoke EXCLUDE_FROM_ALL
    src/tests/backend_orchestrator_shard_smoke.cpp
    # REMOVED: src/tests/backend_orchestrator_shard_smoke_stubs.cpp - stub file deleted
    src/BackendOrchestrator.cpp
    src/InferenceProfiler.cpp
    src/gguf_loader.cpp
    src/gguf_vocab_resolver.cpp
)
target_include_directories(BackendOrchestratorShardSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)
if(WIN32)
    target_link_libraries(BackendOrchestratorShardSmoke PRIVATE kernel32 psapi)
endif()
message(STATUS "[BackendOrchestrator] Smoke test target: BackendOrchestratorShardSmoke")

# ============================================================================
# Agentic Planning Orchestrator Smoke Tests
# ============================================================================
add_executable(agentic_orchestrator_smoke_test EXCLUDE_FROM_ALL
    src/agentic/agentic_planning_orchestrator.cpp
    src/agentic/agentic_orchestrator_integration.cpp
    src/agentic/agentic_audit_sink.cpp
    src/agentic/agentic_orchestrator_smoke_test.cpp
)

target_include_directories(agentic_orchestrator_smoke_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/agentic
    ${CMAKE_SOURCE_DIR}/src/win32app
)

target_compile_features(agentic_orchestrator_smoke_test PRIVATE cxx_std_17)

if(WIN32)
    target_link_libraries(agentic_orchestrator_smoke_test PRIVATE
        Threads::Threads
        user32 gdi32 shell32
    )
else()
    target_link_libraries(agentic_orchestrator_smoke_test PRIVATE Threads::Threads)
endif()

message(STATUS "[AgenticOrchestrator] Smoke test target: agentic_orchestrator_smoke_test (build with: cmake --build . --target agentic_orchestrator_smoke_test)")

# ============================================================================
# Golden Recovery Tests (Mission 2.3: Validation Framework)
# ============================================================================
add_executable(RawrXD-GoldenRecoveryTests EXCLUDE_FROM_ALL
    tests/autonomy/recovery/GoldenRecoveryTests.cpp
    validation/fault_injection/FaultInjector.cpp
    validation/fault_injection/WorkerCrashInjector.cpp
    validation/fault_injection/MemoryPressureInjector.cpp
    validation/fault_injection/ServiceKillInjector.cpp
    validation/fault_injection/StateCorruptionInjector.cpp
    validation/fault_injection/ExceptionStormInjector.cpp
    validation/recovery_telemetry/RecoveryTelemetry.cpp
)

target_include_directories(RawrXD-GoldenRecoveryTests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/agentic
    ${CMAKE_SOURCE_DIR}/validation/fault_injection
    ${CMAKE_SOURCE_DIR}/validation/recovery_telemetry
)

target_compile_features(RawrXD-GoldenRecoveryTests PRIVATE cxx_std_17)

if(WIN32)
    target_link_libraries(RawrXD-GoldenRecoveryTests PRIVATE
        Threads::Threads
    )
else()
    target_link_libraries(RawrXD-GoldenRecoveryTests PRIVATE Threads::Threads)
endif()

set_target_properties(RawrXD-GoldenRecoveryTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)

message(STATUS "[GoldenRecoveryTests] Target: RawrXD-GoldenRecoveryTests (build with: cmake --build . --target RawrXD-GoldenRecoveryTests)")

# ============================================================================
# RawrXD Sovereign Runtime v1.0-ALPHA (War Room: One Binary, One Command, One Evidence Bundle)
# ============================================================================
# Only add sovereign runtime if CLI version not already defined
if(NOT TARGET rawrxd)
    add_executable(rawrxd
        src/sovereign/rawrxd.cpp
        src/sovereign/ExecutionContract.cpp
        src/cpu_inference_engine.cpp
        src/rawrxd_inference.cpp
        src/rawrxd_model_loader.cpp
        src/rawrxd_tokenizer.cpp
        src/rawrxd_transformer.cpp
        src/rawrxd_sampler.cpp
        src/gguf_loader.cpp
        src/gguf_vocab_resolver.cpp
        src/core/inference_witness.cpp
        src/core/swarm_scheduler.cpp
        src/core/memory_stubs.cpp
        src/core/link_stubs.cpp
        src/core/gguf_swarm_plan_builder.cpp
        src/logging/Logger.cpp
        B014/build/b014_profiler.cpp
        src/runtime/StreamRouterAdapter.cpp
        src/runtime/gguf_tensor_loader.cpp
        src/runtime/Deep2ExecutionTelemetry.cpp
        src/runtime/elastic/ElasticEngine.cpp
        src/runtime/elastic/ElasticGGUFIndex.cpp
        src/runtime/elastic/ElasticResidencyManager.cpp
        src/runtime/elastic/VulkanTensorResidencyBackend.cpp
        src/runtime/elastic/ElasticDecoder.cpp
        src/runtime/governance/UnifiedTriggerOrchestrator.cpp
        src/runtime/governance/HardwareCapabilityProbe.cpp
        src/runtime/governance/CapabilityTierMatrix.cpp
        src/runtime/governance/HardwareGovernor.cpp
        src/runtime/governance/PressedStateEngine.cpp
        src/runtime/governance/Ensemble58Governor.cpp
        src/runtime/governance/TLSInstructionGate.cpp
        src/inference/NegativeSpaceProfiler.cpp
    )

target_include_directories(rawrxd PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/sovereign
    ${CMAKE_SOURCE_DIR}/src/runtime/governance
)

# Vulkan SDK support for Sovereign Runtime rawrxd target
if(RAWR_ENABLE_VULKAN AND RAWR_VULKAN_SDK_ROOT)
    message(STATUS "[SovereignRuntime] Vulkan SDK: ${RAWR_VULKAN_SDK_ROOT}")
    target_include_directories(rawrxd PRIVATE "${RAWR_VULKAN_INCLUDE}")
    target_link_directories(rawrxd PRIVATE "${RAWR_VULKAN_SDK_ROOT}/Lib")
    target_link_libraries(rawrxd PRIVATE "${RAWR_VULKAN_LIB}" dxgi)
    target_compile_definitions(rawrxd PRIVATE RAWR_HAS_VULKAN=1 RAWR_VULKAN_AVAILABLE=1)
elseif(RAWR_ENABLE_VULKAN AND VULKAN_INCLUDE_DIR)
    message(STATUS "[SovereignRuntime] Vulkan via VULKAN_INCLUDE_DIR")
    target_include_directories(rawrxd PRIVATE ${VULKAN_INCLUDE_DIR})
    target_link_libraries(rawrxd PRIVATE dxgi)
    target_compile_definitions(rawrxd PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN AND Vulkan_FOUND)
    message(STATUS "[SovereignRuntime] Vulkan via find_package")
    target_include_directories(rawrxd PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(rawrxd PRIVATE Vulkan::Vulkan dxgi)
    target_compile_definitions(rawrxd PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN)
    message(STATUS "[SovereignRuntime] Vulkan not found, using CPU fallback")
    target_compile_definitions(rawrxd PRIVATE RAWR_HAS_VULKAN=0)
    target_link_libraries(rawrxd PRIVATE dxgi)
else()
    target_link_libraries(rawrxd PRIVATE dxgi)
endif()

target_compile_features(rawrxd PRIVATE cxx_std_17)

if(WIN32)
    target_link_libraries(rawrxd PRIVATE
        Threads::Threads
    )
else()
    target_link_libraries(rawrxd PRIVATE Threads::Threads)
endif()

if(TARGET rawrxd_runtime)
    target_link_libraries(rawrxd PRIVATE rawrxd_runtime)
else()
    target_sources(rawrxd PRIVATE
        src/runtime/TensorExecutionRouter.cpp
        src/runtime/memory/PlacementPolicy.cpp
        src/runtime/memory/ResidencyTracker.cpp
        src/runtime/memory/WorkingSetPredictor.cpp
        src/runtime/memory/CapacityManager.cpp
        src/runtime/memory/TransferScheduler.cpp
        src/runtime/memory/TensorPlacementManager.cpp
        src/runtime/memory/PredictiveMemoryManager.cpp
        src/runtime/memory/RouterPredictiveMemoryBridge.cpp
    )
    target_include_directories(rawrxd PRIVATE
        ${CMAKE_SOURCE_DIR}/src/runtime
        ${CMAKE_SOURCE_DIR}/src/runtime/memory
    )
    target_compile_definitions(rawrxd PRIVATE RAWRXD_NO_VULKAN)
    message(STATUS "[SovereignRuntime] rawrxd_runtime target missing; linked router/PMM sources directly for B004")
endif()

set_target_properties(rawrxd PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

    set_target_properties(rawrxd PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    message(STATUS "[SovereignRuntime] Target: rawrxd (build with: cmake --build . --target rawrxd)")
    message(STATUS "[SovereignRuntime] Usage: rawrxd --model MODEL.gguf --prompt TEXT [--inference|--agentic|--validate]")
else()
    message(STATUS "[SovereignRuntime] Skipped - target 'rawrxd' already defined by CLI build")
endif()

# =============================================================================
# Validation Instrumentation (llama.cpp parity tensor dumping)
# Applied to whichever rawrxd target exists (GUI or Sovereign CLI)
# =============================================================================
if(RAWRXD_ENABLE_VALIDATION AND TARGET rawrxd)
    message(STATUS "[Validation] RAWRXD_ENABLE_VALIDATION=ON — runtime tensor dumping enabled")
    target_compile_definitions(rawrxd PRIVATE RAWRXD_ENABLE_VALIDATION=1)
    target_include_directories(rawrxd PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/inference_validation/harness
    )
elseif(TARGET rawrxd)
    message(STATUS "[Validation] RAWRXD_ENABLE_VALIDATION=OFF — runtime tensor dumping disabled")
endif()

# ============================================================================
# Native Address Resolver Utility (deterministic, no PowerShell dependency)
# ============================================================================
option(RAWRXD_BUILD_ADDRESS_RESOLVER_NATIVE "Build native crash address resolver utility" ON)
if(RAWRXD_BUILD_ADDRESS_RESOLVER_NATIVE AND EXISTS "${CMAKE_SOURCE_DIR}/tools/address_resolver_native.cpp")
    add_executable(address_resolver_native EXCLUDE_FROM_ALL
        tools/address_resolver_native.cpp
    )
    target_compile_features(address_resolver_native PRIVATE cxx_std_20)
    target_include_directories(address_resolver_native PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
    )
    if(MSVC)
        target_compile_options(address_resolver_native PRIVATE /W3 /EHsc)
        target_link_libraries(address_resolver_native PRIVATE dbghelp)
    else()
        target_compile_options(address_resolver_native PRIVATE -Wall -Wextra)
    endif()
    set_target_properties(address_resolver_native PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools
        OUTPUT_NAME address_resolver_native
    )

    add_custom_target(address_resolver_native_smoke
        COMMAND ${CMAKE_COMMAND}
            -DRESOLVER_EXE=${CMAKE_BINARY_DIR}/tools/address_resolver_native.exe
            -DMODULE_EXE=${CMAKE_BINARY_DIR}/bin/RawrXD-Win32IDE.exe
            -DMAP_FILE=${CMAKE_BINARY_DIR}/bin/RawrXD-Win32IDE.map
            -DALLOW_MISSING_ARTIFACTS=OFF
            -P ${CMAKE_SOURCE_DIR}/tools/address_resolver_native_smoke.cmake
        DEPENDS address_resolver_native
        COMMENT "Running native address resolver smoke check"
        VERBATIM
    )

    add_custom_target(address_resolver_native_smoke_soft
        COMMAND ${CMAKE_COMMAND}
            -DRESOLVER_EXE=${CMAKE_BINARY_DIR}/tools/address_resolver_native.exe
            -DMODULE_EXE=${CMAKE_BINARY_DIR}/bin/RawrXD-Win32IDE.exe
            -DMAP_FILE=${CMAKE_BINARY_DIR}/bin/RawrXD-Win32IDE.map
            -DALLOW_MISSING_ARTIFACTS=ON
            -P ${CMAKE_SOURCE_DIR}/tools/address_resolver_native_smoke.cmake
        DEPENDS address_resolver_native
        COMMENT "Running native address resolver smoke check (soft mode)"
        VERBATIM
    )

    add_custom_target(address_resolver_native_smoke_minimal
        COMMAND ${CMAKE_COMMAND}
            -DRESOLVER_EXE=${CMAKE_BINARY_DIR}/tools/address_resolver_native.exe
            -DMODULE_EXE=${CMAKE_BINARY_DIR}/bin/RawrXD-Win32IDE.exe
            -DMAP_FILE=${CMAKE_BINARY_DIR}/bin/RawrXD-Win32IDE.map
            -DFORCE_HELP_PROBE=ON
            -P ${CMAKE_SOURCE_DIR}/tools/address_resolver_native_smoke.cmake
        DEPENDS address_resolver_native
        COMMENT "Running native address resolver smoke check (minimal help probe)"
        VERBATIM
    )

    if(BUILD_TESTING)
        add_test(
            NAME address_resolver_native_smoke
            COMMAND ${CMAKE_COMMAND}
                -DRESOLVER_EXE=${CMAKE_BINARY_DIR}/tools/address_resolver_native.exe
                -DMODULE_EXE=${CMAKE_BINARY_DIR}/bin/RawrXD-Win32IDE.exe
                -DMAP_FILE=${CMAKE_BINARY_DIR}/bin/RawrXD-Win32IDE.map
                -DALLOW_MISSING_ARTIFACTS=ON
                -P ${CMAKE_SOURCE_DIR}/tools/address_resolver_native_smoke.cmake
        )

        add_test(
            NAME address_resolver_native_smoke_minimal
            COMMAND ${CMAKE_COMMAND}
                -DRESOLVER_EXE=${CMAKE_BINARY_DIR}/tools/address_resolver_native.exe
                -DMODULE_EXE=${CMAKE_BINARY_DIR}/bin/RawrXD-Win32IDE.exe
                -DMAP_FILE=${CMAKE_BINARY_DIR}/bin/RawrXD-Win32IDE.map
                -DFORCE_HELP_PROBE=ON
                -P ${CMAKE_SOURCE_DIR}/tools/address_resolver_native_smoke.cmake
        )
    endif()

    message(STATUS "[Tools] Native address resolver target enabled: address_resolver_native (smoke targets: address_resolver_native_smoke, address_resolver_native_smoke_soft, address_resolver_native_smoke_minimal)")
endif()

# ============================================================================
# RawrXD-Script LSP Server (VS Code Extension Support)
# ============================================================================
option(RAWRXD_BUILD_LSP_SERVER "Build RawrXD-Script LSP server for VS Code extension" ON)
if(RAWRXD_BUILD_LSP_SERVER AND EXISTS "${CMAKE_SOURCE_DIR}/src/lsp/RawrXD_LSPServer.cpp")
    # Minimal LSP server - core functionality only (avoid hotpatch dependencies)
    add_executable(RawrXD_LSPServer
        src/lsp/RawrXD_LSPServer.cpp
    )
    target_compile_features(RawrXD_LSPServer PRIVATE cxx_std_20)
    target_include_directories(RawrXD_LSPServer BEFORE PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
    )
    if(MSVC)
        target_compile_options(RawrXD_LSPServer PRIVATE /W3 /EHsc
            $<$<NOT:$<CONFIG:Debug>>:/O2>
        )
    else()
        target_compile_options(RawrXD_LSPServer PRIVATE -Wall -Wextra -O2)
    endif()
    set_target_properties(RawrXD_LSPServer PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME RawrXD_LSPServer
    )
    message(STATUS "[LSP] RawrXD-Script Language Server target enabled: RawrXD_LSPServer")
endif()

# ============================================================================
# RawrXD-Script DAP Adapter (VS Code Extension Support)
# ============================================================================
option(RAWRXD_BUILD_DAP_ADAPTER "Build RawrXD-Script DAP adapter for VS Code extension" ON)
if(RAWRXD_BUILD_DAP_ADAPTER AND EXISTS "${CMAKE_SOURCE_DIR}/src/script/debug/minimal_dap_server.cpp")
    add_executable(RawrXDScriptDAPAdapter
        src/script/debug/minimal_dap_server.cpp
    )
    target_compile_features(RawrXDScriptDAPAdapter PRIVATE cxx_std_20)
    target_include_directories(RawrXDScriptDAPAdapter BEFORE PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
    )
    if(MSVC)
        target_compile_options(RawrXDScriptDAPAdapter PRIVATE /W3 /EHsc $<$<NOT:$<CONFIG:Debug>>:/O2>)
    else()
        target_compile_options(RawrXDScriptDAPAdapter PRIVATE -Wall -Wextra -O2)
    endif()
    set_target_properties(RawrXDScriptDAPAdapter PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME RawrXDScriptDAPAdapter
    )
    message(STATUS "[DAP] RawrXD-Script Debug Adapter target enabled: RawrXDScriptDAPAdapter")
endif()

# ============================================================================
# Runtime Component (Predictive Memory / Placement)
# ============================================================================
if(EXISTS "${CMAKE_SOURCE_DIR}/src/runtime/CMakeLists.txt")
    add_subdirectory(src/runtime)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/CMakeLists.txt")
    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================================================
# Sovereign Bridge Test Harness
# ============================================================================
add_executable(TestSovereignBridge EXCLUDE_FROM_ALL
    src/ide/test_sovereign_bridge.cpp
    src/ide/SovereignSharedMemoryBridge.cpp
)
target_compile_features(TestSovereignBridge PRIVATE cxx_std_17)
target_include_directories(TestSovereignBridge PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/ide
    ${CMAKE_SOURCE_DIR}/include
)
if(WIN32)
    target_link_libraries(TestSovereignBridge PRIVATE)
endif()
set_target_properties(TestSovereignBridge PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)
message(STATUS "[Test] Sovereign Bridge test harness: TestSovereignBridge (build with: cmake --build . --target TestSovereignBridge)")

# ============================================================================
# DebugBridge Telemetry Test
# ============================================================================
add_executable(TestDebugBridgeTelemetry EXCLUDE_FROM_ALL
    src/debug/test_debugbridge_telemetry.cpp
)
target_compile_features(TestDebugBridgeTelemetry PRIVATE cxx_std_17)
set_target_properties(TestDebugBridgeTelemetry PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)
message(STATUS "[Test] DebugBridge telemetry test: TestDebugBridgeTelemetry")

# ============================================================================
# Pure MASM Inference Test
# ============================================================================
add_executable(TestMASMInference EXCLUDE_FROM_ALL
    src/test_masm_inference.cpp
    src/gguf_masm_weight_bridge.cpp
)
target_compile_features(TestMASMInference PRIVATE cxx_std_17)
target_include_directories(TestMASMInference PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/masm
    ${CMAKE_SOURCE_DIR}/include
)
if(RAWR_HAS_MASM)
    target_sources(TestMASMInference PRIVATE
        src/masm/rawrxd_math_masm.asm
        src/masm/rawrxd_transformer_masm_fixed.asm
        src/masm/rawrxd_transformer_full.asm
    )
    set_source_files_properties(
        src/masm/rawrxd_math_masm.asm
        src/masm/rawrxd_transformer_masm_fixed.asm
        src/masm/rawrxd_transformer_full.asm
        PROPERTIES LANGUAGE ASM_MASM
    )
endif()
if(WIN32)
    target_link_libraries(TestMASMInference PRIVATE)
endif()
set_target_properties(TestMASMInference PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)
message(STATUS "[Test] Pure MASM inference test: TestMASMInference")

# ============================================================================
# Debugger Stress Tests
# ============================================================================
add_executable(stress_target EXCLUDE_FROM_ALL
    src/debug/stress_target.cpp
)
target_compile_features(stress_target PRIVATE cxx_std_17)
set_target_properties(stress_target PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)

add_executable(stress_memory EXCLUDE_FROM_ALL
    src/debug/stress_memory.cpp
)
target_compile_features(stress_memory PRIVATE cxx_std_17)
set_target_properties(stress_memory PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)

message(STATUS "[Test] Debugger stress tests: stress_target, stress_memory")

# ============================================================================
# SovereignRPC Simulator Test
# ============================================================================
add_executable(test_simulator EXCLUDE_FROM_ALL
    src/rpc/test_simulator.cpp
    src/rpc/SovereignRPC_Simulator.cpp
    src/rpc/SovereignRPC_Scheduler.cpp
)
target_compile_features(test_simulator PRIVATE cxx_std_17)
target_include_directories(test_simulator PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/rpc
    ${CMAKE_SOURCE_DIR}/src/bridge
    ${CMAKE_SOURCE_DIR}/src/scheduler
)
set_target_properties(test_simulator PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)
message(STATUS "[Test] SovereignRPC Simulator: test_simulator (build with: cmake --build . --target test_simulator)")

# ============================================================================
# Q4_0 Preprocessed Kernel Validation Tests (VAL-Q4.0)
# ============================================================================
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/test_q4_fused_pipeline.cpp")
    add_executable(test_q4_fused_pipeline EXCLUDE_FROM_ALL
        tests/test_q4_fused_pipeline.cpp
        src/memory/Q4WeightPreprocess.cpp
    )
    target_compile_features(test_q4_fused_pipeline PRIVATE cxx_std_20)
    target_include_directories(test_q4_fused_pipeline PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/memory
    )
    if(WIN32)
        target_link_libraries(test_q4_fused_pipeline PRIVATE)
    endif()
    set_target_properties(test_q4_fused_pipeline PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    message(STATUS "[VAL-Q4.0] Fused pipeline validation: test_q4_fused_pipeline")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/test_q4_cache_alignment.cpp")
    add_executable(test_q4_cache_alignment EXCLUDE_FROM_ALL
        tests/test_q4_cache_alignment.cpp
        src/memory/Q4WeightPreprocess.cpp
    )
    target_compile_features(test_q4_cache_alignment PRIVATE cxx_std_20)
    target_include_directories(test_q4_cache_alignment PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/memory
    )
    if(WIN32)
        target_link_libraries(test_q4_cache_alignment PRIVATE)
    endif()
    set_target_properties(test_q4_cache_alignment PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    message(STATUS "[VAL-Q4.0] Cache alignment validation: test_q4_cache_alignment")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/test_q4_asm_debug.cpp")
    add_executable(test_q4_asm_debug EXCLUDE_FROM_ALL
        tests/test_q4_asm_debug.cpp
        src/memory/Q4WeightPreprocess.cpp
    )
    target_compile_features(test_q4_asm_debug PRIVATE cxx_std_20)
    target_include_directories(test_q4_asm_debug PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/memory
    )
    if(WIN32)
        target_link_libraries(test_q4_asm_debug PRIVATE)
    endif()
    set_target_properties(test_q4_asm_debug PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    message(STATUS "[VAL-Q4.0] ASM debug test: test_q4_asm_debug")
endif()

# Q4_0 AVX-512 kernel library - must be defined before tests that link to it
if(EXISTS "${CMAKE_SOURCE_DIR}/src/kernels/q4_preprocessed_avx512.asm")
    enable_language(ASM_MASM)
    add_library(q4_preprocessed_avx512 STATIC
        src/kernels/q4_preprocessed_avx512.asm
    )
    set_target_properties(q4_preprocessed_avx512 PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    )
    message(STATUS "[VAL-Q4.0] AVX-512 kernel: q4_preprocessed_avx512")
endif()

# Link tests to the Q4 kernel library (after it's defined)
if(TARGET q4_preprocessed_avx512)
    if(TARGET test_q4_fused_pipeline)
        target_link_libraries(test_q4_fused_pipeline PRIVATE q4_preprocessed_avx512)
    endif()
    if(TARGET test_q4_asm_debug)
        target_link_libraries(test_q4_asm_debug PRIVATE q4_preprocessed_avx512)
    endif()
endif()

# Scalar SIMD simulation test (no ASM dependency)
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/test_q4_scalar_simd.cpp")
    add_executable(test_q4_scalar_simd EXCLUDE_FROM_ALL
        tests/test_q4_scalar_simd.cpp
        src/memory/Q4WeightPreprocess.cpp
    )
    target_compile_features(test_q4_scalar_simd PRIVATE cxx_std_20)
    target_include_directories(test_q4_scalar_simd PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/memory
    )
    set_target_properties(test_q4_scalar_simd PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    message(STATUS "[VAL-Q4.0] Scalar SIMD simulation: test_q4_scalar_simd")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/src/tools/CMakeLists.txt")
    add_subdirectory(src/tools)
endif()

# Validation Harness
if(EXISTS "${CMAKE_SOURCE_DIR}/src/validation/CMakeLists.txt")
    add_subdirectory(src/validation)
endif()

# =============================================================================
# VAL-051 Witness System Test
# =============================================================================
add_executable(witness_system_test EXCLUDE_FROM_ALL
    src/validation/witness_system_test.cpp
    src/core/inference_witness.cpp
)
target_include_directories(witness_system_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_compile_features(witness_system_test PRIVATE cxx_std_17)

# =============================================================================
# MEASUREMENT FRAMEWORK - Rigorous Validation Tests
# =============================================================================
message(STATUS "========================================")
message(STATUS "Measurement Framework Tests")
message(STATUS "========================================")

# 1. Dispatch Overhead Benchmark
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/benchmark_dispatch_overhead.cpp")
    add_executable(benchmark_dispatch_overhead EXCLUDE_FROM_ALL
        tests/benchmark_dispatch_overhead.cpp
        src/nevm/nevm_tensor_descriptor.cpp
        src/nevm/nevm_kernel_bridge.cpp
        src/memory/Q4WeightPreprocess.cpp
    )
    target_compile_features(benchmark_dispatch_overhead PRIVATE cxx_std_20)
    target_include_directories(benchmark_dispatch_overhead PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/nevm
        ${CMAKE_SOURCE_DIR}/src/memory
        ${CMAKE_SOURCE_DIR}/src/kernels
    )
    set_target_properties(benchmark_dispatch_overhead PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    message(STATUS "  [MEASURE] benchmark_dispatch_overhead - RDTSC dispatch latency")
endif()

# 2. Planner Amortization Benchmark
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/benchmark_planner_amortization.cpp")
    add_executable(benchmark_planner_amortization EXCLUDE_FROM_ALL
        tests/benchmark_planner_amortization.cpp
        src/nevm/nevm_execution_plan.cpp
        src/nevm/nevm_kernel_bridge.cpp
    )
    target_compile_features(benchmark_planner_amortization PRIVATE cxx_std_20)
    target_include_directories(benchmark_planner_amortization PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/nevm
        ${CMAKE_SOURCE_DIR}/src/kernels
    )
    set_target_properties(benchmark_planner_amortization PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    message(STATUS "  [MEASURE] benchmark_planner_amortization - Break-even analysis")
endif()

# 3. End-to-End Validation (Four Gates)
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/validate_end_to_end.cpp")
    add_executable(validate_end_to_end EXCLUDE_FROM_ALL
        tests/validate_end_to_end.cpp
        src/nevm/nevm_execution_plan.cpp
        src/nevm/nevm_tensor_descriptor.cpp
        src/nevm/nevm_kernel_bridge.cpp
        src/memory/Q4WeightPreprocess.cpp
    )
    target_compile_features(validate_end_to_end PRIVATE cxx_std_20)
    target_include_directories(validate_end_to_end PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/nevm
        ${CMAKE_SOURCE_DIR}/src/memory
        ${CMAKE_SOURCE_DIR}/src/kernels
    )
    set_target_properties(validate_end_to_end PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    message(STATUS "  [MEASURE] validate_end_to_end - Four independent gates")
endif()

# 4. NEVM Determinism Validation
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/nevm_determinism_validation.cpp")
    add_executable(nevm_determinism_validation EXCLUDE_FROM_ALL
        tests/nevm_determinism_validation.cpp
        src/nevm/nevm_execution_plan.cpp
        src/nevm/nevm_kernel_bridge.cpp
    )
    target_compile_features(nevm_determinism_validation PRIVATE cxx_std_20)
    target_include_directories(nevm_determinism_validation PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/nevm
        ${CMAKE_SOURCE_DIR}/src/kernels
    )
    if(WIN32)
        target_link_libraries(nevm_determinism_validation PRIVATE psapi)
    endif()
    set_target_properties(nevm_determinism_validation PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    message(STATUS "  [MEASURE] nevm_determinism_validation - Thread affinity + FNV-1a hashing")
endif()

# 5. NEVM Stress Test
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/nevm_stress_test.cpp")
    add_executable(nevm_stress_test EXCLUDE_FROM_ALL
        tests/nevm_stress_test.cpp
        src/nevm/nevm_execution_plan.cpp
        src/nevm/nevm_mmu.cpp
    )
    target_compile_features(nevm_stress_test PRIVATE cxx_std_20)
    target_include_directories(nevm_stress_test PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/nevm
    )
    if(WIN32)
        target_link_libraries(nevm_stress_test PRIVATE psapi)
    endif()
    set_target_properties(nevm_stress_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    message(STATUS "  [MEASURE] nevm_stress_test - 100K token stress + RSS monitoring")
endif()

# 6. PMC Profiler Library
if(EXISTS "${CMAKE_SOURCE_DIR}/src/profiling/pmc_profiler.cpp")
    add_library(pmc_profiler STATIC
        src/profiling/pmc_profiler.cpp
    )
    target_compile_features(pmc_profiler PRIVATE cxx_std_20)
    target_include_directories(pmc_profiler PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    message(STATUS "  [MEASURE] pmc_profiler - Performance Monitoring Counters")
endif()

# 6b. Flame Graph Profiler Library
if(EXISTS "${CMAKE_SOURCE_DIR}/src/profiling/flame_graph.cpp")
    add_library(flame_graph_profiler STATIC
        src/profiling/flame_graph.cpp
    )
    target_compile_features(flame_graph_profiler PRIVATE cxx_std_20)
    target_include_directories(flame_graph_profiler PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    if(WIN32)
        target_link_libraries(flame_graph_profiler PRIVATE dbghelp)
    endif()
    message(STATUS "  [MEASURE] flame_graph_profiler - Flame graph generation")
endif()

# 7. PMC Dispatch Benchmark
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/benchmark_dispatch_pmc.cpp")
    add_executable(benchmark_dispatch_pmc EXCLUDE_FROM_ALL
        tests/benchmark_dispatch_pmc.cpp
        src/nevm/nevm_tensor_descriptor.cpp
        src/nevm/nevm_kernel_bridge.cpp
        src/memory/Q4WeightPreprocess.cpp
    )
    target_compile_features(benchmark_dispatch_pmc PRIVATE cxx_std_20)
    target_include_directories(benchmark_dispatch_pmc PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/nevm
        ${CMAKE_SOURCE_DIR}/src/memory
        ${CMAKE_SOURCE_DIR}/src/kernels
        ${CMAKE_SOURCE_DIR}/src/profiling
    )
    if(TARGET pmc_profiler)
        target_link_libraries(benchmark_dispatch_pmc PRIVATE pmc_profiler)
    endif()
    set_target_properties(benchmark_dispatch_pmc PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    message(STATUS "  [MEASURE] benchmark_dispatch_pmc - PMC profiling")
endif()

message(STATUS "========================================")

# =============================================================================
# Native Toolchain Build (separate project)
# =============================================================================
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists_native.txt")
    # Native build is configured separately via CMakeLists_native.txt
    message(STATUS "[Native] Separate native toolchain build available")
endif()

# =============================================================================
# MASM Components
# =============================================================================

# =============================================================================
# SoloIDE — Sovereign Agentic Desktop Environment
# =============================================================================
set(SOLOIDE_SOURCES
    src/soloide/main.cpp
    src/soloide/Core/Bus.cpp
    src/soloide/UI/SoloIDE.cpp
    src/soloide/UI/Panels/EditorPanel.cpp
    src/soloide/UI/Panels/ChatPanel.cpp
    src/soloide/UI/Panels/DebugPanel.cpp
    src/soloide/UI/Panels/TerminalPanel.cpp
    src/soloide/UI/Panels/ProjectPanel.cpp
    src/soloide/UI/Panels/StatusPanel.cpp
    src/soloide/AI/CompletionEngine.cpp
    src/soloide/AI/AdvancedCodingAgent.cpp
    src/soloide/AI/MultiModalModelRouter.cpp
    src/soloide/Debug/DAPClient.cpp
    src/soloide/LSP/LSPClient.cpp
)

set(SOLOIDE_HEADERS
    src/soloide/Core/Bus.hpp
    src/soloide/UI/SoloIDE.hpp
    src/soloide/UI/Panels/EditorPanel.hpp
    src/soloide/UI/Panels/ChatPanel.hpp
    src/soloide/UI/Panels/DebugPanel.hpp
    src/soloide/UI/Panels/TerminalPanel.hpp
    src/soloide/UI/Panels/ProjectPanel.hpp
    src/soloide/UI/Panels/StatusPanel.hpp
    src/soloide/AI/CompletionEngine.hpp
    src/soloide/AI/AdvancedCodingAgent.hpp
    src/soloide/AI/MultiModalModelRouter.hpp
    src/soloide/Debug/DAPClient.hpp
    src/soloide/LSP/LSPClient.hpp
)

if(EXISTS "${CMAKE_SOURCE_DIR}/src/soloide/main.cpp")
    # Find Qt6 — try qmake-based detection first, then manual paths
    find_package(Qt6 COMPONENTS Widgets Network Core QUIET)
    if(NOT Qt6_FOUND)
        find_package(Qt6 COMPONENTS Widgets Network Core
            PATHS
                "C:/Qt/6.7.3/msvc2022_64"
                "C:/Qt/6.8.0/msvc2022_64"
                "C:/Qt/6.7.0/msvc2022_64"
                "C:/Qt/6.6.0/msvc2022_64"
                "C:/Qt/6.5.0/msvc2022_64"
                "D:/Qt/6.8.0/msvc2022_64"
                "D:/Qt/6.7.0/msvc2022_64"
            NO_DEFAULT_PATH
        )
    endif()

    if(Qt6_FOUND)
        add_executable(RawrXD-SoloIDE WIN32 ${SOLOIDE_SOURCES} ${SOLOIDE_HEADERS})
        target_link_libraries(RawrXD-SoloIDE PRIVATE
            Qt6::Widgets Qt6::Network Qt6::Core
        )
        target_include_directories(RawrXD-SoloIDE PRIVATE
            ${CMAKE_SOURCE_DIR}/src/soloide
        )
        target_compile_features(RawrXD-SoloIDE PRIVATE cxx_std_20)
        set_target_properties(RawrXD-SoloIDE PROPERTIES
            OUTPUT_NAME "RawrXD-SoloIDE"
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        )
        message(STATUS "[SoloIDE] RawrXD-SoloIDE target configured with Qt6")
    else()
        message(WARNING "[SoloIDE] Qt6 not found — SoloIDE target skipped. Install Qt6 or set CMAKE_PREFIX_PATH.")
    endif()
endif()
add_executable(test_generation tests/test_generation.cpp src/adapter/RawrXDEngineAdapter.cpp src/deep2/Deep2Engine.cpp src/deep2/Deep2Bridge.cpp src/deep2/FabricTensorTable.cpp src/deep2/Deep2IDEIntegration.cpp src/deep2/InferenceSession.cpp src/deep2/ReverseIntegration.cpp src/deep2/ReverseHotpatchEngine.cpp src/deep2/DualGPUHook.cpp src/deep2/MoERouter.cpp src/deep2/MoEWeightProxy.cpp src/deep2/MoEWeightsLoader.cpp src/deep2/MoEArchitectureParser.cpp src/deep2/MedusaDecoder.cpp src/deep2/NUFusedPacker.cpp src/deep2/WarmupScheduler.cpp src/deep2/CompressedKVCache.cpp src/deep2/NVMeStream.cpp src/deep2/SlidingWindowEngine.cpp src/deep2/QuantKernelRegistry.cpp src/deep2/ThreadPool.cpp src/deep2/KVCache.cpp src/deep2/GGUFLoader.cpp src/deep2/RawrXDInferenceAdapter.cpp src/inference/RawrXD_LlamaNative.cpp src/deep2/ModelRegistry.cpp src/deep2/mars/MARSController.cpp src/deep2/mars/TensorHotpatch.cpp src/deep2/mars/DualGPUBackend.cpp src/deep2/mars/VRAMManager.cpp src/deep2/HotPatcher.cpp src/deep2/HotPatcherSafety.cpp tests/deep2_link_stubs.cpp src/core/memory_stubs.cpp src/core/link_stubs.cpp src/reverse/ReverseEngine.cpp src/reverse/ReverseModelLoader.cpp)
set_target_properties(test_generation PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
target_link_libraries(test_generation PRIVATE InferenceEngine)
add_test(NAME test_generation COMMAND test_generation WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

add_executable(test_llama_native tests/test_llama_native.cpp
    src/deep2/Deep2Bridge.cpp
    src/deep2/Deep2Engine.cpp
    src/deep2/GGUFLoader.cpp
    src/deep2/RawrXDInferenceAdapter.cpp
    src/inference/RawrXD_LlamaNative.cpp
    src/deep2/InferenceSession.cpp
    src/deep2/ReverseIntegration.cpp
    src/deep2/ReverseHotpatchEngine.cpp
    src/deep2/DualGPUHook.cpp
    src/deep2/MoERouter.cpp
    src/deep2/MoEWeightProxy.cpp
    src/deep2/MoEWeightsLoader.cpp
    src/deep2/MoEArchitectureParser.cpp
    src/deep2/MedusaDecoder.cpp
    src/deep2/NUFusedPacker.cpp
    src/deep2/WarmupScheduler.cpp
    src/deep2/CompressedKVCache.cpp
    src/deep2/NVMeStream.cpp
    src/deep2/SlidingWindowEngine.cpp
    src/deep2/QuantKernelRegistry.cpp
    src/deep2/ThreadPool.cpp
    src/deep2/KVCache.cpp
    src/deep2/ModelRegistry.cpp
    src/deep2/mars/MARSController.cpp
    src/deep2/mars/TensorHotpatch.cpp
    src/deep2/mars/DualGPUBackend.cpp
    src/deep2/mars/VRAMManager.cpp
    src/deep2/HotPatcher.cpp
    src/deep2/HotPatcherSafety.cpp
    tests/deep2_link_stubs.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp
    src/reverse/ReverseEngine.cpp
    src/reverse/ReverseModelLoader.cpp
)
set_target_properties(test_llama_native PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
target_link_libraries(test_llama_native PRIVATE InferenceEngine)
add_test(NAME test_llama_native COMMAND test_llama_native WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# K2-001 Runtime Validation Gate
add_executable(k2_runtime_validation tests/k2_runtime_validation.cpp
    src/adapter/RawrXDEngineAdapter.cpp
    src/deep2/K2MLAWeights.cpp
    src/deep2/K2MoEWeights.cpp
    src/deep2/K2GlobalTensorIndex.cpp
    src/deep2/KimiK2Config.cpp
    src/deep2/Deep2Bridge.cpp
    src/deep2/Deep2Engine.cpp
    src/deep2/GGUFLoader.cpp
    src/deep2/RawrXDInferenceAdapter.cpp
    src/deep2/InferenceSession.cpp
    src/deep2/QuantKernelRegistry.cpp
    src/deep2/ThreadPool.cpp
    src/deep2/KVCache.cpp
    src/deep2/ModelRegistry.cpp
    src/deep2/FabricTensorTable.cpp
    src/deep2/Deep2IDEIntegration.cpp
    src/deep2/ReverseIntegration.cpp
    src/deep2/ReverseHotpatchEngine.cpp
    src/deep2/DualGPUHook.cpp
    src/deep2/MoERouter.cpp
    src/deep2/MoEWeightProxy.cpp
    src/deep2/MoEWeightsLoader.cpp
    src/deep2/MoEArchitectureParser.cpp
    src/deep2/MedusaDecoder.cpp
    src/deep2/NUFusedPacker.cpp
    src/deep2/WarmupScheduler.cpp
    src/deep2/CompressedKVCache.cpp
    src/deep2/NVMeStream.cpp
    src/deep2/SlidingWindowEngine.cpp
    src/deep2/mars/MARSController.cpp
    src/deep2/mars/TensorHotpatch.cpp
    src/deep2/mars/DualGPUBackend.cpp
    src/deep2/mars/VRAMManager.cpp
    src/deep2/HotPatcher.cpp
    src/deep2/HotPatcherSafety.cpp
    src/deep2/MASMKernelStubs.cpp
    src/deep2/sovereign_q4k_gemv.asm
    src/deep2/sovereign_deep2_kernels.asm
    src/inference/RawrXD_LlamaNative.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp
    src/reverse/ReverseEngine.cpp
    src/reverse/ReverseModelLoader.cpp)
set_target_properties(k2_runtime_validation PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
target_link_libraries(k2_runtime_validation PRIVATE InferenceEngine)
add_test(NAME k2_runtime_validation COMMAND k2_runtime_validation WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# Q4_K Block Decoder Verification Test
# Must pass before Q4_K GEMV is re-enabled in K2-003 Gate 6b
add_executable(test_q4k_decoder tests/test_q4k_decoder.cpp
    src/deep2/K2GlobalTensorIndex.cpp
    src/deep2/KimiK2Config.cpp
    src/deep2/GGUFLoader.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp)
set_target_properties(test_q4k_decoder PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
target_link_libraries(test_q4k_decoder PRIVATE InferenceEngine)
add_test(NAME test_q4k_decoder COMMAND test_q4k_decoder WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# K2-002 Native Deep2 Runtime Smoke Test
# Exercises real native kernels (RMSNormW, LinearW Q4_K GEMV) with bounded residency
add_executable(k2_native_smoke_test tests/k2_native_smoke_test.cpp
    src/deep2/K2GlobalTensorIndex.cpp
    src/deep2/K2MLAWeights.cpp
    src/deep2/K2MoEWeights.cpp
    src/deep2/KimiK2Config.cpp
    src/deep2/Deep2Engine.cpp
    src/deep2/GGUFLoader.cpp
    src/deep2/QuantKernelRegistry.cpp
    src/deep2/ThreadPool.cpp
    src/deep2/KVCache.cpp
    src/deep2/ModelRegistry.cpp
    src/deep2/FabricTensorTable.cpp
    src/deep2/Deep2IDEIntegration.cpp
    src/deep2/ReverseIntegration.cpp
    src/deep2/ReverseHotpatchEngine.cpp
    src/deep2/DualGPUHook.cpp
    src/deep2/MoERouter.cpp
    src/deep2/MoEWeightProxy.cpp
    src/deep2/MoEWeightsLoader.cpp
    src/deep2/MoEArchitectureParser.cpp
    src/deep2/MedusaDecoder.cpp
    src/deep2/NUFusedPacker.cpp
    src/deep2/WarmupScheduler.cpp
    src/deep2/CompressedKVCache.cpp
    src/deep2/NVMeStream.cpp
    src/deep2/SlidingWindowEngine.cpp
    src/deep2/mars/MARSController.cpp
    src/deep2/mars/TensorHotpatch.cpp
    src/deep2/mars/DualGPUBackend.cpp
    src/deep2/mars/VRAMManager.cpp
    src/deep2/HotPatcher.cpp
    src/deep2/HotPatcherSafety.cpp
    src/deep2/MASMKernelStubs.cpp
    src/deep2/sovereign_q4k_gemv.asm
    src/deep2/sovereign_deep2_kernels.asm
    src/inference/RawrXD_LlamaNative.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp
    src/reverse/ReverseEngine.cpp
    src/reverse/ReverseModelLoader.cpp)
set_target_properties(k2_native_smoke_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
target_link_libraries(k2_native_smoke_test PRIVATE InferenceEngine)
add_test(NAME k2_native_smoke_test COMMAND k2_native_smoke_test WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# K2-002: Bounded Tensor Streaming Validation
add_executable(k2_002_tensor_streaming tests/k2_002_tensor_streaming.cpp
    src/deep2/K2GlobalTensorIndex.cpp
    src/deep2/K2MLAWeights.cpp
    src/deep2/K2MoEWeights.cpp
    src/deep2/KimiK2Config.cpp
    src/deep2/GGUFLoader.cpp
    src/deep2/Deep2Bridge.cpp
    src/deep2/Deep2Engine.cpp
    src/deep2/RawrXDInferenceAdapter.cpp
    src/deep2/InferenceSession.cpp
    src/deep2/QuantKernelRegistry.cpp
    src/deep2/ThreadPool.cpp
    src/deep2/KVCache.cpp
    src/deep2/ModelRegistry.cpp
    src/deep2/FabricTensorTable.cpp
    src/deep2/Deep2IDEIntegration.cpp
    src/deep2/ReverseIntegration.cpp
    src/deep2/ReverseHotpatchEngine.cpp
    src/deep2/DualGPUHook.cpp
    src/deep2/MoERouter.cpp
    src/deep2/MoEWeightProxy.cpp
    src/deep2/MoEWeightsLoader.cpp
    src/deep2/MoEArchitectureParser.cpp
    src/deep2/MedusaDecoder.cpp
    src/deep2/NUFusedPacker.cpp
    src/deep2/WarmupScheduler.cpp
    src/deep2/CompressedKVCache.cpp
    src/deep2/NVMeStream.cpp
    src/deep2/SlidingWindowEngine.cpp
    src/deep2/mars/MARSController.cpp
    src/deep2/mars/TensorHotpatch.cpp
    src/deep2/mars/DualGPUBackend.cpp
    src/deep2/mars/VRAMManager.cpp
    src/deep2/HotPatcher.cpp
    src/deep2/HotPatcherSafety.cpp
    src/inference/RawrXD_LlamaNative.cpp
    tests/deep2_link_stubs.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp
    src/reverse/ReverseEngine.cpp
    src/reverse/ReverseModelLoader.cpp)
set_target_properties(k2_002_tensor_streaming PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
target_link_libraries(k2_002_tensor_streaming PRIVATE InferenceEngine)
add_test(NAME k2_002_tensor_streaming COMMAND k2_002_tensor_streaming WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# K2-003: Native Deep2 Layer Execution
add_executable(k2_003_layer_execution tests/k2_003_layer_execution.cpp
    src/deep2/K2GlobalTensorIndex.cpp
    src/deep2/K2MLAWeights.cpp
    src/deep2/K2MoEWeights.cpp
    src/deep2/KimiK2Config.cpp
    src/deep2/GGUFLoader.cpp
    src/deep2/Deep2Bridge.cpp
    src/deep2/Deep2Engine.cpp
    src/deep2/RawrXDInferenceAdapter.cpp
    src/deep2/InferenceSession.cpp
    src/deep2/QuantKernelRegistry.cpp
    src/deep2/ThreadPool.cpp
    src/deep2/KVCache.cpp
    src/deep2/ModelRegistry.cpp
    src/deep2/FabricTensorTable.cpp
    src/deep2/Deep2IDEIntegration.cpp
    src/deep2/ReverseIntegration.cpp
    src/deep2/ReverseHotpatchEngine.cpp
    src/deep2/DualGPUHook.cpp
    src/deep2/MoERouter.cpp
    src/deep2/MoEWeightProxy.cpp
    src/deep2/MoEWeightsLoader.cpp
    src/deep2/MoEArchitectureParser.cpp
    src/deep2/MedusaDecoder.cpp
    src/deep2/NUFusedPacker.cpp
    src/deep2/WarmupScheduler.cpp
    src/deep2/CompressedKVCache.cpp
    src/deep2/NVMeStream.cpp
    src/deep2/SlidingWindowEngine.cpp
    src/deep2/mars/MARSController.cpp
    src/deep2/mars/TensorHotpatch.cpp
    src/deep2/mars/DualGPUBackend.cpp
    src/deep2/mars/VRAMManager.cpp
    src/deep2/HotPatcher.cpp
    src/deep2/HotPatcherSafety.cpp
    src/inference/RawrXD_LlamaNative.cpp
    tests/deep2_link_stubs.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp
    src/reverse/ReverseEngine.cpp
    src/reverse/ReverseModelLoader.cpp)
set_target_properties(k2_003_layer_execution PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
target_link_libraries(k2_003_layer_execution PRIVATE InferenceEngine)
add_test(NAME k2_003_layer_execution COMMAND k2_003_layer_execution WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# K2-003B: Native GEMV Validation Against Real K2 Q4_K Weights
# Must pass after K2-003A (Q4_K decoder certified) before full layer execution
add_executable(k2_003b_native_gemv_validation tests/k2_003b_native_gemv_validation.cpp
    src/deep2/K2GlobalTensorIndex.cpp
    src/deep2/K2MLAWeights.cpp
    src/deep2/KimiK2Config.cpp
    src/deep2/GGUFLoader.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp)
set_target_properties(k2_003b_native_gemv_validation PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
target_link_libraries(k2_003b_native_gemv_validation PRIVATE InferenceEngine)
add_test(NAME k2_003b_native_gemv_validation COMMAND k2_003b_native_gemv_validation WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# K2-003C: Single-Layer Forward with Real K2 Weights (Bounded)
# Proves one complete transformer layer executes on real Q4_K weights
add_executable(k2_003c_single_layer_forward tests/k2_003c_single_layer_forward.cpp
    src/deep2/K2GlobalTensorIndex.cpp
    src/deep2/K2MLAWeights.cpp
    src/deep2/KimiK2Config.cpp
    src/deep2/GGUFLoader.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp)
set_target_properties(k2_003c_single_layer_forward PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
target_link_libraries(k2_003c_single_layer_forward PRIVATE InferenceEngine)
add_test(NAME k2_003c_single_layer_forward COMMAND k2_003c_single_layer_forward WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# K2-004: Bounded Multi-Layer Prefill
add_executable(k2_004_bounded_prefill tests/k2_004_bounded_prefill.cpp
    src/deep2/K2GlobalTensorIndex.cpp
    src/deep2/K2MLAWeights.cpp
    src/deep2/K2MoEWeights.cpp
    src/deep2/KimiK2Config.cpp
    src/deep2/GGUFLoader.cpp
    src/deep2/Deep2Bridge.cpp
    src/deep2/Deep2Engine.cpp
    src/deep2/RawrXDInferenceAdapter.cpp
    src/deep2/InferenceSession.cpp
    src/deep2/QuantKernelRegistry.cpp
    src/deep2/ThreadPool.cpp
    src/deep2/KVCache.cpp
    src/deep2/ModelRegistry.cpp
    src/deep2/FabricTensorTable.cpp
    src/deep2/Deep2IDEIntegration.cpp
    src/deep2/ReverseIntegration.cpp
    src/deep2/ReverseHotpatchEngine.cpp
    src/deep2/DualGPUHook.cpp
    src/deep2/MoERouter.cpp
    src/deep2/MoEWeightProxy.cpp
    src/deep2/MoEWeightsLoader.cpp
    src/deep2/MoEArchitectureParser.cpp
    src/deep2/MedusaDecoder.cpp
    src/deep2/NUFusedPacker.cpp
    src/deep2/WarmupScheduler.cpp
    src/deep2/CompressedKVCache.cpp
    src/deep2/NVMeStream.cpp
    src/deep2/SlidingWindowEngine.cpp
    src/deep2/mars/MARSController.cpp
    src/deep2/mars/TensorHotpatch.cpp
    src/deep2/mars/DualGPUBackend.cpp
    src/deep2/mars/VRAMManager.cpp
    src/deep2/HotPatcher.cpp
    src/deep2/HotPatcherSafety.cpp
    src/inference/RawrXD_LlamaNative.cpp
    tests/deep2_link_stubs.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp
    src/reverse/ReverseEngine.cpp
    src/reverse/ReverseModelLoader.cpp)
set_target_properties(k2_004_bounded_prefill PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
target_link_libraries(k2_004_bounded_prefill PRIVATE InferenceEngine)
add_test(NAME k2_004_bounded_prefill COMMAND k2_004_bounded_prefill WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# K2-004B: Bounded Multi-Layer Prefill (stream one layer at a time)
# Proves scalability from 003C single-layer to multi-layer with bounded memory
add_executable(k2_004_bounded_multi_layer_prefill tests/k2_004_bounded_multi_layer_prefill.cpp
    src/deep2/K2GlobalTensorIndex.cpp
    src/deep2/K2MLAWeights.cpp
    src/deep2/KimiK2Config.cpp
    src/deep2/GGUFLoader.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp)
set_target_properties(k2_004_bounded_multi_layer_prefill PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
target_link_libraries(k2_004_bounded_multi_layer_prefill PRIVATE InferenceEngine)
add_test(NAME k2_004_bounded_multi_layer_prefill COMMAND k2_004_bounded_multi_layer_prefill WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# K2-005: Bounded Autoregressive Decode with KV Cache
# Proves KV cache correctly stores/retrieves K/V across decode steps
add_executable(k2_005_autoregressive_kv_cache tests/k2_005_autoregressive_kv_cache.cpp
    src/deep2/K2GlobalTensorIndex.cpp
    src/deep2/K2MLAWeights.cpp
    src/deep2/KimiK2Config.cpp
    src/deep2/GGUFLoader.cpp
    src/deep2/KVCache.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp)
set_target_properties(k2_005_autoregressive_kv_cache PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
target_link_libraries(k2_005_autoregressive_kv_cache PRIVATE InferenceEngine)
add_test(NAME k2_005_autoregressive_kv_cache COMMAND k2_005_autoregressive_kv_cache WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# K2-006: Bounded Autoregressive Token Generation
# Proves logits → argmax → token → decode → repeat pipeline works
add_executable(k2_006_bounded_token_generation tests/k2_006_bounded_token_generation.cpp
    src/deep2/K2GlobalTensorIndex.cpp
    src/deep2/K2MLAWeights.cpp
    src/deep2/KimiK2Config.cpp
    src/deep2/GGUFLoader.cpp
    src/deep2/KVCache.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp)
set_target_properties(k2_006_bounded_token_generation PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
target_link_libraries(k2_006_bounded_token_generation PRIVATE InferenceEngine)
add_test(NAME k2_006_bounded_token_generation COMMAND k2_006_bounded_token_generation WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# K2-007: Real Output Projection (Q4_K streamed)
# Proves output.weight can be streamed row-by-row to produce logits
# without unbounded allocation. Independent of K2-006 synthetic baseline.
add_executable(k2_007_real_output_projection tests/k2_007_real_output_projection.cpp
    src/deep2/K2GlobalTensorIndex.cpp
    src/deep2/KimiK2Config.cpp
    src/deep2/GGUFLoader.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp)
set_target_properties(k2_007_real_output_projection PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
target_link_libraries(k2_007_real_output_projection PRIVATE InferenceEngine)
add_test(NAME k2_007_real_output_projection COMMAND k2_007_real_output_projection WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# K2-007A: GGUF Tokenizer Metadata Extraction
# Proves tokenizer.ggml.* arrays are readable from GGUF without external deps.
add_executable(k2_007a_tokenizer_metadata tests/k2_007a_tokenizer_metadata.cpp)
set_target_properties(k2_007a_tokenizer_metadata PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
add_test(NAME k2_007a_tokenizer_metadata COMMAND k2_007a_tokenizer_metadata WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# K2-007B: Projection → Logits Interface
# Narrow integration gate: proves real output projection produces a valid
# logits vector consumable by deterministic argmax. Reuses K2-007 streaming.
add_executable(k2_007b_projection_logits_interface tests/k2_007b_projection_logits_interface.cpp
    src/deep2/K2GlobalTensorIndex.cpp
    src/deep2/KimiK2Config.cpp
    src/deep2/GGUFLoader.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp)
set_target_properties(k2_007b_projection_logits_interface PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
target_link_libraries(k2_007b_projection_logits_interface PRIVATE InferenceEngine)
add_test(NAME k2_007b_projection_logits_interface COMMAND k2_007b_projection_logits_interface WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# K2-008: End-to-End Semantic Generation
# Proves tokenizer + real output projection communicate end-to-end:
# text → tokens → real K2 inference → logits → argmax → token → text.
# Independent of K2-006 synthetic baseline.
add_executable(k2_008_end_to_end_semantic_generation tests/k2_008_end_to_end_semantic_generation.cpp
    src/deep2/K2GlobalTensorIndex.cpp
    src/deep2/K2MLAWeights.cpp
    src/deep2/KimiK2Config.cpp
    src/deep2/GGUFLoader.cpp
    src/deep2/KVCache.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp)
set_target_properties(k2_008_end_to_end_semantic_generation PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
target_link_libraries(k2_008_end_to_end_semantic_generation PRIVATE InferenceEngine)
add_test(NAME k2_008_end_to_end_semantic_generation COMMAND k2_008_end_to_end_semantic_generation WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# K2-TLS: Time-Limited Serving Smoke Test
# Proves scheduler-level inference budget primitives work correctly.
add_executable(k2_tls_smoke_test tests/k2_tls_smoke_test.cpp
    src/deep2/K2TimeLimitedServing.cpp)
set_target_properties(k2_tls_smoke_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
target_link_libraries(k2_tls_smoke_test PRIVATE InferenceEngine)
add_test(NAME k2_tls_smoke_test COMMAND k2_tls_smoke_test WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

# K2-009: Real Token Embedding Lookup
# Proves token_embd.weight can be streamed row-by-row without loading
# the full ~918 MiB matrix. Reuses Q6_K dequantization from K2-007.
add_executable(k2_009_token_embedding_lookup tests/k2_009_token_embedding_lookup.cpp
    src/deep2/K2TokenEmbedding.cpp
    src/deep2/K2GlobalTensorIndex.cpp
    src/deep2/KimiK2Config.cpp
    src/deep2/GGUFLoader.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp)
set_target_properties(k2_009_token_embedding_lookup PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
target_link_libraries(k2_009_token_embedding_lookup PRIVATE InferenceEngine)
add_test(NAME k2_009_token_embedding_lookup COMMAND k2_009_token_embedding_lookup WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

add_executable(k2_010_real_embedding_transformer_integration tests/k2_010_real_embedding_transformer_integration.cpp
    src/deep2/K2TokenEmbedding.cpp
    src/deep2/K2MLAWeights.cpp
    src/deep2/K2GlobalTensorIndex.cpp
    src/deep2/KimiK2Config.cpp
    src/deep2/GGUFLoader.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp)
set_target_properties(k2_010_real_embedding_transformer_integration PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
target_link_libraries(k2_010_real_embedding_transformer_integration PRIVATE InferenceEngine)
add_test(NAME k2_010_real_embedding_transformer_integration COMMAND k2_010_real_embedding_transformer_integration WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

set(B004_TRANSFORMER_ROUTER_STREAMING_SOURCES
    tests/b004_transformer_router_streaming_integration.cpp
    src/cpu_inference_engine.cpp
    src/rawrxd_inference.cpp
    src/rawrxd_model_loader.cpp
    src/rawrxd_tokenizer.cpp
    src/rawrxd_transformer.cpp
    src/rawrxd_sampler.cpp
    src/gguf_loader.cpp
    src/gguf_vocab_resolver.cpp
    src/core/inference_witness.cpp
    src/core/swarm_scheduler.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp
    src/core/gguf_swarm_plan_builder.cpp
    src/logging/Logger.cpp
    src/runtime/TensorExecutionRouter.cpp
    src/runtime/memory/PlacementPolicy.cpp
    src/runtime/memory/ResidencyTracker.cpp
    src/runtime/memory/WorkingSetPredictor.cpp
    src/runtime/memory/CapacityManager.cpp
    src/runtime/memory/TransferScheduler.cpp
    src/runtime/memory/TensorPlacementManager.cpp
    src/runtime/memory/PredictiveMemoryManager.cpp
    src/runtime/memory/RouterPredictiveMemoryBridge.cpp
)

function(rawrxd_configure_b004_target target_name)
    target_include_directories(${target_name} PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/runtime
        ${CMAKE_SOURCE_DIR}/src/runtime/memory
    )
    target_compile_features(${target_name} PRIVATE cxx_std_20)
    target_link_libraries(${target_name} PRIVATE Threads::Threads dxgi)
endfunction()

add_executable(b004_transformer_router_streaming_integration EXCLUDE_FROM_ALL
    ${B004_TRANSFORMER_ROUTER_STREAMING_SOURCES}
)
rawrxd_configure_b004_target(b004_transformer_router_streaming_integration)
target_compile_definitions(b004_transformer_router_streaming_integration PRIVATE RAWRXD_NO_VULKAN)

if(RAWR_ENABLE_VULKAN AND RAWR_VULKAN_SDK_ROOT)
    target_include_directories(b004_transformer_router_streaming_integration PRIVATE "${RAWR_VULKAN_INCLUDE}")
    target_link_directories(b004_transformer_router_streaming_integration PRIVATE "${RAWR_VULKAN_SDK_ROOT}/Lib")
    target_link_libraries(b004_transformer_router_streaming_integration PRIVATE "${RAWR_VULKAN_LIB}")
    target_compile_definitions(b004_transformer_router_streaming_integration PRIVATE RAWR_HAS_VULKAN=1 RAWR_VULKAN_AVAILABLE=1)
elseif(RAWR_ENABLE_VULKAN AND Vulkan_FOUND)
    target_include_directories(b004_transformer_router_streaming_integration PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(b004_transformer_router_streaming_integration PRIVATE Vulkan::Vulkan)
    target_compile_definitions(b004_transformer_router_streaming_integration PRIVATE RAWR_HAS_VULKAN=1)
endif()

add_executable(b004_transformer_router_streaming_integration_cpu EXCLUDE_FROM_ALL
    ${B004_TRANSFORMER_ROUTER_STREAMING_SOURCES}
)
rawrxd_configure_b004_target(b004_transformer_router_streaming_integration_cpu)
target_compile_definitions(b004_transformer_router_streaming_integration_cpu PRIVATE RAWRXD_NO_VULKAN)
target_compile_definitions(b004_transformer_router_streaming_integration_cpu PRIVATE RAWRXD_B004_TEST_FORCE_CPU=1)
if(MSVC)
    target_compile_options(b004_transformer_router_streaming_integration_cpu PRIVATE
        /URAWR_ENABLE_VULKAN
        /URAWR_HAS_VULKAN
        /URAWR_VULKAN_AVAILABLE)
else()
    target_compile_options(b004_transformer_router_streaming_integration_cpu PRIVATE
        -URAWR_ENABLE_VULKAN
        -URAWR_HAS_VULKAN
        -URAWR_VULKAN_AVAILABLE)
endif()

# ============================================================================
# B012: Amortization Crossover Experiment
# ============================================================================
set(B012_AMORTIZATION_SOURCES
    B012/build/amortization_test.cpp
    src/cpu_inference_engine.cpp
    src/rawrxd_inference.cpp
    src/rawrxd_model_loader.cpp
    src/rawrxd_tokenizer.cpp
    src/rawrxd_transformer.cpp
    src/rawrxd_sampler.cpp
    src/gguf_loader.cpp
    src/gguf_vocab_resolver.cpp
    src/core/inference_witness.cpp
    src/core/swarm_scheduler.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp
    src/core/gguf_swarm_plan_builder.cpp
    src/logging/Logger.cpp
    src/runtime/TensorExecutionRouter.cpp
    src/runtime/memory/PlacementPolicy.cpp
    src/runtime/memory/ResidencyTracker.cpp
    src/runtime/memory/WorkingSetPredictor.cpp
    src/runtime/memory/CapacityManager.cpp
    src/runtime/memory/TransferScheduler.cpp
    src/runtime/memory/TensorPlacementManager.cpp
    src/runtime/memory/PredictiveMemoryManager.cpp
    src/runtime/memory/RouterPredictiveMemoryBridge.cpp
)
add_executable(b012_amortization_test EXCLUDE_FROM_ALL
    ${B012_AMORTIZATION_SOURCES}
)
rawrxd_configure_b004_target(b012_amortization_test)
target_compile_definitions(b012_amortization_test PRIVATE RAWRXD_NO_VULKAN)
target_compile_definitions(b012_amortization_test PRIVATE RAWRXD_B004_TEST_FORCE_CPU=1)
if(MSVC)
    target_compile_options(b012_amortization_test PRIVATE
        /URAWR_ENABLE_VULKAN
        /URAWR_HAS_VULKAN
        /URAWR_VULKAN_AVAILABLE)
else()
    target_compile_options(b012_amortization_test PRIVATE
        -URAWR_ENABLE_VULKAN
        -URAWR_HAS_VULKAN
        -URAWR_VULKAN_AVAILABLE)
endif()

if(BUILD_TESTING)
    add_test(NAME b004_transformer_router_streaming_integration
             COMMAND b004_transformer_router_streaming_integration)
    add_test(NAME b004_transformer_router_streaming_integration_cpu
             COMMAND b004_transformer_router_streaming_integration_cpu)
endif()

# ============================================================================
# B013: Correctness + Long-Run Residency Stability
# ============================================================================
set(B013_STABILITY_SOURCES
    B013/build/stability_test.cpp
    B014/build/b014_profiler.cpp
    src/cpu_inference_engine.cpp
    src/rawrxd_inference.cpp
    src/rawrxd_model_loader.cpp
    src/rawrxd_tokenizer.cpp
    src/rawrxd_transformer.cpp
    src/rawrxd_sampler.cpp
    src/gguf_loader.cpp
    src/gguf_vocab_resolver.cpp
    src/core/inference_witness.cpp
    src/core/swarm_scheduler.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp
    src/core/gguf_swarm_plan_builder.cpp
    src/logging/Logger.cpp
    src/runtime/TensorExecutionRouter.cpp
    src/runtime/memory/PlacementPolicy.cpp
    src/runtime/memory/ResidencyTracker.cpp
    src/runtime/memory/WorkingSetPredictor.cpp
    src/runtime/memory/CapacityManager.cpp
    src/runtime/memory/TransferScheduler.cpp
    src/runtime/memory/TensorPlacementManager.cpp
    src/runtime/memory/PredictiveMemoryManager.cpp
    src/runtime/memory/RouterPredictiveMemoryBridge.cpp
)
add_executable(b013_stability_test EXCLUDE_FROM_ALL
    ${B013_STABILITY_SOURCES}
)
rawrxd_configure_b004_target(b013_stability_test)
target_compile_definitions(b013_stability_test PRIVATE RAWRXD_NO_VULKAN)
target_compile_definitions(b013_stability_test PRIVATE RAWRXD_B004_TEST_FORCE_CPU=1)
if(MSVC)
    target_compile_options(b013_stability_test PRIVATE
        /URAWR_ENABLE_VULKAN
        /URAWR_HAS_VULKAN
        /URAWR_VULKAN_AVAILABLE)
else()
    target_compile_options(b013_stability_test PRIVATE
        -URAWR_ENABLE_VULKAN
        -URAWR_HAS_VULKAN
        -URAWR_VULKAN_AVAILABLE)
endif()

# ============================================================================
# B014: Compute Decomposition Measurement
# ============================================================================
set(B014_COMPUTE_SOURCES
    B014/build/compute_decomposition.cpp
    B014/build/b014_profiler.cpp
    B014/build/B014_RingAdapter.cpp
    src/runtime/Sovereign_RingTelemetry.cpp
    src/runtime/Sovereign_KernelBridge_CPP.cpp
    src/cpu_inference_engine.cpp
    src/rawrxd_inference.cpp
    src/rawrxd_model_loader.cpp
    src/rawrxd_tokenizer.cpp
    src/rawrxd_transformer.cpp
    src/rawrxd_sampler.cpp
    src/gguf_loader.cpp
    src/gguf_vocab_resolver.cpp
    src/core/inference_witness.cpp
    src/core/swarm_scheduler.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp
    src/core/gguf_swarm_plan_builder.cpp
    src/logging/Logger.cpp
    src/runtime/TensorExecutionRouter.cpp
    src/runtime/memory/PlacementPolicy.cpp
    src/runtime/memory/ResidencyTracker.cpp
    src/runtime/memory/WorkingSetPredictor.cpp
    src/runtime/memory/CapacityManager.cpp
    src/runtime/memory/TransferScheduler.cpp
    src/runtime/memory/TensorPlacementManager.cpp
    src/runtime/memory/PredictiveMemoryManager.cpp
    src/runtime/memory/RouterPredictiveMemoryBridge.cpp
)
add_executable(b014_compute_decomposition EXCLUDE_FROM_ALL
    ${B014_COMPUTE_SOURCES}
)
rawrxd_configure_b004_target(b014_compute_decomposition)
target_compile_definitions(b014_compute_decomposition PRIVATE RAWRXD_NO_VULKAN)
target_compile_definitions(b014_compute_decomposition PRIVATE RAWRXD_B004_TEST_FORCE_CPU=1)
if(MSVC)
    target_compile_options(b014_compute_decomposition PRIVATE
        /URAWR_ENABLE_VULKAN
        /URAWR_HAS_VULKAN
        /URAWR_VULKAN_AVAILABLE)
else()
    target_compile_options(b014_compute_decomposition PRIVATE
        -URAWR_ENABLE_VULKAN
        -URAWR_HAS_VULKAN
        -URAWR_VULKAN_AVAILABLE)
endif()

# ============================================================================
# B014-BoundaryProbe: Isolate first failure point
# ============================================================================
set(B014_BOUNDARY_SOURCES
    B014/build/b014_boundary_probe.cpp
    B014/build/b014_profiler.cpp
    src/cpu_inference_engine.cpp
    src/rawrxd_inference.cpp
    src/rawrxd_model_loader.cpp
    src/rawrxd_tokenizer.cpp
    src/rawrxd_transformer.cpp
    src/rawrxd_sampler.cpp
    src/gguf_loader.cpp
    src/gguf_vocab_resolver.cpp
    src/core/inference_witness.cpp
    src/core/swarm_scheduler.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp
    src/core/gguf_swarm_plan_builder.cpp
    src/logging/Logger.cpp
    src/runtime/TensorExecutionRouter.cpp
    src/runtime/memory/PlacementPolicy.cpp
    src/runtime/memory/ResidencyTracker.cpp
    src/runtime/memory/WorkingSetPredictor.cpp
    src/runtime/memory/CapacityManager.cpp
    src/runtime/memory/TransferScheduler.cpp
    src/runtime/memory/TensorPlacementManager.cpp
    src/runtime/memory/PredictiveMemoryManager.cpp
    src/runtime/memory/RouterPredictiveMemoryBridge.cpp
)
add_executable(b014_boundary_probe EXCLUDE_FROM_ALL
    ${B014_BOUNDARY_SOURCES}
)
rawrxd_configure_b004_target(b014_boundary_probe)
target_compile_definitions(b014_boundary_probe PRIVATE RAWRXD_NO_VULKAN)
target_compile_definitions(b014_boundary_probe PRIVATE RAWRXD_B004_TEST_FORCE_CPU=1)
if(MSVC)
    target_compile_options(b014_boundary_probe PRIVATE
        /URAWR_ENABLE_VULKAN
        /URAWR_HAS_VULKAN
        /URAWR_VULKAN_AVAILABLE)
else()
    target_compile_options(b014_boundary_probe PRIVATE
        -URAWR_ENABLE_VULKAN
        -URAWR_HAS_VULKAN
        -URAWR_VULKAN_AVAILABLE)
endif()

# ============================================================================
# B014-SingleBoundary: Test one boundary per invocation
# ============================================================================
set(B014_SINGLE_SOURCES
    B014/build/b014_single_boundary.cpp
    B014/build/b014_profiler.cpp
    src/cpu_inference_engine.cpp
    src/rawrxd_inference.cpp
    src/rawrxd_model_loader.cpp
    src/rawrxd_tokenizer.cpp
    src/rawrxd_transformer.cpp
    src/rawrxd_sampler.cpp
    src/gguf_loader.cpp
    src/gguf_vocab_resolver.cpp
    src/core/inference_witness.cpp
    src/core/swarm_scheduler.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp
    src/core/gguf_swarm_plan_builder.cpp
    src/logging/Logger.cpp
    src/runtime/TensorExecutionRouter.cpp
    src/runtime/memory/PlacementPolicy.cpp
    src/runtime/memory/ResidencyTracker.cpp
    src/runtime/memory/WorkingSetPredictor.cpp
    src/runtime/memory/CapacityManager.cpp
    src/runtime/memory/TransferScheduler.cpp
    src/runtime/memory/TensorPlacementManager.cpp
    src/runtime/memory/PredictiveMemoryManager.cpp
    src/runtime/memory/RouterPredictiveMemoryBridge.cpp
)
add_executable(b014_single_boundary EXCLUDE_FROM_ALL
    ${B014_SINGLE_SOURCES}
)
rawrxd_configure_b004_target(b014_single_boundary)
target_compile_definitions(b014_single_boundary PRIVATE RAWRXD_NO_VULKAN)
target_compile_definitions(b014_single_boundary PRIVATE RAWRXD_B004_TEST_FORCE_CPU=1)
if(MSVC)
    target_compile_options(b014_single_boundary PRIVATE
        /URAWR_ENABLE_VULKAN
        /URAWR_HAS_VULKAN
        /URAWR_VULKAN_AVAILABLE)
else()
    target_compile_options(b014_single_boundary PRIVATE
        -URAWR_ENABLE_VULKAN
        -URAWR_HAS_VULKAN
        -URAWR_VULKAN_AVAILABLE)
endif()

# ============================================================================
# B014-LifetimeProbe: Pinpoint exact destructor/cleanup failure
# ============================================================================
set(B014_LIFETIME_SOURCES
    B014/build/b014_lifetime_probe.cpp
    B014/build/b014_profiler.cpp
    src/cpu_inference_engine.cpp
    src/rawrxd_inference.cpp
    src/rawrxd_model_loader.cpp
    src/rawrxd_tokenizer.cpp
    src/rawrxd_transformer.cpp
    src/rawrxd_sampler.cpp
    src/gguf_loader.cpp
    src/gguf_vocab_resolver.cpp
    src/core/inference_witness.cpp
    src/core/swarm_scheduler.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp
    src/core/gguf_swarm_plan_builder.cpp
    src/logging/Logger.cpp
    src/runtime/TensorExecutionRouter.cpp
    src/runtime/memory/PlacementPolicy.cpp
    src/runtime/memory/ResidencyTracker.cpp
    src/runtime/memory/WorkingSetPredictor.cpp
    src/runtime/memory/CapacityManager.cpp
    src/runtime/memory/TransferScheduler.cpp
    src/runtime/memory/TensorPlacementManager.cpp
    src/runtime/memory/PredictiveMemoryManager.cpp
    src/runtime/memory/RouterPredictiveMemoryBridge.cpp
)
add_executable(b014_lifetime_probe EXCLUDE_FROM_ALL
    ${B014_LIFETIME_SOURCES}
)
rawrxd_configure_b004_target(b014_lifetime_probe)
target_compile_definitions(b014_lifetime_probe PRIVATE RAWRXD_NO_VULKAN)
target_compile_definitions(b014_lifetime_probe PRIVATE RAWRXD_B004_TEST_FORCE_CPU=1)
if(MSVC)
    target_compile_options(b014_lifetime_probe PRIVATE
        /URAWR_ENABLE_VULKAN
        /URAWR_HAS_VULKAN
        /URAWR_VULKAN_AVAILABLE)
else()
    target_compile_options(b014_lifetime_probe PRIVATE
        -URAWR_ENABLE_VULKAN
        -URAWR_HAS_VULKAN
        -URAWR_VULKAN_AVAILABLE)
endif()

# ============================================================================
# B005: Canonical Model Certification
# ============================================================================
set(B005_CANONICAL_MODEL_SOURCES
    tests/b005_canonical_model_certification.cpp
    src/canonical/gguf_adapter.cpp
    src/canonical/unified_model_loader.cpp
)

add_executable(b005_canonical_model_certification EXCLUDE_FROM_ALL
    ${B005_CANONICAL_MODEL_SOURCES}
)
target_include_directories(b005_canonical_model_certification PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/canonical
)
target_compile_features(b005_canonical_model_certification PRIVATE cxx_std_20)

if(BUILD_TESTING)
    add_test(NAME b005_canonical_model_certification
             COMMAND b005_canonical_model_certification)
endif()

# ============================================================================
# B006: KV Cache Verification
# ============================================================================
set(B006_KV_CACHE_SOURCES
    tests/b006_kv_cache_verification.cpp
    src/cpu_inference_engine.cpp
    src/rawrxd_inference.cpp
    src/rawrxd_model_loader.cpp
    src/rawrxd_tokenizer.cpp
    src/rawrxd_transformer.cpp
    src/rawrxd_sampler.cpp
    src/gguf_loader.cpp
    src/gguf_vocab_resolver.cpp
    src/core/inference_witness.cpp
    src/core/swarm_scheduler.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp
    src/core/gguf_swarm_plan_builder.cpp
    src/logging/Logger.cpp
    src/runtime/TensorExecutionRouter.cpp
    src/runtime/memory/PredictiveMemoryManager.cpp
    src/runtime/memory/TransferScheduler.cpp
    src/runtime/memory/ResidencyTracker.cpp
    src/runtime/memory/WorkingSetPredictor.cpp
    src/runtime/memory/CapacityManager.cpp
    src/runtime/memory/TensorPlacementManager.cpp
)

add_executable(b006_kv_cache_verification EXCLUDE_FROM_ALL
    ${B006_KV_CACHE_SOURCES}
)
target_include_directories(b006_kv_cache_verification PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include/core
)
target_compile_features(b006_kv_cache_verification PRIVATE cxx_std_20)

# Vulkan SDK support for b006_kv_cache_verification (shares sources with rawrxd)
if(RAWR_ENABLE_VULKAN AND RAWR_VULKAN_SDK_ROOT)
    target_include_directories(b006_kv_cache_verification PRIVATE "${RAWR_VULKAN_INCLUDE}")
    target_link_libraries(b006_kv_cache_verification PRIVATE "${RAWR_VULKAN_LIB}" dxgi)
    target_compile_definitions(b006_kv_cache_verification PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN AND VULKAN_INCLUDE_DIR)
    target_include_directories(b006_kv_cache_verification PRIVATE ${VULKAN_INCLUDE_DIR})
    target_link_libraries(b006_kv_cache_verification PRIVATE dxgi)
    target_compile_definitions(b006_kv_cache_verification PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN AND Vulkan_FOUND)
    target_include_directories(b006_kv_cache_verification PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(b006_kv_cache_verification PRIVATE Vulkan::Vulkan dxgi)
    target_compile_definitions(b006_kv_cache_verification PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN)
    target_compile_definitions(b006_kv_cache_verification PRIVATE RAWR_HAS_VULKAN=0)
    target_link_libraries(b006_kv_cache_verification PRIVATE dxgi)
else()
    target_link_libraries(b006_kv_cache_verification PRIVATE dxgi)
endif()

# Add LIBPATH directly using the SDK paths defined at top of file
if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UM_VER)
    set_property(TARGET b006_kv_cache_verification APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64\"")
endif()
if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UCRT_VER)
    set_property(TARGET b006_kv_cache_verification APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UCRT_VER}/ucrt/x64\"")
endif()
if(DEFINED _MSVC_ROOT)
    set_property(TARGET b006_kv_cache_verification APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_MSVC_ROOT}/lib/x64\"")
endif()

# Use short library names - LIBPATH above provides the paths
target_link_libraries(b006_kv_cache_verification PRIVATE
    kernel32 user32 gdi32 winspool shell32 ole32 oleaut32 uuid comdlg32 advapi32 version ws2_32
)

if(BUILD_TESTING)
    add_test(NAME b006_kv_cache_verification
             COMMAND b006_kv_cache_verification)
endif()

# ============================================================================
# B007: Performance Baseline Certification
# ============================================================================
set(B007_PERF_BASELINE_SOURCES
    tests/b007_performance_baseline.cpp
    src/cpu_inference_engine.cpp
    src/rawrxd_inference.cpp
    src/rawrxd_model_loader.cpp
    src/rawrxd_tokenizer.cpp
    src/rawrxd_transformer.cpp
    src/rawrxd_sampler.cpp
    src/gguf_loader.cpp
    src/gguf_vocab_resolver.cpp
    src/core/inference_witness.cpp
    src/core/swarm_scheduler.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp
    src/core/gguf_swarm_plan_builder.cpp
    src/logging/Logger.cpp
    src/runtime/TensorExecutionRouter.cpp
    src/runtime/memory/PredictiveMemoryManager.cpp
    src/runtime/memory/TransferScheduler.cpp
    src/runtime/memory/ResidencyTracker.cpp
    src/runtime/memory/WorkingSetPredictor.cpp
    src/runtime/memory/CapacityManager.cpp
    src/runtime/memory/TensorPlacementManager.cpp
)

add_executable(b007_performance_baseline EXCLUDE_FROM_ALL
    ${B007_PERF_BASELINE_SOURCES}
)
target_include_directories(b007_performance_baseline PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include/core
)
target_compile_features(b007_performance_baseline PRIVATE cxx_std_20)
# /EHa enables SEH in functions with C++ destructors (required for __try/__except wrappers)
target_compile_options(b007_performance_baseline PRIVATE "/EHa")

# Vulkan SDK support for b007_performance_baseline
if(RAWR_ENABLE_VULKAN AND RAWR_VULKAN_SDK_ROOT)
    target_include_directories(b007_performance_baseline PRIVATE "${RAWR_VULKAN_INCLUDE}")
    target_link_libraries(b007_performance_baseline PRIVATE "${RAWR_VULKAN_LIB}" dxgi)
    target_compile_definitions(b007_performance_baseline PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN AND VULKAN_INCLUDE_DIR)
    target_include_directories(b007_performance_baseline PRIVATE ${VULKAN_INCLUDE_DIR})
    target_link_libraries(b007_performance_baseline PRIVATE dxgi)
    target_compile_definitions(b007_performance_baseline PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN AND Vulkan_FOUND)
    target_include_directories(b007_performance_baseline PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(b007_performance_baseline PRIVATE Vulkan::Vulkan dxgi)
    target_compile_definitions(b007_performance_baseline PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN)
    target_compile_definitions(b007_performance_baseline PRIVATE RAWR_HAS_VULKAN=0)
    target_link_libraries(b007_performance_baseline PRIVATE dxgi)
else()
    target_link_libraries(b007_performance_baseline PRIVATE dxgi)
endif()

if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UM_VER)
    set_property(TARGET b007_performance_baseline APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64\"")
endif()
if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UCRT_VER)
    set_property(TARGET b007_performance_baseline APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UCRT_VER}/ucrt/x64\"")
endif()
if(DEFINED _MSVC_ROOT)
    set_property(TARGET b007_performance_baseline APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_MSVC_ROOT}/lib/x64\"")
endif()

target_link_libraries(b007_performance_baseline PRIVATE
    kernel32 user32 gdi32 winspool shell32 ole32 oleaut32 uuid comdlg32 advapi32 version ws2_32
)

if(BUILD_TESTING)
    add_test(NAME b007_performance_baseline
             COMMAND b007_performance_baseline)
endif()

# ============================================================================
# B008: Build / CI Integration Gate
# ============================================================================
set(B008_CI_SOURCES
    tests/b008_ci_integration.cpp
)

add_executable(b008_ci_integration EXCLUDE_FROM_ALL
    ${B008_CI_SOURCES}
)
target_include_directories(b008_ci_integration PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include/core
)
target_compile_features(b008_ci_integration PRIVATE cxx_std_20)

if(BUILD_TESTING)
    add_test(NAME b008_ci_integration
             COMMAND b008_ci_integration)
endif()

# ============================================================================
# B010: Weight Residency Profiling Baseline (FROZEN)
# ============================================================================
set(B010_RESIDENCY_SOURCES
    tests/b010/b010_weight_residency_profile.cpp
    src/cpu_inference_engine.cpp
    src/rawrxd_inference.cpp
    src/rawrxd_model_loader.cpp
    src/rawrxd_tokenizer.cpp
    src/rawrxd_transformer.cpp
    src/rawrxd_sampler.cpp
    src/gguf_loader.cpp
    src/gguf_vocab_resolver.cpp
    src/core/inference_witness.cpp
    src/core/swarm_scheduler.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp
    src/core/gguf_swarm_plan_builder.cpp
    src/logging/Logger.cpp
    src/runtime/TensorExecutionRouter.cpp
    src/runtime/memory/PredictiveMemoryManager.cpp
    src/runtime/memory/TransferScheduler.cpp
    src/runtime/memory/ResidencyTracker.cpp
    src/runtime/memory/WorkingSetPredictor.cpp
    src/runtime/memory/CapacityManager.cpp
    src/runtime/memory/TensorPlacementManager.cpp
)

add_executable(b010_weight_residency_profile EXCLUDE_FROM_ALL
    ${B010_RESIDENCY_SOURCES}
)
target_include_directories(b010_weight_residency_profile PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include/core
)
target_compile_features(b010_weight_residency_profile PRIVATE cxx_std_20)

if(RAWR_ENABLE_VULKAN AND RAWR_VULKAN_SDK_ROOT)
    target_include_directories(b010_weight_residency_profile PRIVATE "${RAWR_VULKAN_INCLUDE}")
    target_link_libraries(b010_weight_residency_profile PRIVATE "${RAWR_VULKAN_LIB}" dxgi)
    target_compile_definitions(b010_weight_residency_profile PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN AND VULKAN_INCLUDE_DIR)
    target_include_directories(b010_weight_residency_profile PRIVATE ${VULKAN_INCLUDE_DIR})
    target_link_libraries(b010_weight_residency_profile PRIVATE dxgi)
    target_compile_definitions(b010_weight_residency_profile PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN AND Vulkan_FOUND)
    target_include_directories(b010_weight_residency_profile PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(b010_weight_residency_profile PRIVATE Vulkan::Vulkan dxgi)
    target_compile_definitions(b010_weight_residency_profile PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN)
    target_compile_definitions(b010_weight_residency_profile PRIVATE RAWR_HAS_VULKAN=0)
    target_link_libraries(b010_weight_residency_profile PRIVATE dxgi)
else()
    target_link_libraries(b010_weight_residency_profile PRIVATE dxgi)
endif()

if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UM_VER)
    set_property(TARGET b010_weight_residency_profile APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64\"")
endif()
if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UCRT_VER)
    set_property(TARGET b010_weight_residency_profile APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UCRT_VER}/ucrt/x64\"")
endif()
if(DEFINED _MSVC_ROOT)
    set_property(TARGET b010_weight_residency_profile APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_MSVC_ROOT}/lib/x64\"")
endif()

target_link_libraries(b010_weight_residency_profile PRIVATE
    kernel32 user32 gdi32 winspool shell32 ole32 oleaut32 uuid comdlg32 advapi32 version ws2_32
)

if(BUILD_TESTING)
    add_test(NAME b010_weight_residency_profile
             COMMAND b010_weight_residency_profile)
endif()

# ============================================================================
# B011: Targeted Weight Residency Optimization
# ============================================================================
set(B011_RESIDENCY_OPT_SOURCES
    tests/b011_weight_residency_optimization.cpp
    src/cpu_inference_engine.cpp
    src/rawrxd_inference.cpp
    src/rawrxd_model_loader.cpp
    src/rawrxd_tokenizer.cpp
    src/rawrxd_transformer.cpp
    src/rawrxd_sampler.cpp
    src/gguf_loader.cpp
    src/gguf_vocab_resolver.cpp
    src/core/inference_witness.cpp
    src/core/swarm_scheduler.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp
    src/core/gguf_swarm_plan_builder.cpp
    src/logging/Logger.cpp
    src/runtime/TensorExecutionRouter.cpp
    src/runtime/memory/PredictiveMemoryManager.cpp
    src/runtime/memory/TransferScheduler.cpp
    src/runtime/memory/ResidencyTracker.cpp
    src/runtime/memory/WorkingSetPredictor.cpp
    src/runtime/memory/CapacityManager.cpp
    src/runtime/memory/TensorPlacementManager.cpp
)

add_executable(b011_weight_residency_optimization EXCLUDE_FROM_ALL
    ${B011_RESIDENCY_OPT_SOURCES}
)
target_include_directories(b011_weight_residency_optimization PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include/core
)
target_compile_features(b011_weight_residency_optimization PRIVATE cxx_std_20)

if(RAWR_ENABLE_VULKAN AND RAWR_VULKAN_SDK_ROOT)
    target_include_directories(b011_weight_residency_optimization PRIVATE "${RAWR_VULKAN_INCLUDE}")
    target_link_libraries(b011_weight_residency_optimization PRIVATE "${RAWR_VULKAN_LIB}" dxgi)
    target_compile_definitions(b011_weight_residency_optimization PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN AND VULKAN_INCLUDE_DIR)
    target_include_directories(b011_weight_residency_optimization PRIVATE ${VULKAN_INCLUDE_DIR})
    target_link_libraries(b011_weight_residency_optimization PRIVATE dxgi)
    target_compile_definitions(b011_weight_residency_optimization PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN AND Vulkan_FOUND)
    target_include_directories(b011_weight_residency_optimization PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(b011_weight_residency_optimization PRIVATE Vulkan::Vulkan dxgi)
    target_compile_definitions(b011_weight_residency_optimization PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN)
    target_compile_definitions(b011_weight_residency_optimization PRIVATE RAWR_HAS_VULKAN=0)
    target_link_libraries(b011_weight_residency_optimization PRIVATE dxgi)
else()
    target_link_libraries(b011_weight_residency_optimization PRIVATE dxgi)
endif()

if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UM_VER)
    set_property(TARGET b011_weight_residency_optimization APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64\"")
endif()
if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UCRT_VER)
    set_property(TARGET b011_weight_residency_optimization APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UCRT_VER}/ucrt/x64\"")
endif()
if(DEFINED _MSVC_ROOT)
    set_property(TARGET b011_weight_residency_optimization APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_MSVC_ROOT}/lib/x64\"")
endif()

target_link_libraries(b011_weight_residency_optimization PRIVATE
    kernel32 user32 gdi32 winspool shell32 ole32 oleaut32 uuid comdlg32 advapi32 version ws2_32
)

if(BUILD_TESTING)
    add_test(NAME b011_weight_residency_optimization
             COMMAND b011_weight_residency_optimization)
endif()

# ============================================================================
# B015: WeightResidencyPool Integration Validation
# ============================================================================
set(B015_RESIDENCY_VALIDATION_SOURCES
    tests/b015/b015_residency_validation.cpp
    src/rawrxd_model_loader.cpp
    src/rawrxd_transformer.cpp
    src/rawrxd_tokenizer.cpp
    src/rawrxd_sampler.cpp
    src/gguf_loader.cpp
    src/gguf_vocab_resolver.cpp
    src/core/inference_witness.cpp
    src/core/swarm_scheduler.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp
    src/core/gguf_swarm_plan_builder.cpp
    src/logging/Logger.cpp
    src/runtime/TensorExecutionRouter.cpp
    src/runtime/StreamRouterAdapter.cpp
    src/runtime/memory/PredictiveMemoryManager.cpp
    src/runtime/memory/TransferScheduler.cpp
    src/runtime/memory/ResidencyTracker.cpp
    src/runtime/memory/WorkingSetPredictor.cpp
    src/runtime/memory/CapacityManager.cpp
    src/runtime/memory/TensorPlacementManager.cpp
    B014/build/b014_profiler.cpp
)

add_executable(b015_residency_validation EXCLUDE_FROM_ALL
    ${B015_RESIDENCY_VALIDATION_SOURCES}
)
target_include_directories(b015_residency_validation PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include/core
)
target_compile_features(b015_residency_validation PRIVATE cxx_std_20)

if(RAWR_ENABLE_VULKAN AND RAWR_VULKAN_SDK_ROOT)
    target_include_directories(b015_residency_validation PRIVATE "${RAWR_VULKAN_INCLUDE}")
    target_link_libraries(b015_residency_validation PRIVATE "${RAWR_VULKAN_LIB}" dxgi)
    target_compile_definitions(b015_residency_validation PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN AND VULKAN_INCLUDE_DIR)
    target_include_directories(b015_residency_validation PRIVATE ${VULKAN_INCLUDE_DIR})
    target_link_libraries(b015_residency_validation PRIVATE dxgi)
    target_compile_definitions(b015_residency_validation PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN AND Vulkan_FOUND)
    target_include_directories(b015_residency_validation PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(b015_residency_validation PRIVATE Vulkan::Vulkan dxgi)
    target_compile_definitions(b015_residency_validation PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN)
    target_compile_definitions(b015_residency_validation PRIVATE RAWR_HAS_VULKAN=0)
    target_link_libraries(b015_residency_validation PRIVATE dxgi)
else()
    target_link_libraries(b015_residency_validation PRIVATE dxgi)
endif()

if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UM_VER)
    set_property(TARGET b015_residency_validation APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64\"")
endif()
if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UCRT_VER)
    set_property(TARGET b015_residency_validation APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UCRT_VER}/ucrt/x64\"")
endif()
if(DEFINED _MSVC_ROOT)
    set_property(TARGET b015_residency_validation APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_MSVC_ROOT}/lib/x64\"")
endif()

target_link_libraries(b015_residency_validation PRIVATE
    kernel32 user32 gdi32 winspool shell32 ole32 oleaut32 uuid comdlg32 advapi32 version ws2_32
)

if(BUILD_TESTING)
    add_test(NAME b015_residency_validation
             COMMAND b015_residency_validation)
endif()

# ============================================================================
# B015: WeightResidencyPool Micro-Benchmark
# ============================================================================
add_executable(b015_pool_microbench EXCLUDE_FROM_ALL
    tests/b015/b015_pool_microbench.cpp
    src/rawrxd_transformer.cpp
    src/rawrxd_model_loader.cpp
    src/rawrxd_tokenizer.cpp
    src/rawrxd_sampler.cpp
    src/gguf_loader.cpp
    src/gguf_vocab_resolver.cpp
    src/core/inference_witness.cpp
    src/core/swarm_scheduler.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp
    src/core/gguf_swarm_plan_builder.cpp
    src/logging/Logger.cpp
    src/runtime/TensorExecutionRouter.cpp
    src/runtime/memory/PredictiveMemoryManager.cpp
    src/runtime/memory/TransferScheduler.cpp
    src/runtime/memory/ResidencyTracker.cpp
    src/runtime/memory/WorkingSetPredictor.cpp
    src/runtime/memory/CapacityManager.cpp
    src/runtime/memory/TensorPlacementManager.cpp
    B014/build/b014_profiler.cpp
)
target_include_directories(b015_pool_microbench PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include/core
)
target_compile_features(b015_pool_microbench PRIVATE cxx_std_20)

if(RAWR_ENABLE_VULKAN AND RAWR_VULKAN_SDK_ROOT)
    target_include_directories(b015_pool_microbench PRIVATE "${RAWR_VULKAN_INCLUDE}")
    target_link_libraries(b015_pool_microbench PRIVATE "${RAWR_VULKAN_LIB}" dxgi)
    target_compile_definitions(b015_pool_microbench PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN AND VULKAN_INCLUDE_DIR)
    target_include_directories(b015_pool_microbench PRIVATE ${VULKAN_INCLUDE_DIR})
    target_link_libraries(b015_pool_microbench PRIVATE dxgi)
    target_compile_definitions(b015_pool_microbench PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN AND Vulkan_FOUND)
    target_include_directories(b015_pool_microbench PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(b015_pool_microbench PRIVATE Vulkan::Vulkan dxgi)
    target_compile_definitions(b015_pool_microbench PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN)
    target_compile_definitions(b015_pool_microbench PRIVATE RAWR_HAS_VULKAN=0)
    target_link_libraries(b015_pool_microbench PRIVATE dxgi)
else()
    target_link_libraries(b015_pool_microbench PRIVATE dxgi)
endif()

if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UM_VER)
    set_property(TARGET b015_pool_microbench APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64\"")
endif()
if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UCRT_VER)
    set_property(TARGET b015_pool_microbench APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UCRT_VER}/ucrt/x64\"")
endif()
if(DEFINED _MSVC_ROOT)
    set_property(TARGET b015_pool_microbench APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_MSVC_ROOT}/lib/x64\"")
endif()

target_link_libraries(b015_pool_microbench PRIVATE
    kernel32 user32 gdi32 winspool shell32 ole32 oleaut32 uuid comdlg32 advapi32 version ws2_32
)

# ============================================================================
# B015: WeightResidencyPool Direct Unit Test
# ============================================================================
add_executable(b015_pool_direct_test EXCLUDE_FROM_ALL
    tests/b015/b015_pool_direct_test.cpp
)
target_include_directories(b015_pool_direct_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include/core
)
target_compile_features(b015_pool_direct_test PRIVATE cxx_std_20)

if(RAWR_ENABLE_VULKAN AND RAWR_VULKAN_SDK_ROOT)
    target_include_directories(b015_pool_direct_test PRIVATE "${RAWR_VULKAN_INCLUDE}")
    target_link_libraries(b015_pool_direct_test PRIVATE "${RAWR_VULKAN_LIB}" dxgi)
    target_compile_definitions(b015_pool_direct_test PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN AND VULKAN_INCLUDE_DIR)
    target_include_directories(b015_pool_direct_test PRIVATE ${VULKAN_INCLUDE_DIR})
    target_link_libraries(b015_pool_direct_test PRIVATE dxgi)
    target_compile_definitions(b015_pool_direct_test PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN AND Vulkan_FOUND)
    target_include_directories(b015_pool_direct_test PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(b015_pool_direct_test PRIVATE Vulkan::Vulkan dxgi)
    target_compile_definitions(b015_pool_direct_test PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN)
    target_compile_definitions(b015_pool_direct_test PRIVATE RAWR_HAS_VULKAN=0)
    target_link_libraries(b015_pool_direct_test PRIVATE dxgi)
else()
    target_link_libraries(b015_pool_direct_test PRIVATE dxgi)
endif()

if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UM_VER)
    set_property(TARGET b015_pool_direct_test APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64\"")
endif()
if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UCRT_VER)
    set_property(TARGET b015_pool_direct_test APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UCRT_VER}/ucrt/x64\"")
endif()
if(DEFINED _MSVC_ROOT)
    set_property(TARGET b015_pool_direct_test APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_MSVC_ROOT}/lib/x64\"")
endif()

target_link_libraries(b015_pool_direct_test PRIVATE
    kernel32 user32 gdi32 winspool shell32 ole32 oleaut32 uuid comdlg32 advapi32 version ws2_32
)

# ============================================================================
# B015: AVX-512 Q4_K_M Dequantization Kernel Certification
# ============================================================================
set(B015_DEQUANT_SOURCES
    B015/build/b015_cert_test.cpp
)
add_executable(b015_dequant_certification EXCLUDE_FROM_ALL
    ${B015_DEQUANT_SOURCES}
)
target_include_directories(b015_dequant_certification PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/B015/build
)
target_compile_features(b015_dequant_certification PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(b015_dequant_certification PRIVATE /arch:AVX512)
else()
    target_compile_options(b015_dequant_certification PRIVATE -mavx512f)
endif()
target_link_libraries(b015_dequant_certification PRIVATE Threads::Threads)
if(BUILD_TESTING)
    add_test(NAME b015_dequant_certification
             COMMAND b015_dequant_certification)
endif()

# ============================================================================
# B010/B011 Fast Experiment — Direct StreamingMatMul residency test
# ============================================================================
set(B010B011_FAST_SOURCES
    tests/b010_b011_fast_experiment.cpp
    src/rawrxd_model_loader.cpp
    src/gguf_loader.cpp
    src/gguf_vocab_resolver.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp
    src/logging/Logger.cpp
    B014/build/b014_profiler.cpp
)

add_executable(b010_b011_fast_experiment EXCLUDE_FROM_ALL
    ${B010B011_FAST_SOURCES}
)
target_include_directories(b010_b011_fast_experiment PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include/core
)
target_compile_features(b010_b011_fast_experiment PRIVATE cxx_std_20)
# B014: Enable AVX-512 for dot-product optimization
target_compile_options(b010_b011_fast_experiment PRIVATE /arch:AVX512)

if(RAWR_ENABLE_VULKAN AND RAWR_VULKAN_SDK_ROOT)
    target_include_directories(b010_b011_fast_experiment PRIVATE "${RAWR_VULKAN_INCLUDE}")
    target_link_libraries(b010_b011_fast_experiment PRIVATE "${RAWR_VULKAN_LIB}" dxgi)
    target_compile_definitions(b010_b011_fast_experiment PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN AND VULKAN_INCLUDE_DIR)
    target_include_directories(b010_b011_fast_experiment PRIVATE ${VULKAN_INCLUDE_DIR})
    target_link_libraries(b010_b011_fast_experiment PRIVATE dxgi)
    target_compile_definitions(b010_b011_fast_experiment PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN AND Vulkan_FOUND)
    target_include_directories(b010_b011_fast_experiment PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(b010_b011_fast_experiment PRIVATE Vulkan::Vulkan dxgi)
    target_compile_definitions(b010_b011_fast_experiment PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN)
    target_compile_definitions(b010_b011_fast_experiment PRIVATE RAWR_HAS_VULKAN=0)
    target_link_libraries(b010_b011_fast_experiment PRIVATE dxgi)
else()
    target_link_libraries(b010_b011_fast_experiment PRIVATE dxgi)
endif()

if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UM_VER)
    set_property(TARGET b010_b011_fast_experiment APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64\"")
endif()
if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UCRT_VER)
    set_property(TARGET b010_b011_fast_experiment APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UCRT_VER}/ucrt/x64\"")
endif()
if(DEFINED _MSVC_ROOT)
    set_property(TARGET b010_b011_fast_experiment APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_MSVC_ROOT}/lib/x64\"")
endif()

target_link_libraries(b010_b011_fast_experiment PRIVATE
    kernel32 user32 gdi32 winspool shell32 ole32 oleaut32 uuid comdlg32 advapi32 version ws2_32
)

# ============================================================================
# B009: Batched Prefill Certification
# ============================================================================
set(B009_BATCHED_PREFILL_SOURCES
    tests/b009/b009_batched_prefill_certification.cpp
    tests/b009/b009b_batched_gemm.cpp
    src/rawrxd_transformer.cpp
    src/rawrxd_transformer_forwardbatch.cpp
    src/rawrxd_model_loader.cpp
    src/rawrxd_tokenizer.cpp
    src/rawrxd_sampler.cpp
    src/gguf_loader.cpp
    src/gguf_vocab_resolver.cpp
    src/core/inference_witness.cpp
    src/core/swarm_scheduler.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp
    src/core/gguf_swarm_plan_builder.cpp
    src/logging/Logger.cpp
    src/runtime/TensorExecutionRouter.cpp
    src/runtime/StreamRouterAdapter.cpp
    src/runtime/memory/PredictiveMemoryManager.cpp
    src/runtime/memory/TransferScheduler.cpp
    src/runtime/memory/ResidencyTracker.cpp
    src/runtime/memory/WorkingSetPredictor.cpp
    src/runtime/memory/CapacityManager.cpp
    src/runtime/memory/TensorPlacementManager.cpp
    src/inference/NegativeSpaceProfiler.cpp
    B014/build/b014_profiler.cpp
)

add_executable(b009_batched_prefill_certification EXCLUDE_FROM_ALL
    ${B009_BATCHED_PREFILL_SOURCES}
)
target_include_directories(b009_batched_prefill_certification PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include/core
)
target_compile_features(b009_batched_prefill_certification PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(b009_batched_prefill_certification PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/EHsc /W0 /arch:AVX512>)
endif()

if(RAWR_ENABLE_VULKAN AND RAWR_VULKAN_SDK_ROOT)
    target_include_directories(b009_batched_prefill_certification PRIVATE "${RAWR_VULKAN_INCLUDE}")
    target_link_libraries(b009_batched_prefill_certification PRIVATE "${RAWR_VULKAN_LIB}" dxgi)
    target_compile_definitions(b009_batched_prefill_certification PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN AND VULKAN_INCLUDE_DIR)
    target_include_directories(b009_batched_prefill_certification PRIVATE ${VULKAN_INCLUDE_DIR})
    target_link_libraries(b009_batched_prefill_certification PRIVATE dxgi)
    target_compile_definitions(b009_batched_prefill_certification PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN AND Vulkan_FOUND)
    target_include_directories(b009_batched_prefill_certification PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(b009_batched_prefill_certification PRIVATE Vulkan::Vulkan dxgi)
    target_compile_definitions(b009_batched_prefill_certification PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN)
    target_compile_definitions(b009_batched_prefill_certification PRIVATE RAWR_HAS_VULKAN=0)
    target_link_libraries(b009_batched_prefill_certification PRIVATE dxgi)
else()
    target_link_libraries(b009_batched_prefill_certification PRIVATE dxgi)
endif()

if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UM_VER)
    set_property(TARGET b009_batched_prefill_certification APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64\"")
endif()
if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UCRT_VER)
    set_property(TARGET b009_batched_prefill_certification APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UCRT_VER}/ucrt/x64\"")
endif()

# ============================================================================
# B009-A Fast Validation (quick correctness check, {1,3,10} tokens only)
# ============================================================================
add_executable(b009a_fast_validation EXCLUDE_FROM_ALL
    tests/b009/b009a_fast_validation.cpp
    src/rawrxd_transformer.cpp
    src/rawrxd_transformer_forwardbatch.cpp
    src/rawrxd_model_loader.cpp
    src/rawrxd_tokenizer.cpp
    src/rawrxd_sampler.cpp
    src/gguf_loader.cpp
    src/gguf_vocab_resolver.cpp
    src/core/inference_witness.cpp
    src/core/swarm_scheduler.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp
    src/core/gguf_swarm_plan_builder.cpp
    src/logging/Logger.cpp
    src/runtime/TensorExecutionRouter.cpp
    src/runtime/memory/PredictiveMemoryManager.cpp
    src/runtime/memory/TransferScheduler.cpp
    src/runtime/memory/ResidencyTracker.cpp
    src/runtime/memory/WorkingSetPredictor.cpp
    src/runtime/memory/CapacityManager.cpp
    src/runtime/memory/TensorPlacementManager.cpp
)
target_include_directories(b009a_fast_validation PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include/core
)
target_compile_features(b009a_fast_validation PRIVATE cxx_std_20)
if(RAWR_ENABLE_VULKAN AND RAWR_VULKAN_SDK_ROOT)
    target_include_directories(b009a_fast_validation PRIVATE "${RAWR_VULKAN_INCLUDE}")
    target_link_libraries(b009a_fast_validation PRIVATE "${RAWR_VULKAN_LIB}" dxgi)
    target_compile_definitions(b009a_fast_validation PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN AND VULKAN_INCLUDE_DIR)
    target_include_directories(b009a_fast_validation PRIVATE ${VULKAN_INCLUDE_DIR})
    target_link_libraries(b009a_fast_validation PRIVATE dxgi)
    target_compile_definitions(b009a_fast_validation PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN AND Vulkan_FOUND)
    target_include_directories(b009a_fast_validation PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(b009a_fast_validation PRIVATE Vulkan::Vulkan dxgi)
    target_compile_definitions(b009a_fast_validation PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN)
    target_compile_definitions(b009a_fast_validation PRIVATE RAWR_HAS_VULKAN=0)
    target_link_libraries(b009a_fast_validation PRIVATE dxgi)
else()
    target_link_libraries(b009a_fast_validation PRIVATE dxgi)
endif()
if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UM_VER)
    set_property(TARGET b009a_fast_validation APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64\"")
endif()
if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UCRT_VER)
    set_property(TARGET b009a_fast_validation APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UCRT_VER}/ucrt/x64\"")
endif()
if(DEFINED _MSVC_ROOT)
    set_property(TARGET b009_batched_prefill_certification APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_MSVC_ROOT}/lib/x64\"")
endif()

target_link_libraries(b009_batched_prefill_certification PRIVATE
    kernel32 user32 gdi32 winspool shell32 ole32 oleaut32 uuid comdlg32 advapi32 version ws2_32
)

# ============================================================================
# B009-T128 Only Certification (narrowly scoped T=128 correctness check)
# ============================================================================
add_executable(b009_t128_certification EXCLUDE_FROM_ALL
    tests/b009/b009_t128_certification.cpp
    tests/b009/b009b_batched_gemm.cpp
    src/rawrxd_transformer.cpp
    src/rawrxd_transformer_forwardbatch.cpp
    src/rawrxd_model_loader.cpp
    src/rawrxd_tokenizer.cpp
    src/rawrxd_sampler.cpp
    src/gguf_loader.cpp
    src/gguf_vocab_resolver.cpp
    src/core/inference_witness.cpp
    src/core/swarm_scheduler.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp
    src/core/gguf_swarm_plan_builder.cpp
    src/logging/Logger.cpp
    src/runtime/TensorExecutionRouter.cpp
    src/runtime/StreamRouterAdapter.cpp
    src/runtime/memory/PredictiveMemoryManager.cpp
    src/runtime/memory/TransferScheduler.cpp
    src/runtime/memory/ResidencyTracker.cpp
    src/runtime/memory/WorkingSetPredictor.cpp
    src/runtime/memory/CapacityManager.cpp
    src/runtime/memory/TensorPlacementManager.cpp
    src/inference/NegativeSpaceProfiler.cpp
    B014/build/b014_profiler.cpp
)
target_include_directories(b009_t128_certification PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include/core
)
target_compile_features(b009_t128_certification PRIVATE cxx_std_20)
if(RAWR_ENABLE_VULKAN AND RAWR_VULKAN_SDK_ROOT)
    target_include_directories(b009_t128_certification PRIVATE "${RAWR_VULKAN_INCLUDE}")
    target_link_libraries(b009_t128_certification PRIVATE "${RAWR_VULKAN_LIB}" dxgi)
    target_compile_definitions(b009_t128_certification PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN AND VULKAN_INCLUDE_DIR)
    target_include_directories(b009_t128_certification PRIVATE ${VULKAN_INCLUDE_DIR})
    target_link_libraries(b009_t128_certification PRIVATE dxgi)
    target_compile_definitions(b009_t128_certification PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN AND Vulkan_FOUND)
    target_include_directories(b009_t128_certification PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(b009_t128_certification PRIVATE Vulkan::Vulkan dxgi)
    target_compile_definitions(b009_t128_certification PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN)
    target_compile_definitions(b009_t128_certification PRIVATE RAWR_HAS_VULKAN=0)
    target_link_libraries(b009_t128_certification PRIVATE dxgi)
else()
    target_link_libraries(b009_t128_certification PRIVATE dxgi)
endif()
if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UM_VER)
    set_property(TARGET b009_t128_certification APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64\"")
endif()
if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UCRT_VER)
    set_property(TARGET b009_t128_certification APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UCRT_VER}/ucrt/x64\"")
endif()
if(DEFINED _MSVC_ROOT)
    set_property(TARGET b009_t128_certification APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_MSVC_ROOT}/lib/x64\"")
endif()
target_link_libraries(b009_t128_certification PRIVATE
    kernel32 user32 gdi32 winspool shell32 ole32 oleaut32 uuid comdlg32 advapi32 version ws2_32
)

# ============================================================================
# B016: GEMM Efficiency Probe (instrumented batched GEMM profiling)
# ============================================================================
add_executable(b016_gemm_efficiency_probe EXCLUDE_FROM_ALL
    tests/b016/b016_gemm_efficiency_probe.cpp
    tests/b009/b009b_batched_gemm.cpp
    src/rawrxd_transformer.cpp
    src/rawrxd_transformer_forwardbatch.cpp
    src/rawrxd_model_loader.cpp
    src/rawrxd_tokenizer.cpp
    src/rawrxd_sampler.cpp
    src/gguf_loader.cpp
    src/gguf_vocab_resolver.cpp
    src/core/inference_witness.cpp
    src/core/swarm_scheduler.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp
    src/core/gguf_swarm_plan_builder.cpp
    src/logging/Logger.cpp
    src/runtime/TensorExecutionRouter.cpp
    src/runtime/StreamRouterAdapter.cpp
    src/runtime/memory/PredictiveMemoryManager.cpp
    src/runtime/memory/TransferScheduler.cpp
    src/runtime/memory/ResidencyTracker.cpp
    src/runtime/memory/WorkingSetPredictor.cpp
    src/runtime/memory/CapacityManager.cpp
    src/runtime/memory/TensorPlacementManager.cpp
    src/inference/NegativeSpaceProfiler.cpp
    B014/build/b014_profiler.cpp
)
target_include_directories(b016_gemm_efficiency_probe PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include/core
    ${CMAKE_SOURCE_DIR}/tests/inference_validation/harness
)
target_compile_features(b016_gemm_efficiency_probe PRIVATE cxx_std_20)
if(RAWR_ENABLE_VULKAN AND RAWR_VULKAN_SDK_ROOT)
    target_include_directories(b016_gemm_efficiency_probe PRIVATE "${RAWR_VULKAN_INCLUDE}")
    target_link_libraries(b016_gemm_efficiency_probe PRIVATE "${RAWR_VULKAN_LIB}" dxgi)
    target_compile_definitions(b016_gemm_efficiency_probe PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN AND VULKAN_INCLUDE_DIR)
    target_include_directories(b016_gemm_efficiency_probe PRIVATE ${VULKAN_INCLUDE_DIR})
    target_link_libraries(b016_gemm_efficiency_probe PRIVATE dxgi)
    target_compile_definitions(b016_gemm_efficiency_probe PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN AND Vulkan_FOUND)
    target_include_directories(b016_gemm_efficiency_probe PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(b016_gemm_efficiency_probe PRIVATE Vulkan::Vulkan dxgi)
    target_compile_definitions(b016_gemm_efficiency_probe PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN)
    target_compile_definitions(b016_gemm_efficiency_probe PRIVATE RAWR_HAS_VULKAN=0)
    target_link_libraries(b016_gemm_efficiency_probe PRIVATE dxgi)
else()
    target_link_libraries(b016_gemm_efficiency_probe PRIVATE dxgi)
endif()
if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UM_VER)
    set_property(TARGET b016_gemm_efficiency_probe APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64\"")
endif()
if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UCRT_VER)
    set_property(TARGET b016_gemm_efficiency_probe APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UCRT_VER}/ucrt/x64\"")
endif()
if(DEFINED _MSVC_ROOT)
    set_property(TARGET b016_gemm_efficiency_probe APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_MSVC_ROOT}/lib/x64\"")
endif()
target_link_libraries(b016_gemm_efficiency_probe PRIVATE
    kernel32 user32 gdi32 winspool shell32 ole32 oleaut32 uuid comdlg32 advapi32 version ws2_32
)

# ============================================================================
# B009-T128 Instrumented Hang Diagnosis
# ============================================================================
add_executable(b009_t128_instrumented EXCLUDE_FROM_ALL
    tests/b009/b009_t128_instrumented.cpp
    src/rawrxd_transformer.cpp
    src/rawrxd_transformer_forwardbatch.cpp
    src/rawrxd_model_loader.cpp
    src/rawrxd_tokenizer.cpp
    src/rawrxd_sampler.cpp
    src/gguf_loader.cpp
    src/gguf_vocab_resolver.cpp
    src/core/inference_witness.cpp
    src/core/swarm_scheduler.cpp
    src/core/memory_stubs.cpp
    src/core/link_stubs.cpp
    src/core/gguf_swarm_plan_builder.cpp
    src/logging/Logger.cpp
    src/runtime/TensorExecutionRouter.cpp
    src/runtime/memory/PredictiveMemoryManager.cpp
    src/runtime/memory/TransferScheduler.cpp
    src/runtime/memory/ResidencyTracker.cpp
    src/runtime/memory/WorkingSetPredictor.cpp
    src/runtime/memory/CapacityManager.cpp
    src/runtime/memory/TensorPlacementManager.cpp
)
target_include_directories(b009_t128_instrumented PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include/core
)
target_compile_features(b009_t128_instrumented PRIVATE cxx_std_20)
if(RAWR_ENABLE_VULKAN AND RAWR_VULKAN_SDK_ROOT)
    target_include_directories(b009_t128_instrumented PRIVATE "${RAWR_VULKAN_INCLUDE}")
    target_link_libraries(b009_t128_instrumented PRIVATE "${RAWR_VULKAN_LIB}" dxgi)
    target_compile_definitions(b009_t128_instrumented PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN AND VULKAN_INCLUDE_DIR)
    target_include_directories(b009_t128_instrumented PRIVATE ${VULKAN_INCLUDE_DIR})
    target_link_libraries(b009_t128_instrumented PRIVATE dxgi)
    target_compile_definitions(b009_t128_instrumented PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN AND Vulkan_FOUND)
    target_include_directories(b009_t128_instrumented PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(b009_t128_instrumented PRIVATE Vulkan::Vulkan dxgi)
    target_compile_definitions(b009_t128_instrumented PRIVATE RAWR_HAS_VULKAN=1)
elseif(RAWR_ENABLE_VULKAN)
    target_compile_definitions(b009_t128_instrumented PRIVATE RAWR_HAS_VULKAN=0)
    target_link_libraries(b009_t128_instrumented PRIVATE dxgi)
else()
    target_link_libraries(b009_t128_instrumented PRIVATE dxgi)
endif()
if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UM_VER)
    set_property(TARGET b009_t128_instrumented APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64\"")
endif()
if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UCRT_VER)
    set_property(TARGET b009_t128_instrumented APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_WIN10_SDK_ROOT}/Lib/${_SDK_UCRT_VER}/ucrt/x64\"")
endif()
if(DEFINED _MSVC_ROOT)
    set_property(TARGET b009_t128_instrumented APPEND_STRING PROPERTY LINK_FLAGS " /LIBPATH:\"${_MSVC_ROOT}/lib/x64\"")
endif()
target_link_libraries(b009_t128_instrumented PRIVATE
    kernel32 user32 gdi32 winspool shell32 ole32 oleaut32 uuid comdlg32 advapi32 version ws2_32 psapi
)

if(BUILD_TESTING)
    add_test(NAME b009_batched_prefill_certification
             COMMAND b009_batched_prefill_certification)
endif()
