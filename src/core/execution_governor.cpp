// ============================================================================
// execution_governor.cpp — Phase 10A: Execution Governor + Terminal Watchdog
// ============================================================================
//
// Full implementation of the non-blocking execution scheduler with:
//   - TerminalWatchdog: CreateProcess + PeekNamedPipe non-blocking wrapper
//   - ExecutionGovernor: singleton task scheduler with watchdog thread
//
// Build:  Compiled as part of RawrXD-Win32IDE target
// Rule:   NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "execution_governor.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>

// ============================================================================
// TERMINAL WATCHDOG — Non-blocking process execution
// ============================================================================

GovernorCommandResult TerminalWatchdog::ExecuteSafe(
    const std::string& command,
    uint64_t maxDurationMs,
    std::atomic<GovernorTaskState>* statePtr)
{
    GovernorCommandResult result = {};
    result.exitCode = -1;
    result.timedOut = false;
    result.cancelled = false;
    result.bytesRead = 0;

    auto startClock = std::chrono::steady_clock::now();

    // ── Create anonymous pipe for stdout/stderr ─────────────────────────
    HANDLE hReadPipe = nullptr;
    HANDLE hWritePipe = nullptr;
    SECURITY_ATTRIBUTES saAttr = {};
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = nullptr;

    if (!CreatePipe(&hReadPipe, &hWritePipe, &saAttr, 0)) {
        result.statusDetail = "CreatePipe failed: " + std::to_string(GetLastError());
        return result;
    }

    // Ensure read handle is NOT inherited (prevents deadlocks)
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    // ── Launch process ──────────────────────────────────────────────────
    STARTUPINFOA si = {};
    si.cb = sizeof(STARTUPINFOA);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWritePipe;
    si.hStdError  = hWritePipe;  // Merge stderr into stdout
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    std::string cmdLine = "cmd.exe /C " + command;
    std::vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back('\0');

    BOOL created = CreateProcessA(
        nullptr,
        cmdBuf.data(),
        nullptr,
        nullptr,
        TRUE,           // Inherit handles
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi);

    if (!created) {
        result.statusDetail = "CreateProcess failed: " + std::to_string(GetLastError());
        CloseHandle(hWritePipe);
        CloseHandle(hReadPipe);
        return result;
    }

    // Close write end in parent — otherwise ReadFile never sees EOF
    CloseHandle(hWritePipe);
    hWritePipe = nullptr;

    // Update state if tracking pointer provided
    if (statePtr) {
        statePtr->store(GovernorTaskState::Running);
    }

    // ── The "Agentic" Poll Loop ─────────────────────────────────────────
    // PeekNamedPipe prevents blocking reads.
    // WaitForSingleObject(10ms) prevents spinning.
    // GetTickCount64 for timeout checking.

    DWORD startTick = GetTickCount();
    bool processEnded = false;
    char buffer[4096];
    DWORD bytesAvail = 0;
    DWORD bytesReadChunk = 0;

    while (true) {
        // A. Check for external cancellation
        if (statePtr && statePtr->load() == GovernorTaskState::Cancelled) {
            result.cancelled = true;
            TerminateProcess(pi.hProcess, 0xDEAD0001);
            result.output += "\n[RawrXD: Command cancelled by governor]";
            break;
        }

        // B. Check if time is up
        DWORD elapsed = GetTickCount() - startTick;
        if (elapsed > (DWORD)maxDurationMs) {
            result.timedOut = true;
            TerminateProcess(pi.hProcess, 0xDEAD);
            result.output += "\n[RawrXD: Process terminated — timeout after " +
                             std::to_string(maxDurationMs) + "ms]";
            break;
        }

        // C. Check process status without blocking (10ms granularity)
        if (!processEnded) {
            DWORD waitResult = WaitForSingleObject(pi.hProcess, 10);
            if (waitResult == WAIT_OBJECT_0) {
                processEnded = true;
            }
        }

        // D. Non-blocking pipe read via PeekNamedPipe
        //    PeekNamedPipe tells us if data is available WITHOUT blocking.
        //    Only call ReadFile if Peek confirms bytes are available.
        bytesAvail = 0;
        if (PeekNamedPipe(hReadPipe, nullptr, 0, nullptr, &bytesAvail, nullptr) &&
            bytesAvail > 0) {
            bytesReadChunk = 0;
            if (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesReadChunk, nullptr) &&
                bytesReadChunk > 0) {
                buffer[bytesReadChunk] = '\0';
                result.output += buffer;
                result.bytesRead += bytesReadChunk;
            }
        } else if (processEnded) {
            // Process is dead and pipe is drained — we're done
            // Do one final drain pass
            while (PeekNamedPipe(hReadPipe, nullptr, 0, nullptr, &bytesAvail, nullptr) &&
                   bytesAvail > 0) {
                bytesReadChunk = 0;
                if (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesReadChunk, nullptr) &&
                    bytesReadChunk > 0) {
                    buffer[bytesReadChunk] = '\0';
                    result.output += buffer;
                    result.bytesRead += bytesReadChunk;
                } else {
                    break;
                }
            }
            break;
        }
    }

    // ── Cleanup ─────────────────────────────────────────────────────────
    if (!result.timedOut && !result.cancelled) {
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        result.exitCode = (int)exitCode;
        result.statusDetail = "Completed (exit " + std::to_string(exitCode) + ")";
    } else if (result.timedOut) {
        result.exitCode = -1;
        result.statusDetail = "Timed out after " + std::to_string(maxDurationMs) + "ms";
    } else if (result.cancelled) {
        result.exitCode = -2;
        result.statusDetail = "Cancelled by governor";
    }

    auto endClock = std::chrono::steady_clock::now();
    result.durationMs = std::chrono::duration<double, std::milli>(endClock - startClock).count();

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hReadPipe);

    return result;
}

