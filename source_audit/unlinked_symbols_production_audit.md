# Win32IDE Unlinked + Dissolved Symbol Audit

- Log: `/workspace/source_audit/win32ide_production_link_latest.log`
- Total unresolved references: **445**
- Unique unresolved symbols: **188**

## Classification Summary

| Classification | Count |
|---|---:|
| `unlinked_despite_wired` | 99 |
| `unwired_non_stub` | 49 |
| `dissolved_or_external` | 40 |

## Unwired Non-Stub Symbols

| Symbol | Occurrences | Top ref object | Example unwired definition |
|---|---:|---|---|
| `MC_GapBuffer_Destroy` | 32 | `final_gauntlet.cpp.obj` | `/workspace/src/ai/digestion_scanner.asm` |
| `MC_GapBuffer_LineCount` | 20 | `MonacoCoreEngine.cpp.obj` | `/workspace/src/ai/digestion_scanner.asm` |
| `MC_GapBuffer_GetLine` | 12 | `MonacoCoreEngine.cpp.obj` | `/workspace/src/ai/digestion_scanner.asm` |
| `MC_GapBuffer_Insert` | 8 | `MonacoCoreEngine.cpp.obj` | `/workspace/include/RawrXD_MonacoCore.h` |
| `MC_GapBuffer_Length` | 8 | `final_gauntlet.cpp.obj` | `/workspace/src/ai/digestion_scanner.asm` |
| `MC_GapBuffer_Init` | 5 | `final_gauntlet.cpp.obj` | `/workspace/include/RawrXD_MonacoCore.h` |
| `SelfRepairLoop::registerDetour(char const*, void*)` | 5 | `camellia256_bridge.cpp.obj` | `/workspace/src/core/shadow_page_detour.cpp` |
| `MC_GapBuffer_Delete` | 4 | `MonacoCoreEngine.cpp.obj` | `/workspace/include/RawrXD_MonacoCore.h` |
| `SelfRepairLoop::isInitialized() const` | 4 | `auto_repair_orchestrator.cpp.obj` | `/workspace/src/core/shadow_page_detour.cpp` |
| `MultiResponseEngine::~MultiResponseEngine()` | 3 | `HeadlessIDE.cpp.obj` | `/workspace/src/core/multi_response_engine.cpp` |
| `SelfRepairLoop::getActiveDetourCount() const` | 3 | `auto_repair_orchestrator.cpp.obj` | `/workspace/src/core/shadow_page_detour.cpp` |
| `SelfRepairLoop::getDetours() const` | 3 | `auto_repair_orchestrator.cpp.obj` | `/workspace/src/core/shadow_page_detour.cpp` |
| `SelfRepairLoop::rollbackAll()` | 3 | `auto_repair_orchestrator.cpp.obj` | `/workspace/src/core/shadow_page_detour.cpp` |
| `SelfRepairLoop::getKernelStats() const` | 2 | `camellia256_bridge.cpp.obj` | `/workspace/src/core/shadow_page_detour.cpp` |
| `SelfRepairLoop::getSnapshotStats() const` | 2 | `camellia256_bridge.cpp.obj` | `/workspace/src/core/shadow_page_detour.cpp` |
| `SelfRepairLoop::initialize()` | 2 | `camellia256_bridge.cpp.obj` | `/workspace/src/core/shadow_page_detour.cpp` |
| `SelfRepairLoop::rollbackDetour(char const*)` | 2 | `auto_repair_orchestrator.cpp.obj` | `/workspace/src/core/shadow_page_detour.cpp` |
| `SelfRepairLoop::shutdown()` | 2 | `camellia256_bridge.cpp.obj` | `/workspace/src/core/shadow_page_detour.cpp` |
| `SelfRepairLoop::verifyAllDetours() const` | 2 | `auto_repair_orchestrator.cpp.obj` | `/workspace/src/core/shadow_page_detour.cpp` |
| `VecDb_Init` | 2 | `copilot_gap_closer.cpp.obj` | `/workspace/src/modules/copilot_gap_closer.h` |
| `VecDb_Search` | 2 | `Win32IDE_CopilotGapPanel.cpp.obj` | `/workspace/src/modules/copilot_gap_closer.h` |
| `Composer_BeginTransaction` | 1 | `Win32IDE_CopilotGapPanel.cpp.obj` | `/workspace/src/modules/copilot_gap_closer.h` |
| `Composer_Commit` | 1 | `Win32IDE_CopilotGapPanel.cpp.obj` | `/workspace/src/modules/copilot_gap_closer.h` |
| `Crdt_InitDocument` | 1 | `Win32IDE_CopilotGapPanel.cpp.obj` | `/workspace/src/modules/copilot_gap_closer.h` |
| `MultiResponseEngine::MultiResponseEngine()` | 1 | `HeadlessIDE.cpp.obj` | `/workspace/src/core/multi_response_engine.cpp` |
| `MultiResponseEngine::initialize()` | 1 | `HeadlessIDE.cpp.obj` | `/workspace/src/core/multi_response_engine.cpp` |
| `OSExplorerInterceptor::Initialize(unsigned long, void (*)(void*))` | 1 | `Win32IDE_OSExplorerInterceptor.cpp.obj` | `/workspace/src/win32app/OSExplorerInterceptor.cpp` |
| `OSExplorerInterceptor::OSExplorerInterceptor()` | 1 | `Win32IDE_OSExplorerInterceptor.cpp.obj` | `/workspace/src/win32app/OSExplorerInterceptor.cpp` |
| `OSExplorerInterceptor::StartInterception()` | 1 | `Win32IDE_OSExplorerInterceptor.cpp.obj` | `/workspace/src/win32app/OSExplorerInterceptor.cpp` |
| `OSExplorerInterceptor::~OSExplorerInterceptor()` | 1 | `Win32IDE_OSExplorerInterceptor.cpp.obj` | `/workspace/src/win32app/OSExplorerInterceptor.cpp` |
| `SelfRepairLoop::VerifyAndPatch(void*, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&)` | 1 | `unified_hotpatch_manager.cpp.obj` | `/workspace/src/core/shadow_page_detour.cpp` |
| `SelfRepairLoop::applyBinaryPatch(char const*, unsigned char const*, unsigned long long)` | 1 | `unified_hotpatch_manager.cpp.obj` | `/workspace/src/core/shadow_page_detour.cpp` |
| `VecDb_Delete` | 1 | `Win32IDE_CopilotGapPanel.cpp.obj` | `/workspace/src/modules/copilot_gap_closer.h` |
| `Win32IDE::handleDbgAttachEndpoint(unsigned long long, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&)` | 1 | `Win32IDE_LocalServer.cpp.obj` | `/workspace/src/win32app/Win32IDE_NativeDebugPanel.cpp` |
| `Win32IDE::handleDbgBreakpointsEndpoint(unsigned long long)` | 1 | `Win32IDE_LocalServer.cpp.obj` | `/workspace/src/win32app/Win32IDE_NativeDebugPanel.cpp` |
| `Win32IDE::handleDbgDisasmEndpoint(unsigned long long, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&)` | 1 | `Win32IDE_LocalServer.cpp.obj` | `/workspace/src/win32app/Win32IDE_NativeDebugPanel.cpp` |
| `Win32IDE::handleDbgEventsEndpoint(unsigned long long)` | 1 | `Win32IDE_LocalServer.cpp.obj` | `/workspace/src/win32app/Win32IDE_NativeDebugPanel.cpp` |
| `Win32IDE::handleDbgGoEndpoint(unsigned long long)` | 1 | `Win32IDE_LocalServer.cpp.obj` | `/workspace/src/win32app/Win32IDE_NativeDebugPanel.cpp` |
| `Win32IDE::handleDbgLaunchEndpoint(unsigned long long, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&)` | 1 | `Win32IDE_LocalServer.cpp.obj` | `/workspace/src/win32app/Win32IDE_NativeDebugPanel.cpp` |
| `Win32IDE::handleDbgMemoryEndpoint(unsigned long long, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&)` | 1 | `Win32IDE_LocalServer.cpp.obj` | `/workspace/src/win32app/Win32IDE_NativeDebugPanel.cpp` |
| `Win32IDE::handleDbgModulesEndpoint(unsigned long long)` | 1 | `Win32IDE_LocalServer.cpp.obj` | `/workspace/src/win32app/Win32IDE_NativeDebugPanel.cpp` |
| `Win32IDE::handleDbgRegistersEndpoint(unsigned long long)` | 1 | `Win32IDE_LocalServer.cpp.obj` | `/workspace/src/win32app/Win32IDE_NativeDebugPanel.cpp` |
| `Win32IDE::handleDbgStackEndpoint(unsigned long long)` | 1 | `Win32IDE_LocalServer.cpp.obj` | `/workspace/src/win32app/Win32IDE_NativeDebugPanel.cpp` |
| `Win32IDE::handleDbgStatusEndpoint(unsigned long long)` | 1 | `Win32IDE_LocalServer.cpp.obj` | `/workspace/src/win32app/Win32IDE_NativeDebugPanel.cpp` |
| `Win32IDE::handleDbgThreadsEndpoint(unsigned long long)` | 1 | `Win32IDE_LocalServer.cpp.obj` | `/workspace/src/win32app/Win32IDE_NativeDebugPanel.cpp` |
| `Win32IDE::handleDbgWatchesEndpoint(unsigned long long)` | 1 | `Win32IDE_LocalServer.cpp.obj` | `/workspace/src/win32app/Win32IDE_NativeDebugPanel.cpp` |
| `Win32IDE::handlePhase12StatusEndpoint(unsigned long long)` | 1 | `Win32IDE_LocalServer.cpp.obj` | `/workspace/src/win32app/Win32IDE_NativeDebugPanel.cpp` |
| `Win32IDE::handleReSetBinaryEndpoint(unsigned long long, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&)` | 1 | `Win32IDE_LocalServer.cpp.obj` | `/workspace/src/win32app/Win32IDE_NativeDebugPanel.cpp` |
| `g_VecDbNodes` | 1 | `copilot_gap_closer.cpp.obj` | `/workspace/src/modules/copilot_gap_closer.h` |

