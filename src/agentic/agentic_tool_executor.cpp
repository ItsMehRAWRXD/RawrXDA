// agentic_tool_executor.cpp
// Safe system tool execution with approval gates and policy enforcement

#include "agentic_tool_executor.hpp"
#include "observability/Logger.hpp"
#include <chrono>
#include <algorithm>
#include <sstream>
#include <thread>
#include <array>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <processthreadsapi.h>
#include <psapi.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace Agentic {

namespace {

std::string truncateCaptured(const std::string& in, size_t maxBytes) {
    if (maxBytes == 0 || in.size() <= maxBytes) {
        return in;
    }
    const std::string note = "\n[output truncated]";
    if (maxBytes <= note.size()) {
        return note.substr(0, maxBytes);
    }
    return in.substr(0, maxBytes - note.size()) + note;
}

#ifdef _WIN32
struct MonitorContext {
    HANDLE process = nullptr;
    HANDLE stdoutRead = nullptr;
    HANDLE stderrRead = nullptr;
};

std::string quoteArgWin(const std::string& arg) {
    if (arg.empty()) {
        return "\"\"";
    }
    bool needsQuotes = false;
    for (char c : arg) {
        if (c == ' ' || c == '\t' || c == '"') {
            needsQuotes = true;
            break;
        }
    }
    if (!needsQuotes) {
        return arg;
    }
    std::string out;
    out.push_back('"');
    for (char c : arg) {
        if (c == '"') {
            out += "\\\"";
        } else {
            out.push_back(c);
        }
    }
    out.push_back('"');
    return out;
}
#else
struct MonitorContext {
    pid_t pid = -1;
    int stdoutFd = -1;
    int stderrFd = -1;
};
#endif

}  // namespace

// ============================================================================
// ToolExecutor Implementation
// ============================================================================

ToolExecutor::ToolExecutor() {
    // Pre-register common tools with safe defaults
    
    // Build tools
    ToolPolicy cmake;
    cmake.tool_name = "cmake";
    cmake.category = ToolCategory::Build;
    cmake.timeout_seconds = 600;
    cmake.allow_any_args = false;
    cmake.allowed_args = {"--build", ".", "--config", "Debug", "Release", "--target"};
    registerTool(cmake);
    
    // Compiler
    ToolPolicy cl;
    cl.tool_name = "cl.exe";
    cl.category = ToolCategory::Compile;
    cl.timeout_seconds = 300;
    cl.allow_any_args = false;
    cl.allowed_args = {"/c", "/Fo", "/I", "/std:c++17", "/Wall", "/WX"};
    registerTool(cl);
    
    // Test runner
    ToolPolicy ctest;
    ctest.tool_name = "ctest";
    ctest.category = ToolCategory::Test;
    ctest.timeout_seconds = 600;
    ctest.read_only = true;
    registerTool(ctest);
    
    // Formatter
    ToolPolicy clang_format;
    clang_format.tool_name = "clang-format";
    clang_format.category = ToolCategory::Format;
    clang_format.timeout_seconds = 120;
    clang_format.allow_any_args = false;
    clang_format.allowed_args = {"-i", "--style=LLVM", "-r"};
    registerTool(clang_format);
    
    // Linter
    ToolPolicy clang_tidy;
    clang_tidy.tool_name = "clang-tidy";
    clang_tidy.category = ToolCategory::Lint;
    clang_tidy.timeout_seconds = 300;
    clang_tidy.read_only = true;
    registerTool(clang_tidy);
    
    // Git
    ToolPolicy git;
    git.tool_name = "git";
    git.category = ToolCategory::VCS;
    git.timeout_seconds = 120;
    git.allow_any_args = false;
    git.allowed_args = {"status", "log", "diff", "add", "commit", "push", "pull", "branch"};
    registerTool(git);
}

ToolExecutor::~ToolExecutor() {
}

void ToolExecutor::registerTool(const ToolPolicy& policy) {
    m_policies[policy.tool_name] = policy;
    if (m_logFn) {
        m_logFn("Registered tool: " + policy.tool_name);
    }
}

void ToolExecutor::unregisterTool(const std::string& tool_name) {
    m_policies.erase(tool_name);
}

