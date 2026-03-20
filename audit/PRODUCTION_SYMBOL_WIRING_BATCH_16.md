# Production Symbol Wiring - Batch 16

Batch 16 tightens strict-production compatibility gates, expands non-MSVC fallback pruning, and closes additional shim/fallback detection gaps (`*_link_shims.cpp`, `*_fallback_system.cpp`, and related variants).

## Batch 16 fixes applied (15)

1. Added strict guard: `RAWRXD_ENABLE_MISSING_HANDLER_STUBS=ON` now hard-fails when `RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON`.
2. Added strict guard: `RAWR_SSOT_PROVIDER=EXT` now hard-fails under strict production strip mode.
3. Added strict guard: `RAWR_SSOT_PROVIDER=STUBS` now hard-fails under strict production strip mode.
4. Added strict guard: `RAWRXD_ALLOW_AGENTIC_STUB_FALLBACK=ON` now hard-fails when strict production strip is ON.
5. Added `src/inference/gpu_dispatch_gate_win32ide_fallback.cpp` to non-MSVC Win32IDE exclusion list.
6. Added `src/inference/gguf_d3d12_bridge_link_fallback.cpp` to non-MSVC Win32IDE exclusion list.
7. Added `src/core/win32ide_asm_fallback.cpp` to non-MSVC Win32IDE exclusion list.
8. Added `src/win32app/collab_cursor_fallbacks.cpp` to non-MSVC Win32IDE exclusion list.
9. Added `src/core/cot_fallback_system.cpp` to non-MSVC Win32IDE exclusion list.
10. Added `src/core/cot_fallback_system.cpp` to strict production strip removal list for Win32IDE.
11. Expanded agentic real-lane filtering to exclude `*_link_shims.cpp` units.
12. Expanded agentic real-lane filtering from only `*_fallback.cpp`/`*_fallbacks.cpp` to `*_fallback*.cpp` variants (e.g., `*_fallback_system.cpp`).
13. Expanded strict pre-check forbidden patterns to catch `*_link_shims.cpp` and `*_fallback*.cpp` variants.
14. Expanded `EnforceNoStubs` and broad Win32IDE source filters to detect/strip `*_link_shims.cpp` and `*_fallback*.cpp` classes in addition to existing stub/shim naming.
15. Expanded strict post-check forbidden patterns to include both `*_link_shims.cpp` and `*_fallback*.cpp` classes.

## Why this batch matters

This closes remaining strict-lane loopholes where fallback or shim files with variant naming could bypass policy checks, and prevents contradictory “strict production + fallback-enabled” combinations from silently dissolving into compatibility lanes.

## Remaining for next batches

- Replace `src/core/missing_handler_stubs.cpp` history-include shim with canonical source ownership under `src/core`.
- Continue reducing fallback-bearing runtime units in non-strict default lanes through explicit non-production options.
- Add compiled-symbol verification for RawrEngine/Gold/Inference analogous to Win32IDE enforcement depth.
