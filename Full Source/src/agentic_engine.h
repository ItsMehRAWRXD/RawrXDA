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
        
        // Extended configurations for legacy UI
        bool maxMode = false;
        bool deepThinking = false;
        bool deepResearch = false;
        bool noRefusal = false;
    };

    AgenticEngine();
    ~AgenticEngine();

    void initialize();
    void shutdown();

    // Core Interaction
    std::string processQuery(const std::string& query);
    std::string chat(const std::string& message) { return processQuery(message); } // Alias for legacy support
    std::string chatStream(const std::string& message, const std::function<void(const std::string&)>& onToken);
    void processQueryAsync(const std::string& query, std::function<void(std::string)> callback);
    json planTask(const std::string& goal);
    std::string executePlan(const json& plan);

    // Legacy member configuration
    void updateConfig(const GenerationConfig& config);

    // Legacy tool triggers
    std::string runDumpbin(const std::string& filePath, const std::string& mode = "headers");
    std::string runCodex(const std::string& filePath);
    std::string runCompiler(const std::string& sourceFile, const std::string& target = "");

    // Win32 IDE UI expected methods
    std::string analyzeCode(const std::string& code);
    std::string analyzeCodeQuality(const std::string& code);
    std::string detectPatterns(const std::string& code);
    std::string calculateMetrics(const std::string& code);
    std::string suggestImprovements(const std::string& code);
    std::string generateCode(const std::string& prompt);
    std::string generateFunction(const std::string& spec, const std::string& lang = "cpp");
    std::string generateClass(const std::string& spec, const std::string& lang = "cpp");
    std::string generateTests(const std::string& code);
    std::string refactorCode(const std::string& code, const std::string& instruction);
    std::string decomposeTask(const std::string& task);
    std::string generateWorkflow(const std::string& plan);
    std::string estimateComplexity(const std::string& task);
    std::string understandIntent(const std::string& query);
    std::string extractEntities(const std::string& text);
    std::string generateNaturalResponse(const std::string& input, const std::string& context = "");
    std::string summarizeCode(const std::string& code);
    std::string explainError(const std::string& errorMsg);
    void collectFeedback(const std::string& reqId, bool positive, const std::string& comment = "");
    void trainFromFeedback();
    std::string getLearningStats() const;
    void adaptToUserPreferences(const std::string& pref);
    bool validateInput(const std::string& input);
    std::string sanitizeCode(const std::string& code);
    bool isCommandSafe(const std::string& command);
    std::string grepFiles(const std::string& pattern, const std::string& folder = ".");
    std::string readFile(const std::string& path, int startLine = 1, int endLine = 0);
    std::string searchFiles(const std::string& query, const std::string& pattern = "*");
    std::string referenceSymbol(const std::string& symbol);
    std::string runSubAgent(const std::string& description, const std::string& prompt);
    std::string executeChain(const std::vector<std::string>& steps, const std::string& initialInput = "");
    std::string executeSwarm(const std::vector<std::string>& prompts, const std::string& mergeStrategy = "concatenate", int maxParallel = 4);

private:
    RawrXD::InferenceEngine* m_inferenceEngine;
    GenerationConfig m_config;
    class IDEConfig* m_ideConfig;
    
    // Fixed: Use RawrXD::InferenceEngine
    void setInferenceEngine(RawrXD::InferenceEngine* engine) { m_inferenceEngine = engine; }
public:
    // Required for UI bridge
    void setInferenceEnginePublic(RawrXD::InferenceEngine* engine) { m_inferenceEngine = engine; }
private:
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
    std::unordered_map<std::string, std::string> m_userPreferences;
    
    // Internal helpers
    std::string buildPrompt(const std::string& query);
    void logInteraction(const std::string& query, const std::string& response);

    // Context implementation details
    std::vector<std::pair<std::string, std::string>> m_history;
    std::string m_systemPrompt;
};

