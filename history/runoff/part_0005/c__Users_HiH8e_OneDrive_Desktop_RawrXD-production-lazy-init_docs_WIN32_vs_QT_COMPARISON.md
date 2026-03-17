# Win32 IDE vs Qt IDE — Comparison & Parity Map

Date: 2025-12-18
Scope: Practical side-by-side of the native Win32 IDE implementation vs the legacy Qt IDE, with file references and migration/parity status.

## Executive Summary
- The current codebase contains both the legacy Qt implementation (`src/qtapp/*`) and the Qt-free Win32 path (`src/win32app/*` and `src/*` native modules).
- Win32 IDE targets (e.g., `AgenticIDEWin`, `RawrXDIDEV5Win`) build and run without Qt. Qt-era code is kept as reference but should be excluded from Win32 builds.
- Core features (file browser, editor tabs, chat/orchestra, terminal, paint, tool registry) exist in Win32 equivalents.

## High-Level Differences
- UI Toolkit: Qt Widgets/Signals/Slots → Win32 API + custom widgets/layout.
- Types/Containers: `QString`, `QList`, `QByteArray`, `QFile` → `std::string`, `std::vector`, `std::vector<uint8_t>`, `std::filesystem`.
- Concurrency/Timers: `QThread`, `QTimer`, `QtConcurrent` → `std::thread`, `std::condition_variable`, `std::async`.
- Events: Qt signals/slots → `std::function` callbacks and message loop dispatch.
- Build: `find_package(Qt6 ...)` → no Qt; link `user32`, `gdi32`, `comctl32`, `windowscodecs`, etc.

## File Map — Where Things Live
- Qt-era UI: `src/qtapp/*` (e.g., `MainWindow.cpp`, `enterprise_tools_panel.cpp`).
- Win32 UI: `src/win32_ui.cpp`, `src/native_ui.cpp`, `src/native_widgets.cpp`, `src/native_layout.cpp`, `src/native_file_tree.cpp`, `src/native_paint_canvas.cpp`.
- Agentic core (Qt-free): `include/tool_registry.hpp`, `src/tool_registry.cpp`, `src/agentic/*`, `enterprise_core/*` (Qt-free variants).
- AI/Orchestra bridges: `cursor-ai-copilot-extension-win32/orchestration/*`.

## Feature Parity Table
| Area | Qt IDE (then) | Win32 IDE (now) | Status |
|---|---|---|---|
| File Browser | `src/qtapp/file_browser.*` (QTreeView) | `src/native_file_tree.*` (TreeView + drive enum) | Parity achieved |
| Tabbed Editor | `multi_tab_editor.*` | `production_tab_widget_impl.cpp` + custom editor | Parity; undock via native layout |
| Chat/Assistant | `ai_chat_panel*` | Right tabbed chat (Win32 control) wired to orchestra | Parity; multi-tab supported |
| Terminal | `TerminalWidget.*` | `terminal_pool*.cpp` + `cross_platform_terminal.cpp` | Parity; PS/CMD split OK |
| Orchestra Panel | `enterprise_tools_panel.cpp` | Built-in Orchestra (model dropdown + objective + Max Mode) | Parity; ToolRegistry-backed |
| Image/Paint | `ThemedCodeEditor`/Qt paint | `native_paint_canvas.*` + `/paintgen` via WIC | Parity; WIC PNG decode |
| Tool Registry | Qt/Panel-bound | `include/tool_registry.hpp` + `src/tool_registry.cpp` | Superior (observability, retries) |
| GGUF Loader | Qt I/O helpers | `src/streaming_gguf_loader.*` (std/Win32 I/O) | Parity (Qt-free) |
| MASM Comp/Decomp | Qt glue around ASM | `masm_decompressor.cpp`, `gzip_masm_store.cpp` (std glue) | Parity (Qt-free) |
| Telemetry | Qt/Network logging | `logger.cpp/h`, `metrics.cpp/h`, file-based | Qt-free; opt-in |

## API Mappings (Common Replacements)
- `QString` → `std::string` (UTF-8); helpers to/from wide strings for Win32.
- `QByteArray` → `std::vector<uint8_t>`.
- `QList`, `QStringList`, `QMap/QHash` → `std::vector`, `std::unordered_map`.
- `QFile`, `QDir`, `QStandardPaths` → `std::filesystem` + Win32 SH* APIs as needed.
- `QTimer`, `QThread`, `QtConcurrent` → `std::thread`, `std::jthread` (if C++20), `std::async`.
- Signals/slots → `std::function` event handlers + message loop posts.

## Build Differences
- Legacy (Qt): global `find_package(Qt6 ...)`, `AUTOMOC`, Qt modules linked.
- Win32: No Qt discovery; `AgenticIDEWin` links `user32`, `gdi32`, `comctl32`, `ws2_32`, `winhttp`, `ole32`, `winmm`, `windowscodecs`.
- CMake: Guard Qt sections with options; ensure Win32 targets exclude `src/qtapp/*`.

## Known Qt-Only Artifacts (Keep Excluded)
- `src/qtapp/*` including `MainWindow.*`, `enterprise_tools_panel.*`, `ai_*_panel.*`, etc.
- Any `*_OLD` Qt versions retained for reference.

## Verification Checklist (No-Qt Build)
- No Qt headers in compilation units.
- No Qt libs in link line.
- Executables: `AgenticIDEWin`, `RawrXDIDEV5Win`, `AgentOrchestraCLI`, `StatusCheckerCLI` run without Qt DLLs.
- GGUF streaming and MASM wrappers function via std/Win32 I/O.

## Where to Read More
- `README-Win32IDE.md` — Win32 build + usage
- `BEFORE_AFTER_COMPARISON.md` — engine refactors (sampling, tokenizer, perf)
- `docs/FEATURES_CHECKLIST.md` — features to validate without Qt
- `docs/TOOLS_INVENTORY.md` — tool list (~44) to wire into ToolRegistry

## Migration Notes
- Prefer adding thin adapters instead of rewriting core logic.
- Keep Qt-era code as diff reference only; do not link it.
- Use `std::filesystem` for all path work; convert to wide strings at Win32 boundaries.

## Gaps to Watch
- Docking parity depends on custom layout; ensure UX matches expected Qt behavior.
- Any remaining references to `Q*` headers in `src/win32app/*` should be removed.
- Replace any `#warning` with MSVC-compatible pragmas.
