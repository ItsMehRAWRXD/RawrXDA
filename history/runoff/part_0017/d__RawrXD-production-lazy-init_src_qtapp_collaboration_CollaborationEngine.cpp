#include "CollaborationEngine.h"
#include <QJsonDocument>
#include <QUuid>
#include <QDebug>
#include <algorithm>

// ============ UserInfo Serialization ============

QJsonObject CollaborationEngine::UserInfo::toJson() const {
    QJsonObject obj;
    obj["userId"] = userId;
    obj["displayName"] = displayName;
    obj["email"] = email;
    obj["cursorColor"] = cursorColor.name();
    obj["state"] = static_cast<int>(state);
    obj["lastActivity"] = lastActivity.toString(Qt::ISODate);
    obj["currentFile"] = currentFile;
    obj["cursorPosition"] = cursorPosition;
    obj["isTyping"] = isTyping;
    return obj;
}

CollaborationEngine::UserInfo CollaborationEngine::UserInfo::fromJson(const QJsonObject& obj) {
    UserInfo user;
    user.userId = obj["userId"].toString();
    user.displayName = obj["displayName"].toString();
    user.email = obj["email"].toString();
    user.cursorColor = QColor(obj["cursorColor"].toString());
    user.state = static_cast<PresenceState>(obj["state"].toInt());
    user.lastActivity = QDateTime::fromString(obj["lastActivity"].toString(), Qt::ISODate);
    user.currentFile = obj["currentFile"].toString();
    user.cursorPosition = obj["cursorPosition"].toInt();
    user.isTyping = obj["isTyping"].toBool();
    return user;
}

// ============ EditOperation Serialization ============

QJsonObject CollaborationEngine::EditOperation::toJson() const {
    QJsonObject obj;
    obj["operationId"] = operationId;
    obj["type"] = static_cast<int>(type);
    obj["userId"] = userId;
    obj["fileId"] = fileId;
    obj["position"] = position;
    obj["length"] = length;
    obj["content"] = content;
    obj["timestamp"] = timestamp.toString(Qt::ISODate);
    obj["version"] = version;
    return obj;
}

CollaborationEngine::EditOperation CollaborationEngine::EditOperation::fromJson(const QJsonObject& obj) {
    EditOperation op;
    op.operationId = obj["operationId"].toString();
    op.type = static_cast<OperationType>(obj["type"].toInt());
    op.userId = obj["userId"].toString();
    op.fileId = obj["fileId"].toString();
    op.position = obj["position"].toInt();
    op.length = obj["length"].toInt();
    op.content = obj["content"].toString();
    op.timestamp = QDateTime::fromString(obj["timestamp"].toString(), Qt::ISODate);
    op.version = obj["version"].toInt();
    return op;
}

// ============ ConflictInfo Serialization ============

QJsonObject CollaborationEngine::ConflictInfo::toJson() const {
    QJsonObject obj;
    obj["conflictId"] = conflictId;
    obj["localEdit"] = localEdit.toJson();
    obj["remoteEdit"] = remoteEdit.toJson();
    obj["commonAncestor"] = commonAncestor;
    obj["detectedAt"] = detectedAt.toString(Qt::ISODate);
    obj["resolved"] = resolved;
    return obj;
}

// ============ CollaborationSession Serialization ============

QJsonObject CollaborationEngine::CollaborationSession::toJson() const {
    QJsonObject obj;
    obj["sessionId"] = sessionId;
    obj["hostUserId"] = hostUserId;
    obj["projectName"] = projectName;
    obj["createdAt"] = createdAt.toString(Qt::ISODate);
    obj["lastActivity"] = lastActivity.toString(Qt::ISODate);
    
    QJsonArray participants;
    for (const QString& id : participantIds) {
        participants.append(id);
    }
    obj["participants"] = participants;
    
    QJsonObject versions;
    for (auto it = fileVersions.begin(); it != fileVersions.end(); ++it) {
        versions[it.key()] = it.value();
    }
    obj["fileVersions"] = versions;
    
    obj["isActive"] = isActive;
    obj["maxParticipants"] = maxParticipants;
    
    return obj;
}

