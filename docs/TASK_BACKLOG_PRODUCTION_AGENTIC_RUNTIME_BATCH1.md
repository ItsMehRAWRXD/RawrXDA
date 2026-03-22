# Production backlog — Agentic runtime / loaders / mmap (Batch 1 of N)

**Scope:** Replace stub “10-liners” with shippable subsystems. Work is grouped in **batches of 15** full production instances. This file is **Batch 1** — each item has an acceptance criterion and primary code anchor.

**Doctrine (your 4 lanes):** At steady state, only four **core** subsystems may hold budget concurrently: **Parsing**, **Execution**, **UI rendering**, **Memory**. Optional **Extension** lanes are acquired only when a module is explicitly loaded (see `ModelRuntimeGate` in `src/core/model_runtime_gate.*`). Enforcement is **opt-in** via `RAWRXD_STRICT_RUNTIME_LANES=1` so existing flows keep working until wired everywhere.

**Ingest / cache wall:** All loader paths must honor a single **visibility boundary**: mmap ingest → validated header/tensor table → cache line aligned views → execution only after gate marks **generating** (not merely “file mapped”).

---

## Batch 1 — 15 production items

| # | Deliverable | Primary anchor | Acceptance |
|---|-------------|----------------|------------|
| 1 | **ROCm/HIP backend slice** (discover + init + optional dispatch hook; no fake returns) | `src/core/unified_memory_executor.cpp`, new `src/core/rocm_hip_bridge.*` | On Windows without HIP: clean fallback + structured log; with HIP SDK: `hipInit` succeeds or explicit error path |
| 2 | **Modular loader registry** (mmap / streaming / future exllama-v2 adapter as modules) | New `src/core/model_loader_module_registry.*`, wire from `rawrxd_model_loader.hpp` / `streaming_gguf_loader_mmap.*` | Register at least **mmap** + **streaming** backends; `load(path)` tries ordered policies |
| 3 | **Single NVMe → unified ingest path** (“nvUS” internal: **NV**me **U**nified **S**tream) | `src/streaming_gguf_loader_mmap.cpp`, `docs/` cross-link | One documented pipeline diagram; no duplicate header parsers for the same file |
| 4 | **Full mmap lifecycle** (map, prefetch hint, unmap, error codes) | `StreamingGGUFLoaderMMap`, `rawrxd_model_loader.hpp` | Leak-free open/close; handles >4GB with proper mapping flags where supported |
| 5 | **Shard resource pool** (no full-disk scan at IDE startup) | `src/core/` new `shard_resource_pool.*`, remove/guard any startup `Path::recursive` scans | Startup does **not** walk entire trees; shards register explicitly or via workspace root only |
| 6 | **Strict subprocess MMIO / RE I/O** (sandboxed child + mapped views for binary RE) | `src/win32app/Win32IDE_ReverseEngineering.cpp`, `sandbox_integration.cpp` | Child process + read-only maps; audit log on open |
| 7 | **llama.cpp compatibility matrix** (tensor names, Q4_0/Q8_0 blocks, KV layout) | `src/llm_adapter/GGUFRunner.cpp`, `GGUFConstants.hpp` | Documented parity table vs llama.cpp v1 reference tensors for one arch (e.g. LLaMA) |
| 8 | **exllamav2-style GPU cache plan** (separate from CPU mmap; optional DLL boundary) | New `src/llm_adapter/exllama_v2_plan.md` + stub interface `exllamav2_cache_shim.hpp` | Interface + CMake option; no false “implemented” in hot path |
| 9 | **Stopwatch runtime: idle vs generating** | `src/core/model_runtime_gate.*`, `GGUFRunner::runInference` | Logs **active generation ms**; `isGenerating()` false when not in forward/token loop |
|10 | **Layer onload/offload policy** | `src/win32app/Win32IDE_LayerEviction.cpp`, gate hooks | Policy table: max resident layers, eviction under memory pressure |
|11 | **Load balancer across runner instances** | `src/core/` new `model_runner_load_balancer.*` | Round-robin / least-busy over N `GGUFRunner` or remote endpoints; metrics exposed |
|12 | **Compression-aware pool sizing** (no fixed “math table”; derive from GGUF meta + quant) | `unified_memory_executor.cpp`, `model_memory_hotpatch.hpp` | Heap target computed from file size + `GgmlType` + optional `general.quantization_version` |
|13 | **Signed / narrow quant UI strings** (“bit+4/sign” not “-4bit” typo) | `include/rawrxd/quantization_descriptor.hpp`, settings UI | Stable string API for labels; unit test or compile-time asserts |
|14 | **Vectorized / canvas RE rendering path** (GPU or SIMD path for disasm overlay) | `src/win32app/TransparentRenderer.cpp`, `Win32IDE_ReverseEngineering.cpp` | One benchmarked path (e.g. batch line layout) |
|15 | **Microservice boundaries** (HTTP/gRPC or named-pipe) for parser / executor / render / memory workers | `src/server/`, `src/core/rawrxd_subsystem_api.hpp` | Contract doc + one pipe or localhost port with auth token |

---

## Batch 2 preview (next 15 — do not start until Batch 1 exit criteria met)

- Vulkan compute path for Q4_0 matmul parity vs `ggml_gemm_q4_0`
- ROCm graph capture for steady-state decode
- Deterministic replay for agent + model events
- Workspace-only semantic index (no global scan)
- Merged mmap + GPU staging with true async prefetch
- Exllama v2 weight pack import
- Multi-model **0–1** active generation invariant (only one generating unless data-parallel batch)
- NVMe queue depth telemetry
- Canvas diff engine for RE (structural graph + layout)
- Formal **four-lane** static permutation table per build seed (`constexpr` order, no runtime DLL for order)
- Agent “autonomous” loop tied to `ModelRuntimeGate::isGenerating()`
- Negative-scale quantization display (explicit sign + scale field, not a stray `-`)
- Shard compaction / merge job (offline)
- CI: build + link all loader modules
- Security review for mmap + subprocess map surface

---

## Related code (already in tree)

- `src/streaming_gguf_loader_mmap.*` — mmap streaming GGUF  
- `src/rawrxd_model_loader.hpp` — mmap + tensor table  
- `src/llm_adapter/GGUFRunner.cpp` — inference loop (now gated)  
- `src/core/unified_memory_executor.*` — SAM / host-backed heap  
- `src/core/model_runtime_gate.*` — **generation stopwatch + lane budget (Batch 1 seed)**

---

*Maintainers: tick rows in git commits; keep each item ≤ ~400 LOC per PR or split sub-PRs with the same item id.*
