#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include "compression_interface.h"

namespace rawr_xd {
    class CompleteModelLoaderSystem;
}

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
class AgenticEngine : public QObject {
    Q_OBJECT
public:
    // Model source enumeration
    enum ModelSource {
        Local = 0,      // Local GGUF model
        External = 1    // External API (OpenAI, Anthropic, etc.)
    };
    Q_ENUM(ModelSource)

    // External model provider
    enum class Provider {
        OpenAI,
        Anthropic,
        Groq,
        Ollama
    };

    explicit AgenticEngine(QObject* parent = nullptr);
    virtual ~AgenticEngine();
    
    void initialize();
    
    // Set the high-performance model loader backend
    void setModelLoader(rawr_xd::CompleteModelLoaderSystem* loader);
    
    // AI Core Component 1: Code Analysis
    QString analyzeCode(const QString& code);
    QJsonObject analyzeCodeQuality(const QString& code);
    QJsonArray detectPatterns(const QString& code);
    QJsonObject calculateMetrics(const QString& code);
    QString suggestImprovements(const QString& code);
    
    // AI Core Component 2: Code Generation
    QString generateCode(const QString& prompt);
    QString generateFunction(const QString& signature, const QString& description);
    QString generateClass(const QString& className, const QJsonObject& spec);
    QString generateTests(const QString& code);
    QString refactorCode(const QString& code, const QString& refactoringType);
    
    // AI Core Component 3: Task Planning
    QJsonArray planTask(const QString& goal);
    QJsonObject decomposeTask(const QString& task);
    QJsonArray generateWorkflow(const QString& project);
    QString estimateComplexity(const QString& task);
    
    // AI Core Component 4: NLP - Natural Language Processing
    QString understandIntent(const QString& userInput);
    QJsonObject extractEntities(const QString& text);
    QString generateNaturalResponse(const QString& query, const QJsonObject& context);
    QString summarizeCode(const QString& code);
    QString explainError(const QString& errorMessage);
    
    // AI Core Component 5: Learning and Improvement
    void collectFeedback(const QString& responseId, bool positive, const QString& comment);
    void trainFromFeedback();
    QJsonObject getLearningStats() const;
    void adaptToUserPreferences(const QJsonObject& preferences);
    
    // AI Core Component 6: Security and Validation
    bool validateInput(const QString& input);
    QString sanitizeCode(const QString& code);
    bool isCommandSafe(const QString& command);
    
    // ---- compression helpers for agentic use ----
    Q_INVOKABLE bool compressData(const QByteArray& in, QByteArray& out);
    Q_INVOKABLE bool decompressData(const QByteArray& in, QByteArray& out);
    Q_INVOKABLE QString getCompressionStats() const;
    Q_INVOKABLE bool optimizeCompressionForModel(const QString& path);
    
    // Agent tool capabilities (file system operations)
    QString grepFiles(const QString& pattern, const QString& path = ".");
    QString readFile(const QString& filepath, int startLine = -1, int endLine = -1);
    QString searchFiles(const QString& query, const QString& path = ".");
    QString referenceSymbol(const QString& symbol);
    
    // Model management
    bool isModelLoaded() const;
    QString currentModelPath() const { return QString::fromStdString(m_currentModelPath); }
    QString generateResponse(const QString& message);
    void setInferenceEngine(class InferenceEngine* engine);
    
    // CRITICAL: Mark model as loaded after external load (for MainWindow->AgenticEngine sync)
    void markModelAsLoaded(const QString& modelPath) { 
        m_modelLoaded = true; 
        m_currentModelPath = modelPath.toStdString(); 
        qDebug() << "[AgenticEngine::markModelAsLoaded] Model flagged:" << modelPath;
    }

signals:
    // Streaming signals for external model responses
    void streamToken(const QString& token);
    void streamFinished();
    void errorOccurred(const QString& error);
    void responseReady(const QString& response);
    void modelLoadingFinished(bool success, const QString& modelPath);
    void modelReady(bool success);
    void feedbackCollected(const QString& responseId);
    void learningCompleted();
    void securityWarning(const QString& warning);
    
    // Phase 2: Streaming and refactoring signals
    void tokenGenerated(int delta);  // Emitted for each token during generation
    void refactorSuggested(const QString& original, const QString& suggested);  // Emitted when refactor is ready

public:
    // Generation configuration
    struct GenerationConfig {
        float temperature = 0.8f;
        float topP = 0.9f;
        int maxTokens = 512;
    };
    void setGenerationConfig(const GenerationConfig& config);
    GenerationConfig generationConfig() const { return m_genConfig; }
    
    // External model configuration
    void setModelSource(ModelSource source);
    void configureExternalModel(Provider provider, const QString& apiKey, const QString& modelName, const QString& endpoint = "");
    
public slots:
    void setModel(const QString& modelPath);
    void setModelName(const QString& modelName);
    void processMessage(const QString& message, const QString& editorContext = QString(), bool streaming = true);
    
private:
    QString generateTokenizedResponse(const QString& message);
    QString generateFallbackResponse(const QString& message);
    bool loadModelAsync(const std::string& modelPath);
    QString resolveGgufPath(const QString& modelName);
    
    // Internal AI processing
    QString processWithContext(const QString& input, const QJsonObject& context);
    QJsonObject buildCodeContext(const QString& code);
    QString applyTemplate(const QString& templateName, const QJsonObject& params);
    
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
    std::shared_ptr<ICompressionProvider> compression_provider_;
    
    // External model client and source selection
    ModelSource m_modelSource = Local;
    class ExternalModelClient* m_externalClient = nullptr;
    Provider m_externalProvider = Provider::OpenAI;
    QString m_externalApiKey;
    QString m_externalEndpoint;
    rawr_xd::CompleteModelLoaderSystem* m_modelLoader = nullptr;
};