CollaborationEngine::CollaborationSession CollaborationEngine::CollaborationSession::fromJson(const QJsonObject& obj) {
    CollaborationSession session;
    session.sessionId = obj["sessionId"].toString();
    session.hostUserId = obj["hostUserId"].toString();
    session.projectName = obj["projectName"].toString();
    session.createdAt = QDateTime::fromString(obj["createdAt"].toString(), Qt::ISODate);
    session.lastActivity = QDateTime::fromString(obj["lastActivity"].toString(), Qt::ISODate);
    
    QJsonArray participants = obj["participants"].toArray();
    for (const QJsonValue& val : participants) {
        session.participantIds.append(val.toString());
    }
    
    QJsonObject versions = obj["fileVersions"].toObject();
    for (auto it = versions.begin(); it != versions.end(); ++it) {
        session.fileVersions[it.key()] = it.value().toInt();
    }
    
    session.isActive = obj["isActive"].toBool();
    session.maxParticipants = obj["maxParticipants"].toInt();
    
    return session;
}

// ============ CollaborationEngine Implementation ============

CollaborationEngine::CollaborationEngine(QObject* parent)
    : QObject(parent)
    , m_webSocket(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this))
    , m_heartbeatTimer(new QTimer(this))
    , m_reconnectTimer(new QTimer(this))
    , m_conflictStrategy(ConflictStrategy::Merge)
    , m_isConnected(false)
    , m_autoReconnect(true)
    , m_reconnectAttempts(0)
    , m_maxRetries(5)
    , m_heartbeatInterval(30000)
    , m_totalEditOperations(0)
{
    // Setup WebSocket connections
    connect(m_webSocket, &QWebSocket::connected, this, &CollaborationEngine::onWebSocketConnected);
    connect(m_webSocket, &QWebSocket::disconnected, this, &CollaborationEngine::onWebSocketDisconnected);
    connect(m_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            this, &CollaborationEngine::onWebSocketError);
    connect(m_webSocket, &QWebSocket::textMessageReceived, this, &CollaborationEngine::onWebSocketTextMessageReceived);
    
    // Setup heartbeat timer
    m_heartbeatTimer->setInterval(m_heartbeatInterval);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &CollaborationEngine::onHeartbeatTimeout);
    
    // Setup reconnect timer
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &CollaborationEngine::attemptReconnect);
}

CollaborationEngine::~CollaborationEngine() {
    if (m_isConnected) {
        disconnectFromServer();
    }
}

// ============ Session Management ============

QString CollaborationEngine::createSession(const QString& projectName, int maxParticipants) {
    QMutexLocker locker(&m_mutex);
    
    CollaborationSession session;
    session.sessionId = QUuid::createUuid().toString();
    session.hostUserId = m_currentUserId;
    session.projectName = projectName;
    session.createdAt = QDateTime::currentDateTime();
    session.lastActivity = QDateTime::currentDateTime();
    session.isActive = true;
    session.maxParticipants = maxParticipants;
    
    m_sessions[session.sessionId] = session;
    m_currentSessionId = session.sessionId;
    
    // Notify server
    QJsonObject message;
    message["type"] = "session_create";
    message["sessionId"] = session.sessionId;
    message["projectName"] = projectName;
    message["maxParticipants"] = maxParticipants;
    
    locker.unlock();
    sendMessage(message);
    
    emit sessionCreated(session.sessionId);
    
    qDebug() << "Session created:" << session.sessionId;
    
    return session.sessionId;
}

bool CollaborationEngine::joinSession(const QString& sessionId, const UserInfo& user) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_sessions.contains(sessionId)) {
        qWarning() << "Session not found:" << sessionId;
        return false;
    }
    
    CollaborationSession& session = m_sessions[sessionId];
    
    if (session.participantIds.size() >= session.maxParticipants) {
        qWarning() << "Session full:" << sessionId;
        return false;
    }
    
    session.participantIds.append(user.userId);
    session.lastActivity = QDateTime::currentDateTime();
    
    m_users[user.userId] = user;
    m_currentUserId = user.userId;
    m_currentSessionId = sessionId;
    
    // Notify server
    QJsonObject message;
    message["type"] = "session_join";
    message["sessionId"] = sessionId;
    message["user"] = user.toJson();
    
    locker.unlock();
    sendMessage(message);
    
    emit sessionJoined(sessionId, user);
    emit userJoined(user);
    
    qDebug() << "User joined session:" << user.displayName << "in" << sessionId;
    
    return true;
}

void CollaborationEngine::leaveSession(const QString& sessionId) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_sessions.contains(sessionId)) {
        return;
    }
    
    CollaborationSession& session = m_sessions[sessionId];
    session.participantIds.removeAll(m_currentUserId);
    
    // Notify server
    QJsonObject message;
    message["type"] = "session_leave";
    message["sessionId"] = sessionId;
    message["userId"] = m_currentUserId;
    
    locker.unlock();
    sendMessage(message);
    
    emit sessionLeft(sessionId, m_currentUserId);
    emit userLeft(m_currentUserId);
    
    qDebug() << "User left session:" << sessionId;
}

