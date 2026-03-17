# RawrXD Reconstruction & TODO Master List

This document outlines the roadmap for rebuilding functionality in the `src` folder following the complete removal of the Qt Framework. The codebase is currently in a "raw" state (Standard C++) and requires mostly backend reimplementation of services that were previously provided by Qt.

## 1. Core Application Infrastructure (Priority High)

The entry point and main event loop have been stripped.
- [ ] **Establish Entry Point**: Redefine `main()` in a non-Qt file (e.g., `src/core/main.cpp`).
- [ ] **Event Loop Replacement**: 
    - Implement a main application loop.
    - Choose an event handling strategy:
        - *Option A*: Simple polling loop (for CLI/Agents).
        - *Option B*: `libuv` or `asio` for high-performance IO.
        - *Option C*: Native Windows Message Loop (if keeping strictly Windows).
- [ ] **Signal/Slot Replacement**:
    - The `signals:` and `slots:` keywords have been removed.
    - **TODO**: Implement a `Signal<T...>` class or use a lightweight library like `eventpp` or `sigslot`.
    - **TODO**: Refactor all "connections" to use `std::function` callbacks.

## 2. CMake Build System Repair (Priority High)

All `CMakeLists.txt` files have been stripped of `Qt::` dependencies but likely link to targets that no longer exist.
- [ ] **Audit CMakeLists**: Walk through `src/*/CMakeLists.txt` and replace `find_package(Qt...)` with appropriate standard libs.
- [ ] **Define Defines**: Ensure `_WINDOWS`, `_CONSOLE` or appropriate preprocessor definitions are set.
- [ ] **Dependency Management**: Add `FetchContent` or `find_package` for new dependencies (e.g., `nlohmann_json`, `fmt`, `spdlog`).

## 3. Module-Specific TODOs

### A. `src/qtapp` (The "Old Shell")
This directory contains the bulk of the logic but is structureless without Qt.
- [ ] **Rename**: This folder should likely be renamed to `src/app` or `src/editor`.
- [ ] **MainWindow**: `MainWindow.cpp` is currently a shell.
    - **Decision**: Is this a GUI app or a Headless Agent? 
    - *If GUI*: Port to **ImGui**, **wxWidgets**, or **Electron** (via C++ Node addon).
    - *If Headless*: Remove `MainWindow` entirely and move logic to `AgentOrchestrator`.
- [ ] **Widgets**: All files in `src/qtapp/widgets` need to be deleted or ported to a new UI framework.

### B. `src/net` & Networking
- [ ] **Backend Selection**: The `net` folder seems to rely on `net_masm_bridge.h`.
    - **Verify**: Does the MASM implementation work standalone?
    - **Fallback**: If not, integrate `libcurl` or `cpp-httplib` for HTTP/WebSocket support.
- [ ] **JSON Parsing**: Replace `QJsonDocument`/`QJsonObject` usage with `nlohmann::json`.

### C. `src/core` & `src/utils`
- [ ] **String Handling**: `QString` is gone. Ensure all string manipulations use `std::string` or `std::string_view`.
    - **TODO**: Implement `Split()`, `Join()`, `Trim()` helpers in `src/utils/StringHelpers.h`.
- [ ] **Filesystem**: `QFile`, `QDir` are gone.
    - **TODO**: Refactor completely to use `std::filesystem` (C++17).
    - **TODO**: Implement `ReadFile()`, `WriteFile()`, `ListDir()` helpers to replace `QFile` convenience methods.

### D. `src/terminal` & `src/setup`
- [ ] **Process Management**: `QProcess` is removed.
    - **TODO**: Implement a `Process` class using `reproc`, `boost::process`, or Windows `CreateProcess` API directly.
- [ ] **Logging**: `qDebug` is removed.
    - **TODO**: Integrate `spdlog` or a custom macro-based logger to rout output to stdout/file.

## 4. Missing Standard Library Features
List of Qt features that need standard replacements:
- `QTimer` -> `std::chrono` loop or thread.
- `QThread` -> `std::thread` / `std::jthread`.
- `QMutex`/`QReadWriteLock` -> `std::mutex`, `std::shared_mutex`.
- `QSettings` -> JSON config file or SQLite database.
- `QVariant` -> `std::variant` (C++17) or `std::any`.

## 5. Next Steps
1.  **Pilot Compilation**: Try to compile `src/core` or `src/utils` first.
2.  **Fix Core Utils**: Implement the string/file helpers.
3.  **Choose UI Path**: Decide on the new face of the application.
