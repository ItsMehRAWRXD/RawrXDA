# RawrXD Architecture — v14.2.0

> Advanced GGUF Model Loader, Live Hotpatching IDE, and Agentic Execution Platform

---

## 1. System Overview

RawrXD is a **local-first, zero-telemetry agentic platform** that loads GGUF models
directly and provides multi-agent orchestration with full governance. It runs entirely
on the user's machine — no cloud dependencies required.

**Three build targets** share a common agentic core:

| Target | Type | Purpose |
|--------|------|---------|
| `RawrEngine` | Console + HTTP | CLI REPL + REST API on port 8080 |
| `rawrxd-monaco-gen` | Codegen tool | Generates Vite / Monaco / Tailwind React IDEs |
| `RawrXD-Win32IDE` | Win32 GUI | Full native IDE with Direct2D, three-layer hotpatching |

**Requirements:** CMake 3.20+, C++20, MinGW GCC 15+ or MSVC 2022, Windows SDK.
MASM64 (`ml64.exe`) optional for ASM kernel acceleration.

---

## 2. Directory Layout

```
src/
├── main.cpp                          # RawrEngine entry point
├── stubs.cpp                         # Platform stubs (linker)
├── agentic_engine.cpp                # Core agentic loop
├── subagent_core.cpp                 # Sub-agent dispatch
├── agent_history.cpp                 # Append-only JSONL recorder
├── agent_policy.cpp                  # Adaptive policy engine
├── agent_explainability.cpp          # Explainable LLM routing
├── cpu_inference_engine.cpp          # CPU-only transformer inference
├── memory_core.cpp                   # Memory arena management
├── hot_patcher.cpp                   # Legacy hotpatch shim
├── streaming_gguf_loader.cpp         # Streaming GGUF file loader
├── gguf_loader.cpp / gguf_vocab_resolver.cpp
├── model_source_resolver.cpp         # HuggingFace + local path resolution
├── compression_interface.cpp         # Codec abstraction
├── monaco_gen.cpp                    # React IDE generator entry
│
├── engine/                           # Core inference engine
│   ├── gguf_core.cpp                 # GGUF format parser
│   ├── inference_kernels.cpp         # SIMD-optimized kernel dispatch
│   ├── transformer.cpp               # Transformer forward pass
│   ├── bpe_tokenizer.cpp             # Byte-pair encoding tokenizer
│   ├── sampler.cpp                   # Top-k / top-p / temperature sampling
│   ├── rawr_engine.cpp               # High-level engine API
│   ├── core_generator.cpp            # Token generation loop
│   └── react_ide_generator.cpp       # Monaco IDE React codegen
│
├── codec/
│   ├── compression.cpp               # zlib / DEFLATE wrappers
│   └── brutal_gzip.cpp               # Raw gzip stream handler
│
├── core/                             # Shared subsystems
│   ├── streaming_engine_registry.cpp # Multi-engine hot-swap registry
│   ├── gpu_backend_bridge.cpp        # GPU ↔ CPU bridge abstraction
│   ├── enterprise_license.cpp        # License gate logic (Phase 17)
│   ├── enterprise_license_stubs.cpp  # Stub license endpoints
│   ├── multi_response_engine.cpp     # Parallel inference aggregator
│   ├── flash_attention.cpp           # Flash-attention kernel
│   ├── execution_scheduler.cpp       # DAG-based task scheduler
│   ├── execution_governor.cpp        # Rate-limiting + quota enforcement
│   ├── agent_safety_contract.cpp     # Safety invariants
│   ├── deterministic_replay.cpp      # Replay engine (Phase 5)
│   ├── confidence_gate.cpp           # Confidence threshold gating
│   ├── swarm_coordinator.cpp         # Distributed swarm coordinator (Phase 11)
│   ├── swarm_worker.cpp              # Swarm worker process
│   ├── swarm_network_stubs.cpp       # Network transport stubs
│   ├── native_debugger_engine.cpp    # Native debugger engine (Phase 12)
│   ├── debug_engine_stubs.cpp        # Debugger platform stubs
│   │
│   ├── model_memory_hotpatch.hpp/cpp # Layer 1: Memory hotpatching (VirtualProtect)
│   ├── byte_level_hotpatcher.hpp/cpp # Layer 2: Byte-level GGUF patching (mmap)
│   ├── unified_hotpatch_manager.hpp/cpp # Coordination layer (routes to L1/L2/L3)
│   └── proxy_hotpatcher.hpp/cpp      # Token bias, rewrite, termination, validators
│
├── server/
│   └── gguf_server_hotpatch.hpp/cpp  # Layer 3: Server request/response patching
│
├── agent/                            # Agentic failure recovery
│   ├── agentic_failure_detector.hpp/cpp  # Detects refusal/hallucination/timeout
│   └── agentic_puppeteer.hpp/cpp         # Auto-correct failed responses
│
├── asm/                              # MASM64 assembly kernels
│   ├── memory_patch.asm              # VirtualProtect-wrapped memory patching
│   ├── byte_search.asm               # SIMD Boyer-Moore pattern search
│   ├── request_patch.asm             # Server request/response interception
│   ├── inference_core.asm            # AVX2/FMA matrix multiply kernels
│   ├── flash_attn_avx512.asm         # AVX-512 flash attention
│   ├── quant_avx2.asm               # Quantization dequant routines
│   ├── simd_primitives.asm           # SSE/AVX utility primitives
│   └── tokenizer_fast.asm            # Fast BPE merge kernel
│
├── config/
│   └── IDEConfig.cpp                 # IDE configuration management
│
├── utils/
│   └── ErrorReporter.cpp             # Centralized error reporting
│
├── modules/
│   ├── engine_manager.cpp            # Engine lifecycle manager
│   ├── vsix_loader_win32.cpp         # Win32 VSIX extension loader
│   └── codex_ultimate.cpp            # Code generation module
│
└── win32app/                         # Win32 GUI IDE (44+ files)
    ├── main_win32.cpp                # WinMain entry point
    ├── Win32IDE.h                    # Main class declaration (~3750 lines)
    ├── Win32IDE.cpp                  # Core impl + menu bar + window proc
    ├── Win32IDE_Core.cpp             # Window creation + layout
    ├── Win32IDE_VSCodeUI.cpp         # VS Code-like UI rendering
    ├── Win32IDE_Sidebar.cpp          # Activity bar + side panels
    ├── Win32IDE_SyntaxHighlight.cpp  # Multi-language syntax highlighter
    ├── Win32IDE_Themes.cpp           # Theme engine (30+ themes)
    ├── Win32IDE_Commands.cpp         # Command palette + routing (170+ commands)
    ├── Win32IDE_FileOps.cpp          # File I/O, tab management
    ├── Win32IDE_Debugger.cpp         # Integrated debugger UI
    ├── Win32IDE_PowerShell.cpp       # PowerShell backend integration
    ├── Win32IDE_PowerShellPanel.cpp  # Terminal panel UI
    ├── Win32IDE_Logger.cpp           # IDE logging subsystem
    ├── Win32IDE_AgenticBridge.cpp    # Agentic engine ↔ IDE bridge
    ├── Win32IDE_AgentCommands.cpp    # Agent-specific commands
    ├── Win32IDE_Autonomy.cpp         # Autonomous agent execution
    ├── Win32IDE_Annotations.cpp      # Code annotations overlay
    ├── Win32IDE_Session.cpp          # Session save/restore
    ├── Win32IDE_StreamingUX.cpp      # Streaming token display
    ├── Win32IDE_ReverseEngineering.cpp # Disassembler / reverse engineering
    ├── Win32IDE_GhostText.cpp        # Ghost text completion overlay
    ├── Win32IDE_PlanExecutor.cpp     # Plan execution UI
    ├── Win32IDE_FailureDetector.cpp  # Failure detection integration
    ├── Win32IDE_FailureIntelligence.cpp # Failure intelligence UI
    ├── Win32IDE_Settings.cpp         # Settings panel
    ├── Win32IDE_LocalServer.cpp      # Embedded HTTP server
    ├── Win32IDE_BackendSwitcher.cpp  # LLM backend switching
    ├── Win32IDE_LLMRouter.cpp        # Multi-LLM routing
    ├── Win32IDE_LSPClient.cpp        # Language Server Protocol client
    ├── Win32IDE_AsmSemantic.cpp      # ASM semantic analysis
    ├── Win32IDE_LSP_AI_Bridge.cpp    # LSP ↔ AI integration
    ├── Win32IDE_MultiResponse.cpp    # Multi-response comparison
    ├── Win32IDE_ExecutionGovernor.cpp # Execution governance UI
    ├── Win32IDE_SubAgent.cpp         # Sub-agent panel
    ├── Win32IDE_AgentHistory.cpp     # Agent history viewer
    ├── Win32IDE_SwarmPanel.cpp       # Swarm compilation UI (Phase 11)
    ├── Win32IDE_NativeDebugPanel.cpp # Native debugger panel (Phase 12)
    ├── Win32IDE_HotpatchPanel.cpp    # Hotpatch UI integration (Phase 14.2)
    ├── Win32TerminalManager.cpp      # Terminal process management
    └── TransparentRenderer.cpp       # Direct2D transparent rendering
```

