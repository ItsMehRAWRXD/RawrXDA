# Production Symbol Wiring - Batch 28

Batch 28 tightens strict-lane option coherence, removes stale no-op cleanup blocks, and enforces required MASM-backed production auxiliaries for strict production strip builds.

## Batch 28 fixes applied (15)

1. Replaced global `add_definitions(_CRT_SECURE_NO_WARNINGS/NOMINMAX/WIN32_LEAN_AND_MEAN)` with `add_compile_definitions(...)` for cleaner modern CMake behavior.
2. Added strict incompatibility guard: `RAWR_REPLACE_SSOT_HANDLERS=ON` now fails when `RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON`.
3. Added strict incompatibility guard: `RAWR_REPLACE_MASM_FALLBACKS=ON` now fails when strict production strip is ON.
4. Removed `src/core/win32_ide_link_stubs.cpp` from default Win32IDE source seed list.
5. Removed stale Win32IDE normalization `list(REMOVE_ITEM ...)` block containing `win32_ide_link_stubs.cpp` / `multi_file_search_stub.cpp` (no longer needed with current seeding).
6. Removed stale strict-strip `list(REMOVE_ITEM ...)` entry for `src/core/ssot_missing_handlers_provider.cpp` (not seeded in strict lane by default).
7. Removed stale strict-strip `list(REMOVE_ITEM ...)` entry for `src/core/win32_ide_link_stubs.cpp` (now appended only in non-strict compatibility lane).
8. Rewired `src/core/win32_ide_link_stubs.cpp` to append only in non-strict + MSVC fallback lane.
9. Added strict guard: require `RAWRXD_HAS_MW_KERNEL_FLAG=1` when `RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON`.
10. Added strict guard: require `RAWRXD_HAS_PROMPT_ENGINE_FLAG=1` when strict production strip is ON.
11. Added strict guard: require `TARGET RawrXD_MultiWindow_Kernel` in strict production strip mode.
12. Added strict guard: require `TARGET RawrXD_DynamicPromptEngine` in strict production strip mode.
13. Added strict `self_test_gate` dependency on `RawrXD_MultiWindow_Kernel` when target exists.
14. Added strict `self_test_gate` dependency on `RawrXD_DynamicPromptEngine` when target exists.
15. Kept strict fallback append policy deterministic by ensuring compatibility units (including `win32_ide_link_stubs.cpp`) are introduced only in the explicit non-strict lane.

## Why this batch matters

This batch eliminates stale cleanup logic that no longer matched current source ownership and upgrades strict profile checks from pattern-only filtering to required real-provider target presence.

## Remaining for next batches

- Replace the history-indirected `src/core/missing_handler_stubs.cpp` wrapper with canonical in-tree ownership.
- Continue reducing non-strict fallback lanes into explicit compatibility presets/options.
- Add deeper compiled-symbol closure checks for RawrEngine/Gold/Inference targets.
