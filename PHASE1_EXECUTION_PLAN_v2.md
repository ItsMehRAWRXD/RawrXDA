# RawrXD Phase 1 — Ship-Ready Execution Plan v2
# Target: Lock v1.0 MVP (2-3 days)
# Date: 2026-04-30
# Based on: Full Gap Analysis → Prioritized Execution

---

## Philosophy

**Phase 1 = "It works, it's stable, it's usable."**
**Phase 2 = "It's powerful, it's integrated, it's polished."**

The engine core (context fusion, causality, event system, performance instrumentation) is DONE.
What remains is integration and polish — but we ship the MVP before polishing everything.

---

## Day 1 — Core Interaction Loop (Must-Have for MVP)

### [ ] 1. Ghost Text UX — Accept/Dismiss/Update (3-4 hours)
**Files:** `src/win32app/Win32IDE_GhostText.cpp`, `src/win32app/GhostTextContextSubscriber.cpp`, `src/win32app/Win32IDE_Core.cpp`

**Required behaviors:**
- [ ] **Tab accept**: Wire `VK_TAB` in WndProc → call `m_ghostText->AcceptSuggestion()`
- [ ] **Enter accept**: Wire `VK_RETURN` → accept suggestion (configurable)
- [ ] **Esc dismiss**: Wire `VK_ESCAPE` → call `m_ghostText->DismissSuggestion()`
- [ ] **Typing dismiss**: Any `WM_CHAR` while ghost active → auto-dismiss before char insertion
- [ ] **Visual stability**: Ghost overlay clears before new suggestion renders (no flicker)
- [ ] **No jitter**: Suggestion updates atomically — never show partial/old text

**Edge cases:**
- [ ] Cursor moves mid-suggestion → dismiss, do not re-render at new position
- [ ] Rapid typing (5+ chars/sec) → only latest suggestion renders
- [ ] Multi-line suggestions → render correctly with proper indentation
- [ ] Partial acceptance (optional v1.1) → word-by-word accept with Ctrl+Right

**Exit criteria:**
- Type "hel" → ghost shows "hello" → Tab inserts "hello" → cursor at end
- Type "hel" → ghost shows → Esc clears → no insertion
- Type "hel" → ghost shows → type "p" → ghost clears → "help" in buffer
- No flicker under rapid typing (test: hold key down for 3 seconds)

---

### [ ] 2. Editor Stability — Undo/Redo + Cursor + Large Files (3-4 hours)
**Files:** `src/win32app/Win32IDE_EditorEngine.cpp`, `src/core/EditorEngineFactory.cpp`

**Must-have:**
- [ ] **Undo stack**: `EditorEngine::Undo()` restores previous buffer state + cursor position
- [ ] **Redo stack**: `EditorEngine::Redo()` re-applies undone change
- [ ] **Cursor after ghost accept**: Accepting suggestion updates cursor to end of inserted text
- [ ] **Cursor after undo**: Undo restores cursor to pre-suggestion position
- [ ] **File open/save robustness**: Handle permission denied, disk full, path not found
- [ ] **Large file handling**: Open 10k+ line file → scroll + edit + undo without drift
- [ ] **Text rendering stability**: No flicker under updates (double-buffered if needed)

**Exit criteria:**
- 10+ minutes of editing without cursor corruption
- Undo/redo chain of 20+ operations works correctly
- Large file (10k lines) scrolls and edits without drift
- File save failure shows clear error (not silent fail)

---

### [ ] 3. Active Inference Cancellation (2-3 hours)
**Files:** `src/win32app/ChatPanelModelCaller.cpp`, `src/win32app/GhostTextContextSubscriber.cpp`

**Current state:** Rejects stale frames (good). **Missing:** Actually stops in-flight work.

