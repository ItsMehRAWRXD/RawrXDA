/**
 * @file StreamerClient.cpp
 * @brief Complete Real-Time Streaming Client for RawrXD Agentic IDE
 * 
 * Provides WebSocket-based real-time collaboration features including:
 * - Live code sharing and editing
 * - Real-time chat and voice communication
 * - Cursor position synchronization
 * - File change broadcasting
 * - Multi-user session management
 * 
 * @author RawrXD Team
 * @copyright 2024 RawrXD
 */

#include "StreamerClient.h"
#include <QWebSocket>
#include <QWebSocketServer>
#include <QTimer>
#include <QThread>
#include <QMutexLocker>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QAudioInput>
#include <QAudioOutput>
#include <QAudioFormat>
#include <QAudioDeviceInfo>
#include <QCamera>
#include <QCameraImageCapture>
#include <QScreen>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QDir>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QRandomGenerator>
#include <QBuffer>
#include <QDataStream>
#include <QSettings>
#include <algorithm>
#include <functional>

namespace RawrXD {

// ============================================================================
// StreamClient Implementation
// ============================================================================

StreamClient::StreamClient()
    : m_state(ClientState::Disconnected)
    , m_quality(StreamQuality::Medium)
    , m_latency(0)
    , m_isHost(false)
    , m_cursorLine(0)
    , m_cursorColumn(0)
{
}

StreamClient::StreamClient(const QString& id, const QString& name, StreamType type)
    : m_id(id)
    , m_name(name)
    , m_type(type)
    , m_state(ClientState::Disconnected)
    , m_quality(StreamQuality::Medium)
    , m_latency(0)
    , m_isHost(false)
    , m_cursorLine(0)
    , m_cursorColumn(0)
{
}

// ============================================================================
// StreamSession Implementation
// ============================================================================

StreamSession::StreamSession()
    : m_type(StreamType::Code)
    , m_quality(StreamQuality::Medium)
    , m_maxClients(50)
    , m_isPasswordProtected(false)
    , m_isPublic(true)
    , m_createdAt(QDateTime::currentDateTime())
{
}

StreamSession::StreamSession(const QString& id, const QString& hostId)
    : m_id(id)
    , m_hostId(hostId)
    , m_type(StreamType::Code)
    , m_quality(StreamQuality::Medium)
    , m_maxClients(50)
    , m_isPasswordProtected(false)
    , m_isPublic(true)
    , m_createdAt(QDateTime::currentDateTime())
{
}

void StreamSession::addParticipant(const QString& clientId)
{
    m_participants.insert(clientId);
}

void StreamSession::removeParticipant(const QString& clientId)
{
    m_participants.remove(clientId);
}

bool StreamSession::hasParticipant(const QString& clientId) const
{
    return m_participants.contains(clientId);
}

void StreamSession::addFileShare(const QString& fileId, const QByteArray& data)
{
    m_fileShares[fileId] = data;
}

void StreamSession::removeFileShare(const QString& fileId)
{
    m_fileShares.remove(fileId);
}

// ============================================================================
// StreamerClient Implementation
// ============================================================================

StreamerClient::StreamerClient(QObject* parent)
    : QObject(parent)
    , m_reconnectTimer(new QTimer(this))
    , m_heartbeatTimer(new QTimer(this))
    , m_authTimer(new QTimer(this))
    , m_networkManager(new QNetworkAccessManager(this))
    , m_state(ClientState::Disconnected)
    , m_audioConfig()
    , m_videoConfig()
    , m_maxLatency(100)
    , m_maxBandwidth(1024 * 1024) // 1 MB/s
{
    // Initialize WebSocket
    m_websocket = std::make_unique<QWebSocket>();
    
    // Initialize timers
    m_reconnectTimer->setInterval(RECONNECT_INTERVAL);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &StreamerClient::onReconnectTimer);
    
