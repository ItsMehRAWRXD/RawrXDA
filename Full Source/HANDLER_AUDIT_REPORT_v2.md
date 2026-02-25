# Handler Audit Report — Hardcoded, Stubbed & Unwired Values

**Scope:** `feature_handlers.cpp`, `ssot_handlers.cpp`, `missing_handler_stubs.cpp`  
**Date:** 2025  
**Standard:** `tools.instructions.md` — NO SOURCE FILE IS TO BE SIMPLIFIED; production readiness required.

---

## Legend

| Severity | Meaning |
|----------|---------|
| **CRIT** | Fake/hardcoded data returned as if real — silently wrong at runtime |
| **HIGH** | Real subsystem API exists but handler uses `_popen`/`system()` or ignores it |
| **MED**  | Stub that outputs a success string but performs no actual work |
| **LOW**  | Minor: placeholder text, hardcoded default, or TODO |
| **BUG**  | Compilation or logic error (typo, wrong variable name) |

---

## Available Subsystem APIs (from headers)

| Subsystem | Header | Key Methods |
|-----------|--------|-------------|
| `AgentOllamaClient` | `agentic/AgentOllamaClient.h` | `TestConnection()`, `GetVersion()`, `ListModels()`, `ChatSync()`, `ChatStream()`, `FIMSync()`, `FIMStream()` |
| `NativeDebuggerEngine` | `core/native_debugger_engine.h` | `disassembleAt()`, `disassembleFunction()`, `getBreakpoints()`, `removeWatch()`, `setSymbolPath()`, `searchMemory()`, `resolveSymbol()`, `enumerateModules()`, `walkStack()`, `readMemory()`, `evaluate()`, `addWatch()`, `removeBreakpoint()` |
| `AutoRepairOrchestrator` | `core/auto_repair_orchestrator.hpp` | `getStats()`, `statsToJson()`, `getAnomalyLog()`, `getRepairLog()`, `pollNow()`, `pause()`, `resume()` |
| `UnifiedHotpatchManager` | `core/unified_hotpatch_manager.hpp` | `apply_memory_patch()`, `apply_byte_patch()`, `add_server_patch()`, `getStats()`, `getFullStatsJSON()`, `embedding_initialize()`, `vision_initialize()`, `marketplace_initialize()` |
| `ProxyHotpatcher` | `core/proxy_hotpatcher.hpp` | `add_token_bias()`, `add_rewrite_rule()`, `add_validator()`, `getStats()`, `apply_rewrites()` |
| `MultiResponseEngine` | `core/multi_response_engine.h` | `startSession()`, `generateAll()`, `setPreference()`, `getStats()`, `getAllTemplates()`, `setTemplateEnabled()`, `getLatestSession()`, `getRecommendedTemplate()`, `toJson()`, `statsToJson()` |
| `EmbeddingEngine` | `core/embedding_engine.hpp` | `loadModel()`, `embed()`, `embedBatch()`, `embedFile()`, `isReady()` |
| `VisionEncoder` | `core/vision_encoder.hpp` | `loadFromFile()`, `encode()`, model-based analysis pipeline |
| `ConfidenceGate` | `core/confidence_gate.h` | `evaluate()`, `wouldExecute()`, `getStats()`, `init()` |
| `ExecutionGovernor` | `core/execution_governor.h` | `ExecuteSafe()` (TerminalWatchdog), governed task pipeline |
| `StlHttpClient` | `agent/llm_http_client.hpp` | `send()`, `postJson()`, `get()`, `sendAsync()` |
| `ByteLevelHotpatcher` | `core/byte_level_hotpatcher.hpp` | `patch_bytes()`, `direct_read()`, `direct_write()`, `direct_search()`, `apply_byte_mutation()` |
| `GGUFServerHotpatch` | `server/gguf_server_hotpatch.hpp` | `add_patch()`, `apply_patches()` |
| `SubsystemAgentBridge` | `core/subsystem_agent_bridge.hpp` | `executeAction()`, `enumerateCapabilities()`, `canInvoke()` |

---

## FINDINGS

