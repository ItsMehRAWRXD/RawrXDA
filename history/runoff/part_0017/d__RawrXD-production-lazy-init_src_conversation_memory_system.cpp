/**
 * @file conversation_memory_system.cpp
 * @brief Conversation memory and session management implementation
 */

#include "conversation_memory_system.h"
#include <QTimer>
#include <QUuid>
#include <QJsonDocument>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <algorithm>

// ============================================================================
// Message Implementation
// ============================================================================

QJsonObject Message::toJson() const
{
    QJsonObject json;
    json["id"] = id;
    json["role"] = static_cast<int>(role);
    json["content"] = content;
    json["author"] = author;
    json["timestamp"] = timestamp.toSecsSinceEpoch();
    json["tokenCount"] = tokenCount;
    json["metadata"] = metadata;
    json["parentMessageId"] = parentMessageId;
    return json;
}

Message Message::fromJson(const QJsonObject& json)
{
    Message msg;
    msg.id = json["id"].toString();
    msg.role = static_cast<Role>(json["role"].toInt());
    msg.content = json["content"].toString();
    msg.author = json["author"].toString();
    msg.timestamp = QDateTime::fromSecsSinceEpoch(json["timestamp"].toInt());
    msg.tokenCount = json["tokenCount"].toInt();
    msg.metadata = json["metadata"].toObject();
    msg.parentMessageId = json["parentMessageId"].toString();
    return msg;
}

// ============================================================================
// ConversationSession Implementation
// ============================================================================

ConversationSession::ConversationSession(const QString& sessionId)
    : m_sessionId(sessionId.isEmpty() ? QUuid::createUuid().toString() : sessionId),
      m_title("New Conversation"),
      m_createdTime(QDateTime::currentDateTime()),
      m_lastModifiedTime(QDateTime::currentDateTime()),
      m_totalTokens(0)
{
    qInfo() << "[ConversationSession] Created session:" << m_sessionId;
}

void ConversationSession::addMessage(const Message& message)
{
    QMutexLocker locker(&m_mutex);
    m_messages.push_back(message);
    m_totalTokens += message.tokenCount;
    m_lastModifiedTime = QDateTime::currentDateTime();
}

void ConversationSession::insertMessage(int index, const Message& message)
{
    QMutexLocker locker(&m_mutex);
    if (index >= 0 && index <= (int)m_messages.size()) {
        m_messages.insert(m_messages.begin() + index, message);
        m_totalTokens += message.tokenCount;
        m_lastModifiedTime = QDateTime::currentDateTime();
    }
}

void ConversationSession::removeMessage(const QString& messageId)
{
    QMutexLocker locker(&m_mutex);
    auto it = std::find_if(m_messages.begin(), m_messages.end(),
        [&messageId](const Message& msg) { return msg.id == messageId; });
    
    if (it != m_messages.end()) {
        m_totalTokens -= it->tokenCount;
        m_messages.erase(it);
        m_lastModifiedTime = QDateTime::currentDateTime();
    }
}

Message ConversationSession::getMessage(const QString& messageId) const
{
    QMutexLocker locker(&m_mutex);
    for (const auto& msg : m_messages) {
        if (msg.id == messageId) {
            return msg;
        }
    }
    return Message();
}

std::vector<Message> ConversationSession::getMessages(int limit, int offset) const
{
    QMutexLocker locker(&m_mutex);
    std::vector<Message> result;
    
    int startIdx = offset;
    int endIdx = (limit < 0) ? m_messages.size() : std::min((int)m_messages.size(), offset + limit);
    
    if (startIdx >= 0 && startIdx < (int)m_messages.size()) {
        result.assign(m_messages.begin() + startIdx, m_messages.begin() + endIdx);
    }
    
    return result;
}

std::vector<Message> ConversationSession::getMessagesSince(const QDateTime& timestamp) const
{
    QMutexLocker locker(&m_mutex);
    std::vector<Message> result;
    
    for (const auto& msg : m_messages) {
        if (msg.timestamp >= timestamp) {
            result.push_back(msg);
        }
    }
    
    return result;
}

