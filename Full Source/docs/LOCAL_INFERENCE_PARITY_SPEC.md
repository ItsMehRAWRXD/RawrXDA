# Local Inference Parity — Zero-API Cursor/Copilot Parity (x64 MASM)

**Role:** Kernel reverse engineer / x64 MASM specialist  
**Goal:** Achieve full Cursor/Copilot parity using **local GGUF inference only** — no API keys, no cloud, no HTTP to external LLM providers.

---

## 1. High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     RAWRXD LOCAL INFERENCE PARITY STACK                      │
├─────────────────────────────────────────────────────────────────────────────┤
│  Cursor Parity UI (8 modules, 50 commands) — Win32/C++20 — ALREADY DONE     │
│  Telemetry │ Composer │ @-Mentions │ Vision │ Refactor │ Lang │ Semantic │ Res │
└─────────────────────────────────────────────────────────────────────────────┘
                                        │
                                        ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│  CONTEXT ASSEMBLY (C++20) — ALREADY DONE                                    │
│  ContextMentionParser::AssembleContext() │ VisionEncoder::BuildMultimodalPayload │
└─────────────────────────────────────────────────────────────────────────────┘
                                        │
                                        ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│  LOCAL INFERENCE ENGINE — TO BUILD (MASM-heavy)                             │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐ │
│  │ Tokenizer   │→ │ Embed       │→ │ Transformer │→ │ Sampling + Decode   │ │
│  │ (BPE/Sentencepiece) │ │ (embeddings) │ │ (layers)    │ │ (top-k, temp)    │ │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────────────┘ │
│       C++20            MASM            MASM                MASM + C++       │
└─────────────────────────────────────────────────────────────────────────────┘
                                        │
                                        ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│  GGUF MODEL (Q4_0 / Q8_0) — User supplies, no API                           │
│  CodeLlama, DeepSeek-Coder, Mistral, Phi-3, etc.                            │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. What Already Exists (Reuse)

| Component | Location | Status |
|-----------|----------|--------|
| GGUF loader | `src/gguf_loader.cpp`, `include/gguf_loader.h` | ✓ Loads header, metadata, tensors |
| Flash Attention (AVX2) | `kernels/flash_attn_asm_avx2.asm` | ✓ Production MASM, Q8_0 K support |
| Q4_0 GEMM | `kernels/q4_0_gemm_avx2.cc` | ✓ Tile-based, calls `q4_0_unpack_64x64` |
| Q4_0 unpack | `src/asm/` or ggml | ✓ `extern "C" void q4_0_unpack_64x64` |
| Matmul kernel | `kernels/matmul_kernel_avx2.cc` | ✓ AVX2 FMA (C intrinsics; can MASM-ify) |
| ggml_masm bridge | `src/ggml_masm/`, `ggml_masm_bridge.h` | ✓ Quantize, dequant, mul_mat, attention, RoPE, GELU, RMS norm |
| KV cache stubs | `src/ggml_masm/kv_cache.asm` | Partial — needs sliding-window logic |
| Cursor parity UI | `src/win32app/Win32IDE_CursorParity.cpp` | ✓ All 8 modules wired |
| Context assembly | `ContextMentionParser`, `VisionEncoder` | ✓ Assembles prompt for LLM |
| ModelInvoker | `src/agent/model_invoker.hpp` | ✗ HTTP-only — **ADD local path** |

---

## 3. Gap Analysis — What to Build from Scratch

### 3.1 Tokenizer (BPE / SentencePiece)

**Why:** GGUF stores vocab in metadata; inference needs text→token_ids.

**Approach:**
- Parse `tokens` + `token_scores` + `token_types` from GGUF metadata (already in `GGUFMetadata`).
- Implement **BPE merge** or **SentencePiece** in pure C++20.
- **MASM hot path:** `tokenize_fast.asm` — hash lookup for vocab, LCP search for longest match.

```asm
; tokenize_fast.asm — Fast BPE tokenizer core
; rcx = input ptr (char*), rdx = output ptr (int32_t*), r8 = max_tokens
; Returns token count in rax
; Uses AVX2 for 32-byte scan of input; 16-way parallel hash for vocab lookup
```

