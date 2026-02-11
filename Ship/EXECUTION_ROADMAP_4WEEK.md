# Qt Migration Execution Roadmap (4-Week Plan)

## 📋 Overview

This document outlines the complete 4-week strategy to eliminate Qt from the RawrXD codebase, migrating **280+ files** from Qt-based to pure C++20 + Win32 API architecture.

**Goal**: Full Qt elimination with 100% feature parity and zero Qt DLL dependencies

**Timeline**: 4 weeks (20 business days)

---

## Week 1: Critical Path - IDE Launch (Priority 10)

### Objective
Get the IDE running without Qt - unblock UI development for agentic features

### Tasks

#### Day 1-2: MainWindow.cpp Migration (4 hours estimated)
**File**: `src/qtapp/MainWindow.cpp` (39 lines)

**Current Architecture**:
- Inherits from `QMainWindow` with `Q_OBJECT`
- Uses `QVBoxLayout`, `QPushButton`, `connect()` slots
- Menu/toolbar creation via Qt API

**Target DLL**: `RawrXD_MainWindow_Win32.dll`

**Checklist**:
- [ ] Create `MainWindow_Win32.cpp` from template in MIGRATION_EXAMPLES.md
- [ ] Port menu creation to `CreateMenu`/`AppendMenu`
- [ ] Port toolbar to Win32 `TOOLBARCLASSNAME`
- [ ] Port button callbacks to `std::function`
- [ ] Remove Q_OBJECT, #include <Q*>
- [ ] Update CMakeLists.txt to link RawrXD_MainWindow_Win32.lib
- [ ] Compile & verify zero Qt dependencies
- [ ] Test window creation, menu clicks

**Blockers**: None (Foundation already complete)

**Verification**:
```powershell
dumpbin /dependents .\RawrXD_IDE.exe | findstr Qt
# Should return: NOTHING (empty result)
```

---

#### Day 2-3: main_qt.cpp Migration (2 hours estimated)
**File**: `src/qtapp/main_qt.cpp` (entry point)

**Current Architecture**:
- `QApplication app(argc, argv)`
- Qt event loop processing
- Qt plugin loading

**Target**: `RawrXD_Foundation.dll` + `RawrXD_IDE.exe` (Win32 entry point)

**Checklist**:
- [ ] Create `main_win32.cpp` using `wWinMain(HINSTANCE, ...)`
- [ ] Replace `QApplication` with Foundation initialization
- [ ] Load all DLLs manually via `LoadLibraryW`
- [ ] Replace Qt event loop with Win32 message loop
- [ ] Remove Q_OBJECT, #include <Q*>
- [ ] Update CMakeLists.txt
- [ ] Compile & test IDE launch

**Blockers**: MainWindow_Win32.dll must be ready

**Verification**:
```powershell
./RawrXD_IDE.exe
# Should launch with native window (no Qt splash screen)
```

---

#### Day 3-4: TerminalWidget.cpp Migration (3 hours estimated)
**File**: `src/qtapp/TerminalWidget.cpp` (Terminal integration)

**Current Architecture**:
- Inherits from `QWidget`
- Uses `QProcess` for process execution
- `QThread` for background processing
- `QPlainTextEdit` for output display

**Target DLL**: `RawrXD_TerminalManager_Win32.dll`

**Checklist**:
- [ ] Port `QProcess` → `CreateProcessW` + pipe management
- [ ] Port `QThread` → `std::thread`
- [ ] Port `QPlainTextEdit` display → RichEdit control or simple buffer
- [ ] Implement STDIN/STDOUT/STDERR pipe redirection
- [ ] Add process timeout handling
- [ ] Remove all Qt dependencies
- [ ] Compile & verify zero Qt refs
- [ ] Test: Launch process, capture output

**Blockers**: None (uses pure Win32 patterns)

**Verification**:
```powershell
# Test terminal execution
.\RawrXD_IDE.exe
# In terminal: Run a command, verify output appears
```

---

#### Day 4-5: Integration & Verification (3 hours estimated)

**Checklist**:
- [ ] Build entire solution: `cmake --build . --config Release`
- [ ] Run `Verify-QtRemoval.ps1` - expect 4/4 PASS
- [ ] Check binary sizes (should be similar or smaller than Qt versions)
- [ ] Launch IDE: `./RawrXD_IDE.exe`
  - [ ] Main window appears
  - [ ] Menus work
  - [ ] Terminal widget loads
  - [ ] No Qt splash screen