## Dissolved or External Symbols

| Symbol | Occurrences | Top ref object |
|---|---:|---|
| `Crdt_GetDocLength` | 5 | `Win32IDE_CopilotGapPanel.cpp.obj` |
| `VecDb_GetNodeCount` | 4 | `Win32IDE_CopilotGapPanel.cpp.obj` |
| `RawrXD::Debugger::NativeDebuggerEngine::addBreakpointBySourceLine(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, int)` | 3 | `Win32IDE_Debugger.cpp.obj` |
| `RawrXD::Debugger::NativeDebuggerEngine::launchProcess(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&)` | 3 | `Win32IDE_Debugger.cpp.obj` |
| `Composer_GetTxCount` | 2 | `copilot_gap_closer.cpp.obj` |
| `RawrXD::Debugger::NativeDebuggerEngine::enableBreakpoint(unsigned int, bool)` | 2 | `Win32IDE_Debugger.cpp.obj` |
| `RawrXD::Debugger::NativeDebuggerEngine::go()` | 2 | `Win32IDE_Debugger.cpp.obj` |
| `RawrXD::Debugger::NativeDebuggerEngine::initialize(RawrXD::Debugger::DebugConfig const&)` | 2 | `Win32IDE_Debugger.cpp.obj` |
| `RawrXD::Debugger::NativeDebuggerEngine::removeBreakpoint(unsigned int)` | 2 | `Win32IDE_Debugger.cpp.obj` |
| `RawrXD::Debugger::NativeDebuggerEngine::setBreakpointHitCallback(void (*)(RawrXD::Debugger::NativeBreakpoint const*, RawrXD::Debugger::RegisterSnapshot const*, void*), void*)` | 2 | `Win32IDE_Debugger.cpp.obj` |
| `RawrXD::Debugger::NativeDebuggerEngine::setOutputCallback(void (*)(char const*, unsigned int, void*), void*)` | 2 | `Win32IDE_Debugger.cpp.obj` |
| `RawrXD::Debugger::NativeDebuggerEngine::setStateCallback(void (*)(RawrXD::Debugger::DebugSessionState, void*), void*)` | 2 | `Win32IDE_Debugger.cpp.obj` |
| `RawrXD::Debugger::NativeDebuggerEngine::stepOver()` | 2 | `Win32IDE_Debugger.cpp.obj` |
| `RawrXD::Debugger::NativeDebuggerEngine::terminateTarget()` | 2 | `Win32IDE_Debugger.cpp.obj` |
| `VecDb_Insert` | 2 | `Win32IDE_CopilotGapPanel.cpp.obj` |
| `Composer_AddFileOp` | 1 | `Win32IDE_CopilotGapPanel.cpp.obj` |
| `Composer_GetState` | 1 | `copilot_gap_closer.cpp.obj` |
| `Crdt_DeleteText` | 1 | `Win32IDE_CopilotGapPanel.cpp.obj` |
| `Crdt_GetLamport` | 1 | `copilot_gap_closer.cpp.obj` |
| `Crdt_InsertText` | 1 | `Win32IDE_CopilotGapPanel.cpp.obj` |
| `Git_SetBranch` | 1 | `Win32IDE_CopilotGapPanel.cpp.obj` |
| `Git_SetCommitHash` | 1 | `Win32IDE_CopilotGapPanel.cpp.obj` |
| `RawrXD::Debugger::NativeDebuggerEngine::addBreakpointBySymbol(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, RawrXD::Debugger::BreakpointType)` | 1 | `feature_handlers.cpp.obj` |
| `RawrXD::Debugger::NativeDebuggerEngine::addWatch(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&)` | 1 | `Win32IDE_Debugger.cpp.obj` |
| `RawrXD::Debugger::NativeDebuggerEngine::breakExecution()` | 1 | `Win32IDE_Debugger.cpp.obj` |
| `RawrXD::Debugger::NativeDebuggerEngine::detach()` | 1 | `Win32IDE_Debugger.cpp.obj` |
| `RawrXD::Debugger::NativeDebuggerEngine::disassembleAt(unsigned long long, unsigned int, std::vector<RawrXD::Debugger::DisassembledInstruction, std::allocator<RawrXD::Debugger::DisassembledInstruction> >&)` | 1 | `Win32IDE_ReverseEngineering.cpp.obj` |
| `RawrXD::Debugger::NativeDebuggerEngine::getFrameLocals(unsigned int, std::map<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >, std::less<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > >, std::allocator<std::pair<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > > > >&)` | 1 | `Win32IDE_Debugger.cpp.obj` |
| `RawrXD::Debugger::NativeDebuggerEngine::getStats() const` | 1 | `feature_handlers.cpp.obj` |
| `RawrXD::Debugger::NativeDebuggerEngine::stepInto()` | 1 | `Win32IDE_Debugger.cpp.obj` |
| `RawrXD::Debugger::NativeDebuggerEngine::stepOut()` | 1 | `Win32IDE_Debugger.cpp.obj` |
| `RawrXD::Debugger::NativeDebuggerEngine::walkStack(std::vector<RawrXD::Debugger::NativeStackFrame, std::allocator<RawrXD::Debugger::NativeStackFrame> >&, unsigned int)` | 1 | `Win32IDE_Debugger.cpp.obj` |
| `RawrXD::Embeddings::EmbeddingEngine::indexDirectory(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, RawrXD::Embeddings::ChunkingConfig const&)` | 1 | `unified_hotpatch_manager.cpp.obj` |
| `RawrXD::Embeddings::EmbeddingEngine::loadModel(RawrXD::Embeddings::EmbeddingModelConfig const&)` | 1 | `unified_hotpatch_manager.cpp.obj` |
| `RawrXD::Embeddings::EmbeddingEngine::shutdown()` | 1 | `unified_hotpatch_manager.cpp.obj` |
| `_GUID const& __mingw_uuidof<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>()` | 1 | `webview2_bridge.cpp.obj` |
| `_GUID const& __mingw_uuidof<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>()` | 1 | `webview2_bridge.cpp.obj` |
| `__imp__dupenv_s` | 1 | `QuantumAuthUI.cpp.obj` |
| `g_VecDbEntryPoint` | 1 | `copilot_gap_closer.cpp.obj` |
| `g_VecDbMaxLevel` | 1 | `copilot_gap_closer.cpp.obj` |

