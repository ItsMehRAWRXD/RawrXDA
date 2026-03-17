/**
 * @file agentic_executor.hpp
 * @brief Real agentic execution engine - performs actual tasks, not simulated
 * 
 * This is the core agent loop that:
 * - Decomposes user requests into actionable steps
 * - Executes file operations (create folders, files)
 * - Runs compilers and reports results
 * - Uses function calling to interact with the IDE
 * - Maintains memory across tasks
 * - Self-corrects on failures
 */

#ifndef AGENTIC_EXECUTOR_HPP_INCLUDED
#define AGENTIC_EXECUTOR_HPP_INCLUDED

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <functional>
#include <memory>
#include <chrono>
#include <atomic>
#include <mutex>
#include "enhanced_tool_registry.hpp"
#include <condition_variable>
#include <thread>
#include <queue>

// Forward declarations
class AgenticToolExecutor;

/**
 * @struct ExecutionStep
 * @brief Represents a single step in task execution
 */
struct ExecutionStep {
    int stepNumber = 0;
    std::string action;          // e.g., "create_directory", "write_file", "compile"
    std::string description;
    std::map<std::string, std::string> params;
    std::string successCriteria;
    bool completed = false;
    bool success = false;
    std::string result;
    std::string error;
    double executionTimeMs = 0.0;
};

/**
 * @struct ExecutionResult
 * @brief Complete result of executing a user request
 */
struct ExecutionResult {
    std::string request;
    std::string timestamp;
    std::vector<ExecutionStep> steps;
    int totalSteps = 0;
    int completedSteps = 0;
    int successfulSteps = 0;
    bool overallSuccess = false;
    double totalExecutionTimeMs = 0.0;
    std::string summary;
    std::vector<std::string> generatedFiles;
    std::vector<std::string> modifiedFiles;
};

// ToolDefinition moved to enhanced_tool_registry.hpp for unified definition

/**
 * @struct MemoryEntry
 * @brief Persistent memory entry
 */
struct MemoryEntry {
    std::string key;
    std::string value;
    std::string timestamp;
    int accessCount = 0;
    bool persistent = false;
};

/**
 * @class AgenticExecutor
 * @brief Real agentic execution engine with memory, tools, and self-correction
 */
class AgenticExecutor {
public:
    explicit AgenticExecutor();
    ~AgenticExecutor();

    // Initialization
    void initialize();
    void setToolExecutor(std::shared_ptr<AgenticToolExecutor> executor);

    // Main execution entry point
    ExecutionResult executeUserRequest(const std::string& request);

    // Task decomposition
    std::vector<ExecutionStep> decomposeTask(const std::string& goal);
    bool executeStep(ExecutionStep& step);
    bool verifyStepCompletion(const ExecutionStep& step, const std::string& result);

    // File system operations (real, not simulated)
    bool createDirectory(const std::string& path);
    bool createFile(const std::string& path, const std::string& content);
    bool writeFile(const std::string& path, const std::string& content);
    std::string readFile(const std::string& path);
    bool deleteFile(const std::string& path);
    bool deleteDirectory(const std::string& path);
    std::vector<std::string> listDirectory(const std::string& path);
    bool fileExists(const std::string& path);
    bool directoryExists(const std::string& path);

    // Compiler integration (real compilation)
    struct CompileResult {
        bool success = false;
        std::string output;
        std::string error;
        int exitCode = 0;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;
    };
    CompileResult compileProject(const std::string& projectPath, const std::string& compiler = "g++");
    CompileResult runExecutable(const std::string& executablePath, const std::vector<std::string>& args = {});

    // Function calling system (tool use)
    std::vector<ToolDefinition> getAvailableTools() const;
    std::string callTool(const std::string& toolName, const std::map<std::string, std::string>& params);
    void registerTool(const ToolDefinition& tool);

    // Memory and context
    void addToMemory(const std::string& key, const std::string& value, bool persistent = false);
    std::string getFromMemory(const std::string& key) const;
    bool hasMemory(const std::string& key) const;
    void clearMemory();
    void clearNonPersistentMemory();
    std::string getFullContext() const;
    std::vector<std::string> getMemoryKeys() const;