void CollaborationEngine::endSession(const QString& sessionId) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_sessions.contains(sessionId)) {
        return;
    }
    
    m_sessions[sessionId].isActive = false;
    
    // Notify all participants
    QJsonObject message;
    message["type"] = "session_end";
    message["sessionId"] = sessionId;
    
    locker.unlock();
    notifySessionParticipants(sessionId, message);
    
    emit sessionEnded(sessionId);
    
    qDebug() << "Session ended:" << sessionId;
}

CollaborationEngine::CollaborationSession CollaborationEngine::getSession(const QString& sessionId) const {
    QMutexLocker locker(&m_mutex);
    return m_sessions.value(sessionId);
}

QList<CollaborationEngine::CollaborationSession> CollaborationEngine::getActiveSessions() const {
    QMutexLocker locker(&m_mutex);
    
    QList<CollaborationSession> active;
    for (const CollaborationSession& session : m_sessions) {
        if (session.isActive) {
            active.append(session);
        }
    }
    
    return active;
}

// ============ User Management ============

void CollaborationEngine::registerUser(const UserInfo& user) {
    QMutexLocker locker(&m_mutex);
    
    m_users[user.userId] = user;
    m_currentUserId = user.userId;
    
    qDebug() << "User registered:" << user.displayName;
}

void CollaborationEngine::updateUserPresence(const QString& userId, PresenceState state) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_users.contains(userId)) {
        qWarning() << "User not found:" << userId;
        return;
    }
    
    m_users[userId].state = state;
    m_users[userId].lastActivity = QDateTime::currentDateTime();
    
    // Broadcast presence update
    QJsonObject message;
    message["type"] = "presence_update";
    message["userId"] = userId;
    message["state"] = static_cast<int>(state);
    
    locker.unlock();
    sendMessage(message);
    
    emit userPresenceChanged(userId, state);
}

void CollaborationEngine::updateUserCursor(const QString& userId, const QString& fileId, int position) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_users.contains(userId)) {
        return;
    }
    
    m_users[userId].currentFile = fileId;
    m_users[userId].cursorPosition = position;
    m_users[userId].lastActivity = QDateTime::currentDateTime();
    
    // Broadcast cursor update
    QJsonObject message;
    message["type"] = "cursor_update";
    message["userId"] = userId;
    message["fileId"] = fileId;
    message["position"] = position;
    
    locker.unlock();
    sendMessage(message);
    
    emit userCursorMoved(userId, fileId, position);
}

void CollaborationEngine::setUserTyping(const QString& userId, bool isTyping) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_users.contains(userId)) {
        return;
    }
    
    m_users[userId].isTyping = isTyping;
    
    // Broadcast typing status
    QJsonObject message;
    message["type"] = "typing_update";
    message["userId"] = userId;
    message["isTyping"] = isTyping;
    
    locker.unlock();
    sendMessage(message);
    
    emit userTypingChanged(userId, isTyping);
}

CollaborationEngine::UserInfo CollaborationEngine::getUserInfo(const QString& userId) const {
    QMutexLocker locker(&m_mutex);
    return m_users.value(userId);
}

QList<CollaborationEngine::UserInfo> CollaborationEngine::getSessionUsers(const QString& sessionId) const {
    QMutexLocker locker(&m_mutex);
    
    QList<UserInfo> users;
    
    if (!m_sessions.contains(sessionId)) {
        return users;
    }
    
    const CollaborationSession& session = m_sessions[sessionId];
    
    for (const QString& userId : session.participantIds) {
        if (m_users.contains(userId)) {
            users.append(m_users[userId]);
        }
    }
    
    return users;
}

// ============ Real-time Editing ============

void CollaborationEngine::broadcastEdit(const EditOperation& operation) {
    QMutexLocker locker(&m_mutex);
    
    // Store in pending operations
    if (!m_pendingOperations.contains(operation.fileId)) {
        m_pendingOperations[operation.fileId] = QList<EditOperation>();
    }
    m_pendingOperations[operation.fileId].append(operation);
    
    // Increment version
    int currentVersion = m_fileVersions.value(operation.fileId, 0);
    m_fileVersions[operation.fileId] = currentVersion + 1;
    
    m_totalEditOperations++;
    
    // Broadcast to all session participants
    QJsonObject message;
    message["type"] = "edit_operation";
    message["operation"] = operation.toJson();
    message["sessionId"] = m_currentSessionId;
    
    locker.unlock();
    sendMessage(message);
    
    emit editBroadcasted(operation);
    emit operationQueued(m_pendingOperations[operation.fileId].size());
    
    qDebug() << "Edit broadcasted:" << operation.operationId;
}

