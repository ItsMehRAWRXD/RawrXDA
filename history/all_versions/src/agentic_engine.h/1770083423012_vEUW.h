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
    
    // Core Inference Integration
    void setInferenceEngine(RawrXD::CPUInferenceEngine* engine) { m_inferenceEngine = engine; }
    bool isModelLoaded() const { return m_inferenceEngine && m_inferenceEngine->IsModelLoaded(); }
    std::string currentModelPath() const { return m_currentModelPath; }
    
    void updateConfig(const struct GenerationConfig& config);
    // CLI/Native compat
    std::string chat(const std::string& message); 

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
    
private:
    std::string m_currentModelPath;
    RawrXD::CPUInferenceEngine* m_inferenceEngine = nullptr;
    GenerationConfig m_config;
};

    GenerationConfig generationConfig() const { return m_genConfig; }
    
public slots:
    void setModel(const QString& modelPath);
    void setModelName(const QString& modelName);
    void processMessage(const QString& message, const QString& editorContext = QString());
    void setInstructionFilePath(const QString& path);
    QString loadedInstructions() const { return m_preResponseInstructions; }
    
    // Keep/Undo file operations with user approval
    bool createFileWithApproval(const QString& filePath, const QString& content);
    bool modifyFileWithApproval(const QString& filePath, const QString& oldContent, const QString& newContent);
    bool deleteFileWithApproval(const QString& filePath);
    void undoLastFileOperation();
    bool canUndoFileOperation() const;
    
signals:
    void responseReady(const QString& response);
    void modelLoadingFinished(bool success, const QString& modelPath);
    void modelReady(bool success);
    void feedbackCollected(const QString& responseId);
    void learningCompleted();
    void securityWarning(const QString& warning);
    void fileOperationCompleted(const QString& operation, const QString& filePath, bool success);
    
    // Phase 2: Streaming and refactoring signals
    void tokenGenerated(int delta);  // Emitted for each token during generation
    void refactorSuggested(const QString& original, const QString& suggested);  // Emitted when refactor is ready
    
private:
    QString generateTokenizedResponse(const QString& message);
    QString generateFallbackResponse(const QString& message);
    bool loadModelAsync(const std::string& modelPath);
    bool loadInstructionsFromFile(const QString& path);
    void onInstructionFileChanged(const QString& path);
    QString m_instructionFilePath;
    QString m_preResponseInstructions;
    QFileSystemWatcher* m_instructionWatcher = nullptr;
    QString resolveGgufPath(const QString& modelName);
    
    // Internal AI processing
    QString processWithContext(const QString& input, const QJsonObject& context);
    QJsonObject buildCodeContext(const QString& code);
    QString applyTemplate(const QString& templateName, const QJsonObject& params);
    
    // Keep/Undo file operations
    AgenticFileOperations* m_fileOperations;
    AgenticErrorHandler* m_errorHandler;
    
    // Tool registry for agent capabilities
    class ToolRegistry* m_toolRegistry = nullptr;
    std::shared_ptr<class Logger> m_logger;
    std::shared_ptr<class Metrics> m_metrics;
    
    // Learning data structures
    struct FeedbackEntry {
        QString responseId;
        QString input;
        QString output;
        bool positive;
        QString comment;
        qint64 timestamp;
    };
    std::vector<FeedbackEntry> m_feedbackHistory;
    std::unordered_map<QString, int> m_responseRatings;
    
    // User preferences and adaptation
    QJsonObject m_userPreferences;
    int m_totalInteractions = 0;
    int m_positiveResponses = 0;
    
    // Model state
    bool m_modelLoaded = false;
    std::string m_currentModelPath;
    class InferenceEngine* m_inferenceEngine = nullptr;
    GenerationConfig m_genConfig;
};
