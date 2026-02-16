# TODOS - Qt Removal Phase 2 & 3

## 🎯 Current Status (2026-02)

**Qt removal is COMPLETE for the active codebase.**

- ✅ **Verification**: Run `.\Verify-Build.ps1 -BuildDir "D:\rawrxd\build"` — must pass 7/7 (no Qt #includes in src or Ship, no Qt DLLs, no Q_OBJECT).
- ✅ **Replacement layer**: Use `Ship/StdReplacements.hpp` (WideString, Variant, StdFile, StdDir, JsonDoc, etc.); no Qt headers.
- ✅ **Policy**: `.cursorrules` and `Verify-Build.ps1` enforce zero Qt; see `UNFINISHED_FEATURES.md` for remaining open items.

The batches below are **historical**. For any legacy or out-of-tree files that still reference Qt, use the same pattern: add `StdReplacements.hpp` where needed, remove `#include <Q*>` and Q_OBJECT, and re-run Verify-Build.

---

## Historical batch plan (reference only)
- ✅ Phase 1 COMPLETE: Foundation work done
- ✅ Phase 2/3: Qt removed from src + Ship (verified by Verify-Build.ps1)
- ⏳ Phase 4: Ongoing — build fixes, stubs, parity (see UNFINISHED_FEATURES.md)

---

## PHASE 2: Batch Qt Include Removal (1,186 files)

### Strategy
Process 6 priority batches in order. Each batch:
1. Add `#include "QtReplacements.hpp"` after `#pragma once`
2. Remove all `#include <Q*>` lines
3. Remove Q_OBJECT, Q_PROPERTY, QT_BEGIN_NAMESPACE, QT_END_NAMESPACE
4. Update qXXX() function calls (qDebug, qWarning, qrand, etc.)
5. Verify no compile errors (quick check)

### Batch 1: qtapp/ folder (45 files - HIGHEST PRIORITY)
**Files**: D:\RawrXD\src\qtapp\*.cpp, *.h, *.hpp

**High-usage files** (20+ Qt includes each):
- [ ] MainWindow.cpp (74 includes)
- [ ] MainWindow_v5.cpp (30 includes)
- [ ] MainWindow_AI_Integration.cpp (24 includes)
- [ ] metrics_dashboard.cpp (25 includes)
- [ ] discovery_dashboard.cpp (21 includes)
- [ ] digestion_enterprise.cpp (21 includes)
- [ ] startup_readiness_checker.cpp (18 includes)
- [ ] settings_dialog.cpp (20 includes)
- [ ] blob_converter_panel.cpp (22 includes)
- [ ] compiler_interface.cpp (19 includes)
- [ ] gui_command_menu.cpp (13 includes)
- [ ] code_minimap.cpp (14 includes)
- [ ] multi_file_search.cpp (10 includes)
- [ ] [+ 31 more files]

**Subtasks**:
- [ ] Create batch replacement script for qtapp/
- [ ] Apply #include "QtReplacements.hpp" to all 45 files
- [ ] Remove all #include <Q*> from qtapp/
- [ ] Remove Q_OBJECT macros
- [ ] Check for compilation errors

### Batch 2: agent/ folder (25 files)
**Files**: D:\RawrXD\src\agent\*.cpp, *.hpp

**High-usage files** (10+ includes each):
- [ ] auto_update.cpp (18 includes)
- [ ] action_executor.cpp (11 includes)
- [ ] meta_learn.cpp (15 includes)
- [ ] model_invoker.cpp (13 includes)
- [ ] release_agent.cpp (16 includes)
- [ ] zero_touch.cpp (11 includes)
- [ ] self_test.cpp (11 includes)
- [ ] ide_agent_bridge_hot_patching_integration.cpp (13 includes)
- [ ] [+ 17 more files]

**Subtasks**:
- [ ] Apply #include "QtReplacements.hpp" to all 25 files
- [ ] Remove all #include <Q*> from agent/
- [ ] Remove Q_OBJECT macros
- [ ] Check for compilation errors

### Batch 3: agentic/ folder (15 files - CORE ENGINE)
**Files**: D:\RawrXD\src\agentic* and D:\RawrXD\src\agentic\*

**Files**:
- [ ] agentic_engine.cpp
- [ ] agentic_executor.cpp
- [ ] agentic_file_operations.cpp
- [ ] agentic_copilot_bridge.cpp
- [ ] agentic_ide_main.cpp
- [ ] agentic_controller.cpp
- [ ] agentic_engine.h
- [ ] agentic_executor.h
- [ ] [+ more]

**Subtasks**:
- [ ] Apply #include "QtReplacements.hpp" to all files
- [ ] Remove all #include <Q*>
- [ ] Check for compilation errors

### Batch 4: UI/Config Systems (50+ files)
**Folders**: auth/, feedback/, setup/, orchestration/, thermal/

**High-usage files**:
- [ ] auth/QuantumAuthUI.cpp (32 includes)
- [ ] feedback/FeedbackSystem.cpp (31 includes)
- [ ] setup/SetupWizard.hpp (22 includes)
- [ ] orchestration/TaskOrchestrator.cpp (18 includes)
- [ ] thermal/RAWRXD_ThermalDashboard_Enhanced.cpp (23 includes)

**Subtasks**:
- [ ] Apply #include "QtReplacements.hpp" to all files
- [ ] Remove all #include <Q*>
- [ ] Update UI widget replacements
- [ ] Check for compilation errors

### Batch 5: AI Systems & Subsystems (40+ files)
**Folders**: ai/, training/, inference*, streaming*, utils/

**Subtasks**:
- [ ] Apply #include "QtReplacements.hpp" to all files
- [ ] Remove all #include <Q*>
- [ ] Check for compilation errors

### Batch 6: Remaining Files (remaining from 1,186)
**Various**: All remaining .cpp/.h/.hpp files not yet processed

**Subtasks**:
- [ ] Apply #include "QtReplacements.hpp" to all remaining files
- [ ] Remove all #include <Q*>
- [ ] Final verification

### Verification After Each Batch
```bash
# After each batch, run:
cd D:\RawrXD\build
cmake ..
cmake --build . --config Release 2>&1 | grep -i "error" | head -20
```

---

## PHASE 3: Build System Updates

### Task 1: Update CMakeLists.txt (D:\RawrXD\src\CMakeLists.txt)
**Changes**:
- [ ] Find `find_package(Qt5 ...)` - REMOVE entire block
- [ ] Find `qt5_add_resources(...)` - REMOVE entire block
- [ ] Find `qt5_create_translation(...)` - REMOVE entire block
- [ ] Find `add_definitions(-DQT_NO_WARNING_OUTPUT)` - REMOVE
- [ ] Find `Qt5::` in target_link_libraries - REMOVE all Qt5:: libraries

**Add**:
```cmake
# Ensure C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Win32 API libraries
target_link_libraries(target
    kernel32.lib
    user32.lib
    gdi32.lib
    winsock2.lib
    ws2_32.lib
    winhttp.lib
    oleaut32.lib
    ole32.lib
    comctl32.lib
    shlwapi.lib
    psapi.lib
    dbghelp.lib
)
```

**Subtasks**:
- [ ] Read CMakeLists.txt line by line
- [ ] Remove all Qt5 references
- [ ] Verify no find_package(Qt5 ...)
- [ ] Add C++20 requirement
- [ ] Add Win32 libraries

### Task 2: Rebuild Project
```bash
cd D:\RawrXD
rmdir /S build
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019"
cmake --build . --config Release
```

**Subtasks**:
- [ ] Fresh cmake configuration
- [ ] Full build
- [ ] Check for Qt-related errors
- [ ] Resolve any linker issues

---

## PHASE 4: Verification & Testing

### Task 1: Binary Verification
```bash
dumpbin /imports D:\RawrXD\build\Release\RawrXD_IDE.exe | findstr "Qt5"
```
**Expected**: No output (zero Qt DLLs)

**Subtasks**:
- [ ] Run dumpbin on RawrXD_IDE.exe
- [ ] Run dumpbin on RawrXD_CLI.exe
- [ ] Run dumpbin on RawrXD_TestRunner.exe
- [ ] Confirm NO Qt5Core.dll, Qt5Gui.dll, Qt5Network.dll, etc.

### Task 2: Functionality Testing
```bash
D:\RawrXD\build\Release\RawrXD_IDE.exe --help
D:\RawrXD\build\Release\RawrXD_CLI.exe --version
D:\RawrXD\build\Release\RawrXD_TestRunner.exe --list
```

**Subtasks**:
- [ ] IDE starts without errors
- [ ] CLI responds to commands
- [ ] Test runner lists all tests
- [ ] No runtime crashes

### Task 3: Test Suite Execution
```bash
D:\RawrXD\build\Release\RawrXD_TestRunner.exe --xml results.xml
```

**Subtasks**:
- [ ] 35+ tests execute
- [ ] All tests pass
- [ ] No Qt-related test failures
- [ ] JUnit XML report generated

### Task 4: Performance Baseline
**Subtasks**:
- [ ] Measure startup time
- [ ] Measure memory usage
- [ ] Compare vs previous Qt version
- [ ] Document improvements

---

## Automation Helpers (Available)

### Python Script for Analysis
**Location**: D:\RawrXD\src\qt_removal_analysis.py

**Usage**:
```bash
python qt_removal_analysis.py
```

**Output**: Statistics on Qt usage by file

### Batch Replacement Ideas
```bash
# Find all Q_OBJECT macros
grep -r "Q_OBJECT" D:\RawrXD\src\

# Find all Qt includes
grep -r "#include <Q" D:\RawrXD\src\

# Count Qt includes by file
grep -r "#include <Q" D:\RawrXD\src\ | wc -l
```

---

## Critical Checkpoints

### ✅ Before Phase 2
- [x] QtReplacements.hpp created and verified
- [x] Documentation complete
- [ ] Git branch created for safety

### ✅ During Phase 2 (After Each Batch)
- [ ] New includes added to all files in batch
- [ ] Old Qt includes removed
- [ ] Q_OBJECT/Q_PROPERTY macros removed
- [ ] No syntax errors introduced
- [ ] Spot check 2-3 files for correctness

### ✅ Before Phase 3
- [ ] All 1,186 files updated
- [ ] Zero remaining #include <Q*> (verify with grep)
- [ ] Zero remaining Q_OBJECT macros

### ✅ After Phase 3
- [ ] CMakeLists.txt updated
- [ ] Clean cmake run
- [ ] Full project builds
- [ ] No linker errors

### ✅ After Phase 4
- [ ] dumpbin shows NO Qt DLLs
- [ ] All 3 executables run
- [ ] Test suite passes
- [ ] Performance acceptable

---

## Risk Mitigation

### Rollback Plan
If something breaks:
```bash
git checkout D:\RawrXD\src\
# Or restore from backup
```

### Incremental Validation
- Build after each batch (not all at once)
- Test samples from each batch
- Keep running build log

### Fallback Options
- If qtapp/ batch fails - revert and reassess
- If CMakeLists.txt breaks build - simpler config
- If tests fail - investigate individual test

---

## Success Metrics (Must Achieve)

| Metric | Target | Verification |
|--------|--------|--------------|
| Qt Includes Removed | 2,908/2,908 | `grep "#include <Q" \| wc` |
| Files Updated | 1,186/1,186 | File count in src/ |
| Build Errors | 0 | Cmake logs |
| Linker Errors | 0 | Link output |
| Qt DLLs in Binary | 0 | dumpbin /imports |
| Test Pass Rate | 35+/35+ | Test runner output |
| Runtime Stability | 100% | No crashes in 5min |

---

## Time Estimates

| Phase | Duration | Difficulty |
|-------|----------|-----------|
| Phase 2: Qt Removal | 4-8 hours | Medium (systematic) |
| Phase 3: Build Update | 1-2 hours | Low (straightforward) |
| Phase 4: Verification | 1-2 hours | Low (automated) |
| **Total** | **6-12 hours** | **Medium overall** |

---

## Questions During Execution

**Q: A file won't compile after changes?**  
A: Check QtReplacements.hpp is included first, verify syntax, look for missed qXXX() calls

**Q: Build system is confusing?**  
A: Follow the exact CMakeLists.txt changes step-by-step, build after each change

**Q: Tests are failing?**  
A: Could be timing, environment, or Qt-specific logic - check test implementation

**Q: How do I know I'm done?**  
A: When dumpbin shows zero Qt DLLs and all tests pass

---

## Ready to Start?

✅ **YES** - All foundation complete

Next immediate action:
```bash
cd D:\RawrXD\src\qtapp
# Begin Batch 1: MainWindow.cpp and qtapp/ files
```

---

**Document**: TODOS for Qt Removal Continuation  
**Status**: Ready for Execution ✅  
**Next Phase**: Batch 1 (qtapp/ folder, 45 files)  
**Estimated Time**: 4-8 hours for all phases
