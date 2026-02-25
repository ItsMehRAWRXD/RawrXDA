#pragma once

#include <memory>
#include <vector>
#include <string>
#include <map>
#include <functional>

class AgenticEngine;

namespace RawrXD {
    class InferenceEngine;
}
using RawrXD::InferenceEngine;

/**
 * @class AgenticExecutor
 * @brief Real agentic execution - not simulated, actually performs tasks
 */
class AgenticExecutor {

public:
    // Callback typedefs
    using StepStartedCB  = std::function<void(const char*, void*)>;
    using StepCompletedCB = std::function<void(const char*, bool, void*)>;
    using LogMessageCB   = std::function<void(const char*, void*)>;
    using ExecCompleteCB = std::function<void(const char*, void*)>;

    AgenticExecutor();
    ~AgenticExecutor();

    void initialize(AgenticEngine* engine, InferenceEngine* inference);

    // Main agentic execution entry point
    std::string executeUserRequest(const std::string& request);

    // Core agentic capabilities
    std::string decomposeTask(const std::string& goal);
    bool executeStep(const std::string& stepJson);
    bool verifyStepCompletion(const std::string& stepJson, const std::string& result);

    // File system operations (real, not simulated)
    bool createDirectory(const std::string& path);
    bool createFile(const std::string& path, const std::string& content);
    bool writeFile(const std::string& path, const std::string& content);
    std::string readFile(const std::string& path);
    bool deleteFile(const std::string& path);
    bool deleteDirectory(const std::string& path);
    std::vector<std::string> listDirectory(const std::string& path);

    // Compiler integration (real compilation)
    std::string compileProject(const std::string& projectPath, const std::string& compiler = "g++");
    std::string runExecutable(const std::string& executablePath, const std::vector<std::string>& args = std::vector<std::string>());

    // Function calling system (tool use)
    std::string getAvailableTools();
    std::string callTool(const std::string& toolName, const std::string& paramsJson);

    // Model training capabilities
    std::string trainModel(const std::string& datasetPath, const std::string& modelPath, const std::string& configJson);
    bool isTrainingModel() const;

    // Memory and context
    void addToMemory(const std::string& key, const std::string& valueJson);
    std::string getFromMemory(const std::string& key);
    void clearMemory();
    std::string getFullContext();
    void removeMemoryItem(const std::string& key);

    // Self-correction
    bool detectFailure(const std::string& output);
    std::string generateCorrectionPlan(const std::string& failureReason);
    std::string retryWithCorrection(const std::string& failedStepJson);

    // Callbacks
    StepStartedCB  m_onStepStarted;
    StepCompletedCB m_onStepCompleted;
    LogMessageCB   m_onLogMessage;
    ExecCompleteCB m_onExecutionComplete;
    void*          m_callbackContext = nullptr;

private:
    // Agent reasoning using model
    std::string planNextAction(const std::string& currentState, const std::string& goal);
    std::string generateCode(const std::string& specification);
    std::string analyzeError(const std::string& errorOutput);
    std::string improveCode(const std::string& code, const std::string& issue);

    // Internal helpers
    std::string buildToolCallPrompt(const std::string& goal, const std::string& toolsJson);
    std::string extractCodeFromResponse(const std::string& response);
    bool validateGeneratedCode(const std::string& code);

    // Memory persistence
    void loadMemorySettings();
    void loadMemoryFromDisk();
    void persistMemoryToDisk();
    void enforceMemoryLimit();

    AgenticEngine* m_agenticEngine = nullptr;
    InferenceEngine* m_inferenceEngine = nullptr;

    std::map<std::string, std::string> m_memory;
    std::string m_currentWorkingDirectory;

    bool m_memoryEnabled = false;
    int64_t m_memoryLimitBytes = 10 * 1024 * 1024; // 10 MB default

    int m_maxRetries = 3;
    int m_currentRetryCount = 0;
};