void CollaborationEngine::applyRemoteEdit(const EditOperation& operation) {
    QMutexLocker locker(&m_mutex);
    
    // Check for conflicts with pending local operations
    locker.unlock();
    checkForConflicts(operation);
    locker.relock();
    
    // Apply transformation if needed
    EditOperation transformedOp = operation;
    
    if (m_pendingOperations.contains(operation.fileId)) {
        for (const EditOperation& pendingOp : m_pendingOperations[operation.fileId]) {
            if (pendingOp.timestamp < operation.timestamp) {
                transformedOp = transformOperation(transformedOp, pendingOp);
            }
        }
    }
    
    // Update file content
    QString fileId = transformedOp.fileId;
    QString currentContent = m_sharedFiles.value(fileId, "");
    
    if (transformedOp.type == OperationType::Insert) {
        currentContent.insert(transformedOp.position, transformedOp.content);
    } else if (transformedOp.type == OperationType::Delete) {
        currentContent.remove(transformedOp.position, transformedOp.length);
    } else if (transformedOp.type == OperationType::Replace) {
        currentContent.replace(transformedOp.position, transformedOp.length, transformedOp.content);
    }
    
    m_sharedFiles[fileId] = currentContent;
    
    locker.unlock();
    emit remoteEditReceived(transformedOp);
    emit editApplied(fileId, transformedOp.position, transformedOp.content);
    
    qDebug() << "Remote edit applied:" << transformedOp.operationId;
}

void CollaborationEngine::sendCursorUpdate(const QString& fileId, int position) {
    updateUserCursor(m_currentUserId, fileId, position);
}

void CollaborationEngine::sendSelection(const QString& fileId, int start, int end) {
    QJsonObject message;
    message["type"] = "selection_update";
    message["userId"] = m_currentUserId;
    message["fileId"] = fileId;
    message["start"] = start;
    message["end"] = end;
    
    sendMessage(message);
}

// ============ Operational Transformation ============

CollaborationEngine::EditOperation CollaborationEngine::transformOperation(
    const EditOperation& op1, const EditOperation& op2)
{
    if (op1.fileId != op2.fileId) {
        return op1;  // No transformation needed for different files
    }
    
    EditOperation transformed = op1;
    
    // Insert vs Insert
    if (op1.type == OperationType::Insert && op2.type == OperationType::Insert) {
        if (op2.position <= op1.position) {
            transformed.position += op2.content.length();
        }
    }
    // Insert vs Delete
    else if (op1.type == OperationType::Insert && op2.type == OperationType::Delete) {
        if (op2.position < op1.position) {
            transformed.position -= std::min(op2.length, op1.position - op2.position);
        }
    }
    // Delete vs Insert
    else if (op1.type == OperationType::Delete && op2.type == OperationType::Insert) {
        if (op2.position <= op1.position) {
            transformed.position += op2.content.length();
        }
    }
    // Delete vs Delete
    else if (op1.type == OperationType::Delete && op2.type == OperationType::Delete) {
        if (op2.position < op1.position) {
            transformed.position -= std::min(op2.length, op1.position - op2.position);
        } else if (op2.position < op1.position + op1.length) {
            // Overlapping deletes
            int overlap = std::min(op1.position + op1.length, op2.position + op2.length) - op2.position;
            transformed.length -= overlap;
        }
    }
    
    return transformed;
}

QList<CollaborationEngine::EditOperation> CollaborationEngine::getPendingOperations(const QString& fileId) const {
    QMutexLocker locker(&m_mutex);
    return m_pendingOperations.value(fileId);
}

void CollaborationEngine::acknowledgOperation(const QString& operationId) {
    QMutexLocker locker(&m_mutex);
    
    // Remove acknowledged operation from pending list
    for (auto it = m_pendingOperations.begin(); it != m_pendingOperations.end(); ++it) {
        QList<EditOperation>& ops = it.value();
        ops.erase(std::remove_if(ops.begin(), ops.end(),
                                 [&operationId](const EditOperation& op) {
                                     return op.operationId == operationId;
                                 }),
                  ops.end());
    }
}

// ============ Conflict Resolution ============

