# Top 25 AI IDE LSP/Editor Features — Cursor, GitHub Copilot & RawrXD

**Purpose:** Single checklist of the **top 25** AI IDE, LSP, and editor capabilities that RawrXD must match or exceed. Contenders: **Cursor** and **VS Code + GitHub Copilot**.

**Bar:** See **docs/COMPETITIVE_STANDARD_CURSOR_VSCODE_COPILOT.md**. RawrXD targets parity or better in every row.

### Quick list (25 features)

1. Real-time code completion  
2. Inline AI (ghost text)  
3. Chat with codebase  
4. Composer / multi-file edit  
5. Streaming responses  
6. LSP diagnostics (squiggles)  
7. Hover / documentation  
8. Integrated debugging (DAP)  
9. Smart refactoring  
10. Code formatting  
11. Linting integration  
12. Multi-file search/replace  
13. Git UI  
14. Testing in IDE  
15. @-Mention context  
16. Agent / Composer session  
17. Vision / multimodal  
18. Semantic index / codebase  
19. Snippets  
20. Keybinding customization  
21. Tasks / launch config  
22. LSP server control  
23. Telemetry / export  
24. Language / plugin registry  
25. Resource / project scaffold  

---

## Contenders (canonical bar)

| Contender | Description |
|-----------|--------------|
| **Cursor** | AI-native editor: inline ghost text, Tab accept, Composer, @-mentions, agent in loop, chat with codebase, optional local models, fast completion. |
| **GitHub Copilot** | VS Code extension: real-time completions, inline suggestions, Copilot Chat, refactor/explain/fix, plus full VS Code (DAP, LSP diagnostics, Git UI, testing, search, snippets, keybindings). |

---

## Top 25 AI IDE LSP/Editor Features