QJsonObject ConversationSession::toJson() const
{
    QMutexLocker locker(&m_mutex);
    QJsonObject json;
    json["sessionId"] = m_sessionId;
    json["title"] = m_title;
    json["createdTime"] = m_createdTime.toString(Qt::ISODate);
    json["lastModifiedTime"] = m_lastModifiedTime.toString(Qt::ISODate);
    json["totalTokens"] = m_totalTokens;
    json["messageCount"] = static_cast<int>(m_messages.size());
    json["summary"] = m_summary;
    
    QJsonArray messagesArray;
    for (const auto& msg : m_messages) {
        messagesArray.append(msg.toJson());
    }
    json["messages"] = messagesArray;
    
    return json;
}

ConversationSession ConversationSession::fromJson(const QJsonObject& json)
{
    ConversationSession session(json["sessionId"].toString());
    session.m_title = json["title"].toString();
    session.m_totalTokens = json["totalTokens"].toInt();
    session.m_summary = json["summary"].toString();
    
    QJsonArray messages = json["messages"].toArray();
    for (const auto& msgValue : messages) {
        session.m_messages.push_back(Message::fromJson(msgValue.toObject()));
    }
    
    return session;
}

// ============================================================================
// ConversationContextWindow Implementation
// ============================================================================

ConversationContextWindow::ConversationContextWindow(int maxTokens)
    : m_maxTokens(maxTokens), m_strategy(CompressionStrategy::NONE)
{
}

QString ConversationContextWindow::buildContextString(const std::vector<Message>& messages, int maxTokens)
{
    if (maxTokens < 0) maxTokens = m_maxTokens;
    
    QString context;
    int tokenCount = 0;
    
    for (int i = messages.size() - 1; i >= 0; --i) {
        int msgTokens = messages[i].tokenCount;
        if (tokenCount + msgTokens > maxTokens && !context.isEmpty()) {
            context = "[... truncated earlier messages ...]\n" + context;
            break;
        }
        
        QString roleStr;
        switch (messages[i].role) {
            case Message::Role::USER: roleStr = "User"; break;
            case Message::Role::ASSISTANT: roleStr = "Assistant"; break;
            case Message::Role::SYSTEM: roleStr = "System"; break;
            case Message::Role::TOOL: roleStr = "Tool"; break;
        }
        
        context = QString("%1: %2\n").arg(roleStr, messages[i].content) + context;
        tokenCount += msgTokens;
    }
    
    return context;
}

QJsonArray ConversationContextWindow::buildContextArray(const std::vector<Message>& messages, int maxTokens)
{
    if (maxTokens < 0) maxTokens = m_maxTokens;
    
    QJsonArray context;
    int tokenCount = 0;
    bool truncated = false;
    
    for (int i = messages.size() - 1; i >= 0; --i) {
        int msgTokens = messages[i].tokenCount;
        if (tokenCount + msgTokens > maxTokens && !context.isEmpty()) {
            truncated = true;
            break;
        }
        
        context.prepend(messages[i].toJson());
        tokenCount += msgTokens;
    }
    
    if (truncated) {
        QJsonObject truncationNotice;
        truncationNotice["role"] = static_cast<int>(Message::Role::SYSTEM);
        truncationNotice["content"] = "[Earlier messages truncated due to context window limit]";
        context.prepend(truncationNotice);
    }
    
    return context;
}

void ConversationContextWindow::setMaxTokens(int maxTokens)
{
    m_maxTokens = maxTokens;
}

int ConversationContextWindow::estimateTokens(const QString& text) const
{
    // Rough approximation: ~4 characters per token on average
    return (text.length() + 3) / 4;
}

QString ConversationContextWindow::compressMessages(const std::vector<Message>& messages) const
{
    switch (m_strategy) {
        case CompressionStrategy::NONE:
            return "";
        case CompressionStrategy::SUMMARIZE:
            return summarizeMessages(messages);
        case CompressionStrategy::EXTRACT_SUMMARY:
            return summarizeMessages(messages);
        case CompressionStrategy::COMPRESS_MESSAGES:
            return summarizeMessages(messages);
    }
    return "";
}

int ConversationContextWindow::countTokensRough(const QString& text) const
{
    return estimateTokens(text);
}

QString ConversationContextWindow::summarizeMessages(const std::vector<Message>& messages) const
{
    if (messages.empty()) return "";
    
    // Simple summarization: extract key points
    QString summary = "Conversation summary:\n";
    int userMsgCount = 0;
    int assistantMsgCount = 0;
    
    for (const auto& msg : messages) {
        if (msg.role == Message::Role::USER) {
            userMsgCount++;
            summary += QString("- User asked about: %1\n").arg(msg.content.left(100));
        } else if (msg.role == Message::Role::ASSISTANT) {
            assistantMsgCount++;
        }
    }
    
    summary += QString("\nTotal turns: User messages: %1, Assistant responses: %2\n")
        .arg(userMsgCount).arg(assistantMsgCount);
    
    return summary;
}

