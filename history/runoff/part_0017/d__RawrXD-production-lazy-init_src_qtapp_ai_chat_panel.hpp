#ifndef AI_CHAT_PANEL_HPP
#define AI_CHAT_PANEL_HPP

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QTextEdit>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QElapsedTimer>
#include <QDateTime>
#include <QMap>
#include <QVariant>
#include <QJsonDocument>
#include <QJsonObject>
#include <memory>

// Forward declarations
class AgentChatBreadcrumb;
namespace RawrXD {
    namespace Database {
        class ChatHistoryManager;
    }
    namespace Chat {
        class ChatProductionInfrastructure;
    }
}
class CommandPalette;
class InferenceEngine;
class AgenticExecutor;

enum ChatMode {
    ModeDefault = 0,
    ModeMax = 1,
    ModeDeepThinking = 2,
    ModeThinkingResearch = 3,
    ModeDeepResearch = 4
};

class AIChatPanel : public QWidget {
    Q_OBJECT

public:
    enum MessageIntent {
        Unknown,
        Chat,
        CodeEdit,
        ToolUse,
        Planning
    };

    struct ChatMessage {
        enum Role { User, Assistant, System };
        Role role;
        QString content;
        QString timestamp;
        QString source;  // "AgenticEngine" | "AutonomousEngine" | "Win32Bridge" | "InferenceEngine"
        QJsonObject metadata;  // operation details, latency, requestId, etc.
        QString requestId;  // trace back to original request
        bool isStreaming = false;
        bool isInline = false;
        bool isError = false;
        QString errorDetails;
        
        ChatMessage(Role r, const QString& c, const QString& src = "InferenceEngine") 
            : role(r), content(c), source(src) {
            timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
            requestId = QString::number(QDateTime::currentMSecsSinceEpoch());
        }
        
        ChatMessage(Role r, const QString& c, const QString& src, const QJsonObject& meta) 
            : role(r), content(c), source(src), metadata(meta) {
            timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
            requestId = QString::number(QDateTime::currentMSecsSinceEpoch());
        }
        
        void setMetadata(const QString& key, const QVariant& value) {
            if (value.type() == QVariant::String) {
                metadata.insert(key, value.toString());
            } else if (value.type() == QVariant::Int || value.type() == QVariant::LongLong) {
                metadata.insert(key, value.toLongLong());
            } else if (value.type() == QVariant::Double) {
                metadata.insert(key, value.toDouble());
            } else if (value.type() == QVariant::Bool) {
                metadata.insert(key, value.toBool());
            }
        }
        
        QVariant getMetadata(const QString& key, const QVariant& defaultValue = QVariant()) const {
            return metadata.contains(key) ? metadata.value(key).toVariant() : defaultValue;
        }
        
        void setLatency(qint64 latencyMs) {
            setMetadata("latency_ms", latencyMs);
        }
        
        void setEngineSource(const QString& engine) {
            source = engine;
            setMetadata("engine", engine);
        }
        
        void setRequestContext(const QString& context) {
            setMetadata("request_context", context);
        }
        
        void setError(const QString& error, const QString& details = "") {
            isError = true;
            errorDetails = details;
            setMetadata("error", error);
            setMetadata("error_details", details);
        }
        
        QString toJson() const {
            QJsonObject obj;
            obj.insert("role", role == User ? "user" : (role == Assistant ? "assistant" : "system"));
            obj.insert("content", content);
            obj.insert("timestamp", timestamp);
            obj.insert("source", source);
            obj.insert("metadata", metadata);
            obj.insert("requestId", requestId);
            obj.insert("isStreaming", isStreaming);
            obj.insert("isInline", isInline);
            obj.insert("isError", isError);
            obj.insert("errorDetails", errorDetails);
            return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
        }
        
        static ChatMessage fromJson(const QString& jsonStr) {
            QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
            QJsonObject obj = doc.object();
            ChatMessage msg(User, "");
            msg.role = obj["role"].toString() == "user" ? User : (obj["role"].toString() == "assistant" ? Assistant : System);
            msg.content = obj["content"].toString();
            msg.timestamp = obj["timestamp"].toString();
            msg.source = obj["source"].toString();
            msg.metadata = obj["metadata"].toObject();
            msg.requestId = obj["requestId"].toString();
            msg.isStreaming = obj["isStreaming"].toBool();
            msg.isInline = obj["isInline"].toBool();
            msg.isError = obj["isError"].toBool();
            msg.errorDetails = obj["errorDetails"].toString();
            return msg;
        }
    };

