<<<<<<< HEAD
#pragma once

// C++20, no Qt. Agentic execution: callbacks replace signals; JSON as std::string.

#include <string>
#include <vector>
#include <map>
#include <cstdint>

class AgenticEngine;
class InferenceEngine;
class SettingsManager;
class MemorySpaceManager;
/**
 * AgenticExecutor — real agentic execution (file ops, compilers, tool use, training).
 * Raw function-pointer callbacks replace Qt signals. No std::function.
 */
class AgenticExecutor {
public:
    // Raw function-pointer callback types — void* ctx for user data
    using StepStartedFn       = void(*)(const char* step, void* ctx);
    using StepCompletedFn     = void(*)(const char* step, bool success, void* ctx);
    using TaskProgressFn      = void(*)(int current, int total, void* ctx);
    using ExecutionCompleteFn = void(*)(const char* resultJson, void* ctx);
    using ErrorOccurredFn     = void(*)(const char* error, void* ctx);
    using LogMessageFn        = void(*)(const char* message, void* ctx);
    using TrainingProgressFn  = void(*)(int epoch, int totalEpochs, float loss, float perplexity, void* ctx);
    using TrainingCompletedFn = void(*)(const char* modelPath, float finalPerplexity, void* ctx);

    AgenticExecutor();
    ~AgenticExecutor();

    void initialize(AgenticEngine* engine, InferenceEngine* inference);

    void setOnStepStarted(StepStartedFn f)      { m_onStepStarted = f; }
    void setOnStepCompleted(StepCompletedFn f) { m_onStepCompleted = f; }
    void setOnTaskProgress(TaskProgressFn f)    { m_onTaskProgress = f; }
    void setOnExecutionComplete(ExecutionCompleteFn f) { m_onExecutionComplete = f; }
    void setOnErrorOccurred(ErrorOccurredFn f)  { m_onErrorOccurred = f; }
    void setOnLogMessage(LogMessageFn f)       { m_onLogMessage = f; }
    void setOnTrainingProgress(TrainingProgressFn f) { m_onTrainingProgress = f; }
    void setOnTrainingCompleted(TrainingCompletedFn f) { m_onTrainingCompleted = f; }
    void setCallbackContext(void* ctx)          { m_callbackContext = ctx; }

    /** Execute user request; returns result as JSON string. */
    std::string executeUserRequest(const std::string& request);

    std::string decomposeTask(const std::string& goal);  // JSON array string
    bool executeStep(const std::string& stepJson);
    bool verifyStepCompletion(const std::string& stepJson, const std::string& result);

    void setCurrentWorkingDirectory(const std::string& path) { m_currentWorkingDirectory = path; }
    std::string getCurrentWorkingDirectory() const { return m_currentWorkingDirectory; }

    bool createDirectory(const std::string& path);
    bool createFile(const std::string& path, const std::string& content);
    bool writeFile(const std::string& path, const std::string& content);
    std::string readFile(const std::string& path);
    bool deleteFile(const std::string& path);
    bool deleteDirectory(const std::string& path);
    std::vector<std::string> listDirectory(const std::string& path);

    std::string compileProject(const std::string& projectPath, const std::string& compiler = "g++");  // JSON
    std::string runExecutable(const std::string& executablePath, const std::vector<std::string>& args = {});  // JSON

    std::string getAvailableTools();  // JSON array
    std::string callTool(const std::string& toolName, const std::string& paramsJson);

    std::string trainModel(const std::string& datasetPath, const std::string& modelPath, const std::string& configJson);
    bool isTrainingModel() const;

    void addToMemory(const std::string& key, const std::string& valueJson);
    std::string getFromMemory(const std::string& key);
    void clearMemory();
    std::string getFullContext();
    void removeMemoryItem(const std::string& key);

    bool detectFailure(const std::string& output);
    std::string generateCorrectionPlan(const std::string& failureReason);
    std::string retryWithCorrection(const std::string& failedStepJson);

private:
    std::string planNextAction(const std::string& currentState, const std::string& goal);
    std::string generateCode(const std::string& specification);
    std::string analyzeError(const std::string& errorOutput);
    std::string improveCode(const std::string& code, const std::string& issue);

    void loadMemorySettings();
    void loadMemoryFromDisk();
    void persistMemoryToDisk();
    void enforceMemoryLimit();

    std::string buildToolCallPrompt(const std::string& goal, const std::string& toolsJson);
    std::string extractCodeFromResponse(const std::string& response);
    bool validateGeneratedCode(const std::string& code);

    AgenticEngine* m_agenticEngine = nullptr;
    InferenceEngine* m_inferenceEngine = nullptr;
    void* m_modelTrainer = nullptr;  // Opaque; ModelTrainer is Qt-based, not used in Qt-free Win32 build
    SettingsManager* m_settingsManager = nullptr;

    std::map<std::string, std::string> m_memory;
    std::string m_executionHistoryJson;  // JSON array
    std::string m_currentWorkingDirectory;

    bool m_memoryEnabled = false;
    int64_t m_memoryLimitBytes = 134217728;
    int m_maxRetries = 3;
    int m_currentRetryCount = 0;

    StepStartedFn      m_onStepStarted      = nullptr;
    StepCompletedFn    m_onStepCompleted    = nullptr;
    TaskProgressFn     m_onTaskProgress     = nullptr;
    ExecutionCompleteFn m_onExecutionComplete = nullptr;
    ErrorOccurredFn    m_onErrorOccurred    = nullptr;
    LogMessageFn       m_onLogMessage       = nullptr;
    TrainingProgressFn m_onTrainingProgress = nullptr;
    TrainingCompletedFn m_onTrainingCompleted = nullptr;
    void*              m_callbackContext    = nullptr;
};
=======
#pragma once

