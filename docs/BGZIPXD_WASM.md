# BGzipXD WASM — stack identity

**One product name for the combined lane:**

```text
Loader  +  Runner  +  Codec  +  Reverse-engineered  =  BGzipXD WASM
```

| Pillar | Role | Primary anchors (this repo) |
|--------|------|-----------------------------|
| **Loader** | Map / stream / parse GGUF (and related) weights + metadata into a runnable view | `src/gguf_loader.cpp`, `src/streaming_gguf_loader.cpp`, `src/rawrxd_model_loader.cpp` |
| **Runner** | Token loop, quant matmul path, `ModelRuntimeGate` (resident vs generating), adapter hooks | `src/llm_adapter/GGUFRunner.{h,cpp}`, `src/llm_adapter/QuantBackend.*`, `src/core/model_runtime_gate.*` |
| **Codec** | Brutal/stored-block transport (gzip-shaped payloads, optional MASM path when linked) | `include/brutal_gzip.h`, `src/codec/brutal_gzip.cpp`, `src/codec/gzip_brutal_inflate.*`, `src/gpu_masm/deflate_brutal_masm.asm` |
| **Reverse-engineered** | RE library, ingest walls, canvas batching, parity registers — “what we extracted and wired” | `src/reverse_engineering/` (via `RawrXD-RE-Library`), `src/runtime_surface/IngestReverseWall.cpp`, `src/runtime_surface/CanvasREBatch.cpp`, `src/runtime_surface/LlamaExllamaParityRegister.cpp` |

## WASM read

**BGzipXD WASM** is the *same four pillars*, compiled for a **WebAssembly** deliverable (browser or WASI): loader + runner stay the core; codec supplies compact on-wire blobs; RE surface is the spec/manifest + ingest path that keeps parity with native.

Native Win32 today = this stack with Win32 I/O and MASM where enabled; WASM = same logical equation, different platform layer (no Win32, no ml64 — swap codec/runner backends per target).

## Related

- `docs/INTEGRATED_RUNTIME.md` — IDE boot wiring for the larger transcendence coordinator
- `docs/TASK_BACKLOG_PRODUCTION_AGENTIC_RUNTIME_BATCH1.md` — production batch items that touch these pillars
