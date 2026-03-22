// agentic_tool_executor.cpp
// Safe system tool execution with approval gates and policy enforcement

#include "agentic_tool_executor.hpp"
#include "observability/Logger.hpp"
#include <chrono>
#include <algorithm>
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
    
    // Build command line
    std::string cmd_line = exe;
    for (const auto& arg : args) {
        cmd_line += " \"" + arg + "\"";
    }
    
    // For now, return success stub
    // In production: use CreateProcess (Win32) or fork/exec (Unix)
    result.success = true;
    result.exit_code = 0;
    result.stdout_text = "[Simulated output from " + exe + "]";
    result.duration_ms = 100;
    
    if (m_outputFn) {
        m_outputFn(result.stdout_text);
    }
    
    return result;
}

bool ToolExecutor::monitorProcess(void* process_handle, int timeout_ms, std::string& stdout, std::string& stderr) {
    // Stub implementation
    // In production: implement actual process monitoring
    return true;
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