    // Memory persistence
    void loadMemoryFromDisk(const std::string& path = "");
    void persistMemoryToDisk(const std::string& path = "");
    void enforceMemoryLimit();

    // Self-correction
    bool detectFailure(const std::string& output);
    std::string generateCorrectionPlan(const std::string& failureReason);
    ExecutionStep retryWithCorrection(const ExecutionStep& failedStep);

    // Configuration
    void setWorkingDirectory(const std::string& path);
    std::string getWorkingDirectory() const;
    void setMaxRetries(int retries);
    void setMemoryLimit(size_t bytes);
    void setBlockDestructiveCommands(bool block);

    // Callbacks
    using StepCallback = std::function<void(const std::string&)>;
    using ProgressCallback = std::function<void(int current, int total)>;
    using CompletionCallback = std::function<void(const ExecutionResult&)>;
    using ErrorCallback = std::function<void(const std::string&)>;
    using LogCallback = std::function<void(const std::string&)>;

    void setStepStartedCallback(StepCallback cb) { m_onStepStarted = std::move(cb); }
    void setStepCompletedCallback(std::function<void(const std::string&, bool)> cb) { m_onStepCompleted = std::move(cb); }
    void setProgressCallback(ProgressCallback cb) { m_onProgress = std::move(cb); }
    void setCompletionCallback(CompletionCallback cb) { m_onComplete = std::move(cb); }
    void setErrorCallback(ErrorCallback cb) { m_onError = std::move(cb); }
    void setLogCallback(LogCallback cb) { m_onLog = std::move(cb); }

    // Status
    bool isExecuting() const { return m_isExecuting.load(); }
    void cancelExecution();

private:
    // Internal helpers
    void log(const std::string& message);
    void emitStepStarted(const std::string& description);
    void emitStepCompleted(const std::string& description, bool success);
    void emitProgress(int current, int total);
    void emitComplete(const ExecutionResult& result);
    void emitError(const std::string& error);

    // Task planning using heuristics (without LLM for now)
    std::vector<ExecutionStep> planTaskHeuristic(const std::string& goal);
    std::string extractCodeFromResponse(const std::string& response);
    bool validateGeneratedCode(const std::string& code);
    bool isDestructiveCommand(const std::string& program, const std::vector<std::string>& args) const;

    // Process execution
    struct ProcessResult {
        bool success = false;
        std::string stdout_output;
        std::string stderr_output;
        int exitCode = 0;
        double executionTimeMs = 0.0;
    };
    ProcessResult executeProcess(const std::string& program, const std::vector<std::string>& args, int timeoutMs = 30000);

    // Tool executor
    std::shared_ptr<AgenticToolExecutor> m_toolExecutor;

    // Tools registry
    std::vector<ToolDefinition> m_tools;
    mutable std::mutex m_toolsMutex;

    // Memory
    std::unordered_map<std::string, MemoryEntry> m_memory;
    mutable std::mutex m_memoryMutex;
    size_t m_memoryLimitBytes = 100 * 1024 * 1024; // 100MB default

    // Execution history
    std::vector<ExecutionStep> m_executionHistory;
    std::mutex m_historyMutex;

    // Working directory
    std::string m_workingDirectory;

    // Configuration
    int m_maxRetries = 3;
    bool m_blockDestructiveCommands = true;
    std::atomic<bool> m_isExecuting{false};
    std::atomic<bool> m_cancelRequested{false};

    // Callbacks
    StepCallback m_onStepStarted;
    std::function<void(const std::string&, bool)> m_onStepCompleted;
    ProgressCallback m_onProgress;
    CompletionCallback m_onComplete;
    ErrorCallback m_onError;
    LogCallback m_onLog;

    // Initialize default tools
    void initializeDefaultTools();
};

#endif // AGENTIC_EXECUTOR_HPP_INCLUDED
