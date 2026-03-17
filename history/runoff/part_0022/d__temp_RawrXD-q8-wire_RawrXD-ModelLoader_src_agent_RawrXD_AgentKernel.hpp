/**
 * @file RawrXD_AgentKernel.hpp
 * @brief Pure Win32/C++20 Agent Kernel - Zero Qt Dependencies
 * @author RawrXD Sovereign Infrastructure Team
 * @version 2.0.0
 * 
 * This is the production agent kernel that powers autonomous coding.
 * NO Qt dependencies - pure Windows API, C++20 STL, IOCP, overlapped I/O.
 * 
 * Architecture:
 * - Platform abstraction layer (replaces Qt types)
 * - Task & Plan system (autonomous execution)
 * - Tool registry (extensible capabilities)
 * - Error healing (self-correction loop)
 * - MCP protocol (Model Context Protocol)
 * - Win32 UI bridge (message-passing integration)
 */

#pragma once

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#include <winhttp.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <psapi.h>
#include <tlhelp32.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <variant>
#include <vector>

// Link required libraries
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "ws2_32.lib")

namespace RawrXD::Platform {

// ============================================================================
// PLATFORM ABSTRACTIONS (Qt Replacements)
// ============================================================================

// Replacement for QMutex - SRW locks (lighter, faster)
class alignas(64) FastMutex {
    SRWLOCK lock_ = SRWLOCK_INIT;
public:
    void lock() noexcept { AcquireSRWLockExclusive(&lock_); }
    void unlock() noexcept { ReleaseSRWLockExclusive(&lock_); }
    bool try_lock() noexcept { return TryAcquireSRWLockExclusive(&lock_) != FALSE; }
};

// Replacement for QThreadPool
class ThreadPool {
public:
    explicit ThreadPool(size_t threads = std::thread::hardware_concurrency());
    ~ThreadPool();
    
    template<typename F>
    auto enqueue(F&& f) -> std::future<std::invoke_result_t<F>> {
        using ReturnType = std::invoke_result_t<F>;
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(std::forward<F>(f));
        std::future<ReturnType> result = task->get_future();
        
        {
            std::unique_lock lock(queue_mutex_);
            if (stop_) throw std::runtime_error("enqueue on stopped ThreadPool");
            tasks_.emplace([task]() { (*task)(); });
        }
        condition_.notify_one();
        return result;
    }
    
    void wait_for_all();
    
private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_{false};
};

// Replacement for QProcess - overlapped I/O
class Process {
public:
    struct Options {
        std::wstring working_directory;
        std::wstring environment_block;
        bool redirect_stdout = true;
        bool redirect_stderr = true;
        bool hide_window = true;
    };
    
    struct Output {
        int exit_code;
        std::string stdout_data;
        std::string stderr_data;
        bool killed_by_timeout;
        std::chrono::milliseconds duration;
    };
    
    Process();
    ~Process();
    
    bool start(const std::wstring& command, const Options& opts = {});
    Output run_sync(std::chrono::milliseconds timeout = std::chrono::milliseconds(300000));
    bool terminate();
    bool is_running() const noexcept;
    DWORD pid() const noexcept { return pid_; }
    
private:
    HANDLE hProcess_ = nullptr;
    HANDLE hThread_ = nullptr;
    HANDLE hStdOutRead_ = nullptr;
    HANDLE hStdOutWrite_ = nullptr;
    HANDLE hStdErrRead_ = nullptr;
    HANDLE hStdErrWrite_ = nullptr;
    std::atomic<bool> running_{false};
    DWORD pid_ = 0;
    
    void cleanup_handles() noexcept;
};

// UTF-8 / UTF-16 conversions
std::string utf8_from_wide(std::wstring_view wide);
std::wstring wide_from_utf8(std::string_view utf8);

} // namespace RawrXD::Platform

namespace RawrXD::Agent {

// ============================================================================
// CORE TYPES
// ============================================================================

uint64_t generate_snowflake_id();

// Type aliases to avoid Windows.h macro conflicts
using MillisecondsDuration = std::chrono::milliseconds;

enum class TaskType {
    Unknown = 0,
    FileRead,
    FileWrite,
    FileEdit,
    PowerShell,
    Cmd,
    Git,
    Build,
    Search,
    LLMQuery,
    ToolCall
};

enum class TaskStatus {
    Pending = 0,
    Running,
    Success,
    Failed,
    Cancelled,
    Blocked
};

struct TaskResult {
    bool success = false;
    std::string output;
    std::string error_message;
    int exit_code = 0;
    std::chrono::milliseconds duration{0};
    std::string stderr_output;
};

struct Task {
    uint64_t id = 0;
    uint64_t parent_id = 0;
    TaskType type = TaskType::Unknown;
    TaskStatus status = TaskStatus::Pending;
    
