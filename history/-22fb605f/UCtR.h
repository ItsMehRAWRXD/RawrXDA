#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>
#include <mutex>

class AgenticEngine;
class ChatInterface;
class MultiTabEditor;
class TerminalPool;
class AgenticExecutor;

/**
 * @class AgenticCopilotBridge
 * @brief Production-ready Copilot/Cursor-like agent system with thread safety
 * 
 * Features:
 * - Real-time code analysis and generation
 * - Inline code completions (like Copilot)
 * - Multi-turn agent conversations with history
 * - Automatic failure detection and recovery
 * - Model hotpatching for different models
 * - Puppeteer-based response correction
 * - User feedback collection and analysis
 * - Thread-safe concurrent operations
 * - Full IDE integration with all components
 * - On-device model fine-tuning
 */
class AgenticCopilotBridge : public QObject {
    Q_OBJECT
    
public:
    explicit AgenticCopilotBridge(QObject* parent = nullptr);
    virtual ~AgenticCopilotBridge();
    
    // Initialize with IDE components
    void initialize(AgenticEngine* engine, ChatInterface* chat, 
                   MultiTabEditor* editor, TerminalPool* terminals, AgenticExecutor* executor);
    
    // Core Copilot-like capabilities (thread-safe)
    QString generateCodeCompletion(const QString& context, const QString& prefix = "");
    QString analyzeActiveFile();
    QString suggestRefactoring(const QString& code);
    QString generateTestsForCode(const QString& code);
    
    // Multi-turn conversation (like Copilot Chat)
    QString askAgent(const QString& question, const QJsonObject& context = QJsonObject());
    QString continuePreviousConversation(const QString& followUp);
    
    // Puppeteering and hotpatching (Cursor IDE style)
    QString executeWithFailureRecovery(const QString& prompt);
    QString hotpatchResponse(const QString& originalResponse, const QJsonObject& context);
    bool detectAndCorrectFailure(QString& response, const QJsonObject& context);
    
    // Direct agent command execution (full agentic)
    QJsonObject executeAgentTask(const QJsonObject& task);
    QJsonArray planMultiStepTask(const QString& goal);
    
    // Code transformation (refactoring, generation, analysis)
    QJsonObject transformCode(const QString& code, const QString& transformation);
    QString explainCode(const QString& code);
    QString findBugs(const QString& code);
    
    // Production features: User feedback mechanism
    void submitFeedback(const QString& feedback, bool isPositive);
    
    // Production features: Model updates
    void updateModel(const QString& newModelPath);
    
    // Production features: Model training
    QJsonObject trainModel(const QString& datasetPath, const QString& modelPath, const QJsonObject& config);
    bool isTrainingModel() const;
    
    // Production features: Enhanced UI integration
    Q_INVOKABLE void showResponse(const QString& response);
    Q_INVOKABLE void displayMessage(const QString& message);
    
public slots:
    void onChatMessage(const QString& message);
    void onModelLoaded(const QString& modelPath);
    void onEditorContentChanged();
    void onTrainingProgress(int epoch, int totalEpochs, float loss, float perplexity);
    void onTrainingCompleted(const QString& modelPath, float finalPerplexity);
    
signals:
    void completionReady(const QString& completion);
    void analysisReady(const QString& analysis);
    void agentResponseReady(const QString& response);
    void taskExecuted(const QJsonObject& result);
    void errorOccurred(const QString& error);
    void feedbackSubmitted();
    void modelUpdated();
    void trainingProgress(int epoch, int totalEpochs, float loss, float perplexity);
    void trainingCompleted(const QString& modelPath, float finalPerplexity);
    
private:
    // Response correction and validation
    QString correctHallucinations(const QString& response, const QJsonObject& context);
    QString enforceResponseFormat(const QString& response, const QString& format);
    QString bypassRefusals(const QString& response, const QString& originalPrompt);
    
    // Context building
    QJsonObject buildExecutionContext();
    QJsonObject buildCodeContext(const QString& code);
    QJsonObject buildFileContext();
    
    // Internal state
    AgenticEngine* m_agenticEngine = nullptr;
    ChatInterface* m_chatInterface = nullptr;
    MultiTabEditor* m_multiTabEditor = nullptr;
    TerminalPool* m_terminalPool = nullptr;
    AgenticExecutor* m_agenticExecutor = nullptr;
    
    QString m_lastConversationContext;
    QJsonArray m_conversationHistory;
    bool m_hotpatchingEnabled = true;
    
    std::mutex m_mutex; // Mutex for thread-safe operations
};
    AgenticEngine* m_agenticEngine = nullptr;
    ChatInterface* m_chatInterface = nullptr;
    MultiTabEditor* m_multiTabEditor = nullptr;
    TerminalPool* m_terminalPool = nullptr;
    
    QString m_lastConversationContext;
    QJsonArray m_conversationHistory;
    bool m_hotpatchingEnabled = true;
    
    // Thread safety for concurrent operations
    mutable std::mutex m_mutex;
};
