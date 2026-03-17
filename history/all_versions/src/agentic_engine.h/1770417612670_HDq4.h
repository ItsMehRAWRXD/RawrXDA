#pragma once

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include "cpu_inference_engine.h"

// Forward declarations for Keep/Undo functionality
class AgenticFileOperations;
class AgenticErrorHandler;

/**
 * @class AgenticEngine
 * @brief Production-ready AI Core with full agentic capabilities (No Qt)
 */
class AgenticEngine {
public:
    explicit AgenticEngine();
    virtual ~AgenticEngine();
    
    void initialize();
    
    // AI Core Component 1: Code Analysis
    std::string analyzeCode(const std::string& code);
    std::string analyzeCodeQuality(const std::string& code);
    std::string detectPatterns(const std::string& code);
    std::string calculateMetrics(const std::string& code);
    std::string suggestImprovements(const std::string& code);
    
    // AI Core Component 2: Code Generation
    std::string generateCode(const std::string& prompt);
    std::string generateFunction(const std::string& signature, const std::string& description);
    std::string generateClass(const std::string& className, const std::string& spec);
    std::string generateTests(const std::string& code);
    std::string refactorCode(const std::string& code, const std::string& refactoringType);
    
    // AI Core Component 3: Task Planning
    std::string planTask(const std::string& goal);
    std::string decomposeTask(const std::string& task);
    std::string generateWorkflow(const std::string& project);
    std::string estimateComplexity(const std::string& task);
    
    // AI Core Component 4: NLP
    std::string understandIntent(const std::string& userInput);
    std::string extractEntities(const std::string& text);
    std::string generateNaturalResponse(const std::string& query, const std::string& context);
    std::string summarizeCode(const std::string& code);
    std::string explainError(const std::string& errorMessage);
    
    // AI Core Component 5: Learning
    void collectFeedback(const std::string& responseId, bool positive, const std::string& comment);
    void trainFromFeedback();
    std::string getLearningStats() const;
    void adaptToUserPreferences(const std::string& preferences);
    
    // AI Core Component 6: Security
    bool validateInput(const std::string& input);
    std::string sanitizeCode(const std::string& code);
    bool isCommandSafe(const std::string& command);
    
    // Agent tool capabilities
    std::string grepFiles(const std::string& pattern, const std::string& path = ".");
    std::string readFile(const std::string& filepath, int startLine = -1, int endLine = -1);
    std::string searchFiles(const std::string& query, const std::string& path = ".");
    std::string referenceSymbol(const std::string& symbol);
    
    // RE Suite Integration
    std::string runDumpbin(const std::string& filePath, const std::string& mode);
    std::string runCodex(const std::string& filePath);
    std::string runCompiler(const std::string& sourceFile, const std::string& target);

    // Core Inference Integration
    void setInferenceEngine(RawrXD::CPUInferenceEngine* engine) { m_inferenceEngine = engine; }


    bool isModelLoaded() const { return m_inferenceEngine && m_inferenceEngine->IsModelLoaded(); }
    std::string currentModelPath() const { return m_currentModelPath; }
    
    // Configuration
    struct GenerationConfig {
        float temperature = 0.8f;
        float topP = 0.9f;
        int maxTokens = 2048;
        bool maxMode = false;
        bool deepThinking = false;
        bool deepResearch = false;
        bool noRefusal = false;
    };

    void updateConfig(const GenerationConfig& config);
    // CLI/Native compat
    std::string chat(const std::string& message);

    // SubAgent / Chaining / Swarm — thin wrappers for use from the engine
    // The full implementation lives in SubAgentManager; these are convenience
    // entry points for code that only has an AgenticEngine*.
    std::string runSubAgent(const std::string& description, const std::string& prompt);
    std::string executeChain(const std::vector<std::string>& steps, const std::string& initialInput = "");
    std::string executeSwarm(const std::vector<std::string>& prompts,
                              const std::string& mergeStrategy = "concatenate",
                              int maxParallel = 4);
    
private:
    std::string m_currentModelPath;
    RawrXD::CPUInferenceEngine* m_inferenceEngine = nullptr;


    GenerationConfig m_config;
};
