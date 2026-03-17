#pragma once

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include <chrono>

// Forward declarations
namespace CPUInference {
    class CPUInferenceEngine;
}
namespace RawrXD {
    class CodeAnalyzer;
    class IDEDiagnosticSystem;
}
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
    
    // AI Core Component 7: Autonomous Loop (Cursor/Copilot Style)
    std::string executeAutonomousTask(const std::string& goal, std::function<void(const std::string&)> progressCallback = nullptr);
    std::string processToolCalls(const std::string& response);
    
    // Agent tool capabilities
    std::string grepFiles(const std::string& pattern, const std::string& path = ".");
    std::string readFile(const std::string& filepath, int startLine = -1, int endLine = -1);
    std::string writeFile(const std::string& filepath, const std::string& content);
    std::string searchFiles(const std::string& query, const std::string& path = ".");
    std::string referenceSymbol(const std::string& symbol);
    
    // RE Suite Integration
    std::string runDumpbin(const std::string& filePath, const std::string& mode);
    std::string runCodex(const std::string& filePath);
    std::string runCompiler(const std::string& sourceFile, const std::string& target);
    
    // Advanced Analysis (NEW)
    std::string performCompleteCodeAudit(const std::string& code);
    std::string getSecurityAssessment(const std::string& code);
    std::string getPerformanceRecommendations(const std::string& code);
    
    // Diagnostic Integration (NEW)
    void integrateWithDiagnostics(RawrXD::IDEDiagnosticSystem* diagnostics);
    std::string getIDEHealthReport();

    // Configuration (must be before methods that use it)
    struct GenerationConfig {
        float temperature = 0.8f;
        float topP = 0.9f;
        int maxTokens = 2048;
        bool maxMode = false;
        bool deepThinking = false;
        bool deepResearch = false;
        bool noRefusal = false;
        bool autoCorrect = false;
    };

    // Message structure for history
    struct Message {
        std::string role; // "system", "user", "assistant", "tool"
        std::string content;
    };

    // Core Inference Integration
    void setInferenceEngine(CPUInference::CPUInferenceEngine* engine) { m_inferenceEngine = engine; m_ownsInferenceEngine = false; }
    CPUInference::CPUInferenceEngine* getInferenceEngine() const { return m_inferenceEngine; }
    bool isModelLoaded() const; 
    std::string currentModelPath() const { return m_currentModelPath; }
    
    void updateConfig(const GenerationConfig& config);
    GenerationConfig getConfig() const { return m_config; }
    void clearHistory();
    
    // CLI/Native compat
    std::string chat(const std::string& message);
    
private:
    std::string m_currentModelPath;
    CPUInference::CPUInferenceEngine* m_inferenceEngine = nullptr;
    bool m_ownsInferenceEngine = false;
    GenerationConfig m_config;
    std::vector<Message> m_chatHistory;

    struct FeedbackEntry {
        std::string responseId;
        bool positive = false;
        std::string comment;
        std::string timestamp;
    };
    std::vector<FeedbackEntry> m_feedback;
    size_t m_feedbackPositive = 0;
    size_t m_feedbackNegative = 0;
    
    // Internal state and logic
    std::string buildSystemPrompt();
    std::string formatChatHistory();
    void addToHistory(const std::string& role, const std::string& content);

    struct FeedbackEntry {
        std::string responseId;
        bool positive;
        std::string comment;
        std::chrono::system_clock::time_point timestamp;
    };

    std::vector<FeedbackEntry> m_feedback;
    std::string m_userPreferences;

    // Analysis systems (NEW)
    std::shared_ptr<RawrXD::CodeAnalyzer> m_codeAnalyzer;
    RawrXD::IDEDiagnosticSystem* m_diagnosticSystem = nullptr;
};
