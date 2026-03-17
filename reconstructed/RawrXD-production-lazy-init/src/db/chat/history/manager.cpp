#include "chat_history_manager.h"
#include <QDateTime>
#include <QUuid>
#include <QDebug>

namespace RawrXD {
namespace Database {

ChatHistoryManager::ChatHistoryManager(std::shared_ptr<DatabaseManager> dbManager)
    : m_dbManager(dbManager)
{
}

bool ChatHistoryManager::initialize() {
    if (m_initialized) return true;
    if (!m_dbManager) return false;

    // Create tables if they don't exist
    bool s1 = m_dbManager->executeMutation(
        "CREATE TABLE IF NOT EXISTS chat_sessions ("
        "id TEXT PRIMARY KEY, "
        "title TEXT, "
        "created_at INTEGER"
        ")", {}, "sqlite");

    bool s2 = m_dbManager->executeMutation(
        "CREATE TABLE IF NOT EXISTS chat_messages ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "session_id TEXT, "
        "role TEXT, "
        "content TEXT, "
        "timestamp INTEGER, "
        "FOREIGN KEY(session_id) REFERENCES chat_sessions(id)"
        ")", {}, "sqlite");

    // Create checkpoints table
    bool s3 = m_dbManager->executeMutation(
        "CREATE TABLE IF NOT EXISTS chat_checkpoints ("
        "id TEXT PRIMARY KEY, "
        "session_id TEXT, "
        "title TEXT, "
        "created_at INTEGER, "
        "message_count INTEGER, "
        "FOREIGN KEY(session_id) REFERENCES chat_sessions(id)"
        ")", {}, "sqlite");

    // Create checkpoint messages table (snapshot of messages at checkpoint time)
    bool s4 = m_dbManager->executeMutation(
        "CREATE TABLE IF NOT EXISTS checkpoint_messages ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "checkpoint_id TEXT, "
        "role TEXT, "
        "content TEXT, "
        "timestamp INTEGER, "
        "FOREIGN KEY(checkpoint_id) REFERENCES chat_checkpoints(id)"
        ")", {}, "sqlite");

    m_initialized = s1 && s2 && s3 && s4;
    return m_initialized;
}

QString ChatHistoryManager::createSession(const QString& title) {
    if (!initialize()) return "";

    QString sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    bool success = m_dbManager->executeMutation(
        "INSERT INTO chat_sessions (id, title, created_at) VALUES (?, ?, ?)",
        {sessionId, title, now}, "sqlite");

    return success ? sessionId : "";
}

QJsonArray ChatHistoryManager::getSessions() {
    if (!initialize()) return QJsonArray();

    QueryResult result = m_dbManager->executeQuery(
        "SELECT * FROM chat_sessions ORDER BY created_at DESC", {}, "sqlite");

    QJsonArray sessions;
    for (const auto& row : result.rows) {
        QJsonObject obj;
        QVariantMap map = row.toMap();
        obj["id"] = map["id"].toString();
        obj["title"] = map["title"].toString();
        obj["created_at"] = map["created_at"].toLongLong();
        sessions.append(obj);
    }
    return sessions;
}

bool ChatHistoryManager::deleteSession(const QString& sessionId) {
    if (!initialize()) return false;

    // Delete messages first due to foreign key (though SQLite might not enforce it by default)
    m_dbManager->executeMutation("DELETE FROM chat_messages WHERE session_id = ?", {sessionId}, "sqlite");
    return m_dbManager->executeMutation("DELETE FROM chat_sessions WHERE id = ?", {sessionId}, "sqlite");
}

bool ChatHistoryManager::addMessage(const QString& sessionId, const QString& role, const QString& content) {
    if (!initialize()) return false;

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    return m_dbManager->executeMutation(
        "INSERT INTO chat_messages (session_id, role, content, timestamp) VALUES (?, ?, ?, ?)",
        {sessionId, role, content, now}, "sqlite");
}

QJsonArray ChatHistoryManager::getMessages(const QString& sessionId) {
    if (!initialize()) return QJsonArray();

    QueryResult result = m_dbManager->executeQuery(
        "SELECT role, content, timestamp FROM chat_messages WHERE session_id = ? ORDER BY timestamp ASC",
        {sessionId}, "sqlite");

    QJsonArray messages;
    for (const auto& row : result.rows) {
        QJsonObject obj;
        QVariantMap map = row.toMap();
        obj["role"] = map["role"].toString();
        obj["content"] = map["content"].toString();
        obj["timestamp"] = map["timestamp"].toLongLong();
        messages.append(obj);
    }
    return messages;
}

QString ChatHistoryManager::createCheckpoint(const QString& sessionId, const QString& title) {
    if (!initialize()) return "";

    QString checkpointId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    
    // Get current messages count
    QueryResult countResult = m_dbManager->executeQuery(
        "SELECT COUNT(*) as count FROM chat_messages WHERE session_id = ?",
        {sessionId}, "sqlite");
    
    int messageCount = 0;
    if (!countResult.rows.isEmpty()) {
        messageCount = countResult.rows[0].toMap()["count"].toInt();
    }
    
    // Generate checkpoint title if not provided
    QString checkpointTitle = title;
    if (checkpointTitle.isEmpty()) {
        checkpointTitle = QString("Checkpoint at %1").arg(
            QDateTime::fromMSecsSinceEpoch(now).toString("yyyy-MM-dd hh:mm:ss"));
    }
    
    // Create checkpoint record
    bool success = m_dbManager->executeMutation(
        "INSERT INTO chat_checkpoints (id, session_id, title, created_at, message_count) VALUES (?, ?, ?, ?, ?)",
        {checkpointId, sessionId, checkpointTitle, now, messageCount}, "sqlite");
    
    if (!success) return "";
    
    // Snapshot current messages into checkpoint_messages
    QueryResult messages = m_dbManager->executeQuery(
        "SELECT role, content, timestamp FROM chat_messages WHERE session_id = ? ORDER BY timestamp ASC",
        {sessionId}, "sqlite");
    
    for (const auto& row : messages.rows) {
        QVariantMap map = row.toMap();
        m_dbManager->executeMutation(
            "INSERT INTO checkpoint_messages (checkpoint_id, role, content, timestamp) VALUES (?, ?, ?, ?)",
            {checkpointId, map["role"], map["content"], map["timestamp"]}, "sqlite");
    }
    
    // Update last checkpoint time for auto-checkpoint tracking
    m_lastCheckpointTime[sessionId] = now;
    
    qInfo() << "[ChatHistoryManager] Created checkpoint:" << checkpointTitle 
            << "for session:" << sessionId << "with" << messageCount << "messages";
    
    return checkpointId;
}

QJsonArray ChatHistoryManager::getCheckpoints(const QString& sessionId) {
    if (!initialize()) return QJsonArray();

    QueryResult result = m_dbManager->executeQuery(
        "SELECT * FROM chat_checkpoints WHERE session_id = ? ORDER BY created_at DESC",
        {sessionId}, "sqlite");

    QJsonArray checkpoints;
    for (const auto& row : result.rows) {
        QJsonObject obj;
        QVariantMap map = row.toMap();
        obj["id"] = map["id"].toString();
        obj["session_id"] = map["session_id"].toString();
        obj["title"] = map["title"].toString();
        obj["created_at"] = map["created_at"].toLongLong();
        obj["message_count"] = map["message_count"].toInt();
        checkpoints.append(obj);
    }
    return checkpoints;
}

bool ChatHistoryManager::restoreCheckpoint(const QString& checkpointId) {
    if (!initialize()) return false;

    // Get checkpoint info
    QueryResult cpResult = m_dbManager->executeQuery(
        "SELECT session_id FROM chat_checkpoints WHERE id = ?",
        {checkpointId}, "sqlite");
    
    if (cpResult.rows.isEmpty()) {
        qWarning() << "[ChatHistoryManager] Checkpoint not found:" << checkpointId;
        return false;
    }
    
    QString sessionId = cpResult.rows[0].toMap()["session_id"].toString();
    
    // Delete all current messages for this session
    m_dbManager->executeMutation(
        "DELETE FROM chat_messages WHERE session_id = ?",
        {sessionId}, "sqlite");
    
    // Restore messages from checkpoint snapshot
    QueryResult messages = m_dbManager->executeQuery(
        "SELECT role, content, timestamp FROM checkpoint_messages WHERE checkpoint_id = ? ORDER BY timestamp ASC",
        {checkpointId}, "sqlite");
    
    for (const auto& row : messages.rows) {
        QVariantMap map = row.toMap();
        m_dbManager->executeMutation(
            "INSERT INTO chat_messages (session_id, role, content, timestamp) VALUES (?, ?, ?, ?)",
            {sessionId, map["role"], map["content"], map["timestamp"]}, "sqlite");
    }
    
    qInfo() << "[ChatHistoryManager] Restored checkpoint:" << checkpointId 
            << "with" << messages.rows.size() << "messages";
    
    return true;
}

bool ChatHistoryManager::deleteCheckpoint(const QString& checkpointId) {
    if (!initialize()) return false;

    // Delete checkpoint messages first
    m_dbManager->executeMutation(
        "DELETE FROM checkpoint_messages WHERE checkpoint_id = ?",
        {checkpointId}, "sqlite");
    
    // Delete checkpoint record
    return m_dbManager->executeMutation(
        "DELETE FROM chat_checkpoints WHERE id = ?",
        {checkpointId}, "sqlite");
}

QString ChatHistoryManager::getLatestCheckpoint(const QString& sessionId) {
    if (!initialize()) return "";

    QueryResult result = m_dbManager->executeQuery(
        "SELECT id FROM chat_checkpoints WHERE session_id = ? ORDER BY created_at DESC LIMIT 1",
        {sessionId}, "sqlite");
    
    if (result.rows.isEmpty()) return "";
    
    return result.rows[0].toMap()["id"].toString();
}

void ChatHistoryManager::setAutoCheckpointInterval(int minutes) {
    m_autoCheckpointMinutes = qMax(1, minutes);  // Minimum 1 minute
    qDebug() << "[ChatHistoryManager] Auto-checkpoint interval set to" << m_autoCheckpointMinutes << "minutes";
}

bool ChatHistoryManager::shouldAutoCheckpoint(const QString& sessionId) {
    if (!m_lastCheckpointTime.contains(sessionId)) {
        return true;  // No checkpoint yet, create first one
    }
    
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 lastCheckpoint = m_lastCheckpointTime[sessionId];
    qint64 elapsedMinutes = (now - lastCheckpoint) / 60000;  // Convert ms to minutes
    
    return elapsedMinutes >= m_autoCheckpointMinutes;
}

QString ChatHistoryManager::autoCheckpoint(const QString& sessionId) {
    if (!shouldAutoCheckpoint(sessionId)) {
        return "";  // Not time yet
    }
    
    return createCheckpoint(sessionId, QString("Auto-checkpoint"));
}

} // namespace Database
} // namespace RawrXD