    std::string description;
    std::unordered_map<std::string, std::string> params;
    
    int max_retries = 3;
    int retry_count = 0;
    MillisecondsDuration timeout{300000}; // 5 minutes default
    std::vector<uint64_t> dependencies;
    
    TaskResult result;
    
    std::chrono::steady_clock::time_point created;
    std::chrono::steady_clock::time_point started;
    std::chrono::steady_clock::time_point completed;
    
    Task() : timeout(300000) { id = generate_snowflake_id(); created = std::chrono::steady_clock::now(); }
};

enum class PlanState {
    Draft = 0,
    Running,
    Completed,
    Failed,
    Cancelled
};

struct Plan {
    uint64_t id = 0;
    std::string objective;
    std::string original_prompt;
    
    PlanState state = PlanState::Draft;
    std::vector<Task> tasks;
    std::unordered_map<uint64_t, size_t> task_index;
    
    mutable std::atomic<size_t> current_task_idx{0};
    mutable std::atomic<size_t> completed_tasks{0};
    mutable std::atomic<size_t> failed_tasks{0};
    
    std::string working_directory;
    std::string final_summary;
    
    std::chrono::steady_clock::time_point created;
    std::chrono::steady_clock::time_point started;
    std::chrono::steady_clock::time_point completed;
    
    Plan() { id = generate_snowflake_id(); created = std::chrono::steady_clock::now(); }
    
    // Custom copy constructor to handle atomic members
    Plan(const Plan& other)
        : id(other.id),
          objective(other.objective),
          original_prompt(other.original_prompt),
          state(other.state),
          tasks(other.tasks),
          task_index(other.task_index),
          current_task_idx(other.current_task_idx.load()),
          completed_tasks(other.completed_tasks.load()),
          failed_tasks(other.failed_tasks.load()),
          working_directory(other.working_directory),
          final_summary(other.final_summary),
          created(other.created),
          started(other.started),
          completed(other.completed) {}
    
    // Custom move constructor
    Plan(Plan&& other) noexcept
        : id(other.id),
          objective(std::move(other.objective)),
          original_prompt(std::move(other.original_prompt)),
          state(other.state),
          tasks(std::move(other.tasks)),
          task_index(std::move(other.task_index)),
          current_task_idx(other.current_task_idx.load()),
          completed_tasks(other.completed_tasks.load()),
          failed_tasks(other.failed_tasks.load()),
          working_directory(std::move(other.working_directory)),
          final_summary(std::move(other.final_summary)),
          created(other.created),
          started(other.started),
          completed(other.completed) {}
    
    // Custom copy assignment
    Plan& operator=(const Plan& other) {
        if (this != &other) {
            id = other.id;
            objective = other.objective;
            original_prompt = other.original_prompt;
            state = other.state;
            tasks = other.tasks;
            task_index = other.task_index;
            current_task_idx.store(other.current_task_idx.load());
            completed_tasks.store(other.completed_tasks.load());
            failed_tasks.store(other.failed_tasks.load());
            working_directory = other.working_directory;
            final_summary = other.final_summary;
            created = other.created;
            started = other.started;
            completed = other.completed;
        }
        return *this;
    }
    
    // Custom move assignment
    Plan& operator=(Plan&& other) noexcept {
        if (this != &other) {
            id = other.id;
            objective = std::move(other.objective);
            original_prompt = std::move(other.original_prompt);
            state = other.state;
            tasks = std::move(other.tasks);
            task_index = std::move(other.task_index);
            current_task_idx.store(other.current_task_idx.load());
            completed_tasks.store(other.completed_tasks.load());
            failed_tasks.store(other.failed_tasks.load());
            working_directory = std::move(other.working_directory);
            final_summary = std::move(other.final_summary);
            created = other.created;
            started = other.started;
            completed = other.completed;
        }
        return *this;
    }
};

// ============================================================================
// TOOL SYSTEM
// ============================================================================

class Tool {
public:
    virtual ~Tool() = default;
    virtual std::string name() const = 0;
    virtual std::string description() const = 0;
    virtual std::vector<std::string> parameters() const = 0;
    virtual TaskResult execute(const std::unordered_map<std::string, std::string>& params,
                               std::function<void(std::string_view)> output_cb) = 0;
};

class ToolRegistry {
public:
    static ToolRegistry& instance();
    