ExecutionResult ToolExecutor::execute(const ExecutionRequest& request) {
    ExecutionResult result;
    
    if (!validateRequest(request)) {
        result.success = false;
        result.exit_code = -1;
        result.stderr_text = "Invalid execution request";
        return result;
    }
    
    if (m_logFn) {
        m_logFn("[ToolExec] Executing: " + request.tool_name + " (approval_required=" +
               (request.requires_approval ? "yes" : "no") + ")");
    }
    
    // Check approval
    if (!checkApproval(request)) {
        result.success = false;
        result.exit_code = -2;
        result.stderr_text = "Execution rejected by approval policy";
        return result;
    }
    
    // Validate schema for structured tool call
    std::string schemaErr;
    if (!validateToolCallSchema(request, schemaErr)) {
        result.success = false;
        result.exit_code = -3;
        result.stderr_text = "Tool schema validation failed: " + schemaErr;
        return result;
    }

    // Execute with retries and fallback chain
    return executeWithRetry(request);
}

std::vector<ExecutionResult> ToolExecutor::executeBatch(const std::vector<ExecutionRequest>& requests) {
    std::vector<ExecutionResult> results;
    
    for (const auto& request : requests) {
        auto result = execute(request);
        results.push_back(result);
        
        // Stop on first failure unless explicitly continuing
        if (!result.success) {
            if (m_logFn) {
                m_logFn("[ToolExec] Batch execution stopped: " + request.tool_name + " failed");
            }
            break;
        }
    }
    
    return results;
}

ExecutionResult ToolExecutor::executeCompile(const std::vector<std::string>& source_files,
                                             const std::string& output_file,
                                             const std::vector<std::string>& flags) {
    ExecutionRequest req;
    req.tool_name = "cl.exe";
    req.description = "Compile " + std::to_string(source_files.size()) + " files";
    
    for (const auto& src : source_files) {
        req.args.push_back(src);
    }
    
    req.args.push_back("/Fo" + output_file);
    
    for (const auto& flag : flags) {
        req.args.push_back(flag);
    }
    
    return execute(req);
}

ExecutionResult ToolExecutor::executeTests(const std::vector<std::string>& test_targets) {
    ExecutionRequest req;
    req.tool_name = "ctest";
    req.description = "Run " + std::to_string(test_targets.size()) + " test targets";
    req.args.push_back("--verbose");
    
    for (const auto& target : test_targets) {
        req.args.push_back("-R");
        req.args.push_back(target);
    }
    
    return execute(req);
}

ExecutionResult ToolExecutor::executeBuild(const std::string& build_dir) {
    ExecutionRequest req;
    req.tool_name = "cmake";
    req.working_dir = build_dir;
    req.description = "Build in " + build_dir;
    req.args = {"--build", ".", "--config", "Release"};
    
    return execute(req);
}

ExecutionResult ToolExecutor::executeFormat(const std::vector<std::string>& files) {
    ExecutionRequest req;
    req.tool_name = "clang-format";
    req.description = "Format " + std::to_string(files.size()) + " files";
    req.args.push_back("-i");  // In-place
    req.args.push_back("-r");  // Recursive
    
    for (const auto& file : files) {
        req.args.push_back(file);
    }
    
    return execute(req);
}

ExecutionResult ToolExecutor::executeLint(const std::vector<std::string>& files) {
    ExecutionRequest req;
    req.tool_name = "clang-tidy";
    req.description = "Lint " + std::to_string(files.size()) + " files";
    
    for (const auto& file : files) {
        req.args.push_back(file);
    }
    
    return execute(req);
}

ExecutionResult ToolExecutor::executeGit(const std::string& command, const std::vector<std::string>& args) {
    ExecutionRequest req;
    req.tool_name = "git";
    req.args.push_back(command);
    
    for (const auto& arg : args) {
        req.args.push_back(arg);
    }
    
    req.description = "Git " + command;
    return execute(req);
}

ExecutionResult ToolExecutor::copyFiles(const std::vector<std::pair<std::string, std::string>>& src_dst) {
    ExecutionResult batch_result;
    batch_result.success = true;
    
    for (const auto& [src, dst] : src_dst) {
        ExecutionRequest req;
#ifdef _WIN32
        req.tool_name = "copy";
#else
        req.tool_name = "cp";
#endif
        req.args = {src, dst};
        req.description = "Copy " + src + " to " + dst;
        
        auto result = execute(req);
        if (!result.success) {
            batch_result.success = false;
            batch_result.stderr_text += result.stderr_text + "\n";
        } else {
            batch_result.generated_files.push_back(dst);
        }
    }
    
    return batch_result;
}