### 1. `feature_handlers.cpp` (3128 lines)

#### 1.1 — `handleAIEngineSelect` — CRIT: `_popen("curl")` instead of `AgentOllamaClient`
- **File:** `src/core/feature_handlers.cpp`
- **Lines:** ~1630–1660
- **Handler:** `handleAIEngineSelect`
- **Problem:** Uses `_popen("curl -s http://localhost:11434/api/version")` to test Ollama connectivity.
- **Real API:** `AgentOllamaClient::TestConnection()` + `AgentOllamaClient::GetVersion()` — already available, already used in ssot_handlers.cpp.
- **Impact:** Spawns a child process, doesn't respect OllamaConfig (host/port), bypasses StlHttpClient stats/tracing.

#### 1.2 — `handleVoiceTranscribe` — HIGH: `_popen("curl")` for LLM inference
- **File:** `src/core/feature_handlers.cpp`
- **Lines:** ~2001–2040
- **Handler:** `handleVoiceTranscribe`
- **Problem:** Uses `_popen("curl -s http://localhost:11434/api/generate ...")` to send transcription to LLM.
- **Real API:** `AgentOllamaClient::ChatSync()` — properly handles streaming, tools, error reporting, token metrics.
- **Impact:** Hardcodes host:port, no error handling for connection failure, no token/latency metrics.

#### 1.3 — `handleVoiceSpeak` — LOW: `_popen("powershell")` for TTS
- **File:** `src/core/feature_handlers.cpp`
- **Lines:** ~2055–2090
- **Handler:** `handleVoiceSpeak`
- **Problem:** Spawns full PowerShell process to invoke `SAPI.SpVoice`. 
- **Consider:** `ExecutionGovernor::TerminalWatchdog::ExecuteSafe()` for timeout protection, or direct COM `ISpVoice` usage.
- **Mitigation:** PowerShell/SAPI is acceptable but should be governed (no timeout = potential hang).

#### 1.4 — `handleEditFind` — LOW: `_popen("findstr")`
- **File:** `src/core/feature_handlers.cpp`  
- **Lines:** ~580–610
- **Problem:** Uses `_popen("findstr /s /n ...")` for workspace search.
- **Consider:** `ExecutionGovernor::TerminalWatchdog::ExecuteSafe()` for timeout + partial output capture.
- **Acceptable:** findstr is the appropriate tool; just needs governance wrapping.

#### 1.5 — `handleREDisassemble` / `handleREAnalyzeBinary` — LOW: `_popen("dumpbin")`
- **File:** `src/core/feature_handlers.cpp`
- **Lines:** ~1720–1870
- **Problem:** Uses `_popen("dumpbin /disasm ...")` and `_popen("dumpbin /all ...")`.
- **Real API:** `NativeDebuggerEngine::disassembleAt()`, `NativeDebuggerEngine::disassembleFunction()`, `NativeDebuggerEngine::enumerateModules()`. These provide richer output (symbol resolution, structured data) vs raw dumpbin text.
- **Impact:** dumpbin is a static tool; NativeDebuggerEngine provides live process debugging context. Different use cases, but for file-based RE, dumpbin is acceptable.

#### 1.6 — `handleModelPull` — Uses `AgentOllamaClient` correctly ✅
- **Lines:** ~696–740
- **Note:** Properly creates `AgentOllamaClient` and uses it. Good pattern.

---

### 2. `ssot_handlers.cpp` (2050 lines)

#### 2.1 — `handleSwarmEvents` — BUG: Variable name typo
- **File:** `src/core/ssot_handlers.cpp`
- **Lines:** ~2030
- **Handler:** `handleSwarmEvents`
- **Problem:** References `orchstats` — should be `orchStats` (matching the `AutoRepairStats orchStats = ...` declaration pattern used elsewhere).
- **Impact:** **Will not compile** or references wrong variable.

#### 2.2 — `handleAuditRunFull` — BUG: Variable name typo
- **File:** `src/core/ssot_handlers.cpp`
- **Lines:** ~2045
- **Handler:** `handleAuditRunFull`
- **Problem:** References `orchstats` — same typo as above.
- **Impact:** **Will not compile.**

