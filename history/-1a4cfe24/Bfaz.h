#ifndef SESSION_PERSISTENCE_H
#define SESSION_PERSISTENCE_H

#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QHash>
#include <QMutex>
#include <QSharedPointer>
#include <QVector>
#include <math>

namespace RawrXD {

struct ChatMessage {
    QString id;
    QString role; // "user", "assistant", "system"
    QString content;
    QDateTime timestamp;
    QJsonObject metadata;
    
    ChatMessage() {}
    ChatMessage(const QString& r, const QString& c, const QJsonObject& m = QJsonObject())
        : role(r), content(c), timestamp(QDateTime::currentDateTime()), metadata(m) {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
};

struct Session {
    QString id;
    QString title;
    QDateTime created;
    QDateTime lastAccessed;
    QVector<ChatMessage> messages;
    QJsonObject context;
    bool encrypted;
    
    Session() : encrypted(false) {}
    Session(const QString& t) : title(t), created(QDateTime::currentDateTime())
        , lastAccessed(QDateTime::currentDateTime()), encrypted(false) {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    
    void addMessage(const ChatMessage& message) {
        messages.append(message);
        lastAccessed = QDateTime::currentDateTime();
    }
    
    int getMessageCount() const { return messages.size(); }
    qint64 getAgeSeconds() const { return created.secsTo(QDateTime::currentDateTime()); }
};

class VectorStore {
public:
    VectorStore() {}
    virtual ~VectorStore() {}
    
    virtual bool initialize(const QString& path) = 0;
    virtual bool addVector(const QString& id, const QVector<double>& vector, const QJsonObject& metadata) = 0;
    virtual QVector<QString> searchSimilar(const QVector<double>& query, int k = 5, double threshold = 0.7) = 0;
    virtual bool removeVector(const QString& id) = 0;
    virtual int getVectorCount() const = 0;
    
    static double cosineSimilarity(const QVector<double>& a, const QVector<double>& b);
};

class SimpleVectorStore : public VectorStore {
public:
    bool initialize(const QString& path) override;
    bool addVector(const QString& id, const QVector<double>& vector, const QJsonObject& metadata) override;
    QVector<QString> searchSimilar(const QVector<double>& query, int k = 5, double threshold = 0.7) override;
    bool removeVector(const QString& id) override;
    int getVectorCount() const override { return vectors_.size(); }
    
private:
    struct VectorEntry {
        QString id;
        QVector<double> vector;
        QJsonObject metadata;
        QDateTime timestamp;
    };
    
    QHash<QString, VectorEntry> vectors_;
    mutable QMutex mutex_;
};

class SessionPersistence {
public:
    static SessionPersistence& instance();
    
    void initialize(const QString& storagePath = QString(), bool enableRAG = true);
    void shutdown();
    
    // Session management
    QString createSession(const QString& title = QString());
    bool saveSession(const QString& sessionId);
    bool loadSession(const QString& sessionId);
    bool deleteSession(const QString& sessionId);
    QSharedPointer<Session> getSession(const QString& sessionId);
    QList<QString> listSessions();
    
    // Message operations
    bool addMessage(const QString& sessionId, const QString& role, const QString& content, const QJsonObject& metadata = QJsonObject());
    QVector<ChatMessage> getMessages(const QString& sessionId, int limit = 0);
    
    // RAG operations
    bool enableRAG(bool enable);
    QVector<ChatMessage> retrieveRelevantMessages(const QString& sessionId, const QString& query, int limit = 5);
    bool embedAndStoreMessage(const ChatMessage& message);
    
    // Backup and recovery
    bool backupSessions(const QString& backupPath);
    bool restoreSessions(const QString& backupPath);
    
    // Statistics
    int getSessionCount() const;
    int getTotalMessageCount() const;
    qint64 getStorageSize() const;
    
private:
    SessionPersistence() = default;
    ~SessionPersistence();
    
    QString getSessionFilePath(const QString& sessionId);
    QString getVectorStorePath();
    QVector<double> embedText(const QString& text);
    
    QString storagePath_;
    QHash<QString, QSharedPointer<Session>> sessions_;
    QSharedPointer<VectorStore> vectorStore_;
    mutable QMutex mutex_;
    bool initialized_ = false;
    bool ragEnabled_ = false;
    
    static const int DEFAULT_EMBEDDING_SIZE = 384;
};

// Convenience macros
#define SESSION_CREATE(title) RawrXD::SessionPersistence::instance().createSession(title)
#define SESSION_SAVE(id) RawrXD::SessionPersistence::instance().saveSession(id)
#define SESSION_LOAD(id) RawrXD::SessionPersistence::instance().loadSession(id)
#define SESSION_ADD_MESSAGE(id, role, content) RawrXD::SessionPersistence::instance().addMessage(id, role, content)
#define SESSION_RAG_QUERY(id, query) RawrXD::SessionPersistence::instance().retrieveRelevantMessages(id, query)

} // namespace RawrXD

#endif // SESSION_PERSISTENCE_H