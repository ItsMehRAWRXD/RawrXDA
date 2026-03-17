# RawrXD IDE — What It Is NOT Capable Of (Full Audit)

**Scope:** Entire `D:\rawrxd` area  
**Purpose:** Single reference for capabilities the IDE currently **does not** have.  
**Sources:** WIN32_IDE_FEATURES_AUDIT.md, NON_OPERATIONAL_FEATURES.md, AGENTIC_AND_MODEL_LOADING_AUDIT.md, IDE_PRODUCTION_UNINTEGRATED_AUDIT.md, agent exploration, codebase grep.

---

## 1. Model & inference

| Not capable of | Detail | Ref |
|----------------|--------|-----|
| **Using Ollama/cloud without ever loading a GGUF** | Backend manager and LLM router are only initialized inside `loadModelFromPath()` after a local GGUF loads. If you never do File → Load Model (GGUF), BackendSwitcher never engages. | `docs/AGENTIC_AND_MODEL_LOADING_AUDIT.md`; `Win32IDE.cpp` ~4429–4436 |
| **Routing agent/tools through Ollama or cloud** | Agent path (tools, multi-step loop) always goes through `CPUInferenceEngine` (local only). No branch to `routeInferenceRequest()` or BackendSwitcher for agent. | Same; `Win32IDE_AgenticBridge.cpp`, `agentic_engine.cpp` |
| **Model finetune/quantize from UI** | `model.finetune`, `model.quantize` print "Not implemented" in terminal. | `docs/NON_OPERATIONAL_FEATURES.md`; `src/core/ssot_handlers_ext.cpp` |
| **True AI completion streaming** | Completion uses sync path via `s_ideCommandEngine->chat()`; single callback; "streaming TBD". | `docs/AGENTIC_AUDIT_STUBS_AND_WIRING.md`; `src/action_executor.cpp` |
| **AI context 128K–1M** | `ai.context128k` through `ai.context1m` have no underlying context management. | `docs/NON_OPERATIONAL_FEATURES.md` |

---

## 2. Workspace & agent context

| Not capable of | Detail | Ref |
|----------------|--------|-----|
| **Using opened folder as workspace for explorer and agent** | "Open folder" does not set `m_explorerRootPath` or `m_projectRoot`. Explorer and agent never see the opened folder as workspace. | `docs/AGENTIC_AND_MODEL_LOADING_AUDIT.md`; `Win32IDE_Tier1Cosmetics.cpp` (handleWelcomeOpenFolder) |
| **Passing workspace root or file list to the agent** | Agent prompt gets no workspace root or project file list; agent cannot "see" the project. | Same |
| **AgentOrchestrator prompt/mesh_sync doing real work** | `prompt` action only logs; `mesh_sync` is no-op until MASM64 handoff; real execution is `run_tool` + ToolRegistry. | `docs/AGENTIC_AUDIT_STUBS_AND_WIRING.md`; `src/agentic/AgentOrchestrator.cpp` |

---

## 3. UI & panels

| Not capable of | Detail | Ref |
|----------------|--------|-----|
| **Reliable Network / Crash / ColorPicker panels** | Use `m_networkPanelInitialized`, `m_crashReporterInitialized`, `m_colorPickerInitialized`; reported compile errors (missing members) in some builds. | `docs/WIN32_IDE_FEATURES_AUDIT.md` §5; `Win32IDE.h`; `link_stubs_win32ide_widgets.cpp` |
| **Vulkan renderer in all configs** | IRenderer base / include issues in some configs. | Same |
| **Theme / transparency / Monaco from CLI** | `theme.*`, `view.transparency*`, `view.monaco*` are GUI-only; CLI reports "[GUI-only command]". | `docs/NON_OPERATIONAL_FEATURES.md` |
| **Real marketplace install** | Marketplace handlers return OK but are "redirect stub"; no backend download/install. | `docs/NON_OPERATIONAL_FEATURES.md`; `src/core/ssot_handlers_ext.cpp` |
| **All declared handlers in command table** | handleAgentGoal, handleBackendList/Select/Status, handleBreakpoint*, handleDebug*, handleExplain, handleGovernor, handleHistory, handleMultiResponse, handlePolicy, handleReplay, handleSafety, handleSwarmDistribute/Rebalance, handleTerminalNew, handleTools may be unwired or only called directly. | `reports/command_table_audit.md` (if present) |

