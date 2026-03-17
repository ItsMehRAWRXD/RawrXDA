# RawrXD IDE — Architecture Reference

> **Version 7.4.0** | **150 Compilation Units** | **9 MASM64 Kernels** | **3.73 MB Binary**

## Overview

RawrXD is a native Win32 C++20 IDE with integrated AI inference, reverse engineering, and agentic automation. It compiles to a single ~3.7 MB executable with zero Electron/Qt/CEF dependencies.

```
┌─────────────────────────────────────────────────────────────────┐
│                        Win32IDE (GUI)                           │
│  ┌──────────┐ ┌──────────┐ ┌───────────┐ ┌──────────────────┐  │
│  │ Editor   │ │ Sidebar  │ │ Terminal  │ │  Chat Panel      │  │
│  │ Engine   │ │ Explorer │ │ Manager   │ │  (CoT + Voice)   │  │
│  └────┬─────┘ └────┬─────┘ └─────┬─────┘ └────────┬─────────┘  │
│       └─────────────┴─────────────┴────────────────┘            │
│                            │                                     │
├────────────────────────────┼─────────────────────────────────────┤
│                     Core Engine Layer                            │
│  ┌───────────┐ ┌──────────┐ ┌───────────┐ ┌──────────────────┐  │
│  │ Inference │ │ Hotpatch │ │ PDB/MSF   │ │ Accelerator      │  │
│  │ Engine    │ │ 3-Layer  │ │ v7.00     │ │ Router           │  │
│  └─────┬─────┘ └────┬─────┘ └─────┬─────┘ └────────┬─────────┘  │
│        └─────────────┴─────────────┴────────────────┘            │
│                            │                                     │
├────────────────────────────┼─────────────────────────────────────┤
│                     MASM64 Kernel Layer                          │
│  ┌──────────┐ ┌──────────┐ ┌───────────┐ ┌──────────────────┐  │
│  │ AVX-512  │ │ GSI Hash │ │ MonacoCore│ │ StubDetector     │  │
│  │ Inference│ │ + RefProv│ │ GapBuffer │ │ AuditKernel      │  │
│  └──────────┘ └──────────┘ └───────────┘ └──────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

## Phase Index

| Phase | Component | Files |
|-------|-----------|-------|
| 1-8 | Core Engine (GGUF, BPE, Sampler, Streaming) | `src/engine/*.cpp` |
| 9 | Streaming Engine Registry | `src/core/streaming_engine_registry.cpp` |
| 11 | Distributed Swarm Compilation | `src/core/swarm_*.cpp`, `src/cli/swarm_orchestrator.cpp` |
| 12 | Native Debugger Engine | `src/core/native_debugger_engine.cpp` |
| 14.2 | Three-Layer Hotpatch System | `src/core/*hotpatch*.cpp`, `src/server/gguf_server_hotpatch.cpp` |
| 18 | Agent-Driven Hotpatch Orchestration | `src/agent/agentic_hotpatch_orchestrator.cpp` |
| 19 | Feature Manifest + Agentic Decision Tree | `src/cli/agentic_decision_tree.cpp` |
| 19B | 14 Missing Win32 Features | `src/win32app/Win32IDE_*.cpp` |
| 19C | Headless IDE (REPL + HTTP) | `src/win32app/HeadlessIDE.cpp` |
| 20 | CLI Safety, Confidence, Replay, Governor | `src/core/agent_safety_contract.cpp`, etc. |
| 21 | Swarm Bridge + Model Hotpatcher | `src/core/swarm_decision_bridge.cpp` |
| 22 | Production Release Engineering | `src/core/production_release.cpp` |
| 23 | GPU Kernel Auto-Tuner | `src/core/gpu_kernel_autotuner.cpp` |
| 24 | Windows Sandbox Integration | `src/core/sandbox_integration.cpp` |
| 25 | AMD GPU Acceleration | `src/core/amd_gpu_accelerator.cpp` |
| 26 | WebView2 + Monaco Integration | `src/win32app/Win32IDE_WebView2.cpp` |
| 27 | Embedded LSP Server (JSON-RPC 2.0) | `src/lsp/RawrXD_LSPServer.cpp` |
| 28 | MonacoCore Native Editor Engine | `src/core/MonacoCoreEngine.cpp` |
| 29 | Native PDB Symbol Server (MSF v7.00) | `src/core/pdb_native.cpp` |
| 29.2 | GSI Hash + Reference Router | `src/core/pdb_gsi_hash.cpp`, `src/core/pdb_reference_provider.cpp` |
| 30 | Unified Accelerator Router | `src/core/accelerator_router.cpp` |
| 31 | IDE Self-Audit + Verification | `src/core/feature_registry.cpp`, `src/core/menu_auditor.cpp` |
| 32 | Final Gauntlet (Pre-Packaging Verification) | `src/core/final_gauntlet.cpp` |
| 32A-B | Chain-of-Thought Engine | `src/core/chain_of_thought_engine.cpp` |
| 33 | Voice Chat + Quick-Win Ports + Gold Master | `src/core/voice_chat.cpp`, etc. |

## MASM64 Kernel Index

| Kernel | File | Purpose |
|--------|------|---------|
| inference_core | `src/asm/inference_core.asm` | Core GGUF tensor computation |
| FlashAttention_AVX512 | `src/asm/FlashAttention_AVX512.asm` | Flash Attention v2 SIMD |
| quant_avx2 | `src/asm/quant_avx2.asm` | AVX2 quantization ops |
| KQuant_Dequant | `src/asm/RawrXD_KQuant_Dequant.asm` | K-quant dequantization |
| memory_patch | `src/asm/memory_patch.asm` | Hotpatch memory layer |
| byte_search | `src/asm/byte_search.asm` | Hotpatch byte-level search |
| request_patch | `src/asm/request_patch.asm` | Hotpatch server layer |
| inference_kernels | `src/asm/inference_kernels.asm` | Supplementary inference ops |
| MonacoCore | `src/asm/RawrXD_MonacoCore.asm` | Gap buffer + tokenizer (21 KB) |
| PDBKernel | `src/asm/RawrXD_PDBKernel.asm` | MSF magic, PubSym scan, GUID hex |
| RouterBridge | `src/asm/RawrXD_RouterBridge.asm` | Accelerator fast-path dispatch |
| GSIHash | `src/asm/RawrXD_GSIHash.asm` | PDB hash + GSI bucket probe |
| RefProvider | `src/asm/RawrXD_RefProvider.asm` | FNV-1a reference hash |
| StubDetector | `src/asm/RawrXD_StubDetector.asm` | Stub pattern scanner |
| requantize_avx512 | `src/asm/requantize_q4km_to_q2k_avx512.asm` | Q4_K_M → Q2_K requantization |
| gpu_requantize_rocm | `src/asm/gpu_requantize_rocm.asm` | HIP dispatch stubs |
| swarm_tensor_stream | `src/asm/swarm_tensor_stream.asm` | CRC32C, RLE, zero-copy chunks |

## Three-Layer Hotpatch System

```
         ┌─────────────────────────────────┐
         │   Unified Hotpatch Manager      │
         │   (routes, stats, presets)       │
         └──────┬──────────┬──────────┬────┘
                │          │          │
         ┌──────▼───┐ ┌───▼──────┐ ┌─▼───────────┐
         │ Memory   │ │ Byte     │ │ Server      │
         │ Layer    │ │ Layer    │ │ Layer       │
         │ VProtect │ │ mmap/    │ │ Req/Resp    │
         │ + mprotect│ │ pattern  │ │ injection   │
         └──────────┘ └──────────┘ └─────────────┘
```

- **Memory Layer:** Direct RAM patching via `VirtualProtect`/`mprotect`. Operates on loaded tensors.
- **Byte Layer:** Precision GGUF binary modification. Pattern search (Boyer–Moore). Atomic mutations (XOR, rotate, swap, reverse).
- **Server Layer:** Runtime request/response modification. Injection points: PreRequest, PostRequest, PreResponse, PostResponse, StreamChunk.

## GPU Backend Abstraction

```
         ┌──────────────────────────────────┐
         │     Accelerator Router            │
         │  (thermal-aware, deterministic)   │
         └──────┬──────┬──────┬──────┬──────┘
                │      │      │      │
         ┌──────▼┐ ┌───▼──┐ ┌▼─────┐ ┌▼────────┐
         │ AMD   │ │Intel │ │ARM64 │ │Cerebras │
         │ ROCm  │ │oneAPI│ │ NPU  │ │ WSE     │
         └───────┘ └──────┘ └──────┘ └─────────┘
```

## Build System

```bash
# Prerequisites: MSVC 2022 + CMake 3.20+ + Ninja
cmake -B build -G Ninja \
    -DCMAKE_C_COMPILER=cl \
    -DCMAKE_CXX_COMPILER=cl \
    -DCMAKE_ASM_MASM_COMPILER=ml64 \
    -DCMAKE_BUILD_TYPE=Release

cmake --build build --target RawrXD-Win32IDE
```

**Targets:**
- `RawrEngine` — CLI engine (headless inference + agentic core)
- `RawrXD-Win32IDE` — Full GUI IDE (includes headless mode via `--headless`)
- `rawrxd-monaco-gen` — React/Monaco code generator

## Surfaces

| Surface | Entry Point | API |
|---------|-------------|-----|
| **Win32 GUI** | `WinMain()` → `Win32IDE` | Native Win32 message loop |
| **Headless REPL** | `--headless` → `HeadlessIDE` | Console I/O + HTTP server |
| **CLI Shell** | `RawrEngine` → `cli_shell.cpp` | Interactive `!command` dispatch |
| **HTTP API** | LocalServer (port 11434) | REST + Ollama-compatible |
| **HTML Frontend** | `gui/ide_chatbot.html` | Browser-based chat UI |

## Key Design Decisions

1. **No exceptions** in hotpatch code — all results via `PatchResult` structs
2. **No STL allocators** inside MASM bridge code
3. **Singleton pattern** for engine registries (thread-safe via `std::mutex`)
4. **Function pointer callbacks** instead of signals/slots (no Qt dependency)
5. **All pointer math** uses `uintptr_t` in memory layer
6. **Header isolation** — no circular includes between layers
