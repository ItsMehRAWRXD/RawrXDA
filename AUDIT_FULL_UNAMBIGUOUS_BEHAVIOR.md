# RawrXD Audit: Full Unambiguous Behavior

## Current State (Verified)

- IDE build now enforces amphibious artifacts (CLI + GUI) via `RawrXD_IDE_BUILD.ps1`.
- `Build-Amphibious-ml64.ps1` compiles with `ml64` and links both binaries.
- CLI pipeline executes deterministic stage coverage and exits non-zero on failure.

## Gaps Remaining to Reach FULL Unambiguous Behavior

### 1) Real local model execution is not wired in the active amphibious core
- `RawrXD_Amphibious_Core_ml64.asm` currently marks stages and emits log text.
- No active call chain to a local GGUF loader/tokenizer/inference runtime in this path.
- Effect: behavior is deterministic and testable, but still synthetic relative to real model inference.

### 2) GUI build is validated as a PE GUI binary, but not yet a live IDE surface host
- Current GUI entry (`RawrXD_Amphibious_GUI_ml64.asm`) runs cycles and shows success/failure message box.
- It does not yet bind to an IDE editor HWND and stream live decoded tokens into the editor control.

### 3) No hard gate for GUI runtime smoke in CI/build script
- Build now runs mandatory CLI smoke test.
- GUI runtime behavior is not currently automated in headless validation.

### 4) Sovereign/HLL macro path remains parallel to ml64-native path
- Legacy sovereign modules use MASM macro style and are not the active ml64 amphibious runtime path.
- Full parity requires a single source-of-truth runtime path for all production artifacts.

### 5) No persistent telemetry artifact from CLI smoke run
- Runtime logs print to stdout.
- Build does not yet emit structured telemetry JSON for downstream gating.

## Definition of Done for FULL Unambiguous Behavior

1. Active amphibious core invokes local tokenizer+inference functions from production runtime.
2. GUI path streams token output to IDE renderer/editor control (not message-box-only).
3. Build script enforces:
   - compile + link,
   - CLI smoke test,
   - GUI integration test (or mocked UI harness),
   - structured telemetry output + pass/fail gate.
4. Consolidate to one production runtime path (remove drift between legacy macro and ml64-native logic).

## Immediate Next Actions

1. Replace synthetic stage writers in `RawrXD_Amphibious_Core_ml64.asm` with real local model dispatch calls.
2. Add editor HWND streaming path to `RawrXD_Amphibious_GUI_ml64.asm`.
3. Extend `Build-Amphibious-ml64.ps1` to emit `build/amphibious-ml64/smoke_report.json`.
4. Wire audit gate into `RawrXD_IDE_BUILD.ps1` so failures block artifact promotion.