// C++20, no Qt. Agentic execution: callbacks replace signals; JSON as std::string.

#include <string>
#include <vector>
#include <map>
#include <cstdint>

class AgenticEngine;
class InferenceEngine;
class SettingsManager;
class MemorySpaceManager;
/**
 * AgenticExecutor — real agentic execution (file ops, compilers, tool use, training).
 * Raw function-pointer callbacks replace Qt signals. No std::function.
 */
class AgenticExecutor {
public:
    // Raw function-pointer callback types — void* ctx for user data
    using StepStartedFn       = void(*)(const char* step, void* ctx);
    using StepCompletedFn     = void(*)(const char* step, bool success, void* ctx);
    using TaskProgressFn      = void(*)(int current, int total, void* ctx);
    using ExecutionCompleteFn = void(*)(const char* resultJson, void* ctx);
    using ErrorOccurredFn     = void(*)(const char* error, void* ctx);
    using LogMessageFn        = void(*)(const char* message, void* ctx);
    using TrainingProgressFn  = void(*)(int epoch, int totalEpochs, float loss, float perplexity, void* ctx);
    using TrainingCompletedFn = void(*)(const char* modelPath, float finalPerplexity, void* ctx);

    AgenticExecutor();
    ~AgenticExecutor();

    void initialize(AgenticEngine* engine, InferenceEngine* inference);

    void setOnStepStarted(StepStartedFn f)      { m_onStepStarted = f; }
    void setOnStepCompleted(StepCompletedFn f) { m_onStepCompleted = f; }
    void setOnTaskProgress(TaskProgressFn f)    { m_onTaskProgress = f; }
    void setOnExecutionComplete(ExecutionCompleteFn f) { m_onExecutionComplete = f; }
    void setOnErrorOccurred(ErrorOccurredFn f)  { m_onErrorOccurred = f; }
    void setOnLogMessage(LogMessageFn f)       { m_onLogMessage = f; }
    void setOnTrainingProgress(TrainingProgressFn f) { m_onTrainingProgress = f; }
    void setOnTrainingCompleted(TrainingCompletedFn f) { m_onTrainingCompleted = f; }
    void setCallbackContext(void* ctx)          { m_callbackContext = ctx; }

    /** Execute user request; returns result as JSON string. */
    std::string executeUserRequest(const std::string& request);

    std::string decomposeTask(const std::string& goal);  // JSON array string
    bool executeStep(const std::string& stepJson);
    bool verifyStepCompletion(const std::string& stepJson, const std::string& result);

    void setCurrentWorkingDirectory(const std::string& path) { m_currentWorkingDirectory = path; }
    std::string getCurrentWorkingDirectory() const { return m_currentWorkingDirectory; }

    bool createDirectory(const std::string& path);
    bool createFile(const std::string& path, const std::string& content);
    bool writeFile(const std::string& path, const std::string& content);
    std::string readFile(const std::string& path);
    bool deleteFile(const std::string& path);
    bool deleteDirectory(const std::string& path);
    std::vector<std::string> listDirectory(const std::string& path);

    std::string compileProject(const std::string& projectPath, const std::string& compiler = "g++");  // JSON
    std::string runExecutable(const std::string& executablePath, const std::vector<std::string>& args = {});  // JSON

    std::string getAvailableTools();  // JSON array
    std::string callTool(const std::string& toolName, const std::string& paramsJson);

    std::string trainModel(const std::string& datasetPath, const std::string& modelPath, const std::string& configJson);
    bool isTrainingModel() const;

    void addToMemory(const std::string& key, const std::string& valueJson);
    std::string getFromMemory(const std::string& key);
    void clearMemory();
    std::string getFullContext();
    void removeMemoryItem(const std::string& key);

    bool detectFailure(const std::string& output);
    std::string generateCorrectionPlan(const std::string& failureReason);
    std::string retryWithCorrection(const std::string& failedStepJson);

private:
    std::string planNextAction(const std::string& currentState, const std::string& goal);
    std::string generateCode(const std::string& specification);
    std::string analyzeError(const std::string& errorOutput);
    std::string improveCode(const std::string& code, const std::string& issue);

    void loadMemorySettings();
    void loadMemoryFromDisk();
    void persistMemoryToDisk();
    void enforceMemoryLimit();

    std::string buildToolCallPrompt(const std::string& goal, const std::string& toolsJson);
    std::string extractCodeFromResponse(const std::string& response);
    bool validateGeneratedCode(const std::string& code);

    AgenticEngine* m_agenticEngine = nullptr;
    InferenceEngine* m_inferenceEngine = nullptr;
    void* m_modelTrainer = nullptr;  // Opaque; ModelTrainer is Qt-based, not used in Qt-free Win32 build
    SettingsManager* m_settingsManager = nullptr;

    std::map<std::string, std::string> m_memory;
    std::string m_executionHistoryJson;  // JSON array
    std::string m_currentWorkingDirectory;

    bool m_memoryEnabled = false;
    int64_t m_memoryLimitBytes = 134217728;
    int m_maxRetries = 3;
    int m_currentRetryCount = 0;

    StepStartedFn      m_onStepStarted      = nullptr;
    StepCompletedFn    m_onStepCompleted    = nullptr;
    TaskProgressFn     m_onTaskProgress     = nullptr;
    ExecutionCompleteFn m_onExecutionComplete = nullptr;
    ErrorOccurredFn    m_onErrorOccurred    = nullptr;
    LogMessageFn       m_onLogMessage       = nullptr;
    TrainingProgressFn m_onTrainingProgress = nullptr;
    TrainingCompletedFn m_onTrainingCompleted = nullptr;
    void*              m_callbackContext    = nullptr;
};
>>>>>>> origin/main
