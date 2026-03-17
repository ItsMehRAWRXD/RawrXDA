#pragma once

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include "cpu_inference_engine.h" // Ensures RawrXD::InferenceEngine is visible
#include "universal_model_router.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * @class AgenticEngine
 * @brief Production-ready AI Core with full agentic capabilities
 */
class AgenticEngine {
public:
    struct GenerationConfig {
        int maxTokens = 2048;
        float temperature = 0.7f;
        float topP = 0.9f;
        std::vector<std::string> stopSequences;
        bool stream = true;
    };

    AgenticEngine();
    ~AgenticEngine();

    void initialize();
    void shutdown();

    // Core Interaction
    std::string processQuery(const std::string& query);
    void processQueryAsync(const std::string& query, std::function<void(std::string)> callback);
    
    // Configuration
    void updateConfig(const GenerationConfig& config);
    // Fixed: Use RawrXD::InferenceEngine
    void setInferenceEngine(RawrXD::InferenceEngine* engine) { m_inferenceEngine = engine; }
    
    // Context Management
    void clearHistory();
    void appendSystemPrompt(const std::string& prompt);
    void loadContext(const std::string& filepath);
    void saveContext(const std::string& filepath);
    
    // Model Management
    std::vector<std::string> getAvailableModels();
    std::string getCurrentModel();
    
    void setModel(const std::string& modelPath);
    void setModelName(const std::string& modelName);
    void processMessage(const std::string& message, const std::string& editorContext = "");

    // Callbacks (replacing signals)
    std::function<void(const std::string&)> onResponseReady;
    std::function<void(bool)> onModelReady;
    std::function<void(bool, const std::string&)> onModelLoadingFinished;

    // Fixed: Use RawrXD::UniversalModelRouter
    void setRouter(std::shared_ptr<RawrXD::UniversalModelRouter> router) { m_router = router; }

private:
    bool m_modelLoaded;
    std::shared_ptr<RawrXD::UniversalModelRouter> m_router;
    std::string m_currentModelPath;
    RawrXD::InferenceEngine* m_inferenceEngine;
    GenerationConfig m_genConfig;
    std::unordered_map<std::string, std::string> m_userPreferences;
    
    // Internal helpers
    std::string buildPrompt(const std::string& query);
    void logInteraction(const std::string& query, const std::string& response);

    // Context implementation details
    std::vector<std::pair<std::string, std::string>> m_history;
    std::string m_systemPrompt;
};

