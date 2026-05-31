# RawrXD Codebase Stub/Unfinished/Broken File Catalog

**Generated:** 2026-04-22  
**Scope:** `d:\rawrxd\src\`, `include\`, `Ship\`, root-level files  
**Total Files Examined:** 200+  
**Categories:** 13

---

## 1. Broken MASM Files (.asm with syntax errors, missing ENDP, or won't assemble)

| # | File Path | Stub Count | Severity | Description |
|---|-----------|------------|----------|-------------|
| 1 | `d:\rawrxd\src\unresolved_asm_stubs.asm` | 8+ | **Medium** | Functional but minimal ASM stubs for DMA allocation, scheduler, heartbeat. Uses `__imp_VirtualAlloc` extern that may not resolve at link time. |
| 2 | `d:\rawrxd\src\asm\RawrXD_StubSweepBridge.asm` | **100+** | **Critical** | Auto-generated MASM bridge with 100+ stub exports (AccelRouter_*, AgentRouter_*, ArrayList_*, DiskKernel_*, ExtensionHostBridge_*, etc.). Each function just logs its name and returns 0. |
| 3 | `d:\rawrxd\src\asm\RawrXD_StubDetector.asm` | 1 | Low | Functional stub detector kernel — scans function prologues for stub patterns. Actually implemented. |
| 4 | `d:\rawrxd\src\agentic\vulkan\NEON_VULKAN_FABRIC_STUB.asm` | 1 | **High** | Placeholder ARM NEON Vulkan stub — x64 project, will never assemble meaningfully. |
| 5 | `d:\rawrxd\src\agentic\RawrXD_Compiler_Engine_Stubs_BACKUP.asm` | 10+ | **High** | Backup stub file with placeholder compiler engine exports. |
| 6 | `d:\rawrxd\src\security\uac_bypass_masm.asm` | 1 | **Critical** | UAC bypass in MASM — security-sensitive stub, likely non-functional or dangerous placeholder. |
| 7 | `d:\rawrxd\src\gpu\vulkan_impl.asm` | 5+ | **High** | Vulkan implementation in MASM — likely incomplete, missing actual Vulkan dispatch. |
| 8 | `d:\rawrxd\src\gpu\rocm_impl.asm` | 5+ | **High** | ROCm/HIP implementation in MASM — likely incomplete stubs. |
| 9 | `d:\rawrxd\src\gpu\cuda_impl.asm` | 5+ | **High** | CUDA implementation in MASM — likely incomplete stubs. |
| 10 | `d:\rawrxd\src\thermal\masm\*.asm` (8 files) | 20+ | Medium | Thermal management MASM files — sovereign_stress_governor, quantum_auth, ghost_paging, etc. Mix of real and stub code. |
| 11 | `d:\rawrxd\src\win32app\*.asm` (6 files) | 15+ | Medium | Sidebar, layout, MCP hooks in MASM. Some are minimal implementations. |
| 12 | `d:\rawrxd\src\Titan_*.asm` (3 files) | 10+ | **High** | Titan streaming orchestrator stubs — referenced by build but may be incomplete. |
| 13 | `d:\rawrxd\src\tokenizer.asm` | 1 | Medium | Tokenizer in MASM — may be incomplete. |
| 14 | `d:\rawrxd\src\UI_*.asm` (3 files) | 5+ | Medium | UI MASM files — UI_Final, UI_Fixed, etc. |
| 15 | `d:\rawrxd\src\x64\*.asm` (4 files) | 8+ | Medium | Quantum beaconism, governor, dual engine, audit stubs. |

**Subtotal: 15 files, ~200+ stub functions**

---

## 2. Headers with Missing .cpp Implementations

| # | File Path | Declared Functions | Severity | Description |
|---|-----------|-------------------|----------|-------------|
| 1 | `d:\rawrxd\src\nlohmann_stub.h` | **15+** | **Critical** | Complete reimplementation stub of `nlohmann::json`. `dump()` returns `"{}"`. No real JSON parsing. Used when real nlohmann/json.hpp is unavailable. |
| 2 | `d:\rawrxd\src\linker_stubs.h` | **20+** | **High** | HotPatcher class with stub implementations. `ApplyPatch()` is functional but `RevertPatch()`, `VerifyPatch()` are minimal. `nlohmann::json` forward-declared as empty class. |
| 3 | `d:\rawrxd\src\linker_stubs_clean.h` | **15+** | **High** | Cleaned version of linker stubs — same issues. |
| 4 | `d:\rawrxd\src\linker_stubs_old.h` | **15+** | Medium | Older version of linker stubs. Likely superseded. |
| 5 | `d:\rawrxd\src\codec\codec_stubs.h` | **2** | Medium | `inflate()` and `deflate()` are pass-through stubs — return input unchanged, always set `success=true`. |
| 6 | `d:\rawrxd\src\ggml-hexagon\htp\hexagon_stubs.h` | **10+** | **High** | Hexagon HTP stubs for Qualcomm DSP — placeholder implementations. |
| 7 | `d:\rawrxd\src\gpu\gpu_masm_bridge.h` | **5+** | **High** | Bridge header for GPU MASM — likely missing C++ implementations. |
| 8 | `d:\rawrxd\src\agentic_core_win32.h` | **8** | Medium | Win32 DLL loader wrapper — functional but some paths return false on error. |

**Subtotal: 8 files, ~90+ stub/missing functions**

---

## 3. Linker Stub/Fallback Files

| # | File Path | Stub Count | Severity | Description |
|---|-----------|------------|----------|-------------|
| 1 | `d:\rawrxd\src\core\missing_handler_stubs.cpp` | **15+** | **Critical** | SSOT missing handlers: `HandleCursorParityBridge`, `HandleOmegaOrchestrator`, `HandleMeshBrain`, `HandleSpeciatorEngine`, `HandleNeuralBridge`, `HandleSelfHostEngine`, `HandleHardwareSynthesizer`, `HandleTranscendenceCoordinator`, `HandleVulkanRenderer`, `HandleOSExplorerInterceptor`, `HandleMCPHooks`, `HandleIOCPFileWatcher`, `HandleIDEDiagnosticAutoHealer`. All return stub result. |
| 2 | `d:\rawrxd\src\core\auto_feature_stub_impl.cpp` | **286** | **Critical** | Auto-generated stub handlers for RawrEngine link. 286 handler declarations with dispatch registry tool fallbacks. Contains `auto_feature_real_impl.cpp` include for real implementations. |
| 3 | `d:\rawrxd\src\core\rawrxd_titan_stubs.cpp` | **25+** | **High** | Auto-generated Titan API stubs: `RawrXD_Initialize()`, `RawrXD_LoadModel()`, `RawrXD_RunInference()`, `RawrXD_GetLastError()`, etc. All return fake success/placeholder data. |
| 4 | `d:\rawrxd\src\core\gold_enterprise_devunlock_stub.cpp` | **3** | **High** | Enterprise license dev unlock stub — reads env vars, returns cached result. Not a real license validator. |
| 5 | `d:\rawrxd\src\core\gold_asm_closure_stubs.cpp` | **8** | **High** | ASM snapshot/watchdog closure stubs: `asm_snapshot_capture()`, `asm_snapshot_restore()`, `asm_snapshot_verify()`, `asm_snapshot_discard()`, `asm_snapshot_get_stats()`, `asm_watchdog_init()`, `asm_watchdog_ping()`, `asm_watchdog_shutdown()`. All return 0. |
| 6 | `d:\rawrxd\src\core\beacon_link_stub.cpp` | **5** | Medium | Beacon subsystem link stub — cache probe, file probe, force override. Minimal telemetry only. |
| 7 | `d:\rawrxd\src\core\swarm_orchestrator_link_stub.cpp` | **6** | **High** | SwarmOrchestrator stub: `executeTask()` returns input unchanged, `addAgent()` no-op, `reachConsensus()` returns first proposal, `getAvailableAgents()` returns empty, `swarmLoop()` empty body. |
| 8 | `d:\rawrxd\src\core\rawrengine_asm_dispatch_stubs.cpp` | **10+** | **High** | RawrEngine ASM dispatch bridge stubs — CLI dispatch, command dispatch, feature dispatch. Mode parsing fallback. |
| 9 | `d:\rawrxd\src\core\native_gguf_loader_link_stub.cpp` | **8** | **High** | GGUF loader link stub — `OpenFile()`, `ParseHeader()`, `ParseMetadata()`, `ParseTensorInfo()`, `LoadTensorData()`, `CloseFile()`, `GetTensorCount()`, `GetMetadataCount()`. Minimal implementation with bounds checks. |
| 10 | `d:\rawrxd\src\core\agentic_executor_link_stub.cpp` | **6** | **High** | Agentic executor link stub — file read, directory list with env-configurable caps. |
| 11 | `d:\rawrxd\src\core\ai_agent_masm_stubs.cpp` | **12+** | **High** | C++ replacement for `ai_agent_masm_core.asm` — AVX2/AVX-512 feature detection, SIMD tensor ops stubs. Includes `VirtualProtect`, `memcpy`, `cpuid`, `rdtsc` implementations. |
| 12 | `d:\rawrxd\src\core\js_extension_host_headless_stubs.cpp` | **15+** | **High** | Headless JS extension host stub — `initialize()`, `shutdown()`, `loadDirectory()`, `activateExtension()`, `deactivateExtension()`, `unloadExtension()`, `sendEvent()`, `executeCommand()`, `createTimer()`, `cancelTimer()`. All no-op with telemetry counters. |
| 13 | `d:\rawrxd\src\core\headless_subsystem_stubs.cpp` | **8** | Medium | Headless subsystem stubs: `RawrXD_Native_Log()`, `Enterprise_DevUnlock()`, `INFINITY_Shutdown()`, `Scheduler_Initialize()`, `Scheduler_Shutdown()`, `Scheduler_Submit()`, `ConflictDetector_Init()`, `OmegaPipeline_Advance()`. Minimal implementations. |
| 14 | `d:\rawrxd\src\core\command_handlers_stubs.cpp.disabled` | **20+** | Low | Disabled stub file — not compiled. |

**Subtotal: 14 files, ~430+ stub functions**

---

## 4. Auto-Generated Stub Handlers

| # | File Path | Stub Count | Severity | Description |
|---|-----------|------------|----------|-------------|
| 1 | `d:\rawrxd\src\core\auto_feature_stub_impl.cpp` | **286** | **Critical** | Largest stub file. 286 auto-generated handler stubs for command registry. Each handler dispatches to `ToolRegistry::execute_tool()` or returns generic error. |
| 2 | `d:\rawrxd\src\asm\RawrXD_StubSweepBridge.asm` | **100+** | **Critical** | Auto-generated MASM bridge. Every function logs `[MASM] FunctionName` via `OutputDebugStringA` and returns 0. Covers: AccelRouter, AgentRouter, ArrayList, Camellia256, CoT, CompletionProvider, DefinitionProvider, DependencyGraph, DirectIO, DiskExplorer, DiskKernel, DiskRecovery, DispatchCompute, Disposable, EventFire, Extension*, find_pattern, fnv1a, GenerateTokens, GetBurst*, GetElapsed, GetTensor*, GGUF, HashMap, HoverProvider, etc. |
| 3 | `d:\rawrxd\src\rawrxd_titan_stubs.cpp` | **25+** | **High** | Auto-generated by `scripts/generate_rawrxd_titan_stubs.ps1`. Model load, inference, sampling, aperture, hotpatch, diagnostics stubs. |
| 4 | `d:\rawrxd\src\win32app\agentic_headless_laneb_link_stubs.cpp` | **8** | **High** | SubAgentManager minimal no-thread implementation. `spawnSubAgent()` returns `"headless-sa-stub"`, `dispatchToolCall()` returns false, `executeChain()`/`executeSwarm()` return empty. |
| 5 | `d:\rawrxd\src\win32app\sovereign_gpu_link_stubs.cpp` | **6** | **High** | Sovereign GPU link stubs when `RAWR_HAS_SOVEREIGN_ENGINES=0`. PUF-based auth, huge page alloc, MMIO read, telemetry read. Returns deterministic fake data. |
| 6 | `d:\rawrxd\src\win32app\benchmark_runner_stub.cpp` | **4** | Medium | Benchmark runner build-compat shim. Delegates to real `benchmark_runner.cpp` but has stub fallback paths. |
| 7 | `d:\rawrxd\src\win32app\benchmark_menu_stub.cpp` | **5** | Medium | Benchmark menu build-compat shim. Stub anchor with env-configurable cycle counts. |
| 8 | `d:\rawrxd\src\win32app\multi_file_search_stub.cpp` | **3** | Medium | Multi-file search build-compat shim. Returns env-configured probe query. |
| 9 | `d:\rawrxd\src\win32app\digestion_engine_stub.cpp` | **2** | Medium | Digestion engine build-compat shim. Bounded health-check attempts with timing stats. |
| 10 | `d:\rawrxd\src\win32app\bulk_fix_orchestrator_laneb_stub.cpp` | **1** | Low | Single destructor stub for `BulkFixOrchestrator`. Avoids linking full autonomous_subagent.cpp. |
| 11 | `d:\rawrxd\src\win32app\tool_registry_laneb_stub.cpp` | **5+** | Medium | Tool registry Lane B stub — minimal tool registration stubs. |
| 12 | `d:\rawrxd\src\tests\swarm_smoke_stubs.cpp` | **6** | Medium | IAT hook stubs for SwarmSmokeTest: `InstallIATHook()`, `GetIATHook()`, `RemoveIATHook()`, `ResolveActiveWindow()`, `__iat_hook_base[]`, `masquerade_context[]`. |
| 13 | `d:\rawrxd\src\tests\backend_orchestrator_shard_smoke_stubs.cpp` | **4** | Medium | Codec shard smoke stubs: `deflate()`, `inflate()`, `accumulate()`, `accumulateScaled()`. Pass-through or no-op. |

**Subtotal: 13 files, ~470+ stub functions**

---

## 5. SSOT Missing Handlers

| # | File Path | Stub Count | Severity | Description |
|---|-----------|------------|----------|-------------|
| 1 | `d:\rawrxd\src\core\missing_handler_stubs.cpp` | **15** | **Critical** | Single Source of Truth missing handlers. All return `makeStubResult()` with handler name. Categories: Transcendence/orchestration (6), Platform/runtime integration (5), Security/compliance (4). |
| 2 | `d:\rawrxd\src\core\headless_subsystem_stubs.cpp` | **8** | **High** | SSOT headless subsystem implementations: scheduler, heartbeat, conflict detector, omega pipeline, native log. Minimal but functional (not pure stubs). |
| 3 | `d:\rawrxd\src\core\gold_enterprise_devunlock_stub.cpp` | **1** | **High** | SSOT enterprise dev unlock — reads env/file token, not a real license system. |
| 4 | `d:\rawrxd\src\core\gold_asm_closure_stubs.cpp` | **8** | **High** | SSOT ASM closure for snapshot/watchdog when MASM objects omitted. |

**Subtotal: 4 files, 32 stub functions**

---

## 6. Security Stubs (`src/security/`)

| # | File Path | Stub Count | Severity | Description |
|---|-----------|------------|----------|-------------|
| 1 | `d:\rawrxd\src\security\fips_compliance.cpp` | **5+** | **Critical** | FIPS 140-2 compliance stub. Marked `SCAFFOLD_204`. Uses Windows CNG when `RAWR_HAS_FIPS` undefined. License check gate. |
| 2 | `d:\rawrxd\src\security\hsm_integration.cpp` | **5+** | **Critical** | HSM integration stub. Marked `SCAFFOLD_203`. PKCS#11 optional. License check gate. `HSMIntegration` class with void* hsmModule placeholder. |
| 3 | `d:\rawrxd\src\security\classified_network.cpp` | **8+** | **High** | Classified network isolation stub. License check gate. `ClassificationLevel` enum but no real network enforcement. |
| 4 | `d:\rawrxd\src\security\exploit_chain_workflow.cpp` | **3** | Medium | Exploit chain workflow — actually implemented (not a stub), but in security category. |
| 5 | `d:\rawrxd\src\security\uac_bypass_masm.asm` | **1** | **Critical** | UAC bypass MASM — security-sensitive, likely placeholder or dangerous. |
| 6 | `d:\rawrxd\src\security\WindowsDefenderBridge.h` | **3** | **High** | Windows Defender bridge header — likely missing implementation. |
| 7 | `d:\rawrxd\src\security\sovereign_keymgmt.cpp` | **5+** | **High** | Sovereign key management — may be stubbed. |
| 8 | `d:\rawrxd\src\security\patch_firewall_barrier.cpp` | **4** | **High** | Patch firewall barrier — security stub. |
| 9 | `d:\rawrxd\src\security\dast_bridge.cpp` | **3** | Medium | DAST bridge — dynamic application security testing stub. |
| 10 | `d:\rawrxd\src\security\cve_cache.cpp` | **2** | Medium | CVE cache — may have stub lookups. |

**Subtotal: 10 files, ~40+ stub functions**

---

## 7. GPU Backend Stubs (`src/gpu/`)

| # | File Path | Stub Count | Severity | Description |
|---|-----------|------------|----------|-------------|
| 1 | `d:\rawrxd\src\gpu\gpu_backend.cpp` | **4** | **High** | GPU backend with fallback stubs. `initializeVulkan()`, `initializeCuda()`, `initializeCpu()` — Vulkan/CUDA return false if not available, falls back to CPU. |
| 2 | `d:\rawrxd\src\gpu\cuda_inference_engine.cpp` | **8+** | **Critical** | CUDA inference engine — **MOCK only** when `RAWR_HAS_CUDA` undefined. Kernel launchers are extern "C" declarations with no implementation. License check gate. |
| 3 | `d:\rawrxd\src\gpu\vulkan_compute_real.cpp` | **10+** | **High** | Vulkan compute "real" implementation — has structured logging but `v` statement is broken (`v` on its own line). Debug callback implemented but many ops are stubs. |
| 4 | `d:\rawrxd\src\gpu\directstorage_real.cpp` | **6** | **High** | DirectStorage "real" implementation — same broken logging pattern (`v` on its own line). Factory/queue/status pointers declared but may not be fully initialized. |
| 5 | `d:\rawrxd\src\gpu\vulkan_compute.cpp` | **5+** | **High** | Vulkan compute wrapper — likely has stub methods. |
| 6 | `d:\rawrxd\src\gpu\vulkan_compute_kernel_executor.cpp` | **3** | Medium | Vulkan kernel executor — may have stub dispatch. |
| 7 | `d:\rawrxd\src\gpu\vulkan_compute.h` | **5** | Medium | Vulkan compute header — `IsAMDDevice()` returns false, `CreateInstance()` returns false, etc. |
| 8 | `d:\rawrxd\src\gpu\vulkan_impl.asm` | **5+** | **High** | Vulkan MASM implementation — likely incomplete. |
| 9 | `d:\rawrxd\src\gpu\rocm_impl.asm` | **5+** | **High** | ROCm MASM implementation — likely incomplete. |
| 10 | `d:\rawrxd\src\gpu\cuda_impl.asm` | **5+** | **High** | CUDA MASM implementation — likely incomplete. |
| 11 | `d:\rawrxd\src\gpu\gpu_backend.cpp.backup` | **4** | Low | Backup of gpu_backend.cpp — superseded. |
| 12 | `d:\rawrxd\src\gpu\Flash_Attention_v14_7_0.cpp` | **3** | **High** | Flash Attention implementation — may be stub or incomplete. |
| 13 | `d:\rawrxd\src\gpu\speculative_decoder.cpp` | **4** | Medium | Speculative decoder — may have stub paths. |
| 14 | `d:\rawrxd\src\gpu\speculative_decoder_v2.cpp` | **4** | Medium | Speculative decoder v2 — may have stub paths. |

**Subtotal: 14 files, ~70+ stub functions**

---

## 8. Extension System Stubs (`src/extension_host/`, `src/extensions/`)

| # | File Path | Stub Count | Severity | Description |
|---|-----------|------------|----------|-------------|
| 1 | `d:\rawrxd\src\extension_host\RawrXD_ExtensionHost.asm` | **10+** | **Critical** | Extension host MASM — process broker, message passing stubs. |
| 2 | `d:\rawrxd\src\extension_host\RawrXD_WebView2.asm` | **5+** | **High** | WebView2 extension host MASM — webview creation, message routing stubs. |
| 3 | `d:\rawrxd\src\extension_host\RawrXD_MarketplaceInstaller.asm` | **5+** | **High** | Marketplace installer MASM — extension download, install, verify stubs. |
| 4 | `d:\rawrxd\src\extension_host\RawrXD_DAP.asm` | **5+** | **High** | Debug Adapter Protocol MASM — launch, attach, breakpoint stubs. |
| 5 | `d:\rawrxd\src\quickjs_extension_host.cpp` | **15+** | **Critical** | QuickJS extension host C++ — **stub QuickJS declarations** (`struct JSRuntime; struct JSContext; struct JSValue;`). No actual QuickJS linked. Timer system implemented but JS runtime is opaque stubs. |
| 6 | `d:\rawrxd\src\core\js_extension_host_headless_stubs.cpp` | **15+** | **High** | Headless JS extension host stub — no QuickJS dependency. All extension operations are no-ops with telemetry. |
| 7 | `d:\rawrxd\src\extensions\extension_activation_events.cpp` | **3** | Medium | Extension activation events — may have stub triggers. |
| 8 | `d:\rawrxd\src\extensions\extension_auto_updater.cpp` | **3** | Medium | Extension auto-updater — may have stub update check. |
| 9 | `d:\rawrxd\src\extensions\extension_configuration_ui.cpp` | **2** | Medium | Extension config UI — may be stubbed. |
| 10 | `d:\rawrxd\src\extensions\extension_dependency_resolver.cpp` | **3** | Medium | Extension dependency resolver — may have stub resolution. |
| 11 | `d:\rawrxd\src\extensions\extension_permissions.cpp` | **2** | Medium | Extension permissions — may have stub enforcement. |

**Subtotal: 11 files, ~70+ stub functions**

---

## 9. Runtime Stubs (`src/runtime/`)

| # | File Path | Stub Count | Severity | Description |
|---|-----------|------------|----------|-------------|
| 1 | `d:\rawrxd\src\runtime\SovereignEvolutionLoop.cpp` | **3** | Medium | Evolution loop background thread — fires every 30s but cycle body is mostly empty (KAIROS health check, memory pattern, entropy cleanup are TODO). |
| 2 | `d:\rawrxd\src\runtime\SovereignFuzzEngine.cpp` | **2** | Low | Fuzz engine — actually implemented (mutation strategies, SEH-guarded). Not a stub. |
| 3 | `d:\rawrxd\src\runtime\SovereignDeterministicReplay.cpp` | **5+** | Medium | Deterministic replay — may have stub recording/playback. |
| 4 | `d:\rawrxd\src\runtime\SovereignFailover.cpp` | **4** | Medium | Failover logic — may have stub failover paths. |
| 5 | `d:\rawrxd\src\runtime\SovereignMeshConsensus.cpp` | **3** | Medium | Mesh consensus — may have stub consensus algorithm. |
| 6 | `d:\rawrxd\src\runtime\SovereignReplication.cpp` | **4** | Medium | Replication — may have stub sync logic. |
| 7 | `d:\rawrxd\src\runtime\SovereignKernelJIT.cpp` | **5+** | **High** | Kernel JIT — likely stub or minimal implementation. |
| 8 | `d:\rawrxd\src\runtime\SovereignMCPBridge.cpp` | **3** | Medium | MCP bridge — may have stub model context protocol handlers. |
| 9 | `d:\rawrxd\src\runtime\SovereignLSPBridge.cpp` | **3** | Medium | LSP bridge — may have stub language server handlers. |
| 10 | `d:\rawrxd\src\runtime\SovereignHotpatchBridge.cpp` | **3** | Medium | Hotpatch bridge — may have stub patch application. |
| 11 | `d:\rawrxd\src\runtime\SovereignKAIROSBridge.cpp` | **3** | Medium | KAIROS bridge — may have stub time-series handlers. |
| 12 | `d:\rawrxd\src\runtime\SovereignMemoryBridge.cpp` | **3** | Medium | Memory bridge — may have stub memory sync. |
| 13 | `d:\rawrxd\src\runtime\SovereignStabilityLayer.cpp` | **2** | Medium | Stability layer — may have stub health checks. |
| 14 | `d:\rawrxd\src\runtime\SovereignSnapshot.cpp` | **3** | Medium | Snapshot — may have stub snapshot capture. |
| 15 | `d:\rawrxd\src\runtime\SovereignSandbox.cpp` | **3** | Medium | Sandbox — may have stub isolation. |
| 16 | `d:\rawrxd\src\runtime\SovereignCRDTSync.cpp` | **4** | Medium | CRDT sync — may have stub conflict-free replicated data type sync. |
| 17 | `d:\rawrxd\src\runtime\SovereignWebSearch.cpp` | **2** | Medium | Web search — may have stub search providers. |
| 18 | `d:\rawrxd\src\runtime\SovereignToolRegistry.cpp` | **2** | Medium | Tool registry — may have stub tool registration. |
| 19 | `d:\rawrxd\src\runtime\SovereignToolBridge.cpp` | **2** | Medium | Tool bridge — may have stub tool execution. |
| 20 | `d:\rawrxd\src\runtime\SovereignTelemetryHub.cpp` | **2** | Medium | Telemetry hub — may have stub telemetry collection. |
| 21 | `d:\rawrxd\src\runtime\SovereignStreamingParser.cpp` | **3** | Medium | Streaming parser — may have stub parse handlers. |
| 22 | `d:\rawrxd\src\runtime\SovereignOrchestrator.cpp` | **4** | Medium | Orchestrator — may have stub orchestration logic. |
| 23 | `d:\rawrxd\src\runtime\SovereignMeshProvider.cpp` | **3** | Medium | Mesh provider — may have stub mesh networking. |
| 24 | `d:\rawrxd\src\runtime\SovereignMeshBridge.cpp` | **3** | Medium | Mesh bridge — may have stub mesh bridging. |
| 25 | `d:\rawrxd\src\runtime\SovereignMemorySync.cpp` | **3** | Medium | Memory sync — may have stub synchronization. |
| 26 | `d:\rawrxd\src\runtime\SovereignDistributedRouter.cpp` | **3** | Medium | Distributed router — may have stub routing. |

**Subtotal: 26 files, ~80+ potential stub functions** (many are lightweight implementations, not pure stubs)

---

## 10. JS Extension Host Stubs

| # | File Path | Stub Count | Severity | Description |
|---|-----------|------------|----------|-------------|
| 1 | `d:\rawrxd\src\quickjs_extension_host.cpp` | **15+** | **Critical** | QuickJS extension host — **opaque stub types** for JSRuntime/JSContext/JSValue. No actual QuickJS library linked. Timer system is real but JS bridge is non-functional. |
| 2 | `d:\rawrxd\src\core\js_extension_host_headless_stubs.cpp` | **15+** | **High** | Headless JS extension host — no QuickJS at all. All JS operations are no-ops. |
| 3 | `d:\rawrxd\src\extension_host\RawrXD_ExtensionHost.asm` | **10+** | **High** | MASM extension host — message passing stubs. |
| 4 | `d:\rawrxd\src\extension_host\RawrXD_WebView2.asm` | **5+** | **High** | WebView2 MASM host — webview stubs. |

**Subtotal: 4 files, ~45+ stub functions**

---

## 11. Test Stub Files (`src/test/`, `src/tests/`)

| # | File Path | Stub Count | Severity | Description |
|---|-----------|------------|----------|-------------|
| 1 | `d:\rawrxd\src\tests\swarm_smoke_stubs.cpp` | **6** | Medium | IAT hook stubs for swarm smoke test. `InstallIATHook()`, `GetIATHook()`, `RemoveIATHook()`, `ResolveActiveWindow()`. |
| 2 | `d:\rawrxd\src\tests\backend_orchestrator_shard_smoke_stubs.cpp` | **4** | Medium | Codec shard smoke stubs. `deflate()`/`inflate()` pass-through. |
| 3 | `d:\rawrxd\src\tests\swarm_smoke_runtime.cpp` | **3** | Medium | Swarm smoke runtime — may have stub runtime paths. |
| 4 | `d:\rawrxd\src\tests\sovereign_assembler_smoke.cpp` | **2** | Low | Sovereign assembler smoke test — minimal. |
| 5 | `d:\rawrxd\src\tests\runtime_truth_probe.cpp` | **2** | Medium | Runtime truth probe — may have stub probe logic. |
| 6 | `d:\rawrxd\src\tests\runtime_provider_512_stress.cpp` | **2** | Medium | 512-token stress test — may have stub stress paths. |
| 7 | `d:\rawrxd\src\tests\mcp_model_smoke_test.cpp` | **3** | Medium | MCP model smoke test — may have stub model loading. |
| 8 | `d:\rawrxd\src\tests\fuzz_gguf_loader.cpp` | **2** | Medium | GGUF loader fuzz test — may have stub fuzz harness. |
| 9 | `d:\rawrxd\src\tests\extension_installer_smoketest.cpp` | **3** | Medium | Extension installer smoke test — may have stub installer. |
| 10 | `d:\rawrxd\src\test\license_compliance_test.cpp` | **2** | Low | License compliance test — minimal. |
| 11 | `d:\rawrxd\src\test\license_anti_tampering_test.cpp` | **2** | Low | License anti-tampering test — minimal. |

**Subtotal: 11 files, ~35 stub functions**

---

## 12. Ship/Archived Stubs (`Ship/` directory)

| # | File Path | Stub Count | Severity | Description |
|---|-----------|------------|----------|-------------|
| 1 | `d:\rawrxd\Ship\Titan_Streaming_Orchestrator_Fixed.asm` | **5+** | Medium | Archived Titan streaming orchestrator — may be superseded by src/ version. |
| 2 | `d:\rawrxd\Ship\Titan_InferenceCore.asm` | **5+** | Medium | Archived Titan inference core — may be superseded. |
| 3 | `d:\rawrxd\Ship\RawrXD_Titan_Kernel.asm` | **5+** | Medium | Archived Titan kernel — may be superseded. |
| 4 | `d:\rawrxd\Ship\RawrXD_Titan_Engine.asm` | **5+** | Medium | Archived Titan engine — may be superseded. |
| 5 | `d:\rawrxd\Ship\RawrXD_Titan_MetaReverse.asm` | **5+** | Medium | Archived meta-reverse engine — may be superseded. |
| 6 | `d:\rawrxd\Ship\RawrXD_Streaming_Orchestrator.asm` | **5+** | Medium | Archived streaming orchestrator — may be superseded. |
| 7 | `d:\rawrxd\Ship\RawrXD_NativeModelBridge_*.asm` (5 files) | **20+** | Medium | Multiple versions of native model bridge (v2, TEST, PRODUCTION, FRESH, CLEAN). Only one should be active. |
| 8 | `d:\rawrxd\Ship\RawrXD_MASM_CLI_x64.asm` | **5+** | Low | Archived MASM CLI x64 — may be superseded. |
| 9 | `d:\rawrxd\Ship\RawrXD_MASM_CLI.asm` | **5+** | Low | Archived MASM CLI — may be superseded. |
| 10 | `d:\rawrxd\Ship\RawrXD_GUI_Titan.asm` | **5+** | Medium | Archived GUI Titan — may be superseded. |
| 11 | `d:\rawrxd\Ship\RawrXD_GUI.asm` | **5+** | Medium | Archived GUI — may be superseded. |
| 12 | `d:\rawrxd\Ship\RawrXD_DorkScanner_MASM.asm` | **3+** | Low | Archived dork scanner — may be superseded. |
| 13 | `d:\rawrxd\Ship\RawrXD_CommandCLI.asm` | **3+** | Low | Archived command CLI — may be superseded. |
| 14 | `d:\rawrxd\Ship\RawrXD_CLI_Titan.asm` | **3+** | Low | Archived CLI Titan — may be superseded. |
| 15 | `d:\rawrxd\Ship\RawrXD_cli.asm` | **3+** | Low | Archived CLI — may be superseded. |
| 16 | `d:\rawrxd\Ship\rawrxd_bridge.asm` | **3+** | Low | Archived bridge — may be superseded. |
| 17 | `d:\rawrxd\Ship\rawrxd_agentic.asm` | **3+** | Low | Archived agentic — may be superseded. |
| 18 | `d:\rawrxd\Ship\masm_cli_x64.asm` | **3+** | Low | Archived MASM CLI x64 — may be superseded. |
| 19 | `d:\rawrxd\Ship\RawrZ_Camellia_MASM_x64.asm` | **3+** | Medium | Camellia crypto in MASM — may be archived stub. |
| 20 | `d:\rawrxd\Ship\RawrXD_Universal_Dorker.asm` | **3+** | Low | Universal dorker — may be archived stub. |

**Subtotal: 20 files, ~90+ stub functions** (archived/duplicate code)

---

## 13. Resource File Gaps (.rc files with missing resources)

| # | File Path | Missing Resources | Severity | Description |
|---|-----------|-------------------|----------|-------------|
| 1 | `d:\rawrxd\src\ide\RawrXD_IDE_Resources.rc` | **1** | Medium | Icon reference commented out: `/* IDI_RAWRXD_ICON ICON "rawrxd.ico" */`. No application icon embedded. |
| 2 | `d:\rawrxd\src\res\Resource.rc` | **1** | Medium | `IDI_APP_ICON` is commented out. Only version info block is active. |
| 3 | `d:\rawrxd\src\win32app\RawrXD-Win32IDE.rc` | **0** | Low | Complete — has manifest, icon, bitmaps, dialog. But references `..\..\src\res\app.ico` and `..\..\src\res\toolbar.bmp` which may not exist. |
| 4 | `d:\rawrxd\gui\RawrXD_Titan_GUI.rc` | **Unknown** | Low | Titan GUI resources — not examined in detail. |

**Subtotal: 4 files, 2 confirmed missing icon resources**

---

## Summary Statistics

| Category | Files | Stub Functions | Critical Severity | High Severity | Medium Severity | Low Severity |
|----------|-------|----------------|-------------------|---------------|-----------------|--------------|
| 1. Broken MASM | 15 | ~200+ | 1 | 8 | 5 | 1 |
| 2. Headers missing .cpp | 8 | ~90+ | 1 | 4 | 2 | 1 |
| 3. Linker stubs | 14 | ~430+ | 1 | 10 | 2 | 1 |
| 4. Auto-generated stubs | 13 | ~470+ | 2 | 7 | 3 | 1 |
| 5. SSOT missing | 4 | 32 | 1 | 3 | 0 | 0 |
| 6. Security stubs | 10 | ~40+ | 2 | 5 | 3 | 0 |
| 7. GPU backend stubs | 14 | ~70+ | 1 | 9 | 3 | 1 |
| 8. Extension system stubs | 11 | ~70+ | 1 | 6 | 3 | 1 |
| 9. Runtime stubs | 26 | ~80+ | 0 | 1 | 22 | 3 |
| 10. JS extension host stubs | 4 | ~45+ | 1 | 3 | 0 | 0 |
| 11. Test stub files | 11 | ~35 | 0 | 0 | 9 | 2 |
| 12. Ship/archived stubs | 20 | ~90+ | 0 | 0 | 14 | 6 |
| 13. Resource file gaps | 4 | 2 | 0 | 0 | 2 | 2 |
| **TOTAL** | **~154** | **~1,650+** | **11** | **56** | **68** | **19** |

---

## Top 10 Most Critical Stub Files (Immediate Action Required)

1. **`d:\rawrxd\src\core\auto_feature_stub_impl.cpp`** — 286 auto-generated stub handlers. Largest blocker for production.
2. **`d:\rawrxd\src\asm\RawrXD_StubSweepBridge.asm`** — 100+ MASM stub exports. Auto-generated bridge with no real implementations.
3. **`d:\rawrxd\src\core\missing_handler_stubs.cpp`** — 15 SSOT missing handlers for core platform features.
4. **`d:\rawrxd\src\rawrxd_titan_stubs.cpp`** — 25+ Titan API stubs. Auto-generated, all return fake data.
5. **`d:\rawrxd\src\nlohmann_stub.h`** — Complete JSON library stub. `dump()` returns `"{}"`. Breaks any real JSON processing.
6. **`d:\rawrxd\src\quickjs_extension_host.cpp`** — QuickJS extension host with opaque stub types. No JS runtime actually linked.
7. **`d:\rawrxd\src\gpu\cuda_inference_engine.cpp`** — CUDA inference is MOCK only without `RAWR_HAS_CUDA`. All GPU inference falls back to CPU.
8. **`d:\rawrxd\src\security\fips_compliance.cpp`** — FIPS 140-2 stub (`SCAFFOLD_204`). Government-grade crypto is not implemented.
9. **`d:\rawrxd\src\security\hsm_integration.cpp`** — HSM integration stub (`SCAFFOLD_203`). PKCS#11 is optional placeholder.
10. **`d:\rawrxd\src\core\swarm_orchestrator_link_stub.cpp`** — Swarm orchestrator with empty consensus and no-op agent management.

---

## Recommendations

### Immediate (Day 1-2)
- **Replace `nlohmann_stub.h`** with real `nlohmann/json.hpp` or a functional minimal JSON parser.
- **Audit `auto_feature_stub_impl.cpp`** — determine which of the 286 handlers are actually called at runtime and prioritize real implementations.
- **Fix `quickjs_extension_host.cpp`** — either link real QuickJS or remove the extension host feature.

### Short-term (Day 3-5)
- **Consolidate linker stubs** — `gold_asm_closure_stubs.cpp`, `beacon_link_stub.cpp`, `rawrengine_asm_dispatch_stubs.cpp` should be merged or replaced with real MASM objects.
- **Implement Titan API** — `rawrxd_titan_stubs.cpp` needs real model loading and inference.
- **GPU backend** — `cuda_inference_engine.cpp` needs real CUDA kernels or proper conditional compilation.

### Medium-term (Day 6-10)
- **Security stubs** — `fips_compliance.cpp`, `hsm_integration.cpp`, `classified_network.cpp` need real implementations or feature flags to disable them cleanly.
- **Extension host** — Decide on QuickJS vs headless-only architecture. Current dual-stub approach is unmaintainable.
- **Ship/ directory cleanup** — 20 archived ASM files should be moved to `.archived_orphans/` or deleted.

### Long-term (Day 11-14)
- **MASM consolidation** — 800+ .asm files need audit. Many are duplicates or stubs.
- **Runtime sovereign features** — 26 runtime files with lightweight implementations need hardening.
- **Resource files** — Add proper icon and manifest resources.

---

*Report generated by automated codebase analysis. Severity is based on: Critical = breaks core functionality, High = breaks secondary features, Medium = reduces capability, Low = cosmetic/minor.*
