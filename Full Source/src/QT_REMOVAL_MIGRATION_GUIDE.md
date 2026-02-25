# Qt Removal Migration Guide for src/ Folder

## Overview

This guide shows how to migrate source files from Qt-based implementations to Win32 DLL-based implementations using the 32-component foundation.

## Migration Strategy

The src/ folder contains ~280+ files still using Qt. Rather than rewriting everything at once, we migrate by **dependency layer**:

1. **Entry Points** (main_qt.cpp, IDE start)
2. **UI Layer** (MainWindow, widgets, terminal)
3. **Agentic Core** (agent coordination, execution)
4. **Utilities** (file ops, settings, processes)
5. **Orchestration** (task scheduling, planning)
6. **Tests & Stubs** (retire Qt test code)

---

## Migration Patterns

### Pattern 1: Replace Qt Singleton → DLL Instance

**Before (Qt):**
```cpp
#include <QApplication>
#include <QDebug>

class MyComponent : public QObject {
    Q_OBJECT
public:
    static MyComponent& instance() {
        static MyComponent inst;
        return inst;
    }
private:
    MyComponent() {}
    MyComponent(const MyComponent&) = delete;
};
```

**After (Win32):**
```cpp
#include "agentic_core_win32.h"

class MyComponent {
public:
    static MyComponent& instance() {
        static MyComponent inst;
        return inst;
    }
    
    bool initialize(const wchar_t* basePath) {
        return RawrXD::Agentic::InitializeAgenticComponents(basePath);
    }
private:
    MyComponent() {}
};
```

---

### Pattern 2: Replace QThread → Win32 Thread Pool

**Before (Qt):**
```cpp
#include <QThread>
#include <QMutex>
#include <QMutexLocker>

class Worker : public QObject {
    Q_OBJECT
public:
    void startWork() {
        QThread* thread = new QThread;
        connect(thread, &QThread::started, this, &Worker::doWork);
        moveToThread(thread);
        thread->start();
    }
private slots:
    void doWork() { /* work */ }
};
```

**After (Win32):**
```cpp
#include <windows.h>

class Worker {
public:
    void startWork() {
        HANDLE thread = CreateThread(nullptr, 0, WorkerThreadProc, this, 0, nullptr);
        if (thread) CloseHandle(thread);
    }
private:
    static DWORD WINAPI WorkerThreadProc(LPVOID param) {
        Worker* pThis = (Worker*)param;
        pThis->doWork();
        return 0;
    }
    void doWork() { /* work */ }
};
```

---

### Pattern 3: Replace QFile/QDir → RawrXD_FileManager_Win32.dll

**Before (Qt):**
```cpp
#include <QFile>
#include <QDir>

QString content = readFile("config.json");
QStringList files = QDir("/path").entryList("*.cpp");
```

**After (Win32):**
```cpp
#include "file_operations_win32.h"

std::string content = RawrXD::FileOps::readFileContent("config.json");
auto files = RawrXD::FileOps::listFilesInDirectory("/path");
```

---

### Pattern 4: Replace QMainWindow → RawrXD_MainWindow_Win32.dll

**Before (Qt):**
```cpp
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>

class MainWindow : public QMainWindow {
public:
    MainWindow() {
        QWidget* central = new QWidget;
        QVBoxLayout* layout = new QVBoxLayout(central);
        setCentralWidget(central);
    }
};
```

**After (Win32):**
```cpp
// Main window is now managed by DLL
// Create via: RawrXD_MainWindow_Win32.dll export CreateMainWindow()
// Process via: ProcessMessages() in message loop
// See main_qt_migrated.cpp for full example
```

---

### Pattern 5: Replace QSettings → RawrXD_SettingsManager_Win32.dll

**Before (Qt):**
```cpp
#include <QSettings>

QSettings settings("RawrXD", "IDE");
settings.setValue("lastProject", "/path");
QString lastProject = settings.value("lastProject", "").toString();
```

**After (Win32):**
```cpp
#include "agentic_core_win32.h"

// Settings stored in Win32 Registry
auto& config = RawrXD::Agentic::ConfigurationManager::instance();
config.initialize(basePath);
std::string lastProject = config.getString("lastProject", "");
```

---

## Migration Checklist by File Category

### UI Entry Points (PRIORITY 1)
- [ ] `src/qtapp/main_qt.cpp` → Use `main_qt_migrated.cpp` pattern
- [ ] `src/qtapp/MainWindow.cpp` → Remove Qt inheritance, call DLL
- [ ] `src/qtapp/TerminalWidget.cpp` → Use RawrXD_TerminalManager_Win32.dll

### Agentic Components (PRIORITY 2)
- [ ] `src/agentic_executor.cpp` → Use RawrXD_Executor.dll
- [ ] `src/agentic_engine.cpp` → Use RawrXD_AgenticEngine.dll
- [ ] `src/agentic_agent_coordinator.cpp` → Use RawrXD_AgentCoordinator.dll
- [ ] `src/agent/*.cpp` → All migrate to DLL calls

