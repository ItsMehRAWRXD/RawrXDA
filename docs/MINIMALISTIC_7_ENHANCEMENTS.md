# Seven enhancements for every minimalistic feature

**Policy:** Treat work as **modification and extension** of existing, small surfaces—**additive** clarity, persistence, and safety—not wholesale replacement of minimal UX.

**Code is ground truth:** Checklist rows (M01–M07) describe **small modifies** aligned with what the repo **already** does or **thinly** adds. If a row does not match current code, treat it as **doc intent for a future modify** or **omit for that surface**—**do not** use this file to justify a full rewrite, “enterprise-only” redesign, or shipping **non-production** scope. No checklist item implies a separate product line.

Apply **all seven (M01–M07)** to each **minimalistic feature** (single button, one tab, thin panel, one IPC path, one env flag). The **inventory table** lists every current shell surface; the section **M01–M07 for every minimalistic surface** applies the same seven enhancements to each row **by modification** (copy, persistence, states, palette/IPC, logs, safe defaults, verify steps)—not by replacing minimal UX.

Last updated: 2026-03-21.

---

## M01 — Intent in one glance

| Add | Minimal feature gets |
|-----|----------------------|
| **Short label + `title` / tooltip** | What it does in one sentence. |
| **“Does not” line** (tooltip or doc) | Boundaries (e.g. “streams manifest only, not weights”). |

*Modify only:* copy and attributes; no new flows required.

---

## M02 — Persistent where it helps

| Add | Minimal feature gets |
|-----|----------------------|
| **Remember** last sane input | Path, URL, toggle, tab, expanded folder set—**where privacy/size allow**. |
| **Clear reset** | Explicit “Reset” or “defaults” so persistence never feels trapped. |

*Modify only:* `localStorage` / `userData` / existing config merge patterns.

---

## M03 — Empty, loading, error (never silent)

| Add | Minimal feature gets |
|-----|----------------------|
| **Empty state** | One line + **next action** (“Save IRC config, then Start”). |
| **Loading** | Visible in-progress (spinner, phase, `…`). |
| **Error** | Message + **one recovery step** (Refresh, fix setting, check network). |

*Modify only:* status text, toasts, `lastLaunchError`-style fields—not new backends.

---

## M04 — Keyboard & command palette parity

| Add | Minimal feature gets |
|-----|----------------------|
| **Accelerator or palette command** | Documented in palette list or shortcut table. |
| **Same id in menu IPC** | `ide:action` / menu label matches palette label. |

*Modify only:* register existing actions; align names.

---

## M05 — Status line / log correlation

| Add | Minimal feature gets |
|-----|----------------------|
| **After user action** | Status line or ring buffer line names the feature + outcome (`Models: ready`, `[irc] launch failed: …`). |
| **Support grep** | Stable prefix per feature (`[irc]`, `Models:`, `agent:`). |

*Modify only:* `setStatusLine`, `pushIrcLog`, `noisyLog`, structured logs.

---

## M06 — Safe default + documented escape hatch

| Add | Minimal feature gets |
|-----|----------------------|
| **Secure / conservative default** | Short secrets rejected, paths confined to workspace, rate limits on. |
| **One env or policy knob** | Lab-only bypass **named** in doc (`RAWRXD_IRC_ALLOW_WEAK_SECRET`, etc.). |

*Modify only:* document existing gates; add env only if missing and needed.

---

## M07 — Minimal verify path

| Add | Minimal feature gets |
|-----|----------------------|
| **3-step manual check** | In doc: open → click → expect. |
| **Automated hook where cheap** | `npm run test:irc`, `node --check`, or selfcheck script **referenced** from feature doc. |

*Modify only:* docs and package.json scripts list; no obligation to add CI for every row on day one.

---

## Minimalistic feature inventory (extend as you add UI)

