// ============================================================================
// PublicToolRegistry.h
// 
// Purpose: Production-ready C++ API for accessing RawrXD tools
//          WITHOUT reliance on hotpatching layers
//
// Features:
// - Non-hotpatch direct tool access
// - Thread-safe operations
// - Comprehensive error handling
// - Full audit logging
// - IPC integration ready
//
// License: Production Grade - Enterprise Ready
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <cstdint>

namespace RawrXD {

// ============================================================================
// Tool Result Types
// ============================================================================

enum class ToolResultStatus : uint8_t {
    Success = 0,
    Failed = 1,
    ValidationFailed = 2,
    SandboxBlocked = 3,
    TimeoutExceeded = 4,
    NotFound = 5,
    PermissionDenied = 6,
    NetworkError = 7,
    ParseError = 8
};

struct ToolResult {
    ToolResultStatus status;
    std::string output;
    int exit_code = 0;
    std::string error_message;
    uint64_t execution_time_ms = 0;
    std::map<std::string, std::string> metadata;
    
    bool success() const { return status == ToolResultStatus::Success; }
    
    static ToolResult Success(const std::string& output = "", 
                            const std::map<std::string, std::string>& meta = {}) {
        ToolResult r;
        r.status = ToolResultStatus::Success;
        r.output = output;
        r.metadata = meta;
        return r;
    }
    
    static ToolResult Error(ToolResultStatus st, const std::string& error) {
        ToolResult r;
        r.status = st;
        r.error_message = error;
        return r;
    }
};

// ============================================================================
// File Operations (Non-Hotpatch)
// ============================================================================

struct FileReadOptions {
    int start_line = 1;
    int end_line = -1;  // -1 = to end
    bool as_binary = false;
    size_t max_bytes = 10 * 1024 * 1024;  // 10MB default limit
};

struct FileWriteOptions {
    bool create_if_missing = true;
    bool overwrite = false;
    std::string backup_suffix = ".bak";
    bool create_backup = true;
};

// ============================================================================
// Build & Compilation
// ============================================================================

struct BuildOptions {
    std::string target = "all";
    std::string configuration = "Release";
    int parallel_jobs = 4;
    bool verbose = false;
    std::vector<std::string> cmake_args;
};

// ============================================================================
// Code Analysis
// ============================================================================

struct CodeSearchOptions {
    std::string query;
    std::string file_pattern = "*.*";
    bool is_regex = false;
    bool case_sensitive = false;
    int max_results = 100;
};

struct CodeSearchResult {
    std::string file_path;
    int line_number = 0;
    std::string line_content;
    int column = 0;
};

// ============================================================================
// Public Tool Registry (Main API)
// ============================================================================

/**
 * @class PublicToolRegistry
 * @brief Non-hotpatch production-ready tool API
 *
 * Access to RawrXD tools via stable C++ interface
 * Thread-safe, full error handling, audit logging
 */
class PublicToolRegistry {
public:
    // Singleton access
    static PublicToolRegistry& Get();
    
    // ---- File Operations ----
    
    /** Read file contents with optional line range
     * @param path File path
     * @param options Read configuration
     * @return ToolResult with file contents or error
     */
    ToolResult ReadFile(const std::string& path, const FileReadOptions& options = {});
    
    /** Write file contents
     * @param path File path
     * @param content Content to write
     * @param options Write configuration
     * @return ToolResult with status
     */
    ToolResult WriteFile(const std::string& path, const std::string& content, 
                        const FileWriteOptions& options = {});
    
    /** Replace text in file
     * @param path File path
     * @param old_string Text to find
     * @param new_string Text to replace with
     * @return ToolResult with replacement count
     */
    ToolResult ReplaceInFile(const std::string& path, const std::string& old_string,
                            const std::string& new_string);
    
    /** Delete file or directory
     * @param path Path to delete
     * @param recursive For directories, recurse
     * @return ToolResult with status
     */
    ToolResult DeletePath(const std::string& path, bool recursive = false);
    
    /** List directory contents
     * @param path Directory path
     * @param recursive Recurse into subdirectories
     * @param pattern File pattern filter
     * @return ToolResult with file list
     */
    ToolResult ListDirectory(const std::string& path, bool recursive = false,
                            const std::string& pattern = "*");
    
    // ---- Code Search & Analysis ----
    
    /** Search for text in code
     * @param options Search configuration
     * @return ToolResult with search results
     */
    ToolResult SearchCode(const CodeSearchOptions& options);
    
    /** Get code diagnostics (linter, compiler errors)
     * @param file_path Specific file, or empty for all
     * @return ToolResult with diagnostics list
     */
    ToolResult GetDiagnostics(const std::string& file_path = "");
    
    /** Get code coverage for file/function
     * @param file_path Source file
     * @param function_name Optional specific function
     * @return ToolResult with coverage data
     */
    ToolResult GetCoverage(const std::string& file_path, 
                          const std::string& function_name = "");
    
    // ---- Build & Compilation ----
    
    /** Run build system command
     * @param options Build configuration
     * @return ToolResult with build output
     */
    ToolResult BuildProject(const BuildOptions& options = {});
    
    /** Run specific CMake target
     * @param target Target name
     * @param config Build configuration
     * @return ToolResult with build output
     */
    ToolResult BuildTarget(const std::string& target, const std::string& config = "Release");
    
    // ---- System & Execution ----
    
    /** Execute arbitrary shell command
     * @param command Command to execute
     * @param timeout_ms Maximum execution time
     * @return ToolResult with command output
     */
    ToolResult ExecuteCommand(const std::string& command, uint64_t timeout_ms = 30000);
    
    /** Execute PowerShell command
     * @param script PowerShell script code
     * @param timeout_ms Maximum execution time
     * @return ToolResult with script output
     */
    ToolResult ExecutePowerShell(const std::string& script, uint64_t timeout_ms = 30000);
    
