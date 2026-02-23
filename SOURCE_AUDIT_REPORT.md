# RawrXD IDE — Source Audit Report (Corrected)

**Last Updated:** February 2026  
**Purpose:** Canonical status of IDE features, sources, and gaps.

---

## 1. IDE Features (from Win32IDE_FeatureManifest.cpp)

| Category | Features |
|----------|----------|
| File Operations | New, Open, Save, Save As, Save All, Close, Recent Files, Load GGUF Model, Model from HF/Ollama/URL, Unified/Quick Load |
| Editing | Undo, Redo, Cut, Copy, Paste, Select All, Find, Replace |
| View & Layout | Minimap, Output Panel, Floating Panel, Theme Editor, Module Browser, Sidebar, Secondary Sidebar, Panel |
| Themes | Dark+, Monokai, Dracula, 16 built-in themes |
| Syntax Highlighting | C/C++, Assembly, 6 languages (C++, ASM, Python, JS, Rust, GLSL) |
| Terminal | New Terminal, Split Terminal, Kill Terminal, Split Code Viewer |
| Agent | Agent Loop, Execute, Configure Model, View Tools/Status, Stop, Memory, History, Failure Detection, Failure Intelligence |
| Autonomy | Toggle, Set Goal, Rate Limiter |
| AI Mode | Deep Thinking, Deep Research, No Refusal, Context Window (4K–1M) |
| Debugger | Start, Breakpoint, Step Over, Native Debug Engine (DbgEng) |
| Reverse Engineering | PE Analysis, Disassembly, DumpBin, MASM Compile, CFG, Functions, Demangle, SSA, etc. |
| Decompiler View | Direct2D Decompiler, Syntax Color, Sync Selection, Variable Rename |
| Hotpatch | Memory, Byte-Level, Server Hotpatch, Panel UI, Unified Manager |
| Streaming UX | Token Streaming, Ghost Text Completion |
| Session | Save Session, Restore Session |
| Git | Status, Commit, Push, Pull |
| SubAgent | Spawn, Chain, Swarm, Todo List |
| Swarm | Swarm Panel |
| LLM Router | Multi-Engine Router, Backend Switcher |
| LSP | LSP Client, LSP↔AI Bridge, LSP Server, Symbol Indexer, Hover, Completion, Go-to-Definition, References, Symbols, Semantic Tokens, Diagnostics, Stdio |
| PowerShell | Execute, Panel, RawrXD PS Module |
| Settings | Editor Settings |
| Annotations | Inline Annotations |
| Local Server | Built-in HTTP inference server |
| Plan Executor | Multi-step plan approval & execution |
| Execution Governor | Rate limit & safety controls |
| WebView2 / Monaco | Container, Monaco editor, Theme Bridge, etc. |
| Headless | Headless mode, HTTP server, REPL, Single-shot, Batch |
| Other Panels | Dual Agent, Native Debug, Hotpatch, Pipeline, Static Analysis, etc. |

**Total:** ~215+ features across ~30 categories.

---

## 2. win32app Directory — Sourced but Not in IDE Build

| File | Notes |
|------|-------|
| IDEDiagnosticAutoHealer_Impl.cpp | Explicitly excluded in CMake (duplicate DiagnosticUtils) |
| IDEAutoHealerLauncher.cpp | Excluded (defines main, conflicts with ASM) |
| digestion_engine_stub.cpp | Used elsewhere (MASM/ASM), not in Win32 IDE |
| digestion_test_harness.cpp | Test harness; standalone |
| agentic_bridge_headless.cpp | RawrEngine only; headless stub |
| RawrXD_TextEditor_Win32.cpp | GOLD_SOURCES only |
| RawrXD_SettingsManager_Win32.cpp | GOLD_SOURCES only |
| RawrXD_TerminalManager_Win32.cpp | GOLD_SOURCES only; Win32 IDE uses Win32TerminalManager.cpp |
| RawrXD_ResourceManager_Win32.cpp | GOLD_SOURCES only |

---

