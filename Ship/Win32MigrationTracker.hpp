// Win32MigrationTracker.hpp — migration status (C++20, no Qt references)
// Central tracking for Win32/native migrations. Legacy file paths only.
//
// Usage:
//   RawrXD::Migration::MigrationTracker::Instance().Initialize();
//   RawrXD::Migration::MigrationTracker::Instance().StartTask(L"src/win32app/MainWindow.cpp");
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
    Critical = 10,  // MainWindow, main entry, core infrastructure
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
    std::vector<std::wstring> legacyIncludesToRemove;
    std::vector<std::wstring> win32Replacements;
    std::wstring notes;
    std::chrono::system_clock::time_point startedAt;
    std::chrono::system_clock::time_point completedAt;
    int linesOfCode{0};
    int legacyReferences{0};

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
        AddTask(L"src/win32app/MainWindow.cpp", L"RawrXD_MainWindow_Win32.dll",
                Priority::Critical, std::chrono::hours(4), { L"(legacy)" }, { L"CreateWindowEx", L"LoadMenu", L"WndProc" });
        AddTask(L"src/main.cpp", L"RawrXD_Foundation.dll",
                Priority::Critical, std::chrono::hours(2), { L"(legacy)" }, { L"WinMain", L"Foundation::Initialize" });
        AddTask(L"src/win32app/TerminalManager.cpp", L"RawrXD_TerminalManager_Win32.dll",
                Priority::Critical, std::chrono::hours(3), { L"(legacy)" }, { L"CreateProcessW", L"PIPE", L"CRITICAL_SECTION" });
        AddTask(L"src/agentic/agentic_executor.cpp", L"RawrXD_Executor.dll",
                Priority::High, std::chrono::hours(3), { L"(legacy)" }, { L"ToolExecutionEngine", L"ProcessResult" });
        AddTask(L"src/win32app/TextEditor.cpp", L"RawrXD_TextEditor_Win32.dll",
                Priority::High, std::chrono::hours(2), { L"(legacy)" }, { L"MSFTEDIT_CLASS", L"EM_SETTEXTEX" });
        AddTask(L"src/orchestration/plan_orchestrator.cpp", L"RawrXD_PlanOrchestrator.dll",
                Priority::High, std::chrono::hours(4), { L"(legacy)" }, { L"AgentOrchestrator", L"TaskScheduler" });
        AddTask(L"src/agents/planning_agent.cpp", L"RawrXD_AgentCoordinator.dll",
                Priority::High, std::chrono::hours(3), { L"(legacy)" }, { L"std::thread", L"std::mutex" });
        AddTask(L"src/win32app/FileManager.cpp", L"RawrXD_FileManager_Win32.dll",
                Priority::Medium, std::chrono::hours(1), { L"(legacy)" }, { L"FindFirstFileW", L"GetFileAttributesW" });
        AddTask(L"src/win32app/SettingsManager.cpp", L"RawrXD_SettingsManager_Win32.dll",
                Priority::Medium, std::chrono::hours(1), { L"(legacy)" }, { L"RegOpenKeyExW", L"RegQueryValueExW" });
        AddTask(L"tests/test_runner.cpp", L"Test Harness",
                Priority::Low, std::chrono::hours(2), { L"(legacy)" }, { L"RawrXD_TestRunner", L"RAWRXD_TEST macro" });
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
            case 1: case 2: break;
            case 3: task.status = MigrationStatus::BuildVerified; break;
            case 4:
                task.status = MigrationStatus::TestPassed;
                task.completedAt = std::chrono::system_clock::now();
                task.actualEffort = std::chrono::duration_cast<std::chrono::hours>(task.completedAt - task.startedAt);
                completedTasks_++;
                break;
        }
    }

    void PrintProgressReport() const {
        std::wcout << L"\n╔════════════════════════════════════════════════════════════════╗\n";
        std::wcout << L"║              WIN32 MIGRATION PROGRESS REPORT                   ║\n";
        std::wcout << L"╚════════════════════════════════════════════════════════════════╝\n";
        size_t total = tasks_.size();
        size_t done = completedTasks_.load();
        int percent = total > 0 ? (int)(done * 100 / total) : 0;
        std::wcout << L"\nProgress: " << done << L"/" << total << L" (" << percent << L"%)\n";
        std::wcout << L"[";
        for (int i = 0; i < 50; ++i) std::wcout << (i < percent / 2 ? L"█" : L"░");
        std::wcout << L"]\n\n";
        if (done == total && total > 0) std::wcout << L"\n✅ ALL MIGRATIONS COMPLETE.\n";
        std::wcout << L"\n";
    }

    std::vector<std::wstring> GetReadyTasks() const {
        std::vector<std::wstring> ready;
        for (const auto& [file, task] : tasks_)
            if (task.status == MigrationStatus::NotStarted) ready.push_back(file);
        std::sort(ready.begin(), ready.end(), [this](const auto& a, const auto& b) {
            return static_cast<int>(tasks_.at(a).priority) > static_cast<int>(tasks_.at(b).priority);
        });
        return ready;
    }

    const MigrationTask* GetTask(const std::wstring& file) const {
        auto it = tasks_.find(file);
        return it != tasks_.end() ? &it->second : nullptr;
    }

    size_t GetTotalTasks() const { return tasks_.size(); }
    size_t GetCompletedTasks() const { return completedTasks_.load(); }
    int GetProgressPercent() const {
        return tasks_.empty() ? 0 : (int)((completedTasks_.load() * 100) / tasks_.size());
    }

private:
    void AddTask(const std::wstring& file, const std::wstring& dll,
                 Priority prio, std::chrono::hours effort,
                 std::vector<std::wstring> legacyIncludes,
                 std::vector<std::wstring> win32Reps) {
        MigrationTask task;
        task.sourceFile = file;
        task.targetDll = dll;
        task.priority = prio;
        task.estimatedEffort = effort;
        task.legacyIncludesToRemove = std::move(legacyIncludes);
        task.win32Replacements = std::move(win32Reps);
        tasks_[file] = task;
    }

    std::map<std::wstring, MigrationTask> tasks_;
    std::atomic<size_t> completedTasks_{0};
};

} // namespace Migration
} // namespace RawrXD