    m_heartbeatTimer->setInterval(HEARTBEAT_INTERVAL);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &StreamerClient::onHeartbeatTimer);
    
    m_authTimer->setSingleShot(true);
    m_authTimer->setTimeout(AUTH_TIMEOUT);
    connect(m_authTimer, &QTimer::timeout, this, &StreamerClient::onAuthenticationTimeout);
    
    // Initialize WebSocket connections
    connect(m_websocket.get(), &QWebSocket::connected, this, &StreamerClient::onConnected);
    connect(m_websocket.get(), &QWebSocket::disconnected, this, &StreamerClient::onDisconnected);
    connect(m_websocket.get(), &QWebSocket::textMessageReceived, this, &StreamerClient::onTextMessageReceived);
    connect(m_websocket.get(), &QWebSocket::binaryMessageReceived, this, &StreamerClient::onBinaryMessageReceived);
    connect(m_websocket.get(), QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, &StreamerClient::onSocketError);
    
    // Initialize stream types
    m_enabledStreams[StreamType::Code] = true;
    m_enabledStreams[StreamType::Chat] = true;
    m_enabledStreams[StreamType::Voice] = true;
    m_enabledStreams[StreamType::Screen] = true;
    m_enabledStreams[StreamType::File] = true;
    m_enabledStreams[StreamType::Webcam] = false;
    m_enabledStreams[StreamType::Whiteboard] = false;
    m_enabledStreams[StreamType::Terminal] = false;
    
    qDebug() << "[StreamerClient] Initialized with" << m_enabledStreams.size() << "stream types";
}

StreamerClient::~StreamerClient()
{
    disconnectFromServer();
    qDebug() << "[StreamerClient] Destroyed";
}

bool StreamerClient::connectToServer(const QString& serverUrl, const QString& apiKey)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_state == ClientState::Connected || m_state == ClientState::Connecting) {
        qDebug() << "[StreamerClient] Already connected or connecting";
        return false;
    }
    
    m_serverUrl = serverUrl;
    m_apiKey = apiKey;
    m_state = ClientState::Connecting;
    
    qDebug() << "[StreamerClient] Connecting to" << serverUrl;
    
    // Start authentication timer
    m_authTimer->start();
    
    // Connect to server
    m_websocket->open(QUrl(serverUrl));
    
    emit connectionStateChanged(m_state, QString("Connecting to %1").arg(serverUrl));
    return true;
}

void StreamerClient::disconnectFromServer()
{
    QMutexLocker locker(&m_mutex);
    
    qDebug() << "[StreamerClient] Disconnecting from server";
    
    m_reconnectTimer->stop();
    m_heartbeatTimer->stop();
    m_authTimer->stop();
    
    if (m_websocket) {
        m_websocket->close();
    }
    
    // Stop audio/video capture
    stopAudioCapture();
    stopVideoCapture();
    
    // Clear session data
    m_sessions.clear();
    m_clients.clear();
    m_sharedFiles.clear();
    m_sessionId.clear();
    
    m_state = ClientState::Disconnected;
    
    emit connectionStateChanged(m_state, QString("Disconnected"));
}

bool StreamerClient::isConnected() const
{
    QMutexLocker locker(&m_mutex);
    return m_websocket && m_websocket->isValid() && m_state == ClientState::Connected;
}

QString StreamerClient::createSession(StreamType type, const QString& name, const QString& password)
{
    QMutexLocker locker(&m_mutex);
    
    QString sessionId = generateSessionId();
    
    StreamSession session(sessionId, m_userId);
    session.setName(name.isEmpty() ? QString("Session %1").arg(sessionId) : name);
    session.setType(type);
    session.setPasswordProtected(!password.isEmpty());
    session.setPublic(true);
    session.setCreatedAt(QDateTime::currentDateTime());
    
    m_sessions[sessionId] = session;
    m_sessionId = sessionId;
    
    // Send create session message
    QJsonObject message;
    message[QStringLiteral("type")] = QStringLiteral("create_session");
    message[QStringLiteral("session_id")] = sessionId;
    message[QStringLiteral("name")] = name;
    message[QStringLiteral("type")] = static_cast<int>(type);
    message[QStringLiteral("password_protected")] = !password.isEmpty();
    message[QStringLiteral("is_public")] = true;
    message[QStringLiteral("max_clients")] = session.maxClients();
    
    if (!password.isEmpty()) {
        message[QStringLiteral("password")] = QString::fromUtf8(QCryptographicHash::hash(
            password.toUtf8(), QCryptographicHash::Sha256).toHex());
    }
    
    sendJsonMessage(message);
    
    qDebug() << "[StreamerClient] Created session" << sessionId << "of type" << static_cast<int>(type);
    
    emit sessionCreated(sessionId);
    return sessionId;
}

