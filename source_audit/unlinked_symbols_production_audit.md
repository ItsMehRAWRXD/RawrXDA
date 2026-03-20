# Win32IDE Unlinked + Dissolved Symbol Audit

- Log: `/workspace/source_audit/win32ide_production_link_latest.log`
- Total unresolved references: **194**
- Unique unresolved symbols: **71**

## Classification Summary

| Classification | Count |
|---|---:|
| `unlinked_despite_wired` | 35 |
| `dissolved_or_external` | 18 |
| `unwired_non_stub` | 18 |

## Unwired Non-Stub Symbols

| Symbol | Occurrences | Top ref object | Example unwired definition |
|---|---:|---|---|
| `MC_GapBuffer_Destroy` | 32 | `final_gauntlet.cpp.obj` | `/workspace/src/ai/digestion_scanner.asm` |
| `MC_GapBuffer_LineCount` | 20 | `MonacoCoreEngine.cpp.obj` | `/workspace/src/ai/digestion_scanner.asm` |
| `MC_GapBuffer_GetLine` | 12 | `MonacoCoreEngine.cpp.obj` | `/workspace/src/ai/digestion_scanner.asm` |
| `MC_GapBuffer_Insert` | 8 | `MonacoCoreEngine.cpp.obj` | `/workspace/src/ai/digestion_scanner.asm` |
| `MC_GapBuffer_Length` | 8 | `final_gauntlet.cpp.obj` | `/workspace/src/ai/digestion_scanner.asm` |
| `MC_GapBuffer_Init` | 5 | `final_gauntlet.cpp.obj` | `/workspace/include/RawrXD_MonacoCore.h` |
| `MC_GapBuffer_Delete` | 4 | `MonacoCoreEngine.cpp.obj` | `/workspace/include/RawrXD_MonacoCore.h` |
| `VecDb_Init` | 2 | `copilot_gap_closer.cpp.obj` | `/workspace/src/modules/copilot_gap_closer.h` |
| `VecDb_Search` | 2 | `Win32IDE_CopilotGapPanel.cpp.obj` | `/workspace/src/modules/copilot_gap_closer.h` |
| `Composer_BeginTransaction` | 1 | `Win32IDE_CopilotGapPanel.cpp.obj` | `/workspace/src/modules/copilot_gap_closer.h` |
| `Composer_Commit` | 1 | `Win32IDE_CopilotGapPanel.cpp.obj` | `/workspace/src/modules/copilot_gap_closer.h` |
| `Crdt_InitDocument` | 1 | `Win32IDE_CopilotGapPanel.cpp.obj` | `/workspace/src/modules/copilot_gap_closer.h` |
| `OSExplorerInterceptor::Initialize(unsigned long, void (*)(void*))` | 1 | `Win32IDE_OSExplorerInterceptor.cpp.obj` | `/workspace/src/win32app/OSExplorerInterceptor.cpp` |
| `OSExplorerInterceptor::OSExplorerInterceptor()` | 1 | `Win32IDE_OSExplorerInterceptor.cpp.obj` | `/workspace/src/win32app/OSExplorerInterceptor.cpp` |
| `OSExplorerInterceptor::StartInterception()` | 1 | `Win32IDE_OSExplorerInterceptor.cpp.obj` | `/workspace/src/win32app/OSExplorerInterceptor.cpp` |
| `OSExplorerInterceptor::~OSExplorerInterceptor()` | 1 | `Win32IDE_OSExplorerInterceptor.cpp.obj` | `/workspace/src/win32app/OSExplorerInterceptor.cpp` |
| `VecDb_Delete` | 1 | `Win32IDE_CopilotGapPanel.cpp.obj` | `/workspace/src/modules/copilot_gap_closer.h` |
| `g_VecDbNodes` | 1 | `copilot_gap_closer.cpp.obj` | `/workspace/src/modules/copilot_gap_closer.h` |

## Dissolved or External Symbols

