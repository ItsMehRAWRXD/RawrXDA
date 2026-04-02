# Expert heatmap — Win32IDE payload and UI v1

This document matches the output of `RawrXD::Swarm::expertHeatmapSnapshotToJson()` and `RawrXDInference::CaptureSwarmExpertHeatmap()`. Canonical machine-readable schema: `schemas/expert_heatmap_snapshot.schema.json`. **Sample payload:** `schemas/expert_heatmap_snapshot.sample.json`. **Win32 rendering outline:** `EXPERT_HEATMAP_WIN32_UI_OUTLINE.md`.

## Polling and staleness

- **Production:** poll every **1 s**. **Staging:** 100–200 ms.
- **`planGeneration` is the only hard staleness guard.** Compare the snapshot’s value to live `swarmPlanGeneration()` immediately before paint; if they differ, **discard the frame** (avoids mapping a new topology onto an old grid).
- Use `snapshotId` only for logging and triage (“which capture was on screen when the stall happened?”).
- **Backoff:** if `evictionRejectedInUse` or derived `pin_block_latency_ms` spikes vs a rolling baseline, increase the poll interval exponentially (up to ~8×) until metrics normalize.

## Capture parameters (C++)

| Parameter | Conservative production default | Staging |
|-----------|----------------------------------|---------|
| `planRowStride` | **8** (large MoE plans; keeps snapshot time down) | 1–2 |
| `maxCells` | **4000** | 0 (unlimited, still capped at 1M in engine) |
| `expertsOnly` | `true` for MoE grid | optional `false` to include dense/static rows |
| `modelIndex` | `0` or `kExpertHeatmapAllModels` | same |

Older guidance (stride 4–16, 2k–8k cells) remains valid for mid-size models; tune per deployment.

## Lock hold time (backend)

- `captureExpertHeatmapSnapshot` keeps **`m_schedMutex`** only for: counter copies, `inUseSliceCount` scan, and a walk of sampled plan rows with scalar copies and resident / prefetch lookups.
- **Per-row `debugName` is not copied under the mutex** (avoids string allocations and large copies on 200k+ row plans). JSON `name` is usually `""`. Off hot paths, the IDE may call `tryCopySubmittedPlanRow` for selected rows if labels are required.
- **JSON serialization** runs **after** the snapshot struct is filled (`expertHeatmapSnapshotToJson`), outside the scheduler lock.

## Aggregation (UI)

When multiple sampled plan rows map to the same logical **(model, layerStart, expert)**:

- `hold` → **max** across rows  
- `pfInflight` → **any** true  
- `resident` → **any** true  
- `touchSeq` → **max** among resident rows (strongest LRU signal)  
- `bytes` → **max** among resident rows (or sum—pick one product-wide and document)

## JSON top-level fields

| Field | Type | UI use |
|-------|------|--------|
| `snapshotId` | uint64 | Log correlation |
| `planGeneration` | uint64 | Staleness guard vs live `swarmPlanGeneration()` |
| `sampleTsMs` | uint64 | Relative time axis (steady clock ms) |
| `stats` | object | HUD: congestion (see below) |
| `cells` | array | Layer × expert grid |

## `stats` (global, same lock as cells)

Surface next to the grid (toolbar or status strip):

- `inUseSliceCount` — rows with active compute hold.
- `evictionRejectedInUse`, `evictStarvation` — admission pain.
- `pinBlockAttempts`, `pinBlockTimeouts`, `pinBlockLatencyMsTotal` — derive mean latency as `total / max(1, attempts)` for a coarse **pin_block_latency_ms** hint (not per-cell).
- `prefetchEnqueueSkippedDuplicate` — correlates with “duplicate prefetch” noise.
- `jitPinNonResident` — cold JIT pins.

Per-cell **`pinLatencyMs`** / **`pfEnqDup`** are not in v1 JSON; use global `stats` plus cell `pfInflight` / `hold` for triage.

## `cells[]` fields

| JSON key | Meaning | Grid hint |
|----------|---------|-----------|
| `row` | Plan row index | Tooltip |
| `model` | Model index | Filter |
| `layer` | `layerStart` | **Row** in layer × expert matrix |
| `layerEnd` | Exclusive end | Tooltip |
| `expert` | Expert ordinal; **4294967295** = dense/static (`0xFFFFFFFF`) | **Column** (map 4294967295 to a dedicated “dense” column) |
| `span` | `planSpanOrdinal` | Tooltip (multi-span groups) |
| `resident` | In working set | Base fill |
| `pfInflight` | In `m_prefetchInFlightIds` | Anticipation overlay (e.g. yellow border) |
| `hold` | `holdCount` | **Active compute** (e.g. red border or icon) |
| `bytes` | Resident bytes | Tooltip |
| `touchSeq` | Scheduler touch sequence | LRU “heat” (normalize min–max in view) |
| `name` | Escaped `debugName` | Often **empty** in engine snapshots; optional IDE fill via `tryCopySubmittedPlanRow` |

## Grid layout (v1)

- **Axes:** `layer` (Y, ascending), `expert` (X). Aggregate multi-span / multi-row keys into one cell (see **Aggregation** above).
- **Default sort:** columns sorted by expert index; dense column last.

## Color scale (suggested)

Theme-agnostic defaults (tune to Rose Pine in IDE):

| State | Suggested |
|-------|-----------|
| Not resident, not inflight | Muted background |
| `resident` && !`hold` | Cool / pine |
| `pfInflight` | Gold border or tint |
| `hold` > 0 | Love / red accent |
| Dense (`expert == 4294967295`) | Iris or distinct hatch |

## Hover / tooltip payload

Plain text or read-only panel:

- `layer`–`layerEnd`, `expert`, `row`, `span`, `name`
- `resident`, `hold`, `bytes`, `touchSeq`, `pfInflight`
- Append one line: `planGeneration=<n> snapshotId=<id>`

## Filters (operator mode)

- **Starvation:** `hold > 0 && !resident` (should be rare; sustained → I/O / `pin_block` stall).
- **Cold miss:** `!resident && !pfInflight` for layers near the compute cursor (heuristic using last known layer from inference callback).
- **Backbone only:** `expert == 4294967295`.
- Default UI: Starvation and Cold Miss as **collapsed** filter panels until expanded.

## Acceptance hints

- **Backend:** target snapshot **p95** wall time under **5 ms** on production hardware at conservative defaults (measure in staging; stress harness enforces a max per-iteration cap).
- **Frontend:** ≤ **8k** aggregated cells without jank; discard frames on `planGeneration` mismatch.
- **Safety:** production sampling defaults should not measurably increase pin latency or mid-forward evictions (A/B vs heatmap off).

## CI / harness

- Target: `test_swarm_expert_heatmap_stress` (see `tests/test_swarm_expert_heatmap_stress.cpp`).
- Usage: `test_swarm_expert_heatmap_stress [capture_iterations] [max_capture_us]`
  - Default-like CI: `test_swarm_expert_heatmap_stress 4000 50000`
  - Validates: monotonic `planGeneration` from the capturer thread, JSON shape, row indices when `planGeneration` still matches immediately after capture, capture latency max vs limit, and **delta** in `evictStarvation` / `evictionRejectedInUse` over the run (telemetry vs admission stress).

## Related code

- `src/core/swarm_scheduler.hpp` — `ExpertHeatmapCaptureParams`, `ExpertHeatmapSnapshot`, `expertHeatmapSnapshotToJson`.
- `src/rawrxd_inference.h` — `CaptureSwarmExpertHeatmap`, `swarmPlanGeneration()`.
- `schemas/expert_heatmap_snapshot.schema.json` — JSON Schema (draft-07).
