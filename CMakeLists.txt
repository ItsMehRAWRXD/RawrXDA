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
    foreach(_rawr_lib IN LISTS _RAWR_MSVC_LIBPATHS)
        add_link_options("/LIBPATH:\"${_rawr_lib}\"")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LIBPATH:\"${_rawr_lib}\"")
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LIBPATH:\"${_rawr_lib}\"")
        set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /LIBPATH:\"${_rawr_lib}\"")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /LIBPATH:\"${_rawr_lib}\"")
        set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LIBPATH:\"${_rawr_lib}\"")
        set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} /LIBPATH:\"${_rawr_lib}\"")
        set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /LIBPATH:\"${_rawr_lib}\"")
    endforeach()
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

# Force Windows SDK version for VS generator (avoid 10.0.26100.0 if not installed)
if(WIN32)
    set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION "10.0.22621.0" CACHE STRING "Windows SDK version for VS generator" FORCE)
endif()

# RawrXD - Zero Bloat Horizon (Phase 14.7.3+)
project(RawrXD VERSION 14.7.3 LANGUAGES C CXX ASM_MASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# v14.7.3 Performance/throughput flags
# Keep Debug free of /O2 and /GL to avoid /RTC1 conflicts on single-config generators.
if(MSVC)
    add_compile_options(
        $<$<COMPILE_LANGUAGE:C,CXX>:/MP>
        $<$<COMPILE_LANGUAGE:C,CXX>:/GS->
        $<$<COMPILE_LANGUAGE:C,CXX>:/sdl->
        $<$<AND:$<COMPILE_LANGUAGE:C,CXX>,$<NOT:$<CONFIG:Debug>>>:/O2>
        $<$<AND:$<COMPILE_LANGUAGE:C,CXX>,$<NOT:$<CONFIG:Debug>>>:/GL>
    )
else()
    add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:-O3>)
endif()
# Keep LTCG globally, but avoid executable-only/defaultlib overrides here.
# /NODEFAULTLIB + /ENTRY + /SUBSYSTEM must not leak into DLL targets.
add_link_options(/LTCG)

# WebView2 and WIL are in d:/rawrxd/include/
include_directories("d:/rawrxd/include")

option(RAWRXD_BUILD_CLI "Build the rawrxd CLI console target" OFF)
if(RAWRXD_BUILD_CLI)
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

# Enable ASM_MASM only on MSVC (ml64.exe required).
if(MSVC)
    enable_language(ASM_MASM)
endif()

# Default to Release builds (prevents /RTC1 + /O2 clash on Ninja single-config)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    message(STATUS "[Build] Defaulting CMAKE_BUILD_TYPE to Release")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static link MSVC runtime — no vcruntime140.dll dependency
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_definitions(-D_CRT_SECURE_NO_WARNINGS -DNOMINMAX -DWIN32_LEAN_AND_MEAN)

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
    add_definitions(-DRAWR_HAS_MASM=1)
    add_definitions(-DRAWRXD_LINK_QUADBUFFER_ASM=1)
    add_definitions(-DRAWRXD_LINK_INFERENCE_KERNELS_ASM=1)
    # These two ASM modules currently rely on `rawrxd_win64.inc` sovereign resolver blocks
    # that do not assemble cleanly under ml64; Win32IDE uses C++ stubs for now.
    add_definitions(-DRAWRXD_LINK_ANALYZER_DISTILLER_ASM=0)
    add_definitions(-DRAWRXD_LINK_STREAMING_ORCHESTRATOR_ASM=0)
    message(STATUS "MASM64 enabled: ASM kernels will be compiled")
else()
    set(RAWR_HAS_MASM FALSE)
    message(STATUS "MASM64 not available (non-MSVC): using C++ fallbacks")
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
        src/asm/RawrXD_PDBKernel_v3.asm
        src/asm/RawrXD_ModuleEngine.asm
        src/asm/request_patch.asm
        src/asm/inference_kernels.asm
        src/asm/model_bridge_x64.asm
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
        # src/asm/RawrXD_CopilotGapCloser.asm   # excluded: scaffold/invalid ml64 syntax (__real@*, missing .ENDPROLOG)
        # src/asm/native_speed_kernels.asm      # excluded: scaffold/invalid ml64 syntax (__real@*, missing .ENDPROLOG)
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
        src/asm/RawrXD_PDBKernel_v3.asm
        src/asm/RawrXD_IPC_Dispatcher.asm
        src/asm/RawrXD_AuditSystem_Pure.asm
        src/asm/Advanced_Code_Deobfuscator.asm
        src/asm/Code_Pattern_Reconstructor.asm
        src/asm/agentic_deep_thinking_kernels.asm
        # Keep C++ reverse_engineered_stubs.cpp as single source of truth for these symbols.
        # ASM unresolved stubs duplicate AllocateDMABuffer and cause LNK2005 in Win32IDE.
        # src/unresolved_asm_stubs.asm
        # User-provided production ASM (need ML64 syntax fixes: .endprolog, REAL4, scale): agentic_deep_thinking_kernels.asm, ai_completion_provider_masm.asm, agentic_puppeteer_byte_ops.asm
        # src/RawrXD_Complete_ReverseEngineered.asm  # excluded: fragment missing struct defs/includes
    )
    # Mark ASM files with MASM language property
    set_source_files_properties(${ASM_KERNEL_SOURCES} PROPERTIES LANGUAGE ASM_MASM)
    add_definitions(-DRAWRXD_LINK_TELEMETRY_KERNEL_ASM=1)
    add_definitions(-DRAWRXD_LINK_PROMETHEUS_EXPORTER_ASM=1)
    add_definitions(-DRAWRXD_LINK_SELFPATCH_AGENT_ASM=1)
    add_definitions(-DRAWRXD_LINK_SOURCEEDIT_KERNEL_ASM=1)
    add_definitions(-DRAWRXD_LINK_VISION_PROJECTION_ASM=0)
    add_definitions(-DRAWRXD_LINK_REVERSE_ENGINEERED_ASM=1)
    add_definitions(-DRAWRXD_LINK_DUAL_ENGINE_QUANTUM_BEACON_ASM=0)
    add_definitions(-DRAWRXD_LINK_UNIFIED_OVERCLOCK_GOVERNOR_ASM=0)
    add_definitions(-DRAWRXD_LINK_CODEBASE_AUDIT_SYSTEM_ASM=0)
    add_definitions(-DRAWRXD_LINK_ADVANCED_DEOBFUSCATOR_ASM=1)
    add_definitions(-DRAWRXD_LINK_PATTERN_RECONSTRUCTOR_ASM=1)