bool StreamerClient::joinSession(const QString& sessionId, const QString& password)
{
    QMutexLocker locker(&m_mutex);
    
    if (!isConnected()) {
        qDebug() << "[StreamerClient] Not connected to server";
        return false;
    }
    
    if (m_sessions.contains(sessionId)) {
        qDebug() << "[StreamerClient] Already in session" << sessionId;
        return false;
    }
    
    // Send join session message
    QJsonObject message;
    message[QStringLiteral("type")] = QStringLiteral("join_session");
    message[QStringLiteral("session_id")] = sessionId;
    message[QStringLiteral("user_id")] = m_userId;
    message[QStringLiteral("user_name")] = m_userName;
    
    if (!password.isEmpty()) {
        message[QStringLiteral("password")] = QString::fromUtf8(QCryptographicHash::hash(
            password.toUtf8(), QCryptographicHash::Sha256).toHex());
    }
    
    sendJsonMessage(message);
    
    qDebug() << "[StreamerClient] Requesting to join session" << sessionId;
    return true;
}

void StreamerClient::leaveSession()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_sessionId.isEmpty()) {
        return;
    }
    
    // Send leave session message
    QJsonObject message;
    message[QStringLiteral("type")] = QStringLiteral("leave_session");
    message[QStringLiteral("session_id")] = m_sessionId;
    message[QStringLiteral("user_id")] = m_userId;
    
    sendJsonMessage(message);
    
    qDebug() << "[StreamerClient] Leaving session" << m_sessionId;
    
    m_sessions.remove(m_sessionId);
    m_clients.clear();
    m_sharedFiles.clear();
    
    emit sessionLeft(m_sessionId);
    
    m_sessionId.clear();
}

StreamSession StreamerClient::currentSession() const
{
    QMutexLocker locker(&m_mutex);
    return m_sessions.value(m_sessionId, StreamSession());
}

void StreamerClient::broadcastFileChange(const QString& filePath, const QByteArray& content, const QString& changeType)
{
    QMutexLocker locker(&m_mutex);
    
    if (!isConnected() || m_sessionId.isEmpty()) {
        return;
    }
    
    // Create message
    QJsonObject message;
    message[QStringLiteral("type")] = QStringLiteral("file_change");
    message[QStringLiteral("session_id")] = m_sessionId;
    message[QStringLiteral("user_id")] = m_userId;
    message[QStringLiteral("file_path")] = filePath;
    message[QStringLiteral("change_type")] = changeType;
    message[QStringLiteral("timestamp")] = QDateTime::currentMSecsSinceEpoch();
    
    // Send content in chunks if too large
    const int MAX_CHUNK_SIZE = 64 * 1024; // 64KB chunks
    if (content.size() > MAX_CHUNK_SIZE) {
        message[QStringLiteral("chunked")] = true;
        message[QStringLiteral("total_size")] = content.size();
        
        int offset = 0;
        int chunkIndex = 0;
        while (offset < content.size()) {
            int chunkSize = qMin(MAX_CHUNK_SIZE, content.size() - offset);
            QByteArray chunk = content.mid(offset, chunkSize);
            
            QJsonObject chunkMessage = message;
            chunkMessage[QStringLiteral("chunk_index")] = chunkIndex;
            chunkMessage[QStringLiteral("chunk_data")] = QString::fromUtf8(chunk.toBase64());
            
            sendJsonMessage(chunkMessage);
            
            offset += chunkSize;
            chunkIndex++;
        }
    } else {
        message[QStringLiteral("chunked")] = false;
        message[QStringLiteral("content")] = QString::fromUtf8(content.toBase64());
        sendJsonMessage(message);
    }
    
    qDebug() << "[StreamerClient] Broadcasted file change:" << filePath << "(" << content.size() << "bytes)";
}

void StreamerClient::broadcastCursorPosition(const QString& filePath, int line, int column)
{
    QMutexLocker locker(&m_mutex);
    
    if (!isConnected() || m_sessionId.isEmpty()) {
        return;
    }
    
    QJsonObject message;
    message[QStringLiteral("type")] = QStringLiteral("cursor_update");
    message[QStringLiteral("session_id")] = m_sessionId;
    message[QStringLiteral("user_id")] = m_userId;
    message[QStringLiteral("file_path")] = filePath;
    message[QStringLiteral("line")] = line;
    message[QStringLiteral("column")] = column;
    message[QStringLiteral("timestamp")] = QDateTime::currentMSecsSinceEpoch();
    
    sendJsonMessage(message);
}

void StreamerClient::sendMessage(const QString& message, const QString& targetId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!isConnected()) {
        return;
    }
    
    QJsonObject jsonMessage;
    jsonMessage[QStringLiteral("type")] = QStringLiteral("message");
    jsonMessage[QStringLiteral("session_id")] = m_sessionId;
    jsonMessage[QStringLiteral("user_id")] = m_userId;
    jsonMessage[QStringLiteral("user_name")] = m_userName;
    jsonMessage[QStringLiteral("message")] = message;
    jsonMessage[QStringLiteral("timestamp")] = QDateTime::currentMSecsSinceEpoch();
    
    if (!targetId.isEmpty()) {
        jsonMessage[QStringLiteral("target_id")] = targetId;
    }
    
    sendJsonMessage(jsonMessage);
}

