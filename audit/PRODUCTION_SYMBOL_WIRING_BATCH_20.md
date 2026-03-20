# Production Symbol Wiring - Batch 20

Batch 20 removes remaining global-definition leakage for prompt/kernel/QuickJS capability flags and makes capability state explicit per production target.

## Batch 20 fixes applied (15)

1. Added explicit `RAWRXD_HAS_MW_KERNEL_FLAG` initialization (`0`) before MultiWindow kernel wiring.
2. Replaced global `add_definitions(-DRAWRXD_HAS_MW_KERNEL=1)` with local flag assignment when MultiWindow kernel target is enabled.
3. Removed global `add_definitions(-DRAWRXD_HAS_MW_KERNEL=0)` fallback path (no more global leakage).
4. Added explicit `RAWRXD_HAS_PROMPT_ENGINE_FLAG` initialization (`0`) before DynamicPromptEngine wiring.
5. Replaced global `add_definitions(-DRAWRXD_HAS_PROMPT_ENGINE=1)` with local flag assignment when prompt engine target is enabled.
6. Removed global `add_definitions(-DRAWRXD_HAS_PROMPT_ENGINE=0)` fallback path.
7. Added target-scoped `RAWRXD_HAS_MW_KERNEL` and `RAWRXD_HAS_PROMPT_ENGINE` compile definitions to `RawrEngine`.
8. Added target-scoped `RAWRXD_HAS_MW_KERNEL` and `RAWRXD_HAS_PROMPT_ENGINE` compile definitions to `RawrXD_Gold`.
9. Added target-scoped `RAWRXD_HAS_MW_KERNEL` and `RAWRXD_HAS_PROMPT_ENGINE` compile definitions to `RawrXD-InferenceEngine`.
10. Added target-scoped `RAWRXD_HAS_MW_KERNEL` and `RAWRXD_HAS_PROMPT_ENGINE` compile definitions to `RawrXD-Win32IDE`.
11. Removed global `add_definitions(-DRAWR_HAS_QUICKJS=1)` to prevent cross-target capability leakage.
12. Added explicit `RAWR_QUICKJS_STUB=0` compile definition to `RawrXD-Win32IDE` when real QuickJS is present.
13. Added explicit `RAWR_QUICKJS_STUB=0` compile definitions to `RawrEngine`, `RawrXD_Gold`, and `RawrXD-InferenceEngine` when real QuickJS is present.
14. Added explicit `RAWR_QUICKJS_STUB=1` compile definitions to `RawrEngine`, `RawrXD_Gold`, and `RawrXD-InferenceEngine` when QuickJS is absent.
15. Preserved existing Win32IDE `RAWR_QUICKJS_STUB=1` path while harmonizing per-target QuickJS capability state (`RAWR_HAS_QUICKJS` + `RAWR_QUICKJS_STUB`) across all primary production executables.

## Why this batch matters

This batch converts previously global, leak-prone feature flags into target-scoped capability declarations. That reduces dissolved behavior caused by unrelated targets inheriting definitions that do not match their actual linked providers.

## Remaining for next batches

- Promote canonical ownership of `src/core/missing_handler_stubs.cpp` by replacing the history-indirected include wrapper.
- Continue collapsing non-strict compatibility lanes into explicit opt-in options.
- Add stricter compiled-symbol closure checks for Engine/Gold/Inference in addition to source-pattern filtering.