---

## 3. Build System

```bash
# Configure (one-time)
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release

# Build all targets
cmake --build build --config Release

# Build individual targets
cmake --build build --config Release --target RawrEngine
cmake --build build --config Release --target RawrXD-Win32IDE
cmake --build build --config Release --target rawrxd-monaco-gen
```

### Link Libraries (Win32IDE)

```
comctl32 comdlg32 shell32 ole32 oleaut32 uuid shlwapi psapi
dbghelp dbgeng winhttp ws2_32 winmm gdi32 user32 d2d1 dwrite
d3d11 dcomp d3dcompiler dwmapi advapi32 crypt32 gomp
```

---

## 4. Three-Layer Hotpatch Architecture

The hotpatch system modifies model behavior at runtime without reloading.
All three layers coordinate through `UnifiedHotpatchManager`.

```
┌─────────────────────────────────────────────────────┐
│              UnifiedHotpatchManager                  │
│  (Routes patches, tracks stats, preset save/load)   │
├─────────────┬──────────────┬────────────────────────┤
│  Layer 1    │  Layer 2     │  Layer 3               │
│  Memory     │  Byte-Level  │  Server                │
│  Hotpatch   │  Hotpatch    │  Hotpatch              │
├─────────────┼──────────────┼────────────────────────┤
│ VirtualProt │ CreateFileMap│ Request/Response        │
│ Direct RAM  │ mmap GGUF    │ transform callbacks     │
│ Tensor patch│ Pattern scan │ Injection points:       │
│             │ XOR/rotate   │  PreReq PostReq         │
│             │ swap/reverse │  PreRes PostRes          │
│             │              │  StreamChunk             │
└─────────────┴──────────────┴────────────────────────┘
        │               │               │
        ▼               ▼               ▼
┌───────────────────────────────────────────────────┐
│              ProxyHotpatcher                       │
│  Token bias injection, output rewriting,           │
│  stream termination, custom validators             │
│  (function pointers, NOT std::function)            │
└───────────────────────────────────────────────────┘
```

