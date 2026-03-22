# Production Symbol Wiring - Batch 19

Batch 19 tightens strict-mode handling for QuickJS shim fallbacks and stub-based auxiliary targets, while propagating explicit QuickJS capability state to all primary production targets.

## Batch 19 fixes applied (15)

1. Added `src/modules/quickjs_node_shims.cpp` to the Win32IDE strict strip removal list.
2. Updated Win32IDE strict strip status message to explicitly reflect compatibility-unit stripping scope.
3. Linked `quickjs_static` to `RawrXD_Gold` when QuickJS is available.
4. Added QuickJS include path wiring for `RawrXD_Gold` when QuickJS is available.
5. Linked `quickjs_static` to `RawrXD-InferenceEngine` when QuickJS is available.
6. Added QuickJS include path wiring for `RawrXD-InferenceEngine` when QuickJS is available.
7. Added explicit `RAWR_HAS_QUICKJS=1` compile definition to `RawrXD-Win32IDE` when QuickJS is found.
8. Added explicit `RAWR_HAS_QUICKJS=1` compile definition to `RawrEngine` when QuickJS is found.
9. Added explicit `RAWR_HAS_QUICKJS=1` compile definition to `RawrXD_Gold` when QuickJS is found.
10. Added explicit `RAWR_HAS_QUICKJS=1` compile definition to `RawrXD-InferenceEngine` when QuickJS is found.
11. Added strict guard: missing QuickJS now causes `FATAL_ERROR` when `RAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON` (prevents QuickJS stub lane in strict production).
12. Added explicit `RAWR_HAS_QUICKJS=0` compile definition to `RawrXD-Win32IDE` when QuickJS is missing.
13. Added explicit `RAWR_HAS_QUICKJS=0` compile definition to `RawrEngine`, `RawrXD_Gold`, and `RawrXD-InferenceEngine` when QuickJS is missing.
14. Gated `RawrXD-AutoFixCLI` creation out of strict production strip mode (it depends on `cpu_inference_engine_autofix_stub.cpp`) and added explicit skip status logging.
15. Gated `SwarmSmokeTest` out of strict production strip mode (it depends on `swarm_smoke_stubs.cpp`) and added explicit skip status logging.

## Why this batch matters

This batch removes additional dissolved behavior paths where strict production mode could still silently accept shim-backed JS/runtime fallbacks or stub-backed auxiliary executables.

## Remaining for next batches

- Promote canonical ownership of `src/core/missing_handler_stubs.cpp` (remove history-indirected include wrapper).
- Continue converting non-strict default fallback lanes into explicit compatibility options.
- Expand strict symbol-closure verification beyond source-pattern gates.
