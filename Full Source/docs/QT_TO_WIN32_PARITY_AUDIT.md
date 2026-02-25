# Qt → Win32 Parity Audit

**Purpose:** Audit everything from the old Qt build before the Win32 conversion — ensure all Qt-era behavior and features are accounted for in the Win32 codebase (implemented, partially ported, or explicitly deferred/missing).  
**Sources:** `QT_DEPENDENCY_INVENTORY.md`, `docs/CLI_GUI_WIN32_QT_MASM_AUDIT.md`, `docs/WIN32_IDE_CPP_AUDIT_QT_CONVERSION.md`, `docs/WIN32_IDE_FEATURES_AUDIT.md`, `include/mainwindow_qt_original.h`, `Ship/QT_REMOVAL_*`, codebase grep.  
**Date:** 2026-02-12.

---

## 1. Executive Summary

| Category | Status | Notes |
|----------|--------|--------|
| **Main IDE (window, menus, editor, layout)** | ✅ **Converted** | Replaced by `src/win32app/Win32IDE*.cpp`; no Qt in build. |
| **Build system** | ✅ **Qt-free** | Root `CMakeLists.txt` does not `find_package(Qt)`; Win32 IDE built from `win32app/`. |
| **Qt UI / qtapp** | ✅ **Retired** | `src/qtapp/` no longer present; MainWindowSimple/MainWindow/TerminalWidget replaced by Win32 equivalents. |
| **Legacy Qt headers in include/** | ⚠️ **Stub / excluded** | Optional; not in default build; documented in CLI_GUI_WIN32_QT_MASM_AUDIT. |
| **Settings / config** | ⚠️ **Different design** | No `src/settings.cpp` (QSettings) in repo; Win32 uses Registry/file/agentic config where needed. |
| **ANSI → Unicode** | ⚠️ **Open** | Win32IDE.cpp still uses many ANSI APIs; doc: WIN32_IDE_CPP_AUDIT_QT_CONVERSION. |
| **View / Git menu IDs** | ❌ **Broken** | View 2020–2027 and Git 3020–3024 do not match handler ranges; see WIN32_IDE_FEATURES_AUDIT. |

**Conclusion:** The old Qt build’s *scope* has been audited. The **primary IDE surface** (main window, menus, editor, terminal, sidebar, file ops, agent, tools, help) is implemented in Win32 and working where IDs match. Remaining gaps: (1) ANSI→Unicode for parity with Qt’s Unicode model, (2) View and Git menu ID/routing fixes, (3) optional panels and legacy `include/` Qt headers (stub or exclude).

**Qt removal (continued):** `include/tool_registry_init.hpp` converted to C++20 (QString/QProcess → `std::string`/`std::vector<std::string>`); `include/agentic_engine.h` now redirects to Qt-free `src/agentic_engine.h`; `src/gpu/gpu_backend.cpp.backup` stripped of Qt logging (qDebug/qWarning/qInfo → `std::cerr`). Build remains pure MASM/C++20 for Win32 IDE path.

---

## 2. Qt-Era Components vs Win32 Status

### 2.1 Main window and application shell

| Qt-era component | Qt source (historical) | Win32 equivalent | Status |
|------------------|------------------------|-------------------|--------|
| Main window | `QMainWindow`, `include/mainwindow_qt_original.h` (layout restore, AgenticController) | `Win32IDE` in `src/win32app/Win32IDE.cpp` (HWND, CreateWindowEx) | ✅ Converted; Qt-free (see WIN32_IDE_CPP_AUDIT_QT_CONVERSION). |
| Menu bar | QMenuBar / QMenu | `createMenuBar()`, `AppendMenu*`, `m_hMenu` | ✅ Converted. |
| Toolbar | QToolBar | `createToolbar()`, layoutTitleBar | ✅ Converted. |
| Status bar | QStatusBar | `createStatusBar()`, SB_SETTEXT | ✅ Converted. |
| Layout restore / snapshots | `restoreLayout()`, `hydrateLayout()` (QString) | Win32: no direct equivalent; layout state can be added via Registry/file. | ⚠️ Partial; by design or deferred. |

### 2.2 Editor and text

| Qt-era component | Qt source (historical) | Win32 equivalent | Status |
|------------------|------------------------|-------------------|--------|
| Code editor | QPlainTextEdit / Qt widget | RichEdit (MSFTEDIT), `createEditor()`, syntax/theme | ✅ Converted. |
| Line numbers | Qt widget | `createLineNumberGutter()`, updateLineNumbers | ✅ Converted. |
| Find/Replace | QDialog / Qt | CreateWindowEx Find/Replace dialogs, findText/replaceText | ✅ Converted (ANSI; Unicode recommended). |
| Copy with formatting | Rich text to clipboard | copyWithFormatting() — plain text only (see WIN32_IDE_CPP_AUDIT_QT_CONVERSION) | ⚠️ Simplified. |
| Snippet manager | Qt dialog | CreateWindowEx Snippet Manager dialog | ✅ Present (ANSI). |

### 2.3 Panels and sidebars

| Qt-era component | Qt source (historical) | Win32 equivalent | Status |
|------------------|------------------------|-------------------|--------|
| File explorer / tree | QTreeView / QFileSystemModel | `createExplorerView()`, TreeView, populateFileTree, context menu (New/Delete/Rename) | ✅ Converted. |
| Terminal | QTerminal / TerminalWidget | PowerShell/CMD panels, `createPowerShellPanel()`, handleTerminalCommand | ✅ Converted. |
| Output panel | QPlainTextEdit / tabs | `createOutputTabs()`, appendToOutput, severity filter | ✅ Converted. |
| Secondary sidebar (Copilot/AI) | Qt widgets | Chat input/output, model chat, send/clear | ✅ Converted. |
| Module browser | Qt | handleViewCommand 3005; menu uses 2022 (broken until View ID fix) | ⚠️ Implemented but View menu ID wrong. |
| Floating panel | Qt dock | CreateWindowEx RawrXD_FloatingPanel | ✅ Converted (ANSI). |

### 2.4 Menus and commands

| Qt-era component | Win32 status | Notes |
|------------------|--------------|--------|
| File (New/Open/Save/Save As/Exit, Load Model, Recent) | ✅ Working | IDM_FILE_* 2001–2005, 1030; handleFileCommand, openRecentFile. |
| Edit (Undo/Redo/Cut/Copy/Paste/Find/Replace/Select All) | ✅ Working | handleEditCommand; Find Next/Prev, Snippet, Copy Format, Paste Plain, Clipboard History — menu present, no dedicated handler. |
| View (Minimap, Output, Module Browser, Theme, Floating, etc.) | ❌ Broken | Menu uses IDM_VIEW_* 2020–2027; handler expects 3001–3008. Fix: align menu to 3001–3008 or add 2020–2027 in handler. |
| Git (Status, Commit, Push, Pull, Panel) | ❌ Broken | Menu uses 3020–3024; handleGitCommand uses 8001–8005. Fix: use 8001–8005 in menu or route 3020–3024. |
| Terminal, Agent, Tools, Help, Audit, Voice | ✅ Working | See WIN32_IDE_FEATURES_AUDIT. |

### 2.5 Backend and inference (Qt-era vs current)

| Qt-era component | Qt source (historical) | Current state | Status |
|------------------|------------------------|---------------|--------|
| Inference engine | `src/qtapp/inference_engine.cpp` (QObject, QFileInfo) | `src/cpu_inference_engine.cpp` (no Qt); GGUF path in Win32 IDE | ✅ Build uses cpu_inference_engine; qtapp removed. |
| Model router | universal_model_router (QObject, QJson, QFile) | Win32 IDE: backend switcher, LLM router, openModel (GGUF) | ✅ Functionality in Win32; no Qt in active path. |
| Autonomous feature engine | autonomous_feature_engine (QObject, QFile, QString) | Agent/tools in win32app; no direct file in current CMake for this name | ⚠️ Logic may live in agent/agentic; not Qt in build. |
| Agentic controller | mainwindow_qt_original (AgenticController) | Win32: Agent menu, handleAgentCommand, Win32IDE_AgentCommands | ✅ Converted. |

### 2.6 Settings and configuration

| Qt-era component | Qt source (historical) | Current state | Status |
|------------------|------------------------|---------------|--------|
| QSettings (GUI) | settings.cpp (QVariant, setValue, sync) | No `src/settings.cpp` in repo | ✅ Removed; config via file/Registry/agentic where used. |
| Monaco/editor settings dialog | monaco_settings_dialog (QDialog, QTabWidget) | `src/ui/monaco_settings_dialog.cpp` — grep shows no Qt includes | ✅ Migrated to non-Qt (or excluded from Win32 build). |

### 2.7 Qt-only headers in include/ (legacy / optional)

| Header | Qt usage | Build / recommendation |
|-------|----------|------------------------|
| agentic_iterative_reasoning.h | QObject, QString, QJsonObject, signals | Stub or RAWR_HAS_QT; not in default build. |
| ai_completion_provider.h | QObject, QString, QStringList | Same. |
| distributed_trainer.h | QObject, QJson, QNetworkReply | Optional; exclude or stub. |
| ollama_proxy.h | QObject, Q_INVOKABLE, QString | Win32/STL HTTP + callbacks if needed. |
| mainwindow_qt_original.h | QMainWindow, QString | Legacy; not used by Win32 IDE. |
| orchestration/TaskOrchestrator.h | QObject, QString, QNetworkReply | Stub or non-Qt impl. |
| visualization/ContextVisualizer.h | QString, QMap, QSet | Stub or std types. |
| agentic_text_edit.h, multi_tab_editor.h, extension_panel.h, production_agentic_ide.h | Qt UI types | Legacy; exclude from Win32 IDE build. |

These are documented in `docs/CLI_GUI_WIN32_QT_MASM_AUDIT.md`; default build does not require them.

### 2.8 Removed or retired Qt paths

| Item | Status |
|------|--------|
| src/qtapp/ (MainWindowSimple, MainWindow, TerminalWidget, inference_engine, etc.) | **Removed** — directory not present; Win32 IDE is the replacement. |
| src/settings.cpp (QSettings) | **Removed** — file not in repo; config handled elsewhere. |
| Qt test files (test_qmainwindow, minimal_qt_test, agentic_ide_test, qtapp/test_*.cpp) | **Retired** — removed or not in build. |
| find_package(Qt) in CMake | **Not present** — root CMakeLists.txt has no Qt. |

---

## 3. Cross-Reference to Existing Audits

| Document | What it covers |
|----------|----------------|
| **docs/CLI_GUI_WIN32_QT_MASM_AUDIT.md** | Qt dependency audit (zero-Qt for IDE/tool_server/CLI), MASM, C++20, include/ Qt headers. |
| **docs/WIN32_IDE_CPP_AUDIT_QT_CONVERSION.md** | Win32IDE.cpp: no Qt symbols; ANSI→Unicode list; stub/minimal logic (e.g. copy with formatting). |
| **docs/WIN32_IDE_FEATURES_AUDIT.md** | What’s fully working in Win32 GUI (menus, panels, IDs); View/Git broken; optional panels. |
| **docs/WIN32_IDE_WIRING_MANIFEST.md** | Creation order, menu hierarchy, control ID ranges, layout. |
| **Ship/QT_REMOVAL_*.*** | Qt removal phases (includes, types, macros); completion status. |
| **QT_DEPENDENCY_INVENTORY.md** | Historical list of files that had Qt and planned migrations; many items since removed or migrated. |

---

## 4. Outstanding Items (Post-Audit)

1. **ANSI → Unicode (Win32IDE.cpp)**  
   Convert CreateWindowExA, MessageBoxA, GetOpenFileNameA, SetWindowTextA, AppendMenuA, CHARFORMAT2A, TEXTRANGEA, etc. to W variants for i18n and parity with Qt’s Unicode-by-default (see WIN32_IDE_CPP_AUDIT_QT_CONVERSION §3–§6).

2. **View menu (2020–2027)**  
   Either use IDs 3001–3008 in the View menu or add handling for 2020–2027 in handleViewCommand so toggles (Minimap, Output, Module Browser, Theme, Floating Panel, etc.) work.

3. **Git menu (3020–3024)**  
   Either use 8001–8005 in the Git menu or route 3020–3024 to handleGitCommand and add cases for 3020–3024.

4. **Optional panels**  
   Network, CrashReporter, ColorPicker, VulkanRenderer, MCPHooks, OSExplorerInterceptor — fix members/includes if they are to be part of the default build (see WIN32_IDE_FEATURES_AUDIT §5).

5. **Copy with formatting**  
   Currently plain text only; optional improvement: RTF/HTML to clipboard for “copy with formatting” parity with richer Qt behavior.

6. **Layout restore/snapshots**  
   Qt MainWindow had restoreLayout/hydrateLayout; Win32 has no equivalent yet — add if product requirement.

---

## 5. Verification Commands

- **No Qt in Win32 IDE sources:**  
  `grep -r "#include <Q" src/win32app/` → expect no matches (or only in comments/docs).
- **No Qt in main build:**  
  Root CMakeLists.txt: `find_package(Qt` → none.
- **Post-fix wiring:**  
  Run `scripts/win32_ide_wiring_audit.py` and refresh `reports/win32_ide_wiring_manifest.md` after View/Git ID changes.

---

## 6. Checklist Summary

| Area | Audited | Converted & working | Partial / open |
|------|---------|---------------------|----------------|
| Main window / shell | ✅ | ✅ | — |
| Menus (File, Edit, Terminal, Agent, Tools, Help, Audit, Voice) | ✅ | ✅ | View, Git ID fix |
| Editor, Find/Replace, Snippet, Line numbers | ✅ | ✅ | ANSI→Unicode, copy-with-formatting |
| Sidebar, File explorer, Terminal panels, Output, Copilot | ✅ | ✅ | — |
| Inference / model loading | ✅ | ✅ | — |
| Settings / config | ✅ | Different design (no QSettings) | — |
| Qt include/ headers | ✅ | Stub/exclude | Keep excluded from default build |
| qtapp / settings.cpp | ✅ | Retired/removed | — |

**Everything from the old Qt build that was in scope for the Win32 conversion has been audited.** Gaps are documented above and in the referenced docs; no Qt-era feature is left unaudited.
