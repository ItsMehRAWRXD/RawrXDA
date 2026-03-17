#pragma once

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QFrame>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QComboBox>
#include "agent_chat_breadcrumb.hpp"
#include "../agentic_executor.h"  // Use the one from src/

class AgentChatBreadcrumb;

/**
 * @brief GitHub Copilot-style AI chat panel
 * 
 * Features:
 * - Chat-style message bubbles
 * - Streaming responses
 * - Code block highlighting
 * - Quick actions (explain, fix, refactor)
 * - Context awareness (selected code)
 */
class AIChatPanel : public QWidget {
    Q_OBJECT

public:
    struct Message {
        enum Role { User, Assistant, System };
        Role role;
        QString content;
        QString timestamp;
        bool isStreaming = false;
    };

    explicit AIChatPanel(QWidget* parent = nullptr);
    
    /**
     * Two-phase initialization - call after QApplication is ready
     * Creates all Qt widgets and applies theme
     */
    void initialize();
    
    void addUserMessage(const QString& message);
    void addAssistantMessage(const QString& message, bool streaming = false);
    void updateStreamingMessage(const QString& content);
    void finishStreaming();
    void clear();
    
    void setContext(const QString& code, const QString& filePath);
    void setInputEnabled(bool enabled);  // Enable/disable input based on model readiness
    void setCloudConfiguration(bool enabled, const QString& endpoint, const QString& apiKey);
    void setLocalConfiguration(bool enabled, const QString& endpoint);
    void setLocalModel(const QString& modelName);
    void setSelectedModel(const QString& modelName);
    void setRequestTimeout(int timeoutMs);
    void setAgenticExecutor(AgenticExecutor* executor);  // Connect agentic execution
    AgentChatBreadcrumb* getBreadcrumb() const { return m_breadcrumb; }
    
    // Cursor-like features
    enum ChatMode {
        ModeDefault = 0,
        ModeMax,
        ModeDeepThinking,
        ModeThinkingResearch,
        ModeDeepResearch
    };
    void setChatMode(ChatMode mode);
    void setContextWindow(int tokens); // 4k to 1,000,000
    void setSelectedModels(const QStringList& models);
    
signals:
    void messageSubmitted(const QString& message);
    void quickActionTriggered(const QString& action, const QString& context);
    void agentModeChanged(int mode);  // Forwarded from breadcrumb
    void modelSelected(const QString& modelName);  // Forwarded from breadcrumb
    void codeInsertRequested(const QString& code);  // Request to insert code into editor
    void codeApproved(const QString& code);
    void codeRejected(const QString& code);
    void aggregatedResponseReady(const QString& combinedText);
    
private slots:
    void onSendClicked();
    void onQuickActionClicked(const QString& action);
    void onNetworkFinished(QNetworkReply* reply);
    void onNetworkError(QNetworkReply::NetworkError code);
    void onModelsListFetched(QNetworkReply* reply);
    void onModelSelected(int index);
    void fetchAvailableModels();
    void onAggregateFinished(QNetworkReply* reply);
    void openModelsDialog();
    
private:
    void setupUI();
    void applyDarkTheme();
    QWidget* createMessageBubble(const Message& msg);
    QWidget* createQuickActions();
    void scrollToBottom();
    void sendMessageToBackend(const QString& message);
    void sendMessageTriple(const QString& message);
    void sendMessageTripleMultiModel(const QString& message);
    QByteArray buildCloudPayload(const QString& message) const;
    QByteArray buildLocalPayload(const QString& message) const;
    QByteArray buildCloudPayloadForMode(const QString& message, ChatMode mode) const;
    QByteArray buildLocalPayloadForMode(const QString& message, ChatMode mode) const;
    QByteArray buildCloudPayloadForModeModel(const QString& message, ChatMode mode, const QString& model) const;
    QByteArray buildLocalPayloadForModeModel(const QString& message, ChatMode mode, const QString& model) const;
    QString extractAssistantText(const QJsonDocument& doc) const;
    QString extractCodeFromMessage(const QString& message);
    QString modeName(ChatMode mode) const;
    QString modeSystemPrompt(ChatMode mode) const;
    double modeTemperature(ChatMode mode) const;
    void createCheckpoint(const QString& name, const QString& combinedText, int changedFiles);
    void snapshotProjectFiles();
    int computeChangedFilesSinceSnapshot();
    
    // Intent classification and agentic processing
    enum MessageIntent {
        Chat,           // Simple conversation
        CodeEdit,       // Modify code/files
        ToolUse,        // Use tools/commands
        Planning,       // Multi-step task planning
        Unknown         // Could not determine
    };
    
    MessageIntent classifyMessageIntent(const QString& message);
    void processAgenticMessage(const QString& message, MessageIntent intent);
    bool isAgenticRequest(const QString& message) const;
    
    QVBoxLayout* m_messagesLayout = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_messagesContainer = nullptr;
    QLineEdit* m_inputField = nullptr;
    QPushButton* m_sendButton = nullptr;
    QWidget* m_quickActionsWidget = nullptr;
    AgentChatBreadcrumb* m_breadcrumb = nullptr;  // Agent mode and model selector
    QComboBox* m_modelSelector = nullptr;  // Model selection dropdown (legacy)
    QComboBox* m_modeSelector = nullptr;   // Mode selector (Cursor-like)
    QComboBox* m_contextSelector = nullptr; // Context window selector
    QPushButton* m_multiModelButton = nullptr; // Open multi-select dialog
    
    QList<Message> m_messages;
    QWidget* m_streamingBubble = nullptr;
    QTextEdit* m_streamingText = nullptr;
    
    QString m_contextCode;
    QString m_contextFilePath;
    QString m_localModel;  // Currently selected local model
    QStringList m_selectedModels; // Multi-model selection
    
    // Cloud/Local configuration
    bool m_initialized = false;
    bool m_cloudEnabled = false;
    bool m_localEnabled = false;
    QString m_cloudEndpoint;
    QString m_localEndpoint;
    QString m_apiKey;
    int m_requestTimeout = 30000; // 30 seconds
    int m_contextSize = 4096; // default 4k
    ChatMode m_chatMode = ModeDefault;
    
    // Lazy initialization tracking
    bool m_widgetsCreated = false;

    // Networking
    QNetworkAccessManager* m_network = nullptr;
    
    // Aggregation state
    bool m_aggregateSessionActive = false;
    QList<QNetworkReply*> m_aggregateReplies;
    QMap<QNetworkReply*, ChatMode> m_replyModeMap;
    QMap<ChatMode, QString> m_aggregateTexts;
    struct ReplyKey { QString model; ChatMode mode; };
    QMap<QNetworkReply*, ReplyKey> m_replyKeyMap;
    QMap<QString, QMap<ChatMode, QString>> m_textsByModel;
    
    // Project change tracking
    QString m_projectRoot;
    QHash<QString, qint64> m_fileSnapshot; // path -> mtime epoch
    
    // Agentic execution
    AgenticExecutor* m_agenticExecutor = nullptr;
    
    // Helper for local response generation
    QString generateLocalResponse(const QString& message, const QString& model);
};
