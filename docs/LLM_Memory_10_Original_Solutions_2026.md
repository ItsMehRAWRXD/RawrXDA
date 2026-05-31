# LLM Memory: 10 Novel Management Concepts + RawrXD Build Mapping

This file captures the 10 concepts exactly as requested and maps each to concrete implementation shape.

## 1) Temporal Relevance Decay Memory (TRDM)

### Core idea
Memory fades continuously instead of being evicted discretely.

### Implementation shape
- Per-token decay curve driven by attention reuse and recency.
- Precision ladder per token: FP16 -> INT8 -> packed bitmask.
- Soft-fade policy: no hard eviction unless under critical pressure.

---

## 2) Execution-Path Memory Folding (EPMF)

### Core idea
Merge identical reasoning paths across tokens/requests.

### Implementation shape
- Detect repeated attention trajectory signatures.
- Fold matching subpaths to shared KV execution subtree.
- Maintain reference-counted folded path nodes.

---

## 3) Predictive KV Prefetch Graph (PKPG)

### Core idea
Prefetch KV before access using predictor graph.

### Implementation shape
- Build lightweight predictor over next-token attention targets.
- Schedule asynchronous GPU prefetch by confidence bucket.
- Keep cancellation lane for low-confidence mispredictions.

---

## 4) Cross-Session Memory Deduplication (CSMD)

### Core idea
Semantic dedup across user sessions.

### Implementation shape
- Semantic + exact hash index of KV spans.
- Tenant-local by default, global sharing only opt-in.
- Canonical KV entry with refcount/TTL.

### Created module
- `src/memory/cross_session_memory_dedup.h`
- `src/memory/cross_session_memory_dedup.cpp`

---

## 5) Attention Heat-Zone Partitioning (AHZP)

### Core idea
Dynamic hot/warm/cold zones from real-time attention temperature.

### Implementation shape
- Hot: full precision in GPU residency.
- Warm: compressed in GPU/VRAM spill.
- Cold: host RAM or disk-backed tier.
- Pressure-aware threshold morphing with hysteresis.

### Created module
- `src/memory/attention_heat_zone_partitioning.h`
- `src/memory/attention_heat_zone_partitioning.cpp`

---

## 6) Neural Memory Indexing (NMI)

### Core idea
Approximate KV retrieval via embedding index instead of exact lookup.

### Implementation shape
- Store KV block descriptors in compact vector index.
- Retrieve top-k approximate candidates by semantic proximity.
- Optional exact refinement for quality-sensitive layers.

---

## 7) Delta-State KV Encoding (DSKE)

### Core idea
Store KV deltas between adjacent decode states.

### Implementation shape
- Persist base snapshot + compressed delta chain.
- Adaptive checkpointing to cap delta-chain length.
- Fast reconstruct path for random seek.

---

## 8) Speculative Memory Branching (SMB)

### Core idea
Only allocate memory for divergent speculative paths.

### Implementation shape
- Branches share base KV pages.
- Copy-on-write only on touched pages.
- Winner-commit merges one branch, losers retired O(1).

### Created module
- `src/memory/speculative_memory_branching.h`
- `src/memory/speculative_memory_branching.cpp`

---

## 9) Context Topology Compression (CTC)

### Core idea
Represent context as graph topology instead of linear sequence.

### Implementation shape
- Collapse repeated structural regions into reusable subgraphs.
- Rehydrate sequence view lazily for downstream consumers.
- Track graph-node hotness for selective expansion.

---

## 10) Hardware-Adaptive Memory Morphing (HAMM)

### Core idea
Runtime mode switching from telemetry feedback.

### Implementation shape
- Inputs: PCIe congestion, VRAM pressure, cache miss rate.
- Modes: dense, paged, compressed, hybrid.
- Controller with hysteresis/cooldown to prevent oscillation.

---

## Why these matter

They shift memory systems from static/reactive/token-linear to predictive/shared/structure-aware/adaptive.

## Strongest RawrXD direction (implemented baseline)

1. SMB — branch copy-on-write KV sharing.
2. CSMD — cross-session dedup under tenancy policy.
3. AHZP — heat-zoned tiering with pressure-aware thresholds.

## Integration hooks for RawrXD inference controller

- Call AHZP `upsert()` every decode step with attention temperature.
- Route speculative branch events through SMB `beginEpoch/forkBranch/touchPage/commitWinner`.
- Before creating new session KV spans, query CSMD `lookup()` then `bindOrInsert()`.

## Files created in this pass

- `src/memory/speculative_memory_branching.h`
- `src/memory/speculative_memory_branching.cpp`
- `src/memory/cross_session_memory_dedup.h`
- `src/memory/cross_session_memory_dedup.cpp`
- `src/memory/attention_heat_zone_partitioning.h`
- `src/memory/attention_heat_zone_partitioning.cpp`

## 10) Hardware-Telemetry Memory Morph Controller (HTMMC)

### Target
Runtime adaptation to PCIe congestion, NVLink imbalance, and HBM pressure.

### Mechanism
A controller switches memory strategy modes online:
- Mode A: dense high-precision
- Mode B: compressed tiered
- Mode C: offload-heavy with prefetch
- Mode D: degraded quality high-throughput

### Data Structures
- `Telemetry { hbm_used, pcie_rx, pcie_tx, nvlink_util, stall_cycles }`
- `ModePolicy { thresholds, actions, cooldown }`
- `ActionQueue { compress, migrate, prefetch, prune }`

### Control Loop
1. Sample telemetry every T ms.
2. Compute pressure vector.
3. Select mode with hysteresis.
4. Enqueue bounded memory actions.

### Build Stub
- Module: `memory_morph_controller.{h,cpp}`
- APIs:
  - `Mode select_mode(const Telemetry& t);`
  - `void apply_mode(Mode m);`
  - `void tick();`

---

## Integration Notes (Minimal)

- Start with #2 + #10 + #1 for strongest immediate gain.
- Add #5 when multi-tenant traffic is high and tenancy policy is strict.
- Add #8 only if you operate reranker+generator pipelines at scale.

## Evaluation Matrix

For each module, track:
- VRAM peak reduction
- p50/p99 token latency delta
- Throughput gain at fixed quality
- Quality drift (perplexity/task success)
- Recovery correctness (fallback path pass rate)

## Suggested Next Files

- `src/memory/entropy_kv_lattice.h/.cpp`
- `src/memory/spec_kv_cow_fabric.h/.cpp`
- `src/memory/memory_morph_controller.h/.cpp`