### Key Types

```cpp
// Structured result — no exceptions
struct PatchResult {
    bool success; const char* detail; int errorCode;
    static PatchResult ok(const char* msg);
    static PatchResult error(const char* msg, int code);
};

// Event ring buffer for poll-based notification
struct HotpatchEvent {
    enum Type : uint8_t { MemoryPatchApplied, ..., PresetSaved };
    Type type; uint64_t timestamp; uint64_t sequenceId; const char* detail;
};
```

### IDE Integration (Phase 14.2)

Command IDs 9001–9017 map hotpatch operations to the Win32IDE:

| ID | Command | Layer |
|----|---------|-------|
| 9001 | Show Status | All |
| 9002 | Toggle System | All |
| 9003 | Apply Memory Patch | Layer 1 |
| 9004 | Revert Memory Patch | Layer 1 |
| 9005 | Apply Byte Patch | Layer 2 |
| 9006 | Search & Replace | Layer 2 |
| 9007 | Add Server Patch | Layer 3 |
| 9008 | Remove Server Patch | Layer 3 |
| 9009 | Token Bias | Proxy |
| 9010 | Output Rewrite | Proxy |
| 9011 | Stream Termination | Proxy |
| 9012 | Custom Validator | Proxy |
| 9013 | Save Preset | All |
| 9014 | Load Preset | All |
| 9015 | Show Event Log | All |
| 9016 | Reset Stats | All |
| 9017 | Show Proxy Stats | Proxy |

---

## 5. Execution Model