| Symbol | Occurrences | Top ref object |
|---|---:|---|
| `Crdt_GetDocLength` | 5 | `Win32IDE_CopilotGapPanel.cpp.obj` |
| `VecDb_GetNodeCount` | 4 | `Win32IDE_CopilotGapPanel.cpp.obj` |
| `Composer_GetTxCount` | 2 | `copilot_gap_closer.cpp.obj` |
| `VecDb_Insert` | 2 | `Win32IDE_CopilotGapPanel.cpp.obj` |
| `Composer_AddFileOp` | 1 | `Win32IDE_CopilotGapPanel.cpp.obj` |
| `Composer_GetState` | 1 | `copilot_gap_closer.cpp.obj` |
| `Crdt_DeleteText` | 1 | `Win32IDE_CopilotGapPanel.cpp.obj` |
| `Crdt_GetLamport` | 1 | `copilot_gap_closer.cpp.obj` |
| `Crdt_InsertText` | 1 | `Win32IDE_CopilotGapPanel.cpp.obj` |
| `Git_SetBranch` | 1 | `Win32IDE_CopilotGapPanel.cpp.obj` |
| `Git_SetCommitHash` | 1 | `Win32IDE_CopilotGapPanel.cpp.obj` |
| `RawrXD::Debugger::NativeDebuggerEngine::addBreakpointBySymbol(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, RawrXD::Debugger::BreakpointType)` | 1 | `feature_handlers.cpp.obj` |
| `RawrXD::Debugger::NativeDebuggerEngine::getStats() const` | 1 | `feature_handlers.cpp.obj` |
| `RawrXD::Embeddings::EmbeddingEngine::indexDirectory(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, RawrXD::Embeddings::ChunkingConfig const&)` | 1 | `unified_hotpatch_manager.cpp.obj` |
| `RawrXD::Embeddings::EmbeddingEngine::loadModel(RawrXD::Embeddings::EmbeddingModelConfig const&)` | 1 | `unified_hotpatch_manager.cpp.obj` |
| `RawrXD::Embeddings::EmbeddingEngine::shutdown()` | 1 | `unified_hotpatch_manager.cpp.obj` |
| `g_VecDbEntryPoint` | 1 | `copilot_gap_closer.cpp.obj` |
| `g_VecDbMaxLevel` | 1 | `copilot_gap_closer.cpp.obj` |

## Top 40 Unresolved Symbols