### Utilities (PRIORITY 3)
- [ ] `src/utils/qt_directory_manager.*` → Use file_operations_win32.h
- [ ] `src/qtapp/utils/*` → Remove all Qt utilities
- [ ] `src/utils/InferenceSettingsManager.cpp` → Use Configuration DLL

### Orchestration (PRIORITY 4)
- [ ] `src/orchestration/*.cpp` → Call RawrXD_PlanOrchestrator.dll
- [ ] `src/plan_orchestrator.cpp` → Map to DLL export
- [ ] `src/planning_agent.cpp` → Use DLL-based coordination

### Tests & Stubs (PRIORITY 5)
- [ ] `src/test_qmainwindow.cpp` → Retire or port to Win32 harness
- [ ] `src/minimal_qt_test.cpp` → Use RawrXD_FoundationTest.exe
- [ ] All `src/qtapp/test_*.cpp` → Consolidate into test harness

---

## Build System Update

### CMakeLists.txt Migration

**Old (Qt-based):**
```cmake
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)
target_link_libraries(RawrXD_IDE Qt5::Core Qt5::Gui Qt5::Widgets)
```

**New (Win32 DLL-based):**
```cmake
# Link against Win32 DLL exports instead
target_link_libraries(RawrXD_IDE 
    RawrXD_Foundation_Integration.lib
    RawrXD_MainWindow_Win32.lib
    RawrXD_InferenceEngine.lib
    RawrXD_Executor.lib
    # ... other 28 DLL libs
)

# DLL search path
link_directories(${CMAKE_BINARY_DIR}/../Ship)
```

### Build Script Update (build.bat)

```batch
@echo off
REM Compile migrated source with Win32 DLLs instead of Qt

set INCLUDES=/I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\um" ^
             /I"D:\RawrXD\Ship" ^
             /I"D:\RawrXD\src"

set LIBS=D:\RawrXD\Ship\RawrXD_Foundation_Integration.lib ^
         D:\RawrXD\Ship\RawrXD_MainWindow_Win32.lib ^
         D:\RawrXD\Ship\RawrXD_InferenceEngine.lib ^
         D:\RawrXD\Ship\RawrXD_Executor.lib

cl %INCLUDES% /std:c++17 /O2 /EHsc main_qt_migrated.cpp %LIBS% /link kernel32.lib user32.lib
```

---

## Verification Commands

### 1. Verify No Qt Includes
```powershell
cd D:\RawrXD\src
Get-ChildItem -Recurse -Include *.cpp,*.hpp,*.h | Select-String -Pattern "#include\s+<Q"
# Expected: (no output when all migrated)
```

### 2. Build with New System
```powershell
cd D:\RawrXD\build
cmake .. -DUSE_WIN32=ON -DNO_QT=ON
cmake --build . --config Release
```

### 3. Run Foundation Test on Migrated Build
```powershell
cd D:\RawrXD\Ship
.\RawrXD_FoundationTest.exe
# Expected: All 32 components READY
```

### 4. Launch Migrated IDE
```powershell
cd D:\RawrXD\Ship
.\RawrXD_IDE.exe
# Expected: <500ms startup, <100MB RAM
```

---

## Phase-Based Timeline

| Phase | Timeline | What | Files |
|-------|----------|------|-------|
| **1** | ~2 hours | UI entry points | 8 files |
| **2** | ~4 hours | Agentic core | 32 files |
| **3** | ~3 hours | Utilities | 28 files |
| **4** | ~2 hours | Orchestration | 18 files |
| **5** | ~1 hour | Tests/Stubs | 15 files |
| | **~4 hrs** | **TOTAL** | **~280 files** |

---

## Important Notes

1. **No Instrumentation**: As you instructed, skip logging/instrumentation code
2. **Functional Core Only**: Focus on making the code work, not on observability
3. **Use Existing DLLs**: Don't rebuild components - they're already in Ship/ folder
4. **Incremental Builds**: Test after each phase to catch issues early
5. **Zero Qt**: When done, grep should find zero Qt includes

---

## Support Resources

- **Main entry point pattern**: See `src/qtapp/main_qt_migrated.cpp`
- **Agentic wrappers**: See `src/agentic_core_win32.h`
- **File operations**: See `src/file_operations_win32.h`
- **DLL exports reference**: See `D:\RawrXD\Ship\ZERO_QT_ARCHITECTURE_REFERENCE.txt`
- **Foundation API**: See `D:\RawrXD\Ship\FOUNDATION_COMPLETION_REPORT.txt`

---

## Quick Start - Migrate Your First File

1. Choose a file from qtapp/ that uses Qt
2. Open both the file and the corresponding migrated example
3. Replace Qt includes with Win32 includes
4. Replace Qt class usage with DLL/wrapper usage
5. Test with `main_qt_migrated.cpp` pattern
6. Commit and move to next file

**You've got this! 32 components are ready to power the migration.** 🚀