void CollaborationEngine::detectConflicts(const QString& fileId) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_pendingOperations.contains(fileId)) {
        return;
    }
    
    const QList<EditOperation>& operations = m_pendingOperations[fileId];
    
    // Check for overlapping operations
    for (int i = 0; i < operations.size(); ++i) {
        for (int j = i + 1; j < operations.size(); ++j) {
            if (operationsConflict(operations[i], operations[j])) {
                ConflictInfo conflict;
                conflict.conflictId = QUuid::createUuid().toString();
                conflict.localEdit = operations[i];
                conflict.remoteEdit = operations[j];
                conflict.detectedAt = QDateTime::currentDateTime();
                conflict.resolved = false;
                
                if (!m_conflicts.contains(fileId)) {
                    m_conflicts[fileId] = QList<ConflictInfo>();
                }
                m_conflicts[fileId].append(conflict);
                
                locker.unlock();
                emit conflictDetected(conflict);
                locker.relock();
            }
        }
    }
}

QList<CollaborationEngine::ConflictInfo> CollaborationEngine::getConflicts(const QString& fileId) const {
    QMutexLocker locker(&m_mutex);
    return m_conflicts.value(fileId);
}

void CollaborationEngine::resolveConflict(const QString& conflictId, ConflictStrategy strategy) {
    QMutexLocker locker(&m_mutex);
    
    // Find conflict
    ConflictInfo* conflict = nullptr;
    QString fileId;
    
    for (auto it = m_conflicts.begin(); it != m_conflicts.end(); ++it) {
        for (ConflictInfo& c : it.value()) {
            if (c.conflictId == conflictId) {
                conflict = &c;
                fileId = it.key();
                break;
            }
        }
        if (conflict) break;
    }
    
    if (!conflict) {
        qWarning() << "Conflict not found:" << conflictId;
        return;
    }
    
    QString resolvedContent;
    
    switch (strategy) {
    case ConflictStrategy::LastWriteWins:
        resolvedContent = conflict->remoteEdit.timestamp > conflict->localEdit.timestamp
                              ? conflict->remoteEdit.content
                              : conflict->localEdit.content;
        break;
        
    case ConflictStrategy::FirstWriteWins:
        resolvedContent = conflict->remoteEdit.timestamp < conflict->localEdit.timestamp
                              ? conflict->remoteEdit.content
                              : conflict->localEdit.content;
        break;
        
    case ConflictStrategy::Merge:
        locker.unlock();
        resolvedContent = computeThreeWayMerge(
            conflict->commonAncestor,
            conflict->localEdit.content,
            conflict->remoteEdit.content
        );
        locker.relock();
        break;
        
    case ConflictStrategy::Manual:
    case ConflictStrategy::ThreeWayMerge:
        // Emit signal for manual resolution
        locker.unlock();
        emit conflictResolutionRequired(*conflict);
        return;
    }
    
    conflict->resolved = true;
    
    locker.unlock();
    emit conflictResolved(conflictId);
    
    qDebug() << "Conflict resolved:" << conflictId;
}

void CollaborationEngine::acceptRemoteChange(const QString& conflictId) {
    resolveConflict(conflictId, ConflictStrategy::LastWriteWins);
}

void CollaborationEngine::acceptLocalChange(const QString& conflictId) {
    resolveConflict(conflictId, ConflictStrategy::FirstWriteWins);
}

void CollaborationEngine::manualMerge(const QString& conflictId, const QString& mergedContent) {
    QMutexLocker locker(&m_mutex);
    
    // Find and resolve conflict with merged content
    for (auto it = m_conflicts.begin(); it != m_conflicts.end(); ++it) {
        for (ConflictInfo& conflict : it.value()) {
            if (conflict.conflictId == conflictId) {
                conflict.resolved = true;
                QString fileId = it.key();
                m_sharedFiles[fileId] = mergedContent;
                
                locker.unlock();
                emit conflictResolved(conflictId);
                emit fileContentChanged(fileId, mergedContent);
                
                qDebug() << "Manual merge completed:" << conflictId;
                return;
            }
        }
    }
}

// ============ File Synchronization ============

void CollaborationEngine::shareFile(const QString& fileId, const QString& content, const QString& sessionId) {
    QMutexLocker locker(&m_mutex);
    
    m_sharedFiles[fileId] = content;
    m_fileVersions[fileId] = 1;
    
    // Notify session participants
    QJsonObject message;
    message["type"] = "file_share";
    message["fileId"] = fileId;
    message["content"] = content;
    message["sessionId"] = sessionId;
    message["version"] = 1;
    
    locker.unlock();
    sendMessage(message);
    
    emit fileShared(fileId, sessionId);
    
    qDebug() << "File shared:" << fileId;
}

