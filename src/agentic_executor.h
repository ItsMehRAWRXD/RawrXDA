#pragma once


#include <memory>
#include <vector>

class AgenticEngine;
class InferenceEngine;
class ModelTrainer;

/**
 * @class AgenticExecutor
 * @brief Real agentic execution - not simulated, actually performs tasks
 * 
 * This is the core agent loop that:
 * - Decomposes user requests into actionable steps
 * - Executes file operations (create folders, files)
 * - Runs compilers and reports results
 * - Uses function calling to interact with the IDE
 * - Maintains memory across tasks
 * - Self-corrects on failures
 * - Can fine-tune models with on-device training
 */
class AgenticExecutor : public void {

public:
    explicit AgenticExecutor(void* parent = nullptr);
    ~AgenticExecutor();

    void initialize(AgenticEngine* engine, InferenceEngine* inference);

    // Main agentic execution entry point
    void* executeUserRequest(const std::string& request);

    // Core agentic capabilities
    void* decomposeTask(const std::string& goal);
    bool executeStep(const void*& step);
    bool verifyStepCompletion(const void*& step, const std::string& result);

    // File system operations (real, not simulated)
    bool createDirectory(const std::string& path);
    bool createFile(const std::string& path, const std::string& content);
    bool writeFile(const std::string& path, const std::string& content);
    std::string readFile(const std::string& path);
    bool deleteFile(const std::string& path);
    bool deleteDirectory(const std::string& path);
    std::vector<std::string> listDirectory(const std::string& path);

    // Compiler integration (real compilation)
    void* compileProject(const std::string& projectPath, const std::string& compiler = "g++");
    void* runExecutable(const std::string& executablePath, const std::vector<std::string>& args = std::vector<std::string>());

    // Function calling system (tool use)
    void* getAvailableTools();
    void* callTool(const std::string& toolName, const void*& params);

    // Model training capabilities
    void* trainModel(const std::string& datasetPath, const std::string& modelPath, const void*& config);
    bool isTrainingModel() const;

    // Memory and context
    void addToMemory(const std::string& key, const std::any& value);
    std::any getFromMemory(const std::string& key);
    void clearMemory();
    std::string getFullContext();

    // Self-correction
    bool detectFailure(const std::string& output);
    std::string generateCorrectionPlan(const std::string& failureReason);
    void* retryWithCorrection(const void*& failedStep);


    void stepStarted(const std::string& description);
    void stepCompleted(const std::string& description, bool success);
    void taskProgress(int current, int total);
    void executionComplete(const void*& result);
    void errorOccurred(const std::string& error);
    void logMessage(const std::string& message);
    void trainingProgress(int epoch, int totalEpochs, float loss, float perplexity);
    void trainingCompleted(const std::string& modelPath, float finalPerplexity);

private:
    // Agent reasoning using model
    std::string planNextAction(const std::string& currentState, const std::string& goal);
    void* generateCode(const std::string& specification);
    std::string analyzeError(const std::string& errorOutput);
    std::string improveCode(const std::string& code, const std::string& issue);

    // Internal helpers
    void* buildToolCallPrompt(const std::string& goal, const void*& tools);
    std::string extractCodeFromResponse(const std::string& response);
    bool validateGeneratedCode(const std::string& code);

    AgenticEngine* m_agenticEngine = nullptr;
    InferenceEngine* m_inferenceEngine = nullptr;
    std::unique_ptr<ModelTrainer> m_modelTrainer;
    
    std::map<std::string, std::any> m_memory;
    void* m_executionHistory;
    std::string m_currentWorkingDirectory;
    
    int m_maxRetries = 3;
    int m_currentRetryCount = 0;
};