#### 2.3 — `handleAuditExportReport` — BUG: Variable name typo
- **File:** `src/core/ssot_handlers.cpp`
- **Lines:** ~2070
- **Handler:** `handleAuditExportReport`
- **Problem:** References `ostats` — likely should be `oStats` or `orchStats`.
- **Impact:** **Will not compile.**

#### 2.4 — `handleTelemetryExportJson` — BUG: Variable name typo
- **File:** `src/core/ssot_handlers.cpp`
- **Lines:** ~2085
- **Handler:** `handleTelemetryExportJson`
- **Problem:** References `ostats` — same as above.
- **Impact:** **Will not compile.**

#### 2.5 — `handleREDisassembleFunc` — HIGH: `_popen("dumpbin")` when `NativeDebuggerEngine` exists
- **File:** `src/core/ssot_handlers.cpp`
- **Lines:** ~292–340
- **Handler:** `handleREDisassembleFunc`
- **Problem:** Uses `_popen("dumpbin /disasm ...")` for disassembly.
- **Real API:** `NativeDebuggerEngine::disassembleFunction(symbol)` — returns structured `DisassembledInstruction` vector with address, opcode, mnemonic, operands, symbol resolution.
- **Impact:** Loses structured data, no symbol resolution, no JSON serialization.

#### 2.6 — `handleREDetectVulns` — HIGH: `_popen("dumpbin /headers")` for vuln detection
- **File:** `src/core/ssot_handlers.cpp`
- **Lines:** ~500–560
- **Handler:** `handleREDetectVulns`
- **Problem:** Runs `_popen("dumpbin /headers")` and does basic text parsing for security flags (ASLR, DEP, CFG).
- **Real API:** `NativeDebuggerEngine::enumerateModules()` provides `DebugModule` with structured header info. Alternatively, `ByteLevelHotpatcher::direct_read()` can read PE headers directly for static analysis.
- **Note:** For static file analysis without launching, dumpbin is acceptable, but parsing should be more robust.

#### 2.7 — `handleREDemangleName` — LOW: `_popen("undname")`
- **File:** `src/core/ssot_handlers.cpp`
- **Lines:** ~560–600
- **Handler:** `handleREDemangleName`
- **Problem:** Uses `_popen("undname ...")` from MSVC tools.
- **Consider:** `UnDecorateSymbolName()` Win32 API — direct call, no process spawn, already linked via dbghelp.lib.

#### 2.8 — `handleRECompareFiles` — Acceptable: `_popen("fc /b")`
- **File:** `src/core/ssot_handlers.cpp`
- **Lines:** ~340–400
- **Note:** Binary file comparison via `fc /b` is appropriate. Could use `ExecutionGovernor` for timeout.

---

### 3. `missing_handler_stubs.cpp` (3372 lines)

#### 3.1 — `handleEmbeddingEncode` — CRIT: Fake hash-based embedding vectors
- **File:** `src/core/missing_handler_stubs.cpp`
- **Lines:** ~3290–3340
- **Handler:** `handleEmbeddingEncode`
- **Problem:** Generates **fake embedding vectors** using FNV-1a hash:
  ```cpp
  uint32_t h = 2166136261u;
  for (char c : text) { h ^= (uint8_t)c; h *= 16777619u; }
  // Generates 8 "dimensions" from hash bits
  ```
  Returns these as if they were real embeddings — any downstream code (RAG, semantic search, HNSW index) will produce garbage results.
- **Real API:** `RawrXD::Embeddings::EmbeddingEngine::instance().embed(text, outEmbedding)` — loads real GGUF model, produces proper 384/768/1024-dim vectors with SIMD distance computation.
- **Impact:** **Critical production bug.** Semantic search will never work. RAG pipeline silently broken.
- **Fix:** Call `EmbeddingEngine::embed()`, fall back to error message if model not loaded.

