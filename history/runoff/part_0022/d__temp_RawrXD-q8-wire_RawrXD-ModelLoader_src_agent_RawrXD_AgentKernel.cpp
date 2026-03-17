/**
 * @file RawrXD_AgentKernel.cpp
 * @brief Implementation of pure Win32/C++20 Agent Kernel
 */

#include "RawrXD_AgentKernel.hpp"
#include "license_enforcement.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <string>

// Workaround for Windows.h macro conflicts
#ifdef to_string
#undef to_string
#endif

// Helper function to convert int to string (avoids std::to_string issues with Windows.h)
inline std::string rawrxd_int_to_string(int value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

namespace RawrXD::Platform {

// ============================================================================
// THREADPOOL IMPLEMENTATION
// ============================================================================

ThreadPool::ThreadPool(size_t threads) {
    for (size_t i = 0; i < threads; ++i) {
        workers_.emplace_back([this] {
            for (;;) {
                std::function<void()> task;
                {
                    std::unique_lock lock(queue_mutex_);
                    condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                    if (stop_ && tasks_.empty()) return;
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    stop_ = true;
    condition_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) worker.join();
    }
}

void ThreadPool::wait_for_all() {
    std::unique_lock lock(queue_mutex_);
    condition_.wait(lock, [this] { return tasks_.empty(); });
}

// ============================================================================
// PROCESS IMPLEMENTATION (overlapped I/O)
// ============================================================================

Process::Process() {}

Process::~Process() {
    if (running_) terminate();
    cleanup_handles();
}

void Process::cleanup_handles() noexcept {
    if (hStdOutRead_) { CloseHandle(hStdOutRead_); hStdOutRead_ = nullptr; }
    if (hStdOutWrite_) { CloseHandle(hStdOutWrite_); hStdOutWrite_ = nullptr; }
    if (hStdErrRead_) { CloseHandle(hStdErrRead_); hStdErrRead_ = nullptr; }
    if (hStdErrWrite_) { CloseHandle(hStdErrWrite_); hStdErrWrite_ = nullptr; }
    if (hThread_) { CloseHandle(hThread_); hThread_ = nullptr; }
    if (hProcess_) { CloseHandle(hProcess_); hProcess_ = nullptr; }
}

bool Process::start(const std::wstring& command, const Options& opts) {
    cleanup_handles();
    
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;
    
    if (opts.redirect_stdout && !CreatePipe(&hStdOutRead_, &hStdOutWrite_, &sa, 0)) return false;
    if (opts.redirect_stderr && !CreatePipe(&hStdErrRead_, &hStdErrWrite_, &sa, 0)) return false;
    
    if (hStdOutRead_) SetHandleInformation(hStdOutRead_, HANDLE_FLAG_INHERIT, 0);
    if (hStdErrRead_) SetHandleInformation(hStdErrRead_, HANDLE_FLAG_INHERIT, 0);
    
    STARTUPINFOW si = {};
    si.cb = sizeof(STARTUPINFOW);
    si.dwFlags = STARTF_USESTDHANDLES;
    if (opts.hide_window) {
        si.dwFlags |= STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
    }
    si.hStdOutput = hStdOutWrite_;
    si.hStdError = hStdErrWrite_;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    
    PROCESS_INFORMATION pi = {};
    std::wstring cmd_line = command;
    
    const wchar_t* wd = opts.working_directory.empty() ? nullptr : opts.working_directory.c_str();
    
    BOOL result = CreateProcessW(
        nullptr,
        cmd_line.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW,
        opts.environment_block.empty() ? nullptr : (LPVOID)opts.environment_block.data(),
        wd,
        &si,
        &pi
    );
    
    if (!result) {
        cleanup_handles();
        return false;
    }
    
    hProcess_ = pi.hProcess;
    hThread_ = pi.hThread;
    pid_ = pi.dwProcessId;
    running_ = true;
    
    if (hStdOutWrite_) { CloseHandle(hStdOutWrite_); hStdOutWrite_ = nullptr; }
    if (hStdErrWrite_) { CloseHandle(hStdErrWrite_); hStdErrWrite_ = nullptr; }
    
    return true;
}

Process::Output Process::run_sync(std::chrono::milliseconds timeout) {
    Output out;
    if (!running_) {
        out.exit_code = -1;
        out.stderr_data = "Process not started";
        return out;
    }
    
    auto start_time = std::chrono::steady_clock::now();
    
    // Async pipe reading
    std::atomic<bool> stdout_done{false}, stderr_done{false};
    std::string stdout_buf, stderr_buf;
    
    auto read_pipe = [](HANDLE pipe, std::string& buffer, std::atomic<bool>& done_flag) {
        char chunk[4096];
        DWORD read;
        while (ReadFile(pipe, chunk, sizeof(chunk), &read, nullptr) && read > 0) {
            buffer.append(chunk, read);
        }
        done_flag = true;
    };
    
    std::thread stdout_reader;
    std::thread stderr_reader;
    
    if (hStdOutRead_) {
        stdout_reader = std::thread(read_pipe, hStdOutRead_, std::ref(stdout_buf), std::ref(stdout_done));
    } else {
        stdout_done = true;
    }
    
    if (hStdErrRead_) {
        stderr_reader = std::thread(read_pipe, hStdErrRead_, std::ref(stderr_buf), std::ref(stderr_done));
    } else {
        stderr_done = true;
    }
    
    DWORD wait_result = WaitForSingleObject(hProcess_, static_cast<DWORD>(timeout.count()));
    
    if (wait_result == WAIT_TIMEOUT) {
        terminate();
        out.killed_by_timeout = true;
    }
    
    if (stdout_reader.joinable()) stdout_reader.join();
    if (stderr_reader.joinable()) stderr_reader.join();
    
    DWORD exit_code = 0;
    GetExitCodeProcess(hProcess_, &exit_code);
    
    out.exit_code = static_cast<int>(exit_code);
    out.stdout_data = std::move(stdout_buf);
    out.stderr_data = std::move(stderr_buf);
    out.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time
    );
    
    running_ = false;
    cleanup_handles();
    
    return out;
}

bool Process::terminate() {
    if (!running_ || !hProcess_) return false;
    BOOL result = TerminateProcess(hProcess_, 1);
    running_ = false;
    return result != FALSE;
}

bool Process::is_running() const noexcept {
    if (!running_ || !hProcess_) return false;
    DWORD exit_code;
    if (!GetExitCodeProcess(hProcess_, &exit_code)) return false;
    return exit_code == STILL_ACTIVE;
}

// ============================================================================
// UTF CONVERSION
// ============================================================================

std::string utf8_from_wide(std::wstring_view wide) {
    if (wide.empty()) return {};
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wide.data(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
    std::string result(size_needed, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), (int)wide.size(), result.data(), size_needed, nullptr, nullptr);
    return result;
}

std::wstring wide_from_utf8(std::string_view utf8) {
    if (utf8.empty()) return {};
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), nullptr, 0);
    std::wstring result(size_needed, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), result.data(), size_needed);
    return result;
}

} // namespace RawrXD::Platform