ExecutionResult ToolExecutor::deleteFiles(const std::vector<std::string>& files) {
    ExecutionResult batch_result;
    batch_result.success = true;
    
    for (const auto& file : files) {
        ExecutionRequest req;
#ifdef _WIN32
        req.tool_name = "del";
#else
        req.tool_name = "rm";
#endif
        req.args = {file};
        req.description = "Delete " + file;
        
        auto result = execute(req);
        if (!result.success) {
            batch_result.success = false;
            batch_result.stderr_text += result.stderr_text + "\n";
        } else {
            batch_result.deleted_files.push_back(file);
        }
    }
    
    return batch_result;
}

ExecutionResult ToolExecutor::renameFile(const std::string& old_name, const std::string& new_name) {
    ExecutionRequest req;
#ifdef _WIN32
    req.tool_name = "ren";
#else
    req.tool_name = "mv";
#endif
    req.args = {old_name, new_name};
    req.description = "Rename " + old_name + " to " + new_name;
    
    return execute(req);
}

bool ToolExecutor::validateRequest(const ExecutionRequest& request) {
    if (request.tool_name.empty()) {
        return false;
    }
    
    auto policy = getPolicy(request.tool_name);
    if (!policy) {
        if (m_logFn) {
            m_logFn("Warning: Tool not registered: " + request.tool_name);
        }
        // Allow unregistered tools by default
    }
    
    return checkPolicy(policy, request);
}

bool ToolExecutor::checkApproval(const ExecutionRequest& request) {
    if (request.requires_approval && m_approvalFn) {
        return m_approvalFn(request);
    }
    
    auto policy = getPolicy(request.tool_name);
    if (policy && policy->requires_approval && m_approvalFn) {
        return m_approvalFn(request);
    }
    
    return true;  // Default to approved
}