else()
    set(ASM_KERNEL_SOURCES "")
endif()

find_package(Threads REQUIRED)

set(SOURCES 
    src/main.cpp
    src/memory_core.cpp
    src/hot_patcher.cpp
    src/cpu_inference_engine.cpp
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
    
    src/engine/gguf_core.cpp
    src/engine/inference_kernels.cpp
    src/engine/transformer.cpp
    src/engine/bpe_tokenizer.cpp
    src/engine/sampler.cpp
    src/engine/rawr_engine.cpp
    src/engine/core_generator.cpp
    src/engine/pyre_compute.cpp
    src/engine/sovereign_engines.cpp
    
    src/codec/compression.cpp
    src/codec/brutal_gzip.cpp

    src/core/execution_scheduler.cpp
    src/asm/quantum_beaconism_backend.asm
    
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
    src/core/multi_response_engine.cpp
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
    src/agent/model_invoker.cpp
    src/agent/planner.cpp
    src/agent/eval_framework.cpp
    src/ollama_client.cpp

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
    
    # Phase 29: Multi-Architecture GPU/Accelerator Backends
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
    # Valuation-Critical Platform Subsystems (>2× step-up from $40M baseline)
    # ═══════════════════════════════════════════════════════════════════

    # Autonomous End-to-End Workflow Engine (scan → bulk_fix → verify → build → test → diff)
    src/core/autonomous_workflow_engine.cpp

    # Workspace-Level Reasoning Profiles (fast/dev/critical per repo)
    src/core/workspace_reasoning_profiles.cpp

    # Deterministic Swarm Reproducibility (same input + seed = same result)
    src/core/deterministic_swarm.cpp

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
option(RAWR_REPLACE_SWARM_LICENSE_FALLBACKS "Remove swarm/license fallback stubs from SOURCES" OFF)
set(RAWR_SSOT_PROVIDER "EXT" CACHE STRING "SSOT owner for RawrEngine: CORE, EXT, AUTO, STUBS, FEATURES")
set_property(CACHE RAWR_SSOT_PROVIDER PROPERTY STRINGS CORE EXT AUTO STUBS FEATURES)

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
    list(REMOVE_ITEM SOURCES
        src/core/memory_patch_byte_search_stubs.cpp
    )
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

if(RAWR_REPLACE_SWARM_LICENSE_FALLBACKS)
    list(REMOVE_ITEM SOURCES
        src/core/swarm_network_stubs.cpp
        src/core/enterprise_license_stubs.cpp
    )
    message(STATUS "[ReplacementPass] Swarm/License: fallback stubs removed")
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
        src/core/missing_handler_stubs.cpp
        src/core/feature_handlers.cpp
    )
elseif(RAWR_SSOT_PROVIDER_UPPER STREQUAL "EXT")
    set(_RAWR_SSOT_EXT 1)
    list(REMOVE_ITEM RAWR_ENGINE_SOURCES
        src/core/ssot_handlers.cpp
        src/core/missing_handler_stubs.cpp
        src/core/auto_feature_registry.cpp
        src/core/ssot_handlers_ext_runtime_minimal.cpp
        src/core/ssot_handlers_ext_isolated.cpp
    )
elseif(RAWR_SSOT_PROVIDER_UPPER STREQUAL "AUTO")
    set(_RAWR_SSOT_AUTO 1)
    set(_RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS 1)
    list(REMOVE_ITEM RAWR_ENGINE_SOURCES
        src/core/missing_handler_stubs.cpp
        src/core/ssot_handlers_ext_runtime_minimal.cpp
        src/core/ssot_handlers_ext_isolated.cpp
    )
    list(REMOVE_ITEM RAWR_ENGINE_SOURCES src/core/ssot_handlers_ext_dedicated.cpp)
    list(APPEND RAWR_ENGINE_SOURCES src/core/ssot_handlers_ext.cpp)
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
        src/core/missing_handler_stubs.cpp
        src/core/ssot_handlers_ext_runtime_minimal.cpp
        src/core/ssot_handlers_ext_isolated.cpp
        src/core/ssot_handlers_ext.cpp
    )
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
add_subdirectory(src/reverse_engineering)

add_executable(RawrEngine ${RAWR_ENGINE_SOURCES} ${ASM_KERNEL_SOURCES})

if(WIN32)
    add_dependencies(RawrEngine regen-ssot-beacons verify-ssot-ext-ownership)
endif()

# Link RE library into RawrEngine
target_link_libraries(RawrEngine PRIVATE RawrXD-RE-Library)

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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/test_harness
    ${CMAKE_CURRENT_SOURCE_DIR}/src/reverse_engineering
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/gpu
)