bool TerminalWatchdog::SpawnProcess(
    const std::string& command,
    HANDLE& outProcess,
    HANDLE& outReadPipe,
    HANDLE& outThread)
{
    outProcess = nullptr;
    outReadPipe = nullptr;
    outThread = nullptr;

    HANDLE hReadPipe = nullptr;
    HANDLE hWritePipe = nullptr;
    SECURITY_ATTRIBUTES saAttr = {};
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;

    if (!CreatePipe(&hReadPipe, &hWritePipe, &saAttr, 0)) {
        return false;
    }
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb = sizeof(STARTUPINFOA);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWritePipe;
    si.hStdError  = hWritePipe;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    std::string cmdLine = "cmd.exe /C " + command;
    std::vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back('\0');

    BOOL created = CreateProcessA(
        nullptr, cmdBuf.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);

    CloseHandle(hWritePipe);

    if (!created) {
        CloseHandle(hReadPipe);
        return false;
    }

    outProcess  = pi.hProcess;
    outReadPipe = hReadPipe;
    outThread   = pi.hThread;
    return true;
}

bool TerminalWatchdog::PollOnce(
    HANDLE hProcess,
    HANDLE hReadPipe,
    std::string& outputBuffer,
    uint64_t& bytesRead,
    uint64_t startTickMs,
    uint64_t maxDurationMs,
    bool& timedOut)
{
    timedOut = false;

    // Check timeout
    uint64_t now = (uint64_t)GetTickCount64();
    if (now - startTickMs > maxDurationMs) {
        timedOut = true;
        return false;
    }

    // Check process
    DWORD waitResult = WaitForSingleObject(hProcess, 0);
    bool processEnded = (waitResult == WAIT_OBJECT_0);

    // Non-blocking pipe read
    DWORD bytesAvail = 0;
    char buffer[4096];
    if (PeekNamedPipe(hReadPipe, nullptr, 0, nullptr, &bytesAvail, nullptr) &&
        bytesAvail > 0) {
        DWORD bytesReadChunk = 0;
        if (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesReadChunk, nullptr) &&
            bytesReadChunk > 0) {
            buffer[bytesReadChunk] = '\0';
            outputBuffer += buffer;
            bytesRead += bytesReadChunk;
        }
    }

    if (processEnded && bytesAvail == 0) {
        return false; // Done
    }
    return true; // Still running
}