#### 3.2 — `handleVisionAnalyzeImage` — CRIT: Header-only, no actual vision analysis
- **File:** `src/core/missing_handler_stubs.cpp`
- **Lines:** ~3340–3372
- **Handler:** `handleVisionAnalyzeImage`
- **Problem:** Only reads image file headers to detect format (PNG/JPEG/BMP/GIF magic bytes) and reports dimensions. Does NOT perform any actual vision analysis. Contains comment: `"Note: Full vision analysis requires model inference endpoint"`.
- **Real API:** `RawrXD::Vision::VisionEncoder::instance()` — supports CLIP ViT, LLaVA, SigLIP models with `loadFromFile()` → preprocessing → encoding → embedding pipeline.
- **Impact:** Any "analyze image" request returns only format metadata, not content analysis.
- **Fix:** Use `VisionEncoder` pipeline: load image → preprocess → encode → generate description.

#### 3.3 — `handleHybridStreamAnalyze` — MED: Pure stub, static output
- **File:** `src/core/missing_handler_stubs.cpp`
- **Lines:** ~1005
- **Handler:** `handleHybridStreamAnalyze`
- **Problem:** Returns hardcoded string `"[HYBRID] Streaming analysis enabled for current file."` — no actual analysis.
- **Real API:** Should use `AgentOllamaClient::ChatStream()` with file context for actual streaming analysis.
- **Fix:** Wire to real streaming inference with file content as context.

#### 3.4 — `handleHybridSemanticPrefetch` — MED: Pure stub, static output
- **File:** `src/core/missing_handler_stubs.cpp`
- **Lines:** ~1010
- **Handler:** `handleHybridSemanticPrefetch`
- **Problem:** Returns hardcoded string `"[HYBRID] Semantic prefetch triggered."` — no prefetch occurs.
- **Real API:** Should trigger `EmbeddingEngine::embedFile()` on nearby/related files to pre-warm the HNSW index.
- **Fix:** Wire to `EmbeddingEngine` batch indexing.

#### 3.5 — `handleHybridAnnotateDiag` — MED: Stub outputs "Annotations applied" without doing anything
- **File:** `src/core/missing_handler_stubs.cpp`
- **Lines:** ~995
- **Handler:** `handleHybridAnnotateDiag`
- **Problem:** Outputs `"Annotations applied"` but performs no annotation logic.
- **Real API:** Should use `AgentOllamaClient::ChatSync()` to generate diagnostic annotations, then apply to editor buffer.

#### 3.6 — `handleMultiRespToggleTemplate` — MED: Doesn't actually toggle
- **File:** `src/core/missing_handler_stubs.cpp`
- **Lines:** ~1155
- **Handler:** `handleMultiRespToggleTemplate`
- **Problem:** Outputs a toggled status string but doesn't call the engine to persist the change.
- **Real API:** `MultiResponseEngine::setTemplateEnabled(id, enabled)` — already implemented in the engine.
- **Fix:** Call `engine.setTemplateEnabled()` after computing the new state.

#### 3.7 — `handleMultiRespClearHistory` — MED: Outputs "cleared" without clearing
- **File:** `src/core/missing_handler_stubs.cpp`
- **Lines:** ~1170
- **Handler:** `handleMultiRespClearHistory`
- **Problem:** Outputs `"[MRE] Response history cleared."` but doesn't call any engine method to actually clear history.
- **Real API:** `MultiResponseEngine` does not expose `clearHistory()` directly, but the session state management methods should be used. At minimum, reset internal state tracking.
- **Fix:** Implement actual clearing logic or add `clearHistory()` to `MultiResponseEngine`.

#### 3.8 — `handleAsmDetectConvention` — CRIT: Always returns hardcoded Win64 ABI info
- **File:** `src/core/missing_handler_stubs.cpp`
- **Lines:** ~740–780
- **Handler:** `handleAsmDetectConvention`
- **Problem:** Regardless of input, always outputs hardcoded text about Win64 calling convention (RCX, RDX, R8, R9, shadow space). Never inspects the actual binary or ASM input.
- **Real API:** `NativeDebuggerEngine::disassembleAt()` + prologue analysis, or use the ASM semantic parser already present in the same file to detect push/mov patterns indicating cdecl/stdcall/fastcall/thiscall.
- **Impact:** Users get wrong info for x86, stdcall, cdecl, thiscall targets.
- **Fix:** Parse function prologue bytes from the target to detect actual convention.

