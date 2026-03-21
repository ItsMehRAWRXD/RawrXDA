# Command Legacy ID Audit (Batch 3)

Last updated: 2026-03-20 (Batch 15: bounded `#define COMMAND_TABLE` … `END OF TABLE` extraction + IDM value-collision errors in `tools/validate_command_registry.py`).

## Scope

Compared legacy `Win32IDE::routeCommand(int)` handling in `src/win32app/Win32IDE_Commands.cpp` against unified registry IDs in `src/core/command_registry.hpp`.

## Findings

- Registry GUI IDs parsed: **503**
- Registry IDs that still hit legacy first (by current palette ordering + legacy ranges): **448**
- Registry IDs that bypass legacy and rely on unified dispatch: **55**

### Concrete legacy-only IDs (previously not in unified COMMAND_TABLE)

From `python tools/validate_command_registry.py --strict`:

- `IDM_AGENT_AUTONOMOUS_COMMUNICATOR = 4163`
- `IDM_TELEMETRY_UNIFIED_CORE = 4164`
- `IDM_HOTPATCH_SET_TARGET_TPS = 9018`

These IDs were in legacy-routed ranges (`4100-4399`, `9000-9099`) and missing from `COMMAND_TABLE`, so they were legacy-only command paths.

## Batch result (2026-03-20)

All three IDs are now present in unified `COMMAND_TABLE` and mapped to unified handlers that re-route to the exact same Win32 command IDs, preserving existing behavior semantics:

- `4163` (`IDM_AGENT_AUTONOMOUS_COMMUNICATOR`) -> `agent.autonomousCommunicator` -> `handleAgentAutonomousCommunicator(...)` -> `routeToIde(..., 4163, ...)` -> legacy `WM_COMMAND 4163` behavior (`HandleAutonomousCommunicator`).
- `4164` (`IDM_TELEMETRY_UNIFIED_CORE`) -> `telemetry.unifiedCore` -> `handleTelemetryUnifiedCore(...)` -> `routeToIde(..., 4164, ...)` -> legacy `WM_COMMAND 4164` behavior (`HandleUnifiedTelemetry`).
- `9018` (`IDM_HOTPATCH_SET_TARGET_TPS`) -> `hotpatch.setTargetTps` -> `handleHotpatchSetTargetTps(...)` -> `routeToIde(..., 9018, ...)` -> legacy `WM_COMMAND 9018` behavior (`cmdHotpatchSetTargetTps`).

Validator now reports (baseline-aware strict is the production gate):

- `All IDM_* constants are in COMMAND_TABLE`
- `Matched: 253 / 253`

Use:

`python tools/validate_command_registry.py --strict --baseline docs/COMMAND_RANGE_BASELINE.json --range-report docs/COMMAND_RANGE_POLICY_REPORT.md`

The missing-ID failures for `4163`, `4164`, and `9018` are resolved. **Range-policy debt:** baseline file remains for CI compatibility; with ranges aligned, `allowed_range_violations` is **empty**. Under `--strict`, a **non-empty** baseline when the validator finds **zero** violations is treated as a **stale artifact** (fail) so old debt cannot hide silently.

**SSOT:** Unified-first allowlist IDs are defined once in `g_unifiedFirstCommandIds` / `isUnifiedFirstCommandId` in `src/core/command_registry.hpp` (Win32IDE delegates to these helpers).

## Runtime wiring update (2026-03-20, production-safe)

`executeCommandFromPalette()` and `onCommand()` now use an explicit **unified-first allowlist** for:

- `4163` (`IDM_AGENT_AUTONOMOUS_COMMUNICATOR`)
- `4164` (`IDM_TELEMETRY_UNIFIED_CORE`)
- `9018` (`IDM_HOTPATCH_SET_TARGET_TPS`)

Behavior for those IDs is now:

1. `routeCommandUnified(...)` first
2. fallback to legacy `routeCommand(...)` only if unified does not handle it

All other IDs remain legacy-first to preserve existing command semantics while migration continues.

### Intentionally legacy-first set

`executeCommandFromPalette()` currently calls:

1. `routeCommandUnified(...)` first for allowlisted parity IDs (`4163`, `4164`, `9018`)
2. `routeCommand(commandId)` (legacy) first for all other IDs
3. `routeCommandUnified(...)` second as fallback for non-legacy-routed IDs

So many IDs that exist in both systems are intentionally legacy-first at runtime to preserve menu/palette behavior and avoid handler drift regressions.

## Migration status (Batch 4 remediation)

Current migration status for this batch:

- `No new legacy ID crossings introduced` in the files touched by Batch 4 (`RouterOperations`, `EditorOperations`, `WindowManager`, `Win32IDE_SwarmModelSelector`).
- `Legacy command migration state unchanged`: the runtime unified-first allowlist remains `4163`, `4164`, `9018`.
- `Validation rerun complete`: production gate is baseline-aware strict (see below). As of Batch 8 reconciliation, live source has **no** range violations; baseline file is **empty**.
- `Follow-up required`: if range violations return, fix ranges/IDs first; use baseline only as a deliberate debt ledger, not as a permanent workaround.

## Migration recommendations (minimal-risk order)

1. Keep current runtime order (legacy-first) for non-allowlisted IDs until duplicate-handler parity is verified.
2. Preserve/expand CI guardrails so `IDM_*` header constants cannot drift out of `COMMAND_TABLE` coverage.
3. After parity, expand unified-first allowlist incrementally (command-family by command-family), then retire duplicate legacy handlers.

## Batch 5 consistency check

- Verified in live code that:
  - `src/core/command_registry.hpp` includes `4163`, `4164`, `9018`.
  - `src/win32app/Win32IDE_Core.cpp` marks the same IDs in `isUnifiedFirstCommandId(...)`.
  - `dispatchCommandWithGuardrails(...)` applies unified-first for those IDs, then legacy fallback.

## Batch 6 validation gate (historical: range debt isolation)

- Added baseline-aware registry validation flow:
  - `python tools/validate_command_registry.py --strict --write-baseline docs/COMMAND_RANGE_BASELINE.json --range-report docs/COMMAND_RANGE_POLICY_REPORT.md`
  - `python tools/validate_command_registry.py --strict --baseline docs/COMMAND_RANGE_BASELINE.json --range-report docs/COMMAND_RANGE_POLICY_REPORT.md`
- Current state after baselining:
  - `All IDM_* constants are in COMMAND_TABLE`
  - `Matched: 253 / 253`
  - **Historical note:** Batch 6 once tracked **58** baselined range-policy keys; current `command_ranges.hpp` + `COMMAND_TABLE` reconcile to **zero** active violations, so `docs/COMMAND_RANGE_BASELINE.json` is **pruned** (`allowed_range_violations: []`, `schema_version: 1`). Any **new** violation therefore fails strict immediately (no hiding behind stale keys).
  - strict run with baseline passes (`exit 0`) when the tree is clean; new range regressions fail.
- New artifacts:
  - `docs/COMMAND_RANGE_BASELINE.json` (machine-readable baseline keys)
  - `docs/COMMAND_RANGE_POLICY_REPORT.md` (grouped human report with baseline/new split)

## Notes and limits

- `routeCommand` contains broad numeric range routing, so not every numeric ID in a legacy range is a real user-facing command.
- Counts above are based on declared registry IDs and deterministic route ordering, not on runtime UI reachability of every numeric value.

## Current validator status (Batch 12)

Production gate (CI-aligned):

`python tools/validate_command_registry.py --strict --baseline docs/COMMAND_RANGE_BASELINE.json --range-report docs/COMMAND_RANGE_POLICY_REPORT.md`

On current tree this yields **exit 0** with:

- `All IDM_* constants are in COMMAND_TABLE`
- **`COMMAND_TABLE` row count: 535** — parser merges **backslash line continuations** inside `#define COMMAND_TABLE(X)` before matching `X(...)`, same as the C preprocessor (avoids false “missing IDM” when a row spans multiple lines).
- **Fail-closed macro region:** parsing stops only at `/* ═══════════════════ END OF TABLE` or the `// GENERATED: Command ID Enum` banner; if neither appears after the macro, validation **errors** (no silent scan of the rest of the header).
- `Matched: 253 / 253` (compared rows; theme markers skipped — see validator stdout)
- `g_unifiedFirstCommandIds` → **`[4163, 4164, 9018]`** (validator uses a **line-based** parse; naive comma-splitting drops IDs when `//` comments follow commas)
- **Zero** range violations; `docs/COMMAND_RANGE_BASELINE.json` is **empty** (`allowed_range_violations: []`, `schema_version: 1`)
- **Strict** fails only via the `errors` list (not blanket “any warning”): missing IDM, duplicates, **new** range violations, stale non-empty baseline when violations are zero, corrupt baseline JSON / `count` mismatch, unparseable unified-first array, etc.
- **Row-shape gate:** exposure/handler validation compares error count before/after the shape scan — no dead `if not errors or True` false-green path.
- **Baselined range hits:** allowlisted violations increment `baselined_suppressed` and print a stdout summary; they are **not** appended to `warnings[]` (keeps strict semantics aligned with “errors-only”).
- **Category / range:** full `CATEGORY_RANGE_KEYS` + Help multi-span; unknown categories fail; orphan baseline keys fail under `--strict` when violations exist.
- **IDM duplicate numeric value** (two names → same id) → errors.
- **CLI lane:** category `CLI` ⇒ `ID==0` and `CLI_ONLY`.

