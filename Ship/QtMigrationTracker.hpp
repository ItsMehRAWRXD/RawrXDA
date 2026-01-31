// QtMigrationTracker.hpp
// Central tracking system for all 280+ file migrations
// Provides unified status tracking, priority management, and progress reporting
// 
// Usage:
//   RawrXD::Migration::MigrationTracker::Instance().Initialize();
//   RawrXD::Migration::MigrationTracker::Instance().StartTask(L"src/qtapp/MainWindow.cpp");
//   RawrXD::Migration::MigrationTracker::Instance().PrintProgressReport();

#pragma once
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <atomic>
#include <algorithm>
#include <iostream>

namespace RawrXD {
namespace Migration {

enum class MigrationStatus {
    NotStarted,
    InProgress,
    CodeComplete,
    BuildVerified,
    TestPassed,
    Done
};

enum class Priority {
    Critical = 10,  // MainWindow, main_qt, core infrastructure
    High = 8,       // Agentic components, orchestration
    Medium = 5,     // Utils, helpers
    Low = 3         // Tests, stubs
};

struct MigrationTask {
    std::wstring sourceFile;
    std::wstring targetDll;           // Which Win32 DLL to use
    Priority priority;
    MigrationStatus status{MigrationStatus::NotStarted};
    std::chrono::hours estimatedEffort;
    std::chrono::hours actualEffort{0};
    std::wstring assignedTo;
    std::vector<std::wstring> qtIncludesToRemove;
    std::vector<std::wstring> win32Replacements;
    std::wstring notes;
    std::chrono::system_clock::time_point startedAt;
    std::chrono::system_clock::time_point completedAt;
    int linesOfCode{0};
    int qtReferences{0};
    
    std::wstring StatusString() const {
        switch (status) {
            case MigrationStatus::NotStarted: return L"⏳ NotStarted";
            case MigrationStatus::InProgress: return L"🔄 InProgress";
            case MigrationStatus::CodeComplete: return L"✓ CodeComplete";
            case MigrationStatus::BuildVerified: return L"✓✓ BuildVerified";
            case MigrationStatus::TestPassed: return L"✓✓✓ TestPassed";
            case MigrationStatus::Done: return L"✅ Done";
            default: return L"?";
        }
    }
    
    std::wstring PriorityString() const {
        switch (priority) {
            case Priority::Critical: return L"🔴 Critical";
            case Priority::High: return L"🟠 High";
            case Priority::Medium: return L"🟡 Medium";
            case Priority::Low: return L"🟢 Low";
            default: return L"?";
        }
    }
};

class MigrationTracker {
public:
    static MigrationTracker& Instance() {
        static MigrationTracker instance;
        return instance;
    }

    void Initialize() {
        // Priority 10 (CRITICAL) - UI Entry Points
        AddTask(L"src/qtapp/MainWindow.cpp", L"RawrXD_MainWindow_Win32.dll", 
                Priority::Critical, std::chrono::hours(4), {
                    L"#include <QMainWindow>", L"#include <QDockWidget>",
                    L"#include <QMenuBar>", L"#include <QToolBar>"
                }, {
                    L"CreateWindowEx", L"LoadMenu", L"WndProc"
                });
        
        AddTask(L"src/qtapp/main_qt.cpp", L"RawrXD_Foundation.dll",
                Priority::Critical, std::chrono::hours(2), {
                    L"#include <QApplication>", L"#include <QCoreApplication>"
                }, {
                    L"WinMain", L"Foundation::Initialize"
                });
        
        AddTask(L"src/qtapp/TerminalWidget.cpp", L"RawrXD_TerminalManager_Win32.dll",
                Priority::Critical, std::chrono::hours(3), {
                    L"#include <QProcess>", L"#include <QThread>"
                }, {
                    L"CreateProcessW", L"PIPE", L"CRITICAL_SECTION"
                });

        // Priority 9 (HIGH) - Agentic Core
        AddTask(L"src/agentic/agentic_executor.cpp", L"RawrXD_Executor.dll",
                Priority::High, std::chrono::hours(3), {
                    L"#include <QProcess>", L"#include <QTimer>"
                }, {
                    L"ToolExecutionEngine", L"ProcessResult"
                });
        
        AddTask(L"src/agentic/agentic_text_edit.cpp", L"RawrXD_TextEditor_Win32.dll",
                Priority::High, std::chrono::hours(2), {
                    L"#include <QPlainTextEdit>", L"#include <QTextCursor>"
                }, {
                    L"MSFTEDIT_CLASS", L"EM_SETTEXTEX"
                });
        
        // Priority 8 (HIGH) - Orchestration
        AddTask(L"src/orchestration/plan_orchestrator.cpp", L"RawrXD_PlanOrchestrator.dll",
                Priority::High, std::chrono::hours(4), {
                    L"#include <QObject>", L"#include <QThread>"
                }, {
                    L"AgentOrchestrator", L"TaskScheduler"
                });
        
        AddTask(L"src/agents/planning_agent.cpp", L"RawrXD_AgentCoordinator.dll",
                Priority::High, std::chrono::hours(3), {
                    L"#include <QThread>", L"#include <QMutex>"
                }, {
                    L"std::thread", L"std::mutex"
                });

        // Priority 6 (MEDIUM) - Utilities (batch process these)
        AddTask(L"src/utils/qt_directory_manager.cpp", L"RawrXD_FileManager_Win32.dll",
                Priority::Medium, std::chrono::hours(1), {
                    L"#include <QDir>", L"#include <QFileInfo>"
                }, {
                    L"FindFirstFileW", L"GetFileAttributesW"
                });
        
        AddTask(L"src/utils/qt_settings_wrapper.cpp", L"RawrXD_SettingsManager_Win32.dll",
                Priority::Medium, std::chrono::hours(1), {
                    L"#include <QSettings>"
                }, {
                    L"RegOpenKeyExW", L"RegQueryValueExW"
                });

        // Priority 3 (LOW) - Tests
        AddTask(L"src/test/test_qmainwindow.cpp", L"Test Harness",
                Priority::Low, std::chrono::hours(2), {
                    L"#include <QTest>", L"#include <QtTest/QTest>"
                }, {
                    L"RawrXD_TestRunner", L"RAWRXD_TEST macro"
                });
    }

