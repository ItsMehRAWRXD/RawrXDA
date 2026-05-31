cmake_minimum_required(VERSION 3.20)

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
    if(EXISTS "D:/VS2022Enterprise/VC/Tools/MSVC")
        file(GLOB _MSVC_VERS "D:/VS2022Enterprise/VC/Tools/MSVC/*")
    elseif(EXISTS "C:/VS2022Enterprise/VC/Tools/MSVC")
        file(GLOB _MSVC_VERS "C:/VS2022Enterprise/VC/Tools/MSVC/*")
    elseif(EXISTS "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC")
        file(GLOB _MSVC_VERS "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/*")
    else()
        set(_MSVC_VERS "")
    endif()
    if(_MSVC_VERS)
        list(SORT _MSVC_VERS)
        list(GET _MSVC_VERS -1 _MSVC_ROOT)
    else()
        set(_MSVC_ROOT "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.44.35207")
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

# RuntimeProvider 512-token stress certification harness (Milestone A gate)
if(WIN32)
    add_executable(RawrXD-RuntimeProviderStress EXCLUDE_FROM_ALL
        src/tests/runtime_provider_512_stress.cpp
        src/runtime/RuntimeProvider.cpp
        src/standalone_llama_runtime.cpp
    )

    target_include_directories(RawrXD-RuntimeProviderStress PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_compile_options(RawrXD-RuntimeProviderStress PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/EHsc>
        $<$<CXX_COMPILER_ID:MSVC>:/W2>
        $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/O2>
        $<$<CXX_COMPILER_ID:MSVC>:/std:c++17>
    )

    target_link_libraries(RawrXD-RuntimeProviderStress PRIVATE
        kernel32
        user32
    )

    set_target_properties(RawrXD-RuntimeProviderStress PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME RawrXD-RuntimeProviderStress
        ENABLE_EXPORTS OFF
    )

    message(STATUS "[RuntimeProvider] RawrXD-RuntimeProviderStress target added")
endif()

# Force Windows SDK version for VS generator (avoid 10.0.26100.0 if not installed)
if(WIN32)
    set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION "10.0.22621.0" CACHE STRING "Windows SDK version for VS generator" FORCE)
endif()

# RawrXD - Zero Bloat Horizon (Phase 14.7.3+)
project(RawrXD VERSION 14.7.3 LANGUAGES C CXX ASM_MASM)

# MSVC: std::expected (<expected>) is only available at C++23 (STL4038 at C++20).
if(MSVC)
    set(CMAKE_CXX_STANDARD 23)
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
else()
    add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:-O3>)
endif()
# Keep LTCG globally, but avoid executable-only/defaultlib overrides here.
# /NODEFAULTLIB + /ENTRY + /SUBSYSTEM must not leak into DLL targets.
if(MSVC)
    add_link_options($<$<NOT:$<CONFIG:Release>>:/LTCG>)
endif()

# WebView2 and WIL are expected under this optional local SDK path.
set(RAWRXD_LOCAL_WIN32_SDK_DIR "d:/rawrxd/include")
if(EXISTS "${RAWRXD_LOCAL_WIN32_SDK_DIR}")
    include_directories("${RAWRXD_LOCAL_WIN32_SDK_DIR}")
endif()

option(RAWRXD_BUILD_WIN32IDE "Build the legacy Win32IDE target" ON)
option(RAWRXD_BUILD_CLI "Build the rawrxd CLI console target" OFF)
option(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES "Strip stub/shim/mock source units from production targets" OFF)
option(RAWRXD_INCLUDE_STRESS_AND_REPLAY_SOURCES "Include stress/replay harness sources in production targets" OFF)
option(RAWRXD_ENABLE_MISSING_HANDLER_STUBS "Allow missing_handler_stubs.cpp fallback lane for Win32IDE wiring" OFF)
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
        src/cli/rawrxd_cli_link_shims.cpp
        src/res/Resource.rc
        # Agentic components for CLI Ollama testing
        src/agentic/OrchestratorBridge.cpp
        src/agentic/AgenticSubmitInference_Fix.cpp
        src/agentic/AgentOllamaClient.cpp
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

    target_link_libraries(rawrxd PRIVATE
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
    # Explicit absolute library paths ensure linking succeeds even if LIBPATH is dropped
    if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UM_VER)
        target_link_libraries(rawrxd PRIVATE
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
endif()

# Resource compiler for icon/version
enable_language(RC)

message(STATUS "RawrXD v14.7.3 Target Configured - Zero Bloat Mode")

# Default to Release builds (prevents /RTC1 + /O2 clash on Ninja single-config)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    message(STATUS "[Build] Defaulting CMAKE_BUILD_TYPE to Release")
endif()

# CXX standard set above (MSVC 23, others 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static link MSVC runtime — no vcruntime140.dll dependency
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
if(RAWRXD_GOLD_BUILD)
    add_compile_definitions(RAWRXD_GOLD_BUILD=1)
endif()

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
    message(STATUS "[Build Paths] SDK ${_SDK_VER}: ucrt + um + shared")
endif()

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
        src/asm/inference_kernels.asm
        src/asm/token_queue_dequeue.asm
        src/asm/RawrXD_SovereignTokenizerCore.asm
        src/asm/RawrXD_KQuant_Dequant.asm
        src/asm/RawrXD_AVX512_Dequant_BPE.asm
        src/kernels/dequant_q6k_avx512.asm
        src/kernels/kv_accum_avx512.asm
        src/asm/RawrXD_PDBKernel_v3.asm
        src/asm/RawrXD_ModuleEngine.asm
        src/asm/request_patch.asm
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
        # src/asm/native_speed_kernels.asm      # Excluded from ASM_KERNEL_SOURCES: handled by custom command (MASM_OBJECTS) to avoid LNK2005 duplicate symbol with native_speed_kernels.obj
        # src/asm/DirectML_Bridge.asm           # excluded: scaffold/invalid ml64 syntax (__real@*, missing .ENDPROLOG)
        # src/asm/RawrXD_Hotpatch_Kernel.asm    # excluded: scaffold/invalid ml64 syntax (__real@*, missing .ENDPROLOG)
        # src/asm/RawrXD_Snapshot.asm           # excluded: scaffold/invalid ml64 syntax (__real@*, missing .ENDPROLOG)
        # src/asm/RawrXD_Pyre_Compute.asm       # excluded: scaffold/invalid ml64 syntax (__real@*, missing .ENDPROLOG)
        # src/asm/vision_projection_kernel.asm  # excluded: scaffold/invalid ml64 syntax (__real@*, missing .ENDPROLOG)
        # src/asm/quantum_beaconism_backend.asm        # excluded: MASM build errors (undefined symbols / unwind prolog issues)
        # src/asm/RawrXD_DualEngine_QuantumBeacon.asm  # excluded: MASM build errors (undefined symbols / unwind prolog issues)
        # src/asm/RawrXD_DualEngine_Pure.asm    # excluded: ml64 errors (operand size mismatch)
        src/asm/RawrXD_OverclockGov_Pure.asm
        src/asm/RawrXD_Emitter_v1.asm
        src/asm/RawrXD_IPC_Dispatcher.asm
        src/asm/RawrXD_StubSweepBridge.asm
        src/asm/RawrXD_AuditSystem_Pure.asm
        src/asm/Advanced_Code_Deobfuscator.asm
        src/asm/ai_agent_masm_core.asm
        src/asm/Code_Pattern_Reconstructor.asm
        src/asm/agentic_deep_thinking_kernels.asm
        src/kernels/RawrXD_Omega_Singularity.asm
        src/kernels/RawrXD_Omega_Agentic.asm
        # Sovereign FP8 Quantizer - Hardware-independent quantization for RX 7800 XT
        src/kernels/sovereign_fp8_quantizer.asm
        # AVX2 Vectorized FP8 Quantizer - 8x throughput improvement
        src/kernels/sovereign_fp8_quantizer_avx2.asm
        # Sovereign agentic runtime: terminal capture, context buffer, JSON plan parser
        src/asm/RawrXD_TerminalCapture.asm
        src/asm/RawrXD_ContextBuffer.asm
        src/asm/RawrXD_JsonPlanParser.asm
        src/asm/node_heartbeat.asm
        # KV-Cache Aperture Memory Bypass (non-temporal streaming + rolling buffer)
        src/asm/RAWRXD_KV_APERTURE.asm
        src/asm/RawrXD_KVCache_Rolling.asm
        src/asm/Titan_MoE_SparseGather.asm
        # P0 Gaps: Vector Indexing & Search
        src/asm/RawrXD_AVX512_VectorSearch.asm
        # Sovereign terminal capture with anonymous pipe ring buffer
        src/asm/terminal_pipe_engine.asm
        # AVX-512 cosine similarity kernel for HNSW vector indexing
        src/asm/vector_index_avx512.asm
        # Tiered dispatch infrastructure: MXCSR determinism, zero-copy ring buffer, UMS scheduler
        src/asm/mxcsr_determinism.asm
        src/asm/zerocopy_ring_bridge.asm
        src/asm/ums_micro_scheduler.asm
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
    add_compile_definitions(RAWRXD_MASM_CORE_NATIVE_BRIDGE=1)
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

# Hardened sovereign kernel target: compile persistent contract shader for Titan lane.
set(RAWRXD_SOVEREIGN_SHADER_SRC "${CMAKE_SOURCE_DIR}/src/shaders/rawr_kernel_hooks.comp")
set(RAWRXD_SOVEREIGN_SHADER_SPV "${RAWRXD_TEST_SHADER_DIR}/rawr_kernel_hooks.spv")

find_program(RAWRXD_GLSLANG_VALIDATOR glslangValidator)
find_program(RAWRXD_GLSLC glslc)

if(RAWRXD_GLSLANG_VALIDATOR)
    add_custom_command(
        OUTPUT ${RAWRXD_SOVEREIGN_SHADER_SPV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RAWRXD_TEST_SHADER_DIR}
        COMMAND ${RAWRXD_GLSLANG_VALIDATOR}
                -V
                --target-env vulkan1.3
                -o ${RAWRXD_SOVEREIGN_SHADER_SPV}
                ${RAWRXD_SOVEREIGN_SHADER_SRC}
        DEPENDS ${RAWRXD_SOVEREIGN_SHADER_SRC}
        COMMENT "Generating sovereign SPIR-V shader with glslangValidator: rawr_kernel_hooks.spv"
        VERBATIM
    )
elseif(RAWRXD_GLSLC)
    add_custom_command(
        OUTPUT ${RAWRXD_SOVEREIGN_SHADER_SPV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RAWRXD_TEST_SHADER_DIR}
        COMMAND ${RAWRXD_GLSLC}
                -fshader-stage=compute
                --target-env=vulkan1.3
                -O
                ${RAWRXD_SOVEREIGN_SHADER_SRC}
                -o ${RAWRXD_SOVEREIGN_SHADER_SPV}
        DEPENDS ${RAWRXD_SOVEREIGN_SHADER_SRC}
        COMMENT "Generating sovereign SPIR-V shader with glslc: rawr_kernel_hooks.spv"
        VERBATIM
    )
else()
    message(WARNING "[Sovereign] Neither glslangValidator nor glslc found; rawr_kernel_hooks.spv target will be unavailable.")
endif()

if(RAWRXD_GLSLANG_VALIDATOR OR RAWRXD_GLSLC)
    add_custom_target(rawrxd-sovereign-kernel-spv DEPENDS ${RAWRXD_SOVEREIGN_SHADER_SPV})
endif()

# FP8 Quantization Shader
set(RAWRXD_FP8_SHADER_SRC "${CMAKE_SOURCE_DIR}/src/quantization/shaders/fp8_quantize.comp")
set(RAWRXD_FP8_SHADER_SPV "${RAWRXD_TEST_SHADER_DIR}/fp8_quantize.spv")

if(RAWRXD_GLSLANG_VALIDATOR)
    add_custom_command(
        OUTPUT ${RAWRXD_FP8_SHADER_SPV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RAWRXD_TEST_SHADER_DIR}
        COMMAND ${RAWRXD_GLSLANG_VALIDATOR}
                -V
                --target-env vulkan1.3
                -o ${RAWRXD_FP8_SHADER_SPV}
                ${RAWRXD_FP8_SHADER_SRC}
        DEPENDS ${RAWRXD_FP8_SHADER_SRC}
        COMMENT "Generating FP8 SPIR-V shader: fp8_quantize.spv"
        VERBATIM
    )
elseif(RAWRXD_GLSLC)
    add_custom_command(
        OUTPUT ${RAWRXD_FP8_SHADER_SPV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RAWRXD_TEST_SHADER_DIR}
        COMMAND ${RAWRXD_GLSLC}
                -fshader-stage=compute
                --target-env=vulkan1.3
                -O
                ${RAWRXD_FP8_SHADER_SRC}
                -o ${RAWRXD_FP8_SHADER_SPV}
        DEPENDS ${RAWRXD_FP8_SHADER_SRC}
        COMMENT "Generating FP8 SPIR-V shader with glslc: fp8_quantize.spv"
        VERBATIM
    )
endif()

if(RAWRXD_GLSLANG_VALIDATOR OR RAWRXD_GLSLC)
    add_custom_target(rawrxd-fp8-shader-spv DEPENDS ${RAWRXD_FP8_SHADER_SPV})
endif()

# FlashAttention FP8 minimal passthrough (Vulkan path in flash_attention_vulkan_fp8.cpp)
set(RAWRXD_FA8_MINIMAL_COMP "${CMAKE_SOURCE_DIR}/src/inference/shaders/flash_attention_fp8_minimal_test.comp")
set(RAWRXD_FA8_MINIMAL_SPV "${CMAKE_SOURCE_DIR}/src/inference/shaders/flash_attention_fp8_minimal_test.spv")

if(RAWRXD_GLSLANG_VALIDATOR)
    add_custom_command(
        OUTPUT ${RAWRXD_FA8_MINIMAL_SPV}
        COMMAND ${RAWRXD_GLSLANG_VALIDATOR}
                -V
                --target-env vulkan1.3
                -o ${RAWRXD_FA8_MINIMAL_SPV}
                ${RAWRXD_FA8_MINIMAL_COMP}
        DEPENDS ${RAWRXD_FA8_MINIMAL_COMP}
        COMMENT "Compiling flash_attention_fp8_minimal_test.comp -> SPIR-V (Sovereign FA8)"
        VERBATIM
    )
elseif(RAWRXD_GLSLC)
    add_custom_command(
        OUTPUT ${RAWRXD_FA8_MINIMAL_SPV}
        COMMAND ${RAWRXD_GLSLC}
                -fshader-stage=compute
                --target-env=vulkan1.3
                -O
                ${RAWRXD_FA8_MINIMAL_COMP}
                -o ${RAWRXD_FA8_MINIMAL_SPV}
        DEPENDS ${RAWRXD_FA8_MINIMAL_COMP}
        COMMENT "Compiling flash_attention_fp8_minimal_test.comp with glslc -> SPIR-V"
        VERBATIM
    )
else()
    message(STATUS "[FA8] glslangValidator/glslc not found; flash_attention_fp8_minimal_test.spv will not be auto-regenerated")
endif()

if(RAWRXD_GLSLANG_VALIDATOR OR RAWRXD_GLSLC)
    add_custom_target(rawrxd-fa8-minimal-spv DEPENDS ${RAWRXD_FA8_MINIMAL_SPV})
endif()

# FlashAttention FP8 tiled scaffold (online softmax + LDS path; not wired to dispatch yet)
set(RAWRXD_FA8_TILED_COMP "${CMAKE_SOURCE_DIR}/src/inference/shaders/flash_attention_fp8_tiled.comp")
set(RAWRXD_FA8_TILED_SPV "${CMAKE_SOURCE_DIR}/src/inference/shaders/flash_attention_fp8_tiled.spv")

if(RAWRXD_GLSLANG_VALIDATOR)
    add_custom_command(
        OUTPUT ${RAWRXD_FA8_TILED_SPV}
        COMMAND ${RAWRXD_GLSLANG_VALIDATOR}
                -V
                --target-env vulkan1.3
                -o ${RAWRXD_FA8_TILED_SPV}
                ${RAWRXD_FA8_TILED_COMP}
        DEPENDS ${RAWRXD_FA8_TILED_COMP}
        COMMENT "Compiling flash_attention_fp8_tiled.comp -> SPIR-V (tiled scaffold)"
        VERBATIM
    )
elseif(RAWRXD_GLSLC)
    add_custom_command(
        OUTPUT ${RAWRXD_FA8_TILED_SPV}
        COMMAND ${RAWRXD_GLSLC}
                -fshader-stage=compute
                --target-env=vulkan1.3
                -O
                ${RAWRXD_FA8_TILED_COMP}
                -o ${RAWRXD_FA8_TILED_SPV}
        DEPENDS ${RAWRXD_FA8_TILED_COMP}
        COMMENT "Compiling flash_attention_fp8_tiled.comp with glslc -> SPIR-V"
        VERBATIM
    )
endif()

if(RAWRXD_GLSLANG_VALIDATOR OR RAWRXD_GLSLC)
    add_custom_target(rawrxd-fa8-tiled-spv DEPENDS ${RAWRXD_FA8_TILED_SPV})
endif()

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
    src/native_ai_integration.cpp
    src/memory_core.cpp
    src/hot_patcher.cpp
    src/cpu_inference_engine.cpp
    src/dispatch_orchestrator.cpp
    src/rawrxd_model_loader.cpp
    src/rawrxd_transformer.cpp
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
    src/rxa_packer.cpp
    src/prometheus_lowmem.cpp
    
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
    src/core/system_integrity_audit_trail.cpp
    src/core/diff_engine.cpp
    src/core/thread_lifecycle_registry.cpp
    
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
    src/core/hotpatch_wiring.cpp
    src/core/multi_response_engine.cpp
    src/core/streaming_engine_registry.cpp
    src/core/flash_attention.cpp
    src/core/gpu_backend_bridge.cpp
    src/core/gpu_warming_engine.cpp
    
    # Native speed layer + local AI core (zero-dependency inference)
    src/core/native_speed_layer.cpp
    src/core/local_ai_core.cpp
    src/core/native_inference_pipeline.cpp
    src/core/speculative_inference_engine.cpp
    
    # Layer Offload Manager — RAM streaming for 74B+ models on 16GB VRAM
    src/core/layer_offload_manager.cpp

    # Sovereign FP8 Quantizer — Hardware-independent quantization for RX 7800 XT
    src/kernels/sovereign_fp8_quantizer.cpp

    # FP8 Quantization Implementation (E4M3/E5M2 formats)
    src/quantization/fp8_quantizer.cpp
    src/quantization/fp8_gpu_dispatch.cpp
    src/quantization/fp8_kv_cache_integration.cpp

    # Three-layer hotpatch system (needed by decision tree)
    src/core/model_memory_hotpatch.cpp
    src/core/byte_level_hotpatcher.cpp
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

    # P0 Gaps: Vector Indexing & Multi-file editing
    src/CodebaseVectorIndex.cpp
    src/MultiFileRewriteEngine.cpp
    src/SpeculativeDecoder.cpp
    src/GitMCPBridge.cpp
    src/PredictiveEditEngine.cpp

    # Shadow-Page Detour + Sentinel Watchdog (Layer 6)
    src/core/shadow_page_detour.cpp
    src/core/sentinel_watchdog.cpp

    # Agentic Loop State (iterative reasoning state machine)
    src/agentic_loop_state.cpp

    # Autonomous SubAgents — Bulk Fix Orchestration for models/swarms
    src/agent/autonomous_subagent.cpp
    src/agent/agentic_failure_detector.cpp
    src/agent/agentic_puppeteer.cpp

    # HTTP Client — WinHTTP/curl async with future retention
    src/agent/llm_http_client.cpp
    src/agent/hexmag_client.cpp
    src/agent/model_invoker.cpp
    src/agent/planner.cpp
    src/agent/eval_framework.cpp
    src/backend/ollama_client.cpp
    src/tools/file_ops.cpp
    src/tools/git_client.cpp

    # LocalReasoningEngine — API-free static analysis + pattern detection (no Qt, no API key)
    src/agent/local_reasoning_engine.cpp

    # Agent Self-Repair (MASM64 self-patching kernel bridge)
    src/agent/agent_self_repair.cpp
    src/agent/agent_self_healing_orchestrator.cpp

    # T3-D: Deterministic Replay Engine (DivergenceEvent + TelemetrySnapshot)
    src/agentic/DeterministicReplayEngine.cpp
    src/agentic/AgentToolHandlers.cpp
    src/agentic/DiffEngine.cpp
    src/agentic/AgenticChatSession.cpp
    src/agentic/RepositoryIndexer.cpp
    src/agentic/slash_command_parser.cpp
    src/agentic/multi_file_edit_plan.cpp
    src/core/scoped_instructions_provider.cpp
    src/indexing/incremental_indexer.cpp

    # T4: Autonomous Recovery Orchestrator (FailureClass → recovery → verify → commit)
    src/agentic/autonomous_recovery_orchestrator.cpp

    # T5: Agent Orchestrator + Bridge + Navigation (agentic brain wiring)
    src/agentic/AgentOrchestrator.cpp
    src/agentic/OrchestratorBridge.cpp
    src/agentic/AgenticSubmitInference_Fix.cpp
    src/agentic/AgenticNavigator.cpp
    src/agentic/RawrXD_ToolRegistry.cpp
    src/agentic/ToolRegistry.cpp
    src/agentic/SovereignAssembler.cpp
    src/agentic/RawrXD_AgentLoop.cpp
    src/agentic/agentic_executor.cpp
    
    # Phase 1: Streaming Result Injection for Tool Execution (P0.1)
    src/agentic/StreamingResultChannel.cpp

    # T6: Autonomous Repair Orchestrator + Agent Memory Indexer
    src/core/auto_repair_orchestrator.cpp
    src/core/agent_memory_indexer.cpp

    # Disk Recovery Agent (C++ wrapper for ASM SCSI kernel)
    src/agent/DiskRecoveryAgent.cpp
    
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
    
    # Phase 25: AMD/ATI GPU Acceleration Toggle
    src/core/amd_gpu_accelerator.cpp
    src/core/gpu_warmup.cpp
    src/core/unified_memory_executor.cpp
    src/core/model_runtime_gate.cpp
    src/runtime_surface/GenerationStopwatch.cpp
    
    # Phase 29: Multi-Architecture GPU/Accelerator Backends
    src/core/intel_gpu_accelerator.cpp
    src/core/arm64_gpu_accelerator.cpp
    src/core/cerebras_wse_accelerator.cpp
    
    # Phase 30: Unified Accelerator Router
    src/core/accelerator_router.cpp
    
    # Phase 31: NVIDIA CUDA Backend (Driver API)
    src/core/nvidia_cuda_accelerator.cpp
    
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
    # NOTE: tool_registry_init.cpp NOT included here — real implementations are in
    # src/modules/inference_engine.cpp and src/engine/sovereign_engines.cpp
    # tool_registry_init.cpp is linker fallback-only (for targets that don't link those modules)

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

    # Agent failure detect SIMD stub (masm_agent_failure_detect_simd) — required by agentic_failure_detector
    src/core/ai_agent_masm_stubs.cpp

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

    # Subsystem mode exports (CompileMode, EncryptMode, InjectMode, etc.) — required by rawrxd_subsystem_api
    src/core/rawrxd_subsys_modes_a.cpp
    src/core/rawrxd_subsys_modes_b.cpp
    src/core/rawrxd_subsys_modes_c.cpp

    # Feature registry (FeatureRegistry::instance, registerFeature) — required by enterprise_feature_manager
    src/core/feature_registry.cpp
    # Menu auditor (RawrXD::Audit::verifyCommandInMenu) — required by feature_registry
    src/core/menu_auditor.cpp

    # GGUF diagnostic provider + LSP server — required by lsp_hotpatch_bridge
    src/lsp/gguf_diagnostic_provider.cpp
    src/lsp/RawrXD_LSPServer.cpp

    # Extension polyfill engine + VSCode API — required by js_extension_host
    src/core/extension_polyfill_engine.cpp
    # src/modules/vscode_extension_api.cpp  # TEMPORARILY DISABLED - causing build issues

    # JS extension host — required by ssot_handlers_ext
    src/core/js_extension_host.cpp

    # Extension Scheduler Bridge — integrates Extension API with ExecutionScheduler v2
    src/extension_host/ExtensionSchedulerBridge.cpp

    # Model source resolver — required by auto_feature_registry
    src/model_source_resolver.cpp

    # ReverseEngineering (NativeDisassembler, BinaryAnalyzer, RECodex, NativeCompiler) — required by auto_feature_registry
    src/modules/ReverseEngineering.cpp

    # AgenticDeepThinkingEngine full implementation
    src/agent/agentic_deep_thinking_engine.cpp

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

    # Cathedral Hardening (5.0 Readiness)
    src/core/crash_containment.cpp
    src/core/patch_rollback_ledger.cpp
    src/core/quant_hysteresis.cpp
    src/core/masm_stress_harness.cpp
    src/core/perf_telemetry.cpp
    src/core/convergence_stress_harness.cpp

    # GAP 6: Git Context Provider (CLI subprocess)
    src/git/git_context.cpp

    # GAP 7: LSP Diagnostic Consumer + Quick-Fix
    src/lsp/diagnostic_consumer.cpp

    # GAP 8: Jinja2-Style Prompt Template Engine
    src/core/prompt_template_engine.cpp

    # GAP 9: Speculative Decoder v2 (Draft-Verify-Accept)
    src/gpu/speculative_decoder_v2.cpp

    # AGI Gate 1: Speculative Inference Bridge — wires V2 to AutonomousInferenceEngine
    src/gpu/speculative_inference_bridge.cpp

    # Autonomous inference engine (required by speculative_decoder.cpp for draft token generation)
    src/inference/ultra_fast_inference.cpp

    # Token Pipeline - double-buffered token flow for measurable TPS
    src/inference/token_pipeline.cpp

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
    # Valuation-Critical Platform Subsystems (>2× step-up from $40M baseline)
    # ═══════════════════════════════════════════════════════════════════

    # Autonomous End-to-End Workflow Engine (scan → bulk_fix → verify → build → test → diff)
    src/core/autonomous_workflow_engine.cpp

    # Workspace-Level Reasoning Profiles (fast/dev/critical per repo)
    src/core/workspace_reasoning_profiles.cpp

    # Deterministic Swarm Reproducibility (same input + seed = same result)
    src/core/deterministic_swarm.cpp
    src/core/swarm_scheduler.cpp
    src/core/gguf_swarm_plan_builder.cpp

    # Safe-by-Default Bulk Refactor Engine + Diff-Aware Verification Gates
    src/core/safe_refactor_engine.cpp

    # Reasoning Schema Versioning (semver, migration, backwards-compatible)
    src/core/reasoning_schema_versioning.cpp

    # CoT Backend Disable/Fallback System (circuit breaker, graceful degradation)
    src/core/cot_fallback_system.cpp

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

    # Phase 15b: Code Linter — Real-time syntax/semantic linting (C++, ASM, Python, JS)
    src/core/code_linter.cpp

    # Phase 16: Semantic Code Intelligence — Cross-refs, type inference, indexing
    src/core/semantic_code_intelligence.cpp

    # Phase 17: Enterprise Telemetry & Compliance — OTLP tracing, audit, GDPR
    src/core/enterprise_telemetry_compliance.cpp

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
    # Prometheus 120B MoE Engine — Frontier-class inference (MoE, GQA, Ring Attention,
    # Flash Attention, Speculative Decoding, Tool Calling, Artifacts, Extended Thinking)
    # ═══════════════════════════════════════════════════════════════════
    src/prometheus/prometheus_bindings.cpp
    src/prometheus/prometheus_weight_loader.cpp

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

    # ── Transcendence Architecture (Phases E → Ω) ─────────────────────
    src/core/self_host_engine.cpp
    src/core/hardware_synthesizer.cpp
    src/core/mesh_brain.cpp
    src/core/speciator_engine.cpp
    src/core/neural_bridge.cpp
    src/core/omega_orchestrator.cpp
    src/core/transcendence_coordinator.cpp

    # Quantum Production Orchestrator — Master orchestrator for quantum autonomous systems
    src/agent/quantum_autonomous_todo_system.cpp
    src/agent/quantum_multi_model_agent_cycling.cpp
    src/agent/quantum_dynamic_time_manager.cpp
    src/agent/quantum_dynamic_time_manager_impl.cpp
    src/agent/quantum_missing_impl.cpp
    src/agent/quantum_production_orchestrator.cpp

    # ── Transcendence IDE Panel ────────────────────────────────────────
    src/win32app/Win32IDE_TranscendencePanel.cpp

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
        src/core/dml_asm_fallback.cpp
        src/win32app/agentic_bridge_headless.cpp
        src/win32app/Win32IDE_logMessage.cpp
    )
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
    )
    # AUTO lane uses `auto_feature_registry.cpp` + unified dispatch; do not also link EXT handlers.
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
    src/core/ConfigurationValidator.cpp
    src/inference/PerformanceMonitor.cpp
    src/agentic/ErrorRecoveryManager.cpp

    # Headless-safe production command handlers for SSOT/unified dispatch.
    src/core/rawrengine_command_handlers.cpp
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
    src/core/byte_level_hotpatcher.cpp
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
    src/core/js_extension_host_headless_stubs.cpp
    src/core/shadow_page_detour.cpp
    src/core/camellia256_bridge.cpp
    src/core/rawrxd_native_log_fallback.cpp
)

