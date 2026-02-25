# Local Parity Stack — Same Parity, No API Key (x64 MASM + C++20)

**Goal:** Achieve full Cursor/GitHub parity using only on-device code and **no API keys** (no OpenAI, no Cursor cloud, no GitHub token). All hot paths in **x64 MASM** or **C++20**; inference via local GGUF + in-house kernels.

---

## 0. What “Without an API Key” Means

When we say **without an API key**, we mean the IDE runs **locally, agentically, and autonomously** in the same way Cursor and GitHub Copilot do — but with **zero cloud or keys**:

| Aspect | Cursor / GitHub Copilot | RawrXD local parity (this stack) |
|--------|--------------------------|-----------------------------------|
| **Agentic** | Agent plans, uses tools, edits code, iterates | Same: BoundedAgentLoop + ToolRegistry + local LLM; no key |
| **Autonomous** | Can run multi-step workflows and make decisions | Same: autonomous_workflow_engine, agentic_task_graph, local inference |
| **Local** | Often requires API key + network | **Fully on-device:** GGUF + MASM kernels; no key, no auth |
| **Completion / Chat / Composer** | Remote or local (key if remote) | **Always local** when “local parity” is on: LocalParity_NextToken loop |

So: **no API key** = **local + agentic + autonomous** parity. The same workflows (Composer, @-mentions, refactor, semantic index, etc.) run on your machine with a local model; the agent and autonomous pipelines use the same local inference path and never read or send an API key.

---

## 1. Contract: Zero API Keys

| Capability        | With API Key (old)           | Local Parity (this spec)                    |
|-------------------|-----------------------------|---------------------------------------------|
| Code completion   | Remote LLM API (key required)| Local GGUF + MASM inference kernel          |
| Chat / Composer   | Remote API (key)            | **Local agentic:** same model + BoundedAgentLoop (no key) |
| Telemetry export  | N/A (local)                 | Unchanged — already local                   |
| @-mentions, Vision| N/A (local)                 | Unchanged — already local                   |
| Refactor / Lang   | N/A (local)                 | Unchanged — already local                   |
| Semantic index   | N/A (local)                 | Unchanged — already local                   |
| Resource gen     | N/A (local)                 | Unchanged — already local                   |
| Update manifest   | GitHub API or custom URL    | **Public URL only** — WinHTTP GET, no auth  |
| Create release   | GitHub API + token          | **Local only** — write manifest to disk / optional CDN with key out-of-band |

**Result:** Same 8 Cursor modules + GitHub *read* parity; *write* is local/file or optional later. **Agentic and autonomous** flows use local inference only; no key required for daily use.

---

## 2. Architecture: Local Inference Path