| Surface | Likely home |
|---------|-------------|
| Sidebar file browser | `bigdaddyg-ide/src/components/Sidebar.js`, `ProjectContext.js` |
| Toolbar & layout chrome | `Toolbar.js`, `Layout.js`, `App.js` |
| Editor panel | `EditorPanel.js` |
| Terminal | `Terminal.js` |
| Right dock (tab strip, close) | `RightSidebarDock.js` |
| Chat / AI invoke | `ChatPanel.js`, `main.js` `ai:invoke` |
| Agent toggles / approvals | `AgentPanel.js`, orchestrator, `approval_policy.json` |
| Extension modules | `ModulesPanel.js`, `IdeFeaturesContext.js` |
| Symbols & xrefs (MVP) | `SymbolsPanel.js` |
| Model streamer | `ModelsPanel.js`, `ggufStreamingLoader.js` |
| Settings (all tabs) | `SettingsPanel.js` |
| IRC bridge | Settings IRC tab, `irc_bridge.js`, `IRC_MIRC_IDE_BRIDGE.md` |
| Command palette | `CommandPalette.js`, `App.js` `paletteCommands` |
| Status / toasts / noisy feedback | `IdeFeaturesContext.js`, `NoisyLayer.js`, `noisyAudio.js` |
| Inference routing | `INFERENCE_PATH_MATRIX.md`, `providers.js`, `config/providers.json` |
| Enterprise compliance export | `SettingsPanel.js` (General), `electron/main.js` / preload IPC |
| Project knowledge store (IPC) | Preload + `knowledge_store.js`, orchestrator call sites |

---

## M01–M07 for every minimalistic surface

Below, **each surface** gets the **same seven enhancements** (M01–M07), spelled out as *modify-only* additions aligned with **Code is ground truth** (top of this file). Implementation stays thin; this section is the uniform checklist.

### Sidebar file browser

| Code | Add / modify |
|------|----------------|
| **M01** | Row/folder `title`: path + “opens in editor; does not run or install”. |
| **M02** | Expanded folders: `EXPANDED_STORAGE_KEY` in `Sidebar.js`; last project: `ProjectContext`—document clearing those `localStorage` keys for a clean tree (no “Reset layout” unless a real control exists). |
| **M03** | Empty: “Open project…”; loading tree state; error if root unreadable + “Pick another folder”. |
| **M04** | Palette + menu: open/focus sidebar; match `ide:action` ids. |
| **M05** | Status/noisy: `[sidebar] opened file`, `[sidebar] refresh failed: …`. |
| **M06** | Default: no path traversal outside `projectRoot`; document any dev-only folder picker override. |
| **M07** | Doc: open project → click file → editor shows path; optional grep `Sidebar` in UI audit. |

### Toolbar & layout chrome

| Code | Add / modify |
|------|----------------|
| **M01** | Match existing `title` strings in `Toolbar.js` (they already state M01/M06 boundaries)—extend only if a control lacks one. |
| **M02** | Shell prefs/modules live in `IdeFeaturesContext` (`STORAGE_KEY`); toolbar does not add a second store—document `localStorage` key if users need reset. |
| **M03** | If `electronAPI` missing (e.g. `listAIProviders`), dropdown stays empty—toast or status already possible; one-line recovery only. |
| **M04** | Align dock/chat/agent/modules/symbols/models with `paletteCommands` + `onIdeAction` where both exist; provider `<select>` may stay toolbar-only until a palette entry is added. |
| **M05** | Prefer `setStatusLine` / `noisyLog` from parent (`App.js`) after dock toggles—toolbar buttons mostly delegate upward. |
| **M06** | Agent risk is governed by Settings (`agentAutoApprove`, etc.) and `approval_policy.json`—document real keys; do not invent new gates in toolbar alone. |
| **M07** | Launch → click Open project / Chat / Models → observe dock or dialog; matches current wiring. |

### Editor panel

| Code | Add / modify |
|------|----------------|
| **M01** | Match `EditorPanel.js`: tab `title` already states buffer + **no auto-save to disk until Save is wired**. |
| **M02** | Single `activeFile` + cache from `ProjectContext`—no multi-tab “Close all” story unless code adds tabs. |
| **M03** | “No File Open” empty state exists; Monaco `loading=` prop; read errors originate from `read_file` IPC upstream. |
| **M04** | **Today** no Save in `paletteCommands`—add a thin Save only when UI calls `fs:write-file`, or document **buffer-only** honestly. |
| **M05** | Status already `Editor: <name>` on active file—skip `Editor: saved` until a write path exists. |
| **M06** | `fs:write-file` enforces project root in main; in-memory edits in React state do not imply disk writes yet. |
| **M07** | Verify: open file → type → see buffer update; disk round-trip only after explicit save wiring. |

