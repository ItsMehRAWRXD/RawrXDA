# Production Symbol Wiring - Batch 17

Batch 17 strengthens strict-production option compatibility, improves cross-target strict signal propagation, and tightens strict build gating so core production executables are validated together.

## Batch 17 fixes applied (15)

1. Added strict incompatibility guard: `RAWRXD_BUILD_CLI=ON` now fails when `RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON` (CLI target includes compatibility shims).
2. Added strict incompatibility guard: `RAWRXD_INCLUDE_STRESS_AND_REPLAY_SOURCES=ON` now fails when strict production strip is ON.
3. Tightened SSOT strict compatibility: strict mode now requires `RAWR_SSOT_PROVIDER=AUTO`.
4. Added strict post-provider partition filter on `RAWR_ENGINE_SOURCES` to exclude `*_link_shims.cpp`.
5. Added strict post-provider partition filter on `GOLD_UNDERSCORE_SOURCES` to exclude `*_link_shims.cpp`.
6. Added strict filter on `INFERENCE_ENGINE_SOURCES` to exclude `*_link_shims.cpp` generically (beyond the standalone shim file name).
7. Added strict mode target property override so `RawrXD-InferenceEngine` is no longer `EXCLUDE_FROM_ALL`.
8. Added strict mode target property override so `RawrXD-InferenceEngine` is no longer excluded from default build.
9. Added compile definition propagation to `RawrEngine`: `RAWRXD_PRODUCTION_STRIP_STUB_SOURCES` now reaches compile-time code.
10. Added compile definition propagation to `RawrXD_Gold`: `RAWRXD_PRODUCTION_STRIP_STUB_SOURCES` now reaches compile-time code.
11. Added compile definition propagation to `RawrXD-InferenceEngine`: `RAWRXD_PRODUCTION_STRIP_STUB_SOURCES` now reaches compile-time code.
12. Added compile definition propagation to `RawrXD-Win32IDE`: `RAWRXD_PRODUCTION_STRIP_STUB_SOURCES` now reaches compile-time code.
13. Added strict incompatibility guard: `RAWRXD_STRICT_AGENTIC_REALITY=OFF` now fails when strict production strip is ON.
14. Extended `self_test_gate` strict behavior to depend on `RawrEngine`.
15. Extended `self_test_gate` strict behavior to depend on `RawrXD_Gold` and `RawrXD-InferenceEngine`, ensuring strict gate coverage spans all primary production executables.

## Why this batch matters

This batch closes configuration-level loopholes where strict production mode could still be combined with non-production lanes, and ensures strict gate execution covers all primary production binaries rather than only Win32IDE.

## Remaining for next batches

- Replace `src/core/missing_handler_stubs.cpp` history-indirected include shim with canonical implementation ownership in `src/core`.
- Continue reducing default fallback source reliance in non-strict lanes through explicit opt-in compatibility options.
- Add deeper compiled-symbol closure checks for RawrEngine/Gold/Inference analogous to Win32IDE policy depth.