    struct CommandMetrics {
        QString command;
        qint64 executionTimeMs = 0;
        qint64 memoryUsageBytes = 0;
        bool success = false;
        QString result;
        QDateTime timestamp;
    };

    struct ReplyKey {
        QString model;
        ChatMode mode;
    };

    AIChatPanel(QWidget* parent = nullptr);
    ~AIChatPanel() override = default;

    // Initialization
    void initialize(const QString& preloadedModel = QString());
    
    // Session management
    void setCurrentSessionId(const QString& sessionId);
    
    // Model management
    void setSelectedModel(const QString& modelName);
    void setContextWindow(int tokens);
    void setChatMode(ChatMode mode);
    
    // Configuration
    void setCloudConfiguration(bool enabled, const QString& endpoint, const QString& apiKey);
    void setLocalConfiguration(bool enabled, const QString& endpoint);
    void setCloudAIEnabled(bool enabled);
    void setLocalAIEnabled(bool enabled);
    void setCloudEndpoint(const QString& endpoint);
    void setLocalEndpoint(const QString& endpoint);
    void setApiKey(const QString& apiKey);
    void setLocalModel(const QString& model);
    void setRequestTimeout(int ms);
    void setInferenceEngine(InferenceEngine* engine);
    
    // Production infrastructure access
    RawrXD::Chat::ChatProductionInfrastructure* infrastructure() const { return m_infrastructure; }
    QJsonObject getHealthStatus() const;
    QJsonObject getMetrics() const;
    QJsonObject getCacheStats() const;
    QJsonObject getAnalyticsReport() const;
    
    // Context management
    void setContext(const QString& code, const QString& filePath);
    
    // Payload preparation
    QByteArray buildCloudPayload(const QString& message) const;
    QByteArray buildLocalPayload(const QString& message) const;
    QByteArray buildCloudPayloadForMode(const QString& message, ChatMode mode) const;
    QByteArray buildLocalPayloadForMode(const QString& message, ChatMode mode) const;
    QByteArray buildCloudPayloadForModeModel(const QString& message, ChatMode mode, const QString& model) const;
    QByteArray buildLocalPayloadForModeModel(const QString& message, ChatMode mode, const QString& model) const;
    QString modeSystemPrompt(ChatMode mode) const;
    double modeTemperature(ChatMode mode) const;
    
    // Input state
    bool isInputEnabled() const;
    void setInputEnabled(bool enabled);

    // Chat operations
    void sendMessage(const QString& message);
    void appendMessage(const QString& role, const QString& message);
    void addUserMessage(const QString& message);
    void addAssistantMessage(const QString& message, bool streaming = false);
    void addErrorMessage(const QString& error, const QString& details = "", bool retryable = false);
    void updateStreamingMessage(const QString& fragment);
    QString finishStreaming();
    QString lastAssistantMessage() const;
    void clearChat();
    void clear();

    // UI
    void applyTheme();
    void scrollToBottom();
    void populateCommandPaletteCommands();

public slots:
    void onSendClicked();
    void onModelSelected(int index);
    void onQuickActionClicked(const QString& action);
    void onSaveCheckpoint();
    void onRestoreCheckpoint();
    void showContextMenu(const QPoint& pos);
    void fetchAvailableModels();
    void openModelsDialog();
    void snapshotProjectFiles();
    void onChatMessage(const QString& message);
    void onAutoCheckpointTimer();
    void onNetworkFinished(QNetworkReply* reply);
    void onAggregateFinished(QNetworkReply* reply);
    void onNetworkError(QNetworkReply::NetworkError code);
    void onModelsListFetched(QNetworkReply* reply);

signals:
    void messageSent(const QString& message);
    void modelSelected(const QString& modelName);
    void loadModelRequested();
    void agentModeChanged(int mode);
    void chatCleared();
    void messageSubmitted(const QString& message);
    void codeApproved(const QString& code);
    void codeRejected(const QString& code);
    void codeInsertRequested(const QString& code);
    void newChatRequested();
    void quickActionTriggered(const QString& action, const QString& context);
    void aggregatedResponseReady(const QString& response);

private:
    void setupUI();
    QWidget* createQuickActions();
    void processModelResponse(const QString& response);
    QWidget* createMessageBubble(const ChatMessage& msg);
    QString modeName(ChatMode mode) const;
    QString extractCodeFromMessage(const QString& message) const;
    void handleAICommand(const QString& command);
    QString resolveAlias(const QString& alias) const;
    void clearResultCache();
    void sendMessageToBackend(const QString& message);
    void handleModelResponse(const QString& response);
    void showCheckpointsDialog();
    void sendMessageTripleMultiModel(const QString& message);
    QString getPerformanceReport() const;
    void registerCommandAlias(const QString& alias, const QString& command);