#### 3.9 — `handleDbgListBps` — MED: Stub instead of listing breakpoints
- **File:** `src/core/missing_handler_stubs.cpp`
- **Lines:** ~2290
- **Handler:** `handleDbgListBps`
- **Problem:** Outputs `"Use GUI breakpoint panel for full view"` with dummy text.
- **Real API:** `NativeDebuggerEngine::getBreakpoints()` — returns `const std::vector<NativeBreakpoint>&`, already wired in adjacent handlers.
- **Fix:** Iterate `getBreakpoints()` and format output (the engine also has `toJsonBreakpoints()`).

#### 3.10 — `handleDbgRemoveWatch` — MED: Stub, doesn't remove
- **File:** `src/core/missing_handler_stubs.cpp`
- **Lines:** ~2300
- **Handler:** `handleDbgRemoveWatch`
- **Problem:** Outputs `"Watch removed"` without calling `NativeDebuggerEngine::removeWatch()`.
- **Real API:** `NativeDebuggerEngine::removeWatch(watchId)` — already available.
- **Fix:** Parse watch ID from context args, call `removeWatch(id)`.

#### 3.11 — `handleDbgSymbolPath` — MED: Stub, doesn't set symbol path
- **File:** `src/core/missing_handler_stubs.cpp`
- **Lines:** ~2470
- **Handler:** `handleDbgSymbolPath`
- **Problem:** Outputs `"Symbol path set"` without calling engine.
- **Real API:** `NativeDebuggerEngine::setSymbolPath(path)` — already available.
- **Fix:** Extract path from args, call `setSymbolPath()`.

#### 3.12 — `handleBackendSaveConfigs` — MED: Stub, doesn't persist
- **File:** `src/core/missing_handler_stubs.cpp`
- **Lines:** ~2100
- **Handler:** `handleBackendSaveConfigs`
- **Problem:** Outputs `"saved"` but does not write configuration to disk.
- **Real API:** Should serialize `BackendState` to JSON and write to config file. `StlHttpClient` or direct file I/O.
- **Fix:** Implement JSON serialization + file write for backend configs.

#### 3.13 — `handleRouterSaveConfig` — MED: Stub, doesn't persist
- **File:** `src/core/missing_handler_stubs.cpp`
- **Lines:** ~1520
- **Handler:** `handleRouterSaveConfig`
- **Problem:** Outputs `"Config saved"` without writing anything to disk.
- **Fix:** Serialize `RouterState` to JSON file.

#### 3.14 — `handlePluginRefresh` — MED: Stub, doesn't rescan
- **File:** `src/core/missing_handler_stubs.cpp`
- **Lines:** ~2695
- **Handler:** `handlePluginRefresh`
- **Problem:** Outputs `"Plugins refreshed"` without rescanning plugin directory.
- **Fix:** Re-enumerate plugin directory, call `LoadLibrary` for new plugins, verify existing ones.

#### 3.15 — `handleRevengDisassemble` — HIGH: `_popen("dumpbin")` when `NativeDebuggerEngine` exists
- **File:** `src/core/missing_handler_stubs.cpp`
- **Lines:** ~2870–2900
- **Handler:** `handleRevengDisassemble`
- **Problem:** Uses `_popen("dumpbin /disasm ...")` for disassembly.
- **Real API:** `NativeDebuggerEngine::disassembleAt()` / `disassembleFunction()` — returns structured `DisassembledInstruction` with full symbol resolution.
- **Note:** Duplicates functionality in `feature_handlers.cpp` and `ssot_handlers.cpp`.

#### 3.16 — `handleRevengDecompile` — HIGH: `_popen("dumpbin /imports")` as "pseudo-decompile"
- **File:** `src/core/missing_handler_stubs.cpp`
- **Lines:** ~2900–2930
- **Handler:** `handleRevengDecompile`
- **Problem:** Runs `_popen("dumpbin /imports ...")` and labels it "pseudo-decompile" — misleading name.
- **Real API:** `NativeDebuggerEngine::enumerateModules()` provides structured module/import data. For actual decompilation, this needs a proper decompiler integration or at minimum should be labeled honestly.