---

## 4. Stubs & unimplemented features

| Not capable of | Detail | Ref |
|----------------|--------|-----|
| **Headless agentic bridge (full autonomy)** | Headless bridge returns hardcoded failures/empty; incompatible with full agentic autonomy. | `src/win32app/agentic_bridge_headless.cpp`; `docs/AGENTIC_REALITY_AUDIT_2026-03-07.md` |
| **Game engine bridge (Unreal/Unity)** | `unreal.*`, `unity.*` — empty handler. | `docs/NON_OPERATIONAL_FEATURES.md`; `src/core/ssot_handlers_ext.cpp` |
| **Vision AI backend** | `vision.analyze` — no image processing backend. | Same |
| **LSP CLI path (non-stub)** | `lspServer.*` (start, reindex, stats) — CLI prints fallback message; GUI relies on WM_COMMAND. | Same |
| **Deep-thinking / agentic ASM kernels** | `agentic_deep_thinking_kernels.asm` is pass-through `xor eax, eax`; `RawrXD_AgenticOrchestrator.asm` empty. | `docs/NON_OPERATIONAL_FEATURES.md` |
| **GPU backend (Vulkan/CUDA/ROCm) in UnifiedOverclockGovernor** | Unimplemented. | Same; `RawrXD_UnifiedOverclockGovernor.asm` |
| **Swarm P2P / dispatch** | `swarm.asm` P2P/dispatch stub. | Same |
| **Vision pipeline (DirectStorage/Vulkan DMA)** | `vision_projection_kernel.asm` — paths marked TODO. | Same |
| **GPU DMA (real GPU path)** | Multiple `gpu_dma_*.asm` return ERROR_NOT_SUPPORTED (stub to force CPU fallback). | e.g. `gpu_dma_production_final_target9.asm`, `gpu_dma_clean.asm` |
| **KV-cache Phase 2 / SwarmOrchestrator submit** | KVCache_UpdateIncremental stub; SwarmOrchestrator::SubmitInferenceRequest TODO; placeholder token/acks. | `src/bridge_layer.cpp` |
| **Undo/Redo/Settings in alternate main** | IDE_MainWindow.cpp: "[Redo not implemented]", "[Undo not implemented]", "[Settings not implemented]" for commands 4001, 4002, 5002. | `IDE_MainWindow.cpp` (if in tree) |
| **Find in text editor** | Documented as "Not implemented (future feature)"; Find functionality not implemented. | `RawrXD_Architecture_Complete.md`, `FINAL_STUB_COMPLETION_REPORT.md`, `DEPLOYMENT_CHECKLIST.md` |
| **Text editor Undo/Redo and syntax coloring** | Documented as not implemented (future feature). | Same |

---

## 5. Enterprise & license-gated

| Not capable of | Detail | Ref |
|----------------|--------|-----|
| **21 enterprise features with real backing** | AdvancedSettingsPanel, ModelComparison, BatchProcessing, PromptLibrary, ExportImportSessions, SchematicStudioIDE, WiringOracleDebug, ModelSharding, TensorParallel, PipelineParallel, ContinuousBatching, GPTQ/AWQ/CustomQuantSchemes, DynamicBatchSizing, PriorityQueuing, RateLimitingEngine, APIKeyManagement, ModelSigningVerify, RawrTunerIDE, CustomSecurityPolicies — license-gated but stub or no implementation. | `docs/ENTERPRISE_FEATURES_WITHOUT_BACKING.md`; `include/enterprise_license.h` (FeatureID) |

---

## 6. Build & integration