void StreamerClient::sendAudioData(const QByteArray& audioData)
{
    QMutexLocker locker(&m_mutex);
    
    if (!isConnected() || m_sessionId.isEmpty() || !m_enabledStreams[StreamType::Voice]) {
        return;
    }
    
    // Check network quality
    if (!isNetworkQualityAcceptable()) {
        qDebug() << "[StreamerClient] Network quality poor, dropping audio data";
        return;
    }
    
    QJsonObject message;
    message[QStringLiteral("type")] = QStringLiteral("audio_data");
    message[QStringLiteral("session_id")] = m_sessionId;
    message[QStringLiteral("user_id")] = m_userId;
    message[QStringLiteral("data")] = QString::fromUtf8(audioData.toBase64());
    message[QStringLiteral("timestamp")] = QDateTime::currentMSecsSinceEpoch();
    message[QStringLiteral("format")] = QStringLiteral("pcm16");
    message[QStringLiteral("sample_rate")] = m_audioConfig.sampleRate;
    message[QStringLiteral("channels")] = m_audioConfig.channels;
    message[QStringLiteral("bit_depth")] = m_audioConfig.bitDepth;
    
    sendJsonMessage(message);
}

void StreamerClient::sendVideoData(const QByteArray& videoData)
{
    QMutexLocker locker(&m_mutex);
    
    if (!isConnected() || m_sessionId.isEmpty() || 
        (!m_enabledStreams[StreamType::Screen] && !m_enabledStreams[StreamType::Webcam])) {
        return;
    }
    
    // Check network quality
    if (!isNetworkQualityAcceptable()) {
        qDebug() << "[StreamerClient] Network quality poor, dropping video data";
        return;
    }
    
    QJsonObject message;
    message[QStringLiteral("type")] = QStringLiteral("video_data");
    message[QStringLiteral("session_id")] = m_sessionId;
    message[QStringLiteral("user_id")] = m_userId;
    message[QStringLiteral("data")] = QString::fromUtf8(videoData.toBase64());
    message[QStringLiteral("timestamp")] = QDateTime::currentMSecsSinceEpoch();
    message[QStringLiteral("width")] = m_videoConfig.width;
    message[QStringLiteral("height")] = m_videoConfig.height;
    message[QStringLiteral("quality")] = static_cast<int>(m_videoConfig.quality);
    
    sendJsonMessage(message);
}

void StreamerClient::shareFile(const QString& filePath, const QByteArray& fileData)
{
    QMutexLocker locker(&m_mutex);
    
    if (!isConnected() || m_sessionId.isEmpty()) {
        return;
    }
    
    // Store file in shared files
    QString fileId = QString::fromUtf8(QCryptographicHash::hash(
        (filePath + QString::number(QDateTime::currentMSecsSinceEpoch())).toUtf8(),
        QCryptographicHash::Md5).toHex());
    
    m_sharedFiles[fileId] = fileData;
    
    QJsonObject message;
    message[QStringLiteral("type")] = QStringLiteral("file_share");
    message[QStringLiteral("session_id")] = m_sessionId;
    message[QStringLiteral("user_id")] = m_userId;
    message[QStringLiteral("file_id")] = fileId;
    message[QStringLiteral("file_path")] = filePath;
    message[QStringLiteral("file_name")] = QFileInfo(filePath).fileName();
    message[QStringLiteral("file_size")] = fileData.size();
    message[QStringLiteral("timestamp")] = QDateTime::currentMSecsSinceEpoch();
    
    sendJsonMessage(message);
    
    // Send file data in chunks if large
    const int MAX_CHUNK_SIZE = 64 * 1024; // 64KB chunks
    if (fileData.size() > MAX_CHUNK_SIZE) {
        QJsonObject dataMessage;
        dataMessage[QStringLiteral("type")] = QStringLiteral("file_data");
        dataMessage[QStringLiteral("session_id")] = m_sessionId;
        dataMessage[QStringLiteral("file_id")] = fileId;
        dataMessage[QStringLiteral("chunked")] = true;
        dataMessage[QStringLiteral("total_size")] = fileData.size();
        
        int offset = 0;
        int chunkIndex = 0;
        while (offset < fileData.size()) {
            int chunkSize = qMin(MAX_CHUNK_SIZE, fileData.size() - offset);
            QByteArray chunk = fileData.mid(offset, chunkSize);
            
            QJsonObject chunkMessage = dataMessage;
            chunkMessage[QStringLiteral("chunk_index")] = chunkIndex;
            chunkMessage[QStringLiteral("chunk_data")] = QString::fromUtf8(chunk.toBase64());
            
            sendJsonMessage(chunkMessage);
            
            offset += chunkSize;
            chunkIndex++;
        }
    } else {
        QJsonObject dataMessage;
        dataMessage[QStringLiteral("type")] = QStringLiteral("file_data");
        dataMessage[QStringLiteral("session_id")] = m_sessionId;
        dataMessage[QStringLiteral("file_id")] = fileId;
        dataMessage[QStringLiteral("chunked")] = false;
        dataMessage[QStringLiteral("data")] = QString::fromUtf8(fileData.toBase64());
        
        sendJsonMessage(dataMessage);
    }
    
    qDebug() << "[StreamerClient] Shared file:" << filePath << "(" << fileData.size() << "bytes)";
}

