# Production Symbol Wiring - Batch 12

Batch 12 hardens Win32IDE production source wiring by removing stale stub entries, tightening fallback-strip behavior, and making SSOT stub-lane ownership guard opt-in instead of implicit.

## Batch 12 fixes applied (15)

1. Removed `src/core/missing_handler_stubs.cpp` from the Win32IDE source aggregation lane to prevent implicit history-indirected runtime wiring in production.
2. Removed `src/win32app/digestion_engine_stub.cpp` from Win32IDE source aggregation.
3. Removed `src/win32app/Win32IDE_logMessage_stub.cpp` from Win32IDE source aggregation.
4. Removed stale `src/win32app/digestion_engine_stub.cpp` cleanup entry from Win32IDE stub-prune list (no-op cleanup removed).
5. Removed stale `src/win32app/Win32IDE_logMessage_stub.cpp` cleanup entry from Win32IDE stub-prune list (no-op cleanup removed).
6. Removed stale comment claiming `missing_handler_stubs.cpp` is required in the Win32IDE real lane.
7. Extended `RAWRXD_PRODUCTION_STRIP_STUB_SOURCES` to strip `src/core/subsystem_mode_fallbacks.cpp`.
8. Extended `RAWRXD_PRODUCTION_STRIP_STUB_SOURCES` to strip `src/core/model_loader_fallbacks.cpp`.
9. Extended `RAWRXD_PRODUCTION_STRIP_STUB_SOURCES` to strip `src/core/rawrxd_native_log_fallback.cpp`.
10. Extended `RAWRXD_PRODUCTION_STRIP_STUB_SOURCES` to strip `src/llm_adapter/ggufrunner_link_fallbacks.cpp`.
11. Updated the Win32IDE production-strip status message to reflect monaco + ssot + fallback shim stripping.
12. Added `RAWRXD_HAS_SSOT_STUB_LANE_GUARD` state variable to track whether the SSOT stub-lane guard target is actually created.
13. Gated `ssot_stub_lane_objs` / `ssot_stub_lane_guard` creation on `RAWRXD_ENABLE_MISSING_HANDLER_STUBS` (opt-in), instead of only file presence.
14. Updated SSOT guard disabled message to explicitly direct enabling via `RAWRXD_ENABLE_MISSING_HANDLER_STUBS=ON`.
15. Reworked `RAWRXD_ENABLE_HEAVY_GATES` self-test target wiring to invoke `ssot_stub_lane_guard` only when that target exists.

## Additional ABI/runtime fixes in this batch

- `src/core/model_loader_fallbacks.cpp`: corrected fallback export ABI for `Enterprise_DevUnlock` from `bool` to `int64_t` (matches production bridge/header expectations).
- `src/core/rawrxd_native_log_fallback.cpp`: replaced no-op `RawrXD_Native_Log` fallback with actual sink behavior (`stderr` + `OutputDebugStringA` on Win32) to avoid silent dissolved logging.

## Why this batch matters

This batch reduces hidden fallback activation and stale stub ownership in production lanes while preserving explicit opt-in paths for compatibility. It lowers dissolved-symbol risk by making fallback lanes visible and intentional.

## Remaining for next batches

- Eliminate history-indirected implementation ownership in `src/core/missing_handler_stubs.cpp`.
- Continue replacing fallback-only exports with real providers or explicit non-production options.
- Add a strict production preset that turns on `RAWRXD_PRODUCTION_STRIP_STUB_SOURCES` and validates no fallback translation units remain linked.