# =============================================================================
# RawrEngine Lane B (Headless Minimal) — curated list override
# -----------------------------------------------------------------------------
# Hard override to prevent accidental linkage of Win32IDE/Hotpatch/Omega layers.
# =============================================================================
set(RAWR_ENGINE_SOURCES
    src/main_headless_core.cpp
    src/gguf_loader.cpp
    src/gguf_vocab_resolver.cpp
    src/rawrxd_inference.cpp
    src/rawrxd_transformer.cpp
    src/rawrxd_tokenizer.cpp
    src/rawrxd_sampler.cpp
    src/cpu_inference_engine.cpp
    src/dispatch_orchestrator.cpp
    src/rawrxd_model_loader.cpp
    src/logging/Logger.cpp
    src/core/swarm_scheduler.cpp
    src/core/gguf_swarm_plan_builder.cpp
    src/core/headless_subsystem_stubs.cpp
    src/core/js_extension_host_headless_stubs.cpp
    tests/gguf_link_stubs.cpp
    src/win32app/agentic_bridge_headless.cpp
    src/win32app/agentic_headless_laneb_link_stubs.cpp
    src/win32app/bulk_fix_orchestrator_laneb_stub.cpp
    src/agentic/StreamingResultChannel.cpp
    src/agentic/observability/Logger.cpp
)
message(STATUS "[RawrEngine LaneB] Using curated headless core sources")

set(RAWR_ENGINE_LANE_B ON)