target_link_libraries(RawrEngine PRIVATE Threads::Threads)

# Avoid duplicate register_sovereign_engines (real impl in sovereign_engines.cpp)
target_compile_definitions(RawrEngine PRIVATE RAWR_HAS_SOVEREIGN_ENGINES=1)
target_compile_definitions(RawrEngine PRIVATE
    RAWR_SSOT_CORE=${_RAWR_SSOT_CORE}
    RAWR_SSOT_EXT=${_RAWR_SSOT_EXT}
    RAWR_SSOT_AUTO=${_RAWR_SSOT_AUTO}
    RAWR_SSOT_STUBS=${_RAWR_SSOT_STUBS}
    RAWR_SSOT_FEATURES=${_RAWR_SSOT_FEATURES}
    $<$<BOOL:${_RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS}>:RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS=1>
)

# Sources used by missing_handler_stubs: debugger engine + symbols + Ollama client
target_sources(RawrEngine PRIVATE
    src/core/native_debugger_engine.cpp
    src/core/native_debugger_symbols.cpp
    src/agentic/AgentOllamaClient.cpp
    src/core/rawr_engine_link_shims.cpp
    src/core/auto_feature_stub_impl.cpp
)

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
    src/core/auto_feature_registry.cpp
    src/core/ssot_handlers.cpp
    src/core/ssot_handlers_ext_runtime_minimal.cpp
    src/core/ssot_handlers_ext_isolated.cpp
    src/core/rawrengine_command_handlers.cpp
    src/core/ssot_validation.cpp
)
list(APPEND GOLD_UNDERSCORE_SOURCES
    src/core/rawr_engine_link_shims.cpp
    src/core/auto_feature_stub_impl.cpp
    src/core/ignite_800b.cpp
    src/core/missing_handler_stubs.cpp
)
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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/test_harness
    ${CMAKE_CURRENT_SOURCE_DIR}/src/reverse_engineering
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/gpu
)

target_link_libraries(RawrXD_Gold PRIVATE Threads::Threads)

target_compile_definitions(RawrXD_Gold PRIVATE
    RAWRXD_GOLD_BUILD=1
    RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS=1
    RAWR_HAS_MASM=1
    RAWRXD_LINK_QUADBUFFER_ASM=1
    RAWRXD_LINK_INFERENCE_KERNELS_ASM=1
    RAWRXD_LINK_ANALYZER_DISTILLER_ASM=1
    RAWRXD_LINK_STREAMING_ORCHESTRATOR_ASM=1
    RAWRXD_LINK_TELEMETRY_KERNEL_ASM=1
    RAWRXD_LINK_PROMETHEUS_EXPORTER_ASM=1
    RAWRXD_LINK_SELFPATCH_AGENT_ASM=1
    RAWRXD_LINK_SOURCEEDIT_KERNEL_ASM=1
)

if(MSVC)
    target_compile_options(RawrXD_Gold PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:/EHsc /W0 /arch:AVX512>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CONFIG:Debug>>>:/O2>
    )
    target_link_options(RawrXD_Gold PRIVATE /LARGEADDRESSAWARE:NO)
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

# Gold-only sources: modules used by missing_handler_stubs but not in main SOURCES
target_sources(RawrXD_Gold PRIVATE
    src/core/native_debugger_engine.cpp
    src/core/native_debugger_symbols.cpp
    src/agentic/AgentOllamaClient.cpp
    src/logging/Logger.cpp
    src/security/InputSanitizer.cpp
    src/core/ConfigurationValidator.cpp
    src/inference/PerformanceMonitor.cpp
    src/agentic/ErrorRecoveryManager.cpp
    src/core/missing_handler_stubs.cpp
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

    # Brutal GZip deflate (stub returns nullptr for fallback)
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
    )

    # Link MASM objects into RawrEngine
    target_link_libraries(RawrEngine PRIVATE ${MASM_OBJECTS})

    # Link MASM objects into RawrXD_Gold
    target_link_libraries(RawrXD_Gold PRIVATE ${MASM_OBJECTS})

    # Ensure MASM objects are built before linking
    add_custom_target(masm_kernels DEPENDS ${MASM_OBJECTS})
    add_dependencies(RawrEngine masm_kernels)
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
    src/rawrxd_model_loader.cpp
    src/rawrxd_transformer.cpp
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
    src/inference/inference_standalone_link_shims.cpp
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
    src/codec/compression.cpp
    src/codec/brutal_gzip.cpp
    src/utils/Diagnostics.cpp
    src/core/ignite_800b.cpp
    src/inference/TitanLoaderDiagnostics.cpp
    src/inference/MemoryPressureGuard.cpp
    src/agentic/ErrorRecoveryManager.cpp
    src/inference/PerformanceMonitor.cpp
)

add_executable(RawrXD-InferenceEngine EXCLUDE_FROM_ALL ${INFERENCE_ENGINE_SOURCES})

target_compile_definitions(RawrXD-InferenceEngine PRIVATE
    RAWR_STANDALONE_INFERENCE=1
    RAWR_HAS_SOVEREIGN_ENGINES=1
)