## 3. Feature Status — CORRECTED (Win32 IDE)

### ✅ Implemented (Win32 IDE)

| Feature | Source File | Notes |
|---------|-------------|-------|
| Plan Executor | Win32IDE_PlanExecutor.cpp | Plan generation, approval, step execution via AgenticBridge |
| Execution Governor | Win32IDE_ExecutionGovernor.cpp | initPhase10, safety contract, replay, confidence gate |
| Multi-Response Engine | Win32IDE_MultiResponse.cpp | 4 templates, generate, HTTP endpoints; init at startup |
| Failure Detection | Win32IDE_FailureDetector.cpp | 12 failure types, correction strategies |
| Failure Intelligence | Win32IDE_FailureIntelligence.cpp | Analytics, history, HTTP API |
| Inline Annotations | Win32IDE_Annotations.cpp | Overlay, gutter icons, click actions; createAnnotationOverlay wired |
| LSP Server | RawrXD_LSPServer.cpp, Win32IDE_LSPServer.cpp | LSP 3.17 subset, hover, completion, definition, diagnostics; init at startup |
| Hotpatch (Unified) | UnifiedHotpatchManager, Win32IDE_HotpatchPanel | Memory/Byte/Server layers, IDM 9001–9030; initHotpatchUI at startup |
| Swarm Panel | Win32IDE_SwarmPanel.cpp | Phase 11 SwarmCoordinator, SwarmWorker, HTTP /api/swarm/* |

### ⚠️ Partial or Variant Gaps

| Feature | Status | Notes |
|---------|--------|-------|
| agent.viewTools | Real (Win32), Partial (React) | Win32IDE_AgentCommands.cpp |
| agent.configureModel | Real (Win32) | Win32IDE_AgentCommands.cpp |
| Decompiler | Real (Win32), limited backend | Some features partial |
| Native Debug Engine | Real (Win32), Missing (CLI) | DbgEng; CLI has no GUI |
| SubAgent chain/swarm/todo | Real (Win32), Missing (CLI/React) | Win32IDE_SubAgent.cpp |

### 📋 Handler Implementation

- **feature_handlers.cpp:** Upgraded. GUI mode → PostMessage; CLI mode → CreateFile, WinHTTP, UnifiedHotpatchManager, etc.
- **missing_handler_stubs.cpp:** ~153 handlers wired to real subsystems (AgentOllamaClient, NativeDebuggerEngine, MultiResponseEngine, ExecutionGovernor, VisionEncoder, TrainingPipelineOrchestrator, etc.). Tier 3 (end section) handlers are wired, not print stubs.
- **ssot_handlers.cpp:** Delegates to GUI (PostMessage) or CLI (print); CLI paths are lightweight.

---

## 4. Files Not in WIN32IDE but in GOLD_SOURCES

| File | Notes |
|------|-------|
| RawrXD_TextEditor_Win32.cpp | Qt-converted Win32 stack |
| RawrXD_SettingsManager_Win32.cpp | Gold-only |
| RawrXD_TerminalManager_Win32.cpp | Gold uses this; Win32 IDE uses Win32TerminalManager.cpp |
| RawrXD_ResourceManager_Win32.cpp | Gold-only |

---

## 5. Summary

| Metric | Value |
|--------|-------|
| Features documented | ~215+ across ~30 categories |
| Win32 IDE status | Most features **Real**; many subsystems wired at startup |
| Excluded sources | 4 explicit; 4 RawrEngine/Gold-only |
| Handler coverage | feature_handlers + missing_handler_stubs; ~267+ handlers wired to real subsystems |
| Remaining gaps | CLI/React variants for some features; ssot_handlers CLI paths print-only |

---

## 6. Startup Initialization (as of Feb 2026)

These subsystems are initialized at Win32 IDE startup:

- `initPhase10` (Execution Governor)
- `initMultiResponse`
- `initLSPServer`
- `initHotpatchUI`
- `initFailureDetector`
- `initFailureIntelligence`
- `createLineNumberGutter`
- `createAnnotationOverlay`
