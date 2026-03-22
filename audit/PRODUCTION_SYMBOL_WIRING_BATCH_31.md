# Production Symbol Wiring - Batch 31

Batch 31 extends strict production symbol-closure instrumentation to RawrEngine/Gold/Inference and aligns policy enforcement behavior for explicit fallback-lane builds.

## Batch 31 fixes applied (15)

1. Added strict-lane map-output wiring for `RawrXD_Gold` (`_GOLD_MAP_PATH`).
2. Added strict-lane root audit mirror path for `RawrXD_Gold` (`_GOLD_MAP_AUDIT_PATH`).
3. Added `RawrXD_Gold` strict link options `/MAP` + `/MAPINFO:EXPORTS`.
4. Added `RawrXD_Gold` `POST_BUILD` map mirror copy command.
5. Added strict-lane map-output wiring for `RawrEngine` (`_RAWR_ENGINE_MAP_PATH`).
6. Added strict-lane root audit mirror path for `RawrEngine` (`_RAWR_ENGINE_MAP_AUDIT_PATH`).
7. Added `RawrEngine` strict link options `/MAP` + `/MAPINFO:EXPORTS`.
8. Added `RawrEngine` `POST_BUILD` map mirror copy command.
9. Added strict-lane map-output wiring for `RawrXD-InferenceEngine` (`_RAWR_INFERENCE_MAP_PATH`).
10. Added strict-lane root audit mirror path for `RawrXD-InferenceEngine` (`_RAWR_INFERENCE_MAP_AUDIT_PATH`).
11. Added `RawrXD-InferenceEngine` strict link options `/MAP` + `/MAPINFO:EXPORTS`.
12. Added `RawrXD-InferenceEngine` `POST_BUILD` map mirror copy command.
13. Updated Win32IDE enforcement flow: when `RAWRXD_ALLOW_AGENTIC_STUB_FALLBACK=ON`, strict stub enforcement functions are intentionally relaxed with a status marker (instead of hard-failing fallback-intended builds).
14. Strengthened strict `self_test_gate` dependency set to include `RawrXD-Win32IDE`, `RawrEngine`, `RawrXD_Gold`, `RawrXD-InferenceEngine`, `RawrXD_MultiWindow_Kernel`, and `RawrXD_DynamicPromptEngine` deterministically.
15. Replaced stale RawrXD_Gold canonical-location comment that referenced an outdated line number with section-based wording.

## Why this batch matters

Strict production strip mode now emits and mirrors map artifacts for all three non-IDE primary targets, enabling parity with Win32IDE map-based symbol closure inspection and reducing dissolved-symbol blind spots.

## Remaining for next batches

- Replace history-indirected `src/core/missing_handler_stubs.cpp` with canonical in-tree implementation ownership.
- Continue collapsing non-strict compatibility lanes into explicit presets/options.
- Add explicit strict-map validation target(s) that consume generated map artifacts for symbol-policy checks.