bool ToolExecutor::checkPolicy(const ToolPolicy* policy, const ExecutionRequest& request) const {
    if (!policy) {
        return true;  // No policy = allow by default
    }
    
    if (!policy->enabled) {
        return false;
    }
    
    if (policy->read_only && m_all_readonly) {
        return true;
    }
    
    if (!policy->allow_any_args && !policy->allowed_args.empty()) {
        // Check that all args are whitelisted
        for (const auto& arg : request.args) {
            bool found = false;
            for (const auto& allowed : policy->allowed_args) {
                if (arg == allowed || arg.find(allowed) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                if (m_logFn) {
                    m_logFn("Policy violation: arg '" + arg + "' not in whitelist for " + request.tool_name);
                }
                return false;
            }
        }
    }
    
    return true;
}

bool ToolExecutor::validateToolCallSchema(const ExecutionRequest& request, std::string& error) const {
    if (request.tool_name.empty()) {
        error = "tool_name required";
        return false;
    }
    if (request.tool_name.find_first_of("\n\r\0") != std::string::npos) {
        error = "tool_name contains invalid control chars";
        return false;
    }
    if (request.args.size() > 128) {
        error = "too many args";
        return false;
    }
    for (const auto& arg : request.args) {
        if (arg.size() > 4096) {
            error = "arg too long";
            return false;
        }
    }
    // additional mode-specific schema checks
    if (request.tool_name == "execute_command") {
        if (request.args.empty()) {
            error = "command args are required for execute_command";
            return false;
        }
    }
    return true;
}

ToolExecutor::ToolFailureType ToolExecutor::classifyFailure(const ExecutionResult& result) const {
    if (result.success) return ToolFailureType::None;
    if (result.exit_code == -2 || result.exit_code == -3) return ToolFailureType::Permanent;
    if (result.exit_code == WAIT_TIMEOUT || result.stderr_text.find("timeout") != std::string::npos) {
        return ToolFailureType::Timeout;
    }
    if (result.exit_code == 1 || result.exit_code == 2) {
        // common transient tool exit codes
        return ToolFailureType::Transient;
    }
    if (result.stderr_text.find("permission") != std::string::npos || result.stderr_text.find("Access is denied") != std::string::npos) {
        return ToolFailureType::PermissionDenied;
    }
    return ToolFailureType::Unknown;
}

ExecutionResult ToolExecutor::executeWithRetry(const ExecutionRequest& request, int maxRetries) {
    ExecutionResult lastResult;
    int attempt = 0;
    int backoffMs = 250;

    while (attempt <= maxRetries) {
        if (attempt > 0) {
            if (m_logFn) {
                m_logFn("[ToolExec] Retry attempt " + std::to_string(attempt) + " for " + request.tool_name);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(backoffMs));
            backoffMs = std::min(backoffMs * 2, 5000);
        }

        lastResult = executeInternal(request);
        if (lastResult.success) {
            return lastResult;
        }

        auto failure = classifyFailure(lastResult);
        if (failure == ToolFailureType::Permanent || failure == ToolFailureType::PermissionDenied) {
            break;
        }

        if (attempt == maxRetries) {
            break;
        }

        const ToolPolicy* policy = getPolicy(request.tool_name);
        if (policy && !policy->fallback_tools.empty()) {
            for (const auto& fallback : policy->fallback_tools) {
                ExecutionRequest fallbackRequest = request;
                fallbackRequest.tool_name = fallback;
                if (m_logFn) {
                    m_logFn("[ToolExec] Falling back to " + fallback + " for " + request.tool_name);
                }
                auto fallbackResult = executeInternal(fallbackRequest);
                if (fallbackResult.success) {
                    return fallbackResult;
                }
            }
        }

        ++attempt;
    }

    return lastResult;
}

ExecutionResult ToolExecutor::executeInternal(const ExecutionRequest& request) {
    auto start = std::chrono::high_resolution_clock::now();
    
    auto policy = getPolicy(request.tool_name);
    int timeout = (policy ? policy->timeout_seconds : m_default_timeout);
    size_t maxOutputBytes = (policy ? policy->max_output_bytes : (100 * 1024 * 1024));
    
    ExecutionResult result = spawnProcess(request.tool_name,
                                        request.args,
                                        request.working_dir,
                                        timeout,
                                        maxOutputBytes);
    
    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    if (m_logFn) {
        m_logFn("[ToolExec] " + request.tool_name + " exit=" + std::to_string(result.exit_code) +
               " duration=" + std::to_string(result.duration_ms) + "ms");
    }
    
    return result;
}

ExecutionResult ToolExecutor::spawnProcess(const std::string& exe, const std::vector<std::string>& args,
                                           const std::string& cwd, int timeout_seconds,
                                           size_t max_output_bytes) {
    ExecutionResult result;

#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hStdOutRead = nullptr;
    HANDLE hStdOutWrite = nullptr;
    HANDLE hStdErrRead = nullptr;
    HANDLE hStdErrWrite = nullptr;

    if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0) ||
        !CreatePipe(&hStdErrRead, &hStdErrWrite, &sa, 0)) {
        result.success = false;
        result.exit_code = -1;
        result.stderr_text = "CreatePipe failed";
        return result;
    }

    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStdErrRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = hStdOutWrite;
    si.hStdError = hStdErrWrite;

    PROCESS_INFORMATION pi{};
    std::string cmdLine = quoteArgWin(exe);
    for (const auto& arg : args) {
        cmdLine += " ";
        cmdLine += quoteArgWin(arg);
    }
    std::vector<char> mutableCmd(cmdLine.begin(), cmdLine.end());
    mutableCmd.push_back('\0');

    const char* cwdPtr = cwd.empty() ? nullptr : cwd.c_str();
    BOOL ok = CreateProcessA(nullptr, mutableCmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                             nullptr, cwdPtr, &si, &pi);

    CloseHandle(hStdOutWrite);
    CloseHandle(hStdErrWrite);

    if (!ok) {
        DWORD err = GetLastError();
        CloseHandle(hStdOutRead);
        CloseHandle(hStdErrRead);
        result.success = false;
        result.exit_code = -1;
        result.stderr_text = "CreateProcess failed: " + std::to_string(static_cast<unsigned long>(err));
        return result;
    }

    CloseHandle(pi.hThread);

    MonitorContext ctx;
    ctx.process = pi.hProcess;
    ctx.stdoutRead = hStdOutRead;
    ctx.stderrRead = hStdErrRead;

    std::string out;
    std::string err;
    size_t peakBytes = 0;
    const bool completed = monitorProcess(&ctx, timeout_seconds * 1000, out, err, max_output_bytes, &peakBytes);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(hStdOutRead);
    CloseHandle(hStdErrRead);

    result.stdout_text = truncateCaptured(out, max_output_bytes);
    result.stderr_text = truncateCaptured(err, max_output_bytes);
    result.exit_code = static_cast<int>(exitCode);
    result.peak_memory_bytes = peakBytes;
    result.success = completed && (result.exit_code == 0);

    if (!completed && result.stderr_text.find("timeout") == std::string::npos) {
        if (!result.stderr_text.empty()) result.stderr_text += "\n";
        result.stderr_text += "Process timeout or monitor failure";
    }
#else
    int outPipe[2] = {-1, -1};
    int errPipe[2] = {-1, -1};
    if (pipe(outPipe) != 0 || pipe(errPipe) != 0) {
        result.success = false;
        result.exit_code = -1;
        result.stderr_text = "pipe failed";
        return result;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(outPipe[0]); close(outPipe[1]);
        close(errPipe[0]); close(errPipe[1]);
        result.success = false;
        result.exit_code = -1;
        result.stderr_text = "fork failed";
        return result;
    }

    if (pid == 0) {
        dup2(outPipe[1], STDOUT_FILENO);
        dup2(errPipe[1], STDERR_FILENO);
        close(outPipe[0]); close(outPipe[1]);
        close(errPipe[0]); close(errPipe[1]);
        if (!cwd.empty()) {
            chdir(cwd.c_str());
        }
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(exe.c_str()));
        for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        execvp(exe.c_str(), argv.data());
        _exit(127);
    }

    close(outPipe[1]);
    close(errPipe[1]);
    fcntl(outPipe[0], F_SETFL, O_NONBLOCK);
    fcntl(errPipe[0], F_SETFL, O_NONBLOCK);

    MonitorContext ctx;
    ctx.pid = pid;
    ctx.stdoutFd = outPipe[0];
    ctx.stderrFd = errPipe[0];

    std::string out;
    std::string err;
    size_t peakBytes = 0;
    bool completed = monitorProcess(&ctx, timeout_seconds * 1000, out, err, max_output_bytes, &peakBytes);

    int status = 0;
    waitpid(pid, &status, 0);
    close(outPipe[0]);
    close(errPipe[0]);

    result.stdout_text = truncateCaptured(out, max_output_bytes);
    result.stderr_text = truncateCaptured(err, max_output_bytes);
    result.peak_memory_bytes = peakBytes;
    if (WIFEXITED(status)) result.exit_code = WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) result.exit_code = 128 + WTERMSIG(status);
    else result.exit_code = -1;
    result.success = completed && (result.exit_code == 0);
#endif

    if (m_outputFn && !result.stdout_text.empty()) {
        m_outputFn(result.stdout_text);
    }

    return result;
}

