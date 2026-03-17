#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <functional>
#include "cpu_inference_engine.h"

struct GenerationConfig {
    float temperature = 0.8f;
    float top_p = 0.9f;
    int max_tokens = 2048;
    int topK = 40;
    
    // New flags
    bool maxMode = false;
    bool deepThinking = false;
    bool deepResearch = false;
    bool noRefusal = false;
    
    // Compat
    std::vector<std::string> stopSequences;
    bool stream = true;
};

struct Message {
    std::string role;
    std::string content;
};

class AgenticEngine {
public:
    AgenticEngine();
    ~AgenticEngine();
    
    void initialize() {}; // Compat
    void shutdown() {}; // Compat

    void setInferenceEngine(RawrXD::CPUInferenceEngine* engine);
    void updateConfig(const GenerationConfig& config);
    void clearHistory();
    void appendSystemPrompt(const std::string& prompt);
    void loadContext(const std::string& context);
    void saveContext(const std::string& path);
    
    std::string planTask(const std::string& task);
    std::string executePlan(const std::string& plan);
    std::string executePlan(const nlohmann::json& plan); // Compat overload
    std::string chat(const std::string& message);
    std::string analyzeCode(const std::string& code);
    std::string generateCode(const std::string& description);
    std::string bugReport(const std::string& code, const std::string& error);
    std::string codeSuggestions(const std::string& code);

    // Compat methods for existing CLI/GUI calls
    std::string processQuery(const std::string& query);
    void processQueryAsync(const std::string& query, std::function<void(std::string)> callback);
    nlohmann::json planTask(const std::string& goal, bool returnJson); // Compat signature for JSON return if needed
    
    // Callbacks
    std::function<void(const std::string&)> onResponseReady;

    // Missing methods required by CLI stubs
    std::vector<std::string> getAvailableModels();
    std::string getCurrentModel();
    void setModel(const std::string& modelPath);
    void setModelName(const std::string& modelName);
    void processMessage(const std::string& message, const std::string& editorContext);
    std::string buildPrompt(const std::string& query);
    void logInteraction(const std::string& query, const std::string& response);

private:
    RawrXD::CPUInferenceEngine* m_inferenceEngine = nullptr;
    GenerationConfig m_genConfig;
    std::vector<Message> m_history;
    bool m_modelLoaded = false;
};