void TerminalWatchdog::KillProcess(HANDLE hProcess, HANDLE hThread, HANDLE hReadPipe) {
    if (hProcess) {
        TerminateProcess(hProcess, 0xDEAD);
        CloseHandle(hProcess);
    }
    if (hThread) {
        CloseHandle(hThread);
    }
    if (hReadPipe) {
        CloseHandle(hReadPipe);
    }
}

// ============================================================================
// EXECUTION GOVERNOR — Singleton scheduler
// ============================================================================

ExecutionGovernor& ExecutionGovernor::instance() {
    static ExecutionGovernor inst;
    return inst;
}

ExecutionGovernor::ExecutionGovernor()
    : m_initialized(false), m_running(false), m_nextId(1),
      m_maxConcurrent(TerminalWatchdog::MAX_CONCURRENT_PROCESSES),
      m_activeTasks(0) {}

ExecutionGovernor::~ExecutionGovernor() {
    shutdown();
}

bool ExecutionGovernor::init() {
    if (m_initialized.load()) return true;

    m_running.store(true);
    m_watchdogThread = std::thread(&ExecutionGovernor::watchdogLoop, this);
    m_initialized.store(true);
    return true;
}

void ExecutionGovernor::shutdown() {
    if (!m_initialized.load()) return;

    // Kill all running tasks first
    killAll();

    m_running.store(false);
    if (m_watchdogThread.joinable()) {
        m_watchdogThread.join();
    }
    m_initialized.store(false);
}

GovernorTaskId ExecutionGovernor::submitCommand(
    const std::string& command,
    uint64_t timeoutMs,
    GovernorRiskTier risk,
    const std::string& description,
    std::function<void(const GovernorCommandResult&)> onComplete)
{
    if (!m_initialized.load()) init();

    GovernoredTask task;
    task.type = GovernorTaskType::TerminalCommand;
    task.risk = risk;
    task.command = command;
    task.timeoutMs = timeoutMs;
    task.description = description.empty() ? ("cmd: " + command.substr(0, 80)) : description;
    task.onComplete = onComplete;

    return submitTask(std::move(task));
}

GovernorTaskId ExecutionGovernor::submitTask(GovernoredTask&& task) {
    if (!m_initialized.load()) init();

    std::lock_guard<std::mutex> lock(m_mutex);

    // Assign ID
    task.id = m_nextId++;
    task.state.store(GovernorTaskState::Pending);
    task.startTime = std::chrono::steady_clock::now();
    task.bytesRead = 0;

    GovernorTaskId taskId = task.id;

    // Check concurrent limit
    if (m_activeTasks.load() >= m_maxConcurrent) {
        // Queue is full — wait for a slot or reject
        task.state.store(GovernorTaskState::Failed);
        task.outputBuffer = "[RawrXD Governor: Maximum concurrent tasks reached (" +
                            std::to_string(m_maxConcurrent) + ")]";
        m_tasks.emplace(taskId, std::move(task));
        m_stats.totalFailed++;
        m_stats.totalSubmitted++;
        return taskId;
    }

    m_tasks.emplace(taskId, std::move(task));
    m_stats.totalSubmitted++;

    // Launch worker thread for terminal commands
    auto& t = m_tasks[taskId];
    if (t.type == GovernorTaskType::TerminalCommand && !t.command.empty()) {
        t.state.store(GovernorTaskState::Running);
        m_activeTasks.fetch_add(1);

        // Update peak concurrent
        int current = m_activeTasks.load();
        if ((uint64_t)current > m_stats.peakConcurrent) {
            m_stats.peakConcurrent = current;
        }

        // Launch in detached worker thread
        std::thread worker(&ExecutionGovernor::executeCommandWorker, this, taskId);
        worker.detach();
    }

    return taskId;
}

bool ExecutionGovernor::cancelTask(GovernorTaskId id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_tasks.find(id);
    if (it == m_tasks.end()) return false;

    auto& task = it->second;
    GovernorTaskState current = task.state.load();

    if (current == GovernorTaskState::Running || current == GovernorTaskState::Pending) {
        task.state.store(GovernorTaskState::Cancelled);

        // Call custom cancel callback if provided
        if (task.cancelFn) {
            task.cancelFn();
        }

        // Kill process if running
        if (task.hProcess) {
            TerminateProcess(task.hProcess, 0xDEAD0001);
        }

        m_stats.totalCancelled++;
        return true;
    }
    return false;
}