void CollaborationEngine::requestFileSync(const QString& fileId) {
    QJsonObject message;
    message["type"] = "file_sync_request";
    message["fileId"] = fileId;
    message["userId"] = m_currentUserId;
    
    sendMessage(message);
}

QString CollaborationEngine::getSharedFileContent(const QString& fileId) const {
    QMutexLocker locker(&m_mutex);
    return m_sharedFiles.value(fileId, "");
}

int CollaborationEngine::getFileVersion(const QString& fileId) const {
    QMutexLocker locker(&m_mutex);
    return m_fileVersions.value(fileId, 0);
}

void CollaborationEngine::setFileVersion(const QString& fileId, int version) {
    QMutexLocker locker(&m_mutex);
    m_fileVersions[fileId] = version;
}

// ============ Connection Management ============

void CollaborationEngine::connectToServer(const QString& serverUrl) {
    QMutexLocker locker(&m_mutex);
    
    m_serverUrl = serverUrl;
    
    locker.unlock();
    
    qDebug() << "Connecting to collaboration server:" << serverUrl;
    m_webSocket->open(QUrl(serverUrl));
}

void CollaborationEngine::disconnectFromServer() {
    m_isConnected = false;
    m_heartbeatTimer->stop();
    m_webSocket->close();
    
    qDebug() << "Disconnected from collaboration server";
}

bool CollaborationEngine::isConnected() const {
    return m_isConnected;
}

QString CollaborationEngine::getConnectionStatus() const {
    if (m_isConnected) {
        return "Connected";
    } else if (m_reconnectTimer->isActive()) {
        return QString("Reconnecting (attempt %1/%2)").arg(m_reconnectAttempts).arg(m_maxRetries);
    } else {
        return "Disconnected";
    }
}

// ============ Configuration ============

void CollaborationEngine::setConflictStrategy(ConflictStrategy strategy) {
    QMutexLocker locker(&m_mutex);
    m_conflictStrategy = strategy;
}

void CollaborationEngine::setHeartbeatInterval(int intervalMs) {
    m_heartbeatInterval = intervalMs;
    m_heartbeatTimer->setInterval(intervalMs);
}

void CollaborationEngine::enableAutoReconnect(bool enable) {
    m_autoReconnect = enable;
}

void CollaborationEngine::setMaxRetries(int maxRetries) {
    m_maxRetries = maxRetries;
}

// ============ Statistics ============

int CollaborationEngine::getActiveUserCount() const {
    QMutexLocker locker(&m_mutex);
    
    int count = 0;
    for (const UserInfo& user : m_users) {
        if (user.state == PresenceState::Online || user.state == PresenceState::Editing) {
            count++;
        }
    }
    
    return count;
}

int CollaborationEngine::getTotalEditOperations() const {
    return m_totalEditOperations;
}

int CollaborationEngine::getConflictCount() const {
    QMutexLocker locker(&m_mutex);
    
    int count = 0;
    for (const QList<ConflictInfo>& conflicts : m_conflicts) {
        for (const ConflictInfo& conflict : conflicts) {
            if (!conflict.resolved) {
                count++;
            }
        }
    }
    
    return count;
}

qint64 CollaborationEngine::getAverageLatency() const {
    QMutexLocker locker(&m_mutex);
    
    if (m_latencyMeasurements.isEmpty()) {
        return 0;
    }
    
    qint64 sum = 0;
    for (qint64 latency : m_latencyMeasurements) {
        sum += latency;
    }
    
    return sum / m_latencyMeasurements.size();
}

QJsonObject CollaborationEngine::getSessionStatistics(const QString& sessionId) const {
    QMutexLocker locker(&m_mutex);
    
    QJsonObject stats;
    
    if (!m_sessions.contains(sessionId)) {
        return stats;
    }
    
    const CollaborationSession& session = m_sessions[sessionId];
    
    stats["sessionId"] = sessionId;
    stats["activeUsers"] = session.participantIds.size();
    stats["totalFiles"] = session.fileVersions.size();
    stats["sessionDuration"] = session.createdAt.secsTo(QDateTime::currentDateTime());
    
    // Count operations for this session
    int operationCount = 0;
    for (const QList<EditOperation>& ops : m_pendingOperations) {
        operationCount += ops.size();
    }
    stats["pendingOperations"] = operationCount;
    
    return stats;
}

// ============ Private Slots ============