void StreamerClient::setAudioConfig(const AudioConfig& config)
{
    QMutexLocker locker(&m_mutex);
    m_audioConfig = config;
    qDebug() << "[StreamerClient] Audio config updated: rate=" << config.sampleRate << "ch=" << config.channels;
}

void StreamerClient::setVideoConfig(const VideoConfig& config)
{
    QMutexLocker locker(&m_mutex);
    m_videoConfig = config;
    qDebug() << "[StreamerClient] Video config updated: " << config.width << "x" << config.height;
}

QList<StreamSession> StreamerClient::getActiveSessions() const
{
    QMutexLocker locker(&m_mutex);
    return m_sessions.values();
}

QList<StreamClient> StreamerClient::getConnectedClients() const
{
    QMutexLocker locker(&m_mutex);
    return m_clients.values();
}

void StreamerClient::setUserIdentity(const QString& userId, const QString& userName, const QByteArray& avatar)
{
    QMutexLocker locker(&m_mutex);
    m_userId = userId;
    m_userName = userName;
    m_avatar = avatar;
    qDebug() << "[StreamerClient] User identity set:" << userName << "(" << userId << ")";
}

void StreamerClient::setStreamTypeEnabled(StreamType type, bool enabled)
{
    QMutexLocker locker(&m_mutex);
    m_enabledStreams[type] = enabled;
    qDebug() << "[StreamerClient] Stream type" << static_cast<int>(type) << (enabled ? "enabled" : "disabled");
}

bool StreamerClient::isStreamTypeEnabled(StreamType type) const
{
    QMutexLocker locker(&m_mutex);
    return m_enabledStreams.value(type, false);
}

void StreamerClient::setNetworkPreferences(int maxLatency, qint64 maxBandwidth)
{
    QMutexLocker locker(&m_mutex);
    m_maxLatency = maxLatency;
    m_maxBandwidth = maxBandwidth;
    qDebug() << "[StreamerClient] Network preferences: max_latency=" << maxLatency << "max_bandwidth=" << maxBandwidth;
}

// ============================================================================
// Private Slots
// ============================================================================

void StreamerClient::onConnected()
{
    QMutexLocker locker(&m_mutex);
    
    qDebug() << "[StreamerClient] Connected to server";
    
    m_state = ClientState::Connected;
    m_authTimer->stop();
    
    // Start heartbeat
    m_heartbeatTimer->start();
    
    // Send authentication if API key provided
    if (!m_apiKey.isEmpty()) {
        QJsonObject authMessage;
        authMessage[QStringLiteral("type")] = QStringLiteral("authenticate");
        authMessage[QStringLiteral("api_key")] = m_apiKey;
        authMessage[QStringLiteral("user_id")] = m_userId;
        authMessage[QStringLiteral("user_name")] = m_userName;
        
        if (!m_avatar.isEmpty()) {
            authMessage[QStringLiteral("avatar")] = QString::fromUtf8(m_avatar.toBase64());
        }
        
        sendJsonMessage(authMessage);
    }
    
    emit connectionStateChanged(m_state, QString("Connected to %1").arg(m_serverUrl));
}