### Terminal

| Code | Add / modify |
|------|----------------|
| **M01** | **Ground truth:** `Terminal.js` uses **node-pty** (main) + **xterm** (renderer). Tooltips: shell picker / **New session** / **Copy cwd path** / **Elevated** buttons state that elevation opens a **separate UAC window**, not piped into the panel. See `bigdaddyg-ide/docs/TERMINAL_PTY.md`. |
| **M02** | Last chosen shell in `localStorage` (`rawrxd.terminal.shell`). |
| **M03** | If `terminal:create` fails (`NO_PTY` / wrong Electron ABI): error strip + `npm run rebuild:pty`; missing project: toasts for copy cwd / cwd fallback to home in main. |
| **M04** | Palette/menu entry to focus terminal **if** registered in `App.js` / `paletteCommands`—until then, bottom panel + toolbar only. |
| **M05** | `setStatusLine` / `noisyLog` on create fail, session start, exit, copy cwd, elevated launch; stable `[terminal]` / status prefixes. |
| **M06** | Embedded shell runs as the **user**; elevated is explicit separate process; footer warns against pasting untrusted commands. |
| **M07** | Verify: `npm install` + `npm run rebuild:pty` (Windows: Python + MSVC for native build) → open project → type in xterm → see shell I/O; **Elevated** → UAC → external window. |

### Right dock (tab strip, close)

| Code | Add / modify |
|------|----------------|
| **M01** | Tab `title`: panel name + “switches view; does not stop running tasks”. |
| **M02** | Last active tab in `IdeFeaturesContext` / storage if already wired; “Close panel” clears without losing inner state (panels stay mounted). |
| **M03** | N/A chrome-only, or “no panel” empty hint in main area when dock closed. |
| **M04** | Ctrl+L / A / M / Y / G + palette entries match `onIdeAction` cases. |
| **M05** | Existing `Panel: …` status line + `noisyLog('dock open', tab)`. |
| **M06** | Default width conservative; doc any max-width env. |
| **M07** | Open each tab → status updates → close ✕ → panel hidden. |

### Chat / AI invoke

| Code | Add / modify |
|------|----------------|
| **M01** | Send/tooltip: “sends prompt to active provider; does not modify approval policy”. |
| **M02** | Persist draft or history if implemented; clear chat action documented. |
| **M03** | Empty thread copy; streaming/loading indicator; API error + “Check provider / model”. |
| **M04** | `Chat: Open AI Chat` palette + `ide:action` `chat`. |
| **M05** | Prefix `chat:` or `[ai]` on invoke/error in status or noisy log. |
| **M06** | Default provider from matrix; document `preferLocalInferenceFirst` and escapes. |
| **M07** | Open chat → send “ping” → see reply or structured error; see `INFERENCE_PATH_MATRIX.md`. |

### Agent toggles / approvals

| Code | Add / modify |
|------|----------------|
| **M01** | Each toggle: what autonomy level does / does not (e.g. “does not bypass approval_policy”). |
| **M02** | Agent toggles persist via `IdeFeaturesContext` settings (`STORAGE_KEY`) like other shell prefs—document that; no separate “policy reset” UI unless added. |
| **M03** | Idle / running / blocked states; approval pending UI + error if orchestrator down. |
| **M04** | `Agent: Open Agent Panel` + palette; menu id `agent`. |
| **M05** | `agent:` lines in log/status for start/approve/cancel. |
| **M06** | `approval_policy.json` conservative default; named knobs in schema + doc. |
| **M07** | Start dry task → see steps → approve/deny path; reference orchestrator selfcheck if any. |

### Extension modules

| Code | Add / modify |
|------|----------------|
| **M01** | Row: module name + “toggles load hook; does not install from network”. |
| **M02** | Remember enabled set in settings; reset restores default module profile. |
| **M03** | Empty list; loading; load error + “See main process log”. |
| **M04** | `View: Extension Modules` + `ide:action` `modules`. |
| **M05** | `[modules] enabled X`, `[modules] failed: …`. |
| **M06** | Only known module ids; document dev-only list override. |
| **M07** | Toggle one module → restart if required → confirm behavior in doc. |

