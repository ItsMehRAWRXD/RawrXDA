# Production Symbol Wiring - Batch 25

Batch 25 introduces strict production-profile guardrails for toolchain/configuration coherence and propagates an explicit strict-profile compile flag across all primary production targets.

## Batch 25 fixes applied (15)

1. Added strict guard: `RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON` now requires `WIN32`.
2. Added strict guard: `RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON` now requires `MSVC`.
3. Added strict guard: `RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON` now requires `RAWR_HAS_MASM=TRUE`.
4. Added strict single-config guard: when no multi-config generator is used, `CMAKE_BUILD_TYPE` is now required in strict mode.
5. Added strict single-config guard: in strict mode, `CMAKE_BUILD_TYPE` must be `Release` (non-Release build types now fail).
6. Added `RAWRXD_STRICT_PRODUCTION_PROFILE` compile definition to `RawrEngine` (1/0 via generator expression).
7. Added `RAWRXD_STRICT_PRODUCTION_PROFILE` compile definition to `RawrXD_Gold`.
8. Added `RAWRXD_STRICT_PRODUCTION_PROFILE` compile definition to `RawrXD-InferenceEngine`.
9. Added `RAWRXD_STRICT_PRODUCTION_PROFILE` compile definition to `RawrXD-Win32IDE`.
10. Added strict guard: `RAWR_ASAN=ON` now fails when `RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON`.
11. Added strict guard: `RAWRXD_ENABLE_HEAVY_GATES=OFF` now fails when strict production strip is ON.
12. Added strict status telemetry message during configure to signal strict production profile activation.
13. Added strict guard: Python3 interpreter must be present when strict production strip is ON.
14. Added strict `self_test_gate` dependency on `validate_registry` once Python targets are available.
15. Preserved strict `self_test_gate` dependency on `validate_registry_strict` and made strict registry dependency wiring deterministic after target creation.

## Why this batch matters

These changes harden strict production mode from a “best effort filter” into a coherent profile with explicit toolchain, build-type, and CI gate requirements, reducing the chance of false confidence from misconfigured strict runs.

## Remaining for next batches

- Replace the history-indirected `src/core/missing_handler_stubs.cpp` wrapper with canonical in-tree implementation ownership.
- Continue narrowing non-strict compatibility/fallback lanes behind explicit options.
- Add deeper compiled-symbol closure verification for RawrEngine/Gold/Inference targets.