**Implementation:**
- [ ] **Cancellation token**: Add `std::atomic<bool> m_cancelRequested` to `GhostTextContextSubscriber`
- [ ] **New keystroke cancels old**: `OnContextUpdate()` sets `m_cancelRequested = true` before new request
- [ ] **Thread-safe abort**: `RequestGhostText()` checks flag mid-flight → abort if set
- [ ] **HTTP abort**: Cancel the active HTTP request to Ollama (close connection)
- [ ] **No visual rollback**: Ensure cancelled suggestion never renders
- [ ] **Resource cleanup**: Free memory from cancelled inference immediately

**Exit criteria:**
- Rapid typing (5+ chars/sec) → only latest suggestion renders
- No flicker from cancelled intermediate suggestions
- `stale_frames` increments appropriately (telemetry confirms)
- CPU/GPU usage drops when typing resumes (not wasted on stale requests)

---

## Day 2 — UI Shell + Model Integration (Must-Have for MVP)

### [ ] 4. Basic UI Shell — Status Bar + File Explorer + Menu (4-5 hours)
**Files:** `src/win32app/Win32IDE_Sidebar.cpp`, `src/win32app/Win32IDE_StatusBarActions.cpp`, `src/win32app/Win32IDE_Menu.cpp`

**Minimum UI:**
- [ ] **File explorer panel**: List files in current directory (tree view, minimal)
- [ ] **File open**: Double-click in explorer → `Win32IDE::OpenFile(path)`
- [ ] **File menu**: File → Open, File → Save, File → Save As
- [ ] **Status bar**: Show model connection state (connected/disconnected)
- [ ] **Status bar**: Show last inference latency (from `GhostTextContextSubscriber::GetLastLatencyUs()`)
- [ ] **Status bar**: Show current file name + line/column
- [ ] **Basic menu bar**: File, Edit, View, Help (even if some items are stubs)

**Exit criteria:**
- User can open a file via menu or explorer
- Status bar shows "Model: Connected" or "Model: Disconnected"
- Status bar shows "Latency: 23ms" after inference
- Status bar shows "main.cpp | Ln 15, Col 23"

---

### [ ] 5. Model Selection + Fallback Handling (3-4 hours)
**Files:** `src/win32app/Win32IDE_ModelManager.cpp`, `src/win32app/ChatPanelModelCaller.cpp`

**Connection works — now make it resilient:**
- [ ] **Model dropdown**: Populate from Ollama `/api/tags` endpoint
- [ ] **Default model**: Auto-select first available model on startup
- [ ] **Hard timeout**: 30-second timeout on model calls
- [ ] **Soft timeout**: 5-second "typing feel" timeout → show "thinking..." indicator
- [ ] **Retry logic**: 1 retry on timeout, then mark "Disconnected"
- [ ] **Graceful degradation**: If Ollama unavailable → status bar shows "Disconnected", disable ghost text
- [ ] **Auto-reconnect**: Poll every 5 seconds, re-enable when Ollama returns
- [ ] **Warm model cache**: Keep last-used model loaded (Ollama keeps it warm)

**Exit criteria:**
- First run with Ollama running → auto-connects, ghost text works
- Ollama stopped → status bar shows "Disconnected", no hangs
- Ollama restarted → reconnects on next keystroke
- Timeout shows "Model: Timeout" in status bar, not crash

---

### [ ] 6. Config + Persistence + First-Run (2 hours)
**Files:** `src/config/IDEConfig.cpp`, `src/win32app/Win32IDE_Settings.cpp`

**Current state:** Loads config. **Missing:** Saves preferences, first-run experience.

**Implementation:**
- [ ] **Auto-create config**: If `rawrxd.config.json` missing, create with defaults
- [ ] **Defaults**:
  - Model: `"llama3.2:3b"`
  - Timeout: `30000`
  - Ghost text delay: `150ms`
  - Debounce: `50ms`
  - Window size: `1200x800`
  - Window position: centered
- [ ] **Save on change**: Save config when user changes model, window size, etc.
- [ ] **No crash on invalid config**: Parse errors → log warning, use defaults
- [ ] **First-run experience**: Show "Welcome to RawrXD" dialog with quick setup

**Exit criteria:**
- Delete config → restart → config recreated, works
- Corrupt config → restart → uses defaults, logs warning
- Window size/position persists across restarts
- Model selection persists across restarts

---

## Day 3 — Hardening + Packaging (Must-Have for MVP)

### [ ] 7. Failure Handling + Error Surfaces (2-3 hours)
**Files:** `src/win32app/Win32IDE_ErrorDialog.cpp`, `src/core/ErrorReporter.cpp`

**Current state:** Things "just fail silently." **Fix:** Clear error surfaces.

**Implementation:**
- [ ] **Model failure**: Show "Model error: [message]" in status bar (not silent)
- [ ] **Context invalid**: Log warning, continue with reduced context
- [ ] **GPU unavailable**: Fall back to CPU inference automatically
- [ ] **File I/O errors**: Show dialog "Cannot save file: [reason]"
- [ ] **Network errors**: Show "Cannot connect to Ollama" with retry button
- [ ] **Crash guard**: Top-level exception handler → log crash, show "RawrXD encountered an error"

**Exit criteria:**
- Every failure path has a visible error (UI or log)
- No silent failures
- User knows what went wrong and can retry

---

### [ ] 8. Observability — User-Visible Diagnostics (1-2 hours)
**Files:** `src/win32app/Win32IDE_StatusBarActions.cpp`, `src/core/TelemetrySink.cpp`

**Current state:** Strong telemetry (internal). **Missing:** User-visible signals.

**Implementation:**
- [ ] **Latency indicator**: Status bar shows "Latency: 23ms" (green <50ms, yellow 50-100ms, red >100ms)
- [ ] **Model status**: "Model: Connected" (green) / "Disconnected" (red) / "Loading..." (yellow)
- [ ] **Frame counter**: Optional debug overlay (Ctrl+Shift+D) shows frame_version, stale_rate
- [ ] **Log viewer**: View → Logs opens simple log window (last 100 lines)

**Exit criteria:**
- User can see if model is connected at a glance
- User can see if latency is healthy at a glance
- Debug info available without restarting

---

### [ ] 9. Real-World Stress Testing (2-3 hours)
**Files:** `scripts/stress_test.ps1`, `tests/ide_stress.cpp`

**Synthetic harness is excellent — now validate with real-world scenarios:**
- [ ] **Large file test**: Open 10k+ line file → edit for 5 minutes → no drift
- [ ] **Rapid typing burst**: Script types 100 chars/sec for 30 seconds → no corruption
- [ ] **Long session**: Run IDE for 30+ minutes → check memory growth
- [ ] **Memory stability**: Heap usage should not grow >10% over 30 minutes
- [ ] **Latency drift**: Latency should stay <100ms over 30 minutes

**What to watch:**
| Signal | Healthy | Watch | Bad |
|--------|---------|-------|-----|
| `latency_us` | <50ms | 50-100ms | >100ms |
| `stale_rate` | 0-10 | 10-50 | >100 |
| `frame_version` | Monotonic | — | Decreases |
| Memory growth | <10% | 10-50% | >50% |

**Exit criteria:**
- 30-minute session with no crashes
- Memory growth <10%
- `stale_rate` stays <50 during normal use
- Latency stays <100ms

---

### [ ] 10. Packaging — Single-Folder Deploy (1-2 hours)
**Files:** `README.md`, `bin/`, `scripts/package.ps1`

**Needed for "someone else can run it":**
- [ ] **Copy dependencies**: Ensure `bin/` has exe + required DLLs
- [ ] **No dev deps**: Single folder, no Visual Studio required
- [ ] **README**:
  - How to launch: `RawrXD-Win32IDE.exe`
  - Required: Ollama running on default port
  - First-run setup: none (auto-config)
  - Troubleshooting: "If model doesn't connect, check Ollama is running"
- [ ] **Test on clean machine**: Copy `bin/` folder to another PC, verify launches

