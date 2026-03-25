#pragma once

<<<<<<< HEAD
=======

>>>>>>> origin/main
#include <memory>
#include <vector>
#include <string>
#include <map>
<<<<<<< HEAD
#include <mutex>
#include <functional>
#include <filesystem>
#include <nlohmann/json.hpp>
=======
#include <any>
#include "nlohmann/json.hpp"
>>>>>>> origin/main

class AgenticEngine;
class ModelTrainer;

namespace RawrXD {
<<<<<<< HEAD
    class InferenceEngine;
=======
    class CPUInferenceEngine;
    using InferenceEngine = CPUInferenceEngine;
>>>>>>> origin/main
}
using RawrXD::InferenceEngine;

/**
 * @class AgenticExecutor
 * @brief Real agentic execution - not simulated, actually performs tasks.
 *
 * Security: All file-system operations are sandboxed to m_workspaceRoot.
 *           Shell commands use a compiler whitelist and reject metacharacters.
 *           Retry logic is iterative (bounded, not recursive).
 */
class AgenticExecutor {

public:
<<<<<<< HEAD
    using json = nlohmann::json;

    // Callback typedefs
    using StepStartedCB  = std::function<void(const char*, void*)>;
    using StepCompletedCB = std::function<void(const char*, bool, void*)>;
    using LogMessageCB   = std::function<void(const char*, void*)>;
    using ExecCompleteCB = std::function<void(const char*, void*)>;

=======
>>>>>>> origin/main
    explicit AgenticExecutor(void* parent = nullptr);
    ~AgenticExecutor();

    void initialize(AgenticEngine* engine, InferenceEngine* inference);
    void setWorkspaceRoot(const std::filesystem::path& root);

    // Main agentic execution entry point
<<<<<<< HEAD
    json executeUserRequest(const std::string& request);

    // Core agentic capabilities
    json decomposeTask(const std::string& goal);
    bool executeStep(const json& step);
    bool verifyStepCompletion(const json& step, const std::string& result);

    // File system operations (real, sandboxed to workspace root)
=======
    nlohmann::json executeUserRequest(const std::string& request);

    // Core agentic capabilities
    nlohmann::json decomposeTask(const std::string& goal);
    bool executeStep(const nlohmann::json& step);
    bool verifyStepCompletion(const nlohmann::json& step, const std::string& result);

    // File system operations (real, not simulated)
>>>>>>> origin/main
    bool createDirectory(const std::string& path);
    bool createFile(const std::string& path, const std::string& content);
    bool writeFile(const std::string& path, const std::string& content);
    std::string readFile(const std::string& path);
    bool deleteFile(const std::string& path);
    bool deleteDirectory(const std::string& path);
    std::vector<std::string> listDirectory(const std::string& path);

<<<<<<< HEAD
    // Compiler integration (real compilation, whitelisted compilers only)
    json compileProject(const std::string& projectPath, const std::string& compiler = "g++");
    json runExecutable(const std::string& executablePath, const std::vector<std::string>& args = {});

    // Function calling system (tool use)
    json getAvailableTools();
    json callTool(const std::string& toolName, const json& params);

    // Model training capabilities
    json trainModel(const std::string& datasetPath, const std::string& modelPath, const json& config);
    bool isTrainingModel() const;

    // Memory and context
    void addToMemory(const std::string& key, const std::string& value);
    std::string getFromMemory(const std::string& key);
    void clearMemory();
    std::string getFullContext();
    void removeMemoryItem(const std::string& key);

    // Self-correction (iterative, bounded)
    bool detectFailure(const std::string& output);
    std::string generateCorrectionPlan(const std::string& failureReason);
    json retryWithCorrection(const json& failedStep);

    // Logging
    void logMessage(const std::string& msg);
    void errorOccurred(const std::string& msg);

    // Callbacks
    StepStartedCB  m_onStepStarted;
    StepCompletedCB m_onStepCompleted;
    LogMessageCB   m_onLogMessage;
    ExecCompleteCB m_onExecutionComplete;
    void*          m_callbackContext = nullptr;
=======
    // Compiler integration (real compilation)
    nlohmann::json compileProject(const std::string& projectPath, const std::string& compiler = "g++");
    nlohmann::json runExecutable(const std::string& executablePath, const std::vector<std::string>& args = std::vector<std::string>());

    // Function calling system (tool use)
    nlohmann::json getAvailableTools();
    nlohmann::json callTool(const std::string& toolName, const nlohmann::json& params);

    // Model training capabilities
    nlohmann::json trainModel(const std::string& datasetPath, const std::string& modelPath, const nlohmann::json& config);
    bool isTrainingModel() const;

    // Memory and context
    void addToMemory(const std::string& key, const std::any& value);
    std::any getFromMemory(const std::string& key);
    void clearMemory();
    std::string getFullContext();

    // Self-correction
    bool detectFailure(const std::string& output);
    std::string generateCorrectionPlan(const std::string& failureReason);
    nlohmann::json retryWithCorrection(const nlohmann::json& failedStep);


    void stepStarted(const std::string& description);
    void stepCompleted(const std::string& description, bool success);
    void taskProgress(int current, int total);
    void executionComplete(const nlohmann::json& result);
    void errorOccurred(const std::string& error);
    void logMessage(const std::string& message);
    void trainingProgress(int epoch, int totalEpochs, float loss, float perplexity);
    void trainingCompleted(const std::string& modelPath, float finalPerplexity);
>>>>>>> origin/main

private:
    // Path safety — all FS ops go through these
    bool isPathSafe(const std::filesystem::path& p) const;
    std::filesystem::path safePath(const std::string& userPath) const;

    // Shell safety
    static bool hasShellMetachars(const std::string& s);
    static bool isCompilerAllowed(const std::string& compiler);

    // Agent reasoning using model
    std::string planNextAction(const std::string& currentState, const std::string& goal);
<<<<<<< HEAD
    json generateCode(const std::string& specification);
=======
    nlohmann::json generateCode(const std::string& specification);
>>>>>>> origin/main
    std::string analyzeError(const std::string& errorOutput);
    std::string improveCode(const std::string& code, const std::string& issue);

    // Internal helpers
<<<<<<< HEAD
    std::string buildToolCallPrompt(const std::string& goal, const std::string& toolsJson);
    std::string extractCodeFromResponse(const std::string& response);
    bool validateGeneratedCode(const std::string& code);

    // Memory persistence
    void loadMemorySettings();
    void loadMemoryFromDisk();
    void persistMemoryToDisk();
    void enforceMemoryLimit();
=======
    nlohmann::json buildToolCallPrompt(const std::string& goal, const nlohmann::json& tools);
    std::string extractCodeFromResponse(const std::string& response);
    bool validateGeneratedCode(const std::string& code);
>>>>>>> origin/main

    AgenticEngine* m_agenticEngine = nullptr;
    InferenceEngine* m_inferenceEngine = nullptr;
    std::unique_ptr<ModelTrainer> m_modelTrainer;
<<<<<<< HEAD

    std::filesystem::path m_workspaceRoot;
    std::map<std::string, std::string> m_memory;
    mutable std::mutex m_memoryMutex;
    std::string m_currentWorkingDirectory;
    json m_executionHistory;

    bool m_memoryEnabled = false;
    int64_t m_memoryLimitBytes = 10 * 1024 * 1024; // 10 MB default

    static constexpr int m_maxRetries = 3;
=======
    
    std::map<std::string, std::any> m_memory;
    nlohmann::json m_executionHistory;
    std::string m_currentWorkingDirectory;
    
    int m_maxRetries = 3;
    int m_currentRetryCount = 0;
>>>>>>> origin/main
};