```
User Request
    │
    ▼
┌──────────────┐     ┌──────────────────┐
│ AgenticEngine │◄────│ PolicyEngine     │  (Phase 7)
│ (inference)   │     │ (evaluate rules) │
└──────┬───────┘     └──────────────────┘
       │
       ▼
┌──────────────────────────────────────────────────┐
│              SubAgentManager                      │
│  ┌─────────┐  ┌───────────┐  ┌────────────────┐ │
│  │ SubAgent│  │   Chain   │  │  HexMag Swarm  │ │
│  │ (single)│  │(sequential)│  │  (parallel)    │ │
│  └─────────┘  └───────────┘  └────────────────┘ │
│  ┌─────────────────────────────────────────────┐ │
│  │ AgentHistoryRecorder (append-only JSONL)    │ │
│  └─────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────────────────┐
│         Failure Recovery Pipeline                 │
│  ┌─────────────────┐  ┌────────────────────────┐ │
│  │ FailureDetector  │  │ AgenticPuppeteer       │ │
│  │ refusal, timeout │→│ auto-correct, retry    │ │
│  │ hallucination    │  │ proxy rewrite          │ │
│  └─────────────────┘  └────────────────────────┘ │
└──────────────────────────────────────────────────┘
```

---

## 6. Win32IDE Command Architecture

The Win32IDE uses a VS Code-style command palette (Ctrl+Shift+P) with
170+ registered commands across these ID ranges:

| Range | Category |
|-------|----------|
| 1000–1999 | File operations |
| 2000–2999 | Edit operations |
| 3000–3999 | View / panels |
| 4000–4099 | Terminal |
| 4100–4399 | Agent commands |
| 5000–5999 | Tools / diagnostics |
| 6000–6999 | Modules / extensions |
| 7000–7999 | Help |
| 8000–8999 | Git integration |
| 9000–9999 | Hotpatch system |

Command routing: `routeCommand()` in `Win32IDE_Commands.cpp` dispatches
by ID range, each range calling its category handler.

---

## 7. Rendering Pipeline

- **Direct2D** for text rendering, syntax highlighting, ghost text overlay
- **DirectWrite** for font metrics, subpixel positioning
- **D3D11** compositing layer for transparent overlays
- **DirectComposition** for smooth scrolling and panel transitions
- **GDI fallback** for standard Win32 controls (menus, dialogs, status bar)

---

## 8. Phase History

| Phase | Version | Key Addition |
|-------|---------|-------------|
| 1–4 | v1–v4 | Core IDE, agentic engine, failure detection |
| 5 | v5 | Deterministic replay, history recording |
| 7 | v7 | Policy engine, adaptive governance |
| 9 | v9 | Streaming engine registry, multi-response |
| 11 | v10 | Distributed swarm compilation |
| 12 | v11 | Native debugger engine, SSA lifting |
| 13 | v12 | Ghost text, plan executor |
| 14 | v13 | Reverse engineering, ASM semantic analysis |
| 14.5 | v13.5 | Flash attention, execution governor |
| 15 | v14 | LSP client, LLM router, backend switcher |
| 16 | v14.1 | SSA lifting + recursive descent |
| 17 | v14.1 | Type recovery, data flow, license gates |
| 14.1-stable | v14.1.0 | ASM fixes, hotpatch core file creation |
| **14.2** | **v14.2.0** | **Hotpatch UI integration, docs consolidation** |

---

## 9. Threading Model

- `std::mutex` + `std::lock_guard` — no recursive locks
- `std::atomic` for statistics counters
- OS threads (`CreateThread`) for terminal processes
- Event ring buffer + poll for cross-thread notification
- No `std::function` in hotpatch paths — function pointers only

---

## 10. Critical Rules

1. **No exceptions** — all errors via `PatchResult::ok()` / `PatchResult::error()`
2. **No STL allocators** inside patch code paths
3. **All pointer math** uses `uintptr_t`
4. **Factory results** — `PatchResult::ok("msg")`, never `return {true,"ok",0}`
5. **Header isolation** — no circular includes
6. **NO SOURCE FILE IS TO BE SIMPLIFIED** — all files are canonical

---

## 11. Archived Documentation

344 legacy `.md` files from phases 1–17 have been moved to `docs/archive/`.
This single `ARCHITECTURE.md` is the canonical reference.
