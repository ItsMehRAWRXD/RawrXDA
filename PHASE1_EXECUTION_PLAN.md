# RawrXD Phase 1 — Ship-Ready Execution Plan
# Target: 1-2 days to lock v1.0 MVP
# Date: 2026-04-30
# Status: NOT STARTED → IN-PROGRESS → DONE tracking below

## Day 1 — Core Stability (Morning + Afternoon)

### [ ] 1. Ghost Text UX — Accept/Dismiss (2-3 hours)
**Files:** `src/win32app/Win32IDE_GhostText.cpp`, `src/win32app/GhostTextContextSubscriber.cpp`

- [ ] **Tab accept**: Wire `VK_TAB` in `Win32IDE_Core.cpp` WndProc → call `m_ghostText->AcceptSuggestion()`
- [ ] **Esc dismiss**: Wire `VK_ESCAPE` → call `m_ghostText->DismissSuggestion()`
- [ ] **Typing dismiss**: Any `WM_CHAR` while ghost active → auto-dismiss before char insertion
- [ ] **Visual stability**: Ensure ghost text overlay clears before new suggestion renders (no flicker)
- [ ] **Edge case**: Cursor moves mid-suggestion → dismiss, do not re-render at new position

**Exit criteria:**
- Type "hel" → ghost shows "hello" → Tab inserts "hello" → cursor at end
- Type "hel" → ghost shows → Esc clears → no insertion
- Type "hel" → ghost shows → type "p" → ghost clears → "help" in buffer

---

### [ ] 2. Editor Stability — Undo/Redo + Cursor (2-3 hours)
**Files:** `src/win32app/Win32IDE_EditorEngine.cpp`, `src/core/EditorEngineFactory.cpp`

- [ ] **Undo stack**: Verify `EditorEngine::Undo()` restores previous buffer state + cursor position
- [ ] **Redo stack**: Verify `EditorEngine::Redo()` re-applies undone change
- [ ] **Cursor correctness after ghost accept**: Accepting suggestion updates cursor to end of inserted text
- [ ] **Cursor correctness after undo**: Undo restores cursor to pre-suggestion position
- [ ] **Large file test**: Open 10k line file → verify no drift after scroll + edit + undo

**Exit criteria:**
- 10+ minutes of editing without cursor corruption
- Undo/redo chain of 20+ operations works correctly
- Large file (10k lines) scrolls and edits without drift

---

### [ ] 3. Cancellation of Inference (1-2 hours)
**Files:** `src/win32app/ChatPanelModelCaller.cpp`, `src/win32app/GhostTextContextSubscriber.cpp`

- [ ] **Soft cancellation flag**: Add `std::atomic<bool> m_cancelRequested` to `GhostTextContextSubscriber`
- [ ] **New keystroke cancels old**: `OnContextUpdate()` sets `m_cancelRequested = true` before new request
- [ ] **Old result discarded**: `RequestGhostText()` checks flag mid-flight → abort if set
- [ ] **No visual rollback**: Ensure cancelled suggestion never renders

**Exit criteria:**
- Rapid typing (5+ chars/sec) → only latest suggestion renders
- No flicker from cancelled intermediate suggestions
- `stale_frames` increments appropriately (telemetry confirms)

---

## Day 1 Evening / Day 2 Morning — UI Shell

### [ ] 4. Basic UI Shell — Status Bar + File Explorer (3-4 hours)
**Files:** `src/win32app/Win32IDE_Sidebar.cpp`, `src/win32app/Win32IDE_StatusBarActions.cpp`

- [ ] **Status bar**: Show model connection state (connected/disconnected)
- [ ] **Status bar**: Show last inference latency (from `GhostTextContextSubscriber::GetLastLatencyUs()`)
- [ ] **File explorer panel**: List files in current directory (tree view, minimal)
- [ ] **File open**: Double-click in explorer → `Win32IDE::OpenFile(path)`
- [ ] **Menu**: File → Open, File → Save (wire to existing `Win32IDE_FileOps.cpp`)