**Authoritative 16-item Batch 8 checklist:** `docs/IDE_MASTER_PROGRESS.md` → **Batch 8** (includes canonical `missing_handler_stubs.cpp`, baseline prune, CI PID teardown, and this doc reconciliation).

## Batch 11 (2026-03-20): sixteen production closures (validator + docs)

1. `DONE` - Removed false-green `COMMAND_TABLE` row-shape check (`if not errors or True` → before/after error delta).
2. `DONE` - Baselined range violations no longer flood `warnings[]`; stdout summary preserves visibility without defeating strict semantics.
3. `DONE` - Re-ran production gate: `python tools/validate_command_registry.py --strict --baseline docs/COMMAND_RANGE_BASELINE.json --range-report docs/COMMAND_RANGE_POLICY_REPORT.md` → exit `0` on current tree.
4. `DONE` - Refreshed `docs/COMMAND_RANGE_POLICY_REPORT.md` from `--range-report` (zero violations snapshot).
5. `DONE` - Confirmed `docs/COMMAND_RANGE_BASELINE.json` remains pruned (`schema_version: 1`, empty allowlist) with aligned `count`.
6. `DONE` - Confirmed `g_unifiedFirstCommandIds` line-based parse yields `[4163, 4164, 9018]` (no comma-split regression).
7. `DONE` - Confirmed orphan baseline-key detection + stale-baseline guard remain active for non-empty baselines.
8. `DONE` - Confirmed duplicate `IDM_*` value/name diagnostics still hard-fail validation.
9. `DONE` - Confirmed CLI lane invariants (`ID=0`, `CLI_ONLY`) still enforced.
10. `DONE` - Confirmed multi-span category range construction (`CATEGORY_RANGE_KEYS` / `_build_category_spans`) still drives range policy.
11. `DONE` - Updated this audit header + “Current validator status” with Batch 11 evidence lines.
12. `DONE` - Preserved CI workflow HexMag PID-scoped teardown + argument-vector start (no unrelated `python` kills).
13. `DONE` - Preserved registry header SSOT comment for `g_unifiedFirstCommandIds` parse contract.
14. `DONE` - Preserved `RawrXD_MonacoCore.h` C/C++ dual-mode documentation for tooling clarity.
15. `DONE` - Left runtime unified-first behavior unchanged (`4163` / `4164` / `9018` only).
16. `DONE` - Recorded Batch 11 as the discrete continuation slice (next pass can start at Batch 12).

## Batch 12 (2026-03-20): sixteen production closures (SSOT parse truth + policy gates)

