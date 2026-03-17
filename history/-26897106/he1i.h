#pragma once

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include "cpu_inference_engine.h"
#include "nlohmann_stub.h"

using json = nlohmann::json;

// Forward declaration if needed, but we included base class details
using InferenceEngine = CPUInference::InferenceEngine;


/**
 * @class AgenticEngine
 * @brief Production-ready AI Core with full agentic capabilities
 * 
 * Complete AI Core Components:
 * 1. Code Analysis - Static analysis, pattern detection, quality metrics
 * 2. Code Generation - Context-aware code synthesis with templates
 * 3. Task Planning - Multi-step task decomposition and execution
 * 4. NLP - Natural language understanding and generation
 * 5. Learning - Feedback collection and model adaptation
 * 6. Security - Input validation and sandboxed execution
 */
class AgenticEngine {

public:
    explicit AgenticEngine();
    virtual ~AgenticEngine();
    
    void initialize();
    
    // AI Core Component 1: Code Analysis
    std::string analyzeCode(const std::string& code);
    json analyzeCodeQuality(const std::string& code);
    json detectPatterns(const std::string& code);
    json calculateMetrics(const std::string& code);
    std::string suggestImprovements(const std::string& code);
    
    // AI Core Component 2: Code Generation
    std::string generateCode(const std::string& prompt);
    std::string generateFunction(const std::string& signature, const std::string& description);
    std::string generateClass(const std::string& className, const json& spec);
    std::string generateTests(const std::string& code);
    std::string refactorCode(const std::string& code, const std::string& refactoringType);
    
    // AI Core Component 3: Task Planning
    json planTask(const std::string& goal);
    json decomposeTask(const std::string& task);
    json generateWorkflow(const std::string& project);
    std::string estimateComplexity(const std::string& task);
    
    // AI Core Component 4: NLP
    std::string understandIntent(const std::string& userInput);
    json extractEntities(const std::string& text);
    std::string generateNaturalResponse(const std::string& query, const json& context);
    std::string summarizeCode(const std::string& code);
    std::string explainError(const std::string& errorMessage);
    
    // AI Core Component 5: Learning and Improvement
    void collectFeedback(const std::string& responseId, bool positive, const std::string& comment);
    void trainFromFeedback();
    json getLearningStats() const;
    void adaptToUserPreferences(const json& preferences);
    
    // AI Core Component 6: Security and Validation
    bool validateInput(const std::string& input);
    std::string sanitizeCode(const std::string& code);
    bool isCommandSafe(const std::string& command);
    
    // Agent tool capabilities
    std::string grepFiles(const std::string& pattern, const std::string& path = ".");
    std::string readFile(const std::string& filepath, int startLine = -1, int endLine = -1);
    std::string searchFiles(const std::string& query, const std::string& path = ".");
    std::string referenceSymbol(const std::string& symbol);
    
    // Model management
    bool isModelLoaded() const { return m_modelLoaded; }
    std::string currentModelPath() const { return m_currentModelPath; }
    std::string generateResponse(const std::string& message);
    void setInferenceEngine(class InferenceEngine* engine) { m_inferenceEngine = engine; }
    
    void markModelAsLoaded(const std::string& modelPath) { 
        m_modelLoaded = true; 
        m_currentModelPath = modelPath; 
    }
    
    struct GenerationConfig {
        float temperature = 0.8f;
        float topP = 0.9f;
        int maxTokens = 512;
    };
    void setGenerationConfig(const GenerationConfig& config) { m_genConfig = config; }
    GenerationConfig generationConfig() const { return m_genConfig; }
    
    void setModel(const std::string& modelPath);
    void setModelName(const std::string& modelName);
    void processMessage(const std::string& message, const std::string& editorContext = "");

    // Callbacks (replacing signals)
    std::function<void(const std::string&)> onResponseReady;
    std::function<void(bool)> onModelReady;
    std::function<void(bool, const std::string&)> onModelLoadingFinished;

private:
    bool m_modelLoaded;
    std::string m_currentModelPath;
    class InferenceEngine* m_inferenceEngine;
    GenerationConfig m_genConfig;
    std::unordered_map<std::string, std::string> m_userPreferences;
    
    // Internal helpers
    bool loadModelAsync(const std::string& path);
    std::string resolveGgufPath(const std::string& modelName);

    // Feedback data
    int m_totalInteractions;
    int m_positiveResponses;
    std::vector<std::string> m_feedbackHistory;
    std::vector<int> m_responseRatings;
};