### Symbols & xrefs (MVP)

| Code | Add / modify |
|------|----------------|
| **M01** | “MVP index; does not replace full LSP until wired”. |
| **M02** | Last query or filter if stored; clear resets list. |
| **M03** | No file open / no symbols; indexing…; parser error one-liner. |
| **M04** | `RE: Symbols & xrefs` + `Ctrl+Shift+Y` + `symbols` IPC. |
| **M05** | `[symbols] …` on search/navigation. |
| **M06** | Read-only navigation default; doc any execute jump. |
| **M07** | Open file → open symbols → select entry → cursor moves (or doc current limitation). |

### Model streamer

| Code | Add / modify |
|------|----------------|
| **M01** | “Streams GGUF manifest/metadata; does not download arbitrary URLs without confirm”. |
| **M02** | Last path/URL field; reset clears picker state. |
| **M03** | No model; loading phases; read/parse error + “Check path”. |
| **M04** | `Models: Open GGUF Streamer` + `models` action. |
| **M05** | `Models:` phase lines / `noisyLog` already — keep stable prefix. |
| **M06** | Local file default; document allow-list for remote if added. |
| **M07** | Pick small GGUF → see tensor table → cross-check `docs/GGUF_*` if relevant. |

### Settings (all tabs)

| Code | Add / modify |
|------|----------------|
| **M01** | Per-row tooltip + boundary: shell prefs apply via `setSettings` + `IdeFeaturesContext` persist; IRC fields apply when saved over IPC—state what the code actually does. |
| **M02** | `IdeFeaturesContext` → `localStorage` (`STORAGE_KEY`); IRC via main—**no** global Settings “restore defaults” for the whole app today; Modules panel has its own reset for module flags only. |
| **M03** | Use `generalHint`, IRC status text, and inline copy for failures—no separate validation framework required. |
| **M04** | `Preferences: Open Settings` + `Ctrl+,` + `settings` IPC. |
| **M05** | Toast on open + status line (`App.js`). |
| **M06** | Defaults from `defaultSettings` / `defaultIrcForm` in source; name env vars only where implemented (e.g. license stubs on compliance export). |
| **M07** | Toggle checkbox → reload → value sticks via `STORAGE_KEY`; IRC: save config → refresh status. |

### IRC bridge

| Code | Add / modify |
|------|----------------|
| **M01** | Start/stop: “bridges IDE events to IRC; does not store channel logs in repo by default”. |
| **M02** | Persist host/port/nick (not secrets in clear if avoidable); reset clears bridge config fields. |
| **M03** | Not configured; connecting; connection failed + “Check TLS/port/secret”. |
| **M04** | Palette entry if exposed; align with Settings IRC section labels. |
| **M05** | `[irc]` on launch/stop/error (`pushIrcLog` pattern). |
| **M06** | Weak secret rejection; `RAWRXD_IRC_ALLOW_WEAK_SECRET` or policy knob in `IRC_MIRC_IDE_BRIDGE.md`. |
| **M07** | `npm run` / `node` selfcheck referenced in bridge doc; save config → start → see log line. |

### Command palette

| Code | Add / modify |
|------|----------------|
| **M01** | Footer or first-run hint: “runs registered commands; does not execute shell”. |
| **M02** | Recent commands optional if added; Esc clears. |
| **M03** | No matches empty state; filter loading N/A; broken action → toast. |
| **M04** | `Ctrl+Shift+P` + `command-palette` IPC; labels match `paletteCommands`. |
| **M05** | Status on open + on run: `Command palette — …` / command label. |
| **M06** | Commands are allow-listed functions only; document dev palette registration. |
| **M07** | Open palette → type “Chat” → run → dock opens chat. |

### Status / toasts / noisy feedback

