# Finish Started First — Then Add What’s Missing

**Goal:** Complete everything that’s **already started but not finished** before adding net-new features. Missing features can often be **tied into** existing unfinished source (same completion path, same panel, same engine).

---

## 1. Started-but-not-finished (by area)

### 1.1 Editor & AI

| What | Where | Status | Finish by |
|------|--------|--------|-----------|
| **Ghost text** | `Win32IDE_GhostText.cpp` | Renderer + debounce timer + `onGhostTextTimer()` request completion; **stray `return true`** break control flow. | Remove stray returns; ensure completion response is passed to `showGhostText()` and that Tab/Esc are wired. |
| **Completion → editor** | `Win32IDE_LSP_AI_Bridge.cpp` (hybrid LSP+AI+ASM), `CompletionEngine.cpp` | Hybrid completion and merge logic exist; **not triggered on every keystroke** from Win32 editor; results not consistently fed to ghost text. | Wire editor EN_CHANGE / key handler to request hybrid completion; pass top suggestion to ghost text. |
| **CompletionEngine** | `CompletionEngine.cpp` | `getCompletions()`, `getMultiLineCompletions()` exist. | Ensure Win32/LSP bridge calls them when requesting completion; no new engine needed. |

### 1.2 Refactoring

| What | Where | Status | Finish by |
|------|--------|--------|-----------|
| **RefactoringEngine** | `src/ide/refactoring_plugin.h/.cpp` | Full API: `Execute()`, `GetAvailable()`, built-ins (rename, extract method, organize includes, etc.). **Stray `return true`** in .cpp. | Remove stray returns in `refactoring_plugin.cpp`; ensure `cmdRefactor*()` in `Win32IDE_CursorParity.cpp` call `RefactoringEngine::Instance().Execute()` and apply edits to editor. |
| **IDM_REFACTOR_*** | `Win32IDE_CursorParity.cpp` | Menu and `handleCursorParityCommand()` dispatch to `cmdRefactorExtractMethod`, etc. | Implement or complete each `cmdRefactor*()` to build `RefactoringContext`, call engine, apply `RefactoringResult.edits`. |

### 1.3 Format & model routing

| What | Where | Status | Finish by |
|------|--------|--------|-----------|
| **FormatRouter (model)** | `format_router.cpp` | Model format detection (GGUF, HF, Ollama, etc.) is implemented; **stray `return true`** break returns. | Remove all stray `return true` so `route()`, `detectFormat()`, and helpers return correctly. |
| **Code format (IDE)** | — | Gap doc: “format_router empty/stub” for **code** formatting (clang-format, format on save). | **Separate** from model FormatRouter. Add `code_format_router.cpp` or `FormatDocument` command that runs clang-format/Prettier and applies to buffer; can **tie into** existing “Format” command ID and save hook. |

### 1.4 Search & UI

| What | Where | Status | Finish by |
|------|--------|--------|-----------|
| **Multi-file search** | `src/qtapp/widgets/multi_file_search.cpp` | Full Qt widget with results tree, export; **no Win32 panel** that embeds it (Win32 IDE is Qt-free). | Either: (1) Add Win32 “Search in files” panel that reuses the **logic** (search, regex, replace) in C++ and renders in a list/tree, or (2) Expose same backend via command + results in a pop-up or bottom panel. **Tie:** Replace preview can use same result set + apply edits via existing editor API. |
| **LSP diagnostics** | LSP client exists | “LSP client partial”; diagnostics not drawn in editor. | LSP response → editor decorations (squiggles); can **tie into** same paint/decoration path as ghost text or annotations. |
| **Debug panel** | Debug panel/stubs | Breakpoint UI and DAP missing. | DAP adapter or minimal breakpoint storage + list UI; **tie into** existing debug panel container. |

### 1.5 Cursor parity & telemetry

| What | Where | Status | Finish by |
|------|--------|--------|-----------|
| **Telemetry export** | IDM_TELEXPORT_* | Menu IDs exist; export pipeline not implemented. | Implement `TelemetryExporter::ExportToFile(JSON/CSV/Prometheus)` and wire to menu. |
| **@-Mention, Vision, Semantic, Resource** | ContextMentionParser, vision encoder, SemanticIndexEngine, ResourceGeneratorEngine | Spec’d in CURSOR_GITHUB_PARITY_SPEC; some modules exist, need wiring. | Wire IDM_MENTION_*, IDM_VISION_*, IDM_SEMANTIC_*, IDM_RESOURCE_* to existing or minimal implementations; can **tie** semantic index to “Find refs” and completion context. |

---

## 2. Order of work (finish first, then add)

1. **Fix broken control flow** — Remove stray `return true` / fix braces in:
   - `format_router.cpp`
   - `Win32IDE_GhostText.cpp`
   - `src/ide/refactoring_plugin.cpp`
2. **Wire completion → ghost text** — Ensure `onGhostTextTimer()` completion response calls `showGhostText()`; ensure Tab/Esc already work (they’re in GhostText).
3. **Wire refactoring** — `cmdRefactor*()` → `RefactoringEngine::Execute()` → apply edits to editor.
4. **LSP diagnostics → editor** — Map LSP diagnostics to editor decorations (squiggles); optional quick-fix menu.
5. **Code format** — New small module: Format Document command + format on save; call clang-format or equivalent.
6. **Multi-file search UI (Win32)** — Panel or command that uses existing search/replace logic and shows results; replace preview can share editor edit API.
7. **DAP / breakpoints** — Minimal breakpoint list + DAP adapter or stub; tie into existing debug panel.
8. **Cursor parity** — Telemetry export, @-mention, vision, semantic, resource: wire to menu and existing engines where present.

---

## 3. Tying missing features into unfinished source

| Missing feature | Tie into |
|-----------------|----------|
| Real-time diagnostics | LSP client (existing) → same decoration/paint path as ghost text or annotations. |
| Quick fixes | LSP code actions or linter output → same menu/popup as “refactor” or context menu. |
| Replace preview (multi-file) | Same result set as multi-file search; apply via existing editor/buffer edit API. |
| Format on save | Same Format Document implementation; hook into save handler. |
| Snippets | Reuse completion popup or ghost-text path: “snippet” = one-off completion with placeholders. |
| Semantic “Find refs” | SemanticIndexEngine (when built) or LSP `textDocument/references`; show in same panel as search results or a dedicated “References” view. |

---

## 4. References

- **Gap list:** `docs/GAP_VS_CURSOR_VSCODE_COPILOT.md`
- **Cursor/GitHub spec:** `docs/CURSOR_GITHUB_PARITY_SPEC.md`
- **Competitive bar:** `docs/COMPETITIVE_STANDARD_CURSOR_VSCODE_COPILOT.md`

---

## 5. Completed (this pass)

- **format_router.cpp** — Removed stray `return true` blocks so `route()`, `detectFormat()`, and helpers return correctly (model format routing).
- **Win32IDE_GhostText.cpp** — Removed stray `return true` and fixed brace structure so ghost text init, timer, completion request, and render paths compile and run.
- **src/ide/refactoring_plugin.cpp** — Removed stray `return true` from void RefactoringEngine methods; legitimate `return true` in `LoadPlugin()` retained.

---

**Summary:** Finish all “started but not finished” items (fix stray returns, wire completion→ghost text, wire refactoring→engine, LSP→squiggles, then code format, search UI, debug, parity). Then add whatever is still missing; many of those can be tied into the same completion, panel, or engine code.
