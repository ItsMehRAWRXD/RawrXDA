# RawrXD Competitive Standard — Cursor & VS Code + GitHub Copilot

**Bar:** RawrXD is **not** a basic IDE. Every feature area must **compete with today’s top contenders**: **Cursor** and **VS Code with the GitHub Copilot extension**. No “MVP” or “basic” tier for user-facing behavior; implementation may be phased, but the **target** is always parity or better.

---

## 1. Competitive Bar (What “Non-Basic” Means)

| Contender | Bar to meet or exceed |
|-----------|------------------------|
| **Cursor** | AI-native editing (inline ghost text, Tab accept, Composer, @-mentions, agent in loop), fast completion, chat with codebase, optional local models. |
| **VS Code + GitHub Copilot** | Real-time completions, inline suggestions, Copilot Chat, refactor/explain/fix, extensions, debugging (DAP), diagnostics (squiggles + quick fix), Git UI, testing (explorer, CodeLens), multi-file search/replace, snippets, keybinding customization. |

**Principle:** Any new feature or UX in RawrXD must be designed to **match or exceed** the level of Cursor and VS Code+Copilot in that area. If we ship something “basic,” it must be explicitly scoped as a Phase 1 with a documented path to full parity.

---

## 2. Capability Areas — Minimum Expected Level

These are the **minimum** expectations to be competitive. Details and remediation are in **docs/GAP_VS_CURSOR_VSCODE_COPILOT.md** and **docs/CURSOR_GITHUB_PARITY_SPEC.md**.

### 2.1 Editor & AI (Cursor/Copilot core)

| Capability | Cursor / VS Code + Copilot | RawrXD target |
|------------|----------------------------|----------------|
| **Real-time code completion** | Instant, context-aware; symbol/parameter hints; snippet placeholders | CompletionEngine wired to keystrokes; async popup; IntelliSense-style hints |
| **Inline AI (ghost text)** | Gray inline suggestion; Tab accept / Esc reject | Ghost text fed from completion/LLM; Tab/Esc; single current suggestion |
| **Chat with codebase** | @-files, @-symbols, codebase context | @-mention parsing; context assembly; agent with file/symbol context |
| **Composer / multi-file edit** | Multi-file edits; approve/reject batch; transcript | Agentic Composer UX; batch approve/reject; transcript + metrics |
| **Streaming** | Token-by-token; no “basic” wait-for-full-response | Streaming UX; token streaming; no regression to sync-only |

### 2.2 Diagnostics & Debugging

| Capability | Cursor / VS Code + Copilot | RawrXD target |
|------------|----------------------------|----------------|
| **Diagnostics** | Red/yellow squiggles; quick fixes; multi-linter | LSP (and linter) → editor decorations; squiggles; quick-fix menu |
| **Debugging** | DAP; breakpoints, watches, stack, REPL | DAP or equivalent; breakpoint UI; stack/watch panels; REPL |
| **Hover / docs** | Hover docstrings; param hints; Markdown | LSP hover → tooltip; param hints; Markdown render |

### 2.3 Refactor, Format, Lint

| Capability | Cursor / VS Code + Copilot | RawrXD target |
|------------|----------------------------|----------------|
| **Refactoring** | Rename symbol, extract method/variable, organize includes, etc. | RefactoringPlugin; all IDM_REFACTOR_* wired (see CURSOR_GITHUB_PARITY_SPEC) |
| **Format** | Format on save; format selection; .clang-format / .prettierrc | format_router implemented; Format on save + command; config load |
| **Linting** | Real-time lint; squiggles + quick fix | Linter on save/idle; results → diagnostics; editor squiggles |

### 2.4 Search, Git, Testing

| Capability | Cursor / VS Code + Copilot | RawrXD target |
|------------|----------------------------|----------------|
| **Multi-file search** | Workspace search; regex; replace with preview | Search panel with results; replace preview; regex option |
| **Git UI** | Diff, blame, branch, commit/push, merge | Diff viewer; blame; branch/commit UI |
| **Testing** | Test explorer; Run/Debug test; CodeLens “Run Test” | Test explorer panel; run/debug from UI; results/failures |

### 2.5 Cursor & GitHub parity (spec’d)

All items in **docs/CURSOR_GITHUB_PARITY_SPEC.md** are part of the competitive bar:

- **Telemetry export** — JSON, CSV, Prometheus, OTLP, Audit, Verify, Auto (IDM_TELEXPORT_*).
- **Agentic Composer** — New/End Session, Approve/Reject All, Transcript, Metrics (IDM_COMPOSER_*).
- **@-Mention context** — Parse, Suggest, Assemble, Register custom (IDM_MENTION_*).
- **Vision** — Load image, Paste, Screenshot, Build multimodal payload (IDM_VISION_*).
- **Refactoring** — All 9 refactor commands (IDM_REFACTOR_*).
- **Language registry** — Detect, List, Load plugin, Set for file (IDM_LANG_*).
- **Semantic index** — Build index, Fuzzy search, Find refs, Deps, Type hierarchy, Call graph, Cycles (IDM_SEMANTIC_*).
- **Resource generator** — Generate resource, Project scaffold, List/Search templates (IDM_RESOURCE_*).
- **GitHub** — Read (releases); Write (create release, upload asset) via WinHTTP; no curl.

All Cursor parity modules in `initAllCursorParityModules()`; all command IDs 11500–11574 in `handleCursorParityCommand()`; production-ready, not stubs.

### 2.6 Polish & power-user

| Capability | Cursor / VS Code + Copilot | RawrXD target |
|------------|----------------------------|----------------|
| **Snippets** | Built-in + user; placeholders; Tab to next | Snippet registry; placeholders; Tab navigation |
| **Keybindings** | Custom shortcuts; Vim/Emacs presets | Keybinding config; optional Vim/Emacs presets |
| **Tasks / launch** | tasks.json, launch.json; Run task; Start debugging | Task runner; load tasks/launch config; Run / Debug from IDE |

---

## 3. What RawrXD Already Has (Advantages to Keep)

- **Local LLM / Ollama** — no cloud required; works offline.
- **Agentic engine** — autonomous loops, plan execution, tool registry, multi-step.
- **Streaming** — token streaming; streaming UX; PowerShell bridge for external APIs.
- **MASM / assembly** — first-class MASM; ASM semantics.
- **Hotpatch** — three-layer (Memory, Byte, Server) + proxy; HotpatchPanel.
- **Four-pane layout** — File Explorer, Terminal/Debug, Editor, AI Chat (canon).
- **Marketplace panel** — seed catalog; Extension Host Hijacker.
- **LSP client** — present; needs diagnostics → editor and hover.
- **Reverse engineering** — Disassembly, decompiler, PE, CFG.
- **Swarm / subagent** — Swarm panel; subagent spawn; agent history.
- **Backend switcher / LLM router** — multi-engine; local server.

These must **remain** at or above the bar; no “basic” downgrade.

---

## 4. Priority Order (To Be Competitive)

From **docs/GAP_VS_CURSOR_VSCODE_COPILOT.md** §7, in order:

1. **Wire completion + ghost text** — CompletionEngine → keystrokes; feed ghost text; Tab/Esc. (Unblocks “feels like Copilot”.)
2. **Diagnostics in editor** — LSP/linter → squiggles + quick fixes.
3. **Integrated debugging** — DAP or equivalent; breakpoints, stack, watch.
4. **Refactoring + format** — At least rename symbol + format on save.
5. **Cursor Parity + GitHub** — All IDM_* and GitHub read/write per CURSOR_GITHUB_PARITY_SPEC.
6. **Testing + Git UI** — Test explorer; diff/blame/commit UI.
7. **Snippets + keybindings** — Snippet system; customizable shortcuts.

---

## 5. References

| Doc | Purpose |
|-----|---------|
| **docs/TOP_25_AI_IDE_LSP_EDITOR_FEATURES.md** | Top 25 AI IDE LSP/editor features — Cursor, GitHub Copilot & RawrXD target per feature. |
| **docs/GAP_VS_CURSOR_VSCODE_COPILOT.md** | Detailed gap list (critical / high / medium); actions per gap. |
| **docs/CURSOR_GITHUB_PARITY_SPEC.md** | Cursor & GitHub parity — command IDs, implementation requirements (C++20/MASM). |
| **docs/GAP_REMEDIATION_MANIFEST.md** | Functional workflows (LSP, external API, multi-agent, etc.). |
| **FEATURE_MANIFEST.md** | Feature matrix (Win32, CLI, React, PS). |
| **ARCHITECTURE.md** §6.5 | Four-pane layout canon. |

---

## 6. Non-Basic Checklist (Quick Reference)

When adding or reviewing a feature, ask:

- [ ] Does this **match or exceed** what Cursor does in this area?
- [ ] Does this **match or exceed** what VS Code + Copilot does in this area?
- [ ] If we ship a “Phase 1” subset, is the path to **full parity** documented?
- [ ] Are command IDs and wiring in **CURSOR_GITHUB_PARITY_SPEC** and **GAP** docs reflected?

If the answer is “no” or “basic only,” the feature is **not** at the competitive standard until the gap is closed or explicitly scheduled.

---

**Summary:** RawrXD targets parity with **Cursor** and **VS Code + GitHub Copilot**. Nothing is “basic” by design; implementation may be phased, but the bar is always top-contender level.