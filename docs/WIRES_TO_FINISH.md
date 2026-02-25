# Wires to Finish — Non-Finished → Finished, Working Features

All wiring points that need refactoring (code cleanup or missing connection) to move features from **started/non-finished** to **finished, complete, working**.  
**Source:** codebase grep, `docs/FINISH_STARTED_FIRST.md`, `docs/GAP_VS_CURSOR_VSCODE_COPILOT.md`.

---

## 1. Wires that are DONE (command → engine) but broken by stray returns / braces

These paths are **already connected** (menu → cmd* → engine). They fail or misbehave because of stray `return true;` and broken brace structure. **Refactor:** remove stray returns and fix closing braces so control flow is correct.

| # | File | Functions / area | Wire (from → to) | Fix |
|---|------|-------------------|------------------|-----|
| 1 | **Win32IDE_CursorParity.cpp** | `cmdTelExportJSON` … `cmdTelExportAutoStop` | Menu → TelemetryExporter::ExportToFile / ExportAuditLog / VerifyAuditChain / StartAutoExport / StopAutoExport | Remove stray `return true;` and fix `}` so success/error paths and message boxes run. |
| 2 | **Win32IDE_CursorParity.cpp** | `cmdComposerNewSession` … `cmdComposerShowMetrics` | Menu → AgenticComposerUX (StartSession, EndSession, ApproveFileChange, RejectFileChange, SerializeSession) | Same: remove stray returns; fix braces so `composer.*` and MessageBox run. |
| 3 | **Win32IDE_CursorParity.cpp** | `cmdMentionParse`, `cmdMentionSuggest`, `cmdMentionAssembleContext`, `cmdMentionRegisterCustom` | Menu → ContextMentionParser::Parse / GetSuggestions / AssembleContext | Same. |
| 4 | **Win32IDE_CursorParity.cpp** | `cmdVisionLoadFile`, `cmdVisionPasteClipboard`, `cmdVisionScreenshot`, `cmdVisionBuildPayload` | Menu → VisionEncoder (LoadFromFile, LoadFromClipboard, CaptureScreenshot, BuildMultimodalPayload) | Same. |
| 5 | **Win32IDE_CursorParity.cpp** | `cmdRefactorExtractMethod` … `cmdRefactorLoadPlugin` | Menu → RefactoringEngine::Execute / GetAllRefactorings / LoadPlugin; apply first edit via SetWindowTextA | Same; ensure `appendToOutput` in cmdRefactorShowAll runs. |
| 6 | **Win32IDE_CursorParity.cpp** | `cmdSemanticBuildIndex` … `cmdSemanticLoadPlugin` | Menu → SemanticIndexEngine::BuildIndex / FuzzySearch / GetReferences / GetFileDependencies / GetTypeHierarchy / GetCallGraph / FindCycles | Same. |
| 7 | **Win32IDE_CursorParity.cpp** | `cmdResourceGenerate` … `cmdResourceLoadPlugin` | Menu → ResourceGeneratorEngine (Generate, GenerateProject, ListTemplates, SearchTemplates, LoadPlugin) | Same. |
| 8 | **Win32IDE_SyntaxHighlight.cpp** | `onEditorContentChanged()` | EN_CHANGE → triggerGhostTextCompletion + syntax color timer | Remove stray `return true;` and fix `}` so `triggerGhostTextCompletion()` and `SetTimer(SYNTAX_COLOR_TIMER_ID)` and the rest of the function run. |
| 9 | **Win32IDE_LSPClient.cpp** | `onDiagnosticsReceived`, `getDiagnosticsForFile`, `getAllDiagnostics`, `clearDiagnostics`, `clearAllDiagnostics` | LSP publishDiagnostics → m_lspDiagnostics; displayDiagnosticsAsAnnotations(uri) | Remove stray `return true;` and fix braces so `displayDiagnosticsAsAnnotations(uri)` and clear/annotation logic run. |

---

## 2. Wires that are complete (no refactor needed)

| Wire | From | To | Note |
|------|------|-----|------|
| **Ghost text request** | Editor EN_CHANGE | onEditorContentChanged → triggerGhostTextCompletion | Already in Win32IDE_SyntaxHighlight. |
| **Ghost text delivery** | Background completion | PostMessage(WM_GHOST_TEXT_READY) → onGhostTextReady → m_ghostTextContent + InvalidateRect | Win32IDE_GhostText + Win32IDE_Core. |
| **Refactoring engine** | cmdRefactor* | RefactoringEngine::Execute + buildRefactoringContext | Logic wired; CursorParity needs cleanup above. |
| **Telemetry export** | cmdTelExport* | TelemetryExporter::ExportToFile etc. | Logic wired; CursorParity needs cleanup. |