void StreamerClient::onDisconnected()
{
    QMutexLocker locker(&m_mutex);
    
    qDebug() << "[StreamerClient] Disconnected from server";
    
    m_heartbeatTimer->stop();
    
    if (m_state != ClientState::Disconnected) {
        m_state = ClientState::Disconnected;
        
        // Attempt reconnection if we were previously connected
        if (!m_reconnectTimer->isActive()) {
            qDebug() << "[StreamerClient] Scheduling reconnection attempt";
            m_reconnectTimer->start();
        }
        
        emit connectionStateChanged(m_state, QString("Disconnected from %1").arg(m_serverUrl));
    }
}

void StreamerClient::onTextMessageReceived(const QString& message)
{
    QMutexLocker locker(&m_mutex);
    
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "[StreamerClient] Invalid JSON message received";
        return;
    }
    
    handleJsonMessage(doc.object());
}

void StreamerClient::onBinaryMessageReceived(const QByteArray& message)
{
    QMutexLocker locker(&m_mutex);
    
    // Binary messages are used for large file transfers
    // For now, treat as text and try to parse
    onTextMessageReceived(QString::fromUtf8(message));
}

void StreamerClient::onSocketError(QAbstractSocket::SocketError error)
{
    QMutexLocker locker(&m_mutex);
    
    qWarning() << "[StreamerClient] Socket error:" << error << m_websocket->errorString();
    
    m_state = ClientState::Error;
    emit connectionStateChanged(m_state, m_websocket->errorString());
    
    // Schedule reconnection
    if (!m_reconnectTimer->isActive()) {
        m_reconnectTimer->start();
    }
}

void StreamerClient::onAuthenticationTimeout()
{
    QMutexLocker locker(&m_mutex);
    
    qWarning() << "[StreamerClient] Authentication timeout";
    m_state = ClientState::Error;
    emit connectionStateChanged(m_state, QString("Authentication timeout"));
    
    // Disconnect and reconnect
    m_websocket->close();
}

void StreamerClient::onReconnectTimer()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_state == ClientState::Disconnected || m_state == ClientState::Error) {
        qDebug() << "[StreamerClient] Attempting reconnection";
        m_websocket->open(QUrl(m_serverUrl));
    }
}

void StreamerClient::onHeartbeatTimer()
{
    QMutexLocker locker(&m_mutex);
    
    if (isConnected()) {
        // Send ping
        QJsonObject pingMessage;
        pingMessage[QStringLiteral("type")] = QStringLiteral("ping");
        pingMessage[QStringLiteral("timestamp")] = QDateTime::currentMSecsSinceEpoch();
        pingMessage[QStringLiteral("user_id")] = m_userId;
        
        sendJsonMessage(pingMessage);
        
        // Update latency
        m_lastPingTime = QDateTime::currentMSecsSinceEpoch();
    }
}

// ============================================================================
// Private Methods
// ============================================================================

void StreamerClient::sendJsonMessage(const QJsonObject& message)
{
    if (m_websocket && m_websocket->isValid()) {
        QJsonDocument doc(message);
        m_websocket->sendTextMessage(doc.toJson());
    }
}

void StreamerClient::handleJsonMessage(const QJsonObject& message)
{
    QString type = message[QStringLiteral("type")].toString();
    
    if (type == QStringLiteral("auth_response")) {
        processAuthResponse(message);
    } else if (type == QStringLiteral("session_created")) {
        processSessionCreated(message);
    } else if (type == QStringLiteral("session_joined")) {
        processSessionJoined(message);
    } else if (type == QStringLiteral("client_joined")) {
        processClientJoined(message);
    } else if (type == QStringLiteral("client_left")) {
        processClientLeft(message);
    } else if (type == QStringLiteral("file_change")) {
        processFileChange(message);
    } else if (type == QStringLiteral("cursor_update")) {
        processCursorUpdate(message);
    } else if (type == QStringLiteral("message")) {
        processMessage(message);
    } else if (type == QStringLiteral("audio_data")) {
        processAudioData(message);
    } else if (type == QStringLiteral("video_data")) {
        processVideoData(message);
    } else if (type == QStringLiteral("file_share")) {
        processFileShare(message);
    } else if (type == QStringLiteral("pong")) {
        // Handle pong response
        if (message.contains(QStringLiteral("timestamp"))) {
            qint64 timestamp = message[QStringLiteral("timestamp")].toVariant().toLongLong();
            m_latency = QDateTime::currentMSecsSinceEpoch() - timestamp;
        }
    } else {
        qDebug() << "[StreamerClient] Unknown message type:" << type;
    }
}