- [ ] Verify all DLLs copied to output directory
- [ ] dumpbin analysis: Zero Qt.dll dependencies
- [ ] Update Migration Tracker: Week 1 = COMPLETE

**Success Criteria**:
- ✅ IDE launches without Qt
- ✅ UI is responsive (native Win32 performance)
- ✅ dumpbin shows zero Qt dependencies
- ✅ All 3 critical components working

---

## Week 2: Core Logic - Agentic Features (Priority 8-9)

### Objective
Port agentic reasoning engine and plan orchestration to Win32

### High-Priority Files (8 hours + 12 hours = 20 hours)

#### agentic_executor.cpp (3 hours)
- Replace `QProcess` with `CreateProcessW`
- Replace `QThread` with `std::thread`
- Remove `QTimer` dependencies
- Target: `RawrXD_Executor.dll` (already exists, add Win32 wrappers)

#### agentic_text_edit.cpp (2 hours)
- Replace `QPlainTextEdit` with RichEdit control
- Remove `QTextCursor` → use direct buffer manipulation
- Target: `RawrXD_TextEditor_Win32.dll`

#### plan_orchestrator.cpp (4 hours)
- Replace `QObject` base class with plain C++ class
- Replace signals/slots with `std::function` callbacks
- Replace `QThread` with `std::thread` + `std::mutex`
- Target: `RawrXD_PlanOrchestrator.dll`

#### planning_agent.cpp (3 hours)
- Similar to plan_orchestrator
- Focus on threading and task scheduling
- Target: `RawrXD_AgentCoordinator.dll`

#### Other agentic/ files (8 hours)
- Batch process remaining `src/agentic/*.cpp`
- Apply same patterns repeatedly
- Most are 100-200 line files using similar Qt patterns

### Checklist for Week 2
- [ ] Run `Scan-QtDependencies.ps1` to identify all agentic files
- [ ] Start with highest-priority executor.cpp
- [ ] Use MIGRATION_EXAMPLES.md patterns
- [ ] Each file completion: Update Migration Tracker status
- [ ] After each component DLL built: Run `Verify-QtRemoval.ps1`
- [ ] Mid-week (Wednesday): Check progress vs. timeline
  - [ ] If behind: Skip low-impact files, focus on critical path
  - [ ] If ahead: Begin Week 3 prep

### Verification (End of Week 2)
```powershell
# Verify agentic components
dumpbin /dependents RawrXD_Executor.dll | findstr Qt
dumpbin /dependents RawrXD_PlanOrchestrator.dll | findstr Qt
dumpbin /dependents RawrXD_AgentCoordinator.dll | findstr Qt
# All should return: NOTHING
```

**Success Criteria**:
- ✅ All 10+ agentic files migrated
- ✅ All component DLLs built with zero Qt deps
- ✅ IDE still launches and integrates with new components
- ✅ Agentic features work end-to-end

---

## Week 3: Infrastructure Batch - Utilities & Helpers (Priority 5-6)

### Objective
Migrate 40+ utility/helper files - **high volume, low complexity**

### File Categories

#### Utilities (16 hours total)
- `qt_directory_manager.cpp` → `RawrXD_FileManager_Win32.dll`
  - Replace `QDir`, `QFileInfo` with Win32 FindFirstFileW/FindNextFileW
  - Estimated: 2 hours
  
- `qt_settings_wrapper.cpp` → `RawrXD_SettingsManager_Win32.dll`
  - Replace `QSettings` with Registry API (RegOpenKeyExW, RegQueryValueExW)
  - Estimated: 1 hour

- `qt_file_utils.cpp`, `qt_path_utils.cpp` → Utilities DLL
  - File path manipulation with `std::wstring`
  - Directory traversal, file existence checks
  - Estimated: 2 hours each (4 total)

- Other utility wrappers (8 files, ~8 hours)
  - Logging wrapper → direct fprintf or internal buffer
  - Config wrapper → Registry-based
  - Validators, formatters → pure C++20 std library
  - Estimated: 1 hour each

#### UI Components (12 hours)
- `src/ui/*.cpp` files
  - Text controls → RichEdit or native Win32 controls
  - Dialog wrappers → Win32 DialogBox API
  - Layout helpers → manual child positioning
  - Color/styling → GetSysColor or hardcoded Win32 approach
  - Estimated: 1-2 hours each

#### Agent System (8 hours)
- `src/agent/*.cpp` files (similar agentic patterns)
  - Q-based inheritance → plain C++ classes
  - Threading → std::thread
  - Containers → std::vector, std::map
  - Estimated: 1 hour each