    void StartTask(const std::wstring& file) {
        auto it = tasks_.find(file);
        if (it != tasks_.end()) {
            it->second.status = MigrationStatus::InProgress;
            it->second.startedAt = std::chrono::system_clock::now();
        }
    }

    void CompleteStep(const std::wstring& file, int step) {
        auto it = tasks_.find(file);
        if (it == tasks_.end()) return;
        
        auto& task = it->second;
        switch (step) {
            case 1: // Rewired to Win32
                break;
            case 2: // Qt removed
                break;
            case 3: // Build updated
                task.status = MigrationStatus::BuildVerified;
                break;
            case 4: // Tests pass - DONE
                task.status = MigrationStatus::TestPassed;
                task.completedAt = std::chrono::system_clock::now();
                task.actualEffort = std::chrono::duration_cast<std::chrono::hours>(
                    task.completedAt - task.startedAt);
                completedTasks_++;
                break;
        }
    }

    void PrintProgressReport() const {
        std::wcout << L"\n╔════════════════════════════════════════════════════════════════╗\n";
        std::wcout << L"║              QT MIGRATION PROGRESS REPORT                      ║\n";
        std::wcout << L"╚════════════════════════════════════════════════════════════════╝\n";
        
        size_t total = tasks_.size();
        size_t done = completedTasks_.load();
        size_t inProgress = 0;
        
        for (const auto& [file, task] : tasks_) {
            if (task.status == MigrationStatus::InProgress) {
                inProgress++;
            }
        }
        
        int percent = total > 0 ? (done * 100 / total) : 0;
        
        std::wcout << L"\nProgress: " << done << L"/" << total << L" (" << percent << L"%)\n";
        std::wcout << L"[";
        for (int i = 0; i < 50; ++i) {
            if (i < percent / 2) std::wcout << L"█";
            else std::wcout << L"░";
        }
        std::wcout << L"]\n\n";

        std::wcout << L"By Priority:\n";
        for (int p = 10; p >= 3; p -= 2) {
            size_t count = 0;
            size_t doneAtPriority = 0;
            
            for (const auto& [file, task] : tasks_) {
                if (static_cast<int>(task.priority) == p) {
                    count++;
                    if (task.status == MigrationStatus::TestPassed || 
                        task.status == MigrationStatus::Done) {
                        doneAtPriority++;
                    }
                }
            }
            
            if (count > 0) {
                const wchar_t* label = (p == 10) ? L"🔴 Critical" : 
                                      (p == 8) ? L"🟠 High" : 
                                      (p == 5) ? L"🟡 Medium" : L"🟢 Low";
                std::wcout << L"  " << label << L": " << doneAtPriority << L"/" << count << L"\n";
            }
        }

        std::wcout << L"\nCurrent Focus (In Progress):\n";
        if (inProgress == 0) {
            std::wcout << L"  (none)\n";
        } else {
            for (const auto& [file, task] : tasks_) {
                if (task.status == MigrationStatus::InProgress) {
                    std::wcout << L"  🔄 " << file << L"\n";
                }
            }
        }

        if (done == total && total > 0) {
            std::wcout << L"\n✅ ALL MIGRATIONS COMPLETE! Qt fully eliminated.\n";
        }
        
        std::wcout << L"\n";
    }

    std::vector<std::wstring> GetReadyTasks() const {
        std::vector<std::wstring> ready;
        for (const auto& [file, task] : tasks_) {
            if (task.status == MigrationStatus::NotStarted) {
                ready.push_back(file);
            }
        }
        // Sort by priority (highest first)
        std::sort(ready.begin(), ready.end(), [this](const auto& a, const auto& b) {
            return static_cast<int>(tasks_.at(a).priority) > 
                   static_cast<int>(tasks_.at(b).priority);
        });
        return ready;
    }

    const MigrationTask* GetTask(const std::wstring& file) const {
        auto it = tasks_.find(file);
        if (it != tasks_.end()) {
            return &it->second;
        }
        return nullptr;
    }

    size_t GetTotalTasks() const { return tasks_.size(); }
    size_t GetCompletedTasks() const { return completedTasks_.load(); }
    int GetProgressPercent() const {
        if (tasks_.empty()) return 0;
        return (completedTasks_.load() * 100) / tasks_.size();
    }

private:
    void AddTask(const std::wstring& file, const std::wstring& dll,
                 Priority prio, std::chrono::hours effort,
                 std::vector<std::wstring> qtIncludes,
                 std::vector<std::wstring> win32Reps) {
        MigrationTask task;
        task.sourceFile = file;
        task.targetDll = dll;
        task.priority = prio;
        task.estimatedEffort = effort;
        task.qtIncludesToRemove = qtIncludes;
        task.win32Replacements = win32Reps;
        tasks_[file] = task;
    }

    std::map<std::wstring, MigrationTask> tasks_;
    std::atomic<size_t> completedTasks_{0};
};

} // namespace Migration
} // namespace RawrXD