| Code | Add / modify |
|------|----------------|
| **M01** | Noise intensity tooltips: “audio/UI feedback only; does not alter inference routing”. |
| **M02** | `noiseIntensity` / `uiSounds` live in `IdeFeaturesContext` and persist via `STORAGE_KEY`—same as other shell settings; no separate “reset noise” until implemented. |
| **M03** | If audio fails: silent fallback message once. |
| **M04** | Palette link to Settings noise tab if present. |
| **M05** | Centralize prefixes: reuse `noisyLog` categories per feature. |
| **M06** | Default volume not max; document “MAX” as explicit opt-in. |
| **M07** | Toggle intensity → trigger UI sound → confirm audible or logged fallback. |

### Inference routing

| Code | Add / modify |
|------|----------------|
| **M01** | Provider row doc: “routes requests; does not ship model weights”. |
| **M02** | Last selected provider in storage/API; reset via settings + `setActiveAIProvider`. |
| **M03** | No provider; probing; failure + “Install Ollama / check URL”. |
| **M04** | Any palette/provider switcher aligned with `providers.json` ids. |
| **M05** | Consistent tags: `[E03]`, provider name in logs. |
| **M06** | Local-first default when flag set; document remote API keys via env only. |
| **M07** | Switch provider → send chat → confirm path in `INFERENCE_PATH_MATRIX.md`. |

### Enterprise compliance bundle export

| Code | Add / modify |
|------|----------------|
| **M01** | Button `title`: “writes JSON export to userData; does not upload or email”. |
| **M02** | N/A for one-shot export; optional “last export path” in hint only if stored—never secrets. |
| **M03** | Preload missing → inline hint (already); export fail → `generalHint` + one fix (“restart app”, “check disk”). |
| **M04** | Palette command mirroring **Export enterprise compliance bundle** if added; label matches Settings. |
| **M05** | Status/toast: `Compliance: exported …` or `Compliance: failed …`; grep-friendly prefix. |
| **M06** | **Ground truth:** `main.js` writes only under `app.getPath('userData')/exports`—document that; no alternate roots or upload unless code adds them. |
| **M07** | Settings → General → Export → file exists at path shown; cross-check `docs/ENTERPRISE_COMPLIANCE_BUNDLE.md`. |

### Project knowledge store (IPC)

| Code | Add / modify |
|------|----------------|
| **M01** | Call sites: tooltip or code comment “appends structured artifact; does not replace git history”. |
| **M02** | Store grows on disk—document rotation/prune if added; reset = delete store file path in doc only unless UI added. |
| **M03** | IPC failure surfaces: toast or panel line + “See main log”; empty rank returns explained in orchestrator doc. |
| **M04** | If UI exposes “flush/rank”, wire palette id to same IPC as preload names. |
| **M05** | Log prefix `[knowledge]` on append/rank/errors in main or renderer noisy log. |
| **M06** | **Today** `appendArtifact` merges `rec` into fixed fields in `knowledge_store.js`—document that; add size caps / validation in code first, then document—do not claim limits that are not enforced. |
| **M07** | Trigger path that calls `appendKnowledgeArtifact` (e.g. symbols flow) → confirm IPC success / file under `userData/knowledge/` when applicable; see `docs/PROJECT_KNOWLEDGE_STORE.md` for intent, not for features the main process has not shipped. |

---

## How to use this doc

- **Before shipping a small feature:** tick **M01–M07** in the PR description or issue. **Code is ground truth**—do not claim polish the UI does not implement.
- **Audits:** For “minimal” regressions, grep for missing tooltips, missing empty states, or palette drift—**compare to source**, not to imagined enterprise UX.
- **Relation to E/PR backlog:** **`docs/ENHANCEMENT_15_PRODUCT_READY_7.md`** is **capability + ship gates**; this file is **uniform polish** on thin UX—not a second product requirements doc.

---

## Related

- **`docs/ENHANCEMENT_15_PRODUCT_READY_7.md`** — E01–E15, PR01–PR07  
- **`docs/IDE_STRATEGIC_PILLARS.md`** — product themes  
- **`docs/INFERENCE_PATH_MATRIX.md`** — path unification (pairs with M04/M05 for AI surfaces)  
- **`docs/LOCALHOST_COMMUNICATION_MAP.md`** — reverse map: IDE **client** vs optional **bridge** listeners (11434 vs 11435 vs dev UI)  