```
┌─────────────────────────────────────────────────────────────────────────┐
│  Cursor/JB Parity Menu (unchanged)                                       │
│  Telemetry | Composer | @-Mention | Vision | Refactor | Lang | Sem | Res │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  Local Parity Bridge (C++20)                                             │
│  - Load GGUF model once; hold model state in process                      │
│  - On completion/chat: build prompt → LocalParity_NextToken loop (MASM)    │
│  - No HTTP to OpenAI/Cursor; no API key read from env                   │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  RawrXD_LocalParity_Kernel.asm (x64 MASM)                                │
│  - LocalParity_Init / LocalParity_Shutdown                               │
│  - LocalParity_NextToken(ctx, state, out_id)  — hot path; calls C++ GGUF │
│  - LocalParity_ManifestGet(URL, buf, cap, out_len) — WinHTTP GET, no auth│
│  - Optional: BPE/tokenizer hot loop (table-driven, no external deps)     │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  Existing kernels (unchanged)                                            │
│  flash_attn_asm_avx2.asm | matmul_kernel_avx2.cc | q4_0_gemm_avx2.cc     │
│  gguf_loader (C++) → forward pass → next token                            │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 3. MASM Kernel ABI (RawrXD_LocalParity_Kernel.asm)

### 3.1 Lifecycle

| Function                     | Purpose |
|-----------------------------|--------|
| `LocalParity_Init()`        | One-time init (optional WinHTTP session for manifest). Returns 1 ok, 0 fail. |
| `LocalParity_Shutdown()`     | Release resources. |

### 3.2 Inference Hot Path (no API key)

| Function | Signature (Win64) | Purpose |
|----------|-------------------|--------|
| `LocalParity_NextToken` | `uint32_t (void* ctx, void* modelState, uint32_t* outTokenId)` | Single next-token step. `ctx` = opaque C++ context (prompt, params); `modelState` = GGUF model state; `outTokenId` = next token. Returns 1 ok, 0 done/error. Implemented in ASM as a thin wrapper that calls a C++ callback registered at init so that GGUF/forward stays in C++; ASM can still do tokenizer or buffer copy hot loops later. |

### 3.3 Manifest Fetch (no auth)

| Function | Signature | Purpose |
|----------|-----------|--------|
| `LocalParity_ManifestGet` | `int (const char* url, char* outBuf, uint64_t cap, uint64_t* outLen)` | WinHTTP GET to `url`; no Authorization header; no API key. Response body written to `outBuf`, length in `*outLen`. Returns 1 ok, 0 fail. |

### 3.4 Optional: Tokenizer Hot Path

| Function | Signature | Purpose |
|----------|-----------|--------|
| `LocalParity_EncodeChunk` | `uint64_t (const char* text, uint64_t len, uint32_t* outIds, uint64_t maxIds)` | Encode up to `len` bytes of `text` into token IDs using in-process BPE/vocab table (no network). Returns number of IDs written. Can be stub in first version; later filled with table-driven MASM. |

---

## 4. C++ Bridge (local_parity_bridge.cpp)

- **LocalParity_RegisterInferenceCallback(void (*fn)(void* ctx, void* state, uint32_t* outId))**  
  Register the C++ function that runs one forward pass (GGUF) and writes next token ID. MASM `LocalParity_NextToken` calls this.

- **LocalParity_SetModelPath(const char* path)**  
  Set path to GGUF file. Load on first `NextToken` or in init.

- **No API key** is read; no env vars for OpenAI/Cursor; optional env for **local** manifest URL only (e.g. `RAWRXD_MANIFEST_URL` for update check).

---

## 5. Update / Release Without GitHub Token

| Feature | Implementation |
|---------|----------------|
| **Check for update** | WinHTTP GET to a **public** URL (e.g. static JSON on CDN or same URL as today but no `Authorization`). Implemented in `LocalParity_ManifestGet`. |
| **Create release** | Not required for parity. Option 1: write manifest + assets to disk; Option 2: upload to CDN via separate tool with its own credentials (out-of-band). No API key in the IDE/release path. |

---

## 6. File Layout

| Component | Path |
|-----------|------|
| Spec | `docs/LOCAL_PARITY_NO_API_KEY_SPEC.md` (this file) |
| MASM kernel | `interconnect/RawrXD_LocalParity_Kernel.asm` |
| C API | `include/local_parity_kernel.h` |
| C++ bridge | `src/core/local_parity_bridge.cpp` |
| Parity bridge (existing) | `include/cursor_github_parity_bridge.h` — add note: "Local parity mode: use LocalParity_*; no API key." |

---

## 7. Checklist

- [x] `RawrXD_LocalParity_Kernel.asm` implements Init, Shutdown, SetInferenceCallback, NextToken (callback), ManifestGet (WinHTTP GET, no auth), EncodeChunk stub.
- [x] `local_parity_kernel.h` declares all exports; C++ bridge registers inference callback and sets model path.
- [ ] Completion/chat path in IDE uses LocalParity_NextToken loop when "local parity" is enabled; no API key.
- [ ] Update check uses `LocalParity_ManifestGet` with public URL only.
- [x] All 8 Cursor parity modules unchanged; only the LLM/completion backend is swapped to local.

This gives **same parity** (Cursor + GitHub read) **from scratch** in x64 MASM + C++20 with **no API key** required.
