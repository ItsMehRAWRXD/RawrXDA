#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>

class AgenticEngine;
class ChatInterface;
class MultiTabEditor;
class TerminalPool;

/**
 * @class AgenticCopilotBridge
 * @brief Full Copilot/Cursor-like agent system with hotpatching and puppeteering
 * 
 * Features:
 * - Real-time code analysis and generation
 * - Inline code completions (like Copilot)
 * - Multi-turn agent conversations
 * - Automatic failure detection and recovery
 * - Model hotpatching for different models
 * - Puppeteer-based response correction
 * - Execute arbitrary commands through agent
 * - Full IDE integration with all components
 */
class AgenticCopilotBridge : public QObject {
    Q_OBJECT
    
public:
    explicit AgenticCopilotBridge(QObject* parent = nullptr);
    virtual ~AgenticCopilotBridge() = default;
    
    // Initialize with IDE components
    void initialize(AgenticEngine* engine, ChatInterface* chat, 
                   MultiTabEditor* editor, TerminalPool* terminals);
    
    // Core Copilot-like capabilities
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
    
public slots:
    void onChatMessage(const QString& message);
    void onModelLoaded(const QString& modelPath);
    void onEditorContentChanged();
    
signals:
    void completionReady(const QString& completion);
    void analysisReady(const QString& analysis);
    void agentResponseReady(const QString& response);
    void taskExecuted(const QJsonObject& result);
    void errorOccurred(const QString& error);
    
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
    
    QString m_lastConversationContext;
    QJsonArray m_conversationHistory;
    bool m_hotpatchingEnabled = true;
};
