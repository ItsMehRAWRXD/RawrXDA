# IDE Source Fix Scope (1600+ files)

Per `.cursorrules`, all IDE C++ source must follow:

- **Logging**: Use `Logger` / `IDELogger` / `LOG_INFO`, `LOG_ERROR`, etc. — **never** `std::cout`, `printf()`, or `qDebug()`.
- **Errors**: Prefer `std::expected<T, E>`; **never** exceptions for control flow.
- **Memory**: Smart pointers / parent-child; **never** raw `new`/`delete`.
- **No stray control flow**: Remove erroneous `return true;` and similar that break functions.

## Scope

- **~944** `.cpp` and **~745** `.h` under `src/` (~1689 files).
- **Excluded** from logging/error-handling fixes: third-party and vendor code, e.g.:
  - `src/ggml-*`, `src/core/sqlite3.c`, `src/gguf.c`, `ggml.c`, `ggml-quants.c`, `ggml-backend*.cpp`, `ggml-opt.cpp`
  - `src/security-engines/` (JS)
  - ASM, CUDA (`.cu`, `.cuh`), Metal (`.m`)

## Fixes applied so far

1. **Win32IDE_PlanExecutor.cpp** — Removed ~90+ erroneous `return true;` and fixed switch/block structure.
2. **IDELogger.cpp** — Removed stray `return true;` and fixed brace structure.
3. **Win32IDE_Logger.cpp** — Same; all `void` functions no longer return values.
4. **logger.h** — Replaced `std::cout`/`std::cerr` with `OutputDebugStringA` (Win) / `fprintf(stderr)` fallback.
5. **fix_ide_logging.ps1** — Fixed path bug (normalize `$srcRoot`, safe `$rel` from `$full`); script runs without Substring errors.
6. **Win32IDE_ShortcutEditor.cpp** — Removed stray `return true;` and fixed for/if brace structure in `cmdShortcutEditorReset`, `cmdShortcutEditorSave`, `cmdShortcutEditorList`.
7. **qtapp/MainWindow.cpp** — Fixed void handlers: `explainCode`, `fixCode`, `refactorCode`, `generateTests`, `onScreenShareStarted`, `onWhiteboardDraw`, `onTimeEntryAdded`, `onKanbanMoved`, `onPomodoroTick`, `onWallpaperChanged`, `showEditorContextMenu` (removed stray `return true;` and corrected brace structure).
8. **qtapp/widgets/multi_file_search.cpp** — Replaced `FileManager::toRelativePath(filePath, m_projectPath)` with `QDir(m_projectPath).relativeFilePath(filePath)`; fixed destructor, constructor, `onResultItemClicked`, `onSearchCompleted`, `updateResultsTree`, export lambda (removed stray `return true;` and braces).

## How to continue (1-by-1 or batch)

- **Script**: `scripts\fix_ide_logging.ps1` — safe literal `std::cout` → `LOG_INFO` and adds `IDELogger.h` where needed for `win32app`.
- **Manual**: For each file:
  1. Replace `std::cout << ...` with `LOG_INFO(...)` or `logMessage("INFO", ...)` (build the message string).
  2. Replace `printf(...)` with `LOG_INFO`/`LOG_ERROR` (e.g. `char buf[256]; snprintf(buf, sizeof(buf), ...); LOG_INFO(buf);` or use a string stream).
  3. Replace `qDebug() << ...` with `LOG_DEBUG(...)` or equivalent.
  4. Remove any stray `return true;` in `void` functions or in the middle of blocks.
  5. Add `#include "IDELogger.h"` (or `#include "Win32IDE.h"` which may pull it in) when using `LOG_*`.

## Finding remaining violations

```powershell
# Files still using cout/printf/qDebug (from repo root)
rg "std::cout|printf\s*\(|qDebug\s*\(" src --type-add 'cpp:*.cpp' -t cpp -t cc -l
```

Then fix each listed file or run `fix_ide_logging.ps1` for safe automated replacements.