**Alternative:** Vendor minimal `llama.cpp` tokenizer (single file, no deps) — but spec says "from scratch", so prefer in-house BPE.

### 3.2 Embedding Layer

**Why:** Map token_ids → float embedding matrix.

**Existing:** `ggml_masm_mul_mat` or `ggml_masm_gemv_f32` for lookup.

**MASM:** `embed_lookup.asm` — gather rows from embedding matrix by token id. AVX2 `VGATHERDPS` for 8 floats at a time.

```asm
; embed_lookup.asm
; rcx = embedding_matrix (float*), rdx = token_ids (int32_t*), r8 = output (float*)
; r9 = vocab_size, head_dim
; Per-token: VGATHERDPS from embedding + token_id * head_dim
```

### 3.3 Transformer Forward Pass (Single Layer)

**Loop:** For each layer:
1. RMS norm (input) — `ggml_masm_rms_norm`
2. Q, K, V projections — `ggml_masm_mul_mat_q4_0` (weights from GGUF)
3. RoPE — `ggml_masm_rope_f32`
4. Flash Attention — `flash_attn_asm_avx2` (or `ggml_masm_flash_attn_f32`)
5. Output projection — `ggml_masm_mul_mat_q4_0`
6. Residual add
7. RMS norm
8. FFN: gate + up proj, SiLU, down proj — `ggml_masm_silu`, `ggml_masm_mul_mat_q4_0`
9. Residual add

**All ops exist** in ggml_masm or kernels. Need **orchestrator** in C++ that:
- Loads layer weights from GGUF
- Dispatches to MASM routines
- Manages KV cache append

### 3.4 KV Cache (Sliding Window)

**Spec:** `kv_cache.asm` exists but needs full impl.

**MASM layout:**
```
; Flat buffer: [K_0..K_{seq-1}][V_0..V_{seq-1}] per layer
; Sliding: when seq > max, shift left by (seq - max + overlap)
; Overlap = 512 tokens typical
```

```asm
; kv_cache_append.asm — Append K,V for new token
; kv_cache_slide.asm — Evict oldest, shift, keep overlap
```

### 3.5 Sampling + Decode

**Ops:** Logits → token_id (top-k, top-p, temperature).

**C++20:** Simple — argmax or multinomial. No need for MASM here.

**Decode:** Token_ids → text. Use GGUF vocab (tokens array). Reverse BPE merge or direct lookup.

### 3.6 ModelInvoker Local Path

**Change:** Add `ModelInvoker::invokeLocal(const InvocationParams& params)`:
1. Load GGUF from `config/models/<model>.gguf` (user-provided path)
2. Tokenize `params.wish` + `params.context` via in-house tokenizer
3. Run transformer forward (loop layers)
4. Sample next token, append to sequence, repeat until EOS or max_tokens
5. Decode token sequence to string
6. Return `LLMResponse` with `rawOutput` = generated text

**No API key, no HTTP.**

---

## 4. MASM Kernel Additions (From Scratch)

| Kernel | Purpose | Priority |
|--------|---------|----------|
| `tokenize_fast.asm` | BPE/vocab lookup hot path | P0 |
| `embed_lookup.asm` | Token→embedding gather | P0 |
| `kv_cache_append.asm` | Append K,V to cache | P0 |
| `kv_cache_slide.asm` | Sliding window eviction | P1 |
| `matmul_kernel_avx2.asm` | Replace C intrinsics with pure MASM | P2 |
| `rms_norm_avx2.asm` | Inline SIMD for norm | P2 |
| `silu_avx2.asm` | SiLU activation (x * sigmoid(x)) | P2 |

### 4.1 tokenize_fast.asm — Skeleton