| Not capable of | Detail | Ref |
|----------------|--------|-----|
| **Building 60+ RawrXD_*.asm in production** | Only 5–7 files in production pipeline (e.g. Amphibious core, StreamRenderer_DMA, ML_Runtime, GUI_RealInference); 60+ unintegrated. | `IDE_PRODUCTION_UNINTEGRATED_AUDIT.md` |
| **PE writer in production** | RawrXD_PE_Writer*.asm blocked on ml64 syntax; PE Writer components not in production build. | Same, Category 2 |
| **Complete TextEditorGUI.asm** | Loaded but stub/incomplete (e.g. procedures return without rendering); syntax highlighter not wired. | Same, Category 7; `config/masm_syntax_highlighting.json` exists but no init from editor |
| **MASM syntax highlighter in editor** | RawrXD_MASM_SyntaxHighlighter.asm, RawrXD_IDE_SyntaxHighlighting_Integration.asm created but not compiled/wired; no call from TextEditorGUI to init tokenizer. | Same, Category 6 |
| **Deterministic real-vs-stub build** | Build can mix real and stub units; RAWRXD_ALLOW_AGENTIC_STUB_FALLBACK and exclusion of *_stubs.cpp / link_stubs_*.cpp control behavior. | `docs/AGENTIC_REALITY_AUDIT_2026-03-07.md`; multiple stub/link_stub files |
| **MASM/RE kernel at startup** | Not run at startup to avoid AV in Vulkan/GGUF path; must be enabled from menu or WM_APP+150. | `reverse_engineering_reports/STARTUP_MODULE_AUDIT.md` |
| **Backend manager at startup** | Only inited when a GGUF is loaded; not inited at IDE startup. | `docs/AGENTIC_AND_MODEL_LOADING_AUDIT.md` §1 |

---

## 7. MCP, LSP, extensions

| Not capable of | Detail | Ref |
|----------------|--------|-----|
| **MCP hooks in all configs** | Win32IDE_MCPHooks, OSExplorerInterceptor — syntax/include issues in some builds. | `docs/WIN32_IDE_FEATURES_AUDIT.md` §5 |
| **Verified Cursor parity range (11500–11574)** | Optional; "verify initAllCursorParityModules + 11500–11574". | Same §2.9, §5 |
| **Real marketplace backend** | No real download/install for extensions. | `docs/NON_OPERATIONAL_FEATURES.md` |

---

## 8. Tokenizer / amphibious

| Not capable of | Detail | Ref |
|----------------|--------|-----|
| **Real tokenizer in hybrid CLI** | RawrXD_Tokenizer_Encode/Decode in InferenceAPI are stubs so ChatService can link without ML_Runtime; server tokenizes. | `docs/AMPHIBIOUS_HYBRID_INTEGRATION.md` |

---

## Summary table

| Area | Not capable of (short) |
|------|------------------------|
| **Model/inference** | Ollama/cloud without GGUF first; agent on Ollama/cloud; finetune/quantize; true streaming completion; 128K–1M context |
| **Workspace/agent** | Open-folder as workspace; workspace root/file list in agent prompt; prompt/mesh_sync doing real work |
| **UI/panels** | Reliable Network/Crash/ColorPicker/Vulkan; theme/transparency/Monaco from CLI; real marketplace install; all handlers in table |
| **Stubs** | Headless agentic autonomy; Unreal/Unity; vision backend; LSP CLI; deep-thinking/GPU/swarm/vision ASM; GPU DMA; KV-cache Phase 2; Undo/Redo/Settings in alternate main; Find + syntax in text editor |
| **Enterprise** | 21 license-gated features with real implementation |
| **Build** | 60+ ASM in prod; PE writer; complete TextEditorGUI; syntax highlighter wired; deterministic real-vs-stub; MASM/RE at boot; backend at startup |
| **Integration** | MCP hooks in all configs; verified Cursor parity; real marketplace backend |
| **Tokenizer** | Real tokenizer in hybrid CLI (stubs only) |

---

**Last updated:** 2026-03-12  
**Next step:** Use this list to prioritize fixes (e.g. AGENTIC_AND_MODEL_LOADING_AUDIT.md §4, NON_OPERATIONAL_FEATURES.md §4).
