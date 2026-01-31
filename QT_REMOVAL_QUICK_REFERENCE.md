╔════════════════════════════════════════════════════════════════════════════════╗
║                                                                                ║
║                    QT REMOVAL - QUICK REFERENCE CARD                           ║
║                                                                                ║
║                          What, Where, How, When                                ║
║                                                                                ║
╚════════════════════════════════════════════════════════════════════════════════╝


═══════════════════════════════════════════════════════════════════════════════════
 THE FOUR NUMBERS YOU NEED
═══════════════════════════════════════════════════════════════════════════════════

14 tasks total
9 hours sequential (6 hours parallel)
~15-20 problem files
ZERO Qt in final build (100% verified)


═══════════════════════════════════════════════════════════════════════════════════
 WHAT NEEDS TO GO
═══════════════════════════════════════════════════════════════════════════════════

REMOVE THESE:                              REPLACE WITH:
──────────────────────────────────────────────────────────────────────────────────
#include <QObject>                      → agentic_core_win32.h
#include <QThread>                      → Win32 CreateThread or DLL async
#include <QString>                      → std::string
#include <QVariant>                     → Typed C++ values
#include <QVector>                      → std::vector<>
#include <QFile>                        → file_operations_win32.h
#include <QDir>                         → file_operations_win32.h
#include <QFileInfo>                    → file_operations_win32.h
#include <QSettings>                    → Win32 Registry or file-based config
#include <QMainWindow>                  → Win32 CreateWindowEx
#include <QWidget>                      → Win32 CreateWindowEx
#include <QDialog>                      → Win32 CreateWindowEx
#include <QJsonDocument>                → nlohmann::json
#include <QJsonObject>                  → nlohmann::json
#include <QRegularExpression>           → <regex>
#include <QDateTime>                    → time_t or chrono
#include <QTest>                        → Delete (Foundation has test harness)
#include <QSignalSpy>                   → Delete (no signals/slots needed)


═══════════════════════════════════════════════════════════════════════════════════
 PATTERNS TO USE
═══════════════════════════════════════════════════════════════════════════════════

Pattern 1: File Operations
──────────────────────────
OLD:
    #include <QFile>
    #include <QDir>
    QString filename = "test.txt";
    QFile file(filename);
    if (file.open(QIODevice::ReadOnly))
        QString content = QTextStream(&file).readAll();

NEW:
    #include "file_operations_win32.h"
    std::string filename = "test.txt";
    auto content = RawrXD::FileOps::FileManager::instance()
                       .readFile(filename);


Pattern 2: QObject → DLL Wrapper
────────────────────────────────
OLD:
    class InferenceEngine : public QObject {
        Q_OBJECT
    signals:
        void resultReady(const QString& result);
    };

NEW:
    #include "agentic_core_win32.h"
    class InferenceEngine {
        using ResultCallback = std::function<void(const std::string&)>;
        bool submitInference(const std::string& prompt, ResultCallback cb) {
            return RawrXD::Agentic::InferenceEngine::instance()
                       .submitInference(prompt, ???);
        }
    };


Pattern 3: QString → std::string
────────────────────────────────
OLD:
    QString str = "hello";
    int len = str.length();
    QString upper = str.toUpper();

NEW:
    std::string str = "hello";
    size_t len = str.length();
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);


Pattern 4: QJson → nlohmann::json
─────────────────────────────────
OLD:
    #include <QJsonDocument>
    #include <QJsonObject>
    QJsonDocument doc = QJsonDocument::fromJson(jsonString);
    QString value = doc.object()["key"].toString();

NEW:
    #include <nlohmann/json.hpp>
    auto doc = nlohmann::json::parse(jsonString);
    std::string value = doc["key"].get<std::string>();


Pattern 5: Async Operations
──────────────────────────
OLD:
    class Worker : public QObject {
        Q_OBJECT
    signals:
        void finished(const QString& result);
    public slots:
        void doWork() { ... emit finished(...); }
    };

NEW:
    using AsyncCallback = std::function<void(const std::string&)>;
    class Worker {
        void doWork(AsyncCallback on_complete) {
            // Do work synchronously or via DLL
            on_complete(result);  // Call callback when done
        }
    };


═══════════════════════════════════════════════════════════════════════════════════
 THE 5-PHASE PLAN
═══════════════════════════════════════════════════════════════════════════════════

Phase 0 (3 hours):  Complete partial migrations
              ├─ settings.cpp (30 min)
              ├─ monaco_settings_dialog (1 hour)
              └─ MainWindowSimple.cpp (1.5 hours)

Phase 1 (3.5 hours): Fix high-priority classes
              ├─ inference_engine.cpp (1 hour)
              ├─ autonomous_feature_engine.cpp (1.5 hours)
              └─ universal_model_router.cpp (1 hour)

Phase 2 (15 min):  Delete redundant tests
              └─ 10+ test files using Qt test framework

Phase 3 (1.5 hours): Audit remaining folders
              ├─ agentic/ (30 min)
              ├─ agent/ (30 min)
              └─ qtapp/ (30 min)

Phase 4 (20 min):  Update build system
              └─ CMakeLists.txt (remove Qt, add DLLs)

Phase 5 (20 min):  Verify completion
              ├─ Grep verification (5 min)
              ├─ Build verification (10 min)
              └─ Dependency check (5 min)


═══════════════════════════════════════════════════════════════════════════════════
 FILES YOU'LL EDIT MOST
═══════════════════════════════════════════════════════════════════════════════════