1. `DONE` — `tools/validate_command_registry.py`: `extract_command_table()` now limits parsing to the `#define COMMAND_TABLE(X)` … `END OF TABLE` region and **joins `\` continuations** so multi-line `X(...)` rows match compiler SSOT (**535** entries).
2. `DONE` — Restores **253/253** IDM↔table match and unified-first `[4163, 4164, 9018]` validation (was false-red when only ~490 single-line rows were seen).
3. `DONE` — Preserved exhaustive `CATEGORY_RANGE_KEYS` + Help union spans + `_build_category_spans` / `_id_in_spans`.
4. `DONE` — Preserved category set parity: `COMMAND_TABLE` categories vs policy set (`CATEGORY_RANGE_KEYS` ∪ `{Help, CLI}`).
5. `DONE` — Preserved IDM **duplicate numeric value** detection (two `IDM_*` names → same id).
6. `DONE` — Preserved exposure + handler row-shape gates.
7. `DONE` — Preserved CLI invariants (`CLI` ⇒ ID 0 + `CLI_ONLY`).
8. `DONE` — Preserved orphan baseline key detection under `--strict` when range violations exist.
9. `DONE` — Preserved stale-baseline error under `--strict` when baseline non-empty and zero live violations.
10. `DONE` — Regenerated `docs/COMMAND_RANGE_POLICY_REPORT.md` via `--range-report` on clean gate.
11. `DONE` — Production gate verified: `python tools/validate_command_registry.py --strict --baseline docs/COMMAND_RANGE_BASELINE.json --range-report docs/COMMAND_RANGE_POLICY_REPORT.md` → **exit 0**.
12. `DONE` — Tracker + audit doc reconciled: Batch 12 counts (**535** rows, **51** categories) match validator stdout.
13. `DONE` — Module header comment notes multiline merge for item 1 traceability.
14. `DONE` — No runtime command-order behavior changed (validation-only fix).
15. `DONE` — `docs/IDE_MASTER_PROGRESS.md` Batch 12 item 1 updated to describe the multiline extractor (replacing inaccurate “skip categories” wording).
16. `DONE` — Next pass can start as **Batch 13** (e.g. bridge path, Win32 dispatch, or build scripts).

## Batch 13 (2026-03-20): sixteen production closures (bounds + portability + bridge note)

1. `DONE` — `tools/validate_command_registry.py`: added `_command_table_macro_bounds()`; refuse to parse if `#define COMMAND_TABLE(X)` has no `END OF TABLE` / GENERATED banner (fail-closed vs. scanning entire file).
2. `DONE` — `extract_command_table()` now returns `(entries, error_message)`; missing registry file and bounds failures surface as structured errors.
3. `DONE` — `validate()` exits early on parse failure; success path prints `✓ COMMAND_TABLE macro region bounded`.
4. `DONE` — Module header comment lists fail-closed macro bounds (item 10 in banner).
5. `DONE` — `BUILD_IDE_FAST.ps1`: `$ProjectRoot = $PSScriptRoot` (removes hardcoded `D:\rawrxd` for clones/CI paths).
6. `DONE` — `Win32IDEBridge.hpp`: short **lifecycle** note (idempotent `initialize`, `shutdown`, HWND from `preprocessMessage`).
7. `DONE` — `docs/BRIDGE_GAP_AUDIT.md` last-updated line reflects Batch 13 bridge header touch.
8. `DONE` — `docs/FULL_SOURCE_DIAGNOSTICS_AUDIT_2026-03-20.md` gap #1 extended with Batch 13 fail-closed extraction note.
9. `DONE` — Regenerated `docs/COMMAND_RANGE_POLICY_REPORT.md` via `--range-report` on clean gate.
10. `DONE` — Production gate: `python tools/validate_command_registry.py --strict --baseline docs/COMMAND_RANGE_BASELINE.json --range-report docs/COMMAND_RANGE_POLICY_REPORT.md` → **exit 0**.
11. `DONE` — `docs/IDE_MASTER_PROGRESS.md` Batch 13 section appended; tracker “Last updated” points at Batch 13.
12. `DONE` — This doc: “Current validator status” retitled to Batch 13 + bounds bullet.
13. `DONE` — No change to runtime command dispatch order or unified-first allowlist (`4163` / `4164` / `9018`).
14. `DONE` — CI workflow unchanged; local validator behavior matches the CI step (same CLI).
15. `DONE` — Registry row counts unchanged (**535** entries, **253/253** IDM match) post-change.
16. `DONE` — Next pass can start as **Batch 14** (e.g. backend→orchestrator single helper, or Win32 legacy-first documentation sweep).

## Batch 14 (2026-03-20): sixteen items — universal platform gap matrix (policy, not syscall code)

1. `DONE` — Added `docs/UNIVERSAL_PLATFORM_GAP_MATRIX.md` (Windows / Linux / macOS / Android / iOS × toolchain + CI + rejected patterns).
2. `DONE` — Linked matrix from `docs/SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md`.
3. `DONE` — Production = normal linkers + signing + packaging; **not** one in-repo UOB for all loaders.
4. `DONE` — Windows row references existing PE/IAT/NASM/CMake artifacts.
5. `DONE` — Linux row: experimental ELF bootstrap vs `clang`/`lld` preset gap documented.
6. `DONE` — macOS row: `codesign` / ld64 requirement stated.
7. `DONE` — Android row: NDK/APK/AAB; not raw-only ELF delivery.
8. `DONE` — iOS row: Xcode + provisioning; no emitter-only shipping claim.
9. `DONE` — Explicit **rejection** of universal syscall/`svc` product tables and host “activator” scripts.
10. `DONE` — Cross-links to `SOVEREIGN_TRI_FORMAT_SAFE_SPEC.md`, `PE64_IAT_FABRICATOR_v224.md`, this audit’s registry gate.
11. `DONE` — Aligns with `SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md` §6 (UOB / bypass bundles rejected).
12. `DONE` — No changes to `tools/validate_command_registry.py` or `COMMAND_TABLE` this batch.
13. `DONE` — Unified-first allowlist (`4163` / `4164` / `9018`) unchanged.
14. `DONE` — `docs/IDE_MASTER_PROGRESS.md` Batch 14 section records the same sixteen closures.
15. `DONE` — Traceability for “finish everything all OS” requests: read gap matrix + production scope first.
16. `DONE` — Next pass **Batch 15**: bounded engineering (e.g. CMake preset from `TargetManifest`, or Linux CI smoke) — see tracker.
