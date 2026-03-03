# Parity Validation Proof Pack

**Purpose:** Make “command surface parity” and “competitive bar” **objectively testable**. Investors and power users can run the same commands and compare outputs.  
**Companion:** **docs/RAWRXD_COMPETITIVE_STANDARD.md** §4 “How we measure parity.”

---

## 1. Commands to run

### Build

```powershell
cd D:\rawrxd
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target RawrXD-Win32IDE
# Optional: also build self_test_gate if present in this config
cmake --build build --target self_test_gate
```

**Success:** Exit code 0; `build\bin\RawrXD-Win32IDE.exe` (and optionally `self_test_gate.exe`) produced.

### Headless / smoke (no UI)

```powershell
# From repo root — run validation harness (includes self_test_gate if built)
.\test.ps1
# Or run a smoke script (e.g. SmokeTest-AgenticIDE.ps1, smoke_test_proper.ps1)
.\SmokeTest-AgenticIDE.ps1
```

**Success:** `test.ps1` reports pass for self_test_gate (exit 0 with `--quick`) or smoke script completes without crash.

### Self-test gate (when available)

```powershell
.\build\bin\self_test_gate.exe
# Or from build dir
.\bin\self_test_gate.exe
```

**Success:** Exit code 0; output indicates pass/fail per area (see §3).

### Three-build stability (Batch N style)

```powershell
cmake --build build --target RawrXD-Win32IDE
cmake --build build --target RawrXD-Win32IDE
cmake --build build --target RawrXD-Win32IDE
```

**Success:** All three builds exit 0 (incremental sanity).

---

## 2. Output hashes (optional)

To lock “known good” behavior, record SHA256 of artifacts after a clean build:

| Artifact | When to record |
|----------|-----------------|
| `build\bin\RawrXD-Win32IDE.exe` | After successful full build |
| `build_validation_batchN.txt` | After Batch N three-build run (contains HEAD, BUILD1/2/3_EXIT, EXE_SHA256) |

Example (PowerShell):

```powershell
(Get-FileHash -Path "build\bin\RawrXD-Win32IDE.exe" -Algorithm SHA256).Hash
```

Use hashes only for **regression** (same commit → same hash). Different commits or configs will differ.

---

## 3. Checklist: CURSOR_GITHUB_PARITY_SPEC → test case

Maps **docs/CURSOR_GITHUB_PARITY_SPEC.md** command IDs and areas to a test case name and expected outcome. Fill **Expected output** as you implement or validate each row.

| Spec area | Command ID range | Test case name | Expected output / pass criteria |
|-----------|------------------|----------------|----------------------------------|
| Telemetry export | IDM_TELEXPORT_* (11500–11507) | `telexport_export_json` | Export produces valid JSON file; no crash |
| Agentic Composer | IDM_COMPOSER_* (11510–11515) | `composer_new_session` | Session starts; UI or CLI confirms |
| @-Mention context | IDM_MENTION_* | `mention_parse` | Parse @file returns context; no crash |
| Vision encoder | IDM_VISION_* (11530–11534) | `vision_load_file` | Load image into buffer; no crash |
| Refactoring | IDM_REFACTOR_* (11540–11547) | `refactor_rename_symbol` | LSP rename or stub returns ok |
| Language registry | IDM_LANG_* (11550–11553) | `lang_list_all` | List returns ≥0 languages |
| Semantic index | IDM_SEMANTIC_* (11560–11567) | `semantic_build_index` | Index build completes or deferred message |
| Resource generator | IDM_RESOURCE_* (11570–11574) | `resource_list_templates` | List returns ≥0 templates |
| GitHub read/write | (per spec) | `github_read_releases` | Read releases returns data or graceful fail |

**How to use:** For each row, run the corresponding IDE command (menu or CLI) and confirm the “Expected output” cell. Update the cell when behavior is implemented or when you accept a “Phase 1” outcome (e.g. “delegateToGui; WM_COMMAND received”).

---

## 4. Latency and resource envelope (when implemented)

When completion, ghost text, and search are wired, add to this section:

| Metric | Target | How to measure |
|--------|--------|----------------|
| Completion popup | &lt; X ms from keystroke | Instrument CompletionEngine path; log timestamp at keystroke vs first result. |
| Ghost text first token | &lt; Y ms | Instrument streaming path to first token. |
| Workspace search (N files) | &lt; Z ms | Run search over N files; record wall time. |
| Working set — idle | — MB | Task Manager or GetProcessWorkingSetSize after 30 s idle. |
| Working set — indexing | — MB | Same during full project index. |
| Working set — chat streaming | — MB | Same while streaming a reply. |

Replace placeholders (X, Y, Z, —) with actual numbers and document tool (e.g. ETW, log file, script) in **docs/RUNTIME_BOOTSTRAP_AND_VALIDATION.md** or here.

---

## 5. Cloud Bridge safety (v1.3.0)

Cloud Bridge safety validation is defined in **docs/SOVEREIGN_CLOUD_BRIDGE_SPEC.md** (CB-01..CB-07).
Validation focus: default-deny, explicit consent, payload preview/hash match, receipts, panic-off enforcement, and adapter isolation.

### 5.1 CB ship-gate checklist

| ID | Test case name | Expected output / pass criteria | Artifacts / receipts |
|----|----------------|----------------------------------|----------------------|
| `CB-01` | `cloud_bridge_default_off` | Clean install starts with bridge disabled; no outbound cloud request attempted until explicit enable. | `build_validation_cloud_bridge_run1.txt` includes `CB-01=PASS`. |
| `CB-02` | `cloud_bridge_consent_required` | Per-request consent dialog/flow blocks request until approval; denied request produces blocked status. | Receipt file under `.rawrxd/audit/receipts/` with `decision=deny` then `decision=allow` on approval. |
| `CB-03` | `cloud_bridge_sensitive_block` | `D3` payload classes are blocked by default and only pass with explicit one-time override. | Receipt includes `data_class=D3` and `override_used=true` when allowed. |
| `CB-04` | `cloud_bridge_redaction` | Known token/key patterns are redacted before send; raw secret value not present in outbound payload manifest. | Receipt manifest lists redaction actions; no raw key in receipt body. |
| `CB-05` | `cloud_bridge_panic_off` | Panic-off cancels active/queued cloud requests immediately and blocks new cloud egress until re-enabled. | Audit log entry `panic_off=active`; receipt for blocked request after panic-off. |
| `CB-06` | `cloud_bridge_audit_export` | Audit export succeeds (JSON/CSV) and contains decision trail without raw prompt body leakage. | Export files under `.rawrxd/audit/exports/`. |
| `CB-07` | `cloud_bridge_local_parity` | Local-only workflow remains functional with bridge disabled; no regression in local completion/chat path. | `build_validation_cloud_bridge_run1.txt` includes `CB-07=PASS`. |

### 5.2 Receipt locations

- `.rawrxd/audit/receipts/` for per-request receipts
- `.rawrxd/audit/exports/` for exported summaries (JSON/CSV)
- `build_validation_cloud_bridge_run1.txt` for run-level summary (`HEAD`, build exits, CB results)

---

## 6. Completion / ghost text ship-gate (v1.3.0 Next 7)

| ID | Test case name | Expected output / pass criteria | Artifacts / receipts |
|----|----------------|----------------------------------|----------------------|
| `COMP-01` | `completion_ghost_appears_deterministic` | For a deterministic fixture file/input, ghost text appears within debounce window and matches expected prefix extension. | `build_validation_completion_run1.txt` includes `COMP-01=PASS` and measured `keystroke_to_ghost_ms`. |
| `COMP-02` | `completion_tab_accept_exact_insert` | Pressing `Tab` inserts exactly the suggested ghost text into the buffer at caret; no extra characters/newlines. | Diff artifact under `.rawrxd/audit/completion/` with expected == actual insertion payload. |
| `COMP-03` | `completion_esc_clears_overlay` | Pressing `Esc` clears ghost overlay without mutating editor content. | `build_validation_completion_run1.txt` includes `COMP-03=PASS`; before/after buffer hash unchanged. |

---

## 7. Summary

- **Build + smoke + (optional) self_test_gate** = minimum proof that the IDE and command surface exist and run.
- **Checklist (§3)** = maps spec to test cases so parity is auditable.
- **Hashes (§2), latency/resource (§4), Cloud Bridge safety (§5), and completion ship-gates (§6)** = optional hardening for regression, performance, privacy, and UX claims.

**Status:** Proof pack is the single place to point skeptics: “Run these commands; here is the checklist.”