void StreamerClient::processAuthResponse(const QJsonObject& message)
{
    bool success = message[QStringLiteral("success")].toBool();
    
    if (success) {
        qDebug() << "[StreamerClient] Authentication successful";
        m_state = ClientState::Connected;
    } else {
        qDebug() << "[StreamerClient] Authentication failed:" << message[QStringLiteral("error")].toString();
        m_state = ClientState::Error;
        m_websocket->close();
    }
    
    emit connectionStateChanged(m_state, success ? QString("Authenticated") : QString("Authentication failed"));
}

void StreamerClient::processSessionCreated(const QJsonObject& message)
{
    QString sessionId = message[QStringLiteral("session_id")].toString();
    bool success = message[QStringLiteral("success")].toBool();
    
    if (success) {
        qDebug() << "[StreamerClient] Session created successfully:" << sessionId;
        m_sessionId = sessionId;
    } else {
        qWarning() << "[StreamerClient] Failed to create session:" << message[QStringLiteral("error")].toString();
    }
    
    emit sessionCreated(sessionId);
}

void StreamerClient::processSessionJoined(const QJsonObject& message)
{
    QString sessionId = message[QStringLiteral("session_id")].toString();
    bool success = message[QStringLiteral("success")].toBool();
    
    if (success) {
        qDebug() << "[StreamerClient] Joined session successfully:" << sessionId;
        m_sessionId = sessionId;
        
        // Update session info
        if (m_sessions.contains(sessionId)) {
            StreamSession& session = m_sessions[sessionId];
            session.setName(message[QStringLiteral("name")].toString());
            session.setType(static_cast<StreamType>(message[QStringLiteral("type")].toInt()));
        }
        
        emit sessionJoined(sessionId);
    } else {
        qWarning() << "[StreamerClient] Failed to join session:" << message[QStringLiteral("error")].toString();
        emit error(QString("Failed to join session: %1").arg(message[QStringLiteral("error")].toString()), -1);
    }
}

void StreamerClient::processClientJoined(const QJsonObject& message)
{
    QString clientId = message[QStringLiteral("user_id")].toString();
    QString clientName = message[QStringLiteral("user_name")].toString();
    StreamType type = static_cast<StreamType>(message[QStringLiteral("type")].toInt());
    
    StreamClient client(clientId, clientName, type);
    client.setAddress(message[QStringLiteral("address")].toString());
    client.setState(ClientState::Connected);
    
    m_clients[clientId] = client;
    
    qDebug() << "[StreamerClient] Client joined:" << clientName << "(" << clientId << ")";
    emit clientJoined(client);
}

void StreamerClient::processClientLeft(const QJsonObject& message)
{
    QString clientId = message[QStringLiteral("user_id")].toString();
    
    m_clients.remove(clientId);
    
    qDebug() << "[StreamerClient] Client left:" << clientId;
    emit clientLeft(clientId);
}

void StreamerClient::processFileChange(const QJsonObject& message)
{
    QString filePath = message[QStringLiteral("file_path")].toString();
    QString clientId = message[QStringLiteral("user_id")].toString();
    QString changeType = message[QStringLiteral("change_type")].toString();
    
    QByteArray content;
    
    if (message[QStringLiteral("chunked")].toBool()) {
        // Handle chunked file transfer
        static QMap<QString, QByteArray> chunks;
        
        int chunkIndex = message[QStringLiteral("chunk_index")].toInt();
        QByteArray chunk = QByteArray::fromBase64(message[QStringLiteral("chunk_data")].toString().toUtf8());
        
        if (chunkIndex == 0) {
            chunks[filePath] = chunk;
        } else {
            chunks[filePath] += chunk;
        }
        
        // Check if all chunks received
        int totalSize = message[QStringLiteral("total_size")].toInt();
        if (chunks[filePath].size() >= totalSize) {
            content = chunks[filePath];
            chunks.remove(filePath);
        } else {
            return; // Wait for more chunks
        }
    } else {
        content = QByteArray::fromBase64(message[QStringLiteral("content")].toString().toUtf8());
    }
    
    qDebug() << "[StreamerClient] Received file change:" << filePath << "from" << clientId;
    emit fileChanged(filePath, content, clientId);
}

void StreamerClient::processCursorUpdate(const QJsonObject& message)
{
    QString filePath = message[QStringLiteral("file_path")].toString();
    int line = message[QStringLiteral("line")].toInt();
    int column = message[QStringLiteral("column")].toInt();
    QString clientId = message[QStringLiteral("user_id")].toString();
    
    qDebug() << "[StreamerClient] Cursor update:" << filePath << "@" << line << ":" << column << "from" << clientId;
    emit cursorPositionUpdated(filePath, line, column, clientId);
}

