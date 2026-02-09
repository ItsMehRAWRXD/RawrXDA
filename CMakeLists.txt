cmake_minimum_required(VERSION 3.20)
project(RawrEngine VERSION 7.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-D_CRT_SECURE_NO_WARNINGS -DNOMINMAX -DWIN32_LEAN_AND_MEAN)

# =============================================================================
# MASM64 Assembly Kernel Support
# =============================================================================
# Enable ASM_MASM only on MSVC (ml64.exe required).
# MinGW builds skip ASM kernels and use C++ fallbacks.
if(MSVC)
    enable_language(ASM_MASM)
    set(CMAKE_ASM_MASM_FLAGS "/c /Zi /Zd")
    set(RAWR_HAS_MASM TRUE)
    add_definitions(-DRAWR_HAS_MASM=1)
    message(STATUS "MASM64 enabled: ASM kernels will be compiled")
else()
    set(RAWR_HAS_MASM FALSE)
    message(STATUS "MASM64 not available (non-MSVC): using C++ fallbacks")
endif()

# ASM kernel sources (shared between RawrEngine and Win32IDE)
if(RAWR_HAS_MASM)
    set(ASM_KERNEL_SOURCES
        src/asm/inference_core.asm
        src/asm/FlashAttention_AVX512.asm
        src/asm/quant_avx2.asm
        src/asm/RawrXD_KQuant_Dequant.asm
        src/asm/memory_patch.asm
        src/asm/byte_search.asm
        src/asm/request_patch.asm
        src/asm/inference_kernels.asm
        src/asm/model_bridge_x64.asm
    )
    # Mark ASM files with MASM language property
    set_source_files_properties(${ASM_KERNEL_SOURCES} PROPERTIES LANGUAGE ASM_MASM)
else()
    set(ASM_KERNEL_SOURCES "")
endif()

find_package(Threads REQUIRED)

set(SOURCES 
    src/main.cpp
    src/stubs.cpp
    src/memory_core.cpp
    src/hot_patcher.cpp
    src/cpu_inference_engine.cpp
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
    
    src/codec/compression.cpp
    src/codec/brutal_gzip.cpp

    src/core/execution_scheduler.cpp
    
    # Agentic Decision Tree + CLI Autonomy (Phase 19)
    src/cli/agentic_decision_tree.cpp
    src/cli/cli_autonomy_loop.cpp
    
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
    
    # Three-layer hotpatch system (needed by decision tree)
    src/core/model_memory_hotpatch.cpp
    src/core/byte_level_hotpatcher.cpp
    src/core/unified_hotpatch_manager.cpp
    src/core/proxy_hotpatcher.cpp
    src/server/gguf_server_hotpatch.cpp
    src/agent/agentic_hotpatch_orchestrator.cpp
    
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
    src/core/enterprise_license_stubs.cpp
    src/core/swarm_coordinator.cpp
    src/core/swarm_worker.cpp
    src/core/swarm_network_stubs.cpp
    src/tool_registry.cpp

    # Phase 21: Distributed Swarm Inference Orchestrator
    src/cli/swarm_orchestrator.cpp

    # Phase 33: Voice Chat Engine
    src/core/voice_chat.cpp

    # Phase 33: Quick-Win Qt Ports
    src/core/shortcut_manager.cpp
    src/core/backup_manager.cpp
    src/core/alert_system.cpp

    # Phase 32A: Chain-of-Thought Multi-Model Review Engine
    src/core/chain_of_thought_engine.cpp

    # Phase 36: Cross-Process MMF State Synchronization (shared with Win32IDE)
    src/core/rawrxd_state_mmf.cpp

    # Phase 37: Iterative Partial Inference with Adaptive Tensor Traversal
    src/core/traversal_strategy.cpp
    src/core/layer_contribution_scorer.cpp
    src/core/semantic_delta_tracker.cpp
    src/core/cross_run_tensor_cache.cpp
    src/core/convergence_controller.cpp
    src/core/iterative_tensor_traversal.cpp
)