    void register_tool(std::unique_ptr<Tool> tool);
    Tool* get_tool(std::string_view name);
    std::vector<std::string> list_tools() const;
    
    TaskResult execute_tool(std::string_view name, 
                           const std::unordered_map<std::string, std::string>& params,
                           std::function<void(std::string_view)> output_cb = nullptr);
    
private:
    ToolRegistry() = default;
    std::unordered_map<std::string, std::unique_ptr<Tool>> tools_;
    mutable Platform::FastMutex mutex_;
};

// Built-in tools
class PowerShellTool : public Tool {
public:
    std::string name() const override { return "powershell"; }
    std::string description() const override { 
        return "Execute PowerShell commands with full output capture"; 
    }
    std::vector<std::string> parameters() const override { return {"command", "working_dir"}; }
    TaskResult execute(const std::unordered_map<std::string, std::string>& params,
                      std::function<void(std::string_view)> output_cb) override;
};

class FileEditTool : public Tool {
public:
    std::string name() const override { return "file_edit"; }
    std::string description() const override {
        return "Apply multi-line edits to files with conflict detection";
    }
    std::vector<std::string> parameters() const override { 
        return {"path", "old_string", "new_string", "create_if_missing"}; 
    }
    TaskResult execute(const std::unordered_map<std::string, std::string>& params,
                      std::function<void(std::string_view)> output_cb) override;
};

class GitTool : public Tool {
public:
    std::string name() const override { return "git"; }
    std::string description() const override { return "Git operations (status, diff, commit, etc)"; }
    std::vector<std::string> parameters() const override { return {"subcommand", "args"}; }
    TaskResult execute(const std::unordered_map<std::string, std::string>& params,
                      std::function<void(std::string_view)> output_cb) override;
};

// ============================================================================
// AGENT KERNEL
// ============================================================================

class AgentKernel {
public:
    static AgentKernel& instance();

    struct Config {
        size_t max_concurrent_tasks = 4;
        MillisecondsDuration default_task_timeout;
        bool enable_self_healing = true;
        std::string model_endpoint = "http://localhost:11434/api/generate";
        std::string default_model = "codellama:13b";
        
        Config() : default_task_timeout(300000) {}
    };
    
    struct EventCallbacks {
        std::function<void(const Plan&)> on_plan_started;
        std::function<void(const Task&)> on_task_started;
        std::function<void(const Task&)> on_task_progress;
        std::function<void(const Task&)> on_task_completed;
        std::function<void(const Plan&)> on_plan_completed;
        std::function<void(const Plan&, const std::string&)> on_plan_failed;
    };
    
    AgentKernel();
    ~AgentKernel();
    
    bool initialize(const Config& config = {});
    void shutdown();
    bool is_running() const noexcept;
    
    std::future<Plan> execute_plan(Plan plan);
    bool cancel_plan(uint64_t plan_id);
    
    std::vector<Plan> get_active_plans() const;
    void set_event_callbacks(EventCallbacks callbacks);
    
    void RunAsyncTask(std::function<void()> task);

    ToolRegistry& tools() { return ToolRegistry::instance(); }
    
    struct Stats {
        uint64_t plans_executed = 0;
        uint64_t tasks_executed = 0;
        MillisecondsDuration avg_plan_duration;
        
        Stats() : avg_plan_duration(0) {}
    };
    Stats get_stats() const;
    
    // Self-healing functionality
    static std::string parse_error_context(const std::string& build_output);
    static std::string generate_fix_prompt(const std::string& error_context, const std::string& file_content);
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// UI BRIDGE (Win32 message passing - no Qt signals/slots)
// ============================================================================

namespace UI {

class AgentWindowBridge {
public:
    static constexpr UINT WM_AGENT_EVENT = WM_USER + 0x1000;
    
    enum EventType : WPARAM {
        PlanCreated = 1,
        TaskStarted,
        TaskProgress,
        TaskCompleted,
        PlanCompleted,
        PlanFailed
    };
    
    struct EventData {
        uint64_t plan_id;
        uint64_t task_id;
        char message[1024];
    };
    
    static bool initialize(HWND hwnd_target);
    static void shutdown();
    static void post_event(EventType type, const EventData& data);
    static LRESULT handle_message(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
private:
    static HWND hwnd_target_;
    static std::atomic<bool> initialized_;
};

} // namespace UI

} // namespace RawrXD::Agent
