# Production Symbol Wiring - Batch 32

Batch 32 closes the history-indirected SSOT ownership gap by replacing `missing_handler_stubs.cpp` with canonical in-tree code and rewiring stub-provider lanes to consume it directly.

## Batch 32 fixes applied (15)

1. Replaced `src/core/missing_handler_stubs.cpp` one-line history redirect include with canonical in-tree implementation.
2. Added direct Win32 fallback dispatch implementation in `missing_handler_stubs.cpp` (`PostMessageA`-based routing nudge).
3. Added deterministic output-path handling in `missing_handler_stubs.cpp` (`ctx.output` fallback tracing).
4. Materialized the full local `RAWR_MISSING_HANDLER_LIST(...)` macro expansion in `missing_handler_stubs.cpp`.
5. Added local handler emission via `RAWR_MISSING_HANDLER_LIST(DEFINE_MISSING_HANDLER)` in `missing_handler_stubs.cpp`.
6. Added local static cardinality guard (`static_assert(... == 116)`) in `missing_handler_stubs.cpp`.
7. Removed strict-only inline history-wrapper check block from the early `RAWRXD_PRODUCTION_STRIP_STUB_SOURCES` section (de-duplicated ownership validation flow).
8. Added centralized ownership validation for `src/core/missing_handler_stubs.cpp` near SSOT-provider normalization.
9. Upgraded centralized ownership validation to `FATAL_ERROR` for strict/stub-provider lanes when history-indirected wrappers are detected.
10. Added non-fatal `WARNING` path for legacy non-strict/non-stub-provider lanes to make drift visible without immediate build break.
11. Rewired Win32IDE stub lane to append `src/core/missing_handler_stubs.cpp` (canonical file) instead of `src/core/ssot_missing_handlers_provider.cpp`.
12. Updated Win32IDE stub-lane status message to reflect canonical source ownership.
13. Added explicit `list(REMOVE_ITEM WIN32IDE_SOURCES src/core/ssot_missing_handlers_provider.cpp)` in stub lane to avoid mixed-provider duplication.
14. Rewired `ssot_stub_lane_objs` object-library source list to only compile `missing_handler_stubs.cpp` (removing parallel provider duplication risk).
15. Added hard guard: `RAWRXD_ENABLE_MISSING_HANDLER_STUBS=ON` now requires `src/core/missing_handler_stubs.cpp` to exist.

## Why this batch matters

This batch removes the primary history-indirected SSOT fallback owner from active wiring paths and ensures stub-provider lanes resolve to code owned directly in `src/core`, reducing drift and dissolved-symbol risk.

## Remaining for next batches

- Continue collapsing non-strict compatibility lanes into explicit presets/options.
- Add strict map-consumer validation targets for RawrEngine/Gold/Inference symbol closure.
- Reduce duplicate fallback-provider surface between `missing_handler_stubs.cpp` and `ssot_missing_handlers_provider.cpp`.