#### 3.17 — `handleRevengFindVulnerabilities` — HIGH: Duplicates ssot_handlers
- **File:** `src/core/missing_handler_stubs.cpp`
- **Lines:** ~2930–2990
- **Handler:** `handleRevengFindVulnerabilities`
- **Problem:** Uses `_popen("dumpbin /headers")` and parses for ASLR/DEP/CFG — **exact same logic** as `ssot_handlers.cpp::handleREDetectVulns`. Code duplication.
- **Fix:** Consolidate into shared utility or delegate to ssot handler.

#### 3.18 — `handlePromptClassify` — LOW: Keyword-matching instead of model inference
- **File:** `src/core/missing_handler_stubs.cpp`
- **Lines:** ~3372 (end of file)
- **Handler:** `handlePromptClassify`
- **Problem:** Classifies prompts using simple keyword matching (`if contains "code" → coding`, etc.). 
- **Real API:** `AgentOllamaClient::ChatSync()` with a classification system prompt would give semantically accurate classification.
- **Note:** Keyword matching is fast and acceptable as a pre-filter. Could be a two-tier approach: keyword fast-path + LLM fallback.

#### 3.19 — `ConfidenceState::score` — LOW: Hardcoded initial value
- **File:** `src/core/missing_handler_stubs.cpp`
- **Lines:** ~155
- **Problem:** `static float score = 0.85f;` — arbitrary initial confidence value.
- **Real API:** `ConfidenceGate::evaluate()` computes confidence dynamically. The initial value should come from config or the gate's last known state.

#### 3.20 — `handleRouterSimulate` — LOW: "Would route" dry-run text
- **File:** `src/core/missing_handler_stubs.cpp`
- **Lines:** ~1785
- **Handler:** `handleRouterSimulate`
- **Problem:** Outputs `"Would route to: ..."` — this is a simulation verb and may be **by design**.
- **Note:** If deliberate dry-run, acceptable. But confirm the intent — "simulate" should still use real routing logic to determine the destination.

#### 3.21 — `handleDiskRecoveryScan` / `handleDiskRecoveryRecover` — MED: Uses `_popen("chkdsk")` / `_popen("sfc")`
- **File:** `src/core/missing_handler_stubs.cpp`
- **Lines:** ~3100–3170
- **Handler:** `handleDiskRecoveryScan`, `handleDiskRecoveryRecover`
- **Problem:** Uses `_popen("chkdsk")` and `_popen("sfc /scannow")`.
- **Consider:** `ExecutionGovernor::TerminalWatchdog::ExecuteSafe()` for timeout protection — these commands can run for hours.
- **Note:** No timeout on chkdsk via _popen = possible system hang.

#### 3.22 — `handleGovernorSetPowerLevel` — LOW: `_popen("powercfg")`
- **File:** `src/core/missing_handler_stubs.cpp`
- **Lines:** ~3190–3220
- **Handler:** `handleGovernorSetPowerLevel`
- **Problem:** Uses `_popen("powercfg /setactive ...")`.
- **Consider:** Win32 `PowerSetActiveScheme()` API — direct, no process spawn. Or at minimum wrap in `ExecutionGovernor`.

---

## Summary Table

