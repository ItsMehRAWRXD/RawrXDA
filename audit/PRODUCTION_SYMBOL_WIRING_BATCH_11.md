# Production Symbol Wiring - Batch 11

Batch 11 removes non-production stress/replay harness source coupling from default production target graphs to reduce dissolved-symbol pressure and unnecessary shim dependencies.

## Batch 11 fixes applied (15+)

1. Added `RAWRXD_INCLUDE_STRESS_AND_REPLAY_SOURCES` CMake option (default `OFF`).
2. Added conditional removal of `src/core/masm_stress_harness.cpp` from global `SOURCES` when option is `OFF`.
3. Added conditional removal of `src/core/convergence_stress_harness.cpp` from global `SOURCES` when option is `OFF`.
4. Added conditional removal of `src/core/enterprise_stress_tests.cpp` from global `SOURCES` when option is `OFF`.
5. Added conditional removal of `src/test_harness/replay_fixture.cpp` from global `SOURCES` when option is `OFF`.
6. Added conditional removal of `src/test_harness/replay_oracle.cpp` from global `SOURCES` when option is `OFF`.
7. Added conditional removal of `src/test_harness/replay_mock_inference.cpp` from global `SOURCES` when option is `OFF`.
8. Added conditional removal of `src/test_harness/replay_harness.cpp` from global `SOURCES` when option is `OFF`.
9. Added conditional removal of `src/test_harness/replay_reporter.cpp` from global `SOURCES` when option is `OFF`.
10. Removed unconditional `src/test_harness` include path from `RawrEngine` include directories.
11. Removed unconditional `src/test_harness` include path from `RawrXD_Gold` include directories.
12. Added conditional include path restoration for `RawrEngine` when stress/replay option is `ON`.
13. Added conditional include path restoration for `RawrXD_Gold` when stress/replay option is `ON`.
14. Added Win32IDE conditional removal of `src/core/masm_stress_harness.cpp` when option is `OFF`.
15. Added Win32IDE conditional removal of `src/core/convergence_stress_harness.cpp` when option is `OFF`.
16. Added Win32IDE conditional removal of `src/core/enterprise_stress_tests.cpp` when option is `OFF`.

## Why this batch matters

Stress/replay harness files are not production runtime functionality, but they significantly expand symbol dependency surfaces and can force fallback shim ownership into production lanes. Removing them from default production graphs improves symbol closure fidelity and reduces dissolved behavior paths.

## Remaining for next batches

- Eliminate history-indirected production source ownership (`src/core/missing_handler_stubs.cpp`).
- Continue converting remaining fallback-only lanes into explicit non-production options.
- Tighten CMake production profiles so stub providers cannot silently enter production links.