### Batch Processing Strategy
1. **Monday**: Start with utilities (highest impact per hour)
2. **Tuesday-Wednesday**: Continue utils + UI components
3. **Thursday**: Agent system files
4. **Friday**: Remaining orphan files + buffer

### Checklist for Week 3
- [ ] Create batch processing script: `Migrate-UtilityBatch.ps1`
  - [ ] Accept folder pattern (e.g., `src/utils/*`)
  - [ ] Apply standard transformations
  - [ ] Generate report of what was migrated
  
- [ ] Daily checkpoint:
  - [ ] Build & verify no new compile errors
  - [ ] Run `Verify-QtRemoval.ps1` (should be cumulative improvement)
  - [ ] Update Migration Tracker
  
- [ ] Thursday mid-week assessment:
  - [ ] Count files remaining in src/
  - [ ] Calculate remaining effort
  - [ ] Adjust Week 4 plan if needed

### Verification (End of Week 3)
```powershell
# Count Qt dependencies remaining
Get-ChildItem D:\RawrXD\src -Recurse -Include *.cpp,*.hpp,*.h | 
  Select-String "#include\s*<Q" | Measure-Object

# Should show: ~50-100 remaining (down from 280+)
```

**Success Criteria**:
- ✅ 80% of files migrated or in-progress
- ✅ Build still succeeds
- ✅ All utility DLLs have zero Qt dependencies
- ✅ IDE fully functional with new utilities

---

## Week 4: Final Push & Verification (All Remaining)

### Objective
- Complete remaining 20% of files
- Full test suite validation
- Production readiness

### Remaining Categories

#### Orchestration (6 hours)
- `src/orchestration/*.cpp` files not migrated in Week 2
- Final task scheduling, workflow management
- Estimated: 1-2 hours each

#### Infrastructure (4 hours)
- Database wrappers (if using QSqlDatabase → SQLite direct API)
- Configuration management final pieces
- Logging system finalization

#### Tests (8 hours)
- Migrate or disable `src/test/*.cpp` files
  - Option A: Port to custom test framework
  - Option B: Disable Qt-based tests, rely on integration testing
  - Option C: Create new Win32-based test framework
- Estimated: 1-2 hours per test file (most can be disabled safely)

### Final Verification & Testing

#### Monday: Cleanup & Build
- [ ] Final scan for any missed Qt includes
```powershell
.\Scan-QtDependencies.ps1 | Select-String "CRITICAL|HIGH"
# Should return: EMPTY
```

- [ ] Full clean build
```powershell
Remove-Item build -Recurse -Force
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

- [ ] Check binary sizes & dependencies
```powershell
dir D:\RawrXD\Ship\*.dll, D:\RawrXD\Ship\*.exe | 
  ForEach-Object { dumpbin /dependents $_.FullName | Select-String Qt }
