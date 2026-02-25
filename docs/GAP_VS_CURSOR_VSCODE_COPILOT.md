# Gap vs Cursor & VS Code + GitHub Copilot — What’s Still Missing

**Purpose:** Single checklist of what RawrXD still needs to be **on par or beyond** Cursor and VS Code with GitHub Copilot. This is the **detailed gap list** for the competitive standard.  
**Bar:** See **docs/COMPETITIVE_STANDARD_CURSOR_VSCODE_COPILOT.md** — RawrXD targets top-contender level; nothing basic.  
**Top 25 features:** **docs/TOP_25_AI_IDE_LSP_EDITOR_FEATURES.md** — Cursor, GitHub Copilot, and the 25 AI IDE LSP/editor features RawrXD must match or exceed.  
**Sources:** `docs/archive/IDE_GAP_ANALYSIS_VS_VSCODE_COPILOT.md`, `docs/CURSOR_GITHUB_PARITY_SPEC.md`, `FEATURE_MANIFEST.md`, `docs/GAP_REMEDIATION_MANIFEST.md`, codebase grep.

---

## 1. CRITICAL (blockers for “same league”)

| Gap | Cursor / VS Code + Copilot | RawrXD today | Action |
|-----|----------------------------|--------------|--------|
| **Real-time code completion** | Instant autocomplete as you type; context-aware; symbol/parameter hints; snippet placeholders. | CompletionEngine exists but **not wired to keystrokes**; no async popup; no IntelliSense-style hints. | Wire CompletionEngine to editor key events; async fetch; show top-N in popup; add placeholder navigation. |
| **Inline AI suggestions (ghost text)** | Gray inline suggestion; Tab accept / Esc reject; multiple suggestions. | Ghost text **renderer exists**; **no hook to feed it AI suggestions**; no accept/reject keybindings. | Connect ghost renderer to completion/LLM output; Tab/Esc handlers; single “current” suggestion. |
| **Integrated debugging** | DAP; breakpoints, watches, stack trace, REPL. | Debug panel/stubs; **no real breakpoint UI**, no DAP adapter, no stack/watch. | Implement or integrate DAP; breakpoint storage + UI; stack/watch panels. |
| **Real-time diagnostics** | Red/yellow squiggles; quick fixes; multi-linter. | No live error display; no squiggles; no quick fixes; LSP client partial. | LSP diagnostics → editor decorations; squiggle rendering; optional quick-fix menu. |

**Summary:** Editor integration is the main gap — “wire existing pieces + add diagnostics/debugging.”

---

## 2. HIGH (expected in a modern AI IDE)

| Gap | Cursor / VS Code + Copilot | RawrXD today | Action |
|-----|----------------------------|--------------|--------|
| **Smart refactoring** | Rename symbol, extract method/variable, organize includes, inline, etc. | No refactoring engine; CURSOR_GITHUB_PARITY_SPEC defines 9 refactor commands (IDM_REFACTOR_*). | Implement RefactoringPlugin (C++20); wire IDM_REFACTOR_* in Cursor Parity menu. |
| **Linting integration** | Real-time lint (ESLint, pylint, clang-tidy); squiggles + quick fix. | analyzeCode tool exists; **no editor squiggles**, no quick fixes. | Run linter on save/idle; map results to LSP/diagnostics; show in editor. |
| **Code formatting** | Format on save; format selection; .clang-format / .prettierrc. | format_router.cpp **empty/stub**; no format keybinding or config load. | Implement format_router (clang-format/Prettier/msvc); Format on save + command. |
| **Testing in IDE** | Test explorer; Run/Debug test; CodeLens “Run Test”. | runTests tool; **no test explorer**, no result UI, no CodeLens. | Test explorer panel; run/debug test from UI; show results/failures. |
| **Multi-file search/replace** | Workspace search; regex; replace preview; preserve case. | multi_file_search.cpp exists but **no UI**; no replace preview. | Search panel with results; replace with preview; optional regex. |
| **Git UI** | Diff viewer, blame, branch switch, commit/push, merge UI. | gitStatus/gitDiff/gitLog in tool registry; **no UI** for diff/blame/branch. | Diff viewer; blame annotations; branch/commit UI (or reuse existing panels). |
| **Documentation on hover** | Hover docstrings; param hints on keypress; Markdown render. | No hover info; no param hints; no docstring parsing. | LSP hover → tooltip; optional param hint trigger; Markdown render for docs. |

---

## 3. MEDIUM (polish and power-user)

| Gap | Cursor / VS Code + Copilot | RawrXD today | Action |
|-----|----------------------------|--------------|--------|
| **Task / launch config** | tasks.json, launch.json; build/run from IDE. | No task config; no launch.json; planning_agent not IDE-wired. | Task runner UI; load tasks/launch config; “Run task” / “Start debugging”. |
| **Snippets** | Built-in + user snippets; placeholders; variables. | Snippet placeholder comment in Win32IDE; **no snippet system**. | Snippet registry; insert with placeholders; Tab to next placeholder. |
| **Keybinding customization** | Custom shortcuts; Vim/Emacs/Sublime presets. | Limited customization; **no Vim mode**. | Keybinding config; optional Vim/Emacs presets. |
| **Extension/plugin system** | Marketplace; VS Code extension API. | Marketplace **panel + seed**; Extension Host Hijacker; **no full VSIX runtime**. | Document “native plugins only” or invest in minimal extension host. |
| **Project/workspace config** | Multi-root workspace; workspace settings. | Single project; no multi-root. | Optional: workspace file + “open folder” semantics. |