```asm
; tokenize_fast.asm — BPE tokenizer core (x64 MASM)
; Win64 ABI: rcx=text, rdx=vocab_ptr, r8=merge_rules, r9=output_ids
; [rsp+28h] = max_tokens

.code
tokenize_fast PROC
    push    rbp
    mov     rbp, rsp
    ; ... load vocab (tokens), build hash table or sorted LCP trie
    ; ... for each byte position: longest match lookup
    ; ... emit token id, advance pointer
    ; ... loop until EOS or max_tokens
    pop     rbp
    ret
tokenize_fast ENDP
END
```

### 4.2 embed_lookup.asm — Skeleton

```asm
; embed_lookup.asm — VGATHERDPS-based embedding lookup
; rcx=embeddings, rdx=ids, r8=out, r9=count, [rsp+28h]=dim
embed_lookup_avx2 PROC
    vzeroupper
    ; for i in 0..count-1:
    ;   id = ids[i]
    ;   base = embeddings + id * dim
    ;   VGATHERDPS ymm0, [rcx + ymm_index * 4]
    ;   VMOVUPS [r8 + i*dim], ymm0
    vzeroupper
    ret
embed_lookup_avx2 ENDP
```

---

## 5. Parity Feature → Local Inference Mapping

| Cursor Parity Feature | How It Uses LLM | Local Equivalent |
|-----------------------|-----------------|------------------|
| Agentic Composer | Chat/compose with file changes | `invokeLocal()` with context = open files + changes |
| @-Mentions | Assemble @codebase, @file context | `ContextMentionParser::AssembleContext()` → tokenize → infer |
| Vision | Multimodal payload (image + text) | **Defer:** Most GGUF code models are text-only. Use image→caption stub or wait for LLaVA-style small model. |
| Refactoring | "Extract method" etc. → LLM suggests edit | `invokeLocal()` with prompt "Refactor: extract method from selection" |
| Semantic Index | Find refs, deps → context for LLM | `SemanticIndexEngine` output → inject into prompt |
| Resource Generator | "Generate Dockerfile" | `invokeLocal()` with template-style prompt |
| Telemetry / Language | No LLM | Already local |

**Vision gap:** To reach 100% parity, either:
- Integrate a small vision encoder (e.g. CLIP) + projection into text space, or
- Use a GGUF model with vision (LLaVA, Phi-3-Vision) and add image patch embedding in MASM.

---

## 6. Build Integration

1. **New target:** `LocalInferenceEngine` — C++20 + ASM.
2. **CMake:** Add `ml64.exe` for `.asm` files; link `ggml_masm`, `flash_attn_asm_avx2`, `q4_0_gemm`, new kernels.
3. **No new deps:** No curl, no nlohmann in inference path (use manual JSON for config only).
4. **Model path:** `RAWRXD_MODEL_PATH` env or `config/local_model.gguf`.

---

## 7. Implementation Order

| Phase | Deliverable | Est. |
|-------|-------------|------|
| 1 | Tokenizer (C++ BPE, no MASM) + vocab from GGUF | 2–3 days |
| 2 | `ModelInvoker::invokeLocal()` shell: load GGUF, tokenize, stub forward | 1 day |
| 3 | Single-layer forward: RMS norm → Q/K/V → RoPE → FlashAttn → FFN | 2–3 days |
| 4 | Full layer loop + sampling + decode | 1–2 days |
| 5 | KV cache (append + slide) in MASM | 1–2 days |
| 6 | `tokenize_fast.asm`, `embed_lookup.asm` | 1–2 days |
| 7 | Wire Composer, @-mentions, Refactor to `invokeLocal` when no API key | 1 day |
| 8 | Vision: stub or LLaVA-style integration | Optional |

---

## 8. Summary

- **No API key required:** All inference runs locally via GGUF + MASM kernels.
- **Parity:** Same 8 Cursor modules; they feed context into `invokeLocal()` instead of HTTP.
- **MASM-heavy:** Flash attention, Q4 GEMM, embed lookup, KV cache, tokenizer hot path.
- **Reuse:** GGUF loader, ggml_masm, kernels (flash_attn, q4_0, matmul), Cursor parity UI.

The system is a **local, offline Cursor clone** — same UX parity, zero cloud dependency.
