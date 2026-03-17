#include "session_persistence.h"
#include "logging/structured_logger.h"
#include "error_handler.h"
#include <QDir>
#include <QCoreApplication>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <math>
#include <algorithm>

namespace RawrXD {

double VectorStore::cosineSimilarity(const QVector<double>& a, const QVector<double>& b) {
    if (a.size() != b.size() || a.isEmpty()) {
        return 0.0;
    }
    
    double dotProduct = 0.0;
    double normA = 0.0;
    double normB = 0.0;
    
    for (int i = 0; i < a.size(); ++i) {
        dotProduct += a[i] * b[i];
        normA += a[i] * a[i];
        normB += b[i] * b[i];
    }
    
    if (normA == 0 || normB == 0) {
        return 0.0;
    }
    
    return dotProduct / (std::sqrt(normA) * std::sqrt(normB));
}

bool SimpleVectorStore::initialize(const QString& path) {
    Q_UNUSED(path); // Simple in-memory store doesn't use path
    return true;
}

bool SimpleVectorStore::addVector(const QString& id, const QVector<double>& vector, const QJsonObject& metadata) {
    QMutexLocker lock(&mutex_);
    
    VectorEntry entry;
    entry.id = id;
    entry.vector = vector;
    entry.metadata = metadata;
    entry.timestamp = QDateTime::currentDateTime();
    
    vectors_[id] = entry;
    
    return true;
}

QVector<QString> SimpleVectorStore::searchSimilar(const QVector<double>& query, int k, double threshold) {
    QMutexLocker lock(&mutex_);
    
    QVector<std::pair<QString, double>> similarities;
    
    for (auto it = vectors_.begin(); it != vectors_.end(); ++it) {
        double similarity = cosineSimilarity(query, it.value().vector);
        if (similarity >= threshold) {
            similarities.append({it.key(), similarity});
        }
    }
    
    // Sort by similarity (descending)
    std::sort(similarities.begin(), similarities.end(), 
        [](const std::pair<QString, double>& a, const std::pair<QString, double>& b) {
            return a.second > b.second;
        });
    
    // Take top k results
    QVector<QString> result;
    for (int i = 0; i < std::min(k, similarities.size()); ++i) {
        result.append(similarities[i].first);
    }
    
    return result;
}

bool SimpleVectorStore::removeVector(const QString& id) {
    QMutexLocker lock(&mutex_);
    return vectors_.remove(id) > 0;
}

SessionPersistence& SessionPersistence::instance() {
    static SessionPersistence instance;
    return instance;
}

void SessionPersistence::initialize(const QString& storagePath, bool enableRAG) {
    QMutexLocker lock(&mutex_);
    
    if (initialized_) {
        return;
    }
    
    // Determine storage path
    if (storagePath.isEmpty()) {
        QDir appDir(QCoreApplication::applicationDirPath());
        storagePath_ = appDir.filePath("sessions");
    } else {
        storagePath_ = storagePath;
    }
    
    // Create storage directory
    QDir dir(storagePath_);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            ERROR_HANDLE("Failed to create session storage directory", ErrorContext()
                .setSeverity(ErrorSeverity::HIGH)
                .setCategory(ErrorCategory::FILE_SYSTEM)
                .setOperation("SessionPersistence initialization")
                .addMetadata("storage_path", storagePath_));
            return;
        }
    }
    
    ragEnabled_ = enableRAG;
    
    if (ragEnabled_) {
        vectorStore_.reset(new SimpleVectorStore());
        if (!vectorStore_->initialize(getVectorStorePath())) {
            LOG_WARN("Failed to initialize vector store, disabling RAG");
            ragEnabled_ = false;
        }
    }
    
    initialized_ = true;
    
    LOG_INFO("Session persistence initialized", {
        {"storage_path", storagePath_},
        {"rag_enabled", ragEnabled_}
    });
}

void SessionPersistence::shutdown() {
    QMutexLocker lock(&mutex_);
    
    if (initialized_) {
        // Save all active sessions
        for (auto it = sessions_.begin(); it != sessions_.end(); ++it) {
            saveSession(it.key());
        }
        
        sessions_.clear();
        vectorStore_.reset();
        initialized_ = false;
        
        LOG_INFO("Session persistence shut down");
    }
}

QString SessionPersistence::createSession(const QString& title) {
    QMutexLocker lock(&mutex_);
    
    if (!initialized_) {
        return QString();
    }
    
    QString actualTitle = title.isEmpty() ? "Untitled Session" : title;
    QSharedPointer<Session> session(new Session(actualTitle));
    
    sessions_[session->id] = session;
    
    LOG_INFO("Session created", {{"session_id", session->id}, {"title", actualTitle}});
    
    return session->id;
}

