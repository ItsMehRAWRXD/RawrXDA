/**
 * @file conversation_memory_system.h
 * @brief Multi-turn conversation memory and session management for autonomous agents
 * @author RawrXD Team
 * @date 2026
 * 
 * Provides:
 * - Conversation history with full context preservation
 * - Session persistence to disk/database
 * - Memory compression using summarization
 * - Dynamic context window management
 * - Multi-agent conversation support
 */

#pragma once

#include <QString>
#include <QObject>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QMutex>
#include <vector>
#include <memory>

/**
 * @class Message
 * @brief Single message in conversation history
 */
class Message {
public:
    enum class Role {
        USER,
        ASSISTANT,
        SYSTEM,
        TOOL
    };
    
    QString id;
    Role role;
    QString content;
    QString author;
    QDateTime timestamp;
    int tokenCount;
    QJsonObject metadata;
    QString parentMessageId;  // For branching conversations
    
    Message() : role(Role::USER), tokenCount(0) {}
    
    QJsonObject toJson() const;
    static Message fromJson(const QJsonObject& json);
};

/**
 * @class ConversationSession
 * @brief Manages a single conversation session with full history
 */
class ConversationSession {
public:
    ConversationSession(const QString& sessionId = "");
    
    // Message management
    void addMessage(const Message& message);
    void insertMessage(int index, const Message& message);
    void removeMessage(const QString& messageId);
    
    Message getMessage(const QString& messageId) const;
    std::vector<Message> getMessages(int limit = -1, int offset = 0) const;
    std::vector<Message> getMessagesSince(const QDateTime& timestamp) const;
    
    // Session metadata
    QString getSessionId() const { return m_sessionId; }
    QString getTitle() const { return m_title; }
    void setTitle(const QString& title) { m_title = title; }
    
    QDateTime getCreatedTime() const { return m_createdTime; }
    QDateTime getLastModifiedTime() const { return m_lastModifiedTime; }
    
    // Context and memory
    int getTotalTokens() const { return m_totalTokens; }
    int getMessageCount() const { return m_messages.size(); }
    QString getSummary() const { return m_summary; }
    void setSummary(const QString& summary) { m_summary = summary; }
    
    // Export
    QJsonObject toJson() const;
    static ConversationSession fromJson(const QJsonObject& json);
    
private:
    QString m_sessionId;
    QString m_title;
    std::vector<Message> m_messages;
    QDateTime m_createdTime;
    QDateTime m_lastModifiedTime;
    int m_totalTokens;
    QString m_summary;
    QMutex m_mutex;
};

/**
 * @class ConversationContextWindow
 * @brief Manages dynamic context window for maintaining conversation context
 */
class ConversationContextWindow {
public:
    ConversationContextWindow(int maxTokens = 4096);
    
    // Context building
    QString buildContextString(const std::vector<Message>& messages, int maxTokens = -1);
    QJsonArray buildContextArray(const std::vector<Message>& messages, int maxTokens = -1);
    
    // Token management
    void setMaxTokens(int maxTokens);
    int getMaxTokens() const { return m_maxTokens; }
    int estimateTokens(const QString& text) const;
    
    // Compression strategy
    enum class CompressionStrategy {
        NONE,
        SUMMARIZE,
        EXTRACT_SUMMARY,
        COMPRESS_MESSAGES
    };
    
    void setCompressionStrategy(CompressionStrategy strategy) { m_strategy = strategy; }
    QString compressMessages(const std::vector<Message>& messages) const;
    
private:
    int m_maxTokens;
    CompressionStrategy m_strategy;
    
    int countTokensRough(const QString& text) const;
    QString summarizeMessages(const std::vector<Message>& messages) const;
};

/**
 * @class ConversationMemoryManager
 * @brief Centralized management of conversation sessions and memory
 * 
 * Responsibilities:
 * - Create and manage multiple conversation sessions
 * - Persist sessions to disk or database
 * - Memory compression and cleanup
 * - Context window management
 * - Multi-turn state tracking
 */
class ConversationMemoryManager : public QObject {
    Q_OBJECT
    
public:
    explicit ConversationMemoryManager(QObject* parent = nullptr);
    ~ConversationMemoryManager();
    
    // Session management
    QString createSession(const QString& title = "");
    bool loadSession(const QString& sessionId);
    bool saveSession(const QString& sessionId);
    bool deleteSession(const QString& sessionId);
    
    ConversationSession* getCurrentSession() const { return m_currentSession.get(); }
    std::vector<QString> listSessions() const;
    
    // Message operations
    void addMessage(const Message& message);
    Message getLastMessage() const;
    std::vector<Message> getConversationHistory(int limit = -1) const;
    
    // Context management
    QString buildContext(int maxTokens = -1);
    QJsonArray buildContextJson(int maxTokens = -1);
    
    // Memory optimization
    void compressMemory(const QString& sessionId);
    void cleanupOldSessions(int daysOld = 30);
    
    // Statistics
    struct SessionStats {
        QString sessionId;
        int messageCount;
        int totalTokens;
        QDateTime createdTime;
        QDateTime lastModifiedTime;
    };
    
    SessionStats getSessionStats(const QString& sessionId) const;
    std::vector<SessionStats> getAllSessionStats() const;
    
    // Persistence
    void setPersistencePath(const QString& path) { m_persistencePath = path; }
    bool saveToDisk(const QString& sessionId);
    bool loadFromDisk(const QString& sessionId);
    
    // Configuration
    void setContextWindowSize(int maxTokens);
    void setCompressionThreshold(int tokenCount);
    void setMaxSessionAge(int daysOld);
    
signals:
    void sessionCreated(const QString& sessionId);
    void sessionDeleted(const QString& sessionId);
    void messageAdded(const QString& sessionId, const QString& messageId);
    void sessionCompressed(const QString& sessionId);
    void contextWindowUpdated(int tokensUsed, int maxTokens);
    
private slots:
    void onMaintenanceTimer();
    
private:
    std::unique_ptr<ConversationSession> m_currentSession;
    QMap<QString, std::unique_ptr<ConversationSession>> m_sessions;
    
    ConversationContextWindow m_contextWindow;
    
    QString m_persistencePath;
    int m_compressionThreshold;
    int m_maxSessionAgeDays;
    
    QTimer* m_maintenanceTimer;
    QMutex m_mutex;
};

#endif // CONVERSATION_MEMORY_SYSTEM_H
