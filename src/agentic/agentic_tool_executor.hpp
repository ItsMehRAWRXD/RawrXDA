// agentic_tool_executor.hpp
// Safe execution of system tools: compile, test, build, format, refactor

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>

namespace Agentic {

// ============================================================================
// Tool Catalog & Safety Policies
// ============================================================================

enum class ToolCategory : uint8_t {
    Build,        // cmake, make, ninja, msbuild
    Compile,      // cl.exe, gcc, clang
    Test,         // ctest, gtest, unittest
    Format,       // clang-format, prettier
    Lint,         // clang-tidy, cpplint
    VCS,          // git operations
    FileOp,       // copy, delete, rename
    Analysis,     // compilation analysis, header analysis
    Custom        // User-defined tools
};

struct ToolPolicy {
    std::string tool_name;
    ToolCategory category;
    
    bool enabled{true};
    bool requires_approval{false};           // Always ask before running
    bool read_only{false};                   // No file modifications
    
    std::vector<std::string> allowed_args;   // Whitelist of allowed arguments
    bool allow_any_args{false};              // If true, ignore allowed_args
    std::vector<std::string> fallback_tools; // Ordered fallback chain (e.g. ninja, make)
    
    int timeout_seconds{300};
    size_t max_output_bytes{100 * 1024 * 1024};  // 100MB
    
    ToolPolicy() : category(ToolCategory::Custom) {}
};

struct ExecutionRequest {
    std::string tool_name;
    std::vector<std::string> args;
    std::string working_dir;
    std::map<std::string, std::string> env_vars;  // Additional env vars
    
    bool requires_approval{false};
    std::string description;  // Human-readable description
};

struct ExecutionResult {
    bool success{false};
    int exit_code{-1};
    std::string stdout_text;
    std::string stderr_text;
    
    int duration_ms{0};
    size_t peak_memory_bytes{0};
    
    // For analysis
    std::vector<std::string> generated_files;
    std::vector<std::string> modified_files;
    std::vector<std::string> deleted_files;
};

// ============================================================================
// Tool Executor
// ============================================================================

class ToolExecutor {
public:
    using ApprovalCallbackFn = std::function<bool(const ExecutionRequest&)>;
    using OutputCallbackFn = std::function<void(const std::string& output)>;
    using LogFn = std::function<void(const std::string& entry)>;
    
    ToolExecutor();
    ~ToolExecutor();
    
    // Register tool with policy
    void registerTool(const ToolPolicy& policy);
    void unregisterTool(const std::string& tool_name);
    
    // Execute tool with safety checks
    ExecutionResult execute(const ExecutionRequest& request);
    
    // Batch execution (sequential)
    std::vector<ExecutionResult> executeBatch(const std::vector<ExecutionRequest>& requests);
    
    // Common tool wrappers (convenience methods)
    ExecutionResult executeCompile(const std::vector<std::string>& source_files,
                                  const std::string& output_file,
                                  const std::vector<std::string>& flags = {});
    
    ExecutionResult executeTests(const std::vector<std::string>& test_targets);
    ExecutionResult executeBuild(const std::string& build_dir = ".");
    ExecutionResult executeFormat(const std::vector<std::string>& files);
    ExecutionResult executeLint(const std::vector<std::string>& files);
    ExecutionResult executeGit(const std::string& command, const std::vector<std::string>& args);
    
    // File operations
    ExecutionResult copyFiles(const std::vector<std::pair<std::string, std::string>>& src_dst);
    ExecutionResult deleteFiles(const std::vector<std::string>& files);
    ExecutionResult renameFile(const std::string& old_name, const std::string& new_name);
    
    // Callbacks
    void setApprovalCallback(ApprovalCallbackFn fn) { m_approvalFn = fn; }
    void setOutputCallback(OutputCallbackFn fn) { m_outputFn = fn; }
    void setLogFn(LogFn fn) { m_logFn = fn; }
    
    // Policy management
    const ToolPolicy* getPolicy(const std::string& tool) const;
    void setAllToolsReadOnly(bool readonly);

    // Tool call validation and retries
    enum class ToolFailureType : uint8_t {
        None = 0,
        Transient,
        Permanent,
        Timeout,
        PermissionDenied,
        Unknown
    };

    bool validateToolCallSchema(const ExecutionRequest& request, std::string& error) const;
    ToolFailureType classifyFailure(const ExecutionResult& result) const;
    ExecutionResult executeWithRetry(const ExecutionRequest& request, int maxRetries = 3);

private:
    // Safety & validation
    bool validateRequest(const ExecutionRequest& request);
    bool checkApproval(const ExecutionRequest& request);
    bool checkPolicy(const ToolPolicy& policy, const ExecutionRequest& request) const;
    
    // Execution
    ExecutionResult executeInternal(const ExecutionRequest& request);
    ExecutionResult spawnProcess(const std::string& exe, const std::vector<std::string>& args,
                                 const std::string& cwd, int timeout_seconds, size_t max_output_bytes);
    
    // Process monitoring
    bool monitorProcess(void* process_handle, int timeout_ms, std::string& stdout, std::string& stderr,
                        size_t max_output_bytes, size_t* peak_memory_bytes);
    
    std::map<std::string, ToolPolicy> m_policies;
    
    ApprovalCallbackFn m_approvalFn;
    OutputCallbackFn m_outputFn;
    LogFn m_logFn;
    
    bool m_all_readonly{false};
    int m_default_timeout{300};  // seconds
};

} // namespace Agentic