bool ExecutionGovernor::killTask(GovernorTaskId id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_tasks.find(id);
    if (it == m_tasks.end()) return false;

    auto& task = it->second;
    if (task.hProcess) {
        TerminateProcess(task.hProcess, 0xDEAD);
        CloseHandle(task.hProcess);
        task.hProcess = nullptr;
    }
    if (task.hThread) {
        CloseHandle(task.hThread);
        task.hThread = nullptr;
    }
    if (task.hReadPipe) {
        CloseHandle(task.hReadPipe);
        task.hReadPipe = nullptr;
    }

    task.state.store(GovernorTaskState::Killed);
    task.endTime = std::chrono::steady_clock::now();
    m_stats.totalKilled++;
    return true;
}

bool ExecutionGovernor::getTaskResult(GovernorTaskId id, GovernorCommandResult& outResult) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_tasks.find(id);
    if (it == m_tasks.end()) return false;

    auto& task = it->second;
    GovernorTaskState state = task.state.load();

    if (state == GovernorTaskState::Pending || state == GovernorTaskState::Running) {
        return false; // Not done yet
    }

    outResult.output = task.outputBuffer;
    outResult.bytesRead = task.bytesRead;
    outResult.timedOut = (state == GovernorTaskState::TimedOut);
    outResult.cancelled = (state == GovernorTaskState::Cancelled);

    auto duration = std::chrono::duration<double, std::milli>(
        task.endTime - task.startTime);
    outResult.durationMs = duration.count();

    switch (state) {
        case GovernorTaskState::Completed:
            outResult.exitCode = 0;
            outResult.statusDetail = "Completed";
            break;
        case GovernorTaskState::TimedOut:
            outResult.exitCode = -1;
            outResult.statusDetail = "Timed out after " + std::to_string(task.timeoutMs) + "ms";
            break;
        case GovernorTaskState::Cancelled:
            outResult.exitCode = -2;
            outResult.statusDetail = "Cancelled";
            break;
        case GovernorTaskState::Killed:
            outResult.exitCode = -3;
            outResult.statusDetail = "Killed by watchdog";
            break;
        case GovernorTaskState::Failed:
            outResult.exitCode = -4;
            outResult.statusDetail = "Failed: " + task.outputBuffer.substr(0, 200);
            break;
        default:
            outResult.exitCode = -99;
            outResult.statusDetail = "Unknown state";
            break;
    }
    return true;
}

bool ExecutionGovernor::isTaskActive(GovernorTaskId id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_tasks.find(id);
    if (it == m_tasks.end()) return false;
    GovernorTaskState s = it->second.state.load();
    return (s == GovernorTaskState::Pending || s == GovernorTaskState::Running);
}

GovernorTaskState ExecutionGovernor::getTaskState(GovernorTaskId id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_tasks.find(id);
    if (it == m_tasks.end()) return GovernorTaskState::Failed;
    return it->second.state.load();
}

GovernorCommandResult ExecutionGovernor::waitForTask(GovernorTaskId id, uint64_t waitMs) {
    auto startWait = std::chrono::steady_clock::now();
    GovernorCommandResult result = {};
    result.exitCode = -1;

    while (true) {
        if (getTaskResult(id, result)) {
            return result;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startWait).count();
        if ((uint64_t)elapsed > waitMs) {
            result.timedOut = true;
            result.statusDetail = "Wait timeout exceeded (" + std::to_string(waitMs) + "ms)";
            return result;
        }

        Sleep(25); // Yield to other threads
    }
}

GovernorStats ExecutionGovernor::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    GovernorStats s = m_stats;
    s.activeTaskCount = m_activeTasks.load();
    return s;
}

