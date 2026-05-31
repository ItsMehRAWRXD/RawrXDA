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
    
    // === Priority 2: Editor Context Injection ===
    // Contextual editor state passed from IDE to tools for smart decision-making
    struct EditorContext {
        std::string selected_text;           // Current selection in editor
        size_t cursor_position{0};           // Cursor offset from file start
        std::string file_path;               // Current file path
        std::string file_content;            // Full file content
        size_t selection_start{0};           // Selection range start
        size_t selection_end{0};             // Selection range end
        std::string language_id;             // Language (cpp, python, etc.)
        int line_number{0};                  // Current line number
        int column_number{0};                // Current column number
    };
    
    EditorContext editor_context;            // Optional editor context (not all tools use)
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

// === Priority 3: Progress Tracking for UI Feedback ===
// Allows tools to report progress during long operations
struct ProgressUpdate {
    enum class Stage : uint8_t {
        Starting,      // 0% - tool initialization
        Running,       // Active execution
        OutputComing,  // Buffering output
        Finishing,     // 90-99% - cleanup
        Complete       // 100% - done
    };

    std::string tool_name;
    Stage stage{Stage::Starting};
    int percent_complete{0};              // 0-100
    std::string message;                  // "Running: compiling foo.cpp"
    std::string nested_call;              // Track nested tool invocations
    int nesting_depth{0};                 // 0=root, 1=child, 2=grandchild, etc.
};

// ============================================================================
// Tool Executor
// ============================================================================

class ToolExecutor {
public:
    using ApprovalCallbackFn = std::function<bool(const ExecutionRequest&)>;
    using OutputCallbackFn = std::function<void(const std::string& output)>;
    using ProgressCallbackFn = std::function<void(const ProgressUpdate&)>;  // === Priority 3 ===
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
    void setProgressCallback(ProgressCallbackFn fn) { m_progressFn = fn; }  // === Priority 3 ===
    void setLogFn(LogFn fn) { m_logFn = fn; }
    
    // Policy management
    const ToolPolicy* getPolicy(const std::string& tool) const;
    void setAllToolsReadOnly(bool readonly);
    
private:
    // Safety & validation
    bool validateRequest(const ExecutionRequest& request);
    bool checkApproval(const ExecutionRequest& request);
    bool checkPolicy(const ToolPolicy& policy, const ExecutionRequest& request) const;
    
    // Execution
    ExecutionResult executeInternal(const ExecutionRequest& request);
    ExecutionResult spawnProcess(const std::string& exe, const std::vector<std::string>& args,
                                 const std::string& cwd, int timeout_seconds);
    
    // Process monitoring
    bool monitorProcess(void* process_handle, int timeout_ms, std::string& out, std::string& err);
    
    std::map<std::string, ToolPolicy> m_policies;
    
    ApprovalCallbackFn m_approvalFn;
    OutputCallbackFn m_outputFn;
    ProgressCallbackFn m_progressFn;        // === Priority 3: Progress Updates ===
    LogFn m_logFn;
    
    bool m_all_readonly{false};
    int m_default_timeout{300};  // seconds
};

} // namespace Agentic
