<<<<<<< HEAD
╔════════════════════════════════════════════════════════════════════════════════╗
║                                                                                ║
║                  QT DEPENDENCY INVENTORY - CURRENT STATUS                      ║
║                                                                                ║
║              Which Files Have Qt, What Type, What to Do About It               ║
║                                                                                ║
╚════════════════════════════════════════════════════════════════════════════════╝


═══════════════════════════════════════════════════════════════════════════════════
 FILES NEEDING IMMEDIATE ATTENTION (PHASE 0 - Partial Migrations)
═══════════════════════════════════════════════════════════════════════════════════

1. D:\RawrXD\src\settings.cpp
   ────────────────────────────────────────────────────────────────────────────
   Status: ⚠️  PARTIALLY MIGRATED (Work started, incomplete)
   
   Current State:
     - Line 7: "Lightweight constructor - no // Settings initialization removed"
     - Line 25: Comment shows work in progress
     - Functions still broken (settings_=nullptr on line 8)
     - Still has QVariant type in signatures (line 12)
     - Still calls settings_->setValue, settings_->sync()
   
   What To Do:
     1. Replace QVariant type with typed C++ (int, string, double, bool)
     2. Replace QSettings initialization with file-based or Registry config
     3. Use RawrXD::FileOps from file_operations_win32.h for persistence
     4. OR use Win32 Registry API directly
   
   Expected Result:
     - Zero #include <Q...>
     - Settings stored in %APPDATA% or Registry
     - getValue/setValue work with std::string keys and typed values
   
   Task: 1 (Phase 0.1)
   Time: 30 mins


2. D:\RawrXD\src\ui\monaco_settings_dialog.cpp
   ────────────────────────────────────────────────────────────────────────────
   Status: ⚠️  PARTIALLY MIGRATED (Comments show work started)
   
   Current State:
     - Line 3: `QDialog(parent)` - still present!
     - Lines 19+: `QTabWidget(this)`, `QVBoxLayout(this)`, `QPushButton`, etc. all still present
     - Lines 45-52: Comments show "Signal connection removed" but declarations not replaced
     - Still using Qt UI classes (QGroupBox, QFormLayout, etc.)
   
   What To Do:
     1. Replace QDialog with Win32 CreateWindowEx
     2. Replace QVBoxLayout/QHBoxLayout with Win32 positioning (left/top/right/bottom)
     3. Replace QPushButton with Win32 button creation
     4. Replace QTabWidget with Win32 tab control
     5. Replace signal connections with Win32 message handlers (WM_COMMAND)
     6. Remove all #include <Q...>
   
   Expected Result:
     - Native Win32 dialog for Monaco editor settings
     - No Qt dependencies
     - Message-driven instead of signal/slot
   
   Task: 2 (Phase 0.2)
   Time: 1 hour
   
   Related Header:
     - D:\RawrXD\src\ui\monaco_settings_dialog.h