std::string ExecutionGovernor::getStatusString() const {
    auto s = getStats();
    std::ostringstream oss;
    oss << "════════════════════════════════════════════\n"
        << "  Execution Governor Status (Phase 10A)\n"
        << "════════════════════════════════════════════\n"
        << "  Initialized:       " << (m_initialized.load() ? "YES" : "NO") << "\n"
        << "  ─────────────────────────────────────────\n"
        << "  Total Submitted:   " << s.totalSubmitted << "\n"
        << "  Total Completed:   " << s.totalCompleted << "\n"
        << "  Total Timed Out:   " << s.totalTimedOut << "\n"
        << "  Total Killed:      " << s.totalKilled << "\n"
        << "  Total Cancelled:   " << s.totalCancelled << "\n"
        << "  Total Failed:      " << s.totalFailed << "\n"
        << "  Total Rollbacks:   " << s.totalRollbacks << "\n"
        << "  ─────────────────────────────────────────\n"
        << "  Active Tasks:      " << s.activeTaskCount << "\n"
        << "  Peak Concurrent:   " << s.peakConcurrent << "\n"
        << "  Max Concurrent:    " << m_maxConcurrent << "\n"
        << "  ─────────────────────────────────────────\n"
        << "  Total Time:        " << (int)s.totalElapsedMs << " ms\n"
        << "  Avg Duration:      " << std::fixed << std::setprecision(1)
        << s.avgTaskDurationMs << " ms\n"
        << "  Longest Task:      " << s.longestTaskMs << " ms\n"
        << "════════════════════════════════════════════";
    return oss.str();
}

std::vector<std::string> ExecutionGovernor::getActiveTaskDescriptions() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> descs;
    for (const auto& kv : m_tasks) {
        GovernorTaskState s = kv.second.state.load();
        if (s == GovernorTaskState::Running || s == GovernorTaskState::Pending) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - kv.second.startTime).count();
            descs.push_back("[" + std::to_string(kv.first) + "] " +
                            kv.second.description + " (" +
                            std::to_string(elapsed) + "ms / " +
                            std::to_string(kv.second.timeoutMs) + "ms)");
        }
    }
    return descs;
}

void ExecutionGovernor::killAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& kv : m_tasks) {
        auto& task = kv.second;
        GovernorTaskState s = task.state.load();
        if (s == GovernorTaskState::Running || s == GovernorTaskState::Pending) {
            if (task.hProcess) {
                TerminateProcess(task.hProcess, 0xDEAD);
                CloseHandle(task.hProcess);
                task.hProcess = nullptr;
            }
            if (task.hThread) {
                CloseHandle(task.hThread);
                task.hThread = nullptr;
            }
            if (task.hReadPipe) {
                CloseHandle(task.hReadPipe);
                task.hReadPipe = nullptr;
            }
            task.state.store(GovernorTaskState::Killed);
            task.endTime = std::chrono::steady_clock::now();
            m_stats.totalKilled++;
        }
    }
    m_activeTasks.store(0);
}

void ExecutionGovernor::setMaxConcurrent(int max) {
    if (max > 0 && max <= 64) {
        m_maxConcurrent = max;
    }
}

// ============================================================================
// PRIVATE — Watchdog thread
// ============================================================================

void ExecutionGovernor::watchdogLoop() {
    while (m_running.load()) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto now = std::chrono::steady_clock::now();

            for (auto& kv : m_tasks) {
                auto& task = kv.second;
                GovernorTaskState s = task.state.load();
                if (s != GovernorTaskState::Running) continue;

                auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - task.startTime).count();

                if ((uint64_t)elapsedMs > task.timeoutMs) {
                    // TIMEOUT — hard kill
                    if (task.hProcess) {
                        TerminateProcess(task.hProcess, 0xDEAD);
                        // Don't close handles here — worker thread will clean up
                    }
                    task.state.store(GovernorTaskState::TimedOut);
                    task.endTime = now;
                    task.outputBuffer += "\n[RawrXD Watchdog: TIMEOUT after " +
                                         std::to_string(elapsedMs) + "ms — process killed]";
                    m_stats.totalTimedOut++;

                    // Invoke rollback if enabled
                    if (task.rollbackEnabled && task.rollbackFn) {
                        task.rollbackFn();
                        m_stats.totalRollbacks++;
                    }
                }
            }

            // Periodic prune
            pruneCompletedTasks();
        }

        Sleep(50); // Poll every 50ms — ~20 Hz watchdog frequency
    }
}

