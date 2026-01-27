#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariant>
#include <QHash>
#include <mutex>
#include <vector>
#include <memory>

// Forward declarations
class AgenticEngine;
class ChatInterface;
class MultiTabEditor;
class TerminalPool;
class AgenticExecutor;

/**
 * @class AgenticCopilotBridge
 * @brief Bridge between agentic backend and IDE frontend UI components
 * 
 * Provides a unified interface for:
 * - Code completion and suggestions
 * - Code analysis and refactoring
 * - Test generation
 * - Multi-turn conversations
 * - Agent task execution
 * - Model training and updates
 */
class AgenticCopilotBridge : public QObject {
    Q_OBJECT

public:
    explicit AgenticCopilotBridge(QObject* parent = nullptr);
    ~AgenticCopilotBridge();

    // Initialization
    void initialize(AgenticEngine* engine, ChatInterface* chat, MultiTabEditor* editor, 
                   TerminalPool* terminals, AgenticExecutor* executor);

    // Code Generation & Analysis
    QString generateCodeCompletion(const QString& context, const QString& prefix);
    QString analyzeActiveFile();
    QString suggestRefactoring(const QString& code);
    QString generateTestsForCode(const QString& code);

    // Conversation & Interaction
    QString askAgent(const QString& question, const QJsonObject& context = QJsonObject());
    QString continuePreviousConversation(const QString& followUp);

    // Execution & Error Recovery
    QString executeWithFailureRecovery(const QString& prompt);
    QString hotpatchResponse(const QString& originalResponse, const QJsonObject& context);
    bool detectAndCorrectFailure(QString& response, const QJsonObject& context);

    // Agent Task Execution
    QJsonObject executeAgentTask(const QJsonObject& task);
    QJsonArray planMultiStepTask(const QString& goal);

    // Code Transformation
    QJsonObject transformCode(const QString& code, const QString& transformation);
    QString explainCode(const QString& code);
    QString findBugs(const QString& code);

    // Feedback & Model Management
    void submitFeedback(const QString& feedback, bool isPositive);
    void updateModel(const QString& newModelPath);
    QJsonObject trainModel(const QString& datasetPath, const QString& modelPath, const QJsonObject& config);
    bool isTrainingModel() const;

    // Display methods
    void showResponse(const QString& response);
    void displayMessage(const QString& message);

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
    void errorOccurred(const QString& errorMsg);
    void modelUpdated();
    void trainingProgress(int epoch, int totalEpochs, float loss, float perplexity);
    void trainingCompleted(const QString& modelPath, float finalPerplexity);
    void feedbackSubmitted();
    // New signals for enhanced UI interaction and metrics
    void responseReady(const QString& response);
    void messageDisplayed(const QString& message);
    void chatMessageProcessed(const QString& originalMessage, const QString& agentResponse);
    void modelLoaded(const QString& modelPath);
    void editorAnalysisReady(const QString& analysis);

private:
    // Private helper methods for corrections
    QString correctHallucinations(const QString& response, const QJsonObject& context);
    QString enforceResponseFormat(const QString& response, const QString& format);
    QString bypassRefusals(const QString& response, const QString& originalPrompt);

    // Context building
    QJsonObject buildExecutionContext();
    QJsonObject buildCodeContext(const QString& code);
    QJsonObject buildFileContext();

    // Member variables
    mutable std::mutex m_mutex;
    AgenticEngine* m_agenticEngine = nullptr;
    ChatInterface* m_chatInterface = nullptr;
    MultiTabEditor* m_multiTabEditor = nullptr;
    TerminalPool* m_terminalPool = nullptr;
    AgenticExecutor* m_agenticExecutor = nullptr;

    // Conversation & state tracking
    QJsonArray m_conversationHistory;
    QString m_lastConversationContext;
    bool m_hotpatchingEnabled = true;

    // Training state
    bool m_isTraining = false;
};