3. D:\RawrXD\src\qtapp\MainWindowSimple.cpp
   ────────────────────────────────────────────────────────────────────────────
   Status: ⚠️  PARTIALLY MIGRATED (Win32 started, Qt mixed in)
   
   Size: 2378 LINES (massive!)
   
   Current State:
     - Lines 1-20: Already using Win32 includes (richedit.h, commdlg.h, shlobj.h)
     - Line 30+: Constructor looks OK (HWND-based, not Qt)
     - Likely mixed: Some Win32, some Qt references throughout
   
   What To Do:
     1. Audit sections for remaining Qt (especially file dialogs, menus)
     2. Replace any QMenu with Win32 menus
     3. Replace QFileDialog with GetOpenFileName/GetSaveFileName
     4. Verify RichEdit usage is working (it's Win32, good)
     5. Remove any Q... type references
     6. Verify all 2378 lines compile without Qt
   
   Expected Result:
     - 2378 lines of pure Win32 main window code
     - Zero Qt anywhere
   
   Task: 3 (Phase 0.3)
   Time: 1.5 hours
   
   Related Files:
     - D:\RawrXD\src\qtapp\MainWindow.h (likely also needs audit)


═══════════════════════════════════════════════════════════════════════════════════
 FILES NEEDING ATTENTION (PHASE 1 - High Priority Classes)
═══════════════════════════════════════════════════════════════════════════════════

4. D:\RawrXD\src\qtapp\inference_engine.cpp
   ────────────────────────────────────────────────────────────────────────────
   Status: ❌ FULL QT DEPENDENCY
   
   Qt Usage:
     - Line 13: `InferenceEngine::InferenceEngine(const std::string& ggufPath, QObject* parent)`
     - Line 14: `: QObject(parent), m_loader(nullptr)` - QObject inheritance
     - Line 26: Another constructor with `QObject* parent`
     - Line 27: `: QObject(parent), m_loader(nullptr)` - Same inheritance
     - Line 428: `QFileInfo modelInfo(path);` - Qt file operations
     - Line 1002: Comment about marshaling back to QObject thread
   
   What To Do:
     1. Remove QObject from class declaration
     2. Replace QFileInfo with file_operations_win32.h calls
     3. Replace signal/slot async with callback-based async
     4. Remove std::thread / QThread coordination
     5. Use DLL's async execution (RawrXD_InferenceEngine.dll)
   
   Expected Result:
     - No QObject inheritance
     - Async via callbacks instead of signals
     - File ops via file_operations_win32.h
   
   Task: 4 (Phase 1.1)
   Time: 1 hour
   
   Related Header:
     - D:\RawrXD\src\qtapp\inference_engine.h


5. D:\RawrXD\src\autonomous_feature_engine.cpp
   ────────────────────────────────────────────────────────────────────────────
   Status: ❌ MASSIVE QT DEPENDENCY (150+ refs)
   
   File Size: ~760 lines
   
   Qt Usage (Examples):
     - Line 5-11: #include <QFile>, <QTextStream>, <QCryptographicHash>, <QRegularExpression>
     - Line 12: `AutonomousFeatureEngine::AutonomousFeatureEngine(QObject* parent) : QObject(parent)`
     - Line 44+: Methods use `QString code`, `QString filePath`, `QString language`
     - Line 235: `QVector<GeneratedTest> generateTestSuite(const QString& filePath)`
     - Line 238: `QFile file(filePath);`
     - Line 243: `QString code = QTextStream(&file).readAll();`
     - Line 265+: More methods with QString parameters
     - Line 272+: `QDateTime::currentMSecsSinceEpoch()` for timestamps
     - Throughout: QVector return types
   
   What To Do:
     1. Replace all QString with std::string (100+ replacements)
     2. Replace all QVector with std::vector (10+ replacements)
     3. Replace QFile/QTextStream with file_operations_win32.h
     4. Replace QRegularExpression with <regex>
     5. Replace QDateTime with time_t or std::chrono
     6. Remove QObject inheritance and signals
     7. Remove all #include <Q...>
   
   Expected Result:
     - Pure C++ feature engine
     - Uses std:: containers
     - File ops via DLL wrapper
   
   Task: 5 (Phase 1.2)
   Time: 1.5 hours (significant rewrite)


6. D:\RawrXD\src\universal_model_router.cpp
   ────────────────────────────────────────────────────────────────────────────
   Status: ❌ SIGNIFICANT QT DEPENDENCY (100+ refs)
   
   File Size: ~240 lines
   
   Qt Usage:
     - Line 4-9: #include <QJsonDocument>, <QJsonObject>, <QJsonArray>, <QFile>, <QDebug>, <QStandardPaths>
     - Line 11: `UniversalModelRouter::UniversalModelRouter(QObject* parent) : QObject(parent)`
     - Line 21+: Methods use QString parameters: `registerModel(const QString& model_name, ...)`
     - Line 24: `emit error(QString(...))` - signals
     - Line 39: `getModelConfig(const QString& model_name)` returns ModelConfig
     - Line 50: `QStringList getAvailableModels()`
     - Line 68+: `loadConfigFromFile(const QString& config_file_path)`
     - Line 70: `QFile file(config_file_path);`
     - Line 109: `QString backend_str = ... .toString().toUpper();`
     - Throughout: QJson parsing and QString manipulation
   
   What To Do:
     1. Replace QObject with function-based design (no inheritance needed)
     2. Replace QJsonDocument/QJsonObject with nlohmann::json
     3. Replace all QString with std::string (50+ replacements)
     4. Replace QStringList with std::vector<std::string>
     5. Replace QFile with file_operations_win32.h
     6. Replace emit signals with callback functions
     7. Remove all #include <Q...>
   
   Expected Result:
     - Pure C++ model router
     - Uses nlohmann::json for config
     - No Qt signals/slots
   
   Task: 6 (Phase 1.3)
   Time: 1 hour


═══════════════════════════════════════════════════════════════════════════════════
 TEST FILES TO DELETE (PHASE 2)
═══════════════════════════════════════════════════════════════════════════════════

7-16. Test Files (DELETE - no instrumentation needed)
   ────────────────────────────────────────────────────
   Status: ❌ QT TEST FRAMEWORK DEPENDENCY
   
   Files to remove:
     - D:\RawrXD\src\validate_agentic_tools.cpp
       Qt usage: QFile, QDir, QTemporaryDir, QString
       Reason: Redundant with Foundation test harness
     
     - D:\RawrXD\src\test_model_interface.cpp
       Qt usage: QCoreApplication, QTest framework, QString
       Reason: Redundant with Foundation test harness
     
     - D:\RawrXD\src\test_model_trainer_validation.cpp
       Qt usage: QCoreApplication, QDebug, QTest, QSignalSpy, QFile, QJsonDocument
       Reason: Redundant with Foundation test harness
     
     - D:\RawrXD\src\test_qmainwindow.cpp
       Qt usage: Qt test framework
       Reason: No longer needed for Win32 main window
     
     - D:\RawrXD\src\minimal_qt_test.cpp
       Qt usage: Qt framework
       Reason: Development artifact, not production
     
     - D:\RawrXD\src\agentic_ide_test.cpp
       Qt usage: Qt framework
       Reason: Foundation test harness handles validation
     
     - D:\RawrXD\src\qtapp\test_*.cpp (multiple files)
       Qt usage: Qt framework
       Reason: Redundant
     
     - Any other test files with Qt references
   
   Action: Delete all 10+ test files
   Reason: User explicit: "instrumentation and logging arent required"
           Foundation DLL has RawrXD_FoundationTest.exe for validation
   
   Task: 7 (Phase 2.1)
   Time: 15 mins


═══════════════════════════════════════════════════════════════════════════════════
 UNKNOWN STATUS FILES (PHASE 3 - Need Audit)
═══════════════════════════════════════════════════════════════════════════════════

17. D:\RawrXD\src\agentic\agentic_executor.cpp
    ─────────────────────────────────────────
    Status: ❓ UNKNOWN (Needs audit)
    
    Likely Has:
      - Possible std::thread usage (need to check for synchronization with Qt)
      - Possible Q... types if it coordinates with Qt signals
    
    Action: Grep for "#include <Q" - if found, migrate to Win32 threading
    Task: 8 (Phase 3.1)


18. D:\RawrXD\src\agentic\agentic_engine.cpp
    ──────────────────────────────────────────
    Status: ❓ UNKNOWN (Needs audit)
    
    Likely Has:
      - QThread usage for async inference
      - Signals for result delivery
    
    Action: Grep for "#include <Q" - if found, use DLL async
    Task: 8 (Phase 3.1)


19. D:\RawrXD\src\agentic\agentic_agent_coordinator.cpp
    ──────────────────────────────────────────────────
    Status: ❓ UNKNOWN (Needs audit)
    
    Likely Has:
      - QObject-based coordination
      - Signals for task distribution
    
    Action: Grep for "#include <Q" - if found, use DLL coordination
    Task: 8 (Phase 3.1)


20. All other src/agentic/*.cpp files
    ──────────────────────────────────
    Status: ❓ UNKNOWN (Needs audit)
    
    Action: For each file:
      1. Grep for "#include <Q"
      2. If found: Replace with equivalent (see patterns guide)
      3. If not found: Mark as clean
    
    Task: 8 (Phase 3.1)


21. D:\RawrXD\src\agent\agent_main.cpp
    ───────────────────────────────────
    Status: ❓ UNKNOWN (Needs audit)
    
    Likely Has:
      - QCoreApplication creation (main entry point)
      - Possible QObject coordination
    
    Action: Grep for "#include <Q" - if found, migrate to Win32
    Task: 9 (Phase 3.2)


22. All other src/agent/*.cpp files
    ────────────────────────────────
    Status: ❓ UNKNOWN (Needs audit)
    
    Action: For each file:
      1. Grep for "#include <Q"
      2. If found: Replace or delegate to agentic_core_win32.h
      3. If not found: Mark as clean
    
    Task: 9 (Phase 3.2)


23. D:\RawrXD\src\qtapp\MainWindow.cpp
    ──────────────────────────────────
    Status: ❓ UNKNOWN (Likely has Qt)
    
    Likely Has:
      - QMainWindow class definition
      - Menu/toolbar setup with Q... classes
    
    Action: Audit and migrate to Win32
    Task: 10 (Phase 3.3)


24. D:\RawrXD\src\qtapp\TerminalWidget.cpp
    ────────────────────────────────────────
    Status: ❓ UNKNOWN (Likely has Qt)
    
    Likely Has:
      - QWidget inheritance
      - Qt event handling
    
    Action: Audit and migrate to Win32 or retire to DLL
    Task: 10 (Phase 3.3)


25. All other src/qtapp/*.cpp files (not already handled)
    ─────────────────────────────────────────────────────
    Status: ❓ UNKNOWN (Needs audit)
    
    Action: For each file:
      1. Grep for "#include <Q"
      2. If found: Either migrate to Win32 or retire
      3. If not found: Mark as clean
    
    Task: 10 (Phase 3.3)


26. All src/ui/*.cpp files
    ─────────────────────
    Status: ❓ UNKNOWN (Likely has Qt)
    
    Likely Has:
      - Qt UI code
      - Widget hierarchies
    
    Action: Audit which files are critical vs decorative
           - Critical: Migrate to Win32
           - Decorative: Consider retiring
    
    Task: 10 (Phase 3.3)


═══════════════════════════════════════════════════════════════════════════════════
 BUILD SYSTEM (PHASE 4)
═══════════════════════════════════════════════════════════════════════════════════

27. D:\RawrXD\src\CMakeLists.txt
    ─────────────────────────────
    Status: ⚠️  NEEDS UPDATE
    
    Current State:
      - Still likely contains find_package(Qt5) or find_package(Qt6)
      - Still likely includes Qt directories
      - Still likely links Qt libraries
    
    What To Do:
      1. Remove: find_package(Qt5 COMPONENTS ...) or find_package(Qt6 ...)
      2. Remove: Any Qt include_directories
      3. Remove: Any Qt link_libraries (Qt5Core, Qt5Gui, etc.)
      4. Add: include_directories for Windows SDK paths
      5. Add: include_directories for D:\RawrXD\src
      6. Add: link_directories for D:\RawrXD\Ship
      7. Add: link_libraries for all 32 DLL .lib files:
              RawrXD_Foundation_Integration.lib
              RawrXD_CoreServices_Executor.lib
              RawrXD_CoreServices_AgentCoordinator.lib
              RawrXD_CoreServices_InferenceEngine.lib
              RawrXD_CoreServices_ConfigurationManager.lib
              (... and 27 more from Ship/)
      8. Set: Compiler flags /O2 /std:c++17
    
    Task: 11 (Phase 4.1)
    Time: 20 mins


═══════════════════════════════════════════════════════════════════════════════════
 VERIFICATION (PHASE 5)
═══════════════════════════════════════════════════════════════════════════════════

28. Grep Verification (ZERO Qt includes)
    ────────────────────────────────────
    Status: ⏳ TO BE DONE (After all phases)
    
    Command:
      Get-ChildItem -Path "D:\RawrXD\src" -Recurse -Include "*.cpp","*.h","*.hpp" | 
        Where-Object { Select-String -InputObject $_ -Pattern "#include\s+<Q" -Quiet }
    
    Expected Result: NO OUTPUT (empty)
    
    Task: 12 (Phase 5.1)
    Time: 5 mins


29. Build Verification
    ───────────────────
    Status: ⏳ TO BE DONE (After all phases)
    
    Command:
      cd D:\RawrXD\src
      build_win32_migration.bat
    
    Expected Result:
      - RawrXD_IDE_Win32.exe created in D:\RawrXD\Ship\
      - No compilation errors
      - No linker errors
      - Output: "=== BUILD COMPLETE ==="
    
    Task: 13 (Phase 5.2)
    Time: 10 mins


30. Dependency Check (dumpbin verification)
    ───────────────────────────────────────
    Status: ⏳ TO BE DONE (After Phase 5.2)
    
    Command:
      dumpbin /dependents D:\RawrXD\Ship\RawrXD_IDE_Win32.exe | 
        Select-String -Pattern "Qt5|Qt6"
    
    Expected Result: NO OUTPUT (empty - zero Qt dependencies)
    
    Task: 14 (Phase 5.3)
    Time: 5 mins


═══════════════════════════════════════════════════════════════════════════════════
 SUMMARY TABLE
═══════════════════════════════════════════════════════════════════════════════════

Priority | Task | File                                    | Status    | Phase | Time
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
🔴 CRIT  | 1   | settings.cpp                             | ⚠️  PARTIAL | 0.1  | 30m
🔴 CRIT  | 2   | monaco_settings_dialog.cpp               | ⚠️  PARTIAL | 0.2  | 1h
🔴 CRIT  | 3   | MainWindowSimple.cpp (2378 lines)        | ⚠️  PARTIAL | 0.3  | 1.5h
🔴 CRIT  | 4   | inference_engine.cpp                     | ❌ FULL    | 1.1  | 1h
🔴 CRIT  | 5   | autonomous_feature_engine.cpp            | ❌ FULL    | 1.2  | 1.5h
🔴 CRIT  | 6   | universal_model_router.cpp               | ❌ FULL    | 1.3  | 1h
🟡 MED   | 7   | test files (10+ files)                   | ❌ FULL    | 2.1  | 15m
🟡 MED   | 8   | agentic/ folder                          | ❓ UNKNOWN | 3.1  | 30m
🟡 MED   | 9   | agent/ folder                            | ❓ UNKNOWN | 3.2  | 30m
🟡 MED   | 10  | qtapp/ + ui/ remaining files             | ❓ UNKNOWN | 3.3  | 30m
🟡 MED   | 11  | CMakeLists.txt                           | ⚠️  NEEDS  | 4.1  | 20m
🟢 NICE  | 12  | Grep verification (0 Qt includes)        | ⏳ PENDING | 5.1  | 5m
🟢 NICE  | 13  | Build verification                       | ⏳ PENDING | 5.2  | 10m
🟢 NICE  | 14  | Dependency check (dumpbin)               | ⏳ PENDING | 5.3  | 5m


═══════════════════════════════════════════════════════════════════════════════════
 NEXT ACTION
═══════════════════════════════════════════════════════════════════════════════════

START WITH TASK 1:

Open D:\RawrXD\src\settings.cpp
├─ Look for QVariant type references
├─ Look for QSettings calls
├─ Replace with file-based or Registry config
└─ Build and verify

You've got the full inventory. You know exactly what to do. Go get 'em! 🚀

═══════════════════════════════════════════════════════════════════════════════════
=======
╔════════════════════════════════════════════════════════════════════════════════╗
║                                                                                ║
║                  QT DEPENDENCY INVENTORY - CURRENT STATUS                      ║
║                                                                                ║
║              Which Files Have Qt, What Type, What to Do About It               ║
║                                                                                ║
╚════════════════════════════════════════════════════════════════════════════════╝

**Note (2026-02-14):** Many Phase 0 paths below (e.g. src/settings.cpp, src/qtapp/,
src/ui/monaco_settings_dialog Qt version) no longer exist in the repo or are already
Win32. See docs/QT_TO_WIN32_IDE_AUDIT.md for current Qt→Win32 IDE/CLI status.

═══════════════════════════════════════════════════════════════════════════════════
 FILES NEEDING IMMEDIATE ATTENTION (PHASE 0 - Partial Migrations)
═══════════════════════════════════════════════════════════════════════════════════

1. D:\RawrXD\src\settings.cpp  [REMOVED — file not in repo; config via Registry/Win32]
   ────────────────────────────────────────────────────────────────────────────
   Status: ⚠️  PARTIALLY MIGRATED (Work started, incomplete)
   
   Current State:
     - Line 7: "Lightweight constructor - no // Settings initialization removed"
     - Line 25: Comment shows work in progress
     - Functions still broken (settings_=nullptr on line 8)
     - Still has QVariant type in signatures (line 12)
     - Still calls settings_->setValue, settings_->sync()
   
   What To Do:
     1. Replace QVariant type with typed C++ (int, string, double, bool)
     2. Replace QSettings initialization with file-based or Registry config
     3. Use RawrXD::FileOps from file_operations_win32.h for persistence
     4. OR use Win32 Registry API directly
   
   Expected Result:
     - Zero #include <Q...>
     - Settings stored in %APPDATA% or Registry
     - getValue/setValue work with std::string keys and typed values
   
   Task: 1 (Phase 0.1)
   Time: 30 mins


2. D:\RawrXD\src\ui\monaco_settings_dialog.cpp
   ────────────────────────────────────────────────────────────────────────────
   Status: ⚠️  PARTIALLY MIGRATED (Comments show work started)
   
   Current State:
     - Line 3: `QDialog(parent)` - still present!
     - Lines 19+: `QTabWidget(this)`, `QVBoxLayout(this)`, `QPushButton`, etc. all still present
     - Lines 45-52: Comments show "Signal connection removed" but declarations not replaced
     - Still using Qt UI classes (QGroupBox, QFormLayout, etc.)
   
   What To Do:
     1. Replace QDialog with Win32 CreateWindowEx
     2. Replace QVBoxLayout/QHBoxLayout with Win32 positioning (left/top/right/bottom)
     3. Replace QPushButton with Win32 button creation
     4. Replace QTabWidget with Win32 tab control
     5. Replace signal connections with Win32 message handlers (WM_COMMAND)
     6. Remove all #include <Q...>
   
   Expected Result:
     - Native Win32 dialog for Monaco editor settings
     - No Qt dependencies
     - Message-driven instead of signal/slot
   
   Task: 2 (Phase 0.2)
   Time: 1 hour
   
   Related Header:
     - D:\RawrXD\src\ui\monaco_settings_dialog.h


3. D:\RawrXD\src\qtapp\MainWindowSimple.cpp
   ────────────────────────────────────────────────────────────────────────────
   Status: ⚠️  PARTIALLY MIGRATED (Win32 started, Qt mixed in)
   
   Size: 2378 LINES (massive!)
   
   Current State:
     - Lines 1-20: Already using Win32 includes (richedit.h, commdlg.h, shlobj.h)
     - Line 30+: Constructor looks OK (HWND-based, not Qt)
     - Likely mixed: Some Win32, some Qt references throughout
   
   What To Do:
     1. Audit sections for remaining Qt (especially file dialogs, menus)
     2. Replace any QMenu with Win32 menus
     3. Replace QFileDialog with GetOpenFileName/GetSaveFileName
     4. Verify RichEdit usage is working (it's Win32, good)
     5. Remove any Q... type references
     6. Verify all 2378 lines compile without Qt
   
   Expected Result:
     - 2378 lines of pure Win32 main window code
     - Zero Qt anywhere
   
   Task: 3 (Phase 0.3)
   Time: 1.5 hours
   
   Related Files:
     - D:\RawrXD\src\qtapp\MainWindow.h (likely also needs audit)


═══════════════════════════════════════════════════════════════════════════════════
 FILES NEEDING ATTENTION (PHASE 1 - High Priority Classes)
═══════════════════════════════════════════════════════════════════════════════════

4. D:\RawrXD\src\qtapp\inference_engine.cpp
   ────────────────────────────────────────────────────────────────────────────
   Status: ❌ FULL QT DEPENDENCY
   
   Qt Usage:
     - Line 13: `InferenceEngine::InferenceEngine(const std::string& ggufPath, QObject* parent)`
     - Line 14: `: QObject(parent), m_loader(nullptr)` - QObject inheritance
     - Line 26: Another constructor with `QObject* parent`
     - Line 27: `: QObject(parent), m_loader(nullptr)` - Same inheritance
     - Line 428: `QFileInfo modelInfo(path);` - Qt file operations
     - Line 1002: Comment about marshaling back to QObject thread
   
   What To Do:
     1. Remove QObject from class declaration
     2. Replace QFileInfo with file_operations_win32.h calls
     3. Replace signal/slot async with callback-based async
     4. Remove std::thread / QThread coordination
     5. Use DLL's async execution (RawrXD_InferenceEngine.dll)
   
   Expected Result:
     - No QObject inheritance
     - Async via callbacks instead of signals
     - File ops via file_operations_win32.h
   
   Task: 4 (Phase 1.1)
   Time: 1 hour
   
   Related Header:
     - D:\RawrXD\src\qtapp\inference_engine.h


5. D:\RawrXD\src\autonomous_feature_engine.cpp
   ────────────────────────────────────────────────────────────────────────────
   Status: ❌ MASSIVE QT DEPENDENCY (150+ refs)
   
   File Size: ~760 lines
   
   Qt Usage (Examples):
     - Line 5-11: #include <QFile>, <QTextStream>, <QCryptographicHash>, <QRegularExpression>
     - Line 12: `AutonomousFeatureEngine::AutonomousFeatureEngine(QObject* parent) : QObject(parent)`
     - Line 44+: Methods use `QString code`, `QString filePath`, `QString language`
     - Line 235: `QVector<GeneratedTest> generateTestSuite(const QString& filePath)`
     - Line 238: `QFile file(filePath);`
     - Line 243: `QString code = QTextStream(&file).readAll();`
     - Line 265+: More methods with QString parameters
     - Line 272+: `QDateTime::currentMSecsSinceEpoch()` for timestamps
     - Throughout: QVector return types
   
   What To Do:
     1. Replace all QString with std::string (100+ replacements)
     2. Replace all QVector with std::vector (10+ replacements)
     3. Replace QFile/QTextStream with file_operations_win32.h
     4. Replace QRegularExpression with <regex>
     5. Replace QDateTime with time_t or std::chrono
     6. Remove QObject inheritance and signals
     7. Remove all #include <Q...>
   
   Expected Result:
     - Pure C++ feature engine
     - Uses std:: containers
     - File ops via DLL wrapper
   
   Task: 5 (Phase 1.2)
   Time: 1.5 hours (significant rewrite)


6. D:\RawrXD\src\universal_model_router.cpp
   ────────────────────────────────────────────────────────────────────────────
   Status: ❌ SIGNIFICANT QT DEPENDENCY (100+ refs)
   
   File Size: ~240 lines
   
   Qt Usage:
     - Line 4-9: #include <QJsonDocument>, <QJsonObject>, <QJsonArray>, <QFile>, <QDebug>, <QStandardPaths>
     - Line 11: `UniversalModelRouter::UniversalModelRouter(QObject* parent) : QObject(parent)`
     - Line 21+: Methods use QString parameters: `registerModel(const QString& model_name, ...)`
     - Line 24: `emit error(QString(...))` - signals
     - Line 39: `getModelConfig(const QString& model_name)` returns ModelConfig
     - Line 50: `QStringList getAvailableModels()`
     - Line 68+: `loadConfigFromFile(const QString& config_file_path)`
     - Line 70: `QFile file(config_file_path);`
     - Line 109: `QString backend_str = ... .toString().toUpper();`
     - Throughout: QJson parsing and QString manipulation
   
   What To Do:
     1. Replace QObject with function-based design (no inheritance needed)
     2. Replace QJsonDocument/QJsonObject with nlohmann::json
     3. Replace all QString with std::string (50+ replacements)
     4. Replace QStringList with std::vector<std::string>
     5. Replace QFile with file_operations_win32.h
     6. Replace emit signals with callback functions
     7. Remove all #include <Q...>
   
   Expected Result:
     - Pure C++ model router
     - Uses nlohmann::json for config
     - No Qt signals/slots
   
   Task: 6 (Phase 1.3)
   Time: 1 hour


═══════════════════════════════════════════════════════════════════════════════════
 TEST FILES TO DELETE (PHASE 2)
═══════════════════════════════════════════════════════════════════════════════════

7-16. Test Files (DELETE - no instrumentation needed)
   ────────────────────────────────────────────────────
   Status: ❌ QT TEST FRAMEWORK DEPENDENCY
   
   Files to remove:
     - D:\RawrXD\src\validate_agentic_tools.cpp
       Qt usage: QFile, QDir, QTemporaryDir, QString
       Reason: Redundant with Foundation test harness
     
     - D:\RawrXD\src\test_model_interface.cpp
       Qt usage: QCoreApplication, QTest framework, QString
       Reason: Redundant with Foundation test harness
     
     - D:\RawrXD\src\test_model_trainer_validation.cpp
       Qt usage: QCoreApplication, QDebug, QTest, QSignalSpy, QFile, QJsonDocument
       Reason: Redundant with Foundation test harness
     
     - D:\RawrXD\src\test_qmainwindow.cpp
       Qt usage: Qt test framework
       Reason: No longer needed for Win32 main window
     
     - D:\RawrXD\src\minimal_qt_test.cpp
       Qt usage: Qt framework
       Reason: Development artifact, not production
     
     - D:\RawrXD\src\agentic_ide_test.cpp
       Qt usage: Qt framework
       Reason: Foundation test harness handles validation
     
     - D:\RawrXD\src\qtapp\test_*.cpp (multiple files)
       Qt usage: Qt framework
       Reason: Redundant
     
     - Any other test files with Qt references
   
   Action: Delete all 10+ test files
   Reason: User explicit: "instrumentation and logging arent required"
           Foundation DLL has RawrXD_FoundationTest.exe for validation
   
   Task: 7 (Phase 2.1)
   Time: 15 mins


═══════════════════════════════════════════════════════════════════════════════════
 UNKNOWN STATUS FILES (PHASE 3 - Need Audit)
═══════════════════════════════════════════════════════════════════════════════════

17. D:\RawrXD\src\agentic\agentic_executor.cpp
    ─────────────────────────────────────────
    Status: ❓ UNKNOWN (Needs audit)
    
    Likely Has:
      - Possible std::thread usage (need to check for synchronization with Qt)
      - Possible Q... types if it coordinates with Qt signals
    
    Action: Grep for "#include <Q" - if found, migrate to Win32 threading
    Task: 8 (Phase 3.1)


18. D:\RawrXD\src\agentic\agentic_engine.cpp
    ──────────────────────────────────────────
    Status: ❓ UNKNOWN (Needs audit)
    
    Likely Has:
      - QThread usage for async inference
      - Signals for result delivery
    
    Action: Grep for "#include <Q" - if found, use DLL async
    Task: 8 (Phase 3.1)


19. D:\RawrXD\src\agentic\agentic_agent_coordinator.cpp
    ──────────────────────────────────────────────────
    Status: ❓ UNKNOWN (Needs audit)
    
    Likely Has:
      - QObject-based coordination
      - Signals for task distribution
    
    Action: Grep for "#include <Q" - if found, use DLL coordination
    Task: 8 (Phase 3.1)


20. All other src/agentic/*.cpp files
    ──────────────────────────────────
    Status: ❓ UNKNOWN (Needs audit)
    
    Action: For each file:
      1. Grep for "#include <Q"
      2. If found: Replace with equivalent (see patterns guide)
      3. If not found: Mark as clean
    
    Task: 8 (Phase 3.1)


21. D:\RawrXD\src\agent\agent_main.cpp
    ───────────────────────────────────
    Status: ❓ UNKNOWN (Needs audit)
    
    Likely Has:
      - QCoreApplication creation (main entry point)
      - Possible QObject coordination
    
    Action: Grep for "#include <Q" - if found, migrate to Win32
    Task: 9 (Phase 3.2)


22. All other src/agent/*.cpp files
    ────────────────────────────────
    Status: ❓ UNKNOWN (Needs audit)
    
    Action: For each file:
      1. Grep for "#include <Q"
      2. If found: Replace or delegate to agentic_core_win32.h
      3. If not found: Mark as clean
    
    Task: 9 (Phase 3.2)


23. D:\RawrXD\src\qtapp\MainWindow.cpp
    ──────────────────────────────────
    Status: ❓ UNKNOWN (Likely has Qt)
    
    Likely Has:
      - QMainWindow class definition
      - Menu/toolbar setup with Q... classes
    
    Action: Audit and migrate to Win32
    Task: 10 (Phase 3.3)


24. D:\RawrXD\src\qtapp\TerminalWidget.cpp
    ────────────────────────────────────────
    Status: ❓ UNKNOWN (Likely has Qt)
    
    Likely Has:
      - QWidget inheritance
      - Qt event handling
    
    Action: Audit and migrate to Win32 or retire to DLL
    Task: 10 (Phase 3.3)


25. All other src/qtapp/*.cpp files (not already handled)
    ─────────────────────────────────────────────────────
    Status: ❓ UNKNOWN (Needs audit)
    
    Action: For each file:
      1. Grep for "#include <Q"
      2. If found: Either migrate to Win32 or retire
      3. If not found: Mark as clean
    
    Task: 10 (Phase 3.3)


26. All src/ui/*.cpp files
    ─────────────────────
    Status: ❓ UNKNOWN (Likely has Qt)
    
    Likely Has:
      - Qt UI code
      - Widget hierarchies
    
    Action: Audit which files are critical vs decorative
           - Critical: Migrate to Win32
           - Decorative: Consider retiring
    
    Task: 10 (Phase 3.3)


═══════════════════════════════════════════════════════════════════════════════════
 BUILD SYSTEM (PHASE 4)
═══════════════════════════════════════════════════════════════════════════════════

27. D:\RawrXD\src\CMakeLists.txt
    ─────────────────────────────
    Status: ⚠️  NEEDS UPDATE
    
    Current State:
      - Still likely contains find_package(Qt5) or find_package(Qt6)
      - Still likely includes Qt directories
      - Still likely links Qt libraries
    
    What To Do:
      1. Remove: find_package(Qt5 COMPONENTS ...) or find_package(Qt6 ...)
      2. Remove: Any Qt include_directories
      3. Remove: Any Qt link_libraries (Qt5Core, Qt5Gui, etc.)
      4. Add: include_directories for Windows SDK paths
      5. Add: include_directories for D:\RawrXD\src
      6. Add: link_directories for D:\RawrXD\Ship
      7. Add: link_libraries for all 32 DLL .lib files:
              RawrXD_Foundation_Integration.lib
              RawrXD_CoreServices_Executor.lib
              RawrXD_CoreServices_AgentCoordinator.lib
              RawrXD_CoreServices_InferenceEngine.lib
              RawrXD_CoreServices_ConfigurationManager.lib
              (... and 27 more from Ship/)
      8. Set: Compiler flags /O2 /std:c++17
    
    Task: 11 (Phase 4.1)
    Time: 20 mins


═══════════════════════════════════════════════════════════════════════════════════
 VERIFICATION (PHASE 5)
═══════════════════════════════════════════════════════════════════════════════════

28. Grep Verification (ZERO Qt includes)
    ────────────────────────────────────
    Status: ⏳ TO BE DONE (After all phases)
    
    Command:
      Get-ChildItem -Path "D:\RawrXD\src" -Recurse -Include "*.cpp","*.h","*.hpp" | 
        Where-Object { Select-String -InputObject $_ -Pattern "#include\s+<Q" -Quiet }
    
    Expected Result: NO OUTPUT (empty)
    
    Task: 12 (Phase 5.1)
    Time: 5 mins


29. Build Verification
    ───────────────────
    Status: ⏳ TO BE DONE (After all phases)
    
    Command:
      cd D:\RawrXD\src
      build_win32_migration.bat
    
    Expected Result:
      - RawrXD_IDE_Win32.exe created in D:\RawrXD\Ship\
      - No compilation errors
      - No linker errors
      - Output: "=== BUILD COMPLETE ==="
    
    Task: 13 (Phase 5.2)
    Time: 10 mins


30. Dependency Check (dumpbin verification)
    ───────────────────────────────────────
    Status: ⏳ TO BE DONE (After Phase 5.2)
    
    Command:
      dumpbin /dependents D:\RawrXD\Ship\RawrXD_IDE_Win32.exe | 
        Select-String -Pattern "Qt5|Qt6"
    
    Expected Result: NO OUTPUT (empty - zero Qt dependencies)
    
    Task: 14 (Phase 5.3)
    Time: 5 mins


═══════════════════════════════════════════════════════════════════════════════════
 SUMMARY TABLE
═══════════════════════════════════════════════════════════════════════════════════

Priority | Task | File                                    | Status    | Phase | Time
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
🔴 CRIT  | 1   | settings.cpp                             | ⚠️  PARTIAL | 0.1  | 30m
🔴 CRIT  | 2   | monaco_settings_dialog.cpp               | ⚠️  PARTIAL | 0.2  | 1h
🔴 CRIT  | 3   | MainWindowSimple.cpp (2378 lines)        | ⚠️  PARTIAL | 0.3  | 1.5h
🔴 CRIT  | 4   | inference_engine.cpp                     | ❌ FULL    | 1.1  | 1h
🔴 CRIT  | 5   | autonomous_feature_engine.cpp            | ❌ FULL    | 1.2  | 1.5h
🔴 CRIT  | 6   | universal_model_router.cpp               | ❌ FULL    | 1.3  | 1h
🟡 MED   | 7   | test files (10+ files)                   | ❌ FULL    | 2.1  | 15m
🟡 MED   | 8   | agentic/ folder                          | ❓ UNKNOWN | 3.1  | 30m
🟡 MED   | 9   | agent/ folder                            | ❓ UNKNOWN | 3.2  | 30m
🟡 MED   | 10  | qtapp/ + ui/ remaining files             | ❓ UNKNOWN | 3.3  | 30m
🟡 MED   | 11  | CMakeLists.txt                           | ⚠️  NEEDS  | 4.1  | 20m
🟢 NICE  | 12  | Grep verification (0 Qt includes)        | ⏳ PENDING | 5.1  | 5m
🟢 NICE  | 13  | Build verification                       | ⏳ PENDING | 5.2  | 10m
🟢 NICE  | 14  | Dependency check (dumpbin)               | ⏳ PENDING | 5.3  | 5m


═══════════════════════════════════════════════════════════════════════════════════
 NEXT ACTION
═══════════════════════════════════════════════════════════════════════════════════

START WITH TASK 1:

Open D:\RawrXD\src\settings.cpp
├─ Look for QVariant type references
├─ Look for QSettings calls
├─ Replace with file-based or Registry config
└─ Build and verify

You've got the full inventory. You know exactly what to do. Go get 'em! 🚀

═══════════════════════════════════════════════════════════════════════════════════
>>>>>>> origin/main
