# Production Symbol Wiring - Batch 21

Batch 21 makes strict filtering more consistent for `link_stubs_*` naming variants, cleans up inference shim wiring to be lane-explicit, and fixes a strict gate dependency hook that previously executed too early.

## Batch 21 fixes applied (15)

1. Added strict shared-source filtering for `link_stubs_*.cpp` in the top-level `SOURCES` strict strip pass.
2. Added strict `RAWR_ENGINE_SOURCES` filtering for `link_stubs_*.cpp`.
3. Added strict `GOLD_UNDERSCORE_SOURCES` filtering for `link_stubs_*.cpp`.
4. Removed unconditional `src/inference/inference_standalone_link_shims.cpp` from base `INFERENCE_ENGINE_SOURCES`.
5. Rewired inference standalone link shim inclusion to be explicit non-strict append only (`if(NOT RAWRXD_PRODUCTION_STRIP_STUB_SOURCES)`).
6. Added strict `INFERENCE_ENGINE_SOURCES` filtering for `link_stubs_*.cpp`.
7. Removed `src/modules/quickjs_node_shims.cpp` from the default Win32IDE source aggregation list.
8. Added `src/modules/quickjs_node_shims.cpp` only in the non-strict Win32IDE fallback append lane.
9. Added strict Win32IDE strip filter for `link_stubs_*.cpp` in addition to existing explicit removals.
10. Extended `EnforceNoStubs` detection regex to treat `link_stubs_*.cpp` as policy violations.
11. Extended broad Win32IDE post-filter regex to remove `link_stubs_*.cpp`.
12. Extended strict post-check forbidden regex to include `link_stubs_*.cpp`.
13. Removed ineffective `if(TARGET validate_registry_strict)` dependency check that executed before the target existed.
14. Added strict dependency wiring to `self_test_gate` for `validate_registry_strict` immediately after target creation (inside `if(Python3_FOUND)`).
15. Preserved existing strict dependencies for `RawrEngine`, `RawrXD_Gold`, and `RawrXD-InferenceEngine` while making strict registry validation dependency deterministic.

## Why this batch matters

It closes another naming-based loophole (`link_stubs_*`) and ensures strict lane logic is explicit and deterministic rather than relying on broad best-effort filtering or configure-order accidents.

## Remaining for next batches

- Replace the history-indirected `src/core/missing_handler_stubs.cpp` wrapper with canonical in-tree implementation ownership.
- Continue narrowing non-strict fallback lanes into explicit compatibility toggles.
- Add deeper compiled-symbol closure checks for Engine/Gold/Inference targets.