void CollaborationEngine::onWebSocketConnected() {
    m_isConnected = true;
    m_reconnectAttempts = 0;
    m_heartbeatTimer->start();
    
    qDebug() << "WebSocket connected to collaboration server";
    
    emit connected();
}

void CollaborationEngine::onWebSocketDisconnected() {
    m_isConnected = false;
    m_heartbeatTimer->stop();
    
    qDebug() << "WebSocket disconnected from collaboration server";
    
    emit disconnected();
    
    // Auto-reconnect if enabled
    if (m_autoReconnect && m_reconnectAttempts < m_maxRetries) {
        int delay = std::min(30000, 1000 * (1 << m_reconnectAttempts));  // Exponential backoff
        m_reconnectTimer->start(delay);
        
        emit reconnecting(m_reconnectAttempts + 1);
    }
}

void CollaborationEngine::onWebSocketError(QAbstractSocket::SocketError error) {
    QString errorString = m_webSocket->errorString();
    
    qWarning() << "WebSocket error:" << errorString;
    
    emit connectionError(errorString);
}

void CollaborationEngine::onWebSocketTextMessageReceived(const QString& message) {
    QElapsedTimer timer;
    timer.start();
    
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) {
        qWarning() << "Invalid JSON message received";
        return;
    }
    
    handleIncomingMessage(doc.object());
    
    qint64 latency = timer.elapsed();
    
    QMutexLocker locker(&m_mutex);
    m_latencyMeasurements.append(latency);
    if (m_latencyMeasurements.size() > 100) {
        m_latencyMeasurements.removeFirst();
    }
    
    locker.unlock();
    emit latencyMeasured(latency);
}

