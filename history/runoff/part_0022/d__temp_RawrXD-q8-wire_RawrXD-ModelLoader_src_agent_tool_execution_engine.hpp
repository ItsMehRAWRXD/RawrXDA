/**
 * @file tool_execution_engine.hpp
 * @brief High-performance tool execution kernel for RawrXD
 * @note Zero external dependencies - Pure Win32/C++20 for maximum performance
 */

#pragma once

#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <chrono>
#include <optional>
#include <filesystem>

namespace RawrXD {

/**
 * @struct ExecutionResult
 * @brief Detailed output from process execution with Windows-native types
 */
struct ExecutionResult {
    bool success{false};
    std::string stdoutContent;
    std::string stderrContent;
    int exitCode{-1};
    std::chrono::milliseconds duration{0};
    bool timedOut{false};
    std::string errorMessage;
    
    // Structured data (using STL map instead of nlohmann::json)
    std::map<std::string, std::string> metadata;
};

/**
 * @struct FileEdit
 * @brief Represents a single file modification operation
 */
struct FileEdit {
    std::filesystem::path filePath;
    std::string originalText;
    std::string replacementText;
    size_t lineStart{0};
    size_t lineEnd{0};
};

/**
 * @struct ToolInvocation
 * @brief Parameters for invoking a tool
 */
struct ToolInvocation {
    std::string toolId;
    std::map<std::string, std::string> parameters;
    std::string workingDirectory;
    uint32_t timeoutMs{30000}; // 30s default
};

/**
 * @class ToolExecutionEngine
 * @brief Windows-native subprocess execution (pure Win32/C++20)
 * 
 * Features:
 * - CreateProcess with full pipe redirection
 * - Stdout/stderr capture with timeout handling
 * - Basic process tree management helpers
 * - File system helpers (read/write/list/apply edits)
 * - Simple tool registry dispatch (string → handler)
 */
class ToolExecutionEngine {
public:
    ToolExecutionEngine();
    ~ToolExecutionEngine();

    // Core Execution
    ExecutionResult executeCommand(
        const std::string& command,
        const std::vector<std::string>& args,
        const std::string& workingDir = "",
        uint32_t timeoutMs = 30000
    );

    ExecutionResult executePowerShell(
        const std::string& script,
        const std::map<std::string, std::string>& args = {},
        uint32_t timeoutMs = 30000
    );

    ExecutionResult executeBatchFile(
        const std::filesystem::path& batchFile,
        const std::vector<std::string>& args = {},
        uint32_t timeoutMs = 30000
    );

    // Tool Registry
    using ToolHandler = std::function<ExecutionResult(const std::map<std::string, std::string>&)>;
    void registerTool(const std::string& toolId, ToolHandler handler);
    ExecutionResult invokeTool(const ToolInvocation& invocation);

    // Introspection
    std::vector<std::string> listTools() const;

    // File System Operations
    ExecutionResult readFile(const std::filesystem::path& path, size_t startLine = 0, size_t endLine = SIZE_MAX);
    ExecutionResult writeFile(const std::filesystem::path& path, const std::string& content, bool append = false);
    ExecutionResult listDirectory(const std::filesystem::path& path, const std::string& pattern = "*");
    ExecutionResult applyFileEdit(const FileEdit& edit);
    ExecutionResult applyFileEdits(const std::vector<FileEdit>& edits); // Atomic batch

    // Process Management
    void killProcessTree(DWORD processId);
    bool isProcessRunning(DWORD processId);
    std::vector<DWORD> getChildProcesses(DWORD parentId);

    // Configuration
    void setDefaultTimeout(uint32_t timeoutMs) { m_defaultTimeoutMs = timeoutMs; }
    void setWorkingDirectory(const std::filesystem::path& dir) { m_workingDirectory = dir; }
    void setMaxOutputSize(size_t bytes) { m_maxOutputSize = bytes; }

    // Error Analysis
    struct ParsedError {
        std::string file;
        size_t line{0};
        size_t column{0};
        std::string severity;
        std::string message;
        std::string code;
    };
    std::vector<ParsedError> parseCompilerErrors(const std::string& output);

private:
    // Windows Process Spawning
    struct ProcessContext {
        HANDLE hProcess{nullptr};
        HANDLE hThread{nullptr};
        HANDLE hStdoutRead{nullptr};
        HANDLE hStdoutWrite{nullptr};
        HANDLE hStderrRead{nullptr};
        HANDLE hStderrWrite{nullptr};
        DWORD processId{0};
        std::chrono::steady_clock::time_point startTime;
        
        ~ProcessContext();
        void cleanup();
    };

    std::unique_ptr<ProcessContext> createProcess(
        const std::string& commandLine,
        const std::string& workingDir
    );

    ExecutionResult captureOutput(
        std::unique_ptr<ProcessContext> ctx,
        uint32_t timeoutMs
    );

    std::string readPipeAsync(HANDLE hPipe, uint32_t timeoutMs);
    
    // Pipe Management
    bool createPipeNonBlocking(HANDLE* hRead, HANDLE* hWrite);
    
    // Tool Handlers
    ExecutionResult handleReadFile(const std::map<std::string, std::string>& params);
    ExecutionResult handleWriteFile(const std::map<std::string, std::string>& params);
    ExecutionResult handleSearchFiles(const std::map<std::string, std::string>& params);
    ExecutionResult handleGrepSearch(const std::map<std::string, std::string>& params);
    ExecutionResult handleEditFile(const std::map<std::string, std::string>& params);
    ExecutionResult handleListDirectory(const std::map<std::string, std::string>& params);
    ExecutionResult handleGetDiagnostics(const std::map<std::string, std::string>& params);
    ExecutionResult handleRunTerminal(const std::map<std::string, std::string>& params);

    // State
    std::map<std::string, ToolHandler> m_toolRegistry;
    std::filesystem::path m_workingDirectory;
    uint32_t m_defaultTimeoutMs{30000};
    size_t m_maxOutputSize{10 * 1024 * 1024}; // 10MB default
    
    // IOCP Handle for async I/O (reserved for future extensions)
    HANDLE m_iocpHandle{nullptr};
};

} // namespace RawrXD