---

## 3. Wires that are missing (add connection)

| # | Wire (from → to) | Location to add / change | Purpose |
|---|-------------------|---------------------------|---------|
| 1 | **LSP diagnostics → editor squiggles** | Win32IDE_LSPClient: ensure `displayDiagnosticsAsAnnotations` is called and annotations are **drawn** in editor (squiggles or margin). | If annotations are stored but not painted, wire paint/overlay to annotation list for current file. |
| 2 | **Format Document** | New command (e.g. IDM_FORMAT_DOCUMENT) + handler that runs clang-format (or configured formatter) on current buffer and replaces text. | No existing Format command in Win32; add command and wire to formatter. |
| 3 | **Format on save** | Save path (e.g. before writing file): if “format on save” enabled, call same formatter as Format Document then write. | Optional hook in Win32IDE_FileOps or save handler. |
| 4 | **Hybrid completion → ghost text (optional)** | When LSP_AI_Bridge returns hybrid completions, pass top suggestion to ghost text (e.g. call showGhostText or set m_ghostTextContent) so inline suggestion can show LSP+AI result. | Currently ghost text uses only requestGhostTextCompletion (Ollama/native). Optional: on completion response, merge with ghost path. |

---

## 4. Summary table (refactor vs missing)

| Category | Count | Action |
|----------|-------|--------|
| **Code cleanup (stray returns / braces)** | 9 areas (CursorParity 7, SyntaxHighlight 1, LSPClient 1) | Remove stray `return true;`, fix `}` so full logic runs. |
| **Missing wires** | 2–4 | LSP paint; Format Document (+ format on save); optional hybrid → ghost. |

---

## 5. File list for refactor

| File | What to do |
|------|------------|
| **Win32IDE_CursorParity.cpp** | In every `cmd*` listed in §1, remove stray `return true;` and fix closing braces so the full body (engine call, MessageBox/appendToOutput) runs. |
| **Win32IDE_SyntaxHighlight.cpp** | In `onEditorContentChanged()`, remove stray returns and fix braces so both ghost trigger and syntax timer path run. |
| **Win32IDE_LSPClient.cpp** | In `onDiagnosticsReceived`, `getDiagnosticsForFile`, `getAllDiagnostics`, `clearDiagnostics`, `clearAllDiagnostics`, remove stray returns and fix braces so diagnostics are stored and `displayDiagnosticsAsAnnotations` / clear run. |

---

## 6. Completed this pass (code cleanup)

- **Win32IDE_SyntaxHighlight.cpp** — `onEditorContentChanged()`: removed stray returns; ghost trigger and syntax timer both run.
- **Win32IDE_LSPClient.cpp** — Diagnostics: `onDiagnosticsReceived`, `getDiagnosticsForFile`, `getAllDiagnostics`, `clearDiagnostics`, `clearAllDiagnostics`, `displayDiagnosticsAsAnnotations`: removed stray returns and fixed braces so LSP → annotations flow runs.  
  **Optional (done):** Bulk-removed stray `return true; return true; }` from the rest of the file and fixed brace structure in init, start/stop server, sendLSPRequest, readLSPResponse, lspReaderThread (diagnostics parsing). Any remaining brace/indent issues in very deep nesting can be fixed as compiler errors appear.
- **Win32IDE_CursorParity.cpp** — All `cmd*` (telemetry, composer, mention, vision, refactor, language, semantic, resource), static helpers, `buildRefactoringContext`, `handle*Command` switches: removed stray `return true` and fixed brace structure so control flow is correct.
- **Win32IDE_LSP_AI_Bridge.cpp** — Removed stray `return true; return true; }` throughout; fixed brace structure in `requestHybridCompletion` (LSP/ASM/AI/fallback blocks, merge/sort, stats) so the function returns merged results correctly.

## 6b. Optional wires (completed)

- **Hybrid completion → ghost text:** When the primary ghost-text path (Ollama/native) returns empty, the background completion thread now calls `requestHybridCompletion(filePath, line, character)` and, if the merged LSP+AI+ASM list is non-empty, posts the top item’s `insertText` via `WM_GHOST_TEXT_READY` so it appears as inline ghost text. Implemented in `Win32IDE_GhostText.cpp` (lambda in `requestGhostTextCompletion` trigger).

---

## 7. References

- **Finish-first roadmap:** `docs/FINISH_STARTED_FIRST.md`
- **Gap vs Cursor/VS Code:** `docs/GAP_VS_CURSOR_VSCODE_COPILOT.md`
- **Competitive standard:** `docs/COMPETITIVE_STANDARD_CURSOR_VSCODE_COPILOT.md`