# =============================================================================
# Reverse Engineering Suite (sub-module)
# =============================================================================
add_subdirectory(src/reverse_engineering)

add_executable(RawrEngine ${SOURCES} ${ASM_KERNEL_SOURCES})

# Link RE library into RawrEngine
target_link_libraries(RawrEngine PRIVATE RawrXD-RE-Library)

target_include_directories(RawrEngine PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/server
    ${CMAKE_CURRENT_SOURCE_DIR}/src/agent
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cli
    ${CMAKE_CURRENT_SOURCE_DIR}/src/reverse_engineering
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(RawrEngine PRIVATE Threads::Threads)

# ═══════════════════════════════════════════════════════════════════
# MASM x64 Assembly — Phase 20/21 AVX-512 Requantization + Swarm Streaming
# ═══════════════════════════════════════════════════════════════════
if(MSVC)
    # Phase 20: AVX-512 Q4_K_M → Q2_K requantization kernel
    set(ASM_REQUANTIZE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/requantize_q4km_to_q2k_avx512.asm")
    set(ASM_REQUANTIZE_OBJ "${CMAKE_CURRENT_BINARY_DIR}/requantize_q4km_to_q2k_avx512.obj")
    add_custom_command(
        OUTPUT ${ASM_REQUANTIZE_OBJ}
        COMMAND ml64 /c /Zi /Zd /Fo${ASM_REQUANTIZE_OBJ} ${ASM_REQUANTIZE_SRC}
        DEPENDS ${ASM_REQUANTIZE_SRC}
        COMMENT "[MASM] Assembling requantize_q4km_to_q2k_avx512.asm (AVX-512 DQ/BW)"
        VERBATIM
    )

    # Phase 20: ROCm GPU requantization dispatch stubs
    set(ASM_GPU_REQUANT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/gpu_requantize_rocm.asm")
    set(ASM_GPU_REQUANT_OBJ "${CMAKE_CURRENT_BINARY_DIR}/gpu_requantize_rocm.obj")
    add_custom_command(
        OUTPUT ${ASM_GPU_REQUANT_OBJ}
        COMMAND ml64 /c /Zi /Zd /Fo${ASM_GPU_REQUANT_OBJ} ${ASM_GPU_REQUANT_SRC}
        DEPENDS ${ASM_GPU_REQUANT_SRC}
        COMMENT "[MASM] Assembling gpu_requantize_rocm.asm (HIP dispatch)"
        VERBATIM
    )

    # Phase 21: Zero-copy tensor network streaming
    set(ASM_SWARM_TENSOR_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/swarm_tensor_stream.asm")
    set(ASM_SWARM_TENSOR_OBJ "${CMAKE_CURRENT_BINARY_DIR}/swarm_tensor_stream.obj")
    add_custom_command(
        OUTPUT ${ASM_SWARM_TENSOR_OBJ}
        COMMAND ml64 /c /Zi /Zd /Fo${ASM_SWARM_TENSOR_OBJ} ${ASM_SWARM_TENSOR_SRC}
        DEPENDS ${ASM_SWARM_TENSOR_SRC}
        COMMENT "[MASM] Assembling swarm_tensor_stream.asm (CRC32C, RLE, chunks)"
        VERBATIM
    )

    # Phase 28: MonacoCore Native Editor Engine — Gap Buffer + Tokenizer
    set(ASM_MONACOCORE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_MonacoCore.asm")
    set(ASM_MONACOCORE_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_MonacoCore.obj")
    add_custom_command(
        OUTPUT ${ASM_MONACOCORE_OBJ}
        COMMAND ml64 /c /Zi /Zd /Fo${ASM_MONACOCORE_OBJ} ${ASM_MONACOCORE_SRC}
        DEPENDS ${ASM_MONACOCORE_SRC}
        COMMENT "[MASM] Assembling RawrXD_MonacoCore.asm (Gap Buffer, Tokenizer)"
        VERBATIM
    )

    # Phase 29: PDB Symbol Server — Native MSF v7.00 Parser Kernel
    set(ASM_PDBKERNEL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_PDBKernel.asm")
    set(ASM_PDBKERNEL_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_PDBKernel.obj")
    add_custom_command(
        OUTPUT ${ASM_PDBKERNEL_OBJ}
        COMMAND ml64 /c /Zi /Zd /Fo${ASM_PDBKERNEL_OBJ} ${ASM_PDBKERNEL_SRC}
        DEPENDS ${ASM_PDBKERNEL_SRC}
        COMMENT "[MASM] Assembling RawrXD_PDBKernel.asm (MSF magic, PubSym scan, GUID hex)"
        VERBATIM
    )

    # Phase 30: Accelerator Router — MASM Fast-Path Bridge
    set(ASM_ROUTERBRIDGE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_RouterBridge.asm")
    set(ASM_ROUTERBRIDGE_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_RouterBridge.obj")
    add_custom_command(
        OUTPUT ${ASM_ROUTERBRIDGE_OBJ}
        COMMAND ml64 /c /Zi /Zd /Fo${ASM_ROUTERBRIDGE_OBJ} ${ASM_ROUTERBRIDGE_SRC}
        DEPENDS ${ASM_ROUTERBRIDGE_SRC}
        COMMENT "[MASM] Assembling RawrXD_RouterBridge.asm (Fast-Path Router Dispatch)"
        VERBATIM
    )

    # Phase 29.2: GSI Hash — MASM Accelerated Hash + Bucket Probe
    set(ASM_GSIHASH_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_GSIHash.asm")
    set(ASM_GSIHASH_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_GSIHash.obj")
    add_custom_command(
        OUTPUT ${ASM_GSIHASH_OBJ}
        COMMAND ml64 /c /Zi /Zd /Fo${ASM_GSIHASH_OBJ} ${ASM_GSIHASH_SRC}
        DEPENDS ${ASM_GSIHASH_SRC}
        COMMENT "[MASM] Assembling RawrXD_GSIHash.asm (PDB hash, GSI bucket probe)"
        VERBATIM
    )

    # Phase 29.2: Reference Provider — FNV-1a Hash + Pattern Match
    set(ASM_REFPROVIDER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_RefProvider.asm")
    set(ASM_REFPROVIDER_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_RefProvider.obj")
    add_custom_command(
        OUTPUT ${ASM_REFPROVIDER_OBJ}
        COMMAND ml64 /c /Zi /Zd /Fo${ASM_REFPROVIDER_OBJ} ${ASM_REFPROVIDER_SRC}
        DEPENDS ${ASM_REFPROVIDER_SRC}
        COMMENT "[MASM] Assembling RawrXD_RefProvider.asm (Reference hash, pattern match)"
        VERBATIM
    )

    # Phase 31: IDE Self-Audit — Stub Detection Kernel
    set(ASM_STUBDETECTOR_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/RawrXD_StubDetector.asm")
    set(ASM_STUBDETECTOR_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_StubDetector.obj")
    add_custom_command(
        OUTPUT ${ASM_STUBDETECTOR_OBJ}
        COMMAND ml64 /c /Zi /Zd /Fo${ASM_STUBDETECTOR_OBJ} ${ASM_STUBDETECTOR_SRC}
        DEPENDS ${ASM_STUBDETECTOR_SRC}
        COMMENT "[MASM] Assembling RawrXD_StubDetector.asm (Stub Pattern Scanner)"
        VERBATIM
    )

    # Phase 37: Chain-of-Thought Engine — Pure MASM CoT DLL Core
    set(ASM_COT_ENGINE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/rawrxd_cot_engine.asm")
    set(ASM_COT_ENGINE_OBJ "${CMAKE_CURRENT_BINARY_DIR}/rawrxd_cot_engine.obj")
    add_custom_command(
        OUTPUT ${ASM_COT_ENGINE_OBJ}
        COMMAND ml64 /c /Zi /Zd /Fo${ASM_COT_ENGINE_OBJ} ${ASM_COT_ENGINE_SRC}
        DEPENDS ${ASM_COT_ENGINE_SRC}
        COMMENT "[MASM] Assembling rawrxd_cot_engine.asm (CoT Engine: 12 roles, 6 presets, 1GB arena, SEH)"
        VERBATIM
    )

    # Phase 37: Chain-of-Thought Engine — DLL Entry + SRW Lock
    set(ASM_COT_DLL_ENTRY_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/rawrxd_cot_dll_entry.asm")
    set(ASM_COT_DLL_ENTRY_OBJ "${CMAKE_CURRENT_BINARY_DIR}/rawrxd_cot_dll_entry.obj")
    add_custom_command(
        OUTPUT ${ASM_COT_DLL_ENTRY_OBJ}
        COMMAND ml64 /c /Zi /Zd /Fo${ASM_COT_DLL_ENTRY_OBJ} ${ASM_COT_DLL_ENTRY_SRC}
        DEPENDS ${ASM_COT_DLL_ENTRY_SRC}
        COMMENT "[MASM] Assembling rawrxd_cot_dll_entry.asm (DllMain, SRW Lock, Thread Tracking)"
        VERBATIM
    )

    # Phase 39–42: CoT Enterprise Finishers — Snapshot, Telemetry, Copy Engine, Security
    set(ASM_COT_PHASE39_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/asm/rawrxd_cot_phase39.asm")
    set(ASM_COT_PHASE39_OBJ "${CMAKE_CURRENT_BINARY_DIR}/rawrxd_cot_phase39.obj")
    add_custom_command(
        OUTPUT ${ASM_COT_PHASE39_OBJ}
        COMMAND ml64 /c /Zi /Zd /Fo${ASM_COT_PHASE39_OBJ} ${ASM_COT_PHASE39_SRC}
        DEPENDS ${ASM_COT_PHASE39_SRC}
        COMMENT "[MASM] Assembling rawrxd_cot_phase39.asm (Snapshot, Telemetry, CopyEngine, Seal, Quotas)"
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
    )

    # Link MASM objects into RawrEngine
    target_link_libraries(RawrEngine PRIVATE ${MASM_OBJECTS})

    # Ensure MASM objects are built before linking
    add_custom_target(masm_kernels DEPENDS ${MASM_OBJECTS})
    add_dependencies(RawrEngine masm_kernels)
endif()

if(WIN32)
    target_link_libraries(RawrEngine PRIVATE shlwapi psapi dbghelp comctl32 comdlg32 shell32 ole32 oleaut32 uuid winhttp ws2_32)
    if(NOT MSVC)
        target_link_libraries(RawrEngine PRIVATE gomp)
    endif()
endif()

if(NOT MSVC)
    target_compile_options(RawrEngine PRIVATE -march=native -O3 -w -mavx2 -mfma -fopenmp)
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

# Win32 GUI IDE
if(WIN32)
    set(WIN32IDE_SOURCES
        src/win32app/main_win32.cpp
        src/win32app/Win32IDE.cpp
        src/win32app/Win32IDE.h
        src/win32app/HeadlessIDE.h
        src/win32app/HeadlessIDE.cpp
        src/win32app/IOutputSink.h
        src/win32app/Win32IDE_Core.cpp
        src/win32app/Win32IDE_Sidebar.cpp
        src/win32app/Win32IDE_VSCodeUI.cpp
        src/win32app/Win32IDE_PowerShellPanel.cpp
        src/win32app/Win32IDE_PowerShell.cpp
        src/win32app/Win32IDE_Logger.cpp
        src/win32app/IDELogger.h
        src/win32app/Win32IDE_FileOps.cpp
        src/win32app/Win32IDE_Commands.cpp
        src/win32app/Win32IDE_Debugger.cpp
        src/win32app/Win32IDE_AgenticBridge.h
        src/win32app/Win32IDE_AgenticBridge.cpp
        src/win32app/Win32IDE_AgentCommands.cpp
        src/win32app/Win32IDE_Autonomy.h
        src/win32app/Win32IDE_Autonomy.cpp
        src/win32app/Win32IDE_SyntaxHighlight.cpp
        src/win32app/Win32IDE_Themes.cpp
        src/win32app/Win32IDE_Annotations.cpp
        src/win32app/Win32IDE_Session.cpp
        src/win32app/Win32IDE_StreamingUX.cpp
        src/win32app/Win32IDE_ReverseEngineering.cpp
        src/win32app/Win32IDE_DecompilerView.cpp
        src/win32app/Win32IDE_FeatureManifest.cpp
        src/win32app/Win32IDE_GhostText.cpp
        src/win32app/Win32IDE_PlanExecutor.cpp
        src/win32app/Win32IDE_FailureDetector.cpp
        src/win32app/Win32IDE_FailureIntelligence.cpp
        src/win32app/Win32IDE_Settings.cpp
        src/win32app/Win32IDE_LocalServer.cpp
        src/win32app/Win32IDE_BackendSwitcher.cpp
        src/win32app/Win32IDE_LLMRouter.cpp
        src/win32app/Win32IDE_LSPClient.cpp
        src/win32app/Win32IDE_AsmSemantic.cpp
        src/win32app/Win32IDE_LSP_AI_Bridge.cpp
        src/win32app/Win32IDE_MultiResponse.cpp
        src/win32app/Win32IDE_ExecutionGovernor.cpp
        src/win32app/Win32IDE_SubAgent.h
        src/win32app/Win32IDE_SubAgent.cpp
        src/win32app/Win32IDE_AgentHistory.cpp
        src/win32app/Win32TerminalManager.cpp
        src/win32app/Win32TerminalManager.h
        src/win32app/TransparentRenderer.cpp
        src/win32app/TransparentRenderer.h
        src/win32app/ModelConnection.h
        src/win32app/ContextManager.h
        
        # Configuration management
        src/config/IDEConfig.cpp
        
        # Core engine files needed by IDE
        src/stubs.cpp
        src/agentic_engine.cpp
        src/subagent_core.cpp
        src/agent_history.cpp
        src/agent_policy.cpp
        src/agent_explainability.cpp
        src/utils/ErrorReporter.cpp
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
        src/codec/compression.cpp
        src/codec/brutal_gzip.cpp
        
        # Streaming engine registry (Phase 9)
        src/core/streaming_engine_registry.cpp
        src/core/gpu_backend_bridge.cpp
        src/core/enterprise_license.cpp
        src/core/enterprise_license_stubs.cpp
        src/core/multi_response_engine.cpp
        src/core/flash_attention.cpp
        src/core/execution_scheduler.cpp
        src/core/execution_governor.cpp
        src/core/agent_safety_contract.cpp
        src/core/deterministic_replay.cpp
        src/core/confidence_gate.cpp
        
        # Distributed Swarm Compilation (Phase 11)
        src/core/swarm_coordinator.cpp
        src/core/swarm_worker.cpp
        src/core/swarm_network_stubs.cpp
        src/win32app/Win32IDE_SwarmPanel.cpp
        
        # Native Debugger Engine (Phase 12)
        src/core/native_debugger_engine.cpp
        src/core/debug_engine_stubs.cpp
        src/win32app/Win32IDE_NativeDebugPanel.cpp
        
        # Three-Layer Hotpatch System (Phase 14.2)
        src/core/model_memory_hotpatch.cpp
        src/core/byte_level_hotpatcher.cpp
        src/core/unified_hotpatch_manager.cpp
        src/core/proxy_hotpatcher.cpp
        src/server/gguf_server_hotpatch.cpp
        src/win32app/Win32IDE_HotpatchPanel.cpp
        
        # Agent-Driven Hotpatch Orchestration (Phase 18)
        src/agent/agentic_hotpatch_orchestrator.cpp
        
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
        src/win32app/Win32IDE_VoiceChat.cpp

        # Phase 33: Quick-Win Qt Ports
        src/core/shortcut_manager.cpp
        src/core/backup_manager.cpp
        src/core/alert_system.cpp
        src/win32app/Win32IDE_QuickWins.cpp

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
    )
    
    add_executable(RawrXD-Win32IDE WIN32 ${WIN32IDE_SOURCES} ${ASM_KERNEL_SOURCES})

    # =========================================================================
    # Phase 36: QuickJS Static Library (embedded JS runtime)
    # =========================================================================
    # QuickJS sources should be placed in 3rdparty/quickjs/
    # If not present, create stub target that allows compilation without QuickJS
    set(QUICKJS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/quickjs)
    if(EXISTS "${QUICKJS_DIR}/quickjs.c")
        add_library(quickjs_static STATIC
            ${QUICKJS_DIR}/quickjs.c
            ${QUICKJS_DIR}/libregexp.c
            ${QUICKJS_DIR}/libunicode.c
            ${QUICKJS_DIR}/cutils.c
            ${QUICKJS_DIR}/quickjs-libc.c
            ${QUICKJS_DIR}/libbf.c
        )
        target_include_directories(quickjs_static PUBLIC ${QUICKJS_DIR})
        if(MSVC)
            target_compile_options(quickjs_static PRIVATE /W0 /O2)
            target_compile_definitions(quickjs_static PRIVATE
                _CRT_SECURE_NO_WARNINGS
                CONFIG_BIGNUM
                CONFIG_VERSION="2024.01.13"
            )
        else()
            target_compile_options(quickjs_static PRIVATE -w -O2)
            target_compile_definitions(quickjs_static PRIVATE
                CONFIG_BIGNUM
                CONFIG_VERSION="2024.01.13"
                _GNU_SOURCE
            )
        endif()
        target_link_libraries(RawrXD-Win32IDE PRIVATE quickjs_static)
        target_include_directories(RawrXD-Win32IDE PRIVATE ${QUICKJS_DIR}/..)
        message(STATUS "QuickJS found: JS extension host enabled")
        add_definitions(-DRAWR_HAS_QUICKJS=1)
    else()
        message(STATUS "QuickJS not found at ${QUICKJS_DIR} — JS extension host uses stubs")
        # Create stub include directory so headers compile
        target_compile_definitions(RawrXD-Win32IDE PRIVATE RAWR_QUICKJS_STUB=1)
    endif()
    
    # Link RE library into Win32IDE
    target_link_libraries(RawrXD-Win32IDE PRIVATE RawrXD-RE-Library)
    
    target_link_libraries(RawrXD-Win32IDE PRIVATE
        Threads::Threads
        comctl32 comdlg32 shell32 ole32 oleaut32 uuid
        shlwapi psapi dbghelp dbgeng winhttp ws2_32 winmm gdi32 user32
        d2d1 dwrite d3d11 dcomp d3dcompiler dwmapi
        advapi32 crypt32 bcrypt wintrust
        ${MASM_OBJECTS}
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
    if(TARGET re_model_reverse)
        add_dependencies(RawrXD-Win32IDE re_model_reverse)
        target_link_libraries(RawrXD-Win32IDE PRIVATE ${RE_MODEL_OBJECTS})
    endif()
    
    target_include_directories(RawrXD-Win32IDE PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/engine
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core
        ${CMAKE_CURRENT_SOURCE_DIR}/src/server
        ${CMAKE_CURRENT_SOURCE_DIR}/src/agent
        ${CMAKE_CURRENT_SOURCE_DIR}/src/cli
        ${CMAKE_CURRENT_SOURCE_DIR}/src/win32app
        ${CMAKE_CURRENT_SOURCE_DIR}/src/modules
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
        ${CMAKE_CURRENT_SOURCE_DIR}/src/config
        ${CMAKE_CURRENT_SOURCE_DIR}/src/asm
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    if(MSVC)
        target_compile_options(RawrXD-Win32IDE PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:/EHsc /O2 /W0 /arch:AVX2>
        )
    else()
        target_compile_options(RawrXD-Win32IDE PRIVATE -O3 -w -mavx2 -mfma -march=native -fopenmp)
    endif()

    set_target_properties(RawrXD-Win32IDE PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY bin
        OUTPUT_NAME RawrXD-Win32IDE
    )
endif()