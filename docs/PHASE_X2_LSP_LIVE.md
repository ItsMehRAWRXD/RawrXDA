# Phase X+2: LSP Live

**Goal:** Clangd (or pyright/ts-server) connected; diagnostics appear in the editor when you open a file.

---

## What’s wired

1. **Start LSP** — `startLSPServer(LSPLanguage::Cpp)` (menu: Tools → LSP → Start All, or command `IDM_LSP_START_ALL`). Starts clangd on PATH (stdio).
2. **Open file** — `openFile(path)` now sends `textDocument/didOpen` to the running LSP for that file’s language (C/C++, Python, TS). So: start clangd → open a `.cpp` file → LSP receives didOpen.
3. **Diagnostics** — LSP client’s reader thread receives `textDocument/publishDiagnostics` → `onDiagnosticsReceived` → `displayDiagnosticsAsAnnotations(uri)` → annotations (squiggles) on the editor for the **current** file only.
4. **Annotations** — `Win32IDE_Annotations.cpp`: LSP diagnostics are stored as annotations with source `"lsp"`; severity maps from LSP (1=Error, 2=Warning, 3/4=Info/Hint).

So the path is: **Start LSP (menu) → Open a .cpp file → didOpen sent → clangd responds with publishDiagnostics → annotations appear.**

---

## How to validate (manual)

1. Build and run the C++ IDE (`RawrXD-Win32IDE.exe`).
2. Start clangd: **Tools → LSP → Start All** (or equivalent; ensure clangd is on PATH).
3. Open a C/C++ file (e.g. any `.cpp` in the repo).
4. Confirm: Output panel may show LSP messages; after a short delay, diagnostics (if any) should appear as annotations in the editor (e.g. red underline / message for errors).

If clangd is not on PATH, LSP start will fail with a message; install clangd or add it to PATH.

---

## LSP features (C++ IDE)

| Feature | Shortcut / Menu | Notes |
|---------|-----------------|-------|
| Go to Definition | F12, Tools then LSP then Go to Definition | Opens target file and moves cursor. |
| Find References | Shift+F12, Tools then LSP then Find All References | Lists references in Output. |
| Rename Symbol | Tools then LSP then Rename Symbol | Enter new name in chat input, then run command. |
| Hover Info | Tools then LSP then Hover Info | Shows hover text in Output. |
| Signature Help | Tools then LSP then Signature Help | Shows current signature in status bar. |
| Quick Fix | Ctrl+., Tools then LSP then Quick Fix | Applies first code action at cursor. |
| Highlight References | Tools then LSP then Highlight References in File | Marks same-symbol refs; Clear Highlight removes. |
| Format Document | Shift+Alt+F, Edit then Format Document | LSP textDocument/formatting. |
| Format on Save | Edit then Format on Save (toggle) | Persisted in settings. |
| Diagnostics | (automatic) | Painted as annotations in editor. |

All of the above are also in the command palette (Ctrl+Shift+P).

---

## Auto-start LSP on first open (implemented)

When you open a file whose language has an LSP configured (e.g. C/C++, Python, TS), the IDE starts that language server if it is not already running, then sends textDocument/didOpen. To make “open file → diagnostics” work without a manual Start:

- In `openFile`, when `detectLanguageForFile(filePath) != Count` and `getLSPServerState(lang) != Running`, call `startLSPServer(lang)` then `sendDidOpen(...)`.  
- Consider debouncing or starting in the background so the UI doesn’t block.

This is optional; the foundation for “LSP live” is in place with manual Start + open file.

---

## Files touched (Phase X+2)

| File | Change |
|------|--------|
| `Win32IDE.cpp` | In `openFile`, after loading content: if LSP for file’s language is running, call `sendDidOpen(lang, uri, langId, content)`. |
| `Win32IDE_LSPClient.cpp` | (existing) `startLSPServer`, `sendDidOpen`, reader thread handles `publishDiagnostics` → `onDiagnosticsReceived` → `displayDiagnosticsAsAnnotations`. |
| `Win32IDE_Annotations.cpp` | (existing) Annotation layer; LSP diagnostics shown as annotations. |

---

## Next

- **Phase X+3:** Agent loop (e.g. `/agent` command) — see `docs/PHASE_X3_AGENT_LOOP.md`.
- **Phase X+5:** Distributed swarm — multi-GPU inference; see `docs/FOUNDATION_STATUS.md`.
