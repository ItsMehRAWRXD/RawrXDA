# Production Symbol Wiring - Batch 18

Batch 18 harmonizes strict fallback/stub/shim filtering across all primary target lanes and closes additional loopholes where plural `*_shims.cpp` naming could bypass pre/post policy gates.

## Batch 18 fixes applied (15)

1. Added strict guard that fails configure when `RAWRXD_EMERGENCY_STUB_LINK` is set under `RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON`.
2. Added generic strict filter on shared `SOURCES` for `*_fallback*.cpp`.
3. Added generic strict filter on shared `SOURCES` for `*_link_shims.cpp`.
4. Added generic strict filter on shared `SOURCES` for `*_shims.cpp`.
5. Extended strict `RAWR_ENGINE_SOURCES` filtering to exclude `*_shims.cpp`.
6. Extended strict `RAWR_ENGINE_SOURCES` filtering to exclude `*_fallback*.cpp`.
7. Extended strict `GOLD_UNDERSCORE_SOURCES` filtering to exclude `*_shims.cpp`.
8. Extended strict `GOLD_UNDERSCORE_SOURCES` filtering to exclude `*_fallback*.cpp`.
9. Simplified inference strict shim handling by replacing single-file shim removal with generic strict pattern filtering.
10. Extended strict `INFERENCE_ENGINE_SOURCES` filtering to exclude `*_shims.cpp`.
11. Extended strict `INFERENCE_ENGINE_SOURCES` filtering to exclude `*_fallback*.cpp`.
12. Added generic strict fallback/shim filters inside Win32IDE strict strip block (`*_fallback*.cpp`, `*_link_shims.cpp`, `*_shims.cpp`).
13. Added `*_shims.cpp` exclusion to Win32IDE agentic real-lane pre-filter pass.
14. Harmonized Win32IDE strict pre-check, broad policy filter, and strict post-check regexes so plural `*_shims.cpp` files are always classified as forbidden stub/shim units.
15. Extended strict `self_test_gate` dependencies to include `validate_registry_strict` when that target exists.

## Why this batch matters

Before this batch, some `*_shims.cpp` naming variants could survive specific strict checks even though the no-stub policy intended to block them. Batch 18 makes strict behavior deterministic across shared source sets, per-target partitions, and Win32IDE gate passes.

## Remaining for next batches

- Replace `src/core/missing_handler_stubs.cpp` history-indirected ownership with canonical in-tree implementation.
- Continue converting fallback-heavy non-strict default lanes into explicit compatibility options.
- Add deeper compiled-symbol closure verification for RawrEngine/Gold/Inference targets.