bool ToolExecutor::monitorProcess(void* process_handle, int timeout_ms, std::string& stdout,
                                  std::string& stderr, size_t max_output_bytes,
                                  size_t* peak_memory_bytes) {
    if (!process_handle) {
        return false;
    }

    auto appendBounded = [&](std::string& dst, const char* src, size_t count) {
        if (count == 0 || max_output_bytes == 0) {
            return;
        }
        if (dst.size() >= max_output_bytes) {
            return;
        }
        const size_t room = max_output_bytes - dst.size();
        dst.append(src, std::min(room, count));
    };

#ifdef _WIN32
    auto* ctx = static_cast<MonitorContext*>(process_handle);
    const DWORD pollMs = 25;
    const DWORD start = GetTickCount();
    bool stdoutOpen = true;
    bool stderrOpen = true;

    while (true) {
        std::array<char, 4096> buf{};
        DWORD avail = 0;
        DWORD readBytes = 0;

        if (stdoutOpen && PeekNamedPipe(ctx->stdoutRead, nullptr, 0, nullptr, &avail, nullptr) && avail > 0) {
            if (ReadFile(ctx->stdoutRead, buf.data(), static_cast<DWORD>(buf.size()), &readBytes, nullptr) &&
                readBytes > 0) {
                appendBounded(stdout, buf.data(), static_cast<size_t>(readBytes));
            }
        }

        avail = 0;
        readBytes = 0;
        if (stderrOpen && PeekNamedPipe(ctx->stderrRead, nullptr, 0, nullptr, &avail, nullptr) && avail > 0) {
            if (ReadFile(ctx->stderrRead, buf.data(), static_cast<DWORD>(buf.size()), &readBytes, nullptr) &&
                readBytes > 0) {
                appendBounded(stderr, buf.data(), static_cast<size_t>(readBytes));
            }
        }

        if (peak_memory_bytes) {
            PROCESS_MEMORY_COUNTERS_EX pmc{};
            if (GetProcessMemoryInfo(ctx->process, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
                *peak_memory_bytes = (std::max)(*peak_memory_bytes, static_cast<size_t>(pmc.PeakWorkingSetSize));
            }
        }

        DWORD waitRes = WaitForSingleObject(ctx->process, pollMs);
        if (waitRes == WAIT_OBJECT_0) {
            // Drain remaining pipe data quickly
            for (int i = 0; i < 6; ++i) {
                DWORD ra = 0;
                DWORD rb = 0;
                std::array<char, 4096> tail{};
                if (PeekNamedPipe(ctx->stdoutRead, nullptr, 0, nullptr, &ra, nullptr) && ra > 0 &&
                    ReadFile(ctx->stdoutRead, tail.data(), static_cast<DWORD>(tail.size()), &rb, nullptr) && rb > 0) {
                    appendBounded(stdout, tail.data(), static_cast<size_t>(rb));
                }
                ra = 0; rb = 0;
                if (PeekNamedPipe(ctx->stderrRead, nullptr, 0, nullptr, &ra, nullptr) && ra > 0 &&
                    ReadFile(ctx->stderrRead, tail.data(), static_cast<DWORD>(tail.size()), &rb, nullptr) && rb > 0) {
                    appendBounded(stderr, tail.data(), static_cast<size_t>(rb));
                }
                if (ra == 0) break;
            }
            return true;
        }

        if (timeout_ms > 0 && (GetTickCount() - start) > static_cast<DWORD>(timeout_ms)) {
            TerminateProcess(ctx->process, 124);
            if (!stderr.empty()) stderr += "\n";
            stderr += "Process terminated due to timeout";
            return false;
        }
    }
#else
    auto* ctx = static_cast<MonitorContext*>(process_handle);
    const auto start = std::chrono::steady_clock::now();

    while (true) {
        fd_set set;
        FD_ZERO(&set);
        int maxFd = -1;
        if (ctx->stdoutFd >= 0) {
            FD_SET(ctx->stdoutFd, &set);
            maxFd = (std::max)(maxFd, ctx->stdoutFd);
        }
        if (ctx->stderrFd >= 0) {
            FD_SET(ctx->stderrFd, &set);
            maxFd = (std::max)(maxFd, ctx->stderrFd);
        }

        timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = 25000;
        if (maxFd >= 0) {
            select(maxFd + 1, &set, nullptr, nullptr, &tv);
            std::array<char, 4096> buf{};
            if (ctx->stdoutFd >= 0 && FD_ISSET(ctx->stdoutFd, &set)) {
                ssize_t n = read(ctx->stdoutFd, buf.data(), buf.size());
                if (n > 0) appendBounded(stdout, buf.data(), static_cast<size_t>(n));
                else if (n == 0) { close(ctx->stdoutFd); ctx->stdoutFd = -1; }
            }
            if (ctx->stderrFd >= 0 && FD_ISSET(ctx->stderrFd, &set)) {
                ssize_t n = read(ctx->stderrFd, buf.data(), buf.size());
                if (n > 0) appendBounded(stderr, buf.data(), static_cast<size_t>(n));
                else if (n == 0) { close(ctx->stderrFd); ctx->stderrFd = -1; }
            }
        }

        int status = 0;
        pid_t done = waitpid(ctx->pid, &status, WNOHANG);
        if (done == ctx->pid) {
            return true;
        }

        if (timeout_ms > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed > timeout_ms) {
                kill(ctx->pid, SIGKILL);
                if (!stderr.empty()) stderr += "\n";
                stderr += "Process terminated due to timeout";
                return false;
            }
        }
    }
#endif
}

const ToolPolicy* ToolExecutor::getPolicy(const std::string& tool) const {
    auto it = m_policies.find(tool);
    if (it != m_policies.end()) {
        return &it->second;
    }
    return nullptr;
}

void ToolExecutor::setAllToolsReadOnly(bool readonly) {
    m_all_readonly = readonly;
}

} // namespace Agentic