| Symbol | Class | Occurrences | Top ref object |
|---|---|---:|---|
| `MC_GapBuffer_Destroy` | `unwired_non_stub` | 32 | `final_gauntlet.cpp.obj` |
| `MC_GapBuffer_LineCount` | `unwired_non_stub` | 20 | `MonacoCoreEngine.cpp.obj` |
| `MC_GapBuffer_GetLine` | `unwired_non_stub` | 12 | `MonacoCoreEngine.cpp.obj` |
| `MC_GapBuffer_Insert` | `unwired_non_stub` | 8 | `MonacoCoreEngine.cpp.obj` |
| `MC_GapBuffer_Length` | `unwired_non_stub` | 8 | `final_gauntlet.cpp.obj` |
| `Crdt_GetDocLength` | `dissolved_or_external` | 5 | `Win32IDE_CopilotGapPanel.cpp.obj` |
| `MC_GapBuffer_Init` | `unwired_non_stub` | 5 | `final_gauntlet.cpp.obj` |
| `Swarm_ComputeNodeFitness` | `unlinked_despite_wired` | 5 | `swarm_coordinator.cpp.obj` |
| `MC_GapBuffer_Delete` | `unwired_non_stub` | 4 | `MonacoCoreEngine.cpp.obj` |
| `Swarm_BuildPacketHeader` | `unlinked_despite_wired` | 4 | `swarm_coordinator.cpp.obj` |
| `VecDb_GetNodeCount` | `dissolved_or_external` | 4 | `Win32IDE_CopilotGapPanel.cpp.obj` |
| `KQuant_DequantizeQ4_K` | `unlinked_despite_wired` | 3 | `model_training_pipeline.cpp.obj` |
| `KQuant_DequantizeQ6_K` | `unlinked_despite_wired` | 3 | `model_training_pipeline.cpp.obj` |
| `Quant_DequantQ4_0` | `unlinked_despite_wired` | 3 | `model_training_pipeline.cpp.obj` |
| `Quant_DequantQ8_0` | `unlinked_despite_wired` | 3 | `model_training_pipeline.cpp.obj` |
| `RawrXD::Embeddings::EmbeddingEngine::instance()` | `unlinked_despite_wired` | 3 | `unified_hotpatch_manager.cpp.obj` |
| `Swarm_HeartbeatRecord` | `unlinked_despite_wired` | 3 | `swarm_coordinator.cpp.obj` |
| `Swarm_RingBuffer_Init` | `unlinked_despite_wired` | 3 | `swarm_coordinator.cpp.obj` |
| `Swarm_ValidatePacketHeader` | `unlinked_despite_wired` | 3 | `swarm_coordinator.cpp.obj` |
| `asm_scsi_read_capacity` | `unlinked_despite_wired` | 3 | `DiskRecoveryAgent.cpp.obj` |
| `Composer_GetTxCount` | `dissolved_or_external` | 2 | `copilot_gap_closer.cpp.obj` |
| `Swarm_IOCP_Associate` | `unlinked_despite_wired` | 2 | `swarm_coordinator.cpp.obj` |
| `Swarm_XXH64` | `unlinked_despite_wired` | 2 | `swarm_coordinator.cpp.obj` |
| `VecDb_Init` | `unwired_non_stub` | 2 | `copilot_gap_closer.cpp.obj` |
| `VecDb_Insert` | `dissolved_or_external` | 2 | `Win32IDE_CopilotGapPanel.cpp.obj` |
| `VecDb_Search` | `unwired_non_stub` | 2 | `Win32IDE_CopilotGapPanel.cpp.obj` |
| `native_fused_mlp_avx2` | `unlinked_despite_wired` | 2 | `native_speed_layer.cpp.obj` |
| `sgemv_avx2` | `unlinked_despite_wired` | 2 | `native_speed_layer.cpp.obj` |
| `swarm_build_discovery_packet` | `unlinked_despite_wired` | 2 | `swarm_orchestrator.cpp.obj` |
| `BeaconRouterInit` | `unlinked_despite_wired` | 1 | `universal_model_router.cpp.obj` |
| `Composer_AddFileOp` | `dissolved_or_external` | 1 | `Win32IDE_CopilotGapPanel.cpp.obj` |
| `Composer_BeginTransaction` | `unwired_non_stub` | 1 | `Win32IDE_CopilotGapPanel.cpp.obj` |
| `Composer_Commit` | `unwired_non_stub` | 1 | `Win32IDE_CopilotGapPanel.cpp.obj` |
| `Composer_GetState` | `dissolved_or_external` | 1 | `copilot_gap_closer.cpp.obj` |
| `Crdt_DeleteText` | `dissolved_or_external` | 1 | `Win32IDE_CopilotGapPanel.cpp.obj` |
| `Crdt_GetLamport` | `dissolved_or_external` | 1 | `copilot_gap_closer.cpp.obj` |
| `Crdt_InitDocument` | `unwired_non_stub` | 1 | `Win32IDE_CopilotGapPanel.cpp.obj` |
| `Crdt_InsertText` | `dissolved_or_external` | 1 | `Win32IDE_CopilotGapPanel.cpp.obj` |
| `GapCloser_GetPerfCounters` | `unlinked_despite_wired` | 1 | `copilot_gap_closer.cpp.obj` |
| `Git_ExtractContext` | `unlinked_despite_wired` | 1 | `copilot_gap_closer.cpp.obj` |