// ============================================================================
// ConversationMemoryManager Implementation
// ============================================================================

ConversationMemoryManager::ConversationMemoryManager(QObject* parent)
    : QObject(parent),
      m_contextWindow(4096),
      m_compressionThreshold(8000),
      m_maxSessionAgeDays(30)
{
    // Set default persistence path
    m_persistencePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                       + "/conversations";
    QDir().mkpath(m_persistencePath);
    
    // Start maintenance timer (runs every hour)
    m_maintenanceTimer = new QTimer(this);
    connect(m_maintenanceTimer, &QTimer::timeout, this, &ConversationMemoryManager::onMaintenanceTimer);
    m_maintenanceTimer->start(3600000);
    
    qInfo() << "[ConversationMemoryManager] Initialized with persistence path:" << m_persistencePath;
}

ConversationMemoryManager::~ConversationMemoryManager()
{
    if (m_maintenanceTimer) {
        m_maintenanceTimer->stop();
    }
}

QString ConversationMemoryManager::createSession(const QString& title)
{
    QMutexLocker locker(&m_mutex);
    
    auto session = std::make_unique<ConversationSession>();
    if (!title.isEmpty()) {
        session->setTitle(title);
    }
    
    QString sessionId = session->getSessionId();
    m_sessions[sessionId] = std::move(session);
    m_currentSession = std::make_unique<ConversationSession>(sessionId);
    
    emit sessionCreated(sessionId);
    qInfo() << "[ConversationMemoryManager] Created session:" << sessionId;
    
    return sessionId;
}

bool ConversationMemoryManager::loadSession(const QString& sessionId)
{
    QMutexLocker locker(&m_mutex);
    
    auto it = m_sessions.find(sessionId);
    if (it != m_sessions.end()) {
        m_currentSession = std::make_unique<ConversationSession>(*it.value());
        return true;
    }
    
    // Try loading from disk
    return loadFromDisk(sessionId);
}

bool ConversationMemoryManager::saveSession(const QString& sessionId)
{
    QMutexLocker locker(&m_mutex);
    
    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end()) {
        return false;
    }
    
    return saveToDisk(sessionId);
}

bool ConversationMemoryManager::deleteSession(const QString& sessionId)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_sessions.remove(sessionId) > 0) {
        emit sessionDeleted(sessionId);
        
        // Also delete from disk
        QString filePath = m_persistencePath + "/" + sessionId + ".json";
        QFile::remove(filePath);
        
        qInfo() << "[ConversationMemoryManager] Deleted session:" << sessionId;
        return true;
    }
    
    return false;
}

std::vector<QString> ConversationMemoryManager::listSessions() const
{
    QMutexLocker locker(&m_mutex);
    std::vector<QString> result;
    
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        result.push_back(it.key());
    }
    
    return result;
}

void ConversationMemoryManager::addMessage(const Message& message)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_currentSession) {
        m_currentSession->addMessage(message);
        emit messageAdded(m_currentSession->getSessionId(), message.id);
        
        // Check if compression is needed
        if (m_currentSession->getTotalTokens() > m_compressionThreshold) {
            compressMemory(m_currentSession->getSessionId());
        }
    }
}

Message ConversationMemoryManager::getLastMessage() const
{
    QMutexLocker locker(&m_mutex);
    
    if (m_currentSession) {
        auto messages = m_currentSession->getMessages();
        if (!messages.empty()) {
            return messages.back();
        }
    }
    
    return Message();
}

std::vector<Message> ConversationMemoryManager::getConversationHistory(int limit) const
{
    QMutexLocker locker(&m_mutex);
    
    if (m_currentSession) {
        return m_currentSession->getMessages(limit);
    }
    
    return std::vector<Message>();
}

QString ConversationMemoryManager::buildContext(int maxTokens)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_currentSession) {
        auto messages = m_currentSession->getMessages();
        return m_contextWindow.buildContextString(messages, maxTokens);
    }
    
    return "";
}

