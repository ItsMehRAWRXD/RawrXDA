# Full source diagnostics audit — 2026-03-20

Point-in-time diagnostics closure snapshot based on live source state in `D:/rawrxd`.

---

## Scope audited

- Core runtime command/message path:
  - `src/win32app/Win32IDE_Core.cpp`
  - `src/win32app/Win32IDE_Commands.cpp`
- Backend/orchestrator bridge path:
  - `src/win32app/Win32IDE_BackendSwitcher.cpp`
  - `src/agentic/OrchestratorBridge.cpp`
- Functional parity files requested for compile-obvious review:
  - `src/win32app/RouterOperations.cpp`
  - `src/win32app/EditorOperations.cpp`
  - `src/win32app/WindowManager.cpp`
  - `src/win32app/Win32IDE_SwarmModelSelector.cpp`
- Tracker/audit docs synchronized to source truth:
  - `docs/IDE_MASTER_PROGRESS.md`
  - `docs/BRIDGE_GAP_AUDIT.md`
  - `docs/COMMAND_LEGACY_ID_AUDIT.md`
  - `docs/INFERENCE_PATH_MATRIX.md`

---

## Critical gaps

1. **Command registry gate (resolved for range drift as of 2026-03-20).**
   - `command_ranges.hpp` was widened/aligned to live `IDM_*` lanes; `docs/COMMAND_RANGE_BASELINE.json` is **empty** so CI `--strict` enforces zero range violations without masking.
   - Production command: `python tools/validate_command_registry.py --strict --baseline docs/COMMAND_RANGE_BASELINE.json --range-report docs/COMMAND_RANGE_POLICY_REPORT.md` → expect **exit 0**, no stale-baseline error.
   - **Batch 13 (2026-03-20):** extraction is **fail-closed** — the `#define COMMAND_TABLE(X)` region must end with `END OF TABLE` or the GENERATED `CmdID` banner; unbounded scans are rejected. Stdout prints `✓ COMMAND_TABLE macro region bounded` on success.
   - References:
     - `docs/COMMAND_LEGACY_ID_AUDIT.md`
     - `src/core/command_registry.hpp`
     - `src/core/command_ranges.hpp`
     - `tools/validate_command_registry.py`

2. **Legacy-first remains default for most command IDs by design (controlled crossing still active).**
   - `dispatchCommandWithGuardrails(...)` is unified-first only for allowlisted parity IDs (`4163`, `4164`, `9018`), then falls back legacy-first for all other IDs.
   - References:
     - `src/win32app/Win32IDE_Core.cpp`
     - `docs/COMMAND_LEGACY_ID_AUDIT.md`

3. **Bridge status narrative drift existed in docs and is now corrected, but needs continued hygiene.**
   - Live code proves `Win32IDEBridge` init + preprocess + readiness gate are active in core startup path.
   - References:
     - `src/win32app/Win32IDE_Core.cpp`
     - `src/win32app/Win32IDE_BackendSwitcher.cpp`
     - `docs/BRIDGE_GAP_AUDIT.md`

---

## Crossings

- **Command crossing (legacy <-> unified):**
  - Crossing is intentional and guarded; only specific IDs are unified-first.
  - References:
    - `src/win32app/Win32IDE_Core.cpp` (`isUnifiedFirstCommandId`, `dispatchCommandWithGuardrails`)
    - `src/core/command_registry.hpp`

- **Inference crossing (backend switcher <-> orchestrator):**
  - Backend config mutation calls orchestrator sync helper; readiness gate also enforces sync.
  - References:
    - `src/win32app/Win32IDE_BackendSwitcher.cpp`
    - `src/win32app/Win32IDE_Core.cpp`
    - `docs/INFERENCE_PATH_MATRIX.md`

- **UI operation crossing (router/editor/window/parity lane):**
  - Dialogs, clipboard, selection, undo/redo, and window init have concrete implementations in reviewed files.
  - References:
    - `src/win32app/RouterOperations.cpp`
    - `src/win32app/EditorOperations.cpp`
    - `src/win32app/WindowManager.cpp`
    - `src/win32app/Win32IDE_SwarmModelSelector.cpp`

---

## Bridges

- **Win32IDEBridge lifecycle (live): CONNECTED**
  - `onCreate` initializes bridge (if needed), `handleMessage` preprocesses messages, and readiness gate enforces bridge/backend/router startup state.
  - References:
    - `src/win32app/Win32IDE_Core.cpp`

- **Backend manager -> OrchestratorBridge sync (live): CONNECTED/PARTIAL**
  - Endpoint/model sync is wired on init, config load, backend switch, endpoint/model mutation, and settings apply.
  - Still partial due to strict validator lane and broader legacy dispatch migration not fully complete.
  - References:
    - `src/win32app/Win32IDE_BackendSwitcher.cpp`
    - `src/win32app/Win32IDE_Settings.cpp`

- **Command registry bridge (live): PARTIAL**
  - Migrated IDs are bridged and validated for coverage; full registry/range hygiene remains open.
  - References:
    - `src/core/command_registry.hpp`
    - `src/core/ssot_handlers.cpp`
    - `docs/COMMAND_LEGACY_ID_AUDIT.md`

---

## Completion by layer

- **UI / Win32 operations layer:** High completion in audited scope.
  - Dialog/clipboard/selection/edit stack and model selector implementations are concrete.
  - References:
    - `src/win32app/RouterOperations.cpp`
    - `src/win32app/EditorOperations.cpp`
    - `src/win32app/WindowManager.cpp`
    - `src/win32app/Win32IDE_SwarmModelSelector.cpp`

- **Bridge lifecycle layer:** Substantially connected in live startup/message path.
  - References:
    - `src/win32app/Win32IDE_Core.cpp`
    - `src/win32app/Win32IDE_BackendSwitcher.cpp`

- **Dispatch/registry governance layer:** Partial.
  - Unified-first crossing is narrowed to parity IDs; broader policy cleanup remains.
  - References:
    - `src/win32app/Win32IDE_Core.cpp`
    - `docs/COMMAND_LEGACY_ID_AUDIT.md`

- **Docs tracker hygiene layer:** Corrected in this snapshot.
  - Duplicate sections and contradictory status narratives were reconciled.
  - References:
    - `docs/IDE_MASTER_PROGRESS.md`
    - `docs/BRIDGE_GAP_AUDIT.md`

---

## Priority closure order

1. **P0 - Finish strict command policy closure lane without changing runtime semantics.**
   - Keep coverage parity; reduce pre-existing range-policy failures incrementally.
   - References: `tools/validate_command_registry.py`, `src/core/command_registry.hpp`

2. **P0 - Continue command crossing reduction from legacy-first to policy-driven unified-first subsets.**
   - Expand allowlist only after parity verification per command family.
   - References: `src/win32app/Win32IDE_Core.cpp`, `docs/COMMAND_LEGACY_ID_AUDIT.md`

3. **P1 - Keep bridge status docs in lockstep with startup/message wiring changes.**
   - Any `onCreate`/`preprocessMessage`/readiness gate changes should immediately update bridge audit docs.
   - References: `src/win32app/Win32IDE_Core.cpp`, `docs/BRIDGE_GAP_AUDIT.md`

4. **P1 - Maintain inference matrix consistency on every backend mutation path change.**
   - References: `src/win32app/Win32IDE_BackendSwitcher.cpp`, `docs/INFERENCE_PATH_MATRIX.md`