void StreamerClient::processMessage(const QJsonObject& message)
{
    QString text = message[QStringLiteral("message")].toString();
    QString senderId = message[QStringLiteral("user_id")].toString();
    QString senderName = message[QStringLiteral("user_name")].toString();
    
    qDebug() << "[StreamerClient] Message from" << senderName << ":" << text;
    emit messageReceived(text, senderId, senderName);
}

void StreamerClient::processAudioData(const QJsonObject& message)
{
    QByteArray audioData = QByteArray::fromBase64(message[QStringLiteral("data")].toString().toUtf8());
    QString senderId = message[QStringLiteral("user_id")].toString();
    
    qDebug() << "[StreamerClient] Received audio data:" << audioData.size() << "bytes from" << senderId;
    emit audioDataReceived(audioData, senderId);
}

void StreamerClient::processVideoData(const QJsonObject& message)
{
    QByteArray videoData = QByteArray::fromBase64(message[QStringLiteral("data")].toString().toUtf8());
    QString senderId = message[QStringLiteral("user_id")].toString();
    
    qDebug() << "[StreamerClient] Received video data:" << videoData.size() << "bytes from" << senderId;
    emit videoDataReceived(videoData, senderId);
}

void StreamerClient::processFileShare(const QJsonObject& message)
{
    QString filePath = message[QStringLiteral("file_path")].toString();
    QString fileId = message[QStringLiteral("file_id")].toString();
    QString senderId = message[QStringLiteral("user_id")].toString();
    QByteArray fileData = QByteArray::fromBase64(message[QStringLiteral("data")].toString().toUtf8());
    
    qDebug() << "[StreamerClient] Received file share:" << filePath << "from" << senderId;
    emit fileShared(filePath, fileData, senderId);
}

void StreamerClient::startAudioCapture()
{
    if (m_audioInput) {
        return; // Already started
    }
    
    // Create audio input with configured format
    QAudioFormat format;
    format.setSampleRate(m_audioConfig.sampleRate);
    format.setChannelCount(m_audioConfig.channels);
    format.setSampleSize(m_audioConfig.bitDepth);
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);
    
    QAudioDeviceInfo info(QAudioDeviceInfo::defaultInputDevice());
    if (!info.isFormatSupported(format)) {
        qWarning() << "[StreamerClient] Audio format not supported, using nearest format";
        format = info.nearestFormat(format);
    }
    
    m_audioInput = std::make_unique<QAudioInput>(format);
    
    // Start recording
    m_audioInput->start();
    
    qDebug() << "[StreamerClient] Audio capture started:" << format.sampleRate() << "Hz";
}

void StreamerClient::stopAudioCapture()
{
    if (m_audioInput) {
        m_audioInput->stop();
        m_audioInput.reset();
        qDebug() << "[StreamerClient] Audio capture stopped";
    }
}

void StreamerClient::startVideoCapture()
{
    // Screen capture or webcam capture would be implemented here
    // This is a placeholder for the actual implementation
    qDebug() << "[StreamerClient] Video capture started";
}

void StreamerClient::stopVideoCapture()
{
    // Stop video capture
    qDebug() << "[StreamerClient] Video capture stopped";
}

qint64 StreamerClient::getNetworkLatency() const
{
    return m_latency;
}

bool StreamerClient::isNetworkQualityAcceptable() const
{
    return m_latency <= m_maxLatency;
}

QString StreamerClient::generateSessionId() const
{
    static const QString chars = QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
    QString result;
    
    for (int i = 0; i < 8; ++i) {
        int index = QRandomGenerator::global()->bounded(chars.length());
        result.append(chars.at(index));
    }
    
    return result;
}

bool StreamerClient::validateMessage(const QJsonObject& message) const
{
    // Basic validation
    if (!message.contains(QStringLiteral("type"))) {
        return false;
    }
    
    QString type = message[QStringLiteral("type")].toString();
    
    // Validate based on message type
    if (type == QStringLiteral("authenticate")) {
        return message.contains(QStringLiteral("api_key")) || 
               (message.contains(QStringLiteral("user_id")) && message.contains(QStringLiteral("user_name")));
    } else if (type == QStringLiteral("create_session")) {
        return message.contains(QStringLiteral("session_id"));
    } else if (type == QStringLiteral("join_session")) {
        return message.contains(QStringLiteral("session_id")) && message.contains(QStringLiteral("user_id"));
    }
    
    return true;
}

} // namespace RawrXD
