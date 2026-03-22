# Shell unstub batches (non-AI chrome + dock around chat)

Work proceeds in **batches of 7** until the list is done. **AI chat/agent/providers** are out of scope here unless noted.

**Pattern:** stub-like caches and IPC facades → lexical closure — see **`STUB_TO_CLOSURE.md`**.

## Batch 1 — Symbols text index + API surface (**Done** — 2026-03-21)

| # | Item | Action | Done |
|---|------|--------|------|
| 1 | `textSymbolIndex.js` | New util: extract `function` / `class` / `const` / `interface` / etc. from editor text → `{ name, kind, line }[]`. | Yes |
| 2 | `SymbolsPanel.js` | Accept `activeFile`; primary **Index active file**; accurate copy (text index, not binary disasm). | Yes |
| 3 | `RightSidebarDock.js` | Pass `activeFile` into `SymbolsPanel`. | Yes |
| 4 | `useElectron.js` | Expose preload APIs used by shell (`terminal*`, `setMenuAccelerators`, compliance, etc.). | Yes |
| 5 | `App.js` | Palette label + remove stale “Terminal placeholder” comment if wrong. | Yes |
| 6 | `Toolbar.js` + `electron/menu.js` | Symbols labels: no “MVP stub” where behavior is real. | Yes |
| 7 | `EditorPanel.js` | Replace noisy ghost `TODO:` snippet with neutral completion hint. | Yes |

## Batch 2 — Command palette gate + shell polish (**Done** — 2026-03-21)

| # | Item | Notes | Done |
|---|------|--------|------|
| 1 | `ModulesPanel.js` | Command palette toggle documented; gates shortcut, toolbar, menu ide:action, palette UI. | Yes |
| 2 | `Sidebar.js` | Root `readDir` failure shows real IPC error (`rootListingError`); empty folder copy tightened. | Yes |
| 3 | `CommandPalette.js` | Respects `modules.commandPalette`; `requiresModule` filter hook for future commands. | Yes |
| 4 | `NoisyLayer.js` | Default status line uses persisted shortcuts (first 3 + Settings › Keyboard hint). | Yes |
| 5 | `SettingsPanel.js` | Compliance export tooltip: optional / dev placeholder fields (not “stub fields”). | Yes |
| 6 | `ModelsPanel.js` | GGUF warnings (tensor span vs weight region) surfaced in toast/status; richer parse errors. | Yes |
| 7 | `preload.js` / `main.js` | Comment table: production IPC vs lab (IRC, knowledge, compliance export, Ollama probe). | Yes |

## Batch 3 — (next; not started)

| # | Item | Notes |
|---|------|--------|
| 1 | `Terminal.js` | Persist last cwd / session label in `userData` (optional) when PTY reports cwd. |
| 2 | `ModulesPanel` + `gitIntegration` | When on, show “Git context: workspace root” + `git rev-parse` via new thin IPC or honest “not wired” removed. |
| 3 | `lspBridge` module | Palette command “LSP: ping” → IPC no-op with success toast if preload exposes placeholder handler. |
| 4 | `testRunner` module | Palette: “Tests: open workspace folder in explorer” (honest) or wire to `npm test` dry-run IPC stub. |
| 5 | `telemetryOptIn` | Document in Modules + Settings that no backend is connected when false; strip misleading “analytics” if none. |
| 6 | `EditorPanel` | Wire `inlineCompletion` module to actually toggle Monaco ghost text path (if present). |
| 7 | `RightSidebarDock` | `aria-label` / doc string batch: remove “AI-only” where Modules/Models are not AI. |

---

**Related:** `docs/RE_MVP_SYMBOLS_XREFS.md`, `docs/ELECTRON_WASM_CHAT.md`, `docs/TERMINAL_PTY.md`.