# Should return: NOTHING
```

#### Tuesday: Full Feature Testing
- [ ] Launch IDE
  - [ ] All menus work
  - [ ] All UI elements render
  - [ ] No crashes, freezes, or warnings
  
- [ ] Test agentic features
  - [ ] Plan creation works
  - [ ] Task execution works
  - [ ] Terminal integration works
  - [ ] Agent reasoning works
  
- [ ] Test utilities
  - [ ] File manager works
  - [ ] Settings persistence works
  - [ ] Logging works
  
- [ ] Stress testing
  - [ ] Long-running processes
  - [ ] Memory leaks check (task manager)
  - [ ] File load/save operations

#### Wednesday: Regression Testing
- [ ] Compare feature matrix: Original Qt vs. New Win32
  - [ ] UI responsiveness: ✓ (should be faster)
  - [ ] Memory usage: ✓ (should be comparable or lower)
  - [ ] Feature completeness: 100%
  
- [ ] Performance benchmarks
  - [ ] Plan execution time
  - [ ] Terminal response time
  - [ ] File operations speed

#### Thursday: Documentation & Cleanup
- [ ] Update all documentation
  - [ ] Architecture docs
  - [ ] Development setup docs
  - [ ] Migration completion report
  
- [ ] Code cleanup
  - [ ] Remove any #if 0 blocks
  - [ ] Clean up commented code
  - [ ] Verify code style consistency
  
- [ ] Final Migration Tracker update
```cpp
MigrationTracker::Instance().PrintProgressReport();
// Expected: 100% complete, all tasks "Done"
```

#### Friday: Validation & Production Readiness
- [ ] Final verification script
```powershell
.\Verify-QtRemoval.ps1
# Expected output: ALL CHECKS PASSED ✅
```

- [ ] Create final report
  - [ ] Summary statistics
  - [ ] Before/After comparison
  - [ ] List of all migrated components
  - [ ] Lessons learned
  - [ ] Performance metrics

- [ ] Sign-off checklist
  - [ ] No Qt DLL dependencies (verified via dumpbin)
  - [ ] All features working (verified via test)
  - [ ] Documentation complete
  - [ ] Team trained on new architecture
  - [ ] Production build ready

### Deliverables (End of Week 4)

1. **Qt Migration Complete**
   - 280+ files migrated from Qt to Win32
   - Zero Qt dependencies in entire codebase
   - Full feature parity maintained

2. **RawrXD_IDE.exe** (Qt-free)
   - Native Win32 implementation
   - Full UI working (menus, toolbars, dialogs)
   - Agentic features fully integrated
   - Performance optimized

3. **Component DLLs** (all Qt-free)
   - RawrXD_Foundation.dll
   - RawrXD_MainWindow_Win32.dll
   - RawrXD_TerminalManager_Win32.dll
   - RawrXD_Executor.dll
   - RawrXD_TextEditor_Win32.dll
   - RawrXD_PlanOrchestrator.dll
   - RawrXD_AgentCoordinator.dll
   - RawrXD_FileManager_Win32.dll
   - RawrXD_SettingsManager_Win32.dll
   - Plus any new components created during migration

4. **Documentation**
   - Migration completion report
   - Updated architecture documentation
   - Component interface specifications
   - Development guide for Win32 architecture

5. **Verification Report**
   ```
   ✅ ALL CHECKS PASSED
   
   - No Qt #include directives: 0 found
   - No Q_OBJECT macros: 0 found
   - Binary dependencies (Qt): 0 found
   - Foundation integration: PASS
   - Feature completeness: 100%
   - Performance regression: 0%
   ```

---

## Tracking & Metrics

### Daily Standup Template
```
Date: YYYY-MM-DD
Week: [1-4]
Status: On Schedule / At Risk / Behind

Completed Today:
- File 1: [status]
- File 2: [status]

In Progress:
- File 3: [% complete]

Blockers:
- [if any]

Tomorrow's Plan:
- File 4
- File 5
```

### Weekly Progress Metrics
```
Week 1:
- Files migrated: 3/3 (100%)
- Compilation errors: 0
- Test passes: Yes
- On schedule: Yes

Week 2:
- Files migrated: 12/15 (80%)
- Compilation errors: 0
- Test passes: Yes
- On schedule: Yes/No

Week 3:
- Files migrated: 50/60 (83%)
- Compilation errors: 0
- Test passes: Yes
- On schedule: Yes/No

Week 4:
- Files migrated: 280/280 (100%)
- Compilation errors: 0
- Test passes: Yes
- Production ready: Yes
```

---

## Tools & Automation

### Scripts Provided
1. **Scan-QtDependencies.ps1**
   - Run daily to track progress
   - Generates categorized file list

2. **Verify-QtRemoval.ps1**
   - Run after each major milestone
   - 4-point verification system

3. **QtMigrationTracker.hpp**
   - C++ class to track status
   - PrintProgressReport() for visibility

4. **MIGRATION_EXAMPLES.md**
   - Reference implementations
   - Copy/paste patterns for common cases

5. **CMAKE_MIGRATION_GUIDE.md**
   - CMakeLists.txt transformation
   - Build system updates

---

## Success Criteria (Final)

- [ ] **Zero Qt Dependencies**: `dumpbin` shows no Qt.dll files
- [ ] **100% Feature Parity**: All features work as in Qt version
- [ ] **Performance Maintained**: Startup/runtime speed ≥ original
- [ ] **Clean Build**: Zero warnings, zero errors
- [ ] **Tests Pass**: All 4 verification checks pass
- [ ] **Documentation Complete**: Architecture docs updated
- [ ] **Team Trained**: All developers understand Win32 patterns
- [ ] **Production Ready**: Code in main branch, ready to ship

---

## Rollback Plan (if needed)

If any critical issue arises:
1. Git revert to last stable commit
2. Debug the specific issue
3. Create minimal fix
4. Re-apply migration
5. Resume timeline

---

## Notes

- **Parallel Work**: Multiple developers can work on different components simultaneously
- **Dependency Management**: Complete Week 1 (IDE launch) before starting Week 2
- **Build Frequency**: Build and test after every 2-3 file migrations
- **Regression Prevention**: Run Verify-QtRemoval.ps1 every 4 hours

