# StreamingGGUFLoader — Design Specification

## Goals
- Enable large GGUF models (up to 10–20 GB) to run on machines with limited RAM (e.g., 500 MB) by streaming tensor zones on-demand.
- Support concurrent agents (Feature, Security, Performance, Architect) with isolated memory budgets and prefetch strategies.
- Provide deterministic, resumable inference across streamed zones with minimal latency.

## Key Concepts
- Zone-Based Streaming: Partition model tensors into zones (Embedding, Attention Blocks/Layers, MLP, Output/LM Head). Only load active zones during current inference step.
- Index & Manifest: Build an index of tensor metadata (offsets, sizes, dtypes) and a manifest of zone boundaries. Persist cache for re-use across sessions.
- Memory Budgeting: Per-agent configurable memory caps with LRU eviction across zones.
- Prefetching: Anticipate next-likely zones; keep small ring buffer for attention/MLP blocks.

## API
- init(manifestPath, modelPath, options): Parse headers and build zone index; validate compatibility.
- setMemoryBudget(bytes): Apply per-instance budget; configure eviction policy.
- loadZone(zoneId): Load zone tensors into RAM; returns handle with lifetime tracking.
- releaseZone(zoneId): Evict zone tensors respecting pinning rules.
- inferStep(inputTokens, context):
  - Evaluates forward pass by sequencing minimal zones.
  - Emits partial logits/chunks as soon as ready.
  - Returns step result (tokens/logits) and timing metrics.
- getStats(): Current memory usage, cache hits/misses, active zones.
- on(event, callback): Events: ZoneLoaded, ZoneEvicted, PrefetchStart/End, BudgetExceeded.

## Threading Model
- Dedicated IO thread for zone reads; compute thread(s) for inference; signal/slot bridge to UI.
- Use bounded queues to ensure back-pressure.

## Integration Points
- StreamerClient: Optional loader-aware mode bypassing remote streamer when local GGUF files are present.
- QShell: Commands: `Set-QConfig -Loader Local|Remote`, `Get-LoaderStats`, `Invoke-QAgent -UseLoader Local`.
- MainWindow Agent Panel: Show zone activity and loader stats during inference.

## Error Handling
- Manifest/Index corruption: Fail init with specific code; suggest reindex.
- BudgetExceeded: Throttle prefetch and evict non-pinned zones; emit event.
- IO Timeout: Retry with exponential backoff; allow user cancel.

## File Layout (Proposed)
- src/qtapp/gguf/StreamingGGUFLoader.h/.cpp
- src/qtapp/gguf/GGUFManifest.h/.cpp (indexing)
- src/qtapp/gguf/GGUFCache.h/.cpp (LRU cache)

## Minimal Implementation Phase 1
- Parse GGUF header & tensors; produce manifest.
- Stubbed inferStep simulating zone loads; emit synthetic metrics.
- UI wiring to display stats in Agent Panel.

## Phase 2
- Real zone loading with memory caps; partial compute via pluggable kernel (CPU baseline).
- Prefetch heuristics and eviction tuning.

## Phase 3
- Kernel optimization: SIMD, threaded blocks; optional GPU path.
- Persistence of cache and warm-start across sessions.