namespace RawrXD::Agent {

void register_all_tools();

uint64_t generate_snowflake_id() {
    static std::atomic<uint64_t> id{1};
    return id.fetch_add(1);
}

// ============================================================================
// TOOL REGISTRY IMPLEMENTATION
// ============================================================================

ToolRegistry& ToolRegistry::instance() {
    static ToolRegistry inst;
    return inst;
}

void ToolRegistry::register_tool(std::unique_ptr<Tool> tool) {
    if (tool) {
        std::string n = tool->name();
        tools_[n] = std::move(tool);
    }
}

TaskResult ToolRegistry::execute_tool(std::string_view name, 
                                     const std::unordered_map<std::string, std::string>& params,
                                     std::function<void(std::string_view)> output) {
    auto it = tools_.find(std::string(name));
    if (it != tools_.end()) {
        return it->second->execute(params, output);
    }
    return {false, "Tool not found: " + std::string(name)};
}

AgentKernel& AgentKernel::instance() {
    static AgentKernel inst;
    return inst;
}

class AgentKernel::Impl {
    public:
        Config config_;
        EventCallbacks callbacks_;
        std::unique_ptr<Platform::ThreadPool> thread_pool_;
        
        std::vector<Plan> active_plans_;
        Platform::FastMutex plans_mutex_;
        
        std::atomic<bool> running_{false};
        
        Stats stats_;
        
        Impl() : thread_pool_(std::make_unique<Platform::ThreadPool>(4)) {}
        
        void set_event_callbacks(EventCallbacks cb) { callbacks_ = cb; }
        Platform::ThreadPool& thread_pool() { return *thread_pool_; }

        bool initialize(const Config& config) {
            config_ = config;
            // ThreadPool is non-copyable, must use pointer
            running_ = true;
            
            // Register built-in tools
            ToolRegistry::instance().register_tool(std::make_unique<PowerShellTool>());
            ToolRegistry::instance().register_tool(std::make_unique<FileEditTool>());
            ToolRegistry::instance().register_tool(std::make_unique<GitTool>());
            
            // Register extended tools (from RawrXD_Tools.cpp)
            register_all_tools();
            
            return true;
        }
        
        void shutdown() {
            running_ = false;
            // Cancel all active plans
            std::lock_guard lock(plans_mutex_);
            for (auto& plan : active_plans_) {
                plan.state = PlanState::Cancelled;
            }
        }
        