    // UI Components
    QLineEdit* m_inputField = nullptr;
    QPushButton* m_sendButton = nullptr;
    QComboBox* m_modelSelector = nullptr;
    QComboBox* m_modeSelector = nullptr;
    QComboBox* m_contextSelector = nullptr;
    QPushButton* m_multiModelButton = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_messagesContainer = nullptr;
    QVBoxLayout* m_messagesLayout = nullptr;
    QWidget* m_quickActionsWidget = nullptr;
    AgentChatBreadcrumb* m_breadcrumb = nullptr;
    QWidget* m_streamingBubble = nullptr;
    QTextEdit* m_streamingTextEdit = nullptr;
    QString m_streamingText;

    // State
    bool m_initialized = false;
    bool m_widgetsCreated = false;
    QList<ChatMessage> m_messages;
    QStringList m_commandHistory;
    int m_historyIndex = -1;
    bool m_cacheEnabled = true;
    QHash<QString, QString> m_resultCache;
    QMap<QString, QString> m_commandAliases;
    QElapsedTimer m_commandTimer;
    QList<CommandMetrics> m_commandMetrics;
    QString m_contextCode;
    QString m_contextFilePath;
    ChatMode m_chatMode = ModeDefault;
    QString m_currentSessionId;

    // Namespace-qualified pointer to avoid RawrXD namespace error if not fully included
    // We will include chat_history_manager.h in the cpp
    RawrXD::Database::ChatHistoryManager* m_historyManager = nullptr; 

    // Production-ready AI state
    InferenceEngine* m_inferenceEngine = nullptr;
    RawrXD::Chat::ChatProductionInfrastructure* m_infrastructure = nullptr;
    bool m_cloudEnabled = true;
    bool m_localEnabled = true;
    QString m_apiKey;
    QString m_cloudEndpoint;
    QString m_localEndpoint;
    int m_requestTimeout = 30000;
    QStringList m_selectedModels;
    QMap<QString, QMap<QString, QString>> m_textsByModel;
    QMap<QNetworkReply*, ReplyKey> m_replyKeyMap;
    QMap<QNetworkReply*, ChatMode> m_replyModeMap;
    QList<QNetworkReply*> m_aggregateReplies;
    bool m_aggregateSessionActive = false;
    QMap<QString, QString> m_aggregateTexts;
    QMap<QNetworkReply*, QString> m_replyContentBuffer;
    QNetworkAccessManager* m_network = nullptr;

    // Project and Agentic state
    QString m_projectRoot;
    QString m_localModel;
    int m_contextSize = 4096;
    QMap<QString, qint64> m_fileSnapshot;
    AgenticExecutor* m_agenticExecutor = nullptr;
    int m_autoCheckpointIntervalMinutes = 5;
    bool m_autoCheckpointEnabled = true;
    QTimer* m_autoCheckpointTimer = nullptr;
    CommandPalette* m_commandPalette = nullptr;

private:
    // Helper functions
    void sendMessageTriple(const QString& text);
    QString generateLocalResponse(const QString& prompt, const QString& model);
    QString extractAssistantText(const QByteArray& data, ChatMode mode) const;
    QString extractAssistantText(const QJsonDocument& doc) const;
    int computeChangedFilesSinceSnapshot();
    void createCheckpoint(const QString& name = QString());
    void restoreCheckpoint(const QString& name);
    void setSelectedModels(const QStringList& models);
    bool isAgenticRequest(const QString& text) const;
    MessageIntent classifyMessageIntent(const QString& text);
    void processAgenticMessage(const QString& message, MessageIntent intent);
    void setAgenticExecutor(AgenticExecutor* executor);
    void setHistoryManager(RawrXD::Database::ChatHistoryManager* manager);
    void showHistory();
    void showHistoryOld();
    void showAdvancedSettings();
    void showSettings();
    void setCommandPalette(CommandPalette* palette);
    void enableAutoCheckpoint(bool enabled, int intervalMinutes);
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void sessionSelected(const QString& sessionId);
    void settingsRequested();
};

#endif // AI_CHAT_PANEL_HPP
