// agentic_tool_executor.cpp
// Safe system tool execution with approval gates and policy enforcement

#include "agentic_tool_executor.hpp"
#include "observability/Logger.hpp"
#include <chrono>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <processthreadsapi.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace Agentic {

namespace {

std::string quoteCommandArg(const std::string& value) {
    if (value.empty()) {
        return "\"\"";
    }

    bool needs_quotes = false;
    for (char ch : value) {
        if (ch == ' ' || ch == '\t' || ch == '"') {
            needs_quotes = true;
            break;
        }
    }

    if (!needs_quotes) {
        return value;
    }

    std::string quoted;
    quoted.reserve(value.size() + 8);
    quoted.push_back('"');
    for (char ch : value) {
        if (ch == '"') {
            quoted += "\\\"";
        } else {
            quoted.push_back(ch);
        }
    }
    quoted.push_back('"');
    return quoted;
}

} // namespace

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
    
    // Execute
    return executeInternal(request);
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
        std::error_code ec;
        std::filesystem::path src_path(src);
        std::filesystem::path dst_path(dst);

        if (std::filesystem::is_directory(src_path, ec)) {
            ec.clear();
            std::filesystem::copy(src_path,
                                  dst_path,
                                  std::filesystem::copy_options::recursive |
                                      std::filesystem::copy_options::overwrite_existing,
                                  ec);
        } else {
            std::filesystem::create_directories(dst_path.parent_path(), ec);
            ec.clear();
            std::filesystem::copy_file(src_path,
                                       dst_path,
                                       std::filesystem::copy_options::overwrite_existing,
                                       ec);
        }

        if (ec) {
            batch_result.success = false;
            batch_result.stderr_text += "Copy failed: " + src + " -> " + dst + ": " + ec.message() + "\n";
        } else {
            batch_result.generated_files.push_back(dst);
            batch_result.stdout_text += "Copied " + src + " -> " + dst + "\n";
        }
    }

    return batch_result;
}

ExecutionResult ToolExecutor::deleteFiles(const std::vector<std::string>& files) {
    ExecutionResult batch_result;
    batch_result.success = true;

    for (const auto& file : files) {
        std::error_code ec;
        const auto removed = std::filesystem::remove_all(std::filesystem::path(file), ec);
        if (ec) {
            batch_result.success = false;
            batch_result.stderr_text += "Delete failed: " + file + ": " + ec.message() + "\n";
        } else {
            batch_result.deleted_files.push_back(file);
            batch_result.stdout_text += "Deleted " + file + " (" + std::to_string(removed) + " entries)\n";
        }
    }

    return batch_result;
}

ExecutionResult ToolExecutor::renameFile(const std::string& old_name, const std::string& new_name) {
    ExecutionResult result;
    std::error_code ec;
    std::filesystem::path new_path(new_name);
    std::filesystem::create_directories(new_path.parent_path(), ec);
    ec.clear();
    std::filesystem::rename(std::filesystem::path(old_name), new_path, ec);

    result.success = !ec;
    result.exit_code = result.success ? 0 : -1;
    if (result.success) {
        result.modified_files.push_back(new_name);
        result.deleted_files.push_back(old_name);
        result.stdout_text = "Renamed " + old_name + " -> " + new_name;
    } else {
        result.stderr_text = "Rename failed: " + ec.message();
    }
    return result;
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

ExecutionResult ToolExecutor::executeInternal(const ExecutionRequest& request) {
    auto start = std::chrono::high_resolution_clock::now();
    
    auto policy = getPolicy(request.tool_name);
    int timeout = (policy ? policy->timeout_seconds : m_default_timeout);
    
    ExecutionResult result = spawnProcess(request.tool_name,
                                        request.args,
                                        request.working_dir,
                                        timeout);
    
    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    if (m_logFn) {
        m_logFn("[ToolExec] " + request.tool_name + " exit=" + std::to_string(result.exit_code) +
               " duration=" + std::to_string(result.duration_ms) + "ms");
    }
    
    return result;
}

ExecutionResult ToolExecutor::spawnProcess(const std::string& exe, const std::vector<std::string>& args,
                                           const std::string& cwd, int timeout_seconds) {
    ExecutionResult result;

#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE stdout_read = nullptr;
    HANDLE stdout_write = nullptr;
    HANDLE stderr_read = nullptr;
    HANDLE stderr_write = nullptr;

    if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0) ||
        !CreatePipe(&stderr_read, &stderr_write, &sa, 0)) {
        result.stderr_text = "CreatePipe failed";
        return result;
    }

    SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = stdout_write;
    si.hStdError = stderr_write;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi{};

    std::string cmd_line = quoteCommandArg(exe);
    for (const auto& arg : args) {
        cmd_line += " ";
        cmd_line += quoteCommandArg(arg);
    }

    std::vector<char> cmd_buffer(cmd_line.begin(), cmd_line.end());
    cmd_buffer.push_back('\0');

    const char* cwd_ptr = cwd.empty() ? nullptr : cwd.c_str();
    const BOOL created = CreateProcessA(
        nullptr,
        cmd_buffer.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        cwd_ptr,
        &si,
        &pi);

    CloseHandle(stdout_write);
    CloseHandle(stderr_write);

    if (!created) {
        result.stderr_text = "CreateProcess failed with error " + std::to_string(GetLastError());
        CloseHandle(stdout_read);
        CloseHandle(stderr_read);
        return result;
    }

    const DWORD timeout_ms = timeout_seconds <= 0 ? INFINITE : static_cast<DWORD>(timeout_seconds * 1000);
    const DWORD wait_result = WaitForSingleObject(pi.hProcess, timeout_ms);
    if (wait_result == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 124);
        result.exit_code = 124;
        result.stderr_text = "Process timed out";
    }

    std::string stdout_text;
    std::string stderr_text;
    monitorProcess(stdout_read, timeout_seconds * 1000, stdout_text, stderr_text);
    monitorProcess(stderr_read, timeout_seconds * 1000, stderr_text, stderr_text);

    DWORD exit_code = 0;
    if (result.exit_code != 124 && GetExitCodeProcess(pi.hProcess, &exit_code)) {
        result.exit_code = static_cast<int>(exit_code);
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(stdout_read);
    CloseHandle(stderr_read);

    result.stdout_text = stdout_text;
    result.stderr_text = stderr_text;
    result.success = (result.exit_code == 0);

    if (m_outputFn) {
        if (!result.stdout_text.empty()) {
            m_outputFn(result.stdout_text);
        }
        if (!result.stderr_text.empty()) {
            m_outputFn(result.stderr_text);
        }
    }

    return result;
#else
    result.stderr_text = "spawnProcess not implemented on this platform";
    return result;
#endif
}

bool ToolExecutor::monitorProcess(void* process_handle, int timeout_ms, std::string& stdout, std::string& stderr) {
#ifdef _WIN32
    (void)timeout_ms;
    (void)stderr;

    HANDLE handle = static_cast<HANDLE>(process_handle);
    if (!handle) {
        return false;
    }

    constexpr DWORD kBufferSize = 4096;
    char buffer[kBufferSize];
    DWORD bytes_read = 0;
    while (ReadFile(handle, buffer, kBufferSize, &bytes_read, nullptr) && bytes_read > 0) {
        stdout.append(buffer, buffer + bytes_read);
    }
    return true;
#else
    (void)process_handle;
    (void)timeout_ms;
    (void)stdout;
    (void)stderr;
    return false;
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
