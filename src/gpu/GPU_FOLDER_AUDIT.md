# GPU_FOLDER_AUDIT.md

## Folder: `src/gpu/`

### Summary
This folder holds the C++ glue for GPU acceleration and speculative decoding. The current implementations are still **stubs** that log and return success/failure without exercising real GPU paths. Real hardware-facing logic exists separately in `gpu_masm/` and is not yet wired in here.

### Contents
- `gpu_backend.cpp`: Backend selector; currently stubbed `initializeVulkan`/`initializeCuda` that always succeed/fail respectively and never call ggml or MASM paths.
- `kv_cache_optimizer.cpp`: Basic sliding-window eviction in-memory only; no GPU interop or SIMD.
- `speculative_decoder.cpp`: Generates random tokens and accepts all of them; no model calls.

### Dependency Status
- Stubs currently pretend to use Vulkan/CUDA but do **not** call external SDKs or the MASM backends; ggml integration is absent.
- To reach production, these files must call into `gpu_masm` (or a MASM-friendly shim) and drop all implicit Vulkan/CUDA/ROCm assumptions.
- Qt logging (`QDebug/QInfo`) is the only active dependency in these files.

### Findings
- Behavior is non-functional for GPU work; CPU fallback is the only real path.
- Audit docs previously claimed “no external dependencies”; that was inaccurate relative to the stubbed intent and missing MASM wiring.
- MASM implementations already exist under `gpu_masm/` (see `gpu_backend.asm`, `gpu_detection.asm`, `vulkan/`) and should be invoked from here.

### TODOs (ordered)
- [ ] Wire `gpu_backend.cpp` to the MASM entry points (`InitializeGPUBackend`, `AllocateGPUMemory`, etc.) with clear C bindings.
- [ ] Replace stub `initializeVulkan`/`initializeCuda`/`initializeWithFallback` with real detection + MASM-backed init, keeping CPU fallback.
- [ ] Extend `kv_cache_optimizer.cpp` with deterministic eviction tests and, if needed, hooks for MASM tensor moves.
- [ ] Replace random-token speculative decoding with deterministic draft/verify flow and add regression tests.
- [ ] Add structured logging (INFO/ERROR) and latency measurement around backend init and token flows.

### Audit Status
- Audit revised: **implementation incomplete; GPU paths non-functional.**
- Action required before production: integrate MASM backends, add tests, and remove placeholder logic.