set(RAWR_ENGINE_ASM_SOURCES
    src/asm/rawr_cpu_features.asm
    src/asm/rawr_globals.asm
    src/asm/model_streamer_x64.asm
    src/asm/mxcsr_determinism.asm
    src/asm/zerocopy_ring_bridge.asm
    src/asm/ums_micro_scheduler.asm
    src/asm/inference_core.asm
    src/asm/RawrXD_SovereignTokenizerCore.asm
    src/kernels/dequant_q6k_avx512.asm
    src/kernels/kv_accum_avx512.asm
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
# Reverse Engineering Suite (sub-module)
# =============================================================================
add_subdirectory(src)
add_subdirectory(src/reverse_engineering)

add_executable(RawrEngine ${RAWR_ENGINE_SOURCES} ${RAWR_ENGINE_ASM_SOURCES})

if(WIN32)
    add_dependencies(RawrEngine regen-ssot-beacons verify-ssot-ext-ownership)
endif()

# Lane B: curated sources already include required stubs.

# Link RE library into RawrEngine
target_link_libraries(RawrEngine PRIVATE RawrXD-RE-Library)

target_include_directories(RawrEngine PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/win32app
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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/prometheus
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
)

# Lane B: keep RawrEngine minimal (no debugger/ollama shims).
if(NOT RAWR_ENGINE_LANE_B)
# Sources used by missing_handler_stubs: debugger engine + symbols + Ollama client
# Lane B is deliberately minimal; do not pull these subsystems in.
if(NOT RAWR_ENGINE_LANE_B)
    target_sources(RawrEngine PRIVATE
        src/core/native_debugger_engine.cpp
        src/core/native_debugger_symbols.cpp
        src/agentic/AgentOllamaClient.cpp
    )
    if(NOT RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
        target_sources(RawrEngine PRIVATE src/core/rawr_engine_link_shims.cpp)
    endif()
endif()
    if(RAWR_SSOT_PROVIDER_UPPER STREQUAL "EXT" OR RAWR_SSOT_PROVIDER_UPPER STREQUAL "STUBS")
        target_sources(RawrEngine PRIVATE src/core/auto_feature_stub_impl.cpp)
    endif()
endif()

# Win32 system libraries (same as Gold)
if(WIN32)
    target_link_libraries(RawrEngine PRIVATE
        shlwapi psapi dbghelp comctl32 comdlg32 shell32 ole32 oleaut32 uuid
        winhttp ws2_32 ntdll advapi32 crypt32 opengl32 wininet gdi32 user32 bcrypt
        d3d12 dxgi dxcore
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
if(NOT RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
    list(APPEND GOLD_UNDERSCORE_SOURCES src/core/rawr_engine_link_shims.cpp)
endif()
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
        src/core/auto_feature_impl.cpp
        src/core/missing_handler_stubs.cpp
    )
    set(_RAWR_GOLD_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS 0)
endif()
if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
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
    list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/.*_fallback[^/]*\\.asm$")
    list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/.*_(mock|fake)\\.asm$")
    list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/.*(mock_|fake_).*\\.asm$")
    list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/link_stubs_.*\\.cpp$")
    list(FILTER GOLD_UNDERSCORE_SOURCES EXCLUDE REGEX ".*/link_stubs_.*\\.asm$")
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
add_executable(RawrXD_Gold ${GOLD_UNDERSCORE_SOURCES} ${ASM_KERNEL_SOURCES})

if(WIN32)
    add_dependencies(RawrXD_Gold regen-ssot-beacons)
endif()

target_link_libraries(RawrXD_Gold PRIVATE RawrXD-RE-Library)

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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/prometheus
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
    if(NOT MSVC)
        target_link_libraries(RawrXD_Gold PRIVATE gomp)
    endif()
endif()

# Gold-only sources not present in the main SOURCES aggregation lane.
target_sources(RawrXD_Gold PRIVATE
    src/autonomous_model_manager.cpp
    src/core/native_debugger_engine.cpp
    src/core/native_debugger_symbols.cpp
    src/agentic/AgentOllamaClient.cpp
    src/sovereign/fips_compliance.cpp
    src/sovereign/hsm_integration.cpp
    src/sovereign/policy_engine.cpp
    src/sovereign/airgap_deployer.cpp
    src/sovereign/classified_network.cpp
    src/sovereign/RawrXD_SovereignStatusPanel.cpp
    src/win32app/TitanIPC.cpp
    src/runtime/SovereignWebSearch.cpp
    src/runtime/SovereignSnapshot.cpp
    src/runtime/SovereignCRDTSync.cpp
    src/runtime/SovereignFuzzEngine.cpp
    src/runtime/SovereignReplication.cpp
    src/runtime/RawrXDVectorIndex.cpp
    src/runtime/SovereignMeshProvider.cpp
    src/runtime/SovereignMCPBridge.cpp
    src/agentic/ToolRegistry_HardwareSecurity_Batch1.cpp
    src/agentic/RawrXD_ToolRegistry.cpp
    src/debug/ai_debugger.cpp
    src/collab/CollabToolHandlers.cpp
    src/modules/game_engine_manager.cpp
    src/modules/unity_engine_integration.cpp
    src/modules/unreal_engine_integration.cpp
    src/core/feature_handlers_model_load.cpp
    src/core/rawr_engine_link_closure.cpp
    src/core/vram_probe.cpp
    src/core/gold_sentinel_hash_stub.cpp
    src/core/gold_link_closure_v2.cpp
    src/core/enterprise_devunlock_bridge.cpp
    src/core/cot_fallback_system_impl.cpp
    src/core/dml_asm_impl.cpp
    src/ide_engine.cpp
    src/agentic_bridge.cpp
    src/text_buffer.cpp
    src/ai/fast_spec.cpp
    src/ai/fast_spec_inference_bridge.cpp
    src/native_agent.cpp
    src/mapped_window_streamer.cpp
    src/win32app/TodoManager.cpp
    src/ollama_client.cpp
    src/ExecutionScheduler_v2.cpp
    src/agentic/OllamaProvider.cpp
    src/gpu_enforcement.cpp
    src/ai/inference_retry_shim.cpp
    src/agentic/observability/Telemetry.cpp
    src/logging/Logger.cpp
    src/security/InputSanitizer.cpp
    src/core/ConfigurationValidator.cpp
    src/core/context_config.cpp
    src/inference/PerformanceMonitor.cpp
    src/agentic/ErrorRecoveryManager.cpp
    src/agentic/observability/Telemetry.cpp
    src/agentic/hotpatch/Sentinel.cpp
    src/win32app/TitanIPC.cpp
    # Link closure for unified/ssot lanes: implementations live outside main SOURCES or were stripped.
    tests/gguf_link_stubs.cpp
    src/core/rawrengine_command_handlers.cpp
    src/agent/quantum_dynamic_time_manager_impl.cpp
    src/core/native_gguf_loader_link_stub.cpp
    src/agentic/AdvancedAgentCoordinator.cpp
    src/BackendOrchestrator.cpp
    src/core/gold_inference_profiler_minimal.cpp
    src/agentic/hotpatch/Engine.cpp
    src/core/rawrxd_native_log_fallback.cpp
    src/core/gold_enterprise_devunlock_stub.cpp
    src/core/gold_beacon_handlers.cpp
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

    # FlashAttention AVX-512 kernel (provides FlashAttention_Init/Forward/GetTileConfig to Win32IDE)
    set(ASM_FLASHATTN_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/FlashAttention_AVX512.asm")
    set(ASM_FLASHATTN_OBJ "${CMAKE_CURRENT_BINARY_DIR}/FlashAttention_AVX512.obj")
    add_custom_command(
        OUTPUT ${ASM_FLASHATTN_OBJ}
        COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /I "${CMAKE_CURRENT_SOURCE_DIR}/src/asm" /Fo${ASM_FLASHATTN_OBJ} ${ASM_FLASHATTN_SRC}
        DEPENDS ${ASM_FLASHATTN_SRC}
        COMMENT "[MASM] Assembling FlashAttention_AVX512.asm (AVX-512 tiled attention kernel)"
        VERBATIM
    )

    # Quantization AVX2 kernel (provides Quant_DequantQ4_0/Q8_0 to Win32IDE)
    set(ASM_QUANT_AVX2_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/quant_avx2.asm")
    set(ASM_QUANT_AVX2_OBJ "${CMAKE_CURRENT_BINARY_DIR}/quant_avx2.obj")
    add_custom_command(
        OUTPUT ${ASM_QUANT_AVX2_OBJ}
        COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /I "${CMAKE_CURRENT_SOURCE_DIR}/src/asm" /Fo${ASM_QUANT_AVX2_OBJ} ${ASM_QUANT_AVX2_SRC}
        DEPENDS ${ASM_QUANT_AVX2_SRC}
        COMMENT "[MASM] Assembling quant_avx2.asm (Q4_0/Q8_0 dequantization, AVX2)"
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
        ${ASM_FLASHATTN_OBJ}
        ${ASM_QUANT_AVX2_OBJ}
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
find_package(Vulkan QUIET)
if(Vulkan_FOUND)
    message(STATUS "[Vulkan] Found SDK: ${Vulkan_INCLUDE_DIRS}")
    target_include_directories(RawrEngine PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(RawrEngine PRIVATE Vulkan::Vulkan)
    target_compile_definitions(RawrEngine PRIVATE RAWR_HAS_VULKAN=1 RAWR_ENABLE_VULKAN=1)
    if(TARGET InferenceEngine)
        target_include_directories(InferenceEngine PRIVATE ${Vulkan_INCLUDE_DIRS})
        target_link_libraries(InferenceEngine PUBLIC Vulkan::Vulkan)
        target_compile_definitions(InferenceEngine PRIVATE RAWR_HAS_VULKAN=1 RAWR_ENABLE_VULKAN=1)
    endif()
    if(TARGET RawrXD-InferenceEngine)
        target_include_directories(RawrXD-InferenceEngine PRIVATE ${Vulkan_INCLUDE_DIRS})
        target_link_libraries(RawrXD-InferenceEngine PRIVATE Vulkan::Vulkan)
        target_compile_definitions(RawrXD-InferenceEngine PRIVATE RAWR_HAS_VULKAN=1 RAWR_ENABLE_VULKAN=1)
    endif()
    # Gold target
    target_include_directories(RawrXD_Gold PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(RawrXD_Gold PRIVATE Vulkan::Vulkan)
    target_compile_definitions(RawrXD_Gold PRIVATE RAWR_HAS_VULKAN=1 RAWR_ENABLE_VULKAN=1)
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
        target_compile_definitions(RawrEngine PRIVATE RAWR_HAS_VULKAN=1 RAWR_ENABLE_VULKAN=1)
        if(TARGET InferenceEngine)
            target_include_directories(InferenceEngine PRIVATE "${_VK_SDK_DIR}/Include")
            target_link_directories(InferenceEngine PRIVATE "${_VK_SDK_DIR}/Lib")
            target_link_libraries(InferenceEngine PUBLIC vulkan-1)
            target_compile_definitions(InferenceEngine PRIVATE RAWR_HAS_VULKAN=1 RAWR_ENABLE_VULKAN=1)
        endif()
        if(TARGET RawrXD-InferenceEngine)
            target_include_directories(RawrXD-InferenceEngine PRIVATE "${_VK_SDK_DIR}/Include")
            target_link_directories(RawrXD-InferenceEngine PRIVATE "${_VK_SDK_DIR}/Lib")
            target_link_libraries(RawrXD-InferenceEngine PRIVATE vulkan-1)
            target_compile_definitions(RawrXD-InferenceEngine PRIVATE RAWR_HAS_VULKAN=1 RAWR_ENABLE_VULKAN=1)
        endif()
        # Gold target
        target_include_directories(RawrXD_Gold PRIVATE "${_VK_SDK_DIR}/Include")
        target_link_directories(RawrXD_Gold PRIVATE "${_VK_SDK_DIR}/Lib")
        target_link_libraries(RawrXD_Gold PRIVATE vulkan-1)
        target_compile_definitions(RawrXD_Gold PRIVATE RAWR_HAS_VULKAN=1 RAWR_ENABLE_VULKAN=1)
    else()
        message(STATUS "[Vulkan] SDK not found — GPU compute uses CPU fallback")
        target_compile_definitions(RawrEngine PRIVATE RAWR_HAS_VULKAN=0)
        if(TARGET InferenceEngine)
            target_compile_definitions(InferenceEngine PRIVATE RAWR_HAS_VULKAN=0)
        endif()
        if(TARGET RawrXD-InferenceEngine)
            target_compile_definitions(RawrXD-InferenceEngine PRIVATE RAWR_HAS_VULKAN=0)
        endif()
        target_compile_definitions(RawrXD_Gold PRIVATE RAWR_HAS_VULKAN=0)
    endif()
endif()

# RawrCodex.asm uses ADDR32 relocations — require /LARGEADDRESSAWARE:NO
if(MSVC)
    target_link_options(RawrEngine PRIVATE /LARGEADDRESSAWARE:NO)
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
    src/gpu/speculative_decoder_v2.cpp
    src/gpu/speculative_inference_bridge.cpp
    src/inference/polymorphic_loader.cpp
    src/inference/ollama_blob_parser.cpp
    src/cpu_inference_engine.cpp
    src/dispatch_orchestrator.cpp
    src/rawrxd_model_loader.cpp
    src/rawrxd_transformer.cpp
    src/rawrxd_tokenizer.cpp
    src/rawrxd_sampler.cpp
    src/gguf_vocab_resolver.cpp
    src/streaming_gguf_loader.cpp
    src/compression_interface.cpp
    src/brutal_implementation.cpp
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
    src/engine/cpu_engine.cpp
    src/engine/tokenless_loader.cpp
    src/core/layer_offload_manager.cpp
    src/core/engine_registry.cpp
    src/core/engine_revolver.cpp
    src/utils/Diagnostics.cpp
    src/inference/TitanLoaderDiagnostics.cpp
    src/inference/MemoryPressureGuard.cpp
    src/agentic/ErrorRecoveryManager.cpp
    src/inference/PerformanceMonitor.cpp
    src/inference/rawr_inference_autopatch_loop.cpp
    src/ai/fast_spec.cpp
    src/ai/fast_spec_inference_bridge.cpp
    src/gguf_loader.cpp
    # Standalone inference link closure (cpu_inference_engine + model loader + transformer + swarm)
    src/core/swarm_scheduler.cpp
    src/core/gguf_swarm_plan_builder.cpp
    src/logging/Logger.cpp
    src/gguf_factory/behavior_config_v2.cpp
    src/tokenizer/tokenizer_base.cpp
    src/tokenizer/bpe_tokenizer.cpp
    src/tokenizer/huggingface_tokenizer.cpp
    src/tokenizer/tiktoken_tokenizer.cpp
    src/tokenizer/wordpiece_tokenizer.cpp
    src/tokenizer/sentencepiece_tokenizer.cpp
    src/tokenizer/sentencepiece_protobuf.cpp
    src/tokenizer/tokenizer_factory.cpp
    src/loader/gguf_tensor_loader.cpp
    src/loader/tensor_filter.cpp
    src/weights/safetensors.cpp
    src/weights/weight_init.cpp
    src/weights/quantized_loaders.cpp
    src/codec/brutal_gzip.cpp
    src/codec/gzip_brutal_inflate.cpp
)
list(REMOVE_DUPLICATES INFERENCE_ENGINE_SOURCES)
if(NOT RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
    list(APPEND INFERENCE_ENGINE_SOURCES src/inference/inference_standalone_link_shims.cpp)
endif()

# Optional tooling targets
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/tools/tokenizer_roundtrip_test/CMakeLists.txt")
    add_subdirectory(src/tools/tokenizer_roundtrip_test)
endif()
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

add_executable(RawrXD-InferenceEngine EXCLUDE_FROM_ALL
    src/inference/inference_standalone_main.cpp
)
target_link_libraries(RawrXD-InferenceEngine PRIVATE InferenceEngine)

# ---------------------------------------------------------------------------
# Autocomplete engine – zero-dependency, single-file GGUF transformer daemon
# ---------------------------------------------------------------------------
add_executable(RawrXD-Autocomplete EXCLUDE_FROM_ALL
    src/autocomplete.cpp
)
set_target_properties(RawrXD-Autocomplete PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
target_compile_options(RawrXD-Autocomplete PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/O2 /arch:AVX2 /EHsc /W3>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-O3 -mavx2 -pthread -Wall>
)
if(NOT MSVC)
    target_link_libraries(RawrXD-Autocomplete PRIVATE pthread)
    if(UNIX AND NOT APPLE)
        target_link_libraries(RawrXD-Autocomplete PRIVATE rt)
    endif()
endif()
if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
    set_target_properties(RawrXD-InferenceEngine PROPERTIES
        EXCLUDE_FROM_ALL FALSE
        EXCLUDE_FROM_DEFAULT_BUILD FALSE
    )
endif()

# =============================================================================
# Titan ABI Boundary (MASM Control Plane <-> C++ Inference Plane)
# =============================================================================
if(EXISTS "${CMAKE_SOURCE_DIR}/src/titan/titan_abi.cpp")
    add_library(RawrXD_TitanABI STATIC
        src/titan/titan_abi.cpp
    )
    target_include_directories(RawrXD_TitanABI PUBLIC
        ${CMAKE_SOURCE_DIR}/src
    )
    if(MSVC)
        target_compile_options(RawrXD_TitanABI PRIVATE /arch:AVX2)
    endif()
endif()

add_executable(RawrXD-ComparativeBenchmark EXCLUDE_FROM_ALL
    src/tools/speculative_comparative_benchmark.cpp
    src/gpu/speculative_decoder_v2.cpp
)
target_include_directories(RawrXD-ComparativeBenchmark PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_definitions(RawrXD-ComparativeBenchmark PRIVATE
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
)

# =============================================================================
# Maximus TPS Streamer
# =============================================================================
add_library(MaximusStreamer STATIC
    src/maximus_streamer.cpp
)
target_include_directories(MaximusStreamer PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)
target_compile_features(MaximusStreamer PUBLIC cxx_std_20)
set_target_properties(MaximusStreamer PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

add_executable(RawrXD-MaximusDemo EXCLUDE_FROM_ALL
    examples/maximus_demo.cpp
)
target_link_libraries(RawrXD-MaximusDemo PRIVATE MaximusStreamer)
target_include_directories(RawrXD-MaximusDemo PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
set_target_properties(RawrXD-MaximusDemo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# =============================================================================
# Agentic Streamer Bridge
# =============================================================================
add_library(AgenticStreamerBridge STATIC
    src/agentic_streamer_bridge.cpp
    src/agentic/agentic_tool_executor.cpp
)
target_link_libraries(AgenticStreamerBridge PUBLIC
    MaximusStreamer
)
target_include_directories(AgenticStreamerBridge PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(AgenticStreamerBridge PUBLIC cxx_std_20)
set_target_properties(AgenticStreamerBridge PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

add_executable(RawrXD-AgenticBridgeDemo EXCLUDE_FROM_ALL
    examples/agentic_bridge_demo.cpp
)
target_link_libraries(RawrXD-AgenticBridgeDemo PRIVATE AgenticStreamerBridge)
target_include_directories(RawrXD-AgenticBridgeDemo PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
set_target_properties(RawrXD-AgenticBridgeDemo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

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
)

if(Vulkan_FOUND)
    target_include_directories(InferenceEngine PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(InferenceEngine PUBLIC Vulkan::Vulkan)
    target_compile_definitions(InferenceEngine PRIVATE RAWR_HAS_VULKAN=1 RAWR_ENABLE_VULKAN=1)
    target_include_directories(RawrXD-InferenceEngine PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(RawrXD-InferenceEngine PRIVATE Vulkan::Vulkan)
    target_compile_definitions(RawrXD-InferenceEngine PRIVATE RAWR_HAS_VULKAN=1 RAWR_ENABLE_VULKAN=1)
elseif(DEFINED _VK_SDK_DIR AND _VK_SDK_DIR)
    target_include_directories(InferenceEngine PRIVATE "${_VK_SDK_DIR}/Include")
    target_link_directories(InferenceEngine PRIVATE "${_VK_SDK_DIR}/Lib")
    target_link_libraries(InferenceEngine PUBLIC vulkan-1)
    target_compile_definitions(InferenceEngine PRIVATE RAWR_HAS_VULKAN=1 RAWR_ENABLE_VULKAN=1)
    target_include_directories(RawrXD-InferenceEngine PRIVATE "${_VK_SDK_DIR}/Include")
    target_link_directories(RawrXD-InferenceEngine PRIVATE "${_VK_SDK_DIR}/Lib")
    target_link_libraries(RawrXD-InferenceEngine PRIVATE vulkan-1)
    target_compile_definitions(RawrXD-InferenceEngine PRIVATE RAWR_HAS_VULKAN=1 RAWR_ENABLE_VULKAN=1)
else()
    target_compile_definitions(InferenceEngine PRIVATE RAWR_HAS_VULKAN=0)
    target_compile_definitions(RawrXD-InferenceEngine PRIVATE RAWR_HAS_VULKAN=0)
endif()

target_link_libraries(InferenceEngine PUBLIC Threads::Threads)

if(WIN32)
    target_link_libraries(InferenceEngine PUBLIC
        shlwapi psapi dbghelp winhttp bcrypt advapi32 crypt32
    )
endif()

    if(RAWR_HAS_MASM)
        # Link quantization + telemetry kernels into inference engine
        set(INFERENCE_ASM_SOURCES
            src/asm/rawr_globals.asm
            src/asm/rawr_cpu_features.asm
            src/asm/inference_core.asm
            src/asm/inference_kernels.asm
            src/asm/token_queue_dequeue.asm
            src/asm/RawrXD_SovereignTokenizerCore.asm
            src/asm/quant_avx2.asm
            src/asm/RawrXD_KQuant_Dequant.asm
            src/asm/RawrXD_KQuant_Kernel.asm
            src/asm/FlashAttention_AVX512.asm
            src/kernels/dequant_q6k_avx512.asm
            src/kernels/kv_accum_avx512.asm
            src/asm/RawrXD_Telemetry_Kernel.asm
            src/asm/model_streamer_x64.asm
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

# Companion DLL for RawrXD-Serve — same ABI as rawrxd_serve_inference_plugin.cpp expects.
if(MSVC AND TARGET InferenceEngine)
    add_library(RawrXD-ServeInference SHARED
        src/serve/RawrXD_ServeInference_dll.cpp
    )
    target_link_libraries(RawrXD-ServeInference PRIVATE InferenceEngine)
    target_compile_definitions(RawrXD-ServeInference PRIVATE
        RAWRXD_ServeInference_EXPORTS=1
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
        RAWRXD_STRICT_PRODUCTION_PROFILE=$<IF:$<BOOL:${RAWRXD_PRODUCTION_STRIP_STUB_SOURCES}>,1,0>
        RAWR_STANDALONE_INFERENCE=1
        RAWR_HAS_SOVEREIGN_ENGINES=1
        RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=$<IF:$<BOOL:${RAWRXD_PRODUCTION_STRIP_STUB_SOURCES}>,1,0>
        ${_RAWR_ASM_FEATURE_DEFINITIONS}
    )
    target_compile_options(RawrXD-ServeInference PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:/EHsc /W0 /arch:AVX512>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CONFIG:Debug>>>:/O2>
    )
    target_link_options(RawrXD-ServeInference PRIVATE /LARGEADDRESSAWARE:NO)
    set_target_properties(RawrXD-ServeInference PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME RawrXD_ServeInference
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )
    message(STATUS "[Serve] RawrXD-ServeInference DLL target added (CPUInferenceEngine plugin)")
endif()

if(WIN32)
    option(RAWRXD_INLINE_DISPATCH "Inline aperture dispatch hot path for benchmarking" OFF)
    option(RAWR_ENABLE_GGML_LINK "Enable the opt-in GGML bridge lane for RawrXD_Titan" OFF)
    set(RAWRXD_ENABLE_OMEGA_HIP "AUTO" CACHE STRING "Enable Omega HIP kernel lane for Titan (AUTO|ON|OFF)")
    set_property(CACHE RAWRXD_ENABLE_OMEGA_HIP PROPERTY STRINGS AUTO ON OFF)
    option(RAWRXD_ENABLE_OMEGA_HIP_FULL_KERNEL_SET "Enable full Omega HIP ASM pack (experimental/strictly opt-in)" OFF)

    set(_RAWRXD_OMEGA_HIP_ENABLED OFF)
    set(_RAWRXD_OMEGA_HIP_REASON "disabled by default")
    if(RAWRXD_ENABLE_OMEGA_HIP STREQUAL "ON")
        set(_RAWRXD_OMEGA_HIP_ENABLED ON)
        set(_RAWRXD_OMEGA_HIP_REASON "forced ON")
    elseif(RAWRXD_ENABLE_OMEGA_HIP STREQUAL "AUTO")
        execute_process(
            COMMAND powershell -NoProfile -Command "(Get-CimInstance Win32_VideoController | Select-Object -ExpandProperty Name) -join ';'"
            OUTPUT_VARIABLE _RAWRXD_GPU_NAMES
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(_RAWRXD_GPU_NAMES MATCHES "Radeon RX 7800 XT|gfx1101")
            set(_RAWRXD_OMEGA_HIP_ENABLED ON)
            set(_RAWRXD_OMEGA_HIP_REASON "AUTO matched RDNA3 target (${_RAWRXD_GPU_NAMES})")
        else()
            set(_RAWRXD_OMEGA_HIP_REASON "AUTO no RDNA3 match (${_RAWRXD_GPU_NAMES})")
        endif()
    else()
        set(_RAWRXD_OMEGA_HIP_REASON "forced OFF")
    endif()

    set(TITAN_ASM_KERNEL src/kernels/k_swap_aperture_win32_v126.asm)
    set(TITAN_AVX512_DISPATCH_KERNEL src/kernels/aperture_dispatch_avx512_win64.asm)
    set(TITAN_HEADER_VERIFY_KERNEL src/kernels/k_header_verify_fast.asm)
    set(TITAN_JSON_SIMD_KERNEL src/kernels/RawrXD_JSON_SIMD.asm)
set(TITAN_HTTP_CLIENT src/kernels/RawrXD_HttpClient.asm)
set(TITAN_NDJSON_PARSER src/kernels/RawrXD_NDJSON_AVX2.asm)
set(TITAN_SCHEDULER src/kernels/RawrXD_Scheduler.asm)
set(TITAN_OMEGA_SINGULARITY src/kernels/RawrXD_Omega_Singularity.asm)
set(TITAN_NTWRITE_LINK src/kernels/RawrXD_NtWrite_Link.asm)
set(TITAN_CIRCULAR_SDMA src/kernels/RawrXD_Circular_SDMA.asm)
set(TITAN_PROFILER_CORE src/kernels/RAWR_PROFILER_CORE.asm)
set(TITAN_QUERY_FUSION_KERNEL src/kernels/RawrXD_QueryFusion_AVX512.asm)
set(TITAN_QUERY_FUSION_CPP src/kernels/query_fusion_kernel.cpp)
set(VULKAN_TEXT_RENDERER src/render/RawrXD_VulkanText.cpp)
set(SCHEDULER_BRIDGE src/render/SchedulerBridge.cpp)
    option(RAWRXD_ENABLE_OMEGA_SINGULARITY_ASM "Include experimental RawrXD_Omega_Singularity.asm" OFF)
    set_source_files_properties(${TITAN_ASM_KERNEL} PROPERTIES LANGUAGE ASM_MASM)
    set_source_files_properties(${TITAN_AVX512_DISPATCH_KERNEL} PROPERTIES LANGUAGE ASM_MASM)
    set_source_files_properties(${TITAN_HEADER_VERIFY_KERNEL} PROPERTIES LANGUAGE ASM_MASM)
    set(_TITAN_EXTRA_ASM
        ${TITAN_JSON_SIMD_KERNEL}
        ${TITAN_HTTP_CLIENT}
        ${TITAN_NDJSON_PARSER}
        ${TITAN_SCHEDULER}
        ${TITAN_NTWRITE_LINK}
        ${TITAN_CIRCULAR_SDMA}
        ${TITAN_PROFILER_CORE}
        ${TITAN_QUERY_FUSION_KERNEL}
    )
    if(RAWRXD_ENABLE_OMEGA_SINGULARITY_ASM)
        list(APPEND _TITAN_EXTRA_ASM ${TITAN_OMEGA_SINGULARITY})
    endif()

    set(TITAN_GGML_BRIDGE_OBJECTS)
    if(RAWR_ENABLE_GGML_LINK)
        if(NOT TARGET ggml)
            if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/ggml/CMakeLists.txt")
                get_directory_property(_rawrxd_saved_includes INCLUDE_DIRECTORIES)
                set(_rawrxd_ggml_parent_includes)
                file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/include" _rawrxd_blocked_include_norm)
                string(TOLOWER "${_rawrxd_blocked_include_norm}" _rawrxd_blocked_include_norm)
                foreach(_rawrxd_inc IN LISTS _rawrxd_saved_includes)
                    file(TO_CMAKE_PATH "${_rawrxd_inc}" _rawrxd_inc_norm)
                    string(TOLOWER "${_rawrxd_inc_norm}" _rawrxd_inc_norm)
                    if(NOT _rawrxd_inc_norm STREQUAL _rawrxd_blocked_include_norm)
                        list(APPEND _rawrxd_ggml_parent_includes "${_rawrxd_inc}")
                    endif()
                endforeach()
                set_property(DIRECTORY PROPERTY INCLUDE_DIRECTORIES "${_rawrxd_ggml_parent_includes}")
                add_subdirectory(3rdparty/ggml ${CMAKE_BINARY_DIR}/rawrxd_ggml EXCLUDE_FROM_ALL)
                set_property(DIRECTORY PROPERTY INCLUDE_DIRECTORIES "${_rawrxd_saved_includes}")
                message(STATUS "[Titan] Loaded ggml target from 3rdparty/ggml for GGML bridge lane")

                # Keep ggml isolated from RawrXD's legacy public include lane to avoid
                # picking mismatched ggml headers during ggml subproject compilation.
                if(TARGET ggml-base)
                    get_target_property(_rawrxd_ggml_base_includes ggml-base INCLUDE_DIRECTORIES)
                    if(_rawrxd_ggml_base_includes)
                        set(_rawrxd_ggml_base_filtered_includes)
                        foreach(_rawrxd_inc IN LISTS _rawrxd_ggml_base_includes)
                            file(TO_CMAKE_PATH "${_rawrxd_inc}" _rawrxd_inc_norm)
                            string(TOLOWER "${_rawrxd_inc_norm}" _rawrxd_inc_norm)
                            if(NOT _rawrxd_inc_norm STREQUAL _rawrxd_blocked_include_norm)
                                list(APPEND _rawrxd_ggml_base_filtered_includes "${_rawrxd_inc}")
                            endif()
                        endforeach()
                        set_target_properties(ggml-base PROPERTIES INCLUDE_DIRECTORIES "${_rawrxd_ggml_base_filtered_includes}")
                    endif()
                endif()
            endif()
        endif()

        if(NOT TARGET ggml)
            message(FATAL_ERROR "RAWR_ENABLE_GGML_LINK=ON requires the ggml target to be available before RawrXD_Titan is configured")
        endif()

        message(STATUS "[Titan] GGML bridge lane enabled (live handshake owned by titan_infer_dll.cpp)")
    else()
        message(STATUS "[Titan] GGML bridge scaffold disabled (set -DRAWR_ENABLE_GGML_LINK=ON to enable opt-in lane)")
    endif()

    set_source_files_properties(${_TITAN_EXTRA_ASM} PROPERTIES LANGUAGE ASM_MASM)

    add_library(RawrXD_Titan SHARED EXCLUDE_FROM_ALL
        src/titan/RawrXD_Titan.cpp
    )

    # Combined export table link
    if(MSVC)
        target_link_options(RawrXD_Titan PRIVATE
            "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/src/titan_infer.def"
        )
    endif()

    if(_RAWRXD_OMEGA_HIP_ENABLED)
        # Disabled for Ollama HTTP proxy build - no ASM dependencies
        # target_sources(RawrXD_Titan PRIVATE ${TITAN_OMEGA_SINGULARITY})
        target_compile_definitions(RawrXD_Titan PRIVATE RAWRXD_ENABLE_OMEGA_HIP=1)

        if(RAWRXD_ENABLE_OMEGA_HIP_FULL_KERNEL_SET)
            target_sources(RawrXD_Titan PRIVATE
                ${TITAN_HTTP_CLIENT}
                ${TITAN_NDJSON_PARSER}
                ${TITAN_SCHEDULER}
                ${TITAN_CIRCULAR_SDMA}
            )
            message(STATUS "[Titan] Omega HIP full kernel set enabled (experimental)")
        else()
            message(STATUS "[Titan] Omega HIP full kernel set disabled (set -DRAWRXD_ENABLE_OMEGA_HIP_FULL_KERNEL_SET=ON to opt in)")
        endif()
    else()
        target_compile_definitions(RawrXD_Titan PRIVATE RAWRXD_ENABLE_OMEGA_HIP=0)
    endif()

    if(RAWRXD_ENABLE_OMEGA_SINGULARITY_ASM)
        target_sources(RawrXD_Titan PRIVATE ${TITAN_OMEGA_SINGULARITY})
    endif()

    if(TARGET rawrxd-sovereign-kernel-spv)
        add_dependencies(RawrXD_Titan rawrxd-sovereign-kernel-spv)
    endif()

    target_include_directories(RawrXD_Titan PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    if(RAWR_ENABLE_GGML_LINK)
        target_include_directories(RawrXD_Titan BEFORE PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/ggml/include
        )
    endif()

    # Titan DLL source is now src/titan/RawrXD_Titan.cpp (thin wrapper around llama.cpp)
    # Old src/titan_infer_dll.cpp removed to avoid DllMain conflict

    set_target_properties(RawrXD_Titan PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY bin
        LIBRARY_OUTPUT_DIRECTORY bin
        ARCHIVE_OUTPUT_DIRECTORY bin
        OUTPUT_NAME RawrXD_Titan
    )

    # Remove the second (redundant and conflicting) DEF link option
    # if(MSVC)
    #    target_link_options(RawrXD_Titan PRIVATE
    #        "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/src/titan_infer.def"
    #    )
    # endif()

    target_link_libraries(RawrXD_Titan PRIVATE
        user32
        kernel32
        "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.22621.0/um/x64/OneCore.Lib"
    )

    if(RAWR_ENABLE_GGML_LINK)
        target_link_libraries(RawrXD_Titan PRIVATE ggml)
        target_compile_definitions(RawrXD_Titan PRIVATE RAWR_ENABLE_GGML_LINK=1 RAWRXD_TITAN_BRIDGE_EXPORTS=1)
    else()
        target_compile_definitions(RawrXD_Titan PRIVATE RAWR_ENABLE_GGML_LINK=0 RAWRXD_TITAN_BRIDGE_EXPORTS=1)
    endif()

    if(RAWRXD_INLINE_DISPATCH)
        target_compile_definitions(RawrXD_Titan PRIVATE RAWRXD_INLINE_DISPATCH=1)
        message(STATUS "[Titan] RAWRXD_INLINE_DISPATCH=1 (Phase 7A inline dispatcher enabled)")
    else()
        target_compile_definitions(RawrXD_Titan PRIVATE RAWRXD_INLINE_DISPATCH=0)
        message(STATUS "[Titan] RAWRXD_INLINE_DISPATCH=0 (baseline dispatcher)")
    endif()

    message(STATUS "[Titan] RAWRXD_ENABLE_OMEGA_HIP=${RAWRXD_ENABLE_OMEGA_HIP} -> ${_RAWRXD_OMEGA_HIP_ENABLED} (${_RAWRXD_OMEGA_HIP_REASON})")

    message(STATUS "[Titan] RawrXD_Titan DLL target wired: 99-export lane + aperture kernels (swap + AVX-512 dispatch + header verify)")

    # Standalone TitanHost worker process for isolated runtime rebuilds.
    add_executable(RawrXD-TitanHost EXCLUDE_FROM_ALL
        src/win32app/TitanHost.cpp
    )
    set_target_properties(RawrXD-TitanHost PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY bin
        OUTPUT_NAME RawrXD-TitanHost
    )
    target_link_libraries(RawrXD-TitanHost PRIVATE
        kernel32
        shell32
    )
    add_dependencies(RawrXD-TitanHost RawrXD_Titan)
endif()

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
if(WIN32)
    set(WIN32IDE_SOURCES
        src/win32app/main_win32.cpp
        src/win32app/startup_sentinels.cpp
        src/win32app/HeadlessIDE.cpp
        src/win32app/HeadlessIDE_AutonomousWorkflowMode.cpp
        src/api_server.cpp
        src/overclock_governor.cpp
        src/RawrXD_ApertureManager.cpp
        src/agent/quantum_agent_orchestrator.cpp
        src/ui/RawrXD_MainLoop.cpp
        src/ui/webview2_bridge.cpp
        src/ui/debugger_core.cpp
        src/ui/agentic_bridge.cpp
        src/win32app/RawrXD-Win32IDE.rc
        src/bridge/Win32SwarmBridge.cpp
        src/bridge/SwarmIATRegistration.cpp
        src/bridge/UnifiedModelMetadata.cpp
        src/agentic/SubAgentManager.cpp
        src/agentic/RobustOllamaParser.cpp
        src/agentic/ErrorRecoveryManager.cpp
    )

    # MASM64 UI Dispatcher (MSVC/MASM-only).
    if(RAWR_HAS_MASM)
        set(WEBVIEW2_DISPATCHER src/asm/webview2_masm64_dispatcher.asm)
        set_source_files_properties(${WEBVIEW2_DISPATCHER} PROPERTIES LANGUAGE ASM_MASM)
        list(APPEND WIN32IDE_SOURCES ${WEBVIEW2_DISPATCHER})

        option(RAWRXD_ENABLE_SOVMEM_MASM "Enable sovereign memory mapper MASM sync/TLS primitives" ON)
        if(RAWRXD_ENABLE_SOVMEM_MASM)
            set(SOVMEM_MASM_SOURCE src/compression/sovereign_memory_mapper.asm)
            set_source_files_properties(${SOVMEM_MASM_SOURCE} PROPERTIES LANGUAGE ASM_MASM)
            list(APPEND WIN32IDE_SOURCES ${SOVMEM_MASM_SOURCE})
        endif()

        option(RAWRXD_ENABLE_EXPERTGATING_MASM "Enable expert gating & MEM_COMMIT MASM logic" ON)
        if(RAWRXD_ENABLE_EXPERTGATING_MASM)
            set(EXPERTGATING_MASM_SOURCE src/compression/sovereign_expert_gating.asm)
            set_source_files_properties(${EXPERTGATING_MASM_SOURCE} PROPERTIES LANGUAGE ASM_MASM)
            list(APPEND WIN32IDE_SOURCES ${EXPERTGATING_MASM_SOURCE})
        endif()

        option(RAWRXD_ENABLE_IQ2M_MASM "Enable IQ2_M 2.5-bit dequantization AVX2/AVX-512 kernel" ON)
        if(RAWRXD_ENABLE_IQ2M_MASM)
            set(IQ2M_MASM_SOURCE src/compression/iq2m_dequant_kernel.asm)
            set_source_files_properties(${IQ2M_MASM_SOURCE} PROPERTIES LANGUAGE ASM_MASM)
            list(APPEND WIN32IDE_SOURCES ${IQ2M_MASM_SOURCE})
        endif()
    endif()

    # NOTE:
    # Win32IDE target creation/linking is performed in the canonical block below
    # (single add_executable + full source/link configuration).

    list(APPEND WIN32IDE_SOURCES
        # Core Win32IDE implementation units (required for real GUI command/menu behavior)
        src/win32app/Win32IDE.cpp
        src/win32app/Win32IDE_Core.cpp
        src/win32app/Win32IDE_Accelerators.cpp
        src/win32app/Win32IDE_CoreRuntimeSpine.cpp
        src/win32app/Win32IDE_Commands.cpp
        src/win32app/Win32IDE_ModelLab.cpp
        src/win32app/Win32IDE_VSCodeUI.cpp
        src/win32app/Win32IDE_CopilotNativeGlue.cpp
            src/win32app/WindowManager.cpp
            src/win32app/EditorOperations.cpp
            src/win32app/RouterOperations.cpp
                    src/win32app/Win32IDE_Watchdog.cpp
        src/win32app/Win32IDE_SyntaxHighlight.cpp
        src/win32app/Win32IDE_VisionEncoder.cpp
        src/win32app/Win32IDE_Themes.cpp
        src/win32app/Win32IDE_StreamingUX.cpp
        src/win32app/Win32IDE_Sidebar.cpp
        src/win32app/Win32IDE_SidebarRuntime.cpp
        src/gguf_factory/gguf_writer_minimal.cpp
        src/win32app/Win32IDE_Debugger.cpp
        src/win32app/Win32IDE_AutonomousDebugger.cpp
        src/win32app/Win32IDE_Annotations.cpp
        src/win32app/Win32IDE_ReverseEngineering.cpp
        src/win32app/Win32IDE_Logger.cpp
        src/win32app/RawrXD_Omega_LinkGlobals.cpp
        src/win32app/Win32IDE_AgenticBridge.cpp
        src/full_agentic_ide/FullAgenticIDE.cpp
        src/full_agentic_ide/AgenticPlanningOrchestrator.cpp
        src/full_agentic_ide/AgenticIOCPBridge.cpp
        src/win32app/Win32IDE_AgentCommands.cpp
        src/win32app/Win32IDE_HexMag.cpp
        src/win32app/Win32IDE_ChatAutocomplete.cpp
        src/win32app/Win32IDE_CodeCompletion.cpp
        src/win32app/Win32IDE_ChatCommands.cpp
        src/win32app/Win32IDE_ComposerPanel.cpp
        src/win32app/Win32IDE_TabManager.cpp
        src/ui/advanced_docking_system.cpp
        src/kv_cache/kv_cache_fp8_quantizer.cpp
        src/inference/token_pipeline_double_buffer.cpp
        src/speculative/speculative_fused_verify.cpp
        src/kernels/fp8_avx2_interface.cpp
        src/win32app/Win32IDE_Settings.cpp
        src/win32app/Win32IDE_AutonomousCommunicator.cpp
        src/win32app/Win32IDE_AgenticIntegration_stub.cpp
        src/win32ide/ExecModeToolbar.cpp
        src/win32ide/GhostOverlay.cpp
        src/agentic/ExecPipeline.cpp
        src/agentic/PatchEngine.cpp
        src/win32app/Win32IDE_AgenticComposerUX.cpp
        src/win32app/Win32IDE_ContextMentionParser.cpp
        src/win32app/Win32IDE_UnifiedTelemetry.cpp
        src/win32app/Win32IDE_Autonomy.cpp
        src/win32app/Win32IDE_PowerShell.cpp
        src/win32app/Win32IDE_PowerShellPanel.cpp
        src/win32app/SessionController.cpp
        src/win32app/Win32IDE_Session.cpp
        src/engine/trace_causal_drain.cpp
        src/win32app/Win32IDE_ChatHistoryPersistence.cpp
        src/agent/project_scoped_chat.cpp
        src/win32app/Win32IDE_FileOps.cpp
        src/win32app/Win32IDE_DecompilerView.cpp
        src/win32app/Win32IDE_NativePipeline.cpp
        src/win32app/Win32IDE_ModelAnatomy.cpp
        Ship/RawrXD_AutonomousAgenticPipeline.cpp
        Ship/RawrXD_AgentCoordinator.cpp

        src/win32app/Win32IDE_FeatureManifest.cpp
        src/win32app/Win32IDE_GhostText.cpp
        src/win32app/Win32IDE_SlashCommands.cpp
        src/win32app/Win32IDE_MirrorGate.cpp
        src/win32app/Win32IDE_ProjectRagLite.cpp
        src/win32app/ghost_completion_parse.cpp
        src/win32app/Win32IDE_Titan_Heartbeat.cpp
        src/win32app/Win32IDE_OutlinePanel.cpp
        src/win32app/Win32IDE_GoToLine.cpp
        src/win32app/Win32IDE_GoToSymbol.cpp
        src/win32app/Win32IDE_StatusBarActions.cpp
        src/win32app/Win32IDE_QuickOpen.cpp
        src/win32app/Win32IDE_InferenceMetrics.cpp
        src/win32app/Win32IDE_RenamePreview.cpp
        src/win32app/Win32IDE_Tier2Cosmetics.cpp
        src/win32app/Win32IDE_Tier1Cosmetics.cpp
        src/win32app/Win32IDE_Breadcrumbs.cpp
        src/win32app/RawrXD_SymbolEngine.cpp
        src/win32app/Win32IDE_PlanExecutor.cpp
        src/agentic/agentic_planning_orchestrator.cpp
        src/agentic/failure_intelligence_orchestrator.cpp
        src/agentic/agentic_orchestrator_integration.cpp
        src/agentic/agentic_tool_executor_bridge.cpp
        src/agentic/agentic_audit_sink.cpp
        src/agentic/change_impact_analyzer.cpp
        src/agentic/agent_controller_minimal.cpp
        src/agentic/agentic_controller_wiring.cpp
        src/core/link_symbols_impl.cpp
        src/core/rust_parser_v2.cpp
        src/core/symbol_table.cpp
        # Unified local-inference pipeline (CLI ground-truth path shared with Win32IDE)
        src/core/rawr_inference_pipeline.cpp
        src/serve/rawrxd_serve_inference_plugin.cpp
        src/autonomous_model_manager.cpp
        # ExtensionHost minimal link stub for Win32IDE_Commands.cpp callsites
        src/win32app/ExtensionHost_stub.cpp
        src/bridge/symbol_index_bridge.cpp
        src/core/ast_graph_engine_impl.cpp
        src/core/kv_cache_credit_bridge.cpp
        src/flow_control/credit_governor.cpp
        src/flow_control/credit_based_flow_control.cpp
        src/flow_control/phase_latency_tracker.cpp
        src/backend/agentic_tools.cpp
        src/backend/ollama_client.cpp
        src/tools/file_ops.cpp
        src/tools/git_client.cpp

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
        src/win32app/Win32IDE_AgentOllamaClient.cpp
        src/win32app/Win32IDE_ModelDiscovery.cpp
        src/win32app/Win32IDE_ExpertHeatmapPanel.cpp
        src/win32app/Win32IDE_SwarmModelSelector.cpp
        src/win32app/Win32IDE_AsmSemantic.cpp
        src/win32app/Win32IDE_LSP_AI_Bridge.cpp
        src/win32app/Win32IDE_MultiResponse.cpp
        src/win32app/Win32IDE_ExecutionGovernor.cpp
        src/win32app/Win32IDE_LayerEviction.cpp
        src/win32app/Win32IDE_SemanticIndexShim.cpp
        src/win32app/Win32IDE_SubAgent.cpp
        src/win32app/Win32IDE_AgentHistory.cpp
        src/win32app/Win32IDE_AgentPanel.cpp
        src/win32app/Win32IDE_DiskRecovery.cpp
        src/win32app/Win32IDE_Mnemosyne.cpp
        src/win32app/Win32IDE_Phase1CoreUXBridge.cpp
    src/win32app/Win32IDE_FabricBridge.cpp
        src/win32app/Win32IDE_SettingsEvents.cpp
        src/win32app/Win32TerminalManager.cpp
        src/win32app/TransparentRenderer.cpp

        # Context Fusion Layer — Phase 45: Unified semantic state machine
        src/core/ContextFusionEngine.cpp
        src/core/ContextFusionWiring.cpp
        src/win32app/GhostTextContextSubscriber.cpp
        src/win32app/PrefixCache.cpp
        src/win32app/ChatPanelModelCaller.cpp

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
        src/bridge_layer.cpp
        src/titan/titan_abi.cpp
        src/bridge_on_suggestion_ready_stub.cpp

        # Bounded Agent Loop + FIM Provider (Agent→IDE Pivot)
        src/agentic/AgentToolHandlers.cpp
        src/agentic/AgenticChatSession.cpp
        src/agentic/BoundedAgentLoop.cpp
        src/agentic/OllamaProvider.cpp
        src/interactive_shell.cpp
        src/agentic/DiffEngine.cpp
        src/agentic/slash_command_parser.cpp
        src/agentic/multi_file_edit_plan.cpp
        src/core/scoped_instructions_provider.cpp
        src/indexing/incremental_indexer.cpp

        # Runtime vector index used by semantic bridge / ghost text
        src/runtime/RawrXDVectorIndex.cpp
        src/runtime/RuntimeProvider.cpp
        src/standalone_llama_runtime.cpp

        # T3-D: Deterministic Replay Engine (DivergenceEvent + TelemetrySnapshot)
        src/agentic/DeterministicReplayEngine.cpp
        src/engine/telemetry_replay_tool.cpp

        # T4: Autonomous Recovery Orchestrator (FailureClass → recovery → verify → commit)
        src/agentic/autonomous_recovery_orchestrator.cpp

        # T5: Agent Orchestrator + Bridge + Navigation (agentic brain wiring)
        src/agentic/AgentOrchestrator.cpp
        src/agentic/OrchestratorBridge.cpp
        src/agentic/AgenticSubmitInference_Fix.cpp
        src/agentic/AgenticNavigator.cpp
        src/agentic/RawrXD_ToolRegistry.cpp
        src/agentic/ToolRegistry.cpp
        src/agentic/ToolRegistry_HardwareSecurity_Batch1.cpp
        src/agentic/SovereignAssembler.cpp
        src/agentic/RawrXD_AgentLoop.cpp
        src/agentic/agentic_executor.cpp
        src/agentic/FIMPromptBuilder.cpp
        src/agentic/react_tool_loop.cpp
        src/ai/inference_retry_shim.cpp
        src/core/safe_inference_session.cpp
        src/ai/embedding_provider.cpp
        src/core/ai_execution_guardrails.cpp
        
        # SDMA Phase 1: Coordinator (burst scheduling bridge/thread management)
        src/core/sdma/sdma_coordinator.cpp
        
        src/agentic_observability.cpp
        src/core/rawrxd_subsystem_api.cpp
        src/core/rawrxd_subsys_modes_a.cpp
        src/core/rawrxd_subsys_modes_b.cpp
        src/core/rawrxd_subsys_modes_c.cpp
        src/core/rawrxd_subsys_modes_d.cpp
        src/core/context_config.cpp
        src/core/vram_probe.cpp
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
        src/ai/fast_spec.cpp
        src/ai/fast_spec_inference_bridge.cpp
        src/mapped_window_streamer.cpp
        src/dispatch_orchestrator.cpp
        src/core/swarm_scheduler.cpp
        src/core/gguf_swarm_plan_builder.cpp
        src/memory_core.cpp
        src/hot_patcher.cpp
        src/streaming_gguf_loader.cpp
        src/utils/Diagnostics.cpp
        src/diagnostics/self_diagnose.cpp
        src/gguf_loader.cpp
        src/gguf_vocab_resolver.cpp
        src/model_source_resolver.cpp
        src/compression_interface.cpp
        src/engine/gguf_core.cpp
        src/engine/inference_kernels.cpp
        src/engine/transformer.cpp
        src/engine/bpe_tokenizer.cpp
        src/engine/sampler.cpp
        src/engine/grammar_constrained_sampler.cpp
        src/engine/k_quant_kernels.cpp
        src/engine/streaming_detokenizer.cpp
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
        src/core/hotpatch_wiring.cpp
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
        src/debug/ai_debugger.cpp
        src/win32app/Win32IDE_NativeDebugPanel.cpp
        
        # Three-Layer Hotpatch System (Phase 14.2)
        src/core/model_memory_hotpatch.cpp
        src/core/byte_level_hotpatcher.cpp
        src/core/unified_hotpatch_manager.cpp
        src/core/proxy_hotpatcher.cpp
        src/core/model_bruteforce_engine.cpp
        src/server/gguf_server_hotpatch.cpp
        src/server/token_router.cpp
        src/core/pt_driver_contract.cpp
        src/core/shadow_page_detour.cpp
        src/core/sentinel_watchdog.cpp
        src/win32app/Win32IDE_HotpatchPanel.cpp
        
        # Agent-Driven Hotpatch Orchestration (Phase 18)
        src/agent/agentic_hotpatch_orchestrator.cpp

        # HTTP Client — WinHTTP async with future retention (CLI+GUI)
        src/agent/llm_http_client.cpp
        src/agent/hexmag_client.cpp
        # src/agent/model_invoker.cpp  # Test file - excluded from build
        src/agent/planner.cpp
        src/agent/meta_planner.cpp
        src/agent/eval_framework.cpp
        src/agent/autonomous_subagent.cpp
        src/agent/agentic_failure_detector.cpp
        src/agent/agentic_deep_thinking_engine.cpp
        src/agent/quantum_autonomous_todo_system.cpp
        src/agent/quantum_multi_model_agent_cycling.cpp
        src/agent/quantum_dynamic_time_manager.cpp
        src/agent/quantum_missing_impl.cpp
        src/agent/quantum_production_orchestrator.cpp
        src/core/ai_agent_masm_stubs.cpp
        src/agent/agentic_puppeteer.cpp
        src/cloud_api_client.cpp

        # LocalReasoningEngine — API-free static analysis + pattern detection
        src/agent/local_reasoning_engine.cpp

        # Agent Self-Repair — MASM64 self-patching kernel bridge
        src/agent/agent_self_repair.cpp
        src/agent/agent_self_healing_orchestrator.cpp
        
        # Module implementations
        src/modules/vsix_loader_win32.cpp
        src/modules/ExtensionInstaller.cpp
        src/modules/codex_ultimate.cpp

        # Phase 29: VS Code Extension API Compatibility Layer
        src/modules/vscode_extension_api.cpp
        src/win32app/Win32IDE_VSCodeExtAPI.cpp
        
        # Phase 21: Distributed Swarm Bridge + Universal Model Hotpatcher
        src/core/swarm_decision_bridge.cpp
        src/core/universal_model_hotpatcher.cpp
        src/core/request_batch_scheduler.cpp
        src/core/model_reload_controller.cpp
        src/core/kv_cache_prefix_trie.cpp
        src/core/layer_latency_profiler.cpp
        src/core/inference_cancellation.cpp
        src/core/sparse_expert_scheduler.cpp
        src/core/continuous_batch_scheduler.cpp
        src/core/lora_hotswap_manager.cpp
        src/core/gguf_tensor_parallel_loader.cpp
        
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
        
        # Phase 25: AMD/ATI GPU Acceleration Toggle
        src/core/amd_gpu_accelerator.cpp
        src/core/gpu_warmup.cpp
        src/core/unified_memory_executor.cpp
        src/core/model_runtime_gate.cpp
        
        # Phase 29: Multi-Architecture GPU/Accelerator Backends
        src/core/intel_gpu_accelerator.cpp
        src/core/arm64_gpu_accelerator.cpp
        src/core/cerebras_wse_accelerator.cpp
        
        # Phase 30: Unified Accelerator Router
        src/core/accelerator_router.cpp
        
        # Phase 31: NVIDIA CUDA Backend (Driver API)
        src/core/nvidia_cuda_accelerator.cpp
        
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
        src/core/HardwareScout.cpp
        # auto_feature_registry excluded for Win32 IDE — use ssot_handlers (lightweight stubs)
        src/core/intent_engine.cpp
        src/core/model_training_pipeline.cpp
        src/model_trainer.cpp
        src/agentic/AgentOllamaClient.cpp
        src/agentic/hotpatch/Engine.cpp
        src/agentic/observability/Logger.cpp
        src/agentic/AdvancedAgentCoordinator.cpp
        src/modules/autonomous_agentic_orchestrator.cpp
        src/core/native_gguf_loader_link_stub.cpp
        src/codec/compression.cpp
        src/codec/brutal_gzip.cpp
        src/codec/gzip_brutal_inflate.cpp
        src/core/ssot_handlers.cpp
        src/core/ssot_handlers_ext_isolated.cpp
        src/core/beacon_link_stub.cpp
        src/win32app/v280_link_bridge.cpp
        src/core/ssot_validation.cpp

        # Unified Command Dispatch — Zero-drift SSOT dispatch (replaces manual registration)
        src/core/unified_command_dispatch.cpp
        src/core/startup_phase_registry.cpp

        # Phase 21: Distributed Swarm Inference Orchestrator
        src/cli/swarm_orchestrator.cpp
        src/agentic/swarm_orchestrator.cpp

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
        # Valuation-Critical Platform Subsystems (linker deps for UnifiedHotpatchManager)
        # ═══════════════════════════════════════════════════════════════
        src/core/autonomous_workflow_engine.cpp
        src/core/workspace_reasoning_profiles.cpp
        src/core/reasoning_profile.cpp
        src/core/deterministic_swarm.cpp
        src/core/swarm_scheduler.cpp
        src/core/gguf_swarm_plan_builder.cpp
        src/core/reasoning_schema_versioning.cpp
        src/core/input_guard_slicer.cpp
        src/core/safe_refactor_engine.cpp
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

        # Pending Edit Review Gate — tool-driven edit review queue with state machine
        src/core/PendingEditReviewGate.cpp

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
        src/collab/LiveShareSession.cpp
        src/collab/SharedTerminal.cpp
        src/collab/PairProgrammingAI.cpp
        src/collab/CollaborationManager.cpp
        src/collab/CollabToolHandlers.cpp
        src/collab/crdt_buffer.cpp
        src/collab/cursor_widget.cpp
        src/win32app/Win32IDE_TestExplorerTree.cpp
        src/win32app/Win32IDE_NetworkPanel.cpp
        src/core/omega_orchestrator.cpp
        src/core/omega_asm_native_kernel.cpp
        src/core/mesh_brain.cpp
        src/core/speciator_engine.cpp
        src/core/neural_bridge.cpp
        src/core/self_host_engine.cpp
        src/core/hardware_synthesizer.cpp
        src/core/transcendence_coordinator.cpp
        src/core/integrated_runtime.cpp
        src/win32app/Win32IDE_TranscendencePanel.cpp
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
        src/win32app/Win32IDE_ExtensionsPanel.cpp
        src/win32app/Win32IDE_Refactor.cpp
        src/win32app/Win32IDE_SearchPanel.cpp
        src/win32app/Win32IDE_ExpressionEvaluator.cpp
        src/win32app/Win32IDE_FuzzySearch.cpp
        src/win32app/Win32IDE_DAPServer.cpp
        src/win32app/Win32IDE_MissingActions.cpp
        src/agentic/StreamingResultChannel.cpp

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
        src/marketplace/extension_auto_installer.cpp
        # Phase 14-day: Extension Compatibility & Install stack (100%)
        src/win32app/Win32IDE_ExtensionManager.cpp
        src/win32app/Win32IDE_ExtensionToggles.cpp
        src/win32app/Win32IDE_ExtensionMarketplace.cpp
        src/marketplace/extension_marketplace_manager.cpp
        src/marketplace/vsix_installer.cpp
        src/marketplace/offline_cache_store.cpp
        src/marketplace/enterprise_policy_engine.cpp
        src/marketplace/vsix_loader.cpp
        # Multi-file search (real implementation; replaces earlier IDE stub)
        src/multi_file_search.cpp
        # Legacy support unit wiring removed from production lane.
        # Completed modules previously documented as shipped
        src/core/license_manager_panel.cpp
        src/telemetry/logger.cpp
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
        src/win32app/Win32IDE_TerminalAnsi.cpp
        src/win32app/Win32IDE_SnippetEngine.cpp
        src/win32app/Win32IDE_MultiCursor.cpp

        # Phase 16: Agentic Executor State Machine + Watchdog Controller
        src/win32app/Win32IDE_Phase16_AgenticController.cpp

        # Phase 17: Agentic Profiler (per-tool RDTSC cycle accounting)
        src/win32app/Win32IDE_Phase17_AgenticProfiler.cpp

        # Missing source files (linker gap closure)
        src/win32app/Win32IDE_VideoStudio.cpp
        src/win32app/Win32IDE_EnableAllFeatures.cpp
        src/core/feature_handlers_model_load.cpp
        src/autonomous_validation_layer.cpp
        src/video/tubi_backend.cpp

        # Sovereign Cursor — AI-powered editor brain
        src/editor/SovereignGapBuffer.cpp
        src/ai/SovereignVectorStore.cpp
        src/ai/SovereignCursor.cpp

        # Prometheus 800B MoE Weight Loader
        src/inference/PrometheusMoE.cpp

        # Phase 2: Aggressive Memory Architecture
        # VirtualAlloc reservation + lazy commitment for 26.5GB floor
        src/compression/virtualalloc_reservation_manager.cpp
        src/compression/sovereign_pager.cpp
        # INT4 KV unpacking hot-path for 4x memory compression
        src/compression/int4_kv_dequant_kernel.cpp
        # IQ2_M 2.5-bit weight dequantization kernel (scalar fallback + batch dispatch)
        src/compression/iq2m_dequant_kernel.cpp
        # Phase 3: Hot-path integration bridge (expert gating + selective commit)
        src/inference/inference_int4kv_bridge.cpp
        src/inference/flash_attention_vulkan_fp8.cpp
    )

    if(RAWR_HAS_MASM)
        set(TUBI_ASM src/asm/RawrXD_TubiVideoBackend.asm)
        set_source_files_properties(${TUBI_ASM} PROPERTIES LANGUAGE ASM_MASM)
        list(APPEND WIN32IDE_SOURCES ${TUBI_ASM})
    endif()

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
            src/agent/quantum_autonomous_todo_system.cpp
            src/agent/quantum_multi_model_agent_cycling.cpp
            src/agent/quantum_dynamic_time_manager.cpp
            src/agent/quantum_production_orchestrator.cpp
            src/agent/quantum_missing_impl.cpp
            src/vulkan_compute.cpp
            src/core/embedding_engine.cpp
            src/core/vector_index.cpp
            src/core/agentic_task_graph.cpp
            src/core/vision_encoder.cpp
            src/core/masm_stress_harness.cpp
            src/core/convergence_stress_harness.cpp
            src/core/enterprise_stress_tests.cpp
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
        set(PROMPT_ENGINE_ASM src/asm/RawrXD_DynamicPromptEngine.asm)
        set_source_files_properties(${PROMPT_ENGINE_ASM} PROPERTIES LANGUAGE ASM_MASM)

        if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
            add_library(RawrXD_DynamicPromptEngine SHARED
                ${PROMPT_ENGINE_ASM}
                src/core/dynamic_prompt_engine_glue.cpp
            )
        else()
            add_library(RawrXD_DynamicPromptEngine SHARED EXCLUDE_FROM_ALL
                ${PROMPT_ENGINE_ASM}
                src/core/dynamic_prompt_engine_glue.cpp
            )
        endif()
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

    if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
        list(REMOVE_ITEM WIN32IDE_SOURCES
            src/core/subsystem_mode_fallbacks.cpp
            src/core/model_loader_fallbacks.cpp
            src/core/rawrxd_native_log_fallback.cpp
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
    list(APPEND WIN32IDE_SOURCES
        src/core/local_parity_bridge.cpp
        src/core/vector_index_persistence.cpp
        src/context/workspace_context.cpp
        src/orchestration/session_state.cpp
    )
    if(NOT RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
        if(MSVC)
            # Link closure: rawr_engine_link_shims.cpp + fallbacks (Win32IDE removes rawr_engine.cpp — shims satisfy externals).
            set(_RAWRXD_WIN32IDE_FALLBACK_UNITS
                src/core/rawr_engine_link_shims.cpp
                src/core/cot_fallback_system.cpp
                src/core/model_loader_fallbacks.cpp
                src/llm_adapter/ggufrunner_link_fallbacks.cpp
                src/inference/gpu_dispatch_gate_win32ide_fallback.cpp
                src/inference/gguf_d3d12_bridge_link_fallback.cpp
                src/win32app/collab_cursor_fallbacks.cpp
                src/win32app/rtp_protocol_fallback.cpp
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
        src/win32app/sovereign_gpu_link_stubs.cpp
        # Skill System — Cursor-style .cursorrules injection (first 520 lines guarantee)
        src/skill_system/SkillInjectionEngine.cpp
        src/skill_system/SkillToggleUI.cpp
        src/skill_system/SkillSystemBuildIntegration.cpp
        src/skill_system/PromptWarmingEngine.cpp
    )

    set(WIN32IDE_EXTRA_ASM "")
    if(RAWR_HAS_MASM)
        set(WIN32IDE_EXTRA_ASM
            src/asm/RawrXD_PE_Importer.asm
            src/asm/RawrXD_PE_Exporter.asm
            src/asm/ghost_text_ranker.asm
            src/asm/diff_engine.asm
        )

        # GPU Sovereign Control Modules
        # These are intentionally excluded unless explicitly enabled, since they can
        # be noisy/brittle to assemble on many machines.
        option(RAWRXD_LINK_GPU_SOVEREIGN_ASM
            "Build/link src/asm/gpu/* sovereign modules into RawrXD-Win32IDE"
            OFF
        )
        set(RAWRXD_HAS_SOVEREIGN_GPU_ASM 0)
        if(RAWR_HAS_SOVEREIGN_ENGINES AND RAWRXD_LINK_GPU_SOVEREIGN_ASM)
            list(APPEND WIN32IDE_EXTRA_ASM
                src/asm/gpu/RawrXD_KFD_Sovereign_Interface.asm
                src/asm/gpu/RawrXD_RDNA3_Trap_Handler.asm
                src/asm/gpu/RawrXD_RDNA3_Shadow_Pager.asm
                src/asm/gpu/RawrXD_BitPlane_Transcoder.asm
                src/asm/gpu/RawrXD_RDNA3_Power_Sovereign.asm
                src/asm/gpu/RawrXD_RDNA3_Speculative_Preload.asm
                src/asm/gpu/RawrXD_Silicon_PUF_Signer.asm
                src/asm/gpu/RawrXD_RDNA3_MMIO_Recon.asm
                src/asm/gpu/RawrXD_RDNA3_MicroSequencer.asm
                src/asm/gpu/RawrXD_RDNA3_GDS_Neural_Link.asm
                src/asm/gpu/RawrXD_RDNA3_Telemetry_Refiner.asm
                src/asm/gpu/RawrXD_RDNA3_3x_Expander.asm
                src/asm/gpu/RawrXD_DDR5_Saturation_Streamer.asm
                src/asm/gpu/RawrXD_RDNA3_BusMaster_Pager.asm
                src/asm/gpu/RawrXD_RDNA3_HugePage_Expander.asm
                src/asm/gpu/RawrXD_RDNA3_3X_Virtualizer.asm
                src/asm/gpu/RawrXD_RDNA3_Elastic_Manifest.asm
                src/asm/gpu/RawrXD_RDNA3_Custom_Inflator.asm
                src/asm/gpu/RawrXD_RDNA3_Sovereign_Codec.asm
                src/asm/gpu/RawrXD_RDNA3_Silicon_Authenticator.asm
                src/asm/gpu/RawrXD_RDNA3_Neural_Entropy_Shield.asm
            )
            set(RAWRXD_HAS_SOVEREIGN_GPU_ASM 1)
        else()
            message(STATUS "[Win32IDE] Sovereign GPU ASM disabled — skipping src/asm/gpu/* (set -DRAWRXD_LINK_GPU_SOVEREIGN_ASM=ON to enable)")
            set(RAWRXD_HAS_SOVEREIGN_GPU_ASM 0)
        endif()

        set_source_files_properties(${WIN32IDE_EXTRA_ASM} PROPERTIES LANGUAGE ASM_MASM)
    endif()

    # Gold Lane: prefer real implementations for Win32IDE agentic/autonomy path.
    # Default ON: without fallback TUs, Win32IDE hits 100+ LNK2001s (asm_*, RTP_*, GGUF stubs, etc.).
    # Production/CI can force -DRAWRXD_ALLOW_AGENTIC_STUB_FALLBACK=OFF when the full ASM graph is linked.
    option(RAWRXD_ALLOW_AGENTIC_STUB_FALLBACK
        "Allow stub/link-fallback units in RawrXD-Win32IDE agentic path"
        ON
    )
    option(RAWRXD_ENABLE_IDE_PROBE_GATES
        "Enable lightweight Win32IDE probe gate translation units (sovereign/harness stacks stay external)"
        ON
    )
    option(RAWRXD_LINK_GGUF_SOVEREIGN_ASM
        "Link 4-file GGUF sovereign suite (compression, gearbox, reverse stream) into RawrXD-Win32IDE"
        OFF
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
            # ai_agent_masm_stubs.cpp is required (masm_agent_failure_detect_simd), not a fallback
            if(NOT _rawrxd_src MATCHES "ai_agent_masm_stubs\\.cpp$" AND _rawrxd_src MATCHES "(_stubs\\.cpp$|/stubs\\.cpp$|_stubs\\.asm$|/stubs\\.asm$|/.*_stub\\.cpp$|/.*_stub\\.asm$|/stub_.*\\.cpp$|/stub_.*\\.asm$|/shim_.*\\.(cpp|asm)$|/.*_(mock|fake)\\.(cpp|asm)$|/.*(mock_|fake_).*(\\.cpp|\\.asm)$|/.*_shims?\\.(cpp|asm)$|/link_stubs_.*\\.(cpp|asm)$|/.*_link_shims?\\.(cpp|asm)$|/.*_fallback[^/]*\\.(cpp|asm)$|/agentic_bridge_headless\\.cpp$|/Win32IDE_headless_stubs\\.cpp$)")
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
    # No generic stubs are permitted; a small exact-file allowlist is retained
    # for link-closure providers whose historical filenames still contain
    # shim/stub/fallback markers.
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
                                # Exact provider-file exemptions for strict Win32IDE link closure.
                                    if(src MATCHES "ai_agent_masm_stubs\.cpp$|rawr_engine_link_shims\.cpp$|model_loader_fallbacks\.cpp$|rawrxd_native_log_fallback\.cpp$|native_gguf_loader_link_stub\.cpp$|cot_fallback_system(_impl)?\.cpp$|collab_cursor_fallbacks\.cpp$|ggufrunner_link_fallbacks\.cpp$|gpu_dispatch_gate_win32ide_fallback\.cpp$|gguf_d3d12_bridge_link_fallback\.cpp$|rtp_protocol_fallback\.cpp$|quickjs_node_shims\.cpp$|sovereign_gpu_link_stubs\.cpp$")
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
    # These files carry a few historical shim/fallback names, but they are the
    # only providers for several Win32IDE/RawrEngine symbols in the strict lane.
    set(_RAWRXD_REQUIRED_WIN32IDE_PROVIDER_SOURCES
        src/core/ai_agent_masm_stubs.cpp
        src/core/model_loader_fallbacks.cpp
        src/core/native_gguf_loader_link_stub.cpp
        src/core/cot_fallback_system.cpp
        src/win32app/collab_cursor_fallbacks.cpp
        src/llm_adapter/ggufrunner_link_fallbacks.cpp
        src/inference/gpu_dispatch_gate_win32ide_fallback.cpp
        src/inference/gguf_d3d12_bridge_link_fallback.cpp
        src/win32app/rtp_protocol_fallback.cpp
        src/modules/quickjs_node_shims.cpp
        src/win32app/sovereign_gpu_link_stubs.cpp
    )

    # When RAWRXD_ALLOW_AGENTIC_STUB_FALLBACK=ON, keep *_fallback*.cpp / link shims so the IDE links.
    # When OFF, strip generic stub/fallback units (strict lane), then re-append the
    # exact provider files that close the Win32IDE link graph.
    if(NOT RAWRXD_ALLOW_AGENTIC_STUB_FALLBACK)
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX "(_stubs\\.cpp$|/stubs\\.cpp$|_stub\\.cpp$|/stub_.*\\.cpp$|_stubs\\.asm$|/stubs\\.asm$|_stub\\.asm$|/stub_.*\\.asm$|/shim_.*\\.(cpp|asm)$|missing_handler|link_stubs_.*\\.(cpp|asm)$|_shims?\\.(cpp|asm)$|_link_shims?\\.(cpp|asm)$|_fallback[^/]*\\.(cpp|asm)$|_(mock|fake)\\.(cpp|asm)$|stub_|shim_|mock_|fake_)")
    endif()
    # Keep required MASM bridge provider after broad stub filtering.
    if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES AND NOT EXISTS "${CMAKE_SOURCE_DIR}/src/core/ai_agent_masm_stubs.cpp")
        message(FATAL_ERROR "[ProductionLane] Required MASM bridge provider src/core/ai_agent_masm_stubs.cpp is missing.")
    endif()
    foreach(_rawrxd_required_provider IN LISTS _RAWRXD_REQUIRED_WIN32IDE_PROVIDER_SOURCES)
        if(EXISTS "${CMAKE_SOURCE_DIR}/${_rawrxd_required_provider}")
            list(REMOVE_ITEM WIN32IDE_SOURCES ${_rawrxd_required_provider})
            list(APPEND WIN32IDE_SOURCES ${_rawrxd_required_provider})
        endif()
    endforeach()


    # Pre-append strict guard: Verify no stubs were auto-added earlier
    if(RAWRXD_STRICT_AGENTIC_REALITY AND NOT RAWRXD_ALLOW_AGENTIC_STUB_FALLBACK)
        set(_RAWRXD_FORBIDDEN_WIN32IDE_SOURCES_POST "")
        foreach(_rawrxd_src IN LISTS WIN32IDE_SOURCES)
            if(_rawrxd_src MATCHES "(_stubs\\.cpp$|/stubs\\.cpp$|_stubs\\.asm$|/stubs\\.asm$|/.*_stub\\.(cpp|asm)$|/stub_.*\\.(cpp|asm)$|/shim_.*\\.(cpp|asm)$|/.*_(mock|fake)\\.(cpp|asm)$|/.*(mock_|fake_).*(\\.cpp|\\.asm)$|/link_stubs_.*\\.(cpp|asm)$|/.*_shims?\\.(cpp|asm)$|/.*_link_shims?\\.(cpp|asm)$|/.*_fallback[^/]*\\.(cpp|asm)$)" OR
               _rawrxd_src MATCHES "(missing_handler|/stubs\\.(cpp|asm)$|_stub\\.(cpp|asm)$|stub_|shim_|shims?\\.(cpp|asm)$|mock_|fake_|_(mock|fake)\\.(cpp|asm)$)")
                                if(NOT _rawrxd_src MATCHES "ai_agent_masm_stubs\\.cpp$|rawr_engine_link_shims\\.cpp$|model_loader_fallbacks\\.cpp$|rawrxd_native_log_fallback\\.cpp$|native_gguf_loader_link_stub\\.cpp$|cot_fallback_system(_impl)?\\.cpp$|collab_cursor_fallbacks\\.cpp$|ggufrunner_link_fallbacks\\.cpp$|gpu_dispatch_gate_win32ide_fallback\\.cpp$|gguf_d3d12_bridge_link_fallback\\.cpp$|rtp_protocol_fallback\\.cpp$|quickjs_node_shims\\.cpp$|sovereign_gpu_link_stubs\\.cpp$")
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

    # Add transcendence handler files for batch 36-42
    list(APPEND WIN32IDE_SOURCES
        src/win32app/Win32IDE_CursorParityBridge.cpp
        src/win32app/Win32IDE_OmegaOrchestrator.cpp
        src/win32app/Win32IDE_MeshBrain.cpp
        src/win32app/Win32IDE_SpeciatorEngine.cpp
        src/win32app/Win32IDE_NeuralBridge.cpp
        src/win32app/Win32IDE_SelfHostEngine.cpp
        src/win32app/Win32IDE_HardwareSynthesizer.cpp
    )

    # Add transcendence handler files for batch 43-49
    list(APPEND WIN32IDE_SOURCES
        src/win32app/Win32IDE_TranscendenceCoordinator.cpp
        src/win32app/Win32IDE_VulkanRenderer.cpp
        src/win32app/Win32IDE_OSExplorerInterceptor.cpp
        src/win32app/Win32IDE_MCPHooks.cpp
        src/win32app/Win32IDE_IOCPFileWatcher.cpp
        src/win32app/Win32IDE_IDEDiagnosticAutoHealer.cpp
        src/win32app/Win32IDE_ConsentPrompt.cpp
    )

    # Add UI & infrastructure handler files for batch 50-56
    list(APPEND WIN32IDE_SOURCES
        src/win32app/Win32IDE_CommandHandlers.cpp
        src/win32app/Win32IDE_AutonomousAgent.cpp
        src/win32app/Win32IDE_ChatMessageRenderer.cpp
        src/win32app/Win32IDE_ToolActionStatus.cpp
        src/win32app/Win32IDE_ChatPanel.cpp
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
        src/InferenceProfiler.cpp
        src/BackendOrchestrator.cpp
        src/streaming_gguf_loader.cpp
        src/core/thread_lifecycle_registry.cpp
        src/win32app/TitanIPC.cpp
        src/native_inference_backend.cpp
    )

    # Phases 49-55 runtime: WebSearch / Snapshot / CRDT / Fuzz / EvolutionLoop / Replay / Replication
    list(APPEND WIN32IDE_SOURCES
        src/agentic/observability/Telemetry.cpp
        src/runtime/SovereignWebSearch.cpp
        src/runtime/SovereignSnapshot.cpp
        src/runtime/SovereignCRDTSync.cpp
        src/runtime/SovereignFuzzEngine.cpp
        src/runtime/SovereignEvolutionLoop.cpp
        src/runtime/SovereignDeterministicReplay.cpp
        src/runtime/SovereignReplication.cpp
        src/runtime/SovereignMeshProvider.cpp
        src/runtime/SovereignMCPBridge.cpp
    )

    # ── SDMA GPU Scheduler (C++ replacement for MASM) ───────────────────────
    # Replaced broken MASM syntax with portable C++ implementations
    # sdma_scheduler: GPU DMA event loop, burst coalescing, TSC-based scheduling
    # sdma_ring_allocator: O(1) tensor slot bitmap allocator with spinlock
    list(APPEND WIN32IDE_SOURCES
        src/core/sdma/sdma_scheduler.cpp
        src/core/sdma/sdma_ring_allocator.cpp
        src/core/kv_cache_calculator.cpp
    )

    # Batch 33: NLShell + VectorIndex ASM kernels (Phases 46-48)
    list(APPEND WIN32IDE_EXTRA_ASM
        # src/asm/RawrXD_VectorIndex.asm   # excluded in Win32IDE: duplicates Vector_* symbols from src/runtime/RawrXDVectorIndex.cpp
        # src/asm/RawrXD_NLShell.asm  # Using C++ stub in ai_agent_masm_stubs.cpp instead to avoid duplicate symbol
        src/asm/RawrXD_AVX512_VectorSearch.asm
    )

    option(RAWRXD_ENABLE_PHASE1_COREUX_ASM "Enable Phase 1 Core UX MASM module (ghost text/chat/slash commands)" OFF)
    if(RAWR_HAS_MASM AND RAWRXD_ENABLE_PHASE1_COREUX_ASM)
        list(APPEND WIN32IDE_EXTRA_ASM
            src/asm/RawrXD_Phase1_CoreUX.asm
        )
    endif()
    # Ensure Batch 33 ASM files get LANGUAGE ASM_MASM (set_source_files_properties
    # on line ~4045 only covers the initial WIN32IDE_EXTRA_ASM before this append).
    if(RAWR_HAS_MASM)
        set_source_files_properties(
            # src/asm/RawrXD_NLShell.asm  # Using C++ stub instead
            src/asm/RawrXD_AVX512_VectorSearch.asm
            PROPERTIES LANGUAGE ASM_MASM
        )
        if(RAWRXD_ENABLE_PHASE1_COREUX_ASM)
            set_source_files_properties(
                src/asm/RawrXD_Phase1_CoreUX.asm
                PROPERTIES LANGUAGE ASM_MASM
            )
        endif()
    endif()

    # Roadmap: Surpass the Top 4 (P0/P1 Implementation)
    list(APPEND WIN32IDE_SOURCES
        src/CodebaseVectorIndex.cpp
        src/MultiFileRewriteEngine.cpp
        src/SpeculativeDecoder.cpp
        src/GitMCPBridge.cpp
        src/PredictiveEditEngine.cpp
        src/PredictiveGhostText.cpp
        src/VisionEncoder.cpp
        src/AgenticComposer.cpp
        src/NeuralHeatmapRenderer.cpp
        # AGI Gate 1: Speculative Decoder V2 + Inference Bridge
        src/gpu/speculative_decoder_v2.cpp
        src/gpu/speculative_inference_bridge.cpp
        src/gpu/paged_kv_cache.cpp
        src/inference/ultra_fast_inference.cpp
        # AI Engine Core Integration
        src/ai/ai_inference_real.cpp
        src/ai/speculative_decoder.cpp
        src/ai/production_inference_engine.cpp
        # Dual-model speculative orchestrator (simulation + integration seam; see SPECULATIVE_EXECUTION_INTEGRATION.md)
        src/inference/speculative_execution_engine.cpp
    )

    # Ensure Git/MCP tool initialization is always linked into Win32IDE.
    list(APPEND WIN32IDE_SOURCES
        src/tool_registry_init.cpp
        src/core/system_integrity_audit_trail.cpp
        # Link-closure: HTTP server entrypoints used by api_server.cpp
        src/server/RawrXD_HttpServer.cpp
        # Link-closure: NativeAgent ctor/dtor referenced by Win32IDE agentic bridge
        src/native_agent.cpp
        # Link-closure: GPUDispatchGate referenced by quantum_agent_orchestrator.cpp
        src/inference/rawrxd_gpu_dispatch_impl.cpp
        # Link-closure: RTP protocol impl referenced by Win32IDE_Commands.cpp
        src/win32app/rawrxd_rtp_protocol_impl.cpp
        # Link-closure: Sovereign GPU C++ fallback stubs referenced by Win32IDE_TabManager.cpp
        src/win32app/sovereign_gpu_impl.cpp
        # Link-closure: Root NativeGGUFLoader used by ssot_handlers.cpp probe
        native_gguf_loader.cpp
        # Link-closure: SovereignInferenceClient implementation referenced by agentic engines
        src/agentic/SovereignInferenceClient.cpp
        # Link-closure: Tool call parser implementation used by SovereignInferenceClient
        src/agentic/tool_call_parser.cpp
        # Link-closure: D3D12 bridge referenced by GPUDispatchGate fallback
        src/gguf_d3d12_bridge.cpp
        # Link-closure: EngineRegistry implementation referenced by runtime_core/tool init
        src/core/engine_registry.cpp
        src/core/unlinked_symbols_batch_012.cpp
        # Link-closure: real CoT fallback system (unified_hotpatch_manager.hpp)
        src/core/cot_fallback_system.cpp
        src/core/cot_fallback_system_impl.cpp
        # Link-closure: Win32IDE collaboration implementations (global ABI used by include/collab/*.h)
        src/win32app/rawrxd_collab_crdt_impl.cpp
        # Link-closure: ModelInvoker implementation
        src/agent/model_invoker.cpp
        # GGUFRunner callbacks/kernel shims: ggufrunner_link_fallbacks.cpp (avoid LNK2005 vs ggufrunner_impl.cpp)
        # Link-closure: LlamaNativeBridge used by SovereignInferenceClient
        src/inference/RawrXD_LlamaNative.cpp
        # Live telemetry collector for CreditGovernor dashboard
        src/inference/pipeline_telemetry.cpp
    )

    # Model Puller subsystem
    list(APPEND WIN32IDE_SOURCES
        src/model_puller/download_manager.cpp
        src/model_puller/hf_client.cpp
        src/model_puller/ollama_registry.cpp
        src/model_puller/model_index.cpp
        src/model_puller/model_puller.cpp
        src/model_puller/model_puller_cli.cpp
        src/win32app/Win32IDE_ModelManager.cpp
        src/win32app/Win32IDE_ModelManagerBridge.cpp
        src/win32app/Win32IDE_DownloadsPanel.cpp
        src/win32app/Win32IDE_ExtensionPanel.cpp
        src/modules/ExtensionUIState.cpp
        # Link-closure: Win32IDE missing-features method implementations referenced by Win32IDE/Core
        src/win32app/Win32IDE_MissingFeaturesCore.cpp
        # IDEFeatures implementation (initialize, loadWorkspace, openFile)
        src/features/missing_features.cpp
    )

    # Win32IDE link-closure: probe harnesses, runtime/agentic providers, extension/VCS (fixes LNK2019)
    list(APPEND WIN32IDE_SOURCES
        src/win32app/Win32IDE_Phase19_2_Soak.cpp
        src/win32app/Win32IDE_TBA_LinkGraph.cpp
        src/win32app/Win32IDE_GGUFManifold.cpp
        src/win32app/Win32IDE_ContextGovernor.cpp
        src/runtime/SovereignContextGovernor.cpp
        src/win32app/Win32IDE_ActionGraph.cpp
        src/runtime/SovereignActionGraph_link.cpp
        src/win32app/Win32IDE_ExecutionTruth.cpp
        src/agentic/observability/ExecutionTruth_link.cpp
        src/win32app/Win32IDE_ParityEngine.cpp
        src/runtime/SovereignParityEngine.cpp
        src/ui/rawrxd_software_raster.cpp
        src/ui/rawrxd_text_engine.cpp
        src/ui/rawrxd_linestream_raster.cpp
        src/ui/rawrxd_linestrip_cache.cpp
        src/ui/rawrxd_rle_raster.cpp
        src/ui/rawrxd_syntax_bridge.cpp
        src/ui/rawrxd_workspace_matrix.cpp
        src/win32app/Win32IDE_BuildProvenance.cpp
        src/win32app/Win32NeuralBridge.cpp
        src/win32app/Win32IDE_LineStripEditor.cpp
        src/win32app/Win32IDE_SyntaxLayoutBridge.cpp
        src/win32app/Win32IDE_WiringHandlers.cpp
        src/win32app/Win32IDE_AgentCursorOverlay.cpp
        src/ui/SidebarStagingPanel.cpp
        src/win32app/ExtensionEngine_bridge.cpp
        src/win32app/ExtensionHostIpcBridge.cpp
        src/ui/agent_cursor_renderer.cpp
        src/win32app/Win32IDE_TitanPressureLink.cpp
        src/agentic/AgenticRouterBridge.cpp
        src/agentic/agentic_router_bridge_lsp_adapter.cpp
        src/vcs/Win32GitPipe.cpp
        src/vcs/GitProviderTypes.cpp
        src/agentic/agentic_condition_evaluator.cpp
        src/agentic/agentic_planning_persistence.cpp
        src/agentic/lazarus_dispatcher.cpp
        src/agentic/observability/Metrics.cpp
        src/win32app/Win32IDE_Swarm.cpp
        src/win32app/RawrXD_AgentKernel.cpp
    )
    if(RAWR_HAS_MASM)
        list(APPEND WIN32IDE_EXTRA_ASM
            src/asm/sovereign_guard.asm
            src/asm/trinity_beacon_arbiter.asm
            src/asm/sovereign_media.asm
            src/x64/GGUF_UNIFIED_FINALIZER_CORE.asm
            src/win32app/ExtensionEngine.asm
            src/asm/RawrXD_Agent_Dispatch.asm
        )
        set_source_files_properties(
            src/win32app/ExtensionEngine.asm
            src/asm/RawrXD_Agent_Dispatch.asm
            PROPERTIES LANGUAGE ASM_MASM
        )
        if(RAWRXD_LINK_GGUF_SOVEREIGN_ASM)
            list(APPEND WIN32IDE_EXTRA_ASM
                src/x64/GGUF_COMPRESSION_ENGINE.asm
                src/x64/GGUF_MANUAL_GEARBOX.asm
                src/x64/GGUF_REVERSE_STREAM.asm
            )
            set_source_files_properties(
                src/x64/GGUF_COMPRESSION_ENGINE.asm
                src/x64/GGUF_MANUAL_GEARBOX.asm
                src/x64/GGUF_REVERSE_STREAM.asm
                PROPERTIES LANGUAGE ASM_MASM
            )
            message(STATUS "[Win32IDE] GGUF sovereign ASM suite linked (RAWRXD_LINK_GGUF_SOVEREIGN_ASM=ON)")
        endif()
    endif()

    add_executable(RawrXD-Win32IDE WIN32 ${WIN32IDE_SOURCES} ${ASM_KERNEL_SOURCES} ${WIN32IDE_EXTRA_ASM}
        src/ggml.c
        src/ggml-alloc.c
        src/ggml-quants.c
        src/ggml-cpu/ggml-cpu.c
        src/ggml-cpu/ggml-cpu.cpp
        src/ggml-cpu/traits.cpp
        src/ggml-cpu/hbm.cpp
        src/ggml-cpu/vec.cpp
        src/ggml-cpu/ops.cpp
        src/ggml-cpu/unary-ops.cpp
        src/ggml-cpu/binary-ops.cpp
        src/ggml-cpu/quants.c
        src/ggml-cpu/repack.cpp
        src/ggml-cpu/arch/x86/quants.c
        src/ggml-cpu/arch/x86/repack.cpp
    )

    if(RAWRXD_ENABLE_IDE_PROBE_GATES)
        target_sources(RawrXD-Win32IDE PRIVATE
            src/win32app/Win32IDE_KVApertureProbeGate.cpp
            src/win32app/Win32IDE_TextEngineProbeGate.cpp
            src/win32app/Win32IDE_TokenTickProbeGate.cpp
            src/win32app/Win32IDE_SovereignSmokeGate.cpp
        )
        target_compile_definitions(RawrXD-Win32IDE PRIVATE RAWRXD_IDE_PROBE_GATES=1)
        message(STATUS "[Win32IDE] IDE probe gates enabled (RAWRXD_ENABLE_IDE_PROBE_GATES=ON)")
    else()
        target_sources(RawrXD-Win32IDE PRIVATE src/win32app/Win32IDE_ProbeGatesDisabled.cpp)
        target_compile_definitions(RawrXD-Win32IDE PRIVATE RAWRXD_IDE_PROBE_GATES=0 RAWRXD_DISABLE_IDE_PROBES=1)
        message(STATUS "[Win32IDE] IDE probe gates disabled — CLI probe flags are no-ops")
    endif()

    if(TARGET rawrxd-fa8-minimal-spv)
        add_dependencies(RawrXD-Win32IDE rawrxd-fa8-minimal-spv)
    endif()
    if(TARGET rawrxd-fa8-tiled-spv)
        add_dependencies(RawrXD-Win32IDE rawrxd-fa8-tiled-spv)
    endif()

    target_include_directories(RawrXD-Win32IDE PRIVATE
        src/ggml-cpu
        src/bridge
        src/serve
    )

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
            /LARGEADDRESSAWARE
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:/DEBUG:FULL>
            $<$<CONFIG:Release>:/DEBUG:NONE>
            $<$<CONFIG:Release>:/OPT:REF>
            $<$<CONFIG:Release>:/OPT:ICF>
            $<$<CONFIG:Release>:/INCREMENTAL:NO>
            $<$<CONFIG:Release>:/LTCG:OFF>
            "/MAP:${_IDE_MAP_PATH}"
            /MAPINFO:EXPORTS
            /MANIFEST:NO
        )
        # Mirror the map at the build root for audit tools that scan ${CMAKE_BINARY_DIR}.
        add_custom_command(TARGET RawrXD-Win32IDE POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_IDE_MAP_PATH}" "${_IDE_MAP_AUDIT_PATH}"
            VERBATIM
        )
    endif()

    # Runtime feature gates for this target's source graph.
    target_compile_definitions(RawrXD-Win32IDE PRIVATE
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
    target_link_libraries(RawrXD-Win32IDE PRIVATE RawrXD-RE-Library imm32)
    if(TARGET ggml)
        target_link_libraries(RawrXD-Win32IDE PRIVATE ggml)
    endif()
    
    # Singularity Core (Pure MASM layer — optional by default; can be required in strict lanes)
    option(RAWRXD_REQUIRE_SINGULARITY_CORE "Fail configure if singularity_core.lib is missing" OFF)
    set(_RAWRXD_SINGULARITY_CORE_LIB "${CMAKE_SOURCE_DIR}/build/asm_obj/singularity_core.lib")
    if(EXISTS "${_RAWRXD_SINGULARITY_CORE_LIB}")
        target_link_libraries(RawrXD-Win32IDE PRIVATE "${_RAWRXD_SINGULARITY_CORE_LIB}")
        target_compile_definitions(RawrXD-Win32IDE PRIVATE RAWR_SINGULARITY_LINKED=1)
    else()
        if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
            message(FATAL_ERROR "[ProductionLane] singularity_core.lib is required in strict production strip mode but not found at ${_RAWRXD_SINGULARITY_CORE_LIB}. Build/stage MASM core first.")
        elseif(RAWRXD_REQUIRE_SINGULARITY_CORE)
            message(FATAL_ERROR "singularity_core.lib required but not found at ${_RAWRXD_SINGULARITY_CORE_LIB}. Build/stage MASM core first.")
        else()
            message(WARNING "singularity_core.lib not found at ${_RAWRXD_SINGULARITY_CORE_LIB} — Win32IDE links without it (place lib after MASM build).")
        endif()
        target_compile_definitions(RawrXD-Win32IDE PRIVATE RAWR_SINGULARITY_LINKED=0)
    endif()

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
    
    # Keep Titan optional for Win32IDE: runtime path uses explicit LoadLibrary/GetProcAddress.
    # Avoid linking the Titan import library here so missing RawrXD_Titan.dll cannot block process startup.
    if(TARGET RawrXD_Titan)
        target_compile_definitions(RawrXD-Win32IDE PRIVATE RAWRXD_HAS_TITAN_KERNEL=1)
    else()
        target_compile_definitions(RawrXD-Win32IDE PRIVATE RAWRXD_HAS_TITAN_KERNEL=0)
    endif()
    
    # Vulkan SDK for Win32IDE
    if(Vulkan_FOUND)
        target_include_directories(RawrXD-Win32IDE PRIVATE ${Vulkan_INCLUDE_DIRS})
        target_link_libraries(RawrXD-Win32IDE PRIVATE Vulkan::Vulkan)
        target_compile_definitions(RawrXD-Win32IDE PRIVATE RAWR_HAS_VULKAN=1 RAWR_ENABLE_VULKAN=1)
    elseif(_VK_SDK_DIRS)
        target_include_directories(RawrXD-Win32IDE PRIVATE "${_VK_SDK_DIR}/Include")
        target_link_directories(RawrXD-Win32IDE PRIVATE "${_VK_SDK_DIR}/Lib")
        target_link_libraries(RawrXD-Win32IDE PRIVATE vulkan-1)
        target_compile_definitions(RawrXD-Win32IDE PRIVATE RAWR_HAS_VULKAN=1 RAWR_ENABLE_VULKAN=1)
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
        d2d1 dwrite d3d11 dcomp d3dcompiler dwmapi
        dxgi
        advapi32 crypt32 bcrypt wintrust
        opengl32 wininet
        pdh
        rawrxd_gpu_gate
        ${MASM_OBJECTS}
        ${_WIN32IDE_LINKED_MONOLITHIC_OBJS}
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

    # =========================================================================
    # Runtime Plugin & Library Staging
    # =========================================================================
    # Stage ServeInference plugin if available
    if(TARGET RawrXD-ServeInference)
        add_dependencies(RawrXD-Win32IDE RawrXD-ServeInference)
        add_custom_command(TARGET RawrXD-Win32IDE POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different 
                    $<TARGET_FILE:RawrXD-ServeInference>
                    $<TARGET_FILE_DIR:RawrXD-Win32IDE>
            COMMENT "[Win32IDE] Staging RawrXD-ServeInference plugin"
            VERBATIM
        )
    endif()

    # Stage GGML/Llama runtime DLLs for GPU inference support (Vulkan backend)
    # These DLLs provide the backend inference engine that Win32IDE invokes at runtime.
    # Resolution order is CI-first, then local-machine fallbacks.
    if(NOT DEFINED GGML_BIN_DIR OR GGML_BIN_DIR STREQUAL "")
        set(_GGML_BIN_CANDIDATES)

        # Explicit overrides (recommended for CI).
        if(DEFINED ENV{GGML_BIN_DIR} AND NOT "$ENV{GGML_BIN_DIR}" STREQUAL "")
            list(APPEND _GGML_BIN_CANDIDATES "$ENV{GGML_BIN_DIR}")
        endif()
        if(DEFINED ENV{GGML_SDK_PATH} AND NOT "$ENV{GGML_SDK_PATH}" STREQUAL "")
            list(APPEND _GGML_BIN_CANDIDATES "$ENV{GGML_SDK_PATH}")
            list(APPEND _GGML_BIN_CANDIDATES "$ENV{GGML_SDK_PATH}/bin")
        endif()
        if(DEFINED ENV{LLAMA_CPP_BUILD_BIN} AND NOT "$ENV{LLAMA_CPP_BUILD_BIN}" STREQUAL "")
            list(APPEND _GGML_BIN_CANDIDATES "$ENV{LLAMA_CPP_BUILD_BIN}")
        endif()

        # GitHub Actions-friendly guesses.
        if(DEFINED ENV{GITHUB_WORKSPACE} AND NOT "$ENV{GITHUB_WORKSPACE}" STREQUAL "")
            list(APPEND _GGML_BIN_CANDIDATES "$ENV{GITHUB_WORKSPACE}/llama.cpp/build/bin")
            list(APPEND _GGML_BIN_CANDIDATES "$ENV{GITHUB_WORKSPACE}/../llama.cpp/build/bin")
            list(APPEND _GGML_BIN_CANDIDATES "$ENV{GITHUB_WORKSPACE}/external/llama.cpp/build/bin")
        endif()

        # Local-machine fallbacks.
        list(APPEND _GGML_BIN_CANDIDATES
            "D:/llama.cpp/build/bin"
            "C:/llama.cpp/build/bin"
            "${CMAKE_SOURCE_DIR}/llama.cpp/build/bin"
            "${CMAKE_SOURCE_DIR}/external/llama.cpp/build/bin"
        )

        foreach(_ggml_candidate ${_GGML_BIN_CANDIDATES})
            if(EXISTS "${_ggml_candidate}/ggml.dll" AND EXISTS "${_ggml_candidate}/llama.dll")
                set(GGML_BIN_DIR "${_ggml_candidate}")
                break()
            endif()
        endforeach()
        unset(_GGML_BIN_CANDIDATES)
    endif()
    
    if(DEFINED GGML_BIN_DIR AND EXISTS "${GGML_BIN_DIR}")
        set(_GGML_DLLS
            "ggml.dll"
            "ggml-base.dll"
            "ggml-cpu.dll"
            "ggml-vulkan.dll"
            "llama.dll"
        )
        foreach(_dll ${_GGML_DLLS})
            set(_dll_src "${GGML_BIN_DIR}/${_dll}")
            if(EXISTS "${_dll_src}")
                add_custom_command(TARGET RawrXD-Win32IDE POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different 
                            "${_dll_src}"
                            "$<TARGET_FILE_DIR:RawrXD-Win32IDE>"
                    COMMENT "[Win32IDE] Staging ${_dll} for GPU inference support"
                    VERBATIM
                )
            endif()
        endforeach()
        message(STATUS "[Win32IDE] GGML DLL staging enabled from ${GGML_BIN_DIR}")
    else()
        message(STATUS "[Win32IDE] GGML DLL source not found; continuing without runtime staging. Set GGML_BIN_DIR/GGML_SDK_PATH env var or -DGGML_BIN_DIR=<path> to enable staging.")
    endif()
    
    target_include_directories(RawrXD-Win32IDE PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/inference
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
        ${CMAKE_CURRENT_SOURCE_DIR}/src/skill_system
        ${CMAKE_CURRENT_SOURCE_DIR}/src/context
        ${CMAKE_CURRENT_SOURCE_DIR}/src/multimodal_engine
        ${CMAKE_CURRENT_SOURCE_DIR}/src/telemetry
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
        ${CMAKE_CURRENT_SOURCE_DIR}/src/git
        ${CMAKE_CURRENT_SOURCE_DIR}/src/collab
        ${CMAKE_CURRENT_SOURCE_DIR}/src/prometheus
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    if(MSVC)
        target_compile_options(RawrXD-Win32IDE PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:/EHsc /W0 /arch:AVX2 /Z7>
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
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
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
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
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
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
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

option(RAWRXD_ENABLE_HEAVY_GATES "Run heavy ownership/compiled-symbol guards as part of self_test_gate" OFF)
if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES AND NOT RAWRXD_ENABLE_HEAVY_GATES)
    message(FATAL_ERROR "[ProductionLane] RAWRXD_ENABLE_HEAVY_GATES=OFF is incompatible with RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON.")
endif()
if(RAWRXD_ENABLE_HEAVY_GATES)
    if(RAWRXD_HAS_SSOT_STUB_LANE_GUARD)
        add_custom_target(self_test_gate
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config $<CONFIG> --target ssot_stub_lane_guard
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config $<CONFIG> --target RawrXD-Win32IDE
            COMMENT "[CI Gate] Ownership guard + RawrXD-Win32IDE"
            VERBATIM
        )
    else()
        add_custom_target(self_test_gate
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config $<CONFIG> --target RawrXD-Win32IDE
            COMMENT "[CI Gate] RawrXD-Win32IDE (stub ownership guard disabled)"
            VERBATIM
        )
    endif()
else()
    add_custom_target(self_test_gate
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config $<CONFIG> --target RawrXD-Win32IDE
        COMMENT "[CI Gate] RawrXD-Win32IDE (heavy gates disabled)"
        VERBATIM
    )
endif()
if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
    add_dependencies(self_test_gate
        RawrXD-Win32IDE
        RawrEngine
        RawrXD_Gold
        RawrXD-InferenceEngine
        RawrXD_MultiWindow_Kernel
        RawrXD_DynamicPromptEngine
    )
endif()

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

    add_executable(workflow_persistence_smoke EXCLUDE_FROM_ALL
            tests/test_workflow_persistence.cpp
        )

        add_executable(smoke_test_measurement_integration EXCLUDE_FROM_ALL
            src/speculative/smoke_test_measurement_integration.cpp
        )
        target_include_directories(smoke_test_measurement_integration PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/src/speculative
            ${CMAKE_SOURCE_DIR}
        )
        target_link_libraries(smoke_test_measurement_integration PRIVATE kernel32)
        set_target_properties(smoke_test_measurement_integration PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
            CXX_STANDARD 17
        )
        add_test(NAME smoke_test_measurement_integration COMMAND smoke_test_measurement_integration)

        add_executable(e2e_integration_test EXCLUDE_FROM_ALL
            src/speculative/e2e_integration_test.cpp
            src/gguf_loader.cpp
        )
        target_include_directories(e2e_integration_test PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/src/speculative
            ${CMAKE_SOURCE_DIR}
        )
        target_link_libraries(e2e_integration_test PRIVATE kernel32)
        set_target_properties(e2e_integration_test PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
            CXX_STANDARD 17
        )
        add_test(NAME e2e_integration_test COMMAND e2e_integration_test)

        add_executable(workflow_persistence_smoke_original EXCLUDE_FROM_ALL
            tests/test_workflow_persistence.cpp
        src/agentic_loop_state.cpp
        src/agentic_memory_system.cpp
        src/execution_state_persistence.cpp
    )
    target_include_directories(workflow_persistence_smoke PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}
    )
    target_link_libraries(workflow_persistence_smoke PRIVATE kernel32 Rpcrt4)
    set_target_properties(workflow_persistence_smoke PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_executable(agent_workflow_orchestrator_smoke EXCLUDE_FROM_ALL
        tests/test_agent_workflow_orchestrator.cpp
    )
    target_include_directories(agent_workflow_orchestrator_smoke PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}
    )
    target_link_libraries(agent_workflow_orchestrator_smoke PRIVATE kernel32)
    set_target_properties(agent_workflow_orchestrator_smoke PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    add_test(NAME agent_workflow_orchestrator_smoke COMMAND agent_workflow_orchestrator_smoke)

    # Headless ghost-text runtime soak (no HWND/GDI dependency)
    add_executable(RawrXD-GhostSoak EXCLUDE_FROM_ALL
        tests/ghost_text_runtime_smoke.cpp
        src/agentic/OllamaProvider.cpp
        src/ai/inference_retry_shim.cpp
    )
    target_include_directories(RawrXD-GhostSoak PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
    )
    target_link_libraries(RawrXD-GhostSoak PRIVATE
        kernel32
        user32
        winhttp
        ws2_32
        ole32
        oleaut32
        version
        Rpcrt4
        Psapi
    )
    set_target_properties(RawrXD-GhostSoak PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME RawrXD-GhostSoak
    )
    add_test(NAME RawrXD-GhostSoak COMMAND RawrXD-GhostSoak --iterations 200 --max-inflight 16 --timeout-ms 1500)

    add_executable(benchmark_cli EXCLUDE_FROM_ALL
        src/cli_benchmark_main.cpp
        src/streaming/AdaptiveTensorCodec.cpp
        src/streaming/atc_benchmark.cpp
        src/gguf_parser.cpp
        src/gguf_loader.cpp
        src/quantization/BraidedQuantizer.cpp
        src/quantization/braided_quantizer_fingerprint.cpp
    )
    target_include_directories(benchmark_cli PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
    )
    target_link_libraries(benchmark_cli PRIVATE
        kernel32
        user32
        ole32
        oleaut32
        version
        Rpcrt4
        Psapi
    )
    set_target_properties(benchmark_cli PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_executable(model_permutator EXCLUDE_FROM_ALL
        src/model_permutator/permutator_main.cpp
        src/model_permutator/ModelPermutationEngine.cpp
        src/streaming/AdaptiveTensorCodec.cpp
        src/streaming/atc_benchmark.cpp
        src/gguf_parser.cpp
        src/gguf_loader.cpp
    )
    target_include_directories(model_permutator PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
    )
    target_link_libraries(model_permutator PRIVATE
        kernel32
        user32
        ole32
        oleaut32
        version
        Rpcrt4
        Psapi
    )
    set_target_properties(model_permutator PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    if(EXISTS "${CMAKE_SOURCE_DIR}/src/cpu_inference_engine_autofix_stub.cpp" AND NOT RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
        add_executable(RawrXD-AutoFixCLI
            src/app/cli_entrypoint.cpp
            src/agent/quantum_agent_orchestrator.cpp
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
    )
    target_include_directories(RawrXD-Benchmark PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
    )
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

    # Decode micro-benchmark (direct CPUInferenceEngine path, CLI-quirk free)
    add_executable(RawrXD-DecodeMicroBench EXCLUDE_FROM_ALL
        src/speculative/decode_microbench_cpu_engine.cpp
    )
    target_include_directories(RawrXD-DecodeMicroBench PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
    )
    target_link_libraries(RawrXD-DecodeMicroBench PRIVATE
        InferenceEngine
        kernel32
        user32
        ole32
        oleaut32
        version
        Rpcrt4
        Psapi
    )
    set_target_properties(RawrXD-DecodeMicroBench PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME RawrXD-DecodeMicroBench
    )
    message(STATUS "[DecodeProbe] CPUInferenceEngine decode micro-benchmark target: RawrXD-DecodeMicroBench")

    # ═══════════════════════════════════════════════════════════════════
    # Router Benchmark Runner — benchmark_generation + benchmark_long_context
    # ═══════════════════════════════════════════════════════════════════
    add_executable(RawrXD-RouterBenchRunner EXCLUDE_FROM_ALL
        src/tools/router_benchmark_runner.cpp
        src/core/accelerator_router.cpp
        src/core/amd_gpu_accelerator.cpp
        src/core/gpu_warmup.cpp
        src/core/intel_gpu_accelerator.cpp
        src/core/arm64_gpu_accelerator.cpp
        src/core/cerebras_wse_accelerator.cpp
        src/core/nvidia_cuda_accelerator.cpp
        src/core/flash_attention.cpp
    )
    target_include_directories(RawrXD-RouterBenchRunner PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
    )
    target_compile_definitions(RawrXD-RouterBenchRunner PRIVATE
        RAWR_BENCHMARK=1
        RAWR_HAS_SOVEREIGN_ENGINES=0
        RAWR_HAS_RAWR_INFERENCE=0
    )
    if(MSVC)
        target_compile_options(RawrXD-RouterBenchRunner PRIVATE /EHsc /W0)
    endif()
    target_link_libraries(RawrXD-RouterBenchRunner PRIVATE
        kernel32
        user32
        ws2_32
        winhttp
        bcrypt
        advapi32
        crypt32
        psapi
        shlwapi
        dbghelp
    )
    set_target_properties(RawrXD-RouterBenchRunner PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME RawrXD-RouterBenchRunner
    )
    message(STATUS "[Phase 31.8] Router benchmark runner target: RawrXD-RouterBenchRunner")

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
    # Query Fusion Benchmark — Phase 9 deterministic gated fusion microbench
    # ═══════════════════════════════════════════════════════════════════
    add_executable(RawrXD-QueryFusionBenchmark EXCLUDE_FROM_ALL
        src/tools/query_fusion_benchmark.cpp
        src/kernels/query_fusion_kernel.cpp
        src/kernels/RawrXD_QueryFusion_AVX512.asm
    )
    target_include_directories(RawrXD-QueryFusionBenchmark PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
    )
    target_compile_definitions(RawrXD-QueryFusionBenchmark PRIVATE
        RAWR_BENCHMARK=1
    )
    if(MSVC)
        target_compile_options(RawrXD-QueryFusionBenchmark PRIVATE /EHsc /W0 /O2)
    endif()
    set_target_properties(RawrXD-QueryFusionBenchmark PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME RawrXD-QueryFusionBenchmark
    )
    message(STATUS "[Phase 9] Query Fusion Benchmark target: RawrXD-QueryFusionBenchmark")

    # ═══════════════════════════════════════════════════════════════════
    # Disk Paged Inference Benchmark — storage throughput for large-model paging
    # ═══════════════════════════════════════════════════════════════════
    add_executable(RawrXD-DiskPagedBench EXCLUDE_FROM_ALL
        src/tools/disk_paged_inference_benchmark.cpp
    )
    target_include_directories(RawrXD-DiskPagedBench PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
    )
    if(MSVC)
        target_compile_options(RawrXD-DiskPagedBench PRIVATE /EHsc /W2 /O2)
    endif()
    target_link_libraries(RawrXD-DiskPagedBench PRIVATE
        kernel32
    )
    set_target_properties(RawrXD-DiskPagedBench PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME RawrXD-DiskPagedBench
    )
    message(STATUS "[2T] Disk paged benchmark target: RawrXD-DiskPagedBench")

    option(RAWRXD_SOVEREIGN_CODEC_USE_MASM "Enable sovereign codec MASM fast-path entry points" OFF)
    set(RAWRXD_SOVEREIGN_CODEC_SOURCES
        src/compression/sovereign_bitstream_codec.cpp
    )
    if(MSVC AND RAWRXD_SOVEREIGN_CODEC_USE_MASM)
        list(APPEND RAWRXD_SOVEREIGN_CODEC_SOURCES
            src/compression/sovereign_bitstream_codec_masm_stub.asm
        )
    endif()

    add_executable(RawrXD-SovereignCodecParity EXCLUDE_FROM_ALL
        src/tools/sovereign_codec_parity.cpp
        ${RAWRXD_SOVEREIGN_CODEC_SOURCES}
    )
    target_include_directories(RawrXD-SovereignCodecParity PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
    )
    if(RAWRXD_SOVEREIGN_CODEC_USE_MASM)
        target_compile_definitions(RawrXD-SovereignCodecParity PRIVATE RAWRXD_SOVEREIGN_CODEC_ENABLE_MASM=1)
    endif()
    if(MSVC)
        target_compile_options(RawrXD-SovereignCodecParity PRIVATE /EHsc /W2)
    endif()
    target_link_libraries(RawrXD-SovereignCodecParity PRIVATE
        kernel32
    )
    set_target_properties(RawrXD-SovereignCodecParity PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME RawrXD-SovereignCodecParity
    )
    message(STATUS "[Sovereign] Codec parity target: RawrXD-SovereignCodecParity")



    # ═══════════════════════════════════════════════════════════════════
    # TPS smoke — GGUFRunner + model_runtime_gate + runtime_surface unity + decode TPS vs ref (default 239)
    # ═══════════════════════════════════════════════════════════════════
    add_executable(RawrXD-TpsSmoke EXCLUDE_FROM_ALL
        src/tools/RawrXD_TpsSmoke.cpp
        src/llm_adapter/GGUFRunner.cpp
        src/llm_adapter/gguf_k_quants.cpp
        src/llm_adapter/QuantBackend.cpp
        src/llm_adapter/ggufrunner_link_fallbacks.cpp
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
    include(${CMAKE_SOURCE_DIR}/cmake/SmokeE2EIPC.cmake)

# ─────────────────────────────────────────────────────────────────────────────
# RawrXD-MemoryBench — synthetic stress benchmark for the 10-strategy memory
# subsystem. EXCLUDE_FROM_ALL so it never breaks the main IDE build.
# Build: cmake --build build-ninja --target RawrXD-MemoryBench
# Run  : .\build-ninja\bin\RawrXD-MemoryBench.exe [--verbose]
# ─────────────────────────────────────────────────────────────────────────────
add_executable(RawrXD-MemoryBench EXCLUDE_FROM_ALL
    src/memory/memory_system_bench.cpp
    src/memory/memory_morph_controller.cpp
    src/memory/attention_heat_zone_partitioning.cpp
    src/memory/cross_session_memory_dedup.cpp
    src/memory/speculative_memory_branching.cpp
    src/memory/kv_cache_pager.cpp
    src/memory/layer_offload_scheduler.cpp
    src/memory/tensor_compression_pool.cpp
    src/memory/gradient_checkpoint_manager.cpp
    src/memory/sliding_window_kv_eviction.cpp
    src/memory/predictive_kv_prefetch_graph.cpp
    src/memory/execution_path_memory_folding.cpp
    src/memory/delta_state_kv_encoding.cpp
    src/memory/context_topology_compression.cpp
)

target_include_directories(RawrXD-MemoryBench PRIVATE
    src/memory
    include
)

if(MSVC)
    # /O2 only in non-Debug configs; /RTC1 from CMAKE_CXX_FLAGS_DEBUG is incompatible with /O2
    target_compile_options(RawrXD-MemoryBench PRIVATE
        /EHsc /W3 /arch:AVX2 /std:c++20
        $<$<NOT:$<CONFIG:Debug>>:/O2>
    )
else()
    target_compile_options(RawrXD-MemoryBench PRIVATE -O2 -std=c++20 -Wall -Wextra)
endif()

target_link_libraries(RawrXD-MemoryBench PRIVATE kernel32)

set_target_properties(RawrXD-MemoryBench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME RawrXD-MemoryBench
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

message(STATUS "[MemoryBench] RawrXD-MemoryBench target registered (cmake --build ... --target RawrXD-MemoryBench)")

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
        src/tests/swarm_smoke_stubs.cpp
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

# Runtime truth probe (manual): validates MCP stdio transport, NL shell scoring,
# and vector kernel behavior with direct in-process calls.
if(MSVC AND RAWR_HAS_MASM)
    set(MCP_SERVER_ASM_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_MCPServer.asm)
    set(MCP_SERVER_ASM_OBJ ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/RawrXD-MCPServer.dir/RawrXD_MCPServer.obj)
    set(MCP_SERVER_EXE ${CMAKE_BINARY_DIR}/bin/RawrXD-MCPServer.exe)
    set(MCP_SERVER_KERNEL32_LIB "kernel32.lib")
    if(DEFINED _WIN10_SDK_ROOT AND DEFINED _SDK_UM_VER)
        set(_MCP_KERNEL32_CANDIDATE "${_WIN10_SDK_ROOT}/Lib/${_SDK_UM_VER}/um/x64/kernel32.lib")
        if(EXISTS "${_MCP_KERNEL32_CANDIDATE}")
            set(MCP_SERVER_KERNEL32_LIB "${_MCP_KERNEL32_CANDIDATE}")
        endif()
    endif()
    set(MCP_SERVER_LINK_LIBPATH_ARGS "")
    foreach(_lp IN LISTS _RAWR_MSVC_LIBPATHS)
        list(APPEND MCP_SERVER_LINK_LIBPATH_ARGS "/LIBPATH:${_lp}")
    endforeach()

    add_custom_command(
        OUTPUT ${MCP_SERVER_ASM_OBJ}
        COMMAND ${CMAKE_ASM_MASM_COMPILER} /c /Zi /Zd /Fo${MCP_SERVER_ASM_OBJ} ${MCP_SERVER_ASM_SRC}
        DEPENDS ${MCP_SERVER_ASM_SRC}
        COMMENT "[MASM] Assembling RawrXD_MCPServer.asm (stdio JSON-RPC MCP host)"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${MCP_SERVER_EXE}
        COMMAND ${CMAKE_LINKER} /SUBSYSTEM:CONSOLE /ENTRY:RawrXD_MCPServer_Entry /OUT:${MCP_SERVER_EXE} ${MCP_SERVER_ASM_OBJ} ${MCP_SERVER_LINK_LIBPATH_ARGS} ${MCP_SERVER_KERNEL32_LIB}
        DEPENDS ${MCP_SERVER_ASM_OBJ}
        COMMENT "[LINK] Linking RawrXD-MCPServer.exe"
        VERBATIM
    )

    add_custom_target(RawrXD-MCPServer DEPENDS ${MCP_SERVER_EXE})

    add_executable(RawrXD-RuntimeTruthProbe EXCLUDE_FROM_ALL
        src/tests/runtime_truth_probe.cpp
        src/runtime/SovereignMCPBridge.cpp
        src/runtime/RawrXDNLShell.cpp
        src/runtime/RawrXDVectorIndex.cpp
        src/asm/RawrXD_NLShell.asm
        src/asm/RawrXD_VectorIndex.asm
    )
    target_include_directories(RawrXD-RuntimeTruthProbe PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
    )
    target_compile_definitions(RawrXD-RuntimeTruthProbe PRIVATE
        RAWRXD_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )
    if(MSVC)
        target_compile_options(RawrXD-RuntimeTruthProbe PRIVATE /EHsc /W2)
    endif()
    target_link_libraries(RawrXD-RuntimeTruthProbe PRIVATE kernel32)
    add_dependencies(RawrXD-RuntimeTruthProbe RawrXD-MCPServer)
    set_target_properties(RawrXD-RuntimeTruthProbe PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME RawrXD-RuntimeTruthProbe-v6
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
    )
    message(STATUS "[RuntimeTruth] Target: RawrXD-RuntimeTruthProbe")
endif()

# MCP + Model integration smoke test (manual)
add_executable(MCP-ModelSmoke EXCLUDE_FROM_ALL
    src/tests/mcp_model_smoke_test.cpp
    src/runtime/SovereignMCPBridge.cpp
)
target_include_directories(MCP-ModelSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)
if(MSVC)
    target_compile_options(MCP-ModelSmoke PRIVATE /EHsc /W2)
endif()
target_link_libraries(MCP-ModelSmoke PRIVATE kernel32 winhttp)
add_dependencies(MCP-ModelSmoke RawrXD-MCPServer)
set_target_properties(MCP-ModelSmoke PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME MCP-ModelSmoke
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
message(STATUS "[MCPSmoke] Target: MCP-ModelSmoke")

# Extension Installer smoke test — offline + live marketplace parity
# Usage: ExtensionInstallerSmoke.exe            (offline/unit)
#        ExtensionInstallerSmoke.exe --live      (live marketplace GET)
add_executable(ExtensionInstallerSmoke EXCLUDE_FROM_ALL
    src/tests/extension_installer_smoketest.cpp
    src/marketplace/extension_auto_installer.cpp
    src/marketplace/extension_marketplace_manager.cpp
    src/marketplace/vsix_installer.cpp
    src/marketplace/offline_cache_store.cpp
    src/marketplace/enterprise_policy_engine.cpp
    src/win32app/VSCodeMarketplaceAPI.cpp
)
target_include_directories(ExtensionInstallerSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)
if(MSVC)
    target_compile_options(ExtensionInstallerSmoke PRIVATE /EHsc /W2 /Z7)
endif()
target_link_libraries(ExtensionInstallerSmoke PRIVATE
    kernel32 winhttp wintrust crypt32 shell32 ole32
)
set_target_properties(ExtensionInstallerSmoke PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME ExtensionInstallerSmoke
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
message(STATUS "[ExtSmoke] Target: ExtensionInstallerSmoke  (offline + --live)")

# BackendOrchestrator model sharding smoke test (manual)
add_executable(BackendOrchestratorShardSmoke EXCLUDE_FROM_ALL
    src/tests/backend_orchestrator_shard_smoke.cpp
    src/tests/backend_orchestrator_shard_smoke_stubs.cpp
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

# Context decision policy harness (manual + optional ctest)
add_executable(RawrXD-ContextDecisionHarness EXCLUDE_FROM_ALL
    tests/test_context_decision_harness.cpp
    src/core/context_config.cpp
    src/core/vram_probe.cpp
)
target_include_directories(RawrXD-ContextDecisionHarness PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)
if(MSVC)
    target_compile_options(RawrXD-ContextDecisionHarness PRIVATE /EHsc /W2)
endif()
if(WIN32)
    target_link_libraries(RawrXD-ContextDecisionHarness PRIVATE kernel32 dxgi)
endif()
set_target_properties(RawrXD-ContextDecisionHarness PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME RawrXD-ContextDecisionHarness
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
if(BUILD_TESTING)
    add_test(NAME context_decision_harness COMMAND $<TARGET_FILE:RawrXD-ContextDecisionHarness>)
endif()
message(STATUS "[ContextDecision] Harness target: RawrXD-ContextDecisionHarness")

# ============================================================================
# Agentic Planning Orchestrator Smoke Tests
# ============================================================================
add_executable(agentic_orchestrator_smoke_test EXCLUDE_FROM_ALL
    src/agentic/agentic_planning_orchestrator.cpp
    src/agentic/agentic_orchestrator_integration.cpp
    src/agentic/agentic_tool_executor_bridge.cpp
    src/agentic/agentic_audit_sink.cpp
    src/agentic/observability/Logger.cpp
    src/core/execution_governor.cpp
    src/core/hotpatch_wiring.cpp
    src/core/agent_safety_contract.cpp
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
# Intelligent Review System Smoke Tests
# ============================================================================
add_executable(intelligent_review_smoke_test EXCLUDE_FROM_ALL
    src/intelligent_review.cpp
    src/smoke_test_intelligent_review.cpp
)

target_include_directories(intelligent_review_smoke_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_features(intelligent_review_smoke_test PRIVATE cxx_std_17)

if(WIN32)
    target_link_libraries(intelligent_review_smoke_test PRIVATE
        Threads::Threads
        user32 gdi32 shell32
    )
else()
    target_link_libraries(intelligent_review_smoke_test PRIVATE Threads::Threads)
endif()

message(STATUS "[IntelligentReview] Smoke test target: intelligent_review_smoke_test (build with: cmake --build . --target intelligent_review_smoke_test)")

# ============================================================================
# FP8 Quantization Test Suite
# ============================================================================
add_executable(test_fp8_quantization EXCLUDE_FROM_ALL
    tests/test_fp8_quantization.cpp
    src/quantization/fp8_quantizer.cpp
)

target_include_directories(test_fp8_quantization PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_features(test_fp8_quantization PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(test_fp8_quantization PRIVATE /EHsc /W2 /arch:AVX2)
endif()

if(WIN32)
    target_link_libraries(test_fp8_quantization PRIVATE
        Threads::Threads
    )
else()
    target_link_libraries(test_fp8_quantization PRIVATE Threads::Threads)
endif()

set_target_properties(test_fp8_quantization PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)

message(STATUS "[FP8Quantization] Test target: test_fp8_quantization (build with: cmake --build . --target test_fp8_quantization)")

if(WIN32)
    add_executable(test_sovereign_pager_smoke EXCLUDE_FROM_ALL
        tests/test_sovereign_pager_smoke.cpp
        src/compression/sovereign_pager.cpp
    )
    target_include_directories(test_sovereign_pager_smoke PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
    )
    target_compile_features(test_sovereign_pager_smoke PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(test_sovereign_pager_smoke PRIVATE /EHsc /W3)
    endif()
    target_link_libraries(test_sovereign_pager_smoke PRIVATE kernel32)
    set_target_properties(test_sovereign_pager_smoke PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
    )
endif()

# ============================================================================
# RawrXD_Inference.exe — Hybrid Sovereign Inference Kernel
# ============================================================================
# Standalone executable that:
#   - Calls MASM privilege escalation (RawrXD_EnableSeLockMemoryPrivilege)
#   - Initializes SovereignPager for large-page memory management
#   - Loads GGUF weights directly (zero-copy)
#   - Bypasses Ollama and REST API entirely
# Run: psexec -s -i RawrXD_Inference.exe --model "bg40-unleashed.bin"
# ============================================================================
add_executable(RawrXD_Inference
    src/sovereign_inference/RawrXD_Inference.cpp
    src/compression/sovereign_pager.cpp
    src/asm/model_streamer_x64.asm
)

target_include_directories(RawrXD_Inference PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/compression
)

target_compile_features(RawrXD_Inference PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(RawrXD_Inference PRIVATE /EHsc /W3 /O2 /arch:AVX2)
    enable_language(ASM_MASM)
    set_source_files_properties(src/asm/model_streamer_x64.asm PROPERTIES LANGUAGE ASM_MASM)
endif()

target_link_libraries(RawrXD_Inference PRIVATE kernel32 advapi32 dstorage d3d12 dxgi)

set_target_properties(RawrXD_Inference PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# ============================================================================
# FP8 Verifier Test Suite (Scalar vs FP8 Numerical Validation)
# ============================================================================
add_executable(test_fp8_verifier EXCLUDE_FROM_ALL
    tests/test_fp8_verifier.cpp
    src/verify/fp8_verifier.cpp
    src/kernels/sovereign_fp8_quantizer.asm
)

target_include_directories(test_fp8_verifier PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_features(test_fp8_verifier PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(test_fp8_verifier PRIVATE /EHsc /W2 /arch:AVX2)
endif()

if(WIN32)
    target_link_libraries(test_fp8_verifier PRIVATE
        Threads::Threads
    )
else()
    target_link_libraries(test_fp8_verifier PRIVATE Threads::Threads)
endif()

set_target_properties(test_fp8_verifier PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)

message(STATUS "[FP8Verifier] Test target: test_fp8_verifier (build with: cmake --build . --target test_fp8_verifier)")

# ============================================================================
# FP8 Sampling Hook Test (Live Pipeline Shadow Verification)
# ============================================================================
add_executable(test_fp8_sampling_hook EXCLUDE_FROM_ALL
    tests/test_fp8_sampling_hook.cpp
    src/verify/fp8_sampling_hook.cpp
    src/kernels/sovereign_fp8_quantizer.asm
)

target_include_directories(test_fp8_sampling_hook PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_features(test_fp8_sampling_hook PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(test_fp8_sampling_hook PRIVATE /EHsc /W2 /arch:AVX2)
endif()

if(WIN32)
    target_link_libraries(test_fp8_sampling_hook PRIVATE
        Threads::Threads
    )
else()
    target_link_libraries(test_fp8_sampling_hook PRIVATE Threads::Threads)
endif()

set_target_properties(test_fp8_sampling_hook PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)

message(STATUS "[FP8Verifier] Sampling hook test: test_fp8_sampling_hook")

# ============================================================================
# AVX-512 FP8 Quantization Kernel
# ============================================================================
add_library(fp8_avx512_kernel OBJECT
    src/kernels/sovereign_fp8_quantizer_avx512.asm
)

set_target_properties(fp8_avx512_kernel PROPERTIES
    COMPILE_FLAGS "-D__AVX512F__"
)

# ============================================================================
# AVX-512 FP8 Quantization Test
# ============================================================================
add_executable(test_fp8_avx512 EXCLUDE_FROM_ALL
    tests/test_fp8_avx512.cpp
    src/kernels/fp8_quantizer_avx512.cpp
)

target_include_directories(test_fp8_avx512 PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_features(test_fp8_avx512 PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(test_fp8_avx512 PRIVATE /EHsc /W2 /arch:AVX512)
endif()

if(WIN32)
    target_link_libraries(test_fp8_avx512 PRIVATE
        Threads::Threads
    )
else()
    target_link_libraries(test_fp8_avx512 PRIVATE Threads::Threads)
endif()

set_target_properties(test_fp8_avx512 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)

message(STATUS "[FP8] AVX-512 kernel: fp8_avx512_kernel")
message(STATUS "[FP8] AVX-512 test: test_fp8_avx512")

# ============================================================================
# Credit-Based Flow Control
# ============================================================================
add_executable(test_credit_based_flow_control EXCLUDE_FROM_ALL
    tests/test_credit_based_flow_control.cpp
    src/flow_control/credit_based_flow_control.cpp
)

target_include_directories(test_credit_based_flow_control PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_features(test_credit_based_flow_control PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(test_credit_based_flow_control PRIVATE /EHsc /W2 /arch:AVX2)
endif()

if(WIN32)
    target_link_libraries(test_credit_based_flow_control PRIVATE
        Threads::Threads
    )
else()
    target_link_libraries(test_credit_based_flow_control PRIVATE Threads::Threads)
endif()

set_target_properties(test_credit_based_flow_control PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)

message(STATUS "[FlowControl] Credit-based test: test_credit_based_flow_control")

# ============================================================================
# Credit Governor Auto-Tuner
# ============================================================================
add_executable(test_credit_governor EXCLUDE_FROM_ALL
    tests/test_credit_governor.cpp
    src/flow_control/credit_governor.cpp
)

target_include_directories(test_credit_governor PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_features(test_credit_governor PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(test_credit_governor PRIVATE /EHsc /W2 /arch:AVX2)
endif()

set_target_properties(test_credit_governor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)

message(STATUS "[FlowControl] Credit governor test: test_credit_governor")

# ============================================================================
# Pipeline Integration Harness (FP8 + Governor + Credits)
# ============================================================================
add_executable(test_pipeline_integration EXCLUDE_FROM_ALL
    tests/test_pipeline_integration.cpp
    src/flow_control/credit_governor.cpp
    src/flow_control/credit_based_flow_control.cpp
    src/kernels/fp8_quantizer_avx512.cpp
)

target_include_directories(test_pipeline_integration PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_features(test_pipeline_integration PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(test_pipeline_integration PRIVATE /EHsc /W2 /arch:AVX512)
endif()

set_target_properties(test_pipeline_integration PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)

message(STATUS "[Integration] Pipeline harness: test_pipeline_integration")

# ============================================================================
# Rust Parser + Credit Governor Integration
# ============================================================================
add_executable(test_rust_parser_integration EXCLUDE_FROM_ALL
    tests/test_rust_parser_integration.cpp
    src/core/ast_graph_engine_impl.cpp
    src/core/rust_parser_v2.cpp
    src/core/symbol_table.cpp
    src/flow_control/credit_governor.cpp
    src/flow_control/credit_based_flow_control.cpp
)

target_include_directories(test_rust_parser_integration PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_features(test_rust_parser_integration PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(test_rust_parser_integration PRIVATE /EHsc /W2 /arch:AVX2)
endif()

set_target_properties(test_rust_parser_integration PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)

message(STATUS "[Integration] Rust parser harness: test_rust_parser_integration")

# ============================================================================
# Pipeline Telemetry Dashboard (TUI)
# ============================================================================
add_executable(pipeline_dashboard EXCLUDE_FROM_ALL
    tests/pipeline_dashboard.cpp
    src/kernels/fp8_quantizer_avx512.cpp
    src/flow_control/credit_governor.cpp
    src/flow_control/credit_based_flow_control.cpp
)

target_include_directories(pipeline_dashboard PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_features(pipeline_dashboard PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(pipeline_dashboard PRIVATE /EHsc /W2 /arch:AVX2)
endif()

set_target_properties(pipeline_dashboard PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)

message(STATUS "[Dashboard] Pipeline telemetry TUI: pipeline_dashboard")

# ============================================================================
# Hotpatch Tool Smoke Test
# ============================================================================
add_executable(test_hotpatch_tools_smoke EXCLUDE_FROM_ALL
    tests/test_hotpatch_tools_smoke.cpp
    src/agentic/AgentToolHandlers.cpp
)

target_include_directories(test_hotpatch_tools_smoke PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_features(test_hotpatch_tools_smoke PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(test_hotpatch_tools_smoke PRIVATE /EHsc /W2 /arch:AVX2)
endif()

set_target_properties(test_hotpatch_tools_smoke PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)

message(STATUS "[Smoke] Hotpatch tool smoke test: test_hotpatch_tools_smoke")

# ============================================================================
# Fused FP8 Quantization Kernel
# ============================================================================
add_executable(test_fused_fp8_quantizer EXCLUDE_FROM_ALL
    tests/test_fused_fp8_quantizer.cpp
    src/kernels/fused_fp8_quantizer.cpp
)

target_include_directories(test_fused_fp8_quantizer PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_features(test_fused_fp8_quantizer PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(test_fused_fp8_quantizer PRIVATE /EHsc /W2 /arch:AVX2)
endif()

if(WIN32)
    target_link_libraries(test_fused_fp8_quantizer PRIVATE
        Threads::Threads
    )
else()
    target_link_libraries(test_fused_fp8_quantizer PRIVATE Threads::Threads)
endif()

set_target_properties(test_fused_fp8_quantizer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)

message(STATUS "[FP8] Fused kernel test: test_fused_fp8_quantizer")

# ============================================================================
# Context Fusion Harness
# ============================================================================
add_executable(context_fusion_harness EXCLUDE_FROM_ALL
    src/tests/context_fusion_harness.cpp
    src/core/ContextFusionEngine.cpp
    src/win32app/GhostTextContextSubscriber.cpp
    src/win32app/PrefixCache.cpp
)

target_include_directories(context_fusion_harness PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_features(context_fusion_harness PRIVATE cxx_std_17)

if(WIN32)
    target_link_libraries(context_fusion_harness PRIVATE
        Threads::Threads
        user32 gdi32 shell32 ws2_32
    )
else()
    target_link_libraries(context_fusion_harness PRIVATE Threads::Threads)
endif()

message(STATUS "[ContextFusion] Harness target: context_fusion_harness (build with: cmake --build . --target context_fusion_harness)")

# ============================================================================
# TLS Bridge Smoke Tests
# ============================================================================
add_executable(tls_bridge_smoke_test EXCLUDE_FROM_ALL
    src/llm_tls_bridge.cpp
    src/tests/tls_bridge_smoke.cpp
)

target_include_directories(tls_bridge_smoke_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_features(tls_bridge_smoke_test PRIVATE cxx_std_17)

if(WIN32)
    target_link_libraries(tls_bridge_smoke_test PRIVATE
        Threads::Threads
        user32 gdi32 shell32 ws2_32 crypt32
    )
else()
    target_link_libraries(tls_bridge_smoke_test PRIVATE 
        Threads::Threads
        ssl crypto
    )
endif()

message(STATUS "[TLSBridge] Smoke test target: tls_bridge_smoke_test (build with: cmake --build . --target tls_bridge_smoke_test)")

# ============================================================================
# SovereignAssembler encoding smoke (jmp fixup, ALU, movzx)
# ============================================================================
add_executable(sovereign_assembler_smoke EXCLUDE_FROM_ALL
    src/agentic/SovereignAssembler.cpp
    src/tests/sovereign_assembler_smoke.cpp
)
target_include_directories(sovereign_assembler_smoke PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(sovereign_assembler_smoke PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(sovereign_assembler_smoke PRIVATE /W3 /EHsc)
endif()
if(WIN32)
    target_link_libraries(sovereign_assembler_smoke PRIVATE kernel32)
endif()
if(MSVC AND DEFINED _RAWR_MSVC_LIBPATHS AND _RAWR_MSVC_LIBPATHS)
    target_link_directories(sovereign_assembler_smoke PRIVATE ${_RAWR_MSVC_LIBPATHS})
endif()
set_target_properties(sovereign_assembler_smoke PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME sovereign_assembler_smoke
)
# One-shot gate: build + run (same checks as CTest sovereign_assembler_smoke)
add_custom_target(sovereign_assembler_smoke_run
    COMMAND ${CMAKE_COMMAND} -E echo "[SovereignAssembler] Running PE encoding smoke..."
    COMMAND $<TARGET_FILE:sovereign_assembler_smoke>
    DEPENDS sovereign_assembler_smoke
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "[SovereignAssembler] sovereign_assembler_smoke_run (build+execute)"
    VERBATIM
)
message(STATUS "[SovereignAssembler] Smoke target: sovereign_assembler_smoke (cmake --build . --target sovereign_assembler_smoke)")
message(STATUS "[SovereignAssembler] Run gate: sovereign_assembler_smoke_run (cmake --build . --target sovereign_assembler_smoke_run)")

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

# ════════════════════════════════════════════════════════════════════
# ExtensionScheduler Integration Test
# Build: cmake --build build-ninja --target ExtensionScheduler_Integration_Test
# ════════════════════════════════════════════════════════════════════
add_executable(ExtensionScheduler_Integration_Test EXCLUDE_FROM_ALL
    tests/integration/ExtensionScheduler_Integration_Test.cpp
    src/extension_host/ExtensionSchedulerBridge.cpp
)
target_include_directories(ExtensionScheduler_Integration_Test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/extension_host
)
if(MSVC)
    target_compile_options(ExtensionScheduler_Integration_Test PRIVATE /EHsc /W3)
    target_compile_options(ExtensionScheduler_Integration_Test PRIVATE
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:RelWithDebInfo>:/O2>
    )
endif()
target_link_libraries(ExtensionScheduler_Integration_Test PRIVATE kernel32)
set_target_properties(ExtensionScheduler_Integration_Test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME ExtensionScheduler_Integration_Test
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
message(STATUS "[Integration Test] ExtensionScheduler_Integration_Test target added")

# ════════════════════════════════════════════════════════════════════
# Sovereign FP8 Quantizer Test
# Build: cmake --build build-ninja --target SovereignFP8_Test
# ════════════════════════════════════════════════════════════════════
add_executable(SovereignFP8_Test EXCLUDE_FROM_ALL
    tests/integration/test_sovereign_fp8.cpp
    src/kernels/sovereign_fp8_quantizer.cpp
    src/kernels/sovereign_fp8_quantizer.asm
)
target_include_directories(SovereignFP8_Test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/kernels
)
set_source_files_properties(src/kernels/sovereign_fp8_quantizer.asm PROPERTIES LANGUAGE ASM_MASM)
if(MSVC)
    target_compile_options(SovereignFP8_Test PRIVATE /EHsc /W3 /O2)
    target_compile_options(SovereignFP8_Test PRIVATE
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:RelWithDebInfo>:/O2>
    )
endif()
target_link_libraries(SovereignFP8_Test PRIVATE kernel32)
set_target_properties(SovereignFP8_Test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME SovereignFP8_Test
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
message(STATUS "[Sovereign Test] SovereignFP8_Test target added (RX 7800 XT high-throughput validation)")

# ════════════════════════════════════════════════════════════════════
# Token Pipeline TPS Measurement Harness
# Build: cmake --build build-ninja --target TPS_Measurement
# Run:   ./build-ninja/bin/TPS_Measurement.exe
# ════════════════════════════════════════════════════════════════════
add_executable(TPS_Measurement EXCLUDE_FROM_ALL
    tests/tps_measurement_harness.cpp
    src/inference/token_pipeline.cpp
)
target_include_directories(TPS_Measurement PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
if(MSVC)
    target_compile_options(TPS_Measurement PRIVATE /EHsc /W3 /O2 /std:c++20)
endif()
target_link_libraries(TPS_Measurement PRIVATE kernel32)
set_target_properties(TPS_Measurement PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME TPS_Measurement
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
message(STATUS "[TPS Harness] TPS_Measurement target added - produces first measurable throughput number")

# ════════════════════════════════════════════════════════════════════
# Simple TPS Measurement (No dependencies, pure SPSC queue)
# Build: cmake --build build-ninja --target TPS_Simple
# ════════════════════════════════════════════════════════════════════
add_executable(TPS_Simple EXCLUDE_FROM_ALL
    tests/tps_measurement_simple.cpp
)
if(MSVC)
    target_compile_options(TPS_Simple PRIVATE /EHsc /W3 /O2 /std:c++20)
endif()
target_link_libraries(TPS_Simple PRIVATE kernel32)
set_target_properties(TPS_Simple PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME TPS_Simple
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
message(STATUS "[TPS Simple] TPS_Simple target added - pure SPSC queue measurement")

# ════════════════════════════════════════════════════════════════════
# Minimal TPS Measurement (No dependencies)
# Build: cmake --build build-ninja --target TPS_Minimal
# ════════════════════════════════════════════════════════════════════
add_executable(TPS_Minimal EXCLUDE_FROM_ALL
    tests/tps_minimal.cpp
)
if(MSVC)
    target_compile_options(TPS_Minimal PRIVATE /EHsc /W3 /O2 /std:c++20)
endif()
target_link_libraries(TPS_Minimal PRIVATE kernel32)
set_target_properties(TPS_Minimal PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME TPS_Minimal
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
message(STATUS "[TPS Minimal] TPS_Minimal target added - produces first measurable throughput number")

# ════════════════════════════════════════════════════════════════════
# Fixed Token Pipeline Test (Deadlock-free SPSC)
# Build: cmake --build build-ninja --target TokenPipeline_Fixed
# ════════════════════════════════════════════════════════════════════
add_executable(TokenPipeline_Fixed EXCLUDE_FROM_ALL
    tests/token_pipeline_fixed.cpp
)
if(MSVC)
    target_compile_options(TokenPipeline_Fixed PRIVATE /EHsc /W3 /O2 /std:c++20)
endif()
target_link_libraries(TokenPipeline_Fixed PRIVATE kernel32)
set_target_properties(TokenPipeline_Fixed PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME TokenPipeline_Fixed
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
message(STATUS "[TokenPipeline Fixed] TokenPipeline_Fixed target added - deadlock-free SPSC queue")

# ════════════════════════════════════════════════════════════════════
# Hardened Pipeline with Speculative Decoder
# Build: cmake --build build-ninja --target Pipeline_Hardened
# ════════════════════════════════════════════════════════════════════
add_executable(Pipeline_Hardened EXCLUDE_FROM_ALL
    tests/pipeline_hardened_with_decoder.cpp
)
if(MSVC)
    target_compile_options(Pipeline_Hardened PRIVATE /EHsc /W3 /O2 /std:c++20)
endif()
target_link_libraries(Pipeline_Hardened PRIVATE kernel32)
set_target_properties(Pipeline_Hardened PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME Pipeline_Hardened
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
message(STATUS "[Pipeline Hardened] Pipeline_Hardened target added - backpressure + speculative decoder")

# ════════════════════════════════════════════════════════════════════
# 3-Stage Pipeline with Flow Control
# Build: cmake --build build-ninja --target Pipeline_3Stage
# ════════════════════════════════════════════════════════════════════
add_executable(Pipeline_3Stage EXCLUDE_FROM_ALL
    tests/pipeline_3stage_flow_control.cpp
)
if(MSVC)
    target_compile_options(Pipeline_3Stage PRIVATE /EHsc /W3 /O2 /std:c++20)
endif()
target_link_libraries(Pipeline_3Stage PRIVATE kernel32)
set_target_properties(Pipeline_3Stage PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME Pipeline_3Stage
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
message(STATUS "[Pipeline 3Stage] Pipeline_3Stage target added - 3-stage lock-free with flow control")

# ════════════════════════════════════════════════════════════════════
# End-to-End Pipeline + FP8 Quantization (Stage 3 Integration)
# Build: cmake --build build-ninja --target Pipeline_EndToEnd_FP8
# ════════════════════════════════════════════════════════════════════
add_executable(Pipeline_EndToEnd_FP8 EXCLUDE_FROM_ALL
    tests/pipeline_end_to_end_fp8.cpp
    src/kernels/sovereign_fp8_quantizer.cpp
    src/kernels/sovereign_fp8_quantizer.asm
)
if(MSVC)
    target_compile_options(Pipeline_EndToEnd_FP8 PRIVATE /EHsc /W3 /O2 /std:c++20)
endif()
target_include_directories(Pipeline_EndToEnd_FP8 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(Pipeline_EndToEnd_FP8 PRIVATE kernel32)
set_target_properties(Pipeline_EndToEnd_FP8 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME Pipeline_EndToEnd_FP8
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
message(STATUS "[Pipeline EndToEnd+FP8] Pipeline_EndToEnd_FP8 target added - 3-stage + FP8 Stage 3")

# ════════════════════════════════════════════════════════════════════
# FP8 Pipeline - Non-Blocking Egress (Fixes batch starvation deadlock)
# Build: cmake --build build-ninja --target Pipeline_FP8_NonBlocking
# ════════════════════════════════════════════════════════════════════
add_executable(Pipeline_FP8_NonBlocking EXCLUDE_FROM_ALL
    tests/pipeline_fp8_nonblocking.cpp
    src/kernels/sovereign_fp8_quantizer.cpp
    src/kernels/sovereign_fp8_quantizer.asm
)
if(MSVC)
    target_compile_options(Pipeline_FP8_NonBlocking PRIVATE /EHsc /W3 /O2 /std:c++20)
endif()
target_include_directories(Pipeline_FP8_NonBlocking PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(Pipeline_FP8_NonBlocking PRIVATE kernel32)
set_target_properties(Pipeline_FP8_NonBlocking PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME Pipeline_FP8_NonBlocking
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
message(STATUS "[Pipeline FP8 NonBlocking] Pipeline_FP8_NonBlocking target added - non-blocking egress with partial flush")

# ════════════════════════════════════════════════════════════════════
# FP8 AVX2 Pipeline Integration (8x throughput improvement)
# Build: cmake --build build-ninja --target Pipeline_FP8_AVX2
# ════════════════════════════════════════════════════════════════════
add_executable(Pipeline_FP8_AVX2 EXCLUDE_FROM_ALL
    tests/pipeline_fp8_avx2_integration.cpp
    src/kernels/fp8_avx2_interface.cpp
    src/kernels/sovereign_fp8_quantizer_avx2.asm
)
if(MSVC)
    # Only apply C++ flags to C++ sources, not ASM
    target_compile_options(Pipeline_FP8_AVX2 PRIVATE 
        $<$<COMPILE_LANGUAGE:CXX>:/EHsc /W3 /O2 /std:c++20 /arch:AVX2>
    )
endif()
target_include_directories(Pipeline_FP8_AVX2 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(Pipeline_FP8_AVX2 PRIVATE kernel32)
set_target_properties(Pipeline_FP8_AVX2 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME Pipeline_FP8_AVX2
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
message(STATUS "[Pipeline FP8 AVX2] Pipeline_FP8_AVX2 target added - AVX2 vectorized FP8 (8x throughput)")

# ════════════════════════════════════════════════════════════════════
# FP8 AVX2 Hardened Pipeline (Production Flow Control)
# Build: cmake --build build-ninja --target Pipeline_FP8_AVX2_Hardened
# ════════════════════════════════════════════════════════════════════
add_executable(Pipeline_FP8_AVX2_Hardened EXCLUDE_FROM_ALL
    tests/pipeline_fp8_avx2_hardened.cpp
    src/kernels/fp8_avx2_interface.cpp
    src/kernels/sovereign_fp8_quantizer_avx2.asm
)
if(MSVC)
    target_compile_options(Pipeline_FP8_AVX2_Hardened PRIVATE 
        $<$<COMPILE_LANGUAGE:CXX>:/EHsc /W3 /O2 /std:c++20 /arch:AVX2>
    )
endif()
target_include_directories(Pipeline_FP8_AVX2_Hardened PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(Pipeline_FP8_AVX2_Hardened PRIVATE kernel32)
set_target_properties(Pipeline_FP8_AVX2_Hardened PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME Pipeline_FP8_AVX2_Hardened
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
message(STATUS "[Pipeline FP8 AVX2 Hardened] Production flow control with <5% drop rate target")

# ════════════════════════════════════════════════════════════════════
# FP8 AVX2 Synchronized Pipeline (End-to-End Rate Matching)
# Build: cmake --build build-ninja --target Pipeline_FP8_AVX2_Sync
# ════════════════════════════════════════════════════════════════════
add_executable(Pipeline_FP8_AVX2_Sync EXCLUDE_FROM_ALL
    tests/pipeline_fp8_avx2_sync.cpp
    src/kernels/fp8_avx2_interface.cpp
    src/kernels/sovereign_fp8_quantizer_avx2.asm
)
if(MSVC)
    target_compile_options(Pipeline_FP8_AVX2_Sync PRIVATE 
        $<$<COMPILE_LANGUAGE:CXX>:/EHsc /W3 /O2 /std:c++20 /arch:AVX2>
    )
endif()
target_include_directories(Pipeline_FP8_AVX2_Sync PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(Pipeline_FP8_AVX2_Sync PRIVATE kernel32)
set_target_properties(Pipeline_FP8_AVX2_Sync PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME Pipeline_FP8_AVX2_Sync
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
message(STATUS "[Pipeline FP8 AVX2 Sync] End-to-end rate matching with blocking backpressure")

# ════════════════════════════════════════════════════════════════════
# FP8 AVX2 Stable Pipeline (Minimal Working Version)
# Build: cmake --build build-ninja --target Pipeline_FP8_AVX2_Stable
# ════════════════════════════════════════════════════════════════════
add_executable(Pipeline_FP8_AVX2_Stable EXCLUDE_FROM_ALL
    tests/pipeline_fp8_avx2_stable.cpp
    src/kernels/fp8_avx2_interface.cpp
    src/kernels/sovereign_fp8_quantizer_avx2.asm
)
if(MSVC)
    target_compile_options(Pipeline_FP8_AVX2_Stable PRIVATE 
        $<$<COMPILE_LANGUAGE:CXX>:/EHsc /W3 /O2 /std:c++20 /arch:AVX2>
    )
endif()
target_include_directories(Pipeline_FP8_AVX2_Stable PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(Pipeline_FP8_AVX2_Stable PRIVATE kernel32)
set_target_properties(Pipeline_FP8_AVX2_Stable PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME Pipeline_FP8_AVX2_Stable
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
message(STATUS "[Pipeline FP8 AVX2 Stable] Minimal working version with proper flow control")

# ════════════════════════════════════════════════════════════════════
# FP8 Scalar vs AVX2 Benchmark (Direct comparison)
# Build: cmake --build build-ninja --target FP8_ScalarVsAVX2
# ════════════════════════════════════════════════════════════════════
add_executable(FP8_ScalarVsAVX2 EXCLUDE_FROM_ALL
    tests/fp8_scalar_vs_avx2_bench.cpp
    src/kernels/fp8_avx2_interface.cpp
    src/kernels/sovereign_fp8_quantizer_avx2.asm
)
if(MSVC)
    target_compile_options(FP8_ScalarVsAVX2 PRIVATE 
        $<$<COMPILE_LANGUAGE:CXX>:/EHsc /W3 /O2 /std:c++20 /arch:AVX2>
    )
endif()
target_include_directories(FP8_ScalarVsAVX2 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(FP8_ScalarVsAVX2 PRIVATE kernel32)
set_target_properties(FP8_ScalarVsAVX2 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME FP8_ScalarVsAVX2
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
message(STATUS "[FP8 Scalar vs AVX2] Direct performance comparison benchmark")

# ════════════════════════════════════════════════════════════════════
# Fused Decode + FP8 Benchmark (Memory Bandwidth Optimization)
# Build: cmake --build build-ninja --target FusedDecodeFP8_Bench
# ════════════════════════════════════════════════════════════════════
add_executable(FusedDecodeFP8_Bench EXCLUDE_FROM_ALL
    tests/benchmark_fused_decode_fp8.cpp
    src/kernels/fp8_avx2_interface.cpp
    src/kernels/sovereign_fp8_quantizer_avx2.asm
    src/kernels/sovereign_fused_decode_fp8.asm
)
if(MSVC)
    target_compile_options(FusedDecodeFP8_Bench PRIVATE 
        $<$<COMPILE_LANGUAGE:CXX>:/EHsc /W3 /O2 /std:c++20 /arch:AVX2>
    )
endif()
target_include_directories(FusedDecodeFP8_Bench PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(FusedDecodeFP8_Bench PRIVATE kernel32)
set_target_properties(FusedDecodeFP8_Bench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME FusedDecodeFP8_Bench
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
message(STATUS "[Fused Decode + FP8] Kernel fusion benchmark - 69% memory bandwidth reduction")

# ════════════════════════════════════════════════════════════════════
# Adaptive Speculation Governor (Self-Tuning Amplification Control)
# Build: cmake --build build-ninja --target Pipeline_AdaptiveGovernor
# ════════════════════════════════════════════════════════════════════
add_executable(Pipeline_AdaptiveGovernor EXCLUDE_FROM_ALL
    tests/pipeline_adaptive_speculation_governor.cpp
    src/kernels/fp8_avx2_interface.cpp
    src/kernels/sovereign_fp8_quantizer_avx2.asm
)
if(MSVC)
    target_compile_options(Pipeline_AdaptiveGovernor PRIVATE 
        $<$<COMPILE_LANGUAGE:CXX>:/EHsc /W3 /O2 /std:c++20 /arch:AVX2>
    )
endif()
target_include_directories(Pipeline_AdaptiveGovernor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(Pipeline_AdaptiveGovernor PRIVATE kernel32)
set_target_properties(Pipeline_AdaptiveGovernor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME Pipeline_AdaptiveGovernor
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
message(STATUS "[Pipeline Adaptive Governor] Self-tuning speculation targeting 85% success rate")

# ════════════════════════════════════════════════════════════════════
# FP8 AVX2 Credit-Based Flow Control
# Build: cmake --build build-ninja --target Pipeline_FP8_AVX2_Credit
# ════════════════════════════════════════════════════════════════════
add_executable(Pipeline_FP8_AVX2_Credit EXCLUDE_FROM_ALL
    tests/pipeline_fp8_avx2_credit.cpp
    src/kernels/fp8_avx2_interface.cpp
    src/kernels/sovereign_fp8_quantizer_avx2.asm
)
if(MSVC)
    target_compile_options(Pipeline_FP8_AVX2_Credit PRIVATE 
        $<$<COMPILE_LANGUAGE:CXX>:/EHsc /W3 /O2 /std:c++20 /arch:AVX2>
    )
endif()
target_include_directories(Pipeline_FP8_AVX2_Credit PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(Pipeline_FP8_AVX2_Credit PRIVATE kernel32)
set_target_properties(Pipeline_FP8_AVX2_Credit PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME Pipeline_FP8_AVX2_Credit
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
message(STATUS "[Pipeline FP8 AVX2 Credit] Explicit credit-based flow control")

# ════════════════════════════════════════════════════════════════════
# FP8 AVX2 Credit-Based Flow Control (Fixed)
# Build: cmake --build build-ninja --target Pipeline_FP8_AVX2_Credit_Fixed
# ════════════════════════════════════════════════════════════════════
add_executable(Pipeline_FP8_AVX2_Credit_Fixed EXCLUDE_FROM_ALL
    tests/pipeline_fp8_avx2_credit_fixed.cpp
    src/kernels/fp8_avx2_interface.cpp
    src/kernels/sovereign_fp8_quantizer_avx2.asm
)
if(MSVC)
    target_compile_options(Pipeline_FP8_AVX2_Credit_Fixed PRIVATE 
        $<$<COMPILE_LANGUAGE:CXX>:/EHsc /W3 /O2 /std:c++20 /arch:AVX2>
    )
endif()
target_include_directories(Pipeline_FP8_AVX2_Credit_Fixed PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(Pipeline_FP8_AVX2_Credit_Fixed PRIVATE kernel32)
set_target_properties(Pipeline_FP8_AVX2_Credit_Fixed PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME Pipeline_FP8_AVX2_Credit_Fixed
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
message(STATUS "[Pipeline FP8 AVX2 Credit Fixed] Working credit-based flow control")

# ════════════════════════════════════════════════════════════════════
# FP8 AVX2 Windowed Credit Pipeline (Amortized Streaming Model)
# Build: cmake --build build-ninja --target Pipeline_FP8_AVX2_Windowed
# ════════════════════════════════════════════════════════════════════
add_executable(Pipeline_FP8_AVX2_Windowed EXCLUDE_FROM_ALL
    tests/pipeline_fp8_avx2_windowed.cpp
    src/kernels/fp8_avx2_interface.cpp
    src/kernels/sovereign_fp8_quantizer_avx2.asm
)
if(MSVC)
    target_compile_options(Pipeline_FP8_AVX2_Windowed PRIVATE 
        $<$<COMPILE_LANGUAGE:CXX>:/EHsc /W3 /O2 /std:c++20 /arch:AVX2>
    )
endif()
target_include_directories(Pipeline_FP8_AVX2_Windowed PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(Pipeline_FP8_AVX2_Windowed PRIVATE kernel32)
set_target_properties(Pipeline_FP8_AVX2_Windowed PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME Pipeline_FP8_AVX2_Windowed
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
message(STATUS "[Pipeline FP8 AVX2 Windowed] Amortized credit system with 4096-token windows")

# ════════════════════════════════════════════════════════════════════
# Hybrid Credit Governor Pipeline (Deterministic Safety + Elastic Throughput)
# Build: cmake --build build-ninja --target Pipeline_Hybrid_Credit
# ════════════════════════════════════════════════════════════════════
add_executable(Pipeline_Hybrid_Credit EXCLUDE_FROM_ALL
    tests/pipeline_hybrid_credit_governor.cpp
    src/kernels/fp8_avx2_interface.cpp
    src/kernels/sovereign_fp8_quantizer_avx2.asm
)
if(MSVC)
    target_compile_options(Pipeline_Hybrid_Credit PRIVATE 
        $<$<COMPILE_LANGUAGE:CXX>:/EHsc /W3 /O2 /std:c++20 /arch:AVX2>
    )
endif()
target_include_directories(Pipeline_Hybrid_Credit PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(Pipeline_Hybrid_Credit PRIVATE kernel32)
set_target_properties(Pipeline_Hybrid_Credit PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME Pipeline_Hybrid_Credit
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
message(STATUS "[Pipeline Hybrid Credit] Hybrid credit governor - deterministic + elastic")

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/CMakeLists.txt")
    enable_testing()
    add_subdirectory(tests)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/src/tools/CMakeLists.txt")
    add_subdirectory(src/tools)

# ════════════════════════════════════════════════════════════════════
# SWE-bench evaluation harness (Gate 5)
# Build: cmake --build build-ninja --target RawrXD-SWEBench
# Run:   bin/RawrXD-SWEBench.exe [--json report.json] [--run-tests]
# ════════════════════════════════════════════════════════════════════
add_executable(RawrXD-SWEBench EXCLUDE_FROM_ALL
    src/eval/swe_bench_harness.cpp
    src/core/context_config.cpp
)
target_include_directories(RawrXD-SWEBench PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
if(MSVC)
    target_compile_options(RawrXD-SWEBench PRIVATE /EHsc /W3)
    target_compile_options(RawrXD-SWEBench PRIVATE
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:RelWithDebInfo>:/O2>
    )
endif()
target_link_libraries(RawrXD-SWEBench PRIVATE kernel32 winhttp)
set_target_properties(RawrXD-SWEBench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME RawrXD-SWEBench
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
message(STATUS "[Eval] RawrXD-SWEBench target added (Gate 5 — pass@1 benchmark harness)")
endif()

# ============================================================================
# RawrXD-Serve — Zero-dependency Ollama replacement (httpapi.lib)
# ============================================================================
add_executable(RawrXD-Serve EXCLUDE_FROM_ALL
    src/serve/rawrxd_serve_main.cpp
    src/serve/rawrxd_serve.cpp
    src/serve/rawrxd_model_registry.cpp
    src/serve/rawrxd_serve_inference_plugin.cpp
)
target_include_directories(RawrXD-Serve PRIVATE
    ${CMAKE_SOURCE_DIR}/src/serve
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)
if(MSVC)
    # Match static runtime flavor with configuration to avoid LNK2038 when
    # dependent libs are built as MTd in Debug.
    target_compile_options(RawrXD-Serve PRIVATE /EHsc /W3 /MT$<$<CONFIG:Debug>:d>)
    target_compile_options(RawrXD-Serve PRIVATE
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:RelWithDebInfo>:/O2>
    )
endif()
target_link_libraries(RawrXD-Serve PRIVATE
    kernel32 httpapi ws2_32
    rawrxd_gpu_gate
)
set_target_properties(RawrXD-Serve PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME rawrxd
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
if(TARGET RawrXD-ServeInference)
    add_dependencies(RawrXD-Serve RawrXD-ServeInference)
endif()
message(STATUS "[Serve] RawrXD-Serve target added (Ollama-compatible HTTP server + CLI)")

# ============================================================================
# RawrXD-ParityUIDriver — Headless UI-side driver for CLI/UI parity diff.
# Calls runLocalInferencePipeline() the same way Win32IDE.cpp does, with no
# WinMain / message loop. Lets the UI half of the parity proof run in CI.
# ============================================================================
add_executable(RawrXD-ParityUIDriver EXCLUDE_FROM_ALL
    src/tools/parity_ui_driver_main.cpp
    src/core/rawr_inference_pipeline.cpp
    src/serve/rawrxd_serve_inference_plugin.cpp
)
target_include_directories(RawrXD-ParityUIDriver PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/serve
    ${CMAKE_SOURCE_DIR}/include
)
if(MSVC)
    target_compile_options(RawrXD-ParityUIDriver PRIVATE /EHsc /W3 /MT$<$<CONFIG:Debug>:d>)
    target_compile_options(RawrXD-ParityUIDriver PRIVATE
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:RelWithDebInfo>:/O2>
    )
endif()
target_link_libraries(RawrXD-ParityUIDriver PRIVATE
    kernel32
    rawrxd_gpu_gate
)
set_target_properties(RawrXD-ParityUIDriver PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME rawrxd-parity-ui-driver
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
message(STATUS "[Parity] RawrXD-ParityUIDriver target added (headless UI-side parity driver)")
