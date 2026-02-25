# RawrXD — Final Architecture Drawing: MASM x64 + C++20

**Purpose:** Canonical ASCII diagram for the language split and integration boundary.  
**Style:** Same as `ARCHITECTURE.md` (Three-Layer Hotpatch, Execution Model, Four-Pane Layout).  
**Ref:** `docs/CURSOR_GITHUB_PARITY_SPEC.md` §3.1 Language split.

---

## 1. Language Split (Final Drawing)

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                         C++20 LAYER (Control & Orchestration)                    │
├─────────────────────────────────────────────────────────────────────────────────┤
│  • Control flow, data structures, UI orchestration                               │
│  • Plugin loading (Win32 LoadLibrary/GetProcAddress)                             │
│  • Win32 API calls, WinHTTP (update/release/GitHub paths)                         │
│  • File I/O, parsing (manual JSON in critical path)                               │
│  • AgenticEngine, PolicyEngine, SubAgentManager, FailureDetector, Puppeteer     │
│  • UnifiedHotpatchManager, ProxyHotpatcher (function pointers)                   │
│  • Win32IDE: commands, menus, panes, LSP client, streaming UX                     │
│  • Telemetry export, Cursor Parity menu, Vision encoder, RefactoringPlugin     │
└─────────────────────────────────────────────────────────────────────────────────┘
                                        │
                    extern "C" / object linkage (ml64 .obj → link)
                                        │
                                        ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                         x64 MASM LAYER (Hot Paths & Kernels)                      │
├─────────────────────────────────────────────────────────────────────────────────┤
│  • Inference: SGEMM/SGEMV AVX2 6×16, AVX-512 6×32 (inference_core.asm)            │
│  • Flash Attention v2, tiled, online softmax (FlashAttention_AVX512.asm)        │
│  • Dequant: Q4_0/Q8_0, K-quant Q4_K/Q6_K/F16 (quant_avx2, RawrXD_KQuant_Dequant) │
│  • Memory patch (VirtualProtect-wrapped), byte search (SIMD Boyer-Moore)          │
│  • Request/response interception (request_patch.asm)                             │
│  • Disassembler: RawrCodex.asm (master disassembler, SSA/CFG)                    │
│  • Optional: brutal_gzip, custom codec, tokenization hot loops                    │
└─────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Build & Link Flow

```
┌──────────────────┐         ┌──────────────────┐         ┌──────────────────┐
│   C++20 sources  │         │   MASM64 sources  │         │       LINK        │
│   (.cpp / .hpp)  │         │   (.asm / .inc)   │         │                   │
│   MSVC / MinGW   │         │   ml64.exe /c     │         │   link.exe        │
│   cl / g++       │         │   /Fo *.obj       │         │   *.obj (C+++ASM) │
└────────┬─────────┘         └────────┬──────────┘         └────────┬─────────┘
         │                            │                             │
         │   .obj                     │   .obj                      │
         └───────────────────────────┴─────────────────────────────┘
                                     │
                                     ▼
                          ┌──────────────────────┐
                          │  RawrEngine /       │
                          │  RawrXD-Win32IDE /  │
                          │  single executable  │
                          │  or DLL set         │
                          └──────────────────────┘
```

---

## 3. Call Boundary (C++20 → MASM x64)

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│  C++20                                                                           │
│                                                                                  │
│  // Header                                                                       │
│  extern "C" void RawrXD_InferenceCore_SGEMM_AVX2(const float* A, const float* B,  │
│                                                  float* C, int M, int N, int K); │
│  extern "C" void RawrCodex_Disassemble(const uint8_t* code, size_t len, ...);     │
│                                                                                  │
│  // Call site                                                                    │
│  RawrXD_InferenceCore_SGEMM_AVX2(a, b, c, M, N, K);                              │
└─────────────────────────────────────────────────────────────────────────────────┘
                                        │
                                        │  ABI: x64 Windows calling convention
                                        │  (RCX, RDX, R8, R9, stack; volatile RAX, etc.)
                                        ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│  MASM64 (.asm)                                                                   │
│                                                                                  │
│  .code                                                                           │
│  RawrXD_InferenceCore_SGEMM_AVX2 proc                                            │
│      ; RCX=A, RDX=B, R8=C, R9d=M, stack N, K                                     │
│      ...                                                                         │
│      ret                                                                         │
│  RawrXD_InferenceCore_SGEMM_AVX2 endp                                            │
│  end                                                                             │
└─────────────────────────────────────────────────────────────────────────────────┘
```

---

## 4. Summary Table

| Layer    | Language | Role |
|----------|----------|------|
| **Top**  | C++20    | Control flow, UI, Win32, WinHTTP, plugins, agentic engine, hotpatch manager, IDE. |
| **Bottom** | x64 MASM | Inference kernels, Flash Attention, dequant, memory/byte patch, byte search, RawrCodex, optional codec/tokenization. |
| **Boundary** | extern "C" + ml64 .obj | C++ calls ASM via declared `extern "C"` symbols; linker resolves from MASM .obj. |

---

**Version:** 1.0  
**Date:** 2026-02-20  
**References:** `ARCHITECTURE.md`, `docs/CURSOR_GITHUB_PARITY_SPEC.md` §3.1, `src/asm/`, `tools/genesis_build.ps1`.
