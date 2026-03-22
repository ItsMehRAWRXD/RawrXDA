# IDE master progress tracker

Cross-cutting backlog and **measured** doc-derived counts. Use this alongside category-specific lists (sovereign batches, bridge audit). **North-star themes (7 pillars):** `docs/IDE_STRATEGIC_PILLARS.md`. **Partial → ship:** `docs/ENHANCEMENT_15_PRODUCT_READY_7.md` (E01–E15, PR01–PR07).

Last updated: 2026-03-20 (batch 4 UI/remediation).

---

## Your headline estimate (~200 items, ~63 done)

If your personal “200 tasks” list lives outside the repo, keep the **63** count there and bump this file when you reconcile. Below are **repo-measured** slices you can add to that total.

| Slice | Items (rows) | Done (approx.) | File |
|-------|----------------|----------------|------|
| Sovereign batch 1 | 15 | 15 | `docs/TASK_BACKLOG_SOVEREIGN_BATCH1.md` |
| Sovereign batch 2 | 15 (items 16–30) | 3 (16, 19, 24) | `docs/TASK_BACKLOG_SOVEREIGN_BATCH2.md` |
| Bridge / gap audit (actionable themes) | 12 (P0–P2 letters) | P0 **A** wiring improved 2026-03-20; B–L open or partial | `docs/BRIDGE_GAP_AUDIT.md` |

**Suggested reconciliation:** `63 = sovereign_done + bridge_done + other_lists`. Update the first column of your external spreadsheet to match the “Slice” rows here so numbers stay auditable.

---

## Full IDE re-audit (2026-03-20) — connection status

| Area | Status | Notes |
|------|--------|--------|
| **OrchestratorBridge ↔ Win32 IDE** | **Connected** | Single entry `ensureOrchestratorBridgeInitialized()` from live `Win32IDE_Core.cpp` path + settings + GGUF refresh. |
| **OrchestratorBridge ↔ Backend switcher** | **Connected** | `pushOllamaBackendToOrchestratorBridge` / `syncOllamaBackendFromOrchestratorBridge` unchanged; orchestrator helper runs after backend manager init. |
| **Win32IDEBridge** | **Decoupled (Ollama)** | Does not start `OrchestratorBridge`; IDE owns that. **Note:** `Win32IDEBridge::initialize()` is not called from `Win32IDE_Core` in the current tree — full bridge subsystems stay inactive unless another entrypoint calls it. |
| **`Win32IDE_Window.cpp`** | **Orphan TU** | Not in root CMake Win32IDE list — do not assume its `WM_APP+200` path runs in production. |
| **Command routing** | **Intentional legacy-first** | See `docs/COMMAND_DISPATCH_BRIDGE.md` — inverting order is unsafe until SSOT handlers stop re-posting same command IDs. |
| **Swarm** | **Partial** | `RawrXD_SwarmFacade` + IAT lane vs `subagent_core` / CLI — still multiple entrypoints; façade reduces menu vs IAT drift, not a single process-wide API yet. |
| **Inference matrix** | **Partial** | `docs/INFERENCE_PATH_MATRIX.md` maps major actions → lanes; exhaustive menu-ID coverage still P0 **D** in bridge audit. |

---

## Next verifications (short list)

1. **Build** `RawrXD-Win32IDE` after this change set; confirm no duplicate `Win32IDE::onCreate` if `Win32IDE_Window.cpp` is ever added to CMake.
2. **Runtime:** set `ollama.baseUrl` in `rawrxd.config.json`, launch IDE, confirm OrchestratorBridge reaches Ollama without opening Backend panel first.
3. **Optional:** merge or delete orphan `Win32IDE_Window.cpp` to stop confusion.

---

## Canonical session summary (OrchestratorBridge — 2026-03-20)

**Bug:** `ensureOrchestratorBridgeInitialized()` only existed in `Win32IDE_Window.cpp`, which is **not** linked in the root `CMakeLists.txt` Win32IDE target. The shipped binary uses `Win32IDE_Core.cpp` for `WindowProc` / `onCreate` / `handleMessage`, so orchestrator alignment did not run on the production path. `initBackendManager()`’s push/sync alone could miss `m_ollamaBaseUrl` from `rawrxd.config.json` when the Ollama backend row was still empty. **(Historical)** `Win32IDEBridge::initialize()` used to cold-start `127.0.0.1:11434` for `OrchestratorBridge` — that was removed; IDE owns Ollama lifecycle only via `ensureOrchestratorBridgeInitialized()`.

**Fix:** Harden `ensureOrchestratorBridgeInitialized()` (working dir, `push`/`sync` when backend manager is up, `ApplyIdeOllamaSettings` when already initialized). Call it from **`Win32IDE_Core.cpp` `onCreate`**, **`applySettings()`**, and **`handleGgufBackgroundLoadMessage`** after `initBackendManager` / `initLLMRouter`. Remove `OrchestratorBridge::Initialize` from `Win32IDEBridge`. Banner on orphan `Win32IDE_Window.cpp`. **Not done (by design for now):** `Win32IDEBridge::initialize()` is still **not** called from `Win32IDE_Core` — wiring that agentic bridge (capabilities / `preprocessMessage`) is a **separate** product decision; see `docs/BRIDGE_GAP_AUDIT.md` §5.

**Docs:** `docs/BRIDGE_GAP_AUDIT.md` (P0 A), this file.

**Git:** If `Win32IDE.cpp` / `Win32IDE_Core.cpp` have large unrelated edits, use `git add -p` (or a clean branch) so the commit only contains the orchestrator call sites and helper.

**Clangd:** Spurious `~L730` parse errors on `Win32IDE.cpp` were reported in one session; the menu code around that line is normal wide-string `AppendMenuW` calls—re-index or ignore if the file compiles.

**Open:** P0 B–D, P1–P2 per `docs/BRIDGE_GAP_AUDIT.md`; inference matrix doc optional.

---

## Batch 4 of N — UI / diagnostics remediation (2026-03-20)

1. **EditorOperations:** linear undo with redo truncation; no re-entrant mutex in `ReplaceText`/`Undo`/`Redo`; `Cut` uses one lock + clipboard (no `Copy` deadlock); multi-line cut clears undo stack.
2. **RouterOperations:** Vista+ **IFileOpenDialog** / **IFileSaveDialog** with UTF-8 result paths; optional UTF-8 initial path for save via `SHCreateItemFromParsingName`.
3. **Win32IDE_SwarmModelSelector:** real `enumerateSwarmModelCandidates()` + `Win32IDE_SwarmModelSelector.h`; wired in `CMakeLists.txt` for `RawrXD-Win32IDE`.
4. **WindowManager::Initialize:** explicit `IDELogger` WARNING (secondary shell vs `Win32IDE_Core`).
5. **`.github/workflows/ci.yml`:** removed conflict markers; added `master` to branch filters.
6. **Docs:** `docs/FULL_SOURCE_DIAGNOSTICS_AUDIT_2026-03-20.md` remediation table updated.

**Verify:** MSVC build `RawrXD-Win32IDE`; smoke: open/save/folder dialogs on a UTF-8 path; editor undo/redo after replace.
