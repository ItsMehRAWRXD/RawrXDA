# Batch 01 — Runtime surface (15 production tracks)

Order is **not** fixed; load modules in any order. Each row is a **full** implementation target (not a 10-line stub).

| # | Track | Scope |
|---|--------|--------|
| 1 | **mmap I/O** | Windows `CreateFileMapping` / `MapViewOfFile` module: RAII, `std::expected`, Logger; shared read-only model weights + private RW scratch. |
| 2 | **Shard resource pool** | Central registry: fixed arenas per NUMA/socket; refcount + lazy map; no global “scan all files” at startup — explicit `RegisterShard` / `AcquireView`. |
| 3 | **Parsing lane** | Isolated parser service (tokenizer/LSP snapshot ingress): bounded queue, one thread, **no** UI calls. |
| 4 | **Execution lane** | Agent/tool runner: process spawn + job graph; **no** rendering; communicates via message queue only. |
| 5 | **UI lane** | Win32 HWND message pump + composition: **no** model I/O; consumes pre-baked display lists from execution. |
| 6 | **Rendering lane** | Canvas/D2D or GPU path for RE overlays: vectorized glyph/layout batching; **no** parsing. |
| 7 | **Memory lane** | Unified budget: `CompressedModelBytes`, `WorkingSetCap`, `KvCacheCap`; eviction callbacks to execution lane. |
| 8 | **Four-at-a-time gate** | Static bitmap of which of {parse, exec, ui, render} is active; fifth capability only via explicit module load. |
| 9 | **ROCm/HIP bridge** | Optional backend: dynamic load of `amdhip64.dll` / ROCm APIs; same interface as existing `gpu_dispatch_gate` fallback. |
|10 | **Quant metadata** | Signed/unsigned q4/q6 layout descriptors; “negative dash” display vs **true** signed int4/6 decode path (document bit layout in code). |
|11 | **Model lifecycle** | On-demand load: cold → mmap views → first-token path; offload: drop views + IPC handles; load balancer hook (weights only when generating). |
|12 | **Microservice shells** | Named-pipe or localhost JSON-RPC workers: tokenizer, sampler, one per heavy static library boundary. |
|13 | **Stopwatch runtime** | Per-session timer: wall + CPU; gates “model resident” vs “model generating”; telemetry to Logger. |
|14 | **llama.cpp / exllama parity** | Spec sheet: tensor order, KV layout, RoPE, attention — implement in `src/engine` + tests; no shadow DLL without license audit. |
|15 | **Ingest reverse wall** | Single ingress: all external bytes pass verify + manifest hash before map; reject unsigned bundles in production profile. |

**CMake:** keep `-DRAWRXD_ALLOW_AGENTIC_STUB_FALLBACK=ON` until rows 1–8 replace shim coverage.