**Exit criteria:**
- Another person can run it in <5 minutes with just the README
- No "missing DLL" errors on clean Windows install
- First-run auto-creates config and connects to Ollama

---

## Definition of Done (Phase 1)

You can confidently say:

> "This is a usable AI-powered editor that is stable, responsive, and predictable."

**Checklist:**
- [ ] Open file → edit → save works (including error handling)
- [ ] Ghost text appears, accepts with Tab, dismisses with Esc
- [ ] No flicker or corruption under rapid typing
- [ ] Undo/redo works correctly
- [ ] Model connects automatically, fails gracefully, shows status
- [ ] Status bar shows connection + latency + file info
- [ ] Config auto-creates with sensible defaults, persists changes
- [ ] Every failure has a visible error (no silent failures)
- [ ] 30-minute session stable (memory, latency, no crashes)
- [ ] Another person can run it in <5 minutes
- [ ] `stale_rate` stays <50 during normal use

---

## What NOT to Do in Phase 1 (Deferred to Phase 2)

| Feature | Why Deferred | Phase 2 Priority |
|---------|-------------|------------------|
| Multi-tab | Adds complexity, single buffer is enough for MVP | High |
| LSP integration | Requires external process management | High |
| Streaming responses | Full completion is fine for v1 | Medium |
| Partial accept (word-by-word) | Nice, not required | Medium |
| Theme support | Default theme is fine | Low |
| Auto-update | Manual download for v1 | Low |
| Crash reporting | Log files are enough for v1 | Medium |
| Project-level context | Single file context is enough for MVP | High |
| Symbol/index awareness | Requires LSP first | High |
| Multi-buffer awareness | Single buffer for MVP | Medium |
| Keybind config | Hardcoded keybinds for v1 | Low |
| Window layout persistence | Basic position/size is enough | Low |
| Installer | ZIP file is enough for v1 | Low |

---

## Phase 2 — Advanced IDE Roadmap (Post-MVP)

### P2.1: Multi-Tab + Project Awareness
- Multi-tab editor with buffer switching
- Project-level context (open files, project structure)
- Recent files list

### P2.2: LSP Integration
- Language server protocol client
- Diagnostics integration into ghost text context
- Symbol index (functions, classes) for context fusion
- Go-to-definition, find references

### P2.3: Streaming + Partial Accept
- Streaming responses from Ollama
- Word-by-word partial acceptance
- Real-time suggestion updates as tokens arrive

### P2.4: Advanced UI
- Theme support (dark/light/custom)
- Configurable keybinds
- Window layout persistence (pane positions)
- Split editor view

### P2.5: Performance + Scale
- 100k+ line file support
- Multi-model routing (auto-select best model for task)
- Model preloading on idle
- GPU memory management

### P2.6: Collaboration
- Multi-user editing (optional)
- Git integration
- Code review workflow

---

## Execution Order (Shortest Path to "Done")

Focus in this exact order:

1. **Ghost text UX** (accept/dismiss cleanly) — highest impact, most visible
2. **Editor stability** (undo/redo, cursor correctness) — foundation everything sits on
3. **Active cancellation** (stop in-flight work) — performance under load
4. **Basic UI shell** (explorer + status bar) — makes it feel like an IDE
5. **Model selection + fallback** (resilient connection) — no hangs, graceful degradation
6. **Config + persistence** (first-run experience) — user can customize
7. **Failure handling** (clear errors) — no silent failures
8. **Observability** (user-visible diagnostics) — user knows what's happening
9. **Stress testing** (30-minute stability) — prove it's ready
10. **Packaging** (single-folder deploy) — someone else can run it

---

## Next Step

Pick **Item 1 (Ghost Text UX)** and start. It's the highest-impact, most visible feature.

**Files to edit first:**
- `src/win32app/Win32IDE_Core.cpp` — WndProc for Tab/Esc/typing
- `src/win32app/Win32IDE_GhostText.cpp` — Accept/Dismiss methods
- `src/win32app/GhostTextContextSubscriber.cpp` — Typing cancellation