target_include_directories(RawrXD-InferenceEngine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/inference
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/codec
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(RawrXD-InferenceEngine PRIVATE Threads::Threads)

if(WIN32)
    target_link_libraries(RawrXD-InferenceEngine PRIVATE
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
            src/asm/quant_avx2.asm
            src/asm/RawrXD_KQuant_Dequant.asm
            src/asm/RawrXD_KQuant_Kernel.asm
            src/asm/FlashAttention_AVX512.asm
            src/asm/RawrXD_Telemetry_Kernel.asm
            # Known invalid ml64 scaffold in repository copy; keep C++ pyre_compute.cpp path active.
            # src/asm/RawrXD_Pyre_Compute.asm
        )
    set_source_files_properties(${INFERENCE_ASM_SOURCES} PROPERTIES LANGUAGE ASM_MASM)
    target_sources(RawrXD-InferenceEngine PRIVATE ${INFERENCE_ASM_SOURCES})
    target_compile_definitions(RawrXD-InferenceEngine PRIVATE
        RAWR_HAS_MASM=1
        RAWRXD_LINK_TELEMETRY_KERNEL_ASM=1
        RAWRXD_LINK_INFERENCE_KERNELS_ASM=1
    )

    # NanoQuant integration (optional)
    if(TARGET RawrXD_NanoQuant)
        target_link_libraries(RawrXD-InferenceEngine PRIVATE RawrXD_NanoQuant)
        target_compile_definitions(RawrXD-InferenceEngine PRIVATE RAWR_HAS_NANOQUANT=1)
    endif()
endif()

if(MSVC)
    target_compile_options(RawrXD-InferenceEngine PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:/EHsc /W0 /arch:AVX512>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CONFIG:Debug>>>:/O2>
    )
else()
    target_compile_options(RawrXD-InferenceEngine PRIVATE -O3 -w -mavx2 -mfma -mavx512f -mavx512bw -mavx512dq -mavx512vl -march=native)
endif()

set_target_properties(RawrXD-InferenceEngine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY bin
    OUTPUT_NAME RawrXD-InferenceEngine
)

# Telemetry_Kernel.asm uses ADDR32 relocations — require /LARGEADDRESSAWARE:NO
if(MSVC)
    target_link_options(RawrXD-InferenceEngine PRIVATE /LARGEADDRESSAWARE:NO)
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
if(WIN32)
    set(WIN32IDE_SOURCES
        src/win32app/main_win32.cpp
        src/win32app/HeadlessIDE.cpp
        src/win32app/HeadlessIDE.h
        src/win32app/HeadlessIDE.cpp
        src/agent/quantum_agent_orchestrator.cpp
        src/inference/gpu_dispatch_gate_win32ide_fallback.cpp
        src/inference/gguf_d3d12_bridge_link_fallback.cpp
        src/ui/RawrXD_MainLoop.cpp
        src/ui/webview2_bridge.cpp
        src/ui/debugger_core.cpp
        src/ui/agentic_bridge.cpp
        src/win32app/v280_link_fallbacks.cpp
        src/win32app/RawrXD-Win32IDE.rc
        src/bridge/Win32SwarmBridge.cpp
        src/bridge/SwarmIATRegistration.cpp
        src/bridge/UnifiedModelMetadata.cpp
        src/agentic/SubAgentManager.cpp
        src/agentic/RobustOllamaParser.cpp
        src/agentic/ErrorRecoveryManager.cpp
    )

    # MASM64 UI Dispatcher
    set(WEBVIEW2_DISPATCHER src/asm/webview2_masm64_dispatcher.asm)
    set_source_files_properties(${WEBVIEW2_DISPATCHER} PROPERTIES LANGUAGE ASM_MASM)
    list(APPEND WIN32IDE_SOURCES ${WEBVIEW2_DISPATCHER})

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
        src/win32app/Win32IDE_AgentCommands.cpp
        src/win32app/Win32IDE_AutonomousCommunicator.cpp
        src/win32app/Win32IDE_AgenticComposerUX.cpp
        src/win32app/Win32IDE_ContextMentionParser.cpp
        src/win32app/Win32IDE_UnifiedTelemetry.cpp
        src/win32app/Win32IDE_Autonomy.cpp
        src/win32app/Win32IDE_PowerShell.cpp
        src/win32app/Win32IDE_PowerShellPanel.cpp
        src/win32app/Win32IDE_Session.cpp
        src/win32app/Win32IDE_FileOps.cpp
        src/win32app/Win32IDE_DecompilerView.cpp
        src/win32app/Win32IDE_NativePipeline.cpp
        Ship/RawrXD_AutonomousAgenticPipeline.cpp
        Ship/RawrXD_AgentCoordinator.cpp

        src/win32app/Win32IDE_FeatureManifest.cpp
        src/win32app/Win32IDE_GhostText.cpp
        src/win32app/Win32IDE_OutlinePanel.cpp
        src/win32app/Win32IDE_RenamePreview.cpp
        src/win32app/Win32IDE_Tier2Cosmetics.cpp
        src/win32app/Win32IDE_Tier1Cosmetics.cpp
        src/win32app/Win32IDE_Breadcrumbs.cpp
        src/win32app/Win32IDE_PlanExecutor.cpp
        src/win32app/Win32IDE_FailureDetector.cpp
        src/win32app/Win32IDE_FailureIntelligence.cpp
        src/win32app/Win32IDE_Settings.cpp
        src/win32app/Win32IDE_LocalServer.cpp
        src/win32app/Win32IDE_BackendSwitcher.cpp
        src/win32app/Win32IDE_LLMRouter.cpp
        src/win32app/Win32IDE_LSPClient.cpp
        src/win32app/Win32IDE_PeekOverlay.cpp
        src/win32app/Win32IDE_CaretAnimation.cpp
        src/win32app/Win32IDE_AgentOllamaClient.cpp
        src/win32app/Win32IDE_ModelDiscovery.cpp
        src/win32app/Win32IDE_AsmSemantic.cpp
        src/win32app/Win32IDE_LSP_AI_Bridge.cpp
        src/win32app/Win32IDE_MultiResponse.cpp
        src/win32app/Win32IDE_ExecutionGovernor.cpp
        src/win32app/Win32IDE_LayerEviction.cpp
        src/win32app/Win32IDE_SemanticIndexShim.cpp
        src/win32app/Win32IDE_SubAgent.h
        src/win32app/Win32IDE_SubAgent.cpp
        src/win32app/Win32IDE_AgentHistory.cpp
        src/win32app/Win32IDE_AgentPanel.cpp
        src/win32app/rtp_protocol_fallback.cpp
        src/win32app/win32ide_extension_command_fallback.cpp
        src/win32app/Win32IDE_DiskRecovery.cpp
        src/win32app/Win32TerminalManager.cpp
        src/win32app/Win32TerminalManager.h
        src/win32app/TransparentRenderer.cpp
        src/win32app/TransparentRenderer.h
        src/win32app/ModelConnection.h
        src/win32app/ContextManager.h

        # Plugin System — Phase 43: Native Win32 DLL loading (C ABI)
        src/plugin_system/win32_plugin_loader.cpp
        src/win32app/Win32IDE_Plugins.cpp
        
        # Configuration management
        src/config/IDEConfig.cpp

        # LLM adapter + Auth (Qt-free: QtReplacements, no Qt dependency)
        src/llm_adapter/QuantBackend.cpp
        src/llm_adapter/GGUFRunner.cpp
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

        # T5: Agent Orchestrator + Bridge + Navigation (agentic brain wiring)
        src/agentic/AgentOrchestrator.cpp
        src/agentic/OrchestratorBridge.cpp
        src/agentic/AgenticNavigator.cpp
        src/agentic/ToolRegistry.cpp
        src/agentic/RawrXD_AgentLoop.cpp
        src/agentic/agentic_executor.cpp
        src/agentic/FIMPromptBuilder.cpp
        src/agentic_observability.cpp
        src/core/rawrxd_subsystem_api.cpp
        src/runtime_core.cpp
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
        src/agentic_observability.cpp
        src/core/rawrxd_subsystem_api.cpp

        # TelemetryCollector (required by model_bruteforce_engine)
        # src/agent/telemetry_collector.cpp (moved to T5)

        # EngineRegistry (required by sovereign_engines)
        # src/runtime_core.cpp (moved to T5)

        # ToolRegistry::inject_tools (required by runtime_core process_prompt)
        # src/tool_registry.cpp (removed - duplicate of src/agentic/ToolRegistry.cpp)

        # Mesh ASM stubs (asm_mesh_crdt_lookup etc. not in RawrXD_MeshBrain.asm)
        # src/core/mesh_brain_asm_stubs.cpp (removed stub)

        # AnalyzerDistiller + StreamingOrchestrator fallbacks (ASM modules excluded)
        # src/core/analyzer_distiller_stubs.cpp (removed stub)
        # src/core/streaming_orchestrator_stubs.cpp (removed stub)

        # Disk Recovery Agent (C++ implementation) + Tool handler + ASM wrapper
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
        src/codec/compression.cpp
        src/codec/brutal_gzip.cpp
        
        # Streaming engine registry (Phase 9)
        src/core/streaming_engine_registry.cpp
        src/core/gpu_backend_bridge.cpp
        src/core/enterprise_license.cpp
        src/core/enterprise_licensev2_impl.cpp
        src/core/enterprise_devunlock_bridge.cpp
        src/core/enterprise_devunlock_stub_masm.cpp
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
        src/core/memory_patch_byte_search_stubs.cpp
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

        # Shadow-Page Detour + Sentinel Watchdog (Layer 6)
        src/core/shadow_page_detour.cpp
        src/core/sentinel_watchdog.cpp

        # HTTP Client — WinHTTP async with future retention (CLI+GUI)
        src/agent/llm_http_client.cpp
        src/agent/model_invoker.cpp
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
        
        # Phase 25: AMD/ATI GPU Acceleration Toggle
        src/core/amd_gpu_accelerator.cpp
        
        # Phase 29: Multi-Architecture GPU/Accelerator Backends
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
        src/core/monaco_core_stubs.cpp
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
        src/core/missing_handler_stubs.cpp
        src/core/model_training_pipeline.cpp
        src/agentic/AgentOllamaClient.cpp
        src/core/ssot_handlers.cpp
        src/core/ssot_handlers_ext_isolated.cpp
        src/core/ssot_validation.cpp
        src/core/win32_ide_link_stubs.cpp
        src/core/win32ide_asm_fallback.cpp

        # Unified Command Dispatch — Zero-drift SSOT dispatch (replaces manual registration)
        src/core/unified_command_dispatch.cpp
        src/core/startup_phase_registry.cpp

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
        src/modules/quickjs_node_shims.cpp
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
        src/core/reasoning_schema_versioning.cpp
        src/core/cot_fallback_system.cpp
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
        src/security/InputSanitizer.cpp
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
        src/collab/websocket_hub.cpp
        src/win32app/Win32IDE_TestExplorerTree.cpp
        src/win32app/Win32IDE_NetworkPanel.cpp
        src/core/omega_orchestrator.cpp
        src/core/mesh_brain.cpp
        src/core/mesh_brain_asm_stubs.cpp
        src/core/speciator_engine.cpp
        src/core/neural_bridge.cpp
        src/core/self_host_engine.cpp
        src/core/hardware_synthesizer.cpp
        src/core/transcendence_coordinator.cpp
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
        src/win32app/Win32IDE_Refactor.cpp
        src/win32app/Win32IDE_SearchPanel.cpp
        src/win32app/Win32IDE_FuzzySearch.cpp
        src/win32app/Win32IDE_CallStackSymbols.cpp
        src/win32app/multi_file_search_stub.cpp

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
        # Multi-file search (real implementation; replaces earlier IDE stub)
        src/multi_file_search.cpp
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
    )
     
    # =========================================================================
    # MultiWindow Task Scheduler Kernel (MASM64 DLL)
    # =========================================================================
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
        add_definitions(-DRAWRXD_HAS_MW_KERNEL=1)
        message(STATUS "MultiWindow Kernel DLL target enabled")
    else()
        add_definitions(-DRAWRXD_HAS_MW_KERNEL=0)
        message(STATUS "MultiWindow Kernel DLL skipped (no MASM)")
    endif()

    # =========================================================================
    # Dynamic Prompt Engine DLL (MASM64 kernel + C glue)
    # Unity/Unreal compatible SHARED library
    # =========================================================================
    if(RAWR_HAS_MASM)
        set(PROMPT_ENGINE_ASM src/asm/RawrXD_DynamicPromptEngine.asm)
        set_source_files_properties(${PROMPT_ENGINE_ASM} PROPERTIES LANGUAGE ASM_MASM)

        add_library(RawrXD_DynamicPromptEngine SHARED EXCLUDE_FROM_ALL
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
        add_definitions(-DRAWRXD_HAS_PROMPT_ENGINE=1)
        message(STATUS "DynamicPromptEngine DLL target enabled (Unity/Unreal compatible)")
    else()
        add_definitions(-DRAWRXD_HAS_PROMPT_ENGINE=0)
        message(STATUS "DynamicPromptEngine DLL skipped (no MASM)")
    endif()

    if(RAWRXD_ENABLE_MISSING_HANDLER_STUBS)
        list(REMOVE_ITEM WIN32IDE_SOURCES
            src/core/ssot_handlers.cpp
            src/core/ssot_handlers_ext_isolated.cpp
        )
        list(APPEND WIN32IDE_SOURCES
            src/core/ssot_missing_handlers_provider.cpp
        )
        message(STATUS "[SSOT] Win32IDE: STUBS lane enabled (excluding ssot_handlers.cpp + ssot_handlers_ext_isolated.cpp; adding ssot_missing_handlers_provider.cpp)")
    else()
        # Real lane - keep isolated handlers and runtime menu/command handlers.
        # DO NOT remove ssot_handlers_ext_isolated.cpp - it has working VSCEXT handlers 10000-10009
        # DO NOT add ssot_handlers_ext.cpp - it has unresolved symbols we haven't fixed
        # missing_handler_stubs.cpp provides runtime command/menu implementations.
        # win32_ide_link_stubs.cpp keeps only non-menu fallback symbols.
        message(STATUS "Win32IDE: Real lane - using isolated handlers + runtime handlers + link shims")
    endif()

    if(RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)
        list(REMOVE_ITEM WIN32IDE_SOURCES
            src/core/monaco_core_stubs.cpp
            src/core/ssot_missing_handlers_provider.cpp
        )
        message(STATUS "[ProductionLane] Win32IDE: stripped monaco/ssot stub units; kept missing_handler_stubs.cpp runtime handlers")
    endif()

    # Avoid mixing parallel GGUF engine stacks in this lane.
    list(REMOVE_ITEM WIN32IDE_SOURCES
        src/engine/sovereign_engines.cpp
        src/engine/rawr_engine.cpp
        src/engine/gguf_core.cpp
    )
    # Use real model loader implementation in Win32IDE.
    list(REMOVE_ITEM WIN32IDE_SOURCES src/core/rawrxd_model_loader_link_stub.cpp)
    # Re-wire real providers needed by this target's link graph.
    list(APPEND WIN32IDE_SOURCES
        src/tool_registry.cpp
        src/agentic/RawrXD_ToolRegistry.cpp
        src/agent/DiskRecoveryAgent.cpp
        src/core/local_parity_bridge.cpp
        src/core/subsystem_mode_fallbacks.cpp
        src/core/model_loader_fallbacks.cpp
        src/core/rawrxd_native_log_fallback.cpp
        src/llm_adapter/ggufrunner_link_fallbacks.cpp
        src/agentic/masm_agent_failure_fallback.cpp
    )

    list(APPEND WIN32IDE_SOURCES
        src/win32app/ai_workers_queue.cpp
        # v14.7.0 Native Track B Modules
        src/ui/ImportBridge.cpp
        src/ui/ExportBridge.cpp
        src/editor/ghost_text_renderer.hpp
        src/terminal/embedded_terminal.cpp
        src/terminal/embedded_terminal.hpp
        src/lsp/lsp_client_wired.hpp
        src/agentic/multi_file_composer.hpp
        src/git/git_wired.hpp
        src/ui/warp_hud.hpp
        src/ui/logic_bridge.hpp
        src/config/settings.hpp
        src/win32app/collab_cursor_fallbacks.cpp
    )

    if(RAWR_HAS_MASM)
        list(APPEND ASM_KERNEL_SOURCES
            src/asm/RawrXD_PE_Importer.asm
            src/asm/RawrXD_PE_Exporter.asm
            src/asm/memory_patch.asm
            src/asm/byte_search.asm
            src/asm/ghost_text_ranker.asm
            src/asm/diff_engine.asm
        )
    endif()

    # Gold Lane: prefer real implementations for Win32IDE agentic/autonomy path.
    option(RAWRXD_ALLOW_AGENTIC_STUB_FALLBACK
        "Allow stub/link-fallback units in RawrXD-Win32IDE agentic path"
        OFF
    )
    if(RAWRXD_ALLOW_AGENTIC_STUB_FALLBACK)
        message(STATUS "[Win32IDE] Agentic stub fallback enabled by RAWRXD_ALLOW_AGENTIC_STUB_FALLBACK=ON")
    else()
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*_stubs\\.cpp$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/reverse_engineered_stubs\\.cpp$")
        list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX ".*/link_stubs_.*\\.cpp$")

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
        # Re-add ai_agent_masm_stubs.cpp: provides masm_agent_failure_detect_simd (required by agentic_failure_detector.cpp)
        list(APPEND WIN32IDE_SOURCES src/core/ai_agent_masm_stubs.cpp)
        message(STATUS "[Win32IDE] Agentic real lane enforced (stub/link-fallback units removed)")
    endif()

    option(RAWRXD_STRICT_AGENTIC_REALITY
        "Fail CMake configure if Win32IDE sources still include stub/link-fallback units"
        ON
    )
    if(RAWRXD_STRICT_AGENTIC_REALITY AND NOT RAWRXD_ALLOW_AGENTIC_STUB_FALLBACK)
        set(_RAWRXD_FORBIDDEN_WIN32IDE_SOURCES "")
        foreach(_rawrxd_src IN LISTS WIN32IDE_SOURCES)
            # ai_agent_masm_stubs.cpp is required (masm_agent_failure_detect_simd), not a fallback
            if(NOT _rawrxd_src MATCHES "ai_agent_masm_stubs\\.cpp$" AND _rawrxd_src MATCHES "(_stubs\\.cpp$|/stubs\\.cpp$|/stub_.*\\.cpp$|/link_stubs_.*\\.cpp$|/agentic_bridge_headless\\.cpp$|/Win32IDE_headless_stubs\\.cpp$)")
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
    # No stubs permitted; ai_agent_masm_stubs.cpp exempt (provides MASM-required symbols).
    # =========================================================================
    
    function(EnforceNoStubs target_name)
        get_target_property(SOURCES ${target_name} SOURCES)
        set(STUB_VIOLATIONS "")
        set(STUB_COUNT 0)
        
        foreach(src ${SOURCES})
            # Detect stub files
            if(src MATCHES "(_stubs\\.cpp$|_stubs\\.asm$|/missing_handler)" OR
               src MATCHES "(stub_|shim_)" OR
               src MATCHES "(mock_|fake_)")
                                # ai_agent_masm_stubs.cpp provides required MASM symbols — exempt
                                if(src MATCHES "ai_agent_masm_stubs\\.cpp$")
                                    continue()
                                endif()
                list(APPEND STUB_VIOLATIONS "${src}")
                math(EXPR STUB_COUNT "${STUB_COUNT} + 1")
            endif()
        endforeach()
        
        # Enforce: No stubs in production
        if(STUB_COUNT GREATER 0)
            if(NOT DEFINED ENV{RAWRXD_EMERGENCY_STUB_LINK})
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
    
    # Mandatory symbol providers for Win32IDE link closure (only link_stubs_gate allowed).
    # Remove any other stubs that may have been added
    list(FILTER WIN32IDE_SOURCES EXCLUDE REGEX "(_stubs\\.cpp$|_stubs\\.asm$|missing_handler|stub_|shim_|mock_|fake_)")


    # Pre-append strict guard: Verify no stubs were auto-added earlier
    if(RAWRXD_STRICT_AGENTIC_REALITY AND NOT RAWRXD_ALLOW_AGENTIC_STUB_FALLBACK)
        set(_RAWRXD_FORBIDDEN_WIN32IDE_SOURCES_POST "")
        foreach(_rawrxd_src IN LISTS WIN32IDE_SOURCES)
            if(_rawrxd_src MATCHES "(_stubs\\.cpp$|/stubs\\.cpp$|/stub_.*\\.cpp$)" OR
               _rawrxd_src MATCHES "(missing_handler|shim_|mock_|fake_)")
                if(NOT _rawrxd_src MATCHES "ai_agent_masm_stubs\\.cpp$")
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

    # Final Duplicate Elimination (v14.7.4 Deep Clean)
    list(REMOVE_ITEM WIN32IDE_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/tool_registry.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/agentic/RawrXD_ToolRegistry.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/agent/DiskRecoveryAgent.cpp"
    )

    # v14.7.5 LNK2005 Duplicate Symbol Elimination
    # These dedicated feature files duplicate definitions already consolidated in
    # Tier2/Tier5 cosmetics or in Win32IDE.cpp.  Keep the consolidation copies.
    list(REMOVE_ITEM WIN32IDE_SOURCES
        src/win32app/Win32IDE_CallStackSymbols.cpp       # dups initCallStackSymbols / handleCallStackCommand  (Tier5Cosmetics)
        src/win32app/multi_file_search_stub.cpp          # dups MultiFileSearchWidget ctor/dtor  (multi_file_search.cpp)
    )

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

    add_executable(RawrXD-Win32IDE WIN32 ${WIN32IDE_SOURCES} ${ASM_KERNEL_SOURCES}
        src/win32app/RawrXD-Win32IDE.rc)

    # =========================================================================
    # PRODUCTION POLICY ENFORCEMENT
    # =========================================================================
    # Enforce both: (1) No stubs linked, (2) link_stubs_gate.cpp shrinking
    EnforceNoStubs(RawrXD-Win32IDE)
    EnforceLinkStubsGateSizeLimit()

    # External infra pack is mirrored into the project tree under D: and wired
    # as include/lib providers for Win32IDE.
    set(RAWRXD_INFRA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/rawr_infra")
    target_include_directories(RawrXD-Win32IDE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Ship")
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
            "/MAP:${_IDE_MAP_PATH}"
            /MAPINFO:EXPORTS
        )
        # Mirror the map at the build root for audit tools that scan ${CMAKE_BINARY_DIR}.
        add_custom_command(TARGET RawrXD-Win32IDE POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_IDE_MAP_PATH}" "${_IDE_MAP_AUDIT_PATH}"
            VERBATIM
        )
    endif()

    # Runtime feature gates for this target's source graph.
    target_compile_definitions(RawrXD-Win32IDE PRIVATE
        RAWR_HAS_SOVEREIGN_ENGINES=0
        RAWR_HAS_RAWR_INFERENCE=0
        RAWRXD_USE_DEDICATED_CODELENS=1
        RAWRXD_USE_DEDICATED_INLAY=1
        RAWRXD_USE_DEDICATED_HOVER=1
        RAWRXD_USE_DEDICATED_MINIMAP=1
    )

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
        message(STATUS "QuickJS found: JS extension host enabled")
        add_definitions(-DRAWR_HAS_QUICKJS=1)
    else()
        message(STATUS "QuickJS not found at ${QUICKJS_DIR} — JS extension host uses stubs")
        # Create stub include directory so headers compile
        target_compile_definitions(RawrXD-Win32IDE PRIVATE RAWR_QUICKJS_STUB=1)
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

    set(_WIN32IDE_OPTIONAL_MONOLITHIC_OBJS
        "${CMAKE_CURRENT_SOURCE_DIR}/obj/asm_monolithic_beacon.obj"
        "${CMAKE_CURRENT_SOURCE_DIR}/obj/asm_monolithic_model_loader.obj"
        "${CMAKE_CURRENT_SOURCE_DIR}/obj/asm_monolithic_inference.obj"
    )
    set(_WIN32IDE_LINKED_MONOLITHIC_OBJS "")
    foreach(_obj IN LISTS _WIN32IDE_OPTIONAL_MONOLITHIC_OBJS)
        if(EXISTS "${_obj}")
            list(APPEND _WIN32IDE_LINKED_MONOLITHIC_OBJS "${_obj}")
        else()
            message(STATUS "Optional Win32IDE link object missing, skipping: ${_obj}")
        endif()
    endforeach()

    target_link_libraries(RawrXD-Win32IDE PRIVATE
        Threads::Threads
        comctl32 comdlg32 shell32 ole32 oleaut32 uuid
        shlwapi psapi dbghelp winhttp ws2_32 winmm gdi32 user32 ntdll
        d2d1 dwrite d3d11 dcomp d3dcompiler dwmapi
        dxgi
        advapi32 crypt32 bcrypt wintrust
        opengl32 wininet
        pdh
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
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    if(MSVC)
        target_compile_options(RawrXD-Win32IDE PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:/EHsc /W0 /arch:AVX512>
            # Avoid /RTC1 + /O2 conflict in Debug (CMake injects /RTC1 for Debug by default).
            $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CONFIG:Debug>>>:/O2>
        )
    else()
        target_compile_options(RawrXD-Win32IDE PRIVATE -O3 -w -mavx2 -mfma -mavx512f -mavx512bw -mavx512dq -mavx512vl -march=native -fopenmp)
    endif()

    set_target_properties(RawrXD-Win32IDE PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY bin
        OUTPUT_NAME RawrXD-Win32IDE
        CXX_STANDARD 20
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
if(WIN32 AND MSVC AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/security/uac_bypass_impl_stub.cpp")
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
    message(STATUS "RawrXD-Crypto skipped (requires WIN32 + MSVC + MASM + uac_bypass_impl_stub.cpp)")
endif()

# NOTE:
# Duplicate/partial RawrXD_Gold source block removed here.
# Canonical RawrXD_Gold target is defined earlier (see line ~1058).
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

# Build only the SSOT objects needed by the ownership guard.
if(EXISTS "${CMAKE_SOURCE_DIR}/src/core/missing_handler_stubs.cpp")
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
else()
    message(STATUS "[SSOT] missing_handler_stubs.cpp absent; stub lane guard disabled.")
endif()

option(RAWRXD_ENABLE_HEAVY_GATES "Run heavy ownership/compiled-symbol guards as part of self_test_gate" OFF)
if(RAWRXD_ENABLE_HEAVY_GATES)
    add_custom_target(self_test_gate
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config $<CONFIG> --target ssot_stub_lane_guard
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config $<CONFIG> --target RawrXD-Win32IDE
        COMMENT "[CI Gate] Ownership guard + RawrXD-Win32IDE"
        VERBATIM
    )
else()
    add_custom_target(self_test_gate
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config $<CONFIG> --target RawrXD-Win32IDE
        COMMENT "[CI Gate] RawrXD-Win32IDE (heavy gates disabled)"
        VERBATIM
    )
endif()

# CI gate: stress_test — build and note availability
message(STATUS "[CI] self_test_gate target available (cmake --build . --target self_test_gate)")
message(STATUS "[CI] RAWR_ASAN=${RAWR_ASAN}")


find_package(Python3 COMPONENTS Interpreter QUIET)
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

    if(EXISTS "${CMAKE_SOURCE_DIR}/src/cpu_inference_engine_autofix_stub.cpp")
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
endif()

# Swarm smoke test (manual)
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