void CollaborationEngine::onHeartbeatTimeout() {
    QJsonObject message;
    message["type"] = "heartbeat";
    message["userId"] = m_currentUserId;
    message["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    sendMessage(message);
}

void CollaborationEngine::attemptReconnect() {
    m_reconnectAttempts++;
    
    qDebug() << "Attempting reconnect" << m_reconnectAttempts << "of" << m_maxRetries;
    
    m_webSocket->open(QUrl(m_serverUrl));
}

// ============ Private Helper Methods ============

void CollaborationEngine::sendMessage(const QJsonObject& message) {
    if (!m_isConnected) {
        qWarning() << "Cannot send message: not connected";
        return;
    }
    
    QJsonDocument doc(message);
    m_webSocket->sendTextMessage(doc.toJson(QJsonDocument::Compact));
}

void CollaborationEngine::handleIncomingMessage(const QJsonObject& message) {
    QString type = message["type"].toString();
    
    if (type == "presence_update") {
        handleUserPresenceUpdate(message);
    } else if (type == "edit_operation") {
        handleEditOperation(message);
    } else if (type == "conflict_notification") {
        handleConflictNotification(message);
    } else if (type == "file_sync") {
        handleFileSync(message);
    } else if (type == "session_update") {
        // Handle session updates (users joining/leaving, etc.)
        QString sessionId = message["sessionId"].toString();
        updateSessionActivity(sessionId);
    } else {
        qDebug() << "Unknown message type:" << type;
    }
}

void CollaborationEngine::handleUserPresenceUpdate(const QJsonObject& data) {
    QString userId = data["userId"].toString();
    PresenceState state = static_cast<PresenceState>(data["state"].toInt());
    
    updateUserPresence(userId, state);
}

void CollaborationEngine::handleEditOperation(const QJsonObject& data) {
    QJsonObject opObj = data["operation"].toObject();
    EditOperation operation = EditOperation::fromJson(opObj);
    
    applyRemoteEdit(operation);
}

void CollaborationEngine::handleConflictNotification(const QJsonObject& data) {
    // Server detected a conflict and notified us
    QString conflictId = data["conflictId"].toString();
    
    qDebug() << "Conflict notification received:" << conflictId;
    
    // Trigger local conflict detection
    QString fileId = data["fileId"].toString();
    detectConflicts(fileId);
}

void CollaborationEngine::handleFileSync(const QJsonObject& data) {
    QString fileId = data["fileId"].toString();
    QString content = data["content"].toString();
    int version = data["version"].toInt();
    
    QMutexLocker locker(&m_mutex);
    
    m_sharedFiles[fileId] = content;
    m_fileVersions[fileId] = version;
    
    locker.unlock();
    
    emit fileSyncCompleted(fileId, version);
    emit fileContentChanged(fileId, content);
    
    qDebug() << "File synced:" << fileId << "version" << version;
}

int CollaborationEngine::transformPosition(int position, const EditOperation& precedingOp) {
    if (precedingOp.type == OperationType::Insert) {
        if (precedingOp.position <= position) {
            return position + precedingOp.content.length();
        }
    } else if (precedingOp.type == OperationType::Delete) {
        if (precedingOp.position < position) {
            return position - std::min(precedingOp.length, position - precedingOp.position);
        }
    }
    
    return position;
}

bool CollaborationEngine::operationsConflict(const EditOperation& op1, const EditOperation& op2) {
    if (op1.fileId != op2.fileId) {
        return false;
    }
    
    // Check for overlapping positions
    int op1End = op1.position + (op1.type == OperationType::Delete ? op1.length : op1.content.length());
    int op2End = op2.position + (op2.type == OperationType::Delete ? op2.length : op2.content.length());
    
    return !(op1End <= op2.position || op2End <= op1.position);
}

CollaborationEngine::EditOperation CollaborationEngine::mergeOperations(
    const EditOperation& op1, const EditOperation& op2)
{
    // Simple merge: concatenate operations if they're adjacent inserts
    if (op1.type == OperationType::Insert && op2.type == OperationType::Insert) {
        if (op1.position + op1.content.length() == op2.position) {
            EditOperation merged = op1;
            merged.content += op2.content;
            merged.operationId = QUuid::createUuid().toString();
            merged.timestamp = QDateTime::currentDateTime();
            return merged;
        }
    }
    
    return op1;  // Cannot merge
}

void CollaborationEngine::checkForConflicts(const EditOperation& operation) {
    detectConflicts(operation.fileId);
}

QString CollaborationEngine::computeThreeWayMerge(const QString& base, const QString& local, const QString& remote) {
    // Simple line-based three-way merge
    QStringList baseLines = base.split('\n');
    QStringList localLines = local.split('\n');
    QStringList remoteLines = remote.split('\n');
    
    QStringList merged;
    
    int baseIdx = 0, localIdx = 0, remoteIdx = 0;
    
    while (baseIdx < baseLines.size() || localIdx < localLines.size() || remoteIdx < remoteLines.size()) {
        if (baseIdx < baseLines.size() && localIdx < localLines.size() && remoteIdx < remoteLines.size()) {
            if (baseLines[baseIdx] == localLines[localIdx] && baseLines[baseIdx] == remoteLines[remoteIdx]) {
                // No conflict: all same
                merged.append(baseLines[baseIdx]);
                baseIdx++; localIdx++; remoteIdx++;
            } else if (baseLines[baseIdx] == localLines[localIdx]) {
                // Remote changed
                merged.append(remoteLines[remoteIdx]);
                baseIdx++; localIdx++; remoteIdx++;
            } else if (baseLines[baseIdx] == remoteLines[remoteIdx]) {
                // Local changed
                merged.append(localLines[localIdx]);
                baseIdx++; localIdx++; remoteIdx++;
            } else {
                // Conflict: both changed differently
                merged.append("<<<<<<< LOCAL");
                merged.append(localLines[localIdx]);
                merged.append("=======");
                merged.append(remoteLines[remoteIdx]);
                merged.append(">>>>>>> REMOTE");
                baseIdx++; localIdx++; remoteIdx++;
            }
        } else if (localIdx < localLines.size()) {
            merged.append(localLines[localIdx]);
            localIdx++;
        } else if (remoteIdx < remoteLines.size()) {
            merged.append(remoteLines[remoteIdx]);
            remoteIdx++;
        } else {
            break;
        }
    }
    
    return merged.join('\n');
}

void CollaborationEngine::updateSessionActivity(const QString& sessionId) {
    QMutexLocker locker(&m_mutex);
    
    if (m_sessions.contains(sessionId)) {
        m_sessions[sessionId].lastActivity = QDateTime::currentDateTime();
    }
}

void CollaborationEngine::cleanupInactiveSessions() {
    QMutexLocker locker(&m_mutex);
    
    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-3600);  // 1 hour timeout
    
    QList<QString> toRemove;
    
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        if (it.value().lastActivity < cutoff) {
            toRemove.append(it.key());
        }
    }
    
    for (const QString& sessionId : toRemove) {
        m_sessions.remove(sessionId);
        qDebug() << "Cleaned up inactive session:" << sessionId;
    }
}

void CollaborationEngine::notifySessionParticipants(const QString& sessionId, const QJsonObject& message) {
    // In a real implementation, this would send to all participants via server
    // For now, we just send it through the WebSocket
    QJsonObject notification = message;
    notification["sessionId"] = sessionId;
    
    sendMessage(notification);
}
