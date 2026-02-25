# Top 25 AI IDE LSP/Editor Features — RawrXD Audit

**Purpose:** Audit of what RawrXD **has today** vs the [Top 25 features](TOP_25_AI_IDE_LSP_EDITOR_FEATURES.md) (Cursor / GitHub Copilot bar).  
**Date:** 2026-02-22.  
**Scope:** `src/` (Win32 IDE, LSP, Cursor Parity, core).

---

## Summary

| Status | Count | Meaning |
|--------|--------|--------|
| **Present** | 23 | Feature exists and is wired (menu/command/UI or API). |
| **Partial** | 1 | DAP: scaffold + stack/watch panels exist; full adapter integration TBD. |
| **Missing / Stub** | 1 | DAP end-to-end (adapter process + JSON-RPC + fill UI). |

**Post-implementation (2026-02-22):** Hover tooltip, completion popup (Ctrl+Space), lint-on-save (sendDidSave), Git diff viewer, Test Explorer menu + ID fix, Task Runner UI, DAP client scaffold. See “Per-feature audit” for updated status.

---

## Per-feature audit

| # | Feature | Status | Evidence |
|---|---------|--------|----------|
| 1 | **Real-time code completion** | **Present** | `CompletionEngine` + ghost text path; **LSP completion popup** on **Ctrl+Space** (`cmdLSPCompletionPopup`, `lspCompletion`, IDM_LSP_COMPLETION_POPUP 5994). List at cursor; double-click inserts. Menu: LSP → Completion List. |
| 2 | **Inline AI (ghost text)** | **Present** | `Win32IDE_GhostText.cpp`: gray inline render, Tab accept / Esc dismiss, debounce, `WM_GHOST_TEXT_READY`, cursor-move dismiss. Command 5010 "Toggle Ghost Text". |
| 3 | **Chat with codebase** | **Present** | `ContextMentionParser`; @-mention parse/assemble; `cmdMentionAssembleContext`; IDM_MENTION_* in Cursor Parity. Chat panel + agent with context. |
| 4 | **Composer / multi-file edit** | **Present** | `AgenticComposerUX`; `cmdComposerNewSession`, `cmdComposerEndSession`, `cmdComposerApproveAll`, `cmdComposerRejectAll`, `cmdComposerShowTranscript`, `cmdComposerShowMetrics`; IDM_COMPOSER_* (11510–11515). Gap Composer: `cmdGapComposerBegin/Add/Commit/Status`. |
| 5 | **Streaming responses** | **Present** | `appendStreamingToken`, streaming loader, `m_useStreamingLoader`; Copilot-style token updates in chat; GGUF streaming loader. |
| 6 | **LSP diagnostics (squiggles)** | **Present** | `textDocument/publishDiagnostics` in LSP client; `onDiagnosticsReceived` → `displayDiagnosticsAsAnnotations(uri)`; `InlineAnnotation`; annotations per line and action menu. |
| 7 | **Hover / documentation** | **Present** | LSP `textDocument/hover`; `lspHover()`, `cmdLSPHoverInfo()`. Hover is shown in **tooltip at cursor** (`m_hwndHoverTooltip`, TTM_TRACKACTIVATE) and still in output panel. |
| 8 | **Integrated debugging (DAP)** | **Partial** | Breakpoint UI (F9, list); **stack/watch/variables panels** in `Win32IDE_Debugger.cpp` (IDC_DEBUGGER_STACK_LIST, WATCH_LIST, VARIABLE_TREE); **DAP client scaffold** in `include/dap_client.hpp` + `src/core/dap_client.cpp` (connect/initialize/launch stubs). Full DAP adapter integration TBD. |
| 9 | **Smart refactoring** | **Present** | `initRefactoringEngine`, `handleRefactoringCommand`; IDM_REFACTOR_* (11540–11547): Extract Method/Variable, Rename Symbol, Organize Includes, Convert to Auto, Remove Dead Code, Show All, Load Plugin. |
| 10 | **Code formatting** | **Present** | `cmdFormatDocument()`, IDM_FORMAT_DOCUMENT (2030), Shift+Alt+F; `handleEditFormatDocument` (SSOT). Format-on-save (2031) in menu. |
| 11 | **Linting integration** | **Present** | **Lint-on-save:** `saveFile()` calls `sendDidSave()` after save so LSP server sends `publishDiagnostics` → `displayDiagnosticsAsAnnotations` (squiggles). LSP clang-tidy in config; annotations "linter". |
| 12 | **Multi-file search/replace** | **Present** | `performWorkspaceSearch()`, `replaceInFiles()` in `Win32IDE_Sidebar.cpp`; `createSearchView`, `m_hwndSearchResults`, `updateSearchResults`. Search/replace in files with options. |
| 13 | **Git UI** | **Present** | `showGitStatus()`, `showGitDiff()` (IDM_GIT_DIFF 3025) — diff in output panel; `gitCommit`/`gitPush`/`gitPull`; sidebar SCM view with commit message box; Git menu (Status, Show Diff, Commit, Push, Pull, Panel). |
| 14 | **Testing in IDE** | **Present** | **Test Explorer** panel (`Win32IDE_TestExplorerTree.cpp`): `cmdTestExplorerShow`, `cmdTestExplorerRun`, `cmdTestExplorerRefresh`, `cmdTestExplorerFilter`; View → Test Explorer (IDM_TESTEXPLORER_SHOW 11620). Tree shows pass/fail/skip; Run runs suite and parses output. CodeLens; `cmdAuditRunTests`, Gauntlet. |
| 15 | **@-Mention context** | **Present** | `ContextMentionParser`; init/parse/assemble; IDM_MENTION_PARSE, IDM_MENTION_SUGGEST, IDM_MENTION_ASSEMBLE, IDM_MENTION_REGISTER_CUSTOM. |
| 16 | **Agent / Composer session** | **Present** | Same as #4; Agentic Composer UX and Gap Composer commands. |
| 17 | **Vision / multimodal** | **Present** | `VisionEncoder`; `initVisionEncoderUI`; IDM_VISION_LOAD_FILE, PASTE_CLIPBOARD, SCREENSHOT, BUILD_PAYLOAD, VIEW_GUI_HOTPATCH (11530–11534). |
| 18 | **Semantic index / codebase** | **Present** | `initSemanticIndex`, `handleSemanticIndexCommand`; IDM_SEMANTIC_* (11560–11567): Build Index, Fuzzy Search, Find Refs, Show Deps, Type Hierarchy, Call Graph, Find Cycles, Load Plugin. SemanticIndexEngine. |
| 19 | **Snippets** | **Present** | `loadCodeSnippets()`, `showSnippetManager()`, `m_codeSnippets`; IDM_EDIT_SNIPPET (2012); built-in snippets (e.g. PowerShell). Command 5005 "Code Snippets". |
| 20 | **Keybinding customization** | **Present** | `handleQwShortcutEditor`, `handleQwShortcutReset`; `Win32IDE_ShortcutEditor.cpp` (keybindings list, edit); SSOT `handleQwShortcutEditor`. |
| 21 | **Tasks / launch config** | **Present** | **Task Runner** (`Win32IDE_TaskRunner.cpp`): View → Run Task (IDM_TASK_RUNNER_SHOW 11630); loads `.vscode/tasks.json` or `.rawrxd/tasks.json`, lists tasks in window, Run executes selected task (CreateProcess). `build_task_provider` generates tasks.json; Sidebar creates launch.json. |
| 22 | **LSP server control** | **Present** | LSP start/stop per server; `LSPServerState::Starting/Running/Stopped`; `handleLspSrvStart`, `handleLspSrvStop`, `handleLspSrvStatus`, etc. (ssot_handlers.h). |
| 23 | **Telemetry / export** | **Present** | `initTelemetryExport`, `shutdownTelemetryExport`, `handleTelemetryExportCommand`; IDM_TELEXPORT_* (11500–11507); JSON, CSV, Prometheus, OTLP, Audit, Verify, Auto. |
| 24 | **Language / plugin registry** | **Present** | `initLanguageRegistry()`; `LanguageRegistry::Instance()`; IDM_LANG_DETECT, LIST_ALL, LOAD_PLUGIN, SET_FOR_FILE (11550–11553). |
| 25 | **Resource / project scaffold** | **Present** | `initResourceGenerator()`; `ResourceGeneratorEngine::Instance()`; IDM_RESOURCE_GENERATE, GEN_PROJECT, LIST_TEMPLATES, SEARCH, LOAD_PLUGIN (11570–11574). |

---

## Recommended next steps (to reach full parity)

1. **DAP end-to-end:** Implement DAP client (include/dap_client.hpp) to spawn adapter process, send initialize/launch over JSON-RPC stdio, and forward events to populate stack/watch/variables in the existing debugger UI.

---

## References

- **Feature list:** [docs/TOP_25_AI_IDE_LSP_EDITOR_FEATURES.md](TOP_25_AI_IDE_LSP_EDITOR_FEATURES.md)
- **Gap list:** [docs/GAP_VS_CURSOR_VSCODE_COPILOT.md](GAP_VS_CURSOR_VSCODE_COPILOT.md)
- **Competitive bar:** [docs/COMPETITIVE_STANDARD_CURSOR_VSCODE_COPILOT.md](COMPETITIVE_STANDARD_CURSOR_VSCODE_COPILOT.md)