| # | File | Handler | Severity | Issue |
|---|------|---------|----------|-------|
| 1 | feature_handlers.cpp | `handleAIEngineSelect` | **CRIT** | `_popen("curl")` → use `AgentOllamaClient::TestConnection()` |
| 2 | feature_handlers.cpp | `handleVoiceTranscribe` | **HIGH** | `_popen("curl")` → use `AgentOllamaClient::ChatSync()` |
| 3 | feature_handlers.cpp | `handleVoiceSpeak` | LOW | `_popen("powershell")` for TTS — add timeout governance |
| 4 | feature_handlers.cpp | `handleEditFind` | LOW | `_popen("findstr")` — add `ExecutionGovernor` wrapping |
| 5 | ssot_handlers.cpp | `handleSwarmEvents` | **BUG** | `orchstats` typo → `orchStats` |
| 6 | ssot_handlers.cpp | `handleAuditRunFull` | **BUG** | `orchstats` typo → `orchStats` |
| 7 | ssot_handlers.cpp | `handleAuditExportReport` | **BUG** | `ostats` typo → `oStats` |
| 8 | ssot_handlers.cpp | `handleTelemetryExportJson` | **BUG** | `ostats` typo → `oStats` |
| 9 | ssot_handlers.cpp | `handleREDisassembleFunc` | **HIGH** | `_popen("dumpbin")` → use `NativeDebuggerEngine::disassembleFunction()` |
| 10 | ssot_handlers.cpp | `handleREDetectVulns` | **HIGH** | `_popen("dumpbin /headers")` → use `NativeDebuggerEngine` or `ByteLevelHotpatcher::direct_read()` |
| 11 | ssot_handlers.cpp | `handleREDemangleName` | LOW | `_popen("undname")` → use `UnDecorateSymbolName()` Win32 API |
| 12 | missing_handler_stubs.cpp | `handleEmbeddingEncode` | **CRIT** | FNV-1a fake vectors → use `EmbeddingEngine::embed()` |
| 13 | missing_handler_stubs.cpp | `handleVisionAnalyzeImage` | **CRIT** | Header-only format detection → use `VisionEncoder` pipeline |
| 14 | missing_handler_stubs.cpp | `handleAsmDetectConvention` | **CRIT** | Hardcoded Win64 ABI → parse actual prologue bytes |
| 15 | missing_handler_stubs.cpp | `handleHybridStreamAnalyze` | **MED** | Static string → wire to `AgentOllamaClient::ChatStream()` |
| 16 | missing_handler_stubs.cpp | `handleHybridSemanticPrefetch` | **MED** | Static string → wire to `EmbeddingEngine::embedFile()` |
| 17 | missing_handler_stubs.cpp | `handleHybridAnnotateDiag` | **MED** | "Annotations applied" with no action → wire to LLM |
| 18 | missing_handler_stubs.cpp | `handleMultiRespToggleTemplate` | **MED** | No engine call → use `MultiResponseEngine::setTemplateEnabled()` |
| 19 | missing_handler_stubs.cpp | `handleMultiRespClearHistory` | **MED** | "Cleared" no-op → implement actual clearing |
| 20 | missing_handler_stubs.cpp | `handleDbgListBps` | **MED** | Stub → use `NativeDebuggerEngine::getBreakpoints()` |
| 21 | missing_handler_stubs.cpp | `handleDbgRemoveWatch` | **MED** | Stub → use `NativeDebuggerEngine::removeWatch()` |
| 22 | missing_handler_stubs.cpp | `handleDbgSymbolPath` | **MED** | Stub → use `NativeDebuggerEngine::setSymbolPath()` |
| 23 | missing_handler_stubs.cpp | `handleBackendSaveConfigs` | **MED** | "Saved" no-op → implement file persistence |
| 24 | missing_handler_stubs.cpp | `handleRouterSaveConfig` | **MED** | "Saved" no-op → implement file persistence |
| 25 | missing_handler_stubs.cpp | `handlePluginRefresh` | **MED** | "Refreshed" no-op → rescan plugin directory |
| 26 | missing_handler_stubs.cpp | `handleRevengDisassemble` | **HIGH** | `_popen("dumpbin")` → use `NativeDebuggerEngine::disassembleAt()` |
| 27 | missing_handler_stubs.cpp | `handleRevengDecompile` | **HIGH** | `_popen("dumpbin /imports")` labeled "decompile" → misleading |
| 28 | missing_handler_stubs.cpp | `handleRevengFindVulns` | **HIGH** | Duplicated code from ssot_handlers → consolidate |
| 29 | missing_handler_stubs.cpp | `handleDiskRecoveryScan` | **MED** | `_popen("chkdsk")` with no timeout → use `ExecutionGovernor` |
| 30 | missing_handler_stubs.cpp | `handlePromptClassify` | LOW | Keyword matching → consider LLM fallback tier |
| 31 | missing_handler_stubs.cpp | `ConfidenceState::score` | LOW | Hardcoded `0.85f` → derive from `ConfidenceGate` |
| 32 | missing_handler_stubs.cpp | `handleGovernorSetPowerLevel` | LOW | `_popen("powercfg")` → use `PowerSetActiveScheme()` API |

---

## Priority Remediation Order

### Immediate (CRIT + BUG — blocks correctness / compilation)
1. **Fix 4 typo BUGs** in ssot_handlers.cpp (#5–8) — compilation blockers
2. **Wire `handleEmbeddingEncode`** (#12) to `EmbeddingEngine::embed()` — semantic search is silently broken
3. **Wire `handleVisionAnalyzeImage`** (#13) to `VisionEncoder` — vision feature non-functional
4. **Wire `handleAsmDetectConvention`** (#14) to actual prologue analysis — returns wrong data
5. **Wire `handleAIEngineSelect`** (#1) to `AgentOllamaClient::TestConnection()` — inconsistent with rest of codebase

### Short-term (HIGH — subsystem API available and superior)
6. Replace `_popen("curl")` in `handleVoiceTranscribe` (#2) with `AgentOllamaClient::ChatSync()`
7. Replace `_popen("dumpbin")` in RE handlers (#9, #10, #26, #27, #28) with `NativeDebuggerEngine` methods
8. Consolidate duplicate vuln detection (#28 vs #10)

### Medium-term (MED — stubs that lie about success)
9. Wire all "saved/cleared/refreshed" no-op stubs (#18–25) to real engine methods
10. Wire hybrid LSP-AI stubs (#15–17) to real inference
11. Wrap all `_popen()` calls (#3, #4, #29, #32) in `ExecutionGovernor::TerminalWatchdog::ExecuteSafe()`

### Backlog (LOW)
12. Replace `_popen("undname")` (#11) with `UnDecorateSymbolName()`
13. Add LLM classification tier to `handlePromptClassify` (#30)
14. Derive initial confidence from `ConfidenceGate` (#31)

---

## Correctly Wired Handlers (Positive Findings)

These handlers properly use real subsystem APIs — no changes needed:

| File | Handler | Subsystem Used |
|------|---------|----------------|
| feature_handlers.cpp | `handleModelPull` | `AgentOllamaClient` ✅ |
| ssot_handlers.cpp | `handleAutonomyStatus` | `AutoRepairOrchestrator::getStats()` ✅ |
| ssot_handlers.cpp | `handleAutonomyMemory` | `AutoRepairOrchestrator::getAnomalyLog()` ✅ |
| ssot_handlers.cpp | `handleAIContextInject` | `AgentOllamaClient::ChatSync()` ✅ |
| ssot_handlers.cpp | `handleAINoRefusal` | `AgentOllamaClient::ChatSync()` ✅ |
| missing_handler_stubs.cpp | `handleRouterRoute*` (21 handlers) | `routeToBackend()` using `AgentOllamaClient` ✅ |
| missing_handler_stubs.cpp | `handleBackendOllama*` (11 handlers) | `AgentOllamaClient` ✅ |
| missing_handler_stubs.cpp | `handleDbg*` (most of 28) | `NativeDebuggerEngine` ✅ |
| missing_handler_stubs.cpp | `handlePlugin{Load,Unload,List,Info}` | `LoadLibrary`/`FreeLibrary` ✅ |
| missing_handler_stubs.cpp | `handleSafety*` | `ReplayJournal` ✅ |
| missing_handler_stubs.cpp | `handleMultiResp{Start,Select,Stats,Compare}` | `MultiResponseEngine` ✅ |
| missing_handler_stubs.cpp | `handleGovernor{Status,Pause,Resume}` | `GovernorState` ✅ |

---

*End of audit. 32 findings total: 5 CRIT, 7 HIGH, 12 MED, 4 BUG, 4 LOW.*
