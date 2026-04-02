# Expert heatmap — Win32 UI v1 outline (custom grid)

Companion to `EXPERT_HEATMAP_WIN32_IDE_SPEC.md`. This file is the minimal **rendering** contract for native Win32 (no React in-tree).

## Recommended control: custom-drawn grid

For thousands of cells and overlays (`pfInflight`, `hold`, dense column), a **virtualized custom-drawn grid** (GDI, GDI+, or Direct2D) is preferable to a standard `ListView`:

- Fixed cell size (e.g. 10×10 px) or zoomable; only paint visible rect.
- O(visible) paint cost; scroll via `SetScrollInfo` / thumb position.
- Smooth “neural fire” updates at 10–15 Hz without owning tens of thousands of HWNDs.

`ListView` in owner-data mode is acceptable for **operator tables** (sort by `touchSeq`, filter rows) but awkward for a true layer×expert matrix.

## Data path

1. Timer (1 s prod / 100–200 ms staging) fires on a **non-inference** thread or low-priority timer.
2. `CaptureSwarmExpertHeatmap(params, snap)` with conservative defaults (`planRowStride=8`, `maxCells=4000`, `expertsOnly=true`).
3. If `snap.planGeneration != swarmPlanGeneration()` → **discard**; do not index `cells[]`.
4. **Aggregate** sampled rows into a map keyed by `(modelIndex, layerStart, expertIndex)`:
   - `hold = max(hold)` across spans
   - `pfInflight = pfInflight_0 || pfInflight_1 || …`
   - `resident = resident_0 || resident_1 || …`
   - `bytes = max(bytes)` among resident spans (or sum—pick one and document)
   - `touchSeq = max(touchSeq)` among resident spans (LRU “hottest”)
5. Build JSON only if crossing a process boundary; otherwise use `ExpertHeatmapSnapshot` in memory.

## CSS-equivalent color tokens (map to Rose Pine)

| Token | Role | Suggested RGB |
|-------|------|-----------------|
| `--heatmap-bg` | Grid background | `#191724` |
| `--heatmap-cell` | Empty / cold | `#26233a` |
| `--heatmap-resident` | Resident fill | `#31748f` |
| `--heatmap-pf` | `pfInflight` border | `#f6c177` |
| `--heatmap-hold` | `hold>0` accent | `#eb6f92` |
| `--heatmap-dense` | `expert==4294967295` | `#c4a7e7` |

## Tooltip layout (single block, copy-friendly)

```
L3–4 expert=5 row=20 span=0
resident=1 hold=2 bytes=524288 touchSeq=9005 pfInflight=0
planGeneration=7 snapshotId=42
```

(`name` often empty in engine snapshots; omit line if empty.)

## Congestion strip (HUD)

- Show **pin_block_ms** ≈ `pinBlockLatencyMsTotal / max(1, pinBlockAttempts)`.
- Warn if `evictionRejectedInUse > 0` or `evictStarvation > 0` (“Memory pressure”).
- Optional: ring buffer of last N `inUseSliceCount` values → sparkline per session.

## Polling backoff

If `evictionRejectedInUse` or `pin_block_ms` spikes vs a rolling baseline, **double** poll interval up to 8× until metrics normalize (exponential backoff).

## Fixture for UI dev

- `schemas/expert_heatmap_snapshot.sample.json` — small synthetic payload for layout and parser tests.
