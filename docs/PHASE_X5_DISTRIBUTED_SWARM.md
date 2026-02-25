# Phase X+5: Distributed Swarm (Multi-GPU Inference Sharding)

**Goal:** Run inference for 120B+ models by sharding across multiple GPUs (e.g. RX 7800 XT + second GPU). Single logical model, layers or batches split across devices; token generation coordinates hidden state across shards.

---

## Current pieces (already in tree)

| Component | Role |
|-----------|------|
| **SwarmOrchestrator** (`swarm_orchestrator.h/.cpp`) | Task-level consensus, worker queues, `executeTask` / `submitTaskAsync`. Use for high-level “swarm” tasks, not per-token GPU dispatch. |
| **MultiEngineSystem** (`multi_engine_system.h`) | Multi-drive CPU shards (e.g. 800B across 5 drives). Pattern: multiple engines, shard discovery, coordination thread. Today CPU-only. |
| **GGUF-DML bridge** (`core/gguf_dml_bridge.h`, `dml_inference_engine`) | Single (or dual) DML session, GGUF load, layer streaming, VRAM budget. One adapter per session today. |
| **Swarm commands** (`command_registry.hpp`, `handleSwarm*`) | CLI/menu: `!swarm`, `!swarm_*` (leader, worker, nodes, build, cache, etc.). Can surface “inference swarm” status later. |

---

## Proposed direction

1. **Inference shard coordinator**
   - Enumerate D3D12/DML adapters (e.g. 2 GPUs).
   - One “inference shard” per GPU: DML session + subset of model layers (or one full copy per GPU with batch split).
   - Coordinator owns: which layers live on which shard, and the forward pass order (shard 0 → shard 1 → … for layer-by-layer; or batch split).
   - First milestone: **design + stub** that enumerates 2 adapters and reports “ready for layer split” (no actual layer split yet).

2. **Layer partitioning**
   - For transformer blocks: assign contiguous blocks to shards (e.g. layers 0–19 on GPU0, 20–39 on GPU1). Hidden state and KV move between devices (DML copy or readback/upload).
   - Reuse existing GGUF-DML layer metadata (`layerIndex`, `GGUFModelConfig::numLayers`) and streaming/eviction ideas where useful.

3. **Beacon / telemetry**
   - Reuse existing beacon for “swarm” telemetry: e.g. “inference shard 0 busy”, “shard 1 done”, so agent loop or UI can show status.

4. **Validation**
   - Script or manual: confirm 2 (or N) adapters visible (D3D12/DML enumeration).
   - Later: run a tiny model split across 2 shards and compare output to single-GPU (sanity check).

---

## Definition of done (first milestone)

| Criterion | Target |
|-----------|--------|
| Design documented | Yes — this doc. |
| Adapter enumeration | **Done** — `EnumerateInferenceAdapters()` in `src/core/inference_shard_coordinator.cpp` via DXGI; returns count and `InferenceAdapterInfo`. |
| Integration point | Clear place to plug “InferenceShardCoordinator” (e.g. next to `MultiEngineSystem` or inside `core/`). |
| No regression | Existing single-GPU and CPU paths unchanged. |

---

## Files touched (first milestone)

| Area | Files |
|------|--------|
| Adapter enumeration | `include/inference_shard_coordinator.h`, `src/core/inference_shard_coordinator.cpp` (DXGI). |
| Build | `CMakeLists.txt` — added `inference_shard_coordinator.cpp`. |
| Validation | `scripts/test_x5_adapters.ps1`. |

## Files to touch (later)

| Area | Files |
|------|--------|
| Layer split (later) | `core/gguf_dml_bridge.h` (layer range per shard), `dml_inference_engine.cpp` (multi-session forward). |
| Swarm status | Optional: `Win32IDE` or command handler to show “Inference swarm: 2 GPUs” from coordinator. |
| Validation | New: `scripts/test_x5_adapters.ps1` or C++ test that calls enumeration and asserts count. |

---

## Next steps (after first milestone)

- Implement layer-range assignment and hidden-state handoff between shards.
- Wire coordinator into existing model load path (e.g. “load 120B” → create 2 shards, split layers).
- Add Beacon events for shard busy/done and surface in UI or `!swarm stats`.

You're on a solid foundation; X+5 is scoped and ready for implementation.