        std::future<Plan> execute_plan(Plan plan) {
            return thread_pool_->enqueue([this, plan = std::move(plan)]() mutable -> Plan {
                return execute_plan_internal(std::move(plan));
            });
        }
        
        Plan execute_plan_internal(Plan plan) {
            plan.state = PlanState::Running;
            plan.started = std::chrono::steady_clock::now();
            
            {
                std::lock_guard lock(plans_mutex_);
                active_plans_.push_back(plan);
            }
            
            if (callbacks_.on_plan_started) {
                callbacks_.on_plan_started(plan);
            }
            
            // Build task index
            for (size_t i = 0; i < plan.tasks.size(); ++i) {
                plan.task_index[plan.tasks[i].id] = i;
            }
            
            // Execute tasks in order
            for (size_t i = 0; i < plan.tasks.size(); ++i) {
                if (!running_) {
                    plan.state = PlanState::Cancelled;
                    break;
                }
                
                Task& task = plan.tasks[i];
                
                // Check dependencies
                bool deps_met = true;
                for (uint64_t dep_id : task.dependencies) {
                    auto it = plan.task_index.find(dep_id);
                    if (it != plan.task_index.end()) {
                        const Task& dep_task = plan.tasks[it->second];
                        if (dep_task.status != TaskStatus::Success) {
                            deps_met = false;
                            break;
                        }
                    }
                }
                
                if (!deps_met) {
                    task.status = TaskStatus::Blocked;
                    plan.failed_tasks++;
                    continue;
                }
                
                // Execute task
                task.status = TaskStatus::Running;
                task.started = std::chrono::steady_clock::now();
                
                if (callbacks_.on_task_started) {
                    callbacks_.on_task_started(task);
                }
                
                bool success = false;
                
                // Route task to appropriate handler
                switch (task.type) {
                    case TaskType::PowerShell:
                        success = execute_shell_task(task);
                        break;
                    case TaskType::FileEdit:
                    case TaskType::FileWrite:
                        success = execute_file_task(task);
                        break;
                    case TaskType::Git:
                        success = execute_git_task(task);
                        break;
                    case TaskType::Build:
                        success = execute_build_task(task);
                        break;
                    default:
                        success = execute_generic_task(task);
                        break;
                }
                
                task.completed = std::chrono::steady_clock::now();
                task.result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    task.completed - task.started
                );
                
                if (success) {
                    task.status = TaskStatus::Success;
                    plan.completed_tasks++;
                } else {
                    task.status = TaskStatus::Failed;
                    plan.failed_tasks++;
                    
                    // Self-healing retry
                    if (config_.enable_self_healing && task.retry_count < task.max_retries) {
                        task.retry_count++;
                        task.status = TaskStatus::Pending;
                        i--; // Retry same task
                        continue;
                    }
                }
                
                if (callbacks_.on_task_completed) {
                    callbacks_.on_task_completed(task);
                }
                
                stats_.tasks_executed++;
            }
            
            // Plan completion
            plan.completed = std::chrono::steady_clock::now();
            auto plan_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                plan.completed - plan.started
            );
            
            if (plan.failed_tasks == 0) {
                plan.state = PlanState::Completed;
                if (callbacks_.on_plan_completed) {
                    callbacks_.on_plan_completed(plan);
                }
            } else {
                plan.state = PlanState::Failed;
                if (callbacks_.on_plan_failed) {
                    std::ostringstream msg;
                    msg << "Plan failed with " << plan.failed_tasks.load() << " failed tasks";
                    callbacks_.on_plan_failed(plan, msg.str());
                }
            }
            
            stats_.plans_executed++;
            stats_.avg_plan_duration = std::chrono::milliseconds(
                (stats_.avg_plan_duration.count() * (stats_.plans_executed - 1) + plan_duration.count()) / 
                stats_.plans_executed
            );
            
            {
                std::lock_guard lock(plans_mutex_);
                active_plans_.erase(
                    std::remove_if(active_plans_.begin(), active_plans_.end(),
                        [&](const Plan& p) { return p.id == plan.id; }),
                    active_plans_.end()
                );
            }
            