    // ---- Native Tools ----
    
    /** Run dumpbin PE analyzer
     * @param binary_path Path to binary
     * @param mode Dumpbin mode (headers, imports, exports, etc.)
     * @return ToolResult with dumpbin output
     */
    ToolResult RunDumpbin(const std::string& binary_path, const std::string& mode = "headers");
    
    /** Run CodeX code analyzer
     * @param source_path Path to source file
     * @return ToolResult with analysis
     */
    ToolResult RunCodex(const std::string& source_path);
    
    /** Run compiler analysis
     * @param source_path Path to source file
     * @return ToolResult with compiler analysis
     */
    ToolResult RunCompiler(const std::string& source_path);
    
    // ---- Advanced Operations ----
    
    /** Hotpatch system layer (memory, byte-level, or server)
     * @param layer Patch layer: "memory", "byte", or "server"
     * @param target Target address/file/endpoint
     * @param data Patch payload (hex-encoded for memory/byte)
     * @return ToolResult with patch status
     */
    ToolResult ApplyHotpatch(const std::string& layer, const std::string& target,
                            const std::string& data);
    
    /** Disk recovery operations
     * @param action Action: scan, init, extract_key, run, abort, stats, cleanup
     * @param drive Optional physical drive number for init
     * @return ToolResult with recovery output
     */
    ToolResult DiskRecovery(const std::string& action, int drive = -1);

    /** Execute any registered tool by name (non-hotpatch direct dispatch)
     * @param tool_name Registered tool name
     * @param args_json JSON object string with tool arguments
     * @return ToolResult with tool output or error
     */
    ToolResult ExecuteRegisteredTool(const std::string& tool_name,
                                     const std::string& args_json = "{}");
    
    // ---- Configuration & State ----
    
    /** List all available tools
     * @return Vector of tool names
     */
    std::vector<std::string> ListAvailableTools() const;
    
    /** Get tool documentation for specific tool
     * @param tool_name Tool name
     * @return JSON schema for tool
     */
    std::string GetToolSchema(const std::string& tool_name);
    
    /** Enable/disable tool
     * @param tool_name Tool name
     * @param enabled Whether to enable
     */
    void SetToolEnabled(const std::string& tool_name, bool enabled);
    
    /** Check if tool is enabled
     * @param tool_name Tool name
     * @return True if enabled
     */
    bool IsToolEnabled(const std::string& tool_name) const;
    
    // ---- Statistics & Monitoring ----
    
    struct ToolStatistics {
        std::string name;
        uint64_t invocation_count = 0;
        uint64_t success_count = 0;
        uint64_t error_count = 0;
        double average_execution_ms = 0.0;
        uint64_t total_bytes_processed = 0;
        std::string last_error;
    };
    
    /** Get statistics for a tool
     * @param tool_name Tool name
     * @return Tool statistics
     */
    ToolStatistics GetToolStatistics(const std::string& tool_name) const;
    
    /** Get all tool statistics
     * @return Map of tool names to statistics
     */
    std::map<std::string, ToolStatistics> GetAllStatistics() const;
    
    // ---- Callbacks & Hooks ----
    
    using AuditCallback = std::function<void(const std::string& tool_name, 
                                            const std::string& operation,
                                            const ToolResult& result)>;
    
    /** Register audit callback for all tool operations
     * @param callback Callback to invoke on each operation
     */
    void SetAuditCallback(AuditCallback callback);
    
    using PermissionCallback = std::function<bool(const std::string& tool_name,
                                                  const std::string& operation)>;
    
    /** Register permission callback for tool access control
     * @param callback Callback to check permissions
     */
    void SetPermissionCallback(PermissionCallback callback);
    
private:
    PublicToolRegistry();
    ~PublicToolRegistry();
    PublicToolRegistry(const PublicToolRegistry&) = delete;
    PublicToolRegistry& operator=(const PublicToolRegistry&) = delete;
    
    // Implementation details (pImpl pattern)
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

// ============================================================================
// Convenience Functions (free functions)
// ============================================================================

/** Global convenience: read file
 * @param path File path
 * @param start_line Optional start line
 * @param end_line Optional end line
 * @return ToolResult with file contents
 */
inline ToolResult PublicReadFile(const std::string& path, int start_line = 1, int end_line = -1) {
    FileReadOptions opts;
    opts.start_line = start_line;
    opts.end_line = end_line;
    return PublicToolRegistry::Get().ReadFile(path, opts);
}

/** Global convenience: write file
 * @param path File path
 * @param content Content to write
 * @return ToolResult with status
 */
inline ToolResult PublicWriteFile(const std::string& path, const std::string& content) {
    return PublicToolRegistry::Get().WriteFile(path, content);
}

/** Global convenience: replace in file
 * @param path File path
 * @param old_str Text to find
 * @param new_str Text to replace with
 * @return ToolResult with status
 */
inline ToolResult PublicReplaceInFile(const std::string& path, 
                                     const std::string& old_str, 
                                     const std::string& new_str) {
    return PublicToolRegistry::Get().ReplaceInFile(path, old_str, new_str);
}

/** Global convenience: search code
 * @param query Search pattern
 * @param pattern File pattern (default: *.*)
 * @param is_regex Use regex matching
 * @return ToolResult with search results
 */
inline ToolResult PublicSearchCode(const std::string& query, 
                                  const std::string& pattern = "*.*",
                                  bool is_regex = false) {
    CodeSearchOptions opts;
    opts.query = query;
    opts.file_pattern = pattern;
    opts.is_regex = is_regex;
    return PublicToolRegistry::Get().SearchCode(opts);
}

} // namespace RawrXD