// ============================================================================
// PRIVATE — Worker thread for command execution
// ============================================================================

void ExecutionGovernor::executeCommandWorker(GovernorTaskId taskId) {
    // Copy command and timeout under lock
    std::string command;
    uint64_t timeoutMs;
    std::atomic<GovernorTaskState>* statePtr = nullptr;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_tasks.find(taskId);
        if (it == m_tasks.end()) {
            m_activeTasks.fetch_sub(1);
            return;
        }
        command = it->second.command;
        timeoutMs = it->second.timeoutMs;
        statePtr = &(it->second.state);
    }

    // Execute with watchdog (this blocks until done/timeout/cancel)
    GovernorCommandResult result = TerminalWatchdog::ExecuteSafe(command, timeoutMs, statePtr);

    // Store results back
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_tasks.find(taskId);
        if (it != m_tasks.end()) {
            auto& task = it->second;
            task.outputBuffer = result.output;
            task.bytesRead = result.bytesRead;
            task.endTime = std::chrono::steady_clock::now();

            GovernorTaskState finalState;
            if (result.timedOut) {
                finalState = GovernorTaskState::TimedOut;
            } else if (result.cancelled) {
                finalState = GovernorTaskState::Cancelled;
            } else if (result.exitCode == 0) {
                finalState = GovernorTaskState::Completed;
            } else {
                finalState = GovernorTaskState::Completed; // Non-zero exit is still "completed"
            }

            // Only update state if watchdog hasn't already set it
            GovernorTaskState current = task.state.load();
            if (current == GovernorTaskState::Running) {
                task.state.store(finalState);
            }

            // Update stats
            auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                task.endTime - task.startTime).count();

            if (finalState == GovernorTaskState::Completed)   m_stats.totalCompleted++;
            else if (finalState == GovernorTaskState::TimedOut) m_stats.totalTimedOut++;

            m_stats.totalElapsedMs += (double)durationMs;
            if ((uint64_t)durationMs > m_stats.longestTaskMs) {
                m_stats.longestTaskMs = durationMs;
            }
            if (m_stats.totalCompleted + m_stats.totalTimedOut + m_stats.totalFailed > 0) {
                m_stats.avgTaskDurationMs = m_stats.totalElapsedMs /
                    (double)(m_stats.totalCompleted + m_stats.totalTimedOut + m_stats.totalFailed);
            }

            // Fire completion callback (outside task lock would be better,
            // but for safety we keep it simple)
            if (task.onComplete) {
                task.onComplete(result);
            }

            // Rollback on failure if enabled
            if (result.exitCode != 0 && task.rollbackEnabled && task.rollbackFn) {
                task.rollbackFn();
                m_stats.totalRollbacks++;
            }
        }
    }

    m_activeTasks.fetch_sub(1);
}

// ============================================================================
// PRIVATE — Finalize task
// ============================================================================

void ExecutionGovernor::finalizeTask(GovernorTaskId taskId, GovernorTaskState finalState) {
    // Already handled in executeCommandWorker — this is a utility for future task types
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_tasks.find(taskId);
    if (it == m_tasks.end()) return;

    it->second.state.store(finalState);
    it->second.endTime = std::chrono::steady_clock::now();
}

// ============================================================================
// PRIVATE — Prune old completed tasks
// ============================================================================

void ExecutionGovernor::pruneCompletedTasks() {
    // Already under lock from watchdogLoop
    if (m_tasks.size() <= (size_t)MAX_RETAINED_TASKS) return;

    auto now = std::chrono::steady_clock::now();
    std::vector<GovernorTaskId> toPrune;

    for (const auto& kv : m_tasks) {
        GovernorTaskState s = kv.second.state.load();
        if (s != GovernorTaskState::Running && s != GovernorTaskState::Pending) {
            auto age = std::chrono::duration_cast<std::chrono::seconds>(
                now - kv.second.endTime).count();
            if (age > PRUNE_AFTER_SECONDS) {
                toPrune.push_back(kv.first);
            }
        }
    }

    for (auto id : toPrune) {
        m_tasks.erase(id);
    }
}