## Top 40 Unresolved Symbols

| Symbol | Class | Occurrences | Top ref object |
|---|---|---:|---|
| `MC_GapBuffer_Destroy` | `unwired_non_stub` | 32 | `final_gauntlet.cpp.obj` |
| `SelfRepairLoop::instance()` | `unlinked_despite_wired` | 25 | `unified_hotpatch_manager.cpp.obj` |
| `MC_GapBuffer_LineCount` | `unwired_non_stub` | 20 | `MonacoCoreEngine.cpp.obj` |
| `Enterprise_GetLicenseStatus` | `unlinked_despite_wired` | 18 | `enterprise_license.cpp.obj` |
| `MC_GapBuffer_GetLine` | `unwired_non_stub` | 12 | `MonacoCoreEngine.cpp.obj` |
| `Enterprise_CheckFeature` | `unlinked_despite_wired` | 8 | `enterprise_license.cpp.obj` |
| `MC_GapBuffer_Insert` | `unwired_non_stub` | 8 | `MonacoCoreEngine.cpp.obj` |
| `MC_GapBuffer_Length` | `unwired_non_stub` | 8 | `final_gauntlet.cpp.obj` |
| `Crdt_GetDocLength` | `dissolved_or_external` | 5 | `Win32IDE_CopilotGapPanel.cpp.obj` |
| `MC_GapBuffer_Init` | `unwired_non_stub` | 5 | `final_gauntlet.cpp.obj` |
| `SelfRepairLoop::registerDetour(char const*, void*)` | `unwired_non_stub` | 5 | `camellia256_bridge.cpp.obj` |
| `Swarm_ComputeNodeFitness` | `unlinked_despite_wired` | 5 | `swarm_coordinator.cpp.obj` |
| `DiskRecovery_GetStats` | `unlinked_despite_wired` | 4 | `DiskRecoveryAgent.cpp.obj` |
| `MC_GapBuffer_Delete` | `unwired_non_stub` | 4 | `MonacoCoreEngine.cpp.obj` |
| `SelfRepairLoop::isInitialized() const` | `unwired_non_stub` | 4 | `auto_repair_orchestrator.cpp.obj` |
| `Swarm_BuildPacketHeader` | `unlinked_despite_wired` | 4 | `swarm_coordinator.cpp.obj` |
| `VecDb_GetNodeCount` | `dissolved_or_external` | 4 | `Win32IDE_CopilotGapPanel.cpp.obj` |
| `rawrxd_enumerate_modules_peb` | `unlinked_despite_wired` | 4 | `debugger_core.cpp.obj` |
| `rawrxd_walk_export_table` | `unlinked_despite_wired` | 4 | `debugger_core.cpp.obj` |
| `DiskRecovery_Cleanup` | `unlinked_despite_wired` | 3 | `DiskRecoveryAgent.cpp.obj` |
| `FlashAttention_GetTileConfig` | `unlinked_despite_wired` | 3 | `flash_attention.cpp.obj` |
| `KQuant_DequantizeQ4_K` | `unlinked_despite_wired` | 3 | `model_training_pipeline.cpp.obj` |
| `KQuant_DequantizeQ6_K` | `unlinked_despite_wired` | 3 | `model_training_pipeline.cpp.obj` |
| `MultiResponseEngine::~MultiResponseEngine()` | `unwired_non_stub` | 3 | `HeadlessIDE.cpp.obj` |
| `Quant_DequantQ4_0` | `unlinked_despite_wired` | 3 | `model_training_pipeline.cpp.obj` |
| `Quant_DequantQ8_0` | `unlinked_despite_wired` | 3 | `model_training_pipeline.cpp.obj` |
| `RawrXD::Debugger::NativeDebuggerEngine::addBreakpointBySourceLine(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, int)` | `dissolved_or_external` | 3 | `Win32IDE_Debugger.cpp.obj` |
| `RawrXD::Debugger::NativeDebuggerEngine::launchProcess(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&)` | `dissolved_or_external` | 3 | `Win32IDE_Debugger.cpp.obj` |
| `RawrXD::Embeddings::EmbeddingEngine::instance()` | `unlinked_despite_wired` | 3 | `unified_hotpatch_manager.cpp.obj` |
| `SelfRepairLoop::getActiveDetourCount() const` | `unwired_non_stub` | 3 | `auto_repair_orchestrator.cpp.obj` |
| `SelfRepairLoop::getDetours() const` | `unwired_non_stub` | 3 | `auto_repair_orchestrator.cpp.obj` |
| `SelfRepairLoop::rollbackAll()` | `unwired_non_stub` | 3 | `auto_repair_orchestrator.cpp.obj` |
| `Swarm_HeartbeatRecord` | `unlinked_despite_wired` | 3 | `swarm_coordinator.cpp.obj` |
| `Swarm_RingBuffer_Init` | `unlinked_despite_wired` | 3 | `swarm_coordinator.cpp.obj` |
| `Swarm_ValidatePacketHeader` | `unlinked_despite_wired` | 3 | `swarm_coordinator.cpp.obj` |
| `asm_camellia256_decrypt_block` | `unlinked_despite_wired` | 3 | `camellia256_bridge.cpp.obj` |
| `asm_camellia256_decrypt_ctr` | `unlinked_despite_wired` | 3 | `camellia256_bridge.cpp.obj` |
| `asm_camellia256_encrypt_block` | `unlinked_despite_wired` | 3 | `camellia256_bridge.cpp.obj` |
| `asm_camellia256_encrypt_ctr` | `unlinked_despite_wired` | 3 | `camellia256_bridge.cpp.obj` |
| `asm_camellia256_get_hmac_key` | `unlinked_despite_wired` | 3 | `camellia256_bridge.cpp.obj` |