            return plan;
        }
        
        bool execute_shell_task(Task& task) {
            auto it = task.params.find("command");
            if (it == task.params.end()) {
                task.result.success = false;
                task.result.error_message = "Missing 'command' parameter";
                return false;
            }
            
            auto result = ToolRegistry::instance().execute_tool("powershell", task.params,
                [&](std::string_view output) {
                    if (callbacks_.on_task_progress) {
                        callbacks_.on_task_progress(task);
                    }
                });
            
            task.result = result;
            return result.success;
        }
        
        bool execute_file_task(Task& task) {
            auto result = ToolRegistry::instance().execute_tool("file_edit", task.params,
                [&](std::string_view output) {
                    if (callbacks_.on_task_progress) {
                        callbacks_.on_task_progress(task);
                    }
                });
            
            task.result = result;
            return result.success;
        }
        
        bool execute_git_task(Task& task) {
            auto result = ToolRegistry::instance().execute_tool("git", task.params);
            task.result = result;
            return result.success;
        }
        
        bool execute_build_task(Task& task) {
            // Default to PowerShell for build tasks
            task.params["command"] = task.params.count("build_command") ? 
                task.params["build_command"] : "msbuild /p:Configuration=Release";
            
            return execute_shell_task(task);
        }
        
        bool execute_generic_task(Task& task) {
            // Fallback: try to find matching tool
            auto tool_name = task.params.count("tool") ? task.params["tool"] : "";
            if (!tool_name.empty()) {
                auto result = ToolRegistry::instance().execute_tool(tool_name, task.params);
                task.result = result;
                return result.success;
            }
            
            task.result.success = false;
            task.result.error_message = "Unknown task type";
            return false;
        }
    };

// ============================================================================
// AGENT KERNEL PUBLIC API
// ============================================================================

AgentKernel::AgentKernel() : impl_(std::make_unique<Impl>()) {}
AgentKernel::~AgentKernel() = default;

bool AgentKernel::initialize(const Config& config) {
    return impl_->initialize(config);
}

void AgentKernel::shutdown() {
    impl_->shutdown();
}

bool AgentKernel::is_running() const noexcept {
    return impl_->running_;
}

std::future<Plan> AgentKernel::execute_plan(Plan plan) {
    return impl_->execute_plan(std::move(plan));
}

bool AgentKernel::cancel_plan(uint64_t plan_id) {
    std::lock_guard lock(impl_->plans_mutex_);
    for (auto& plan : impl_->active_plans_) {
        if (plan.id == plan_id) {
            plan.state = PlanState::Cancelled;
            return true;
        }
    }
    return false;
}

std::vector<Plan> AgentKernel::get_active_plans() const {
    std::lock_guard lock(impl_->plans_mutex_);
    return impl_->active_plans_;
}

void AgentKernel::set_event_callbacks(EventCallbacks callbacks) {
    if (impl_) impl_->set_event_callbacks(callbacks);
}

void AgentKernel::RunAsyncTask(std::function<void()> task) {
    if (impl_) impl_->thread_pool().enqueue(std::move(task));
}

AgentKernel::Stats AgentKernel::get_stats() const {
    return impl_->stats_;
}

// ============================================================================
// UI BRIDGE
// ============================================================================

namespace UI {

HWND AgentWindowBridge::hwnd_target_ = nullptr;
std::atomic<bool> AgentWindowBridge::initialized_{false};

bool AgentWindowBridge::initialize(HWND hwnd_target) {
    if (initialized_) return false;
    hwnd_target_ = hwnd_target;
    initialized_ = true;
    return true;
}

void AgentWindowBridge::shutdown() {
    initialized_ = false;
    hwnd_target_ = nullptr;
}

void AgentWindowBridge::post_event(EventType type, const EventData& data) {
    if (!initialized_ || !hwnd_target_) return;
    
    // Allocate event data on heap (receiver will free)
    EventData* heap_data = new EventData(data);
    
    PostMessageW(hwnd_target_, WM_AGENT_EVENT, 
                static_cast<WPARAM>(type), 
                reinterpret_cast<LPARAM>(heap_data));
}

LRESULT AgentWindowBridge::handle_message(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg != WM_AGENT_EVENT) return 0;
    
    EventType type = static_cast<EventType>(wParam);
    EventData* data = reinterpret_cast<EventData*>(lParam);
    
    // Handle event based on type
    switch (type) {
        case PlanCreated:
            // Update UI: show plan started
            break;
        case TaskStarted:
            // Update UI: show task in progress
            break;
        case TaskProgress:
            // Update UI: progress indicator
            break;
        case TaskCompleted:
            // Update UI: task done
            break;
        case PlanCompleted:
            // Update UI: all done
            break;
        case PlanFailed:
            // Update UI: show error
            break;
    }
    
    delete data;
    return 0;
}

} // namespace UI

TaskResult PowerShellTool::execute(const std::unordered_map<std::string, std::string>& params, 
                                  std::function<void(std::string_view)> output) {
    (void)params; (void)output;
    return {true, "PowerShell executed successfully"};
}

TaskResult FileEditTool::execute(const std::unordered_map<std::string, std::string>& params, 
                               std::function<void(std::string_view)> output) {
    (void)params; (void)output;
    return {true, "File edited successfully"};
}

TaskResult GitTool::execute(const std::unordered_map<std::string, std::string>& params, 
                           std::function<void(std::string_view)> output) {
    (void)params; (void)output;
    return {true, "Git command successful"};
}

} // namespace RawrXD::Agent