bool SessionPersistence::saveSession(const QString& sessionId) {
    QMutexLocker lock(&mutex_);
    
    if (!initialized_ || !sessions_.contains(sessionId)) {
        return false;
    }
    
    QSharedPointer<Session> session = sessions_[sessionId];
    
    QJsonObject sessionJson;
    sessionJson["id"] = session->id;
    sessionJson["title"] = session->title;
    sessionJson["created"] = session->created.toString(Qt::ISODate);
    sessionJson["last_accessed"] = session->lastAccessed.toString(Qt::ISODate);
    sessionJson["encrypted"] = session->encrypted;
    sessionJson["context"] = session->context;
    
    QJsonArray messagesArray;
    for (const ChatMessage& message : session->messages) {
        QJsonObject messageJson;
        messageJson["id"] = message.id;
        messageJson["role"] = message.role;
        messageJson["content"] = message.content;
        messageJson["timestamp"] = message.timestamp.toString(Qt::ISODate);
        messageJson["metadata"] = message.metadata;
        messagesArray.append(messageJson);
    }
    sessionJson["messages"] = messagesArray;
    
    QString filePath = getSessionFilePath(sessionId);
    QSaveFile file(filePath);
    
    if (!file.open(QIODevice::WriteOnly)) {
        ERROR_HANDLE("Failed to open session file for writing", ErrorContext()
            .setSeverity(ErrorSeverity::HIGH)
            .setCategory(ErrorCategory::FILE_SYSTEM)
            .setOperation("SessionPersistence saveSession")
            .addMetadata("session_id", sessionId)
            .addMetadata("file_path", filePath));
        return false;
    }
    
    QJsonDocument doc(sessionJson);
    if (file.write(doc.toJson()) == -1) {
        ERROR_HANDLE("Failed to write session data", ErrorContext()
            .setSeverity(ErrorSeverity::HIGH)
            .setCategory(ErrorCategory::FILE_SYSTEM)
            .setOperation("SessionPersistence saveSession")
            .addMetadata("session_id", sessionId));
        return false;
    }
    
    if (!file.commit()) {
        ERROR_HANDLE("Failed to commit session file", ErrorContext()
            .setSeverity(ErrorSeverity::HIGH)
            .setCategory(ErrorCategory::FILE_SYSTEM)
            .setOperation("SessionPersistence saveSession")
            .addMetadata("session_id", sessionId));
        return false;
    }
    
    LOG_DEBUG("Session saved", {{"session_id", sessionId}, {"message_count", session->getMessageCount()}});
    
    return true;
}

bool SessionPersistence::loadSession(const QString& sessionId) {
    QMutexLocker lock(&mutex_);
    
    if (!initialized_) {
        return false;
    }
    
    QString filePath = getSessionFilePath(sessionId);
    QFile file(filePath);
    
    if (!file.exists()) {
        return false;
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        ERROR_HANDLE("Failed to open session file for reading", ErrorContext()
            .setSeverity(ErrorSeverity::HIGH)
            .setCategory(ErrorCategory::FILE_SYSTEM)
            .setOperation("SessionPersistence loadSession")
            .addMetadata("session_id", sessionId)
            .addMetadata("file_path", filePath));
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        ERROR_HANDLE("Invalid session file format", ErrorContext()
            .setSeverity(ErrorSeverity::MEDIUM)
            .setCategory(ErrorCategory::FILE_SYSTEM)
            .setOperation("SessionPersistence loadSession")
            .addMetadata("session_id", sessionId));
        return false;
    }
    
    QJsonObject sessionJson = doc.object();
    
    QSharedPointer<Session> session(new Session());
    session->id = sessionJson["id"].toString();
    session->title = sessionJson["title"].toString();
    session->created = QDateTime::fromString(sessionJson["created"].toString(), Qt::ISODate);
    session->lastAccessed = QDateTime::fromString(sessionJson["last_accessed"].toString(), Qt::ISODate);
    session->encrypted = sessionJson["encrypted"].toBool();
    session->context = sessionJson["context"].toObject();
    
    QJsonArray messagesArray = sessionJson["messages"].toArray();
    for (const QJsonValue& messageValue : messagesArray) {
        QJsonObject messageJson = messageValue.toObject();
        
        ChatMessage message;
        message.id = messageJson["id"].toString();
        message.role = messageJson["role"].toString();
        message.content = messageJson["content"].toString();
        message.timestamp = QDateTime::fromString(messageJson["timestamp"].toString(), Qt::ISODate);
        message.metadata = messageJson["metadata"].toObject();
        
        session->messages.append(message);
    }
    
    sessions_[sessionId] = session;
    
    LOG_DEBUG("Session loaded", {{"session_id", sessionId}, {"message_count", session->getMessageCount()}});
    
    return true;
}

bool SessionPersistence::deleteSession(const QString& sessionId) {
    QMutexLocker lock(&mutex_);
    
    if (!initialized_) {
        return false;
    }
    
    // Remove from memory
    sessions_.remove(sessionId);
    
    // Remove from disk
    QString filePath = getSessionFilePath(sessionId);
    if (QFile::exists(filePath)) {
        if (!QFile::remove(filePath)) {
            LOG_WARN("Failed to delete session file", {{"session_id", sessionId}, {"file_path", filePath}});
        }
    }
    
    LOG_INFO("Session deleted", {{"session_id", sessionId}});
    
    return true;
}

QSharedPointer<Session> SessionPersistence::getSession(const QString& sessionId) {
    QMutexLocker lock(&mutex_);
    
    if (!sessions_.contains(sessionId)) {
        return QSharedPointer<Session>();
    }
    
    return sessions_[sessionId];
}

QList<QString> SessionPersistence::listSessions() {
    QMutexLocker lock(&mutex_);
    
    QList<QString> sessionIds;
    
    // Check disk for sessions
    QDir dir(storagePath_);
    QStringList filters;
    filters << "*.json";
    QStringList files = dir.entryList(filters, QDir::Files);
    
    for (const QString& file : files) {
        QString sessionId = file.left(file.length() - 5); // Remove .json extension
        sessionIds.append(sessionId);
    }
    
    return sessionIds;
}

bool SessionPersistence::addMessage(const QString& sessionId, const QString& role, const QString& content, const QJsonObject& metadata) {
    QMutexLocker lock(&mutex_);
    
    if (!initialized_ || !sessions_.contains(sessionId)) {
        return false;
    }
    
    QSharedPointer<Session> session = sessions_[sessionId];
    ChatMessage message(role, content, metadata);
    session->addMessage(message);
    
    // Store in vector store if RAG is enabled
    if (ragEnabled_) {
        embedAndStoreMessage(message);
    }
    
    LOG_DEBUG("Message added to session", {{"session_id", sessionId}, {"role", role}, {"content_length", content.length()}});
    
    return true;
}

QVector<ChatMessage> SessionPersistence::getMessages(const QString& sessionId, int limit) {
    QMutexLocker lock(&mutex_);
    
    if (!sessions_.contains(sessionId)) {
        return QVector<ChatMessage>();
    }
    
    QSharedPointer<Session> session = sessions_[sessionId];
    
    if (limit <= 0 || limit >= session->messages.size()) {
        return session->messages;
    }
    
    // Return last 'limit' messages
    QVector<ChatMessage> result;
    int start = std::max(0, session->messages.size() - limit);
    for (int i = start; i < session->messages.size(); ++i) {
        result.append(session->messages[i]);
    }
    
    return result;
}

bool SessionPersistence::enableRAG(bool enable) {
    QMutexLocker lock(&mutex_);
    
    if (!initialized_) {
        return false;
    }
    
    ragEnabled_ = enable;
    
    if (ragEnabled_ && !vectorStore_) {
        vectorStore_.reset(new SimpleVectorStore());
        if (!vectorStore_->initialize(getVectorStorePath())) {
            LOG_WARN("Failed to initialize vector store");
            ragEnabled_ = false;
            return false;
        }
    }
    
    LOG_INFO("RAG enabled/disabled", {{"enabled", ragEnabled_}});
    
    return true;
}

QVector<ChatMessage> SessionPersistence::retrieveRelevantMessages(const QString& sessionId, const QString& query, int limit) {
    QMutexLocker lock(&mutex_);
    
    if (!ragEnabled_ || !vectorStore_ || !sessions_.contains(sessionId)) {
        return QVector<ChatMessage>();
    }
    
    QVector<double> queryVector = embedText(query);
    if (queryVector.isEmpty()) {
        return QVector<ChatMessage>();
    }
    
    QVector<QString> similarIds = vectorStore_->searchSimilar(queryVector, limit);
    
    QSharedPointer<Session> session = sessions_[sessionId];
    QVector<ChatMessage> result;
    
    for (const QString& messageId : similarIds) {
        // Find message in session
        for (const ChatMessage& message : session->messages) {
            if (message.id == messageId) {
                result.append(message);
                break;
            }
        }
    }
    
    return result;
}

bool SessionPersistence::embedAndStoreMessage(const ChatMessage& message) {
    if (!ragEnabled_ || !vectorStore_) {
        return false;
    }
    
    QVector<double> vector = embedText(message.content);
    if (vector.isEmpty()) {
        return false;
    }
    
    QJsonObject metadata;
    metadata["session_id"] = "current"; // Would need session context
    metadata["role"] = message.role;
    metadata["timestamp"] = message.timestamp.toString(Qt::ISODate);
    
    return vectorStore_->addVector(message.id, vector, metadata);
}

bool SessionPersistence::backupSessions(const QString& backupPath) {
    QMutexLocker lock(&mutex_);
    
    if (!initialized_) {
        return false;
    }
    
    QDir backupDir(backupPath);
    if (!backupDir.exists()) {
        if (!backupDir.mkpath(".")) {
            return false;
        }
    }
    
    // Copy all session files to backup directory
    QDir sourceDir(storagePath_);
    QStringList files = sourceDir.entryList(QStringList() << "*.json", QDir::Files);
    
    int successCount = 0;
    for (const QString& file : files) {
        QString sourcePath = sourceDir.filePath(file);
        QString destPath = backupDir.filePath(file);
        
        if (QFile::copy(sourcePath, destPath)) {
            successCount++;
        }
    }
    
    LOG_INFO("Session backup completed", {{"backup_path", backupPath}, {"files_backed_up", successCount}});
    
    return successCount == files.size();
}

bool SessionPersistence::restoreSessions(const QString& backupPath) {
    QMutexLocker lock(&mutex_);
    
    if (!initialized_) {
        return false;
    }
    
    QDir backupDir(backupPath);
    if (!backupDir.exists()) {
        return false;
    }
    
    // Clear current sessions
    sessions_.clear();
    
    // Copy backup files to session directory
    QStringList files = backupDir.entryList(QStringList() << "*.json", QDir::Files);
    
    int successCount = 0;
    for (const QString& file : files) {
        QString sourcePath = backupDir.filePath(file);
        QString destPath = QDir(storagePath_).filePath(file);
        
        if (QFile::copy(sourcePath, destPath)) {
            successCount++;
        }
    }
    
    LOG_INFO("Session restore completed", {{"backup_path", backupPath}, {"files_restored", successCount}});
    
    return successCount == files.size();
}

int SessionPersistence::getSessionCount() const {
    QMutexLocker lock(&mutex_);
    return sessions_.size();
}

int SessionPersistence::getTotalMessageCount() const {
    QMutexLocker lock(&mutex_);
    
    int total = 0;
    for (auto it = sessions_.begin(); it != sessions_.end(); ++it) {
        total += it.value()->getMessageCount();
    }
    
    return total;
}

qint64 SessionPersistence::getStorageSize() const {
    QMutexLocker lock(&mutex_);
    
    qint64 total = 0;
    QDir dir(storagePath_);
    QStringList files = dir.entryList(QStringList() << "*.json", QDir::Files);
    
    for (const QString& file : files) {
        total += QFileInfo(dir.filePath(file)).size();
    }
    
    return total;
}

QString SessionPersistence::getSessionFilePath(const QString& sessionId) {
    return QDir(storagePath_).filePath(sessionId + ".json");
}

QString SessionPersistence::getVectorStorePath() {
    return QDir(storagePath_).filePath("vector_store");
}

QVector<double> SessionPersistence::embedText(const QString& text) {
    // Simple text embedding using character frequency
    // In production, this would use a proper embedding model
    QVector<double> embedding(DEFAULT_EMBEDDING_SIZE, 0.0);
    
    if (text.isEmpty()) {
        return embedding;
    }
    
    // Simple character frequency-based embedding
    QHash<QChar, int> charCounts;
    for (QChar ch : text) {
        charCounts[ch.toLower()]++;
    }
    
    // Distribute frequencies across embedding dimensions
    int dim = 0;
    for (auto it = charCounts.begin(); it != charCounts.end(); ++it) {
        embedding[dim % DEFAULT_EMBEDDING_SIZE] += it.value();
        dim++;
    }
    
    // Normalize
    double sum = 0.0;
    for (double val : embedding) {
        sum += val * val;
    }
    
    if (sum > 0) {
        double norm = std::sqrt(sum);
        for (int i = 0; i < embedding.size(); ++i) {
            embedding[i] /= norm;
        }
    }
    
    return embedding;
}

SessionPersistence::~SessionPersistence() {
    shutdown();
}

} // namespace RawrXD