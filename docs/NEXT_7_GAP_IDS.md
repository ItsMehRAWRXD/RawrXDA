# NEXT_7 Run #1 - Completion + Ghost Text Parity (P0/P1-equivalent)

## Source mapping

- Primary spec: `docs/RAWRXD_COMPETITIVE_STANDARD.md` Sec 2.1 and Sec 4 Priority #1
- Evidence: `docs/PARITY_VALIDATION_PROOF_PACK.md` (self_test_gate + latency/resource table)

## Gap IDs (local) and TOP_50 references

> TOP_50 is category/table-row oriented and does not label P0/P1; these IDs are stable internal anchors for the "Next 7" cadence. Each item lists where it belongs in TOP_50 (section/table row) and how it is proven closed.

---

## NX7-01 - Real-time completion wired to keystrokes

**TOP_50 reference:** AI/Editor: "Real-time code completion (Copilot-style)" row
**Closed when:**

- Completion triggers on typing (debounced), cancelable on new keystrokes
- Shows suggestions without manual command invocation
- `self_test_gate` has `completion_keystroke_smoke=PASS`

---

## NX7-02 - Ghost text render layer (inline suggestion)

**TOP_50 reference:** AI/Editor: "Inline ghost text + accept/reject" row
**Closed when:**

- Suggestion renders inline at caret position
- Tracks scrolling/wrapping correctly
- `self_test_gate` has `ghost_text_render=PASS`

---

## NX7-03 - Tab accept / Esc reject loop

**TOP_50 reference:** AI/Editor: "Tab accept / Esc reject" row
**Closed when:**

- Tab commits suggestion as an undoable edit
- Esc clears suggestion without modifying buffer
- No regression to editor keybindings
- `self_test_gate` has `ghost_text_accept_reject=PASS`

---

## NX7-04 - Completion context assembly (bounded)

**TOP_50 reference:** Cross-cutting: "Context window management / prompt assembly" row
**Closed when:**

- Context includes local window + symbol scope + recent edits
- Hard caps enforced (bytes/tokens)
- `self_test_gate` has `completion_context_bounds=PASS`

---

## NX7-05 - Provider arbitration (LSP vs local AI)

**TOP_50 reference:** AI/Editor: "Hybrid completion routing" row
**Closed when:**

- Deterministic selection rules
- Timeout fallback (LSP -> AI or AI -> LSP)
- Debug trace "why this completion" available in logs
- `self_test_gate` has `completion_arbitration=PASS`

---

## NX7-06 - Completion caching + invalidation

**TOP_50 reference:** Cross-cutting: "Caching for responsiveness" row
**Closed when:**

- Cache keyed by file-hash/prefix/cursor window
- Invalidation on edits affecting prefix window
- Prevents stale suggestions
- `self_test_gate` has `completion_cache_invalidation=PASS`

---

## NX7-07 - Latency instrumentation (time-to-suggest)

**TOP_50 reference:** Telemetry/Perf: "Performance counters / latency tracking" row
**Closed when:**

- Logs time-to-first-suggestion and total completion latency
- Exportable via perf dump (local)
- `docs/PARITY_VALIDATION_PROOF_PACK.md` latency table has real numbers

---

## Validation artifact for this run

**Required:** `build_validation_next7_run1.txt` containing:

- `HEAD=...`
- `BUILD1/2/3_EXIT=0`
- hashes for `RawrXD-Win32IDE.exe` and (if produced) `RawrXD_Gold.exe`
- list of `self_test_gate` cases executed and PASS/FAIL

---

# Then Run #2 - Extension Host Completeness

(Keep the 7 you already enumerated; copy the same format into `NEXT_7` once Run #1 ships.)

---

## One critical policy note for repo hygiene

When replacing fallback shims:

- Never replace a fallback with `return true;` unless it is a real delegation to a real subsystem and it can fail cleanly.
- A real implementation must have:
- input validation
- deterministic output schema
- measurable side effects or returned results
- test case in `self_test_gate`