CRITICAL (Edit heavily):
  D:\RawrXD\src\settings.cpp                    [~2KB]
  D:\RawrXD\src\ui\monaco_settings_dialog.cpp   [~690 lines]
  D:\RawrXD\src\qtapp\MainWindowSimple.cpp      [~2378 lines!!!]
  D:\RawrXD\src\qtapp\inference_engine.cpp      [~1000 lines]
  D:\RawrXD\src\autonomous_feature_engine.cpp   [~760 lines]
  D:\RawrXD\src\universal_model_router.cpp      [~240 lines]

MODERATE (Edit some):
  D:\RawrXD\src\agentic\agentic_executor.cpp
  D:\RawrXD\src\agentic\agentic_engine.cpp
  D:\RawrXD\src\agent\agent_main.cpp

LIGHT (Audit/delete):
  ~10 test files (validate_agentic_tools.cpp, test_model_interface.cpp, etc.)


═══════════════════════════════════════════════════════════════════════════════════
 COMMANDS YOU'LL RUN
═══════════════════════════════════════════════════════════════════════════════════

1. TEST FOR QT (Find problem files):
   Get-ChildItem -Path "D:\RawrXD\src" -Recurse -Include "*.cpp","*.h","*.hpp" | 
     Where-Object { Select-String -InputObject $_ -Pattern "#include\s+<Q" -Quiet }

2. COUNT QT FILES (Before/after):
   $files = Get-ChildItem -Path "D:\RawrXD\src" -Recurse -Include "*.cpp","*.h","*.hpp" | 
     Where-Object { Select-String -InputObject $_ -Pattern "#include\s+<Q" -Quiet }
   $files.Count

3. BUILD IT:
   cd D:\RawrXD\src
   build_win32_migration.bat

4. CHECK DEPENDENCIES:
   dumpbin /dependents D:\RawrXD\Ship\RawrXD_IDE_Win32.exe | Select-String -Pattern "Qt5|Qt6"

5. RUN THE TEST:
   D:\RawrXD\Ship\RawrXD_FoundationTest.exe


═══════════════════════════════════════════════════════════════════════════════════
 SUCCESS CHECKLIST
═══════════════════════════════════════════════════════════════════════════════════

Complete this checklist to verify you're done:

☐ Phase 0 complete: settings.cpp, monaco_settings_dialog.cpp, MainWindowSimple.cpp
☐ Phase 1 complete: inference_engine, autonomous_feature_engine, universal_model_router
☐ Phase 2 complete: All test files deleted/retired
☐ Phase 3 complete: agentic/, agent/, qtapp/ audited
☐ Phase 4 complete: CMakeLists.txt updated for Win32 DLLs
☐ Grep verification: 0 files with #include <Q
☐ Build verification: RawrXD_IDE_Win32.exe created successfully
☐ Dependency check: 0 Qt5/Qt6 in dumpbin output
☐ Foundation test: All 32 components show READY
☐ IDE launch: <500ms startup, fully responsive


═══════════════════════════════════════════════════════════════════════════════════
 FILES THAT WILL HELP YOU
═══════════════════════════════════════════════════════════════════════════════════

Open these while working:

1. D:\RawrXD\QT_REMOVAL_REVERSE_ENGINEERING_REPORT.md
   └─ The complete analysis - use for detailed reference

2. D:\RawrXD\QT_REMOVAL_MIGRATION_LAUNCH.txt
   └─ Migration patterns and strategy overview

3. D:\RawrXD\src\agentic_core_win32.h
   └─ Copy/paste for component replacements

4. D:\RawrXD\src\file_operations_win32.h
   └─ Copy/paste for file operation replacements

5. D:\RawrXD\src\qtapp\main_qt_migrated.cpp
   └─ Reference for app initialization pattern

6. D:\RawrXD\src\build_win32_migration.bat
   └─ Run this after major milestones


═══════════════════════════════════════════════════════════════════════════════════
 WHEN YOU GET STUCK
═══════════════════════════════════════════════════════════════════════════════════

Problem: Can't find the Qt replacement pattern
Solution: Check QT_REMOVAL_MIGRATION_GUIDE.md - has before/after examples

Problem: Linker errors
Solution: Check that 32 DLLs are in D:\RawrXD\Ship\ and build_win32_migration.bat
          found them. Use dumpbin to verify they were linked.

Problem: Code doesn't compile after changes
Solution: Make sure you:
  1. Included the right header (file_operations_win32.h or agentic_core_win32.h)
  2. Used std:: prefix where needed (std::string, std::vector)
  3. Removed Q... prefixes (QString → std::string, etc.)

Problem: Something still uses Qt after you thought you removed it
Solution: Use: grep -r "QString\|QFile\|QObject" D:\RawrXD\src\[filename]
          This will find any lingering Qt references


═══════════════════════════════════════════════════════════════════════════════════
 ONE-SENTENCE SUMMARY
═══════════════════════════════════════════════════════════════════════════════════

Replace 280+ Qt-dependent source files to use Win32 DLLs instead through 14
systematic tasks following established patterns, with automated verification.


═══════════════════════════════════════════════════════════════════════════════════
 NEXT 5 MINUTES
═══════════════════════════════════════════════════════════════════════════════════

1. Read this card (done ✓)
2. Open D:\RawrXD\src\settings.cpp (Task 1)
3. Review QT_REMOVAL_MIGRATION_GUIDE.md patterns
4. Start replacing QSettings with file-based config
5. Build & verify with build_win32_migration.bat

That's it. One task. You've got this. 🚀

═══════════════════════════════════════════════════════════════════════════════════