| # | Feature | Cursor | GitHub Copilot / VS Code | RawrXD target |
|---|---------|--------|--------------------------|---------------|
| 1 | **Real-time code completion** | Instant, context-aware; symbol/param hints | Same; IntelliSense-style popup; snippet placeholders | CompletionEngine → keystrokes; async popup; IntelliSense-style hints |
| 2 | **Inline AI (ghost text)** | Gray inline suggestion; Tab accept / Esc reject | Same; single or multiple suggestions | Ghost text from completion/LLM; Tab/Esc; single current suggestion |
| 3 | **Chat with codebase** | @-files, @-symbols; codebase context in chat | Copilot Chat; file/symbol context | @-mention parsing; context assembly; agent with file/symbol context |
| 4 | **Composer / multi-file edit** | Multi-file edits; approve/reject batch; transcript | Multi-file apply; diff preview | Agentic Composer UX; batch approve/reject; transcript + metrics |
| 5 | **Streaming responses** | Token-by-token; no wait-for-full | Same for chat/completions | Streaming UX; token streaming; no sync-only regression |
| 6 | **LSP diagnostics (squiggles)** | Red/yellow squiggles; quick fixes | Same; multi-linter | LSP/linter → editor decorations; squiggles; quick-fix menu |
| 7 | **Hover / documentation** | Hover docstrings; param hints; Markdown | Same | LSP hover → tooltip; param hints; Markdown render |
| 8 | **Integrated debugging (DAP)** | Breakpoints, watches, stack, REPL | Full DAP; run/debug from UI | DAP or equivalent; breakpoint UI; stack/watch panels; REPL |
| 9 | **Smart refactoring** | Rename symbol; extract method/variable; organize includes | Same; inline, move, etc. | RefactoringPlugin; all IDM_REFACTOR_* wired |
| 10 | **Code formatting** | Format on save; format selection; .clang-format / .prettierrc | Same | format_router; Format on save + command; config load |
| 11 | **Linting integration** | Real-time lint; squiggles + quick fix | ESLint, pylint, clang-tidy, etc. | Linter on save/idle; results → diagnostics; editor squiggles |
| 12 | **Multi-file search/replace** | Workspace search; regex; replace preview | Same; preserve case | Search panel; results; replace with preview; regex option |
| 13 | **Git UI** | Diff, blame, branch, commit/push, merge | Same | Diff viewer; blame; branch/commit UI |
| 14 | **Testing in IDE** | Test explorer; Run/Debug test; CodeLens “Run Test” | Same | Test explorer panel; run/debug from UI; results/failures |
| 15 | **@-Mention context** | @file, @symbol, @folder, custom | Copilot Chat @-mentions | Parse, suggest, assemble context; register custom (IDM_MENTION_*) |
| 16 | **Agent / Composer session** | New/End session; Approve/Reject All; transcript | Multi-turn chat; apply changes | Agentic Composer; cmdComposer*; transcript + metrics (IDM_COMPOSER_*) |
| 17 | **Vision / multimodal** | Paste image; screenshot; image in prompt | Copilot with images (where supported) | Load image, Paste, Screenshot; build multimodal payload (IDM_VISION_*) |
| 18 | **Semantic index / codebase** | Fuzzy search; find refs; deps; type hierarchy; call graph | Symbol search; references; outline | Build index; fuzzy search; find refs, deps, type hierarchy, call graph, cycles (IDM_SEMANTIC_*) |
| 19 | **Snippets** | Built-in + user; placeholders; Tab to next | Same | Snippet registry; placeholders; Tab navigation |
| 20 | **Keybinding customization** | Custom shortcuts; Vim/Emacs presets | Same | Keybinding config; optional Vim/Emacs presets |
| 21 | **Tasks / launch config** | tasks.json, launch.json; Run task; Start debugging | Same | Task runner; load tasks/launch; Run / Debug from IDE |
| 22 | **LSP server control** | Start/stop LSP; reindex; publish diagnostics | VS Code language extensions | LSP start/stop; reindex; publish diag; config (handleLspSrv*) |
| 23 | **Telemetry / export** | Optional usage/audit export | VS Code telemetry settings | Telemetry export: JSON, CSV, Prometheus, OTLP, Audit, Verify, Auto (IDM_TELEXPORT_*) |
| 24 | **Language / plugin registry** | Per-language plugins; detect, list, set for file | Language extensions | Detect, list, load plugin, set for file (IDM_LANG_*) |
| 25 | **Resource / project scaffold** | Generate resource; project from template | Same; project wizards | Generate resource; project scaffold; list/search templates (IDM_RESOURCE_*) |

---

## Mapping to existing docs

| Doc | Content |
|-----|--------|
| **docs/TOP_25_AI_IDE_FEATURES_AUDIT.md** | **Audit:** What RawrXD has today vs the 25 features (Present / Partial / Missing). |
| **docs/COMPETITIVE_STANDARD_CURSOR_VSCODE_COPILOT.md** | Competitive bar; capability areas; priority order. |
| **docs/GAP_VS_CURSOR_VSCODE_COPILOT.md** | Detailed gap list (critical / high / medium); actions per gap. |
| **docs/CURSOR_GITHUB_PARITY_SPEC.md** | Command IDs (11500–11574); Cursor Parity menu; GitHub read/write. |
| **This doc** | Top 25 feature rows with Cursor, Copilot, RawrXD columns. |

---

## Quick checklist (Top 25)

When adding or reviewing a feature, ensure it appears in this list where applicable and that RawrXD target is **match or exceed** Cursor and GitHub Copilot in that row.

- [ ] 1–5: Completion, ghost text, chat, Composer, streaming  
- [ ] 6–8: Diagnostics, hover, debugging  
- [ ] 9–11: Refactor, format, lint  
- [ ] 12–14: Search, Git, testing  
- [ ] 15–18: @-mentions, Composer session, vision, semantic index  
- [ ] 19–21: Snippets, keybindings, tasks  
- [ ] 22–25: LSP server, telemetry, language registry, resource scaffold  

---

**Summary:** RawrXD competes with **Cursor** and **GitHub Copilot** on these 25 AI IDE LSP/editor features. Implementation may be phased; the bar is always parity or better.