QJsonArray ConversationMemoryManager::buildContextJson(int maxTokens)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_currentSession) {
        auto messages = m_currentSession->getMessages();
        return m_contextWindow.buildContextArray(messages, maxTokens);
    }
    
    return QJsonArray();
}

void ConversationMemoryManager::compressMemory(const QString& sessionId)
{
    QMutexLocker locker(&m_mutex);
    
    auto it = m_sessions.find(sessionId);
    if (it != m_sessions.end()) {
        auto messages = it.value()->getMessages();
        QString summary = m_contextWindow.compressMessages(messages);
        it.value()->setSummary(summary);
        
        emit sessionCompressed(sessionId);
        qInfo() << "[ConversationMemoryManager] Compressed session:" << sessionId;
    }
}

void ConversationMemoryManager::cleanupOldSessions(int daysOld)
{
    QMutexLocker locker(&m_mutex);
    
    QDateTime cutoff = QDateTime::currentDateTime().addDays(-daysOld);
    auto it = m_sessions.begin();
    
    while (it != m_sessions.end()) {
        if (it.value()->getLastModifiedTime() < cutoff) {
            saveToDisk(it.key());
            it = m_sessions.erase(it);
        } else {
            ++it;
        }
    }
}

ConversationMemoryManager::SessionStats ConversationMemoryManager::getSessionStats(const QString& sessionId) const
{
    QMutexLocker locker(&m_mutex);
    
    SessionStats stats{};
    auto it = m_sessions.find(sessionId);
    if (it != m_sessions.end()) {
        stats.sessionId = sessionId;
        stats.messageCount = it.value()->getMessageCount();
        stats.totalTokens = it.value()->getTotalTokens();
        stats.createdTime = it.value()->getCreatedTime();
        stats.lastModifiedTime = it.value()->getLastModifiedTime();
    }
    
    return stats;
}

std::vector<ConversationMemoryManager::SessionStats> ConversationMemoryManager::getAllSessionStats() const
{
    QMutexLocker locker(&m_mutex);
    std::vector<SessionStats> result;
    
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        SessionStats stats;
        stats.sessionId = it.key();
        stats.messageCount = it.value()->getMessageCount();
        stats.totalTokens = it.value()->getTotalTokens();
        stats.createdTime = it.value()->getCreatedTime();
        stats.lastModifiedTime = it.value()->getLastModifiedTime();
        result.push_back(stats);
    }
    
    return result;
}

void ConversationMemoryManager::setPersistencePath(const QString& path)
{
    QMutexLocker locker(&m_mutex);
    m_persistencePath = path;
    QDir().mkpath(path);
}

bool ConversationMemoryManager::saveToDisk(const QString& sessionId)
{
    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end()) {
        return false;
    }
    
    QString filePath = m_persistencePath + "/" + sessionId + ".json";
    QFile file(filePath);
    
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "[ConversationMemoryManager] Failed to save session to:" << filePath;
        return false;
    }
    
    QJsonDocument doc(it.value()->toJson());
    file.write(doc.toJson());
    file.close();
    
    qInfo() << "[ConversationMemoryManager] Saved session to:" << filePath;
    return true;
}

bool ConversationMemoryManager::loadFromDisk(const QString& sessionId)
{
    QString filePath = m_persistencePath + "/" + sessionId + ".json";
    QFile file(filePath);
    
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[ConversationMemoryManager] Failed to load session from:" << filePath;
        return false;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (!doc.isObject()) {
        return false;
    }
    
    auto session = std::make_unique<ConversationSession>(
        ConversationSession::fromJson(doc.object()));
    m_currentSession = std::move(session);
    
    qInfo() << "[ConversationMemoryManager] Loaded session from:" << filePath;
    return true;
}

void ConversationMemoryManager::setContextWindowSize(int maxTokens)
{
    QMutexLocker locker(&m_mutex);
    m_contextWindow.setMaxTokens(maxTokens);
    emit contextWindowUpdated(0, maxTokens);
}

void ConversationMemoryManager::setCompressionThreshold(int tokenCount)
{
    QMutexLocker locker(&m_mutex);
    m_compressionThreshold = tokenCount;
}

void ConversationMemoryManager::setMaxSessionAge(int daysOld)
{
    QMutexLocker locker(&m_mutex);
    m_maxSessionAgeDays = daysOld;
}

void ConversationMemoryManager::onMaintenanceTimer()
{
    cleanupOldSessions(m_maxSessionAgeDays);
}