**Exit criteria:**
- User can open a file via menu or explorer
- Status bar shows "Model: Connected" or "Model: Disconnected"
- Status bar shows "Latency: 23ms" after inference

---

## Day 2 — Model Integration + Polish

### [ ] 5. Model Selection + Fallback Handling (2-3 hours)
**Files:** `src/win32app/Win32IDE_ModelManager.cpp`, `src/win32app/ChatPanelModelCaller.cpp`

- [ ] **Model dropdown**: Populate from Ollama `/api/tags` endpoint
- [ ] **Default model**: Auto-select first available model on startup
- [ ] **Timeout**: 30-second hard timeout on model calls
- [ ] **Retry**: 1 retry on timeout, then mark "Disconnected"
- [ ] **Graceful fallback**: If Ollama unavailable → status bar shows "Model: Disconnected", disable ghost text

**Exit criteria:**
- First run with Ollama running → auto-connects, ghost text works
- Ollama stopped → status bar shows "Disconnected", no hangs
- Ollama restarted → reconnects on next keystroke

---

### [ ] 6. Config + Defaults (1 hour)
**Files:** `src/config/IDEConfig.cpp`, `src/win32app/Win32IDE_Settings.cpp`

- [ ] **Auto-create config**: If `rawrxd.config.json` missing, create with defaults
- [ ] **Defaults**:
  - Model: `"llama3.2:3b"`
  - Timeout: `30000`
  - Ghost text delay: `150ms`
  - Debounce: `50ms`
- [ ] **No crash on invalid config**: Parse errors → log warning, use defaults

**Exit criteria:**
- Delete config → restart → config recreated, works
- Corrupt config → restart → uses defaults, logs warning

---

### [ ] 7. Packaging — Single Folder (1 hour)
**Files:** `README.md`, `bin/`

- [ ] **Copy dependencies**: Ensure `bin/` has exe + required DLLs
- [ ] **README**:
  - How to launch: `RawrXD-Win32IDE.exe`
  - Required: Ollama running on default port
  - First-run setup: none (auto-config)
- [ ] **Test on clean machine**: Copy `bin/` folder to another PC, verify launches

**Exit criteria:**
- Another person can run it in <5 minutes with just the README

---

## Telemetry Validation During Development

While implementing, watch these telemetry signals:

| Signal | Healthy | Watch | Bad |
|--------|---------|-------|-----|
| `latency_us` | <50ms | 50-100ms | >100ms |
| `stale_rate` | 0-10 | 10-50 | >100 |
| `frame_version` | Monotonic | — | Decreases |

**Log format to expect:**
```
[GhostText] frame_version=42 latency_us=28471 cursor=(15,23) file=main.cpp lang=cpp stale_frames=0 total_frames=120 stale_rate=0
```

---

## Definition of Done (Phase 1)

You can confidently say:

> "This is a usable AI-powered editor that is stable, responsive, and predictable."

**Checklist:**
- [ ] Open file → edit → save works
- [ ] Ghost text appears, accepts with Tab, dismisses with Esc
- [ ] No flicker or corruption under rapid typing
- [ ] Undo/redo works correctly
- [ ] Model connects automatically, fails gracefully
- [ ] Status bar shows connection + latency
- [ ] Config auto-creates with sensible defaults
- [ ] Another person can run it in <5 minutes
- [ ] `stale_rate` stays <50 during normal use

---

## What NOT to Do in Phase 1

| Feature | Why Deferred |
|---------|-------------|
| Multi-tab | Adds complexity, single buffer is enough for MVP |
| LSP integration | Requires external process management |
| Streaming responses | Full completion is fine for v1 |
| Partial accept | Word-by-word is nice, not required |
| Theme support | Default theme is fine |
| Auto-update | Manual download for v1 |
| Crash reporting | Log files are enough |

---

## Next Step

Pick **Item 1 (Ghost Text UX)** and start. It's the highest-impact, most visible feature.
