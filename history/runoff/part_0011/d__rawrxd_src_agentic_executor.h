#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <map>
#include "settings_manager_real.hpp"
#include <nlohmann/json.hpp>
#include "../src/cpu_inference_engine.h"

using Json = nlohmann::json;

class AgenticEngine;
class ModelTrainer;

// Use CPUInference::CPUInferenceEngine directly

namespace CPUInference { class CPUInferenceEngine; }

class AgenticExecutor {
public:
    AgenticExecutor();
    ~AgenticExecutor();

    void initialize(AgenticEngine* engine, CPUInference::CPUInferenceEngine* inference);

    // Callbacks replacing signals
    std::function<void(const std::string&)> onStepStarted;
    std::function<void(const std::string&, bool)> onStepCompleted;
    std::function<void(int, int)> onProgress;
    std::function<void(const Json&)> onComplete;
    std::function<void(const std::string&)> onError;
    std::function<void(const std::string&)> onLog;
    std::function<void(int, int, float, float)> onTrainingProgress;
    std::function<void(const std::string&, float)> onTrainingCompleted;

    // Main execution
    Json executeUserRequest(const std::string& request);

    // Core capabilities
    Json decomposeTask(const std::string& goal);
    bool executeStep(const Json& step);
    bool verifyStepCompletion(const Json& step, const std::string& result);

    // Additional methods from reading the cpp
    Json callTool(const std::string& toolName, const Json& params);
    Json trainModel(const std::string& datasetPath, const std::string& modelPath, const Json& config);
    bool isTrainingModel() const;
    
    // Memory
    void addToMemory(const std::string& key, const Json& value);
    Json getFromMemory(const std::string& key);
    void clearMemory();
    std::string getFullContext();
    
    // Failure handling
    bool detectFailure(const std::string& output);
    std::string generateCorrectionPlan(const std::string& failureReason);
    Json retryWithCorrection(const Json& failedStep);
    
    // Legacy support methods
    void loadMemorySettings();
    void removeMemoryItem(const std::string& key);
    void loadMemoryFromDisk();
    void persistMemoryToDisk();
    void enforceMemoryLimit();
    std::string planNextAction(const std::string& currentState, const std::string& goal);
    void logMessage(const std::string& message);
    
    Json compileProject(const std::string& projectPath, const std::string& compiler);
    Json runExecutable(const std::string& executablePath, const std::vector<std::string>& args);

    // File system
    bool createDirectory(const std::string& path);
    bool createFile(const std::string& path, const std::string& content);
    bool writeFile(const std::string& path, const std::string& content);
    std::string readFile(const std::string& path);
    bool deleteFile(const std::string& path);
    bool deleteDirectory(const std::string& path);
    std::vector<std::string> listDirectory(const std::string& path);
    Json getAvailableTools();

private:
    std::string m_currentWorkingDirectory;
    std::unique_ptr<SettingsManager> m_settingsManager;
    AgenticEngine* m_agenticEngine = nullptr;
    CPUInference::CPUInferenceEngine* m_inferenceEngine = nullptr;
    std::map<std::string, Json> m_memory;
};
