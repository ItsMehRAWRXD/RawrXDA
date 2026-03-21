# Production Symbol Wiring - Batch 27

Batch 27 replaces remaining global MASM/link feature macro leakage with target-scoped definitions for the primary production targets, reducing cross-target dissolved behavior caused by ambient `add_definitions(...)`.

## Batch 27 fixes applied (15)

1. Removed global `add_definitions(-DRAWR_HAS_MASM=1)` from MASM detection block.
2. Removed global `add_definitions(-DRAWRXD_LINK_QUADBUFFER_ASM=1)` from MASM detection block.
3. Removed global `add_definitions(-DRAWRXD_LINK_INFERENCE_KERNELS_ASM=1)` from MASM detection block.
4. Removed global `add_definitions(-DRAWRXD_LINK_ANALYZER_DISTILLER_ASM=0)` from MASM detection block.
5. Removed global `add_definitions(-DRAWRXD_LINK_STREAMING_ORCHESTRATOR_ASM=0)` from MASM detection block.
6. Removed global ASM-kernel block `add_definitions(...)` for telemetry/prometheus/selfpatch/sourceedit/reverse-engineering/deobfuscator-related feature flags.
7. Added shared target-scoped `_RAWR_ASM_FEATURE_DEFINITIONS` set with explicit MASM-aware generator expressions.
8. Added `RAWRXD_LINK_QUADBUFFER_ASM` target-scoped definition via `_RAWR_ASM_FEATURE_DEFINITIONS`.
9. Added `RAWRXD_LINK_INFERENCE_KERNELS_ASM` target-scoped definition via `_RAWR_ASM_FEATURE_DEFINITIONS`.
10. Added `RAWRXD_LINK_TELEMETRY_KERNEL_ASM` target-scoped definition via `_RAWR_ASM_FEATURE_DEFINITIONS`.
11. Added `RAWRXD_LINK_PROMETHEUS_EXPORTER_ASM` / `RAWRXD_LINK_SELFPATCH_AGENT_ASM` / `RAWRXD_LINK_SOURCEEDIT_KERNEL_ASM` target-scoped definitions via `_RAWR_ASM_FEATURE_DEFINITIONS`.
12. Added `_RAWR_ASM_FEATURE_DEFINITIONS` to `RawrEngine` compile definitions (explicit per-target capability ownership).
13. Added `_RAWR_ASM_FEATURE_DEFINITIONS` to `RawrXD_Gold` and removed duplicated per-flag constants from Gold’s compile-definition block.
14. Added `_RAWR_ASM_FEATURE_DEFINITIONS` to `RawrXD-InferenceEngine` and removed redundant MASM-only per-flag compile-definition block in inference ASM section.
15. Added `_RAWR_ASM_FEATURE_DEFINITIONS` to `RawrXD-Win32IDE`, and also to `rawrxd` CLI target when that target exists.

## Why this batch matters

This batch prevents global macro bleed between unrelated targets and ensures each primary production target receives a deterministic MASM/link feature contract based on the actual build lane (`RAWR_HAS_MASM`), improving symbol/provider consistency.

## Remaining for next batches

- Replace history-indirected `src/core/missing_handler_stubs.cpp` with canonical in-tree implementation ownership.
- Continue collapsing non-strict compatibility lanes behind explicit options.
- Add deeper compiled-symbol closure checks for RawrEngine/Gold/Inference targets.