---

## 4. CURSOR / GITHUB PARITY (spec’d but not all wired)

From `docs/CURSOR_GITHUB_PARITY_SPEC.md` — **Cursor/JB Parity menu** and **GitHub** features that should be production-ready. Many have command IDs and init hooks; implementation status varies.

| Area | Commands / features | Status |
|------|---------------------|--------|
| **Telemetry export** | IDM_TELEXPORT_* (JSON, CSV, Prometheus, OTLP, Audit, Verify, Auto). | Implement export pipeline; wire to menu. |
| **Agentic Composer UX** | New/End Session, Approve/Reject All, Show Transcript/Metrics. | cmdComposer* exist; ensure full session + batch apply. |
| **@-Mention context** | Parse, Suggest, Assemble context, Register custom. | ContextMentionParser; wire IDM_MENTION_*. |
| **Vision encoder** | Load image, Paste, Screenshot, Build multimodal payload. | Vision encoder; wire IDM_VISION_*. |
| **Refactoring** | Extract method/variable, Rename symbol, Organize includes, etc. | RefactoringPlugin; wire IDM_REFACTOR_*. |
| **Language registry** | Detect, List, Load plugin, Set for file. | LanguagePlugin registry; wire IDM_LANG_*. |
| **Semantic index** | Build index, Fuzzy search, Find refs, Deps, Type hierarchy, Call graph, Cycles. | SemanticIndexEngine; wire IDM_SEMANTIC_*. |
| **Resource generator** | Generate resource, Project scaffold, List/Search templates. | ResourceGeneratorEngine; wire IDM_RESOURCE_*. |
| **GitHub (read)** | Fetch latest release (WinHTTP + manual JSON). | Done in `update_signature.cpp`. |
| **GitHub (write)** | Create release, upload asset (WinHTTP POST + token). | Stub in `release_agent.cpp`; implement when needed. |

**Checklist (from spec):** All Cursor parity modules in `initAllCursorParityModules()`; all command IDs 11500–11574 in `handleCursorParityCommand()`; no curl in update/release path; hot paths C++20 or MASM.

---

## 5. MCP & COMPOSER (present in tree, completeness TBD)

| Feature | RawrXD | Note |
|---------|--------|------|
| **MCP** | `initMCP()`, `MCPServer`, Beacon `mcp.*` / `BeaconKind::MCPServer`. | Ensure MCP server + client are fully usable (list tools, call tools). |
| **Composer** | `cmdGapComposerBegin/Add/Commit/Status`; `AgenticComposerUX`, cmdComposer*. | Multi-file edit session; approve/reject batch; transcript/metrics. |
| **Cursor Parity menu** | `createCursorParityMenu`, `handleCursorParityCommand`, `initAllCursorParityModules`. | All parity commands wired and non-no-op. |

---

## 6. WHAT RAWRXD ALREADY HAS (advantages)

- **Local LLM / Ollama** — no cloud required; works offline.
- **Agentic engine** — autonomous loops, plan execution, tool registry, multi-step.
- **Streaming** — token streaming, streaming UX; PowerShell bridge for external APIs.
- **MASM / assembly** — first-class MASM support; ASM semantics (partial).
- **Hotpatch** — three-layer (Memory, Byte, Server) + proxy; HotpatchPanel.
- **Four-pane layout** — File Explorer, Terminal/Debug, Editor, AI Chat (canon).
- **Marketplace panel** — seed catalog; Extension Host Hijacker entry.
- **LSP client** — exists; needs diagnostics → editor and hover integration.
| **Reverse engineering** — Disassembly, decompiler, PE, CFG, etc.
- **Swarm / subagent** — Swarm panel; subagent spawn; agent history.
- **Chat + queue** — Chat with message queue (follow-ups while streaming).
- **Backend switcher / LLM router** — multi-engine, local server.

---

## 7. PRIORITY ORDER (to be “on par or beyond”)

1. **Wire completion + ghost text** — Connect CompletionEngine to keystrokes; feed ghost text; Tab/Esc. (Unblocks “feels like Copilot”.)
2. **Diagnostics in editor** — LSP (and/or linter) → squiggles + quick fixes.
3. **Integrated debugging** — DAP or equivalent; breakpoints, stack, watch.
4. **Refactoring + format** — At least rename symbol + format on save.
5. **Cursor Parity + GitHub** — All IDM_* and GitHub read/write per spec.
6. **Testing + Git UI** — Test explorer; diff/blame/commit UI.
7. **Snippets + keybindings** — Snippet system; customizable shortcuts.

**Rough effort (from existing gap docs):** ~2–3 weeks for an “acceptable MVP” (Phase 1: completion, ghost text, debugging); ~8–12 weeks for broad parity with VS Code (excluding a full extension ecosystem).

---

**References:** `docs/TOP_25_AI_IDE_LSP_EDITOR_FEATURES.md` (Top 25 Cursor/Copilot/LSP features), `docs/archive/IDE_GAP_ANALYSIS_VS_VSCODE_COPILOT.md`, `docs/CURSOR_GITHUB_PARITY_SPEC.md`, `FEATURE_MANIFEST.md`, `docs/GAP_REMEDIATION_MANIFEST.md`, `ARCHITECTURE.md` §6.5–6.6.
