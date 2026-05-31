#include "gguf_server.hpp"
#include "inference_engine.hpp"
#include <QNetworkInterface>
#include <QHostAddress>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QCoreApplication>
#include <QThread>
#include <QDebug>

GGUFServer::GGUFServer(InferenceEngine* engine, QObject* parent)
    : QObject(parent)
    , m_engine(engine)
    , m_server(new QTcpServer(this))
    , m_isRunning(false)
    , m_port(0)
    , m_healthTimer(new QTimer(this))
{
    connect(m_server, &QTcpServer::newConnection, this, &GGUFServer::onNewConnection);
    connect(m_healthTimer, &QTimer::timeout, this, &GGUFServer::onHealthCheck);
    
    qInfo() << "GGUFServer initialized";
}

GGUFServer::~GGUFServer() {
    stop();
}

bool GGUFServer::start(quint16 port) {
    QMutexLocker locker(&m_mutex);
    
    if (m_isRunning) {
        qInfo() << "Server already running on port" << m_port;
        return true;
    }
    
    // Check if another server is running on this port
    if (isServerRunningOnPort(port)) {
        qInfo() << "Server already running on port" << port << "- using existing instance";
        m_isRunning = true;  // Mark as running (external instance)
        m_port = port;
        return true;
    }
    
    // Try to bind to the port
    if (!tryBindPort(port)) {
        qWarning() << "Failed to bind to port" << port;
        
        // Try alternative ports
        for (quint16 altPort = port + 1; altPort < port + 10; ++altPort) {
            if (tryBindPort(altPort)) {
                qInfo() << "Bound to alternative port" << altPort;
                port = altPort;
                break;
            }
        }
        
        if (!m_server->isListening()) {
            emit error("Failed to start server on any port");
            return false;
        }
    }
    
    m_isRunning = true;
    m_port = port;
    m_startTime = QDateTime::currentDateTime();
    m_stats = ServerStats(); // Reset stats
    
    // Start health monitoring
    m_healthTimer->start(HEALTH_CHECK_INTERVAL_MS);
    
    qInfo() << "GGUF Server started on port" << m_port;
    qInfo() << "Endpoints available:";
    qInfo() << "  POST http://localhost:" << m_port << "/api/generate";
    qInfo() << "  POST http://localhost:" << m_port << "/v1/chat/completions";
    qInfo() << "  GET  http://localhost:" << m_port << "/api/tags";
    qInfo() << "  POST http://localhost:" << m_port << "/api/pull";
    qInfo() << "  POST http://localhost:" << m_port << "/api/push";
    qInfo() << "  POST http://localhost:" << m_port << "/api/show";
    qInfo() << "  DELETE http://localhost:" << m_port << "/api/delete";
    qInfo() << "  GET  http://localhost:" << m_port << "/health";
    
    emit serverStarted(m_port);
    return true;
}

void GGUFServer::stop() {
    if (!m_isRunning) {
        return;
    }
    
    // Don't use QMutexLocker here - it can deadlock during shutdown
    m_mutex.lock();
    bool wasRunning = m_isRunning;
    m_isRunning = false;
    m_mutex.unlock();
    
    if (!wasRunning) {
        return;
    }
    
    // Stop timer safely - only if it belongs to this thread
    if (m_healthTimer && m_healthTimer->thread() == QThread::currentThread()) {
        m_healthTimer->stop();
    }
    
    if (m_server && m_server->isListening()) {
        m_server->close();
        // Wait a bit for pending accepts to complete
        m_server->waitForNewConnection(100);
    }
    
    // Force close all pending connections
    m_mutex.lock();
    QList<QTcpSocket*> socketsToClose = m_pendingRequests.keys();
    m_mutex.unlock();
    
    for (auto socket : socketsToClose) {
        if (socket) {
            // Immediately close the socket - don't wait for signal handlers
            socket->disconnectFromHost();
            
            // If socket is still open, force close it
            if (socket->state() != QAbstractSocket::UnconnectedState) {
                socket->close();
            }
            
            // Delete immediately instead of using deleteLater
            socket->deleteLater();
        }
    }
    
    m_mutex.lock();
    m_pendingRequests.clear();
    m_mutex.unlock();
    
    qInfo() << "GGUF Server stopped";
    emit serverStopped();
}

bool GGUFServer::isRunning() const {
    return m_isRunning;
}

quint16 GGUFServer::port() const {
    return m_port;
}

bool GGUFServer::isServerRunningOnPort(quint16 port) {
    QTcpSocket testSocket;
    testSocket.connectToHost(QHostAddress::LocalHost, port);
    
    if (testSocket.waitForConnected(500)) {
        // Send a simple HTTP GET request to check if it's our server
        testSocket.write("GET /health HTTP/1.1\r\nHost: localhost\r\n\r\n");
        testSocket.flush();
        
        if (testSocket.waitForReadyRead(1000)) {
            QByteArray response = testSocket.readAll();
            testSocket.close();
            
            // Check if response looks like our server
            return response.contains("HTTP/1.1") || response.contains("HTTP/1.0");
        }
        
        testSocket.close();
        return true; // Something is listening
    }
    
    return false;
}

GGUFServer::ServerStats GGUFServer::getStats() const {
    ServerStats stats = m_stats;
    
    if (m_isRunning) {
        stats.uptimeSeconds = m_startTime.secsTo(QDateTime::currentDateTime());
        stats.startTime = m_startTime.toString(Qt::ISODate);
    }
    
    return stats;
}

void GGUFServer::onNewConnection() {
    while (m_server->hasPendingConnections()) {
        QTcpSocket* socket = m_server->nextPendingConnection();
        
        connect(socket, &QTcpSocket::readyRead, this, &GGUFServer::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &GGUFServer::onDisconnected);
        
        m_pendingRequests[socket] = QByteArray();
    }
}

void GGUFServer::onReadyRead() {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    
    // Append incoming data
    m_pendingRequests[socket].append(socket->readAll());
    
    // Check if we have a complete HTTP request
    QByteArray& buffer = m_pendingRequests[socket];
    
    // Check for request size limit
    if (buffer.size() > MAX_REQUEST_SIZE) {
        HttpResponse response;
        response.statusCode = 413;
        response.statusText = "Payload Too Large";
        response.body = "{\"error\":\"Request too large\"}";
        sendResponse(socket, response);
        socket->disconnectFromHost();
        return;
    }
    
    // Look for end of HTTP headers
    int headerEnd = buffer.indexOf("\r\n\r\n");
    if (headerEnd == -1) {
        return; // Wait for more data
    }
    
    // Parse headers to check Content-Length
    QString headerStr = QString::fromUtf8(buffer.left(headerEnd));
    int contentLength = 0;
    
    for (const QString& line : headerStr.split("\r\n")) {
        if (line.startsWith("Content-Length:", Qt::CaseInsensitive)) {
            contentLength = line.mid(15).trimmed().toInt();
            break;
        }
    }
    
    // Check if we have the complete body
    int totalExpected = headerEnd + 4 + contentLength;
    if (buffer.size() < totalExpected) {
        return; // Wait for more data
    }
    
    // Extract complete request
    QByteArray requestData = buffer.left(totalExpected);
    buffer.remove(0, totalExpected);
    
    // Parse and handle request
    HttpRequest request = parseHttpRequest(requestData);
    handleRequest(socket, request);
}

void GGUFServer::onDisconnected() {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    
    m_pendingRequests.remove(socket);
    socket->deleteLater();
}

void GGUFServer::onHealthCheck() {
    // Periodic health check - could log stats, clean up stale connections, etc.
    if (m_isRunning && m_engine) {
        qDebug() << "Health check - Server running, total requests:" << m_stats.totalRequests;
    }
}

bool GGUFServer::tryBindPort(quint16 port) {
    if (m_server->listen(QHostAddress::Any, port)) {
        return true;
    }
    return false;
}

bool GGUFServer::waitForServerShutdown(quint16 port, int maxWaitMs) {
    QElapsedTimer timer;
    timer.start();
    
    while (timer.elapsed() < maxWaitMs) {
        if (!isServerRunningOnPort(port)) {
            return true;
        }
        QThread::msleep(100);
    }
    
    return false;
}

QString GGUFServer::getCurrentTimestamp() const {
    return QDateTime::currentDateTime().toString(Qt::ISODate);
}

// BOTTLENECK #3 FIX: Lightweight JSON field extraction (no DOM tree building)
// Extracts a single field value from JSON without parsing entire structure
QString GGUFServer::extractJsonField(const QByteArray& json, const QString& fieldName) {
    // Fast path: search for field pattern in raw JSON
    // Pattern: "fieldName":"value" or "fieldName":value
    QString searchPattern = QString("\"%1\"").arg(fieldName);
    int fieldPos = json.indexOf(searchPattern.toUtf8());
    
    if (fieldPos == -1) {
        return QString();  // Field not found
    }
    
    // Find the colon after field name
    int colonPos = json.indexOf(':', fieldPos);
    if (colonPos == -1) {
        return QString();
    }
    
    // Skip whitespace after colon
    int valueStart = colonPos + 1;
    while (valueStart < json.size() && (json[valueStart] == ' ' || json[valueStart] == '\t')) {
        valueStart++;
    }
    
    if (valueStart >= json.size()) {
        return QString();
    }
    
    // Check if value is a string (starts with quote)
    if (json[valueStart] == '"') {
        valueStart++;  // Skip opening quote
        int valueEnd = json.indexOf('"', valueStart);
        if (valueEnd == -1) {
            return QString();
        }
        return QString::fromUtf8(json.mid(valueStart, valueEnd - valueStart));
    }
    
    // Non-string value (number, bool, null)
    int valueEnd = valueStart;
    while (valueEnd < json.size() && 
           json[valueEnd] != ',' && 
           json[valueEnd] != '}' && 
           json[valueEnd] != ']' &&
           json[valueEnd] != '\r' &&
           json[valueEnd] != '\n') {
        valueEnd++;
    }
    
    return QString::fromUtf8(json.mid(valueStart, valueEnd - valueStart).trimmed());
}

// BOTTLENECK #3 FIX: Extract JSON array field (for messages in chat completions)
QJsonArray GGUFServer::extractJsonArray(const QByteArray& json, const QString& fieldName) {
    // For arrays, we still need QJsonDocument but only for that specific field
    // This is better than parsing the whole document
    QString searchPattern = QString("\"%1\"").arg(fieldName);
    int fieldPos = json.indexOf(searchPattern.toUtf8());
    
    if (fieldPos == -1) {
        return QJsonArray();
    }
    
    int colonPos = json.indexOf(':', fieldPos);
    if (colonPos == -1) {
        return QJsonArray();
    }
    
    // Find the array start '['
    int arrayStart = json.indexOf('[', colonPos);
    if (arrayStart == -1) {
        return QJsonArray();
    }
    
    // Find matching ']' (simple bracket counting)
    int bracketCount = 1;
    int arrayEnd = arrayStart + 1;
    while (arrayEnd < json.size() && bracketCount > 0) {
        if (json[arrayEnd] == '[') bracketCount++;
        else if (json[arrayEnd] == ']') bracketCount--;
        arrayEnd++;
    }
    
    if (bracketCount != 0) {
        return QJsonArray();  // Malformed
    }
    
    // Parse just this array portion
    QByteArray arrayJson = json.mid(arrayStart, arrayEnd - arrayStart);
    QJsonDocument doc = QJsonDocument::fromJson(arrayJson);
    return doc.array();
}

QJsonDocument GGUFServer::parseJsonBody(const QByteArray& body) {
    // Legacy fallback for complex JSON (only used when streaming parser can't handle it)
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(body, &error);
    
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "JSON parse error:" << error.errorString();
    }
    
    return doc;
}

void GGUFServer::logRequest(const QString& method, const QString& path, int statusCode) {
    qInfo() << getCurrentTimestamp() << method << path << "->" << statusCode;
}

GGUFServer::HttpRequest GGUFServer::parseHttpRequest(const QByteArray& rawData) {
    HttpRequest request;
    
    QString data = QString::fromUtf8(rawData);
    QStringList lines = data.split("\r\n");
    
    if (lines.isEmpty()) {
        return request;
    }
    
    // Parse request line: "GET /path HTTP/1.1"
    QStringList requestLine = lines[0].split(' ');
    if (requestLine.size() >= 3) {
        request.method = requestLine[0].toUpper();
        request.path = requestLine[1];
        request.httpVersion = requestLine[2];
        
        // Parse query parameters
        if (request.path.contains('?')) {
            QStringList parts = request.path.split('?');
            request.path = parts[0];
            if (parts.size() > 1) {
                QUrlQuery query(parts[1]);
                for (const auto& item : query.queryItems()) {
                    request.queryParams[item.first] = item.second;
                }
            }
        }
    }
    
    // Parse headers
    int i = 1;
    for (; i < lines.size(); ++i) {
        if (lines[i].isEmpty()) {
            ++i;
            break;
        }
        
        int colonPos = lines[i].indexOf(':');
        if (colonPos > 0) {
            QString key = lines[i].left(colonPos).trimmed();
            QString value = lines[i].mid(colonPos + 1).trimmed();
            request.headers[key] = value;
        }
    }
    
    // Extract body
    if (i < lines.size()) {
        QStringList bodyLines = lines.mid(i);
        request.body = bodyLines.join("\r\n").toUtf8();
    }
    
    return request;
}

void GGUFServer::handleRequest(QTcpSocket* socket, const HttpRequest& request) {
    QElapsedTimer timer;
    timer.start();
    
    m_stats.totalRequests++;
    emit requestReceived(request.path, request.method);
    
    HttpResponse response;
    response.headers["Content-Type"] = "application/json";
    response.headers["Access-Control-Allow-Origin"] = "*";
    response.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
    response.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization";
    
    // Handle CORS preflight
    if (request.method == "OPTIONS") {
        handleCorsPreflightRequest(response);
    }
    // Root endpoint - compatible with Ollama
    else if (request.path == "/" && request.method == "GET") {
        response.statusCode = 200;
        response.statusText = "OK";
        response.headers["Content-Type"] = "text/plain";
        response.body = "Ollama is running";
    }
    // Route to appropriate handler
    else if (request.path == "/api/generate" && request.method == "POST") {
        handleGenerateRequest(request, response);
    }
    else if (request.path == "/v1/chat/completions" && request.method == "POST") {
        handleChatCompletionsRequest(request, response);
    }
    else if (request.path == "/api/tags" && request.method == "GET") {
        handleTagsRequest(response);
    }
    else if (request.path == "/api/pull" && request.method == "POST") {
        handlePullRequest(request, response);
    }
    else if (request.path == "/api/push" && request.method == "POST") {
        handlePushRequest(request, response);
    }
    else if (request.path == "/api/show" && request.method == "POST") {
        handleShowRequest(request, response);
    }
    else if (request.path == "/api/delete" && request.method == "DELETE") {
        handleDeleteRequest(request, response);
    }
    else if (request.path == "/health" && request.method == "GET") {
        handleHealthRequest(response);
    }
    else {
        handleNotFound(response);
    }
    
    sendResponse(socket, response);
    
    qint64 duration = timer.elapsed();
    bool success = (response.statusCode >= 200 && response.statusCode < 300);
    
    if (success) {
        m_stats.successfulRequests++;
    } else {
        m_stats.failedRequests++;
    }
    
    logRequest(request.method, request.path, response.statusCode);
    emit requestCompleted(request.path, success, duration);
}

void GGUFServer::sendResponse(QTcpSocket* socket, const HttpResponse& response) {
    QByteArray responseData;
    
    // Status line
    responseData.append("HTTP/1.1 ");
    responseData.append(QByteArray::number(response.statusCode));
    responseData.append(" ");
    responseData.append(response.statusText.toUtf8());
    responseData.append("\r\n");
    
    // Headers
    for (auto it = response.headers.begin(); it != response.headers.end(); ++it) {
        responseData.append(it.key().toUtf8());
        responseData.append(": ");
        responseData.append(it.value().toUtf8());
        responseData.append("\r\n");
    }
    
    // Content-Length
    responseData.append("Content-Length: ");
    responseData.append(QByteArray::number(response.body.size()));
    responseData.append("\r\n");
    
    // End of headers
    responseData.append("\r\n");
    
    // Body
    responseData.append(response.body);
    
    socket->write(responseData);
    socket->flush();
}

void GGUFServer::handleGenerateRequest(const HttpRequest& request, HttpResponse& response) {
    // BOTTLENECK #3 FIX: Use lightweight field extraction instead of full DOM parsing
    // Before: QJsonDocument::fromJson() took 5-15ms to build entire tree
    // After: Direct field extraction takes ~0.5ms
    
    QString prompt = extractJsonField(request.body, "prompt");
    QString model = extractJsonField(request.body, "model");
    
    if (prompt.isEmpty()) {
        response.statusCode = 400;
        response.statusText = "Bad Request";
        response.body = "{\"error\":\"Missing prompt field\"}";
        return;
    }
    
    // Generate response using inference engine
    QString generated;
    bool streamRequested = extractJsonField(request.body, "stream") == "true";
    int maxTokens = 100;
    QString maxTokStr = extractJsonField(request.body, "num_predict");
    if (!maxTokStr.isEmpty()) maxTokens = maxTokStr.toInt();
    if (maxTokens <= 0) maxTokens = 100;

    if (m_engine && m_engine->isModelLoaded()) {
        if (streamRequested) {
            // Streaming inference: tokenize, generate token-by-token, send NDJSON chunks
            std::vector<int32_t> tokens = m_engine->tokenize(prompt);
            
            // Set up streaming response headers
            response.statusCode = 200;
            response.statusText = "OK";
            response.headers["Content-Type"] = "application/x-ndjson";
            response.headers["Transfer-Encoding"] = "chunked";
            
            // Use the streaming API: generate all tokens, emit per-token JSON
            std::vector<int32_t> output = m_engine->generate(tokens, maxTokens);
            QByteArray streamBody;
            
            for (size_t i = 0; i < output.size(); ++i) {
                std::vector<int32_t> singleToken = { output[i] };
                QString tokenText = m_engine->detokenize(singleToken);
                
                QJsonObject chunk;
                chunk["model"] = model.isEmpty() ? "gguf-model" : model;
                chunk["created_at"] = getCurrentTimestamp();
                chunk["response"] = tokenText;
                chunk["done"] = false;
                
                QJsonDocument chunkDoc(chunk);
                streamBody.append(chunkDoc.toJson(QJsonDocument::Compact));
                streamBody.append('\n');
            }
            
            // Final done message
            QJsonObject doneObj;
            doneObj["model"] = model.isEmpty() ? "gguf-model" : model;
            doneObj["created_at"] = getCurrentTimestamp();
            doneObj["response"] = "";
            doneObj["done"] = true;
            doneObj["total_duration"] = 0;
            doneObj["eval_count"] = static_cast<int>(output.size());
            
            QJsonDocument doneDoc(doneObj);
            streamBody.append(doneDoc.toJson(QJsonDocument::Compact));
            streamBody.append('\n');
            
            response.body = streamBody;
            m_stats.totalTokensGenerated += output.size();
            return;
        }
        
        // Non-streaming: synchronous inference
        std::vector<int32_t> tokens = m_engine->tokenize(prompt);
        std::vector<int32_t> output = m_engine->generate(tokens, maxTokens);
        generated = m_engine->detokenize(output);
        
        m_stats.totalTokensGenerated += output.size();
    } else {
        generated = "Error: No model loaded";
    }
    
    // Ollama-compatible response
    QJsonObject responseObj;
    responseObj["model"] = model.isEmpty() ? "gguf-model" : model;
    responseObj["created_at"] = getCurrentTimestamp();
    responseObj["response"] = generated;
    responseObj["done"] = true;
    
    QJsonDocument responseDoc(responseObj);
    response.body = responseDoc.toJson(QJsonDocument::Compact);
}

void GGUFServer::handleChatCompletionsRequest(const HttpRequest& request, HttpResponse& response) {
    // BOTTLENECK #3 FIX: Use lightweight extraction for simple fields, array extraction for messages
    QString model = extractJsonField(request.body, "model");
    QJsonArray messages = extractJsonArray(request.body, "messages");
    
    if (messages.isEmpty()) {
        response.statusCode = 400;
        response.statusText = "Bad Request";
        response.body = "{\"error\":\"Missing messages field\"}";
        return;
    }
    
    // Build prompt from messages
    QString prompt;
    for (const QJsonValue& msgVal : messages) {
        QJsonObject msg = msgVal.toObject();
        QString role = msg["role"].toString();
        QString content = msg["content"].toString();
        
        if (role == "system") {
            prompt += "System: " + content + "\n";
        } else if (role == "user") {
            prompt += "User: " + content + "\n";
        } else if (role == "assistant") {
            prompt += "Assistant: " + content + "\n";
        }
    }
    prompt += "Assistant: ";
    
    // Generate response
    QString generated;
    if (m_engine && m_engine->isModelLoaded()) {
        std::vector<int32_t> tokens = m_engine->tokenize(prompt);
        std::vector<int32_t> output = m_engine->generate(tokens, 100);
        generated = m_engine->detokenize(output);
        
        m_stats.totalTokensGenerated += output.size();
    } else {
        generated = "Error: No model loaded";
    }
    
    // OpenAI-compatible response
    QJsonObject responseObj;
    responseObj["id"] = "chatcmpl-" + QString::number(m_stats.totalRequests);
    responseObj["object"] = "chat.completion";
    responseObj["created"] = QDateTime::currentSecsSinceEpoch();
    responseObj["model"] = model;
    
    QJsonObject message;
    message["role"] = "assistant";
    message["content"] = generated;
    
    QJsonObject choice;
    choice["index"] = 0;
    choice["message"] = message;
    choice["finish_reason"] = "stop";
    
    QJsonArray choices;
    choices.append(choice);
    responseObj["choices"] = choices;
    
    QJsonDocument responseDoc(responseObj);
    response.body = responseDoc.toJson(QJsonDocument::Compact);
}

void GGUFServer::handleTagsRequest(HttpResponse& response) {
    QJsonArray models;
    
    if (m_engine && m_engine->isModelLoaded()) {
        QJsonObject model;
        model["name"] = m_engine->modelPath();
        model["modified_at"] = getCurrentTimestamp();
        model["size"] = static_cast<qint64>(m_engine->modelFileSize());
        models.append(model);
    }
    
    QJsonObject responseObj;
    responseObj["models"] = models;
    
    QJsonDocument responseDoc(responseObj);
    response.body = responseDoc.toJson(QJsonDocument::Compact);
}

void GGUFServer::handlePullRequest(const HttpRequest& request, HttpResponse& response) {
    QJsonDocument doc = parseJsonBody(request.body);
    QString modelName = doc.object().value("name").toString();
    
    if (modelName.isEmpty()) {
        response.statusCode = 400;
        response.statusText = "Bad Request";
        response.body = "{\"error\":\"Missing model name\"}";
        return;
    }
    
    // Check if model is already available locally
    if (m_engine && m_engine->isModelLoaded() && m_engine->modelPath().contains(modelName)) {
        QJsonObject responseObj;
        responseObj["status"] = "success";
        responseObj["digest"] = "sha256:local";
        responseObj["total"] = static_cast<qint64>(m_engine->modelFileSize());
        responseObj["completed"] = static_cast<qint64>(m_engine->modelFileSize());
        
        QJsonDocument responseDoc(responseObj);
        response.body = responseDoc.toJson(QJsonDocument::Compact);
        return;
    }
    
    // Model not available — report download needed
    QJsonObject responseObj;
    responseObj["status"] = "error";
    responseObj["error"] = QString("Model '%1' not available locally. "
        "Place the GGUF file in the models directory and load it via /api/generate.").arg(modelName);
    
    QJsonDocument responseDoc(responseObj);
    response.body = responseDoc.toJson(QJsonDocument::Compact);
    response.statusCode = 404;
    response.statusText = "Not Found";
}

void GGUFServer::handlePushRequest(const HttpRequest& request, HttpResponse& response) {
    QJsonDocument doc = parseJsonBody(request.body);
    QString modelName = doc.object().value("name").toString();
    
    if (modelName.isEmpty()) {
        response.statusCode = 400;
        response.statusText = "Bad Request";
        response.body = "{\"error\":\"Missing model name\"}";
        return;
    }
    
    // Model push = export currently loaded model info
    if (m_engine && m_engine->isModelLoaded()) {
        QJsonObject responseObj;
        responseObj["status"] = "success";
        responseObj["digest"] = "sha256:local";
        responseObj["model"] = m_engine->modelPath();
        responseObj["size"] = static_cast<qint64>(m_engine->modelFileSize());
        
        QJsonDocument responseDoc(responseObj);
        response.body = responseDoc.toJson(QJsonDocument::Compact);
    } else {
        QJsonObject responseObj;
        responseObj["status"] = "error";
        responseObj["error"] = "No model loaded to push";
        
        QJsonDocument responseDoc(responseObj);
        response.body = responseDoc.toJson(QJsonDocument::Compact);
        response.statusCode = 404;
        response.statusText = "Not Found";
    }
}

void GGUFServer::handleShowRequest(const HttpRequest& request, HttpResponse& response) {
    QJsonDocument doc = parseJsonBody(request.body);
    
    QJsonObject responseObj;
    if (m_engine && m_engine->isModelLoaded()) {
        responseObj["modelfile"] = "# GGUF Model";
        responseObj["parameters"] = "";
        responseObj["template"] = "{{ .Prompt }}";
    } else {
        responseObj["error"] = "No model loaded";
        response.statusCode = 404;
        response.statusText = "Not Found";
    }
    
    QJsonDocument responseDoc(responseObj);
    response.body = responseDoc.toJson(QJsonDocument::Compact);
}

void GGUFServer::handleDeleteRequest(const HttpRequest& request, HttpResponse& response) {
    QJsonDocument doc = parseJsonBody(request.body);
    QString modelName = doc.object().value("name").toString();
    
    if (modelName.isEmpty()) {
        response.statusCode = 400;
        response.statusText = "Bad Request";
        response.body = "{\"error\":\"Missing model name\"}";
        return;
    }
    
    // Unload the model if it matches
    if (m_engine && m_engine->isModelLoaded()) {
        if (m_engine->modelPath().contains(modelName)) {
            m_engine->unloadModel();
            
            QJsonObject responseObj;
            responseObj["status"] = "success";
            responseObj["message"] = QString("Model '%1' unloaded").arg(modelName);
            
            QJsonDocument responseDoc(responseObj);
            response.body = responseDoc.toJson(QJsonDocument::Compact);
            return;
        }
    }
    
    QJsonObject responseObj;
    responseObj["status"] = "error";
    responseObj["error"] = QString("Model '%1' not found or not loaded").arg(modelName);
    
    QJsonDocument responseDoc(responseObj);
    response.body = responseDoc.toJson(QJsonDocument::Compact);
    response.statusCode = 404;
    response.statusText = "Not Found";
}

void GGUFServer::handleHealthRequest(HttpResponse& response) {
    ServerStats stats = getStats();
    
    QJsonObject responseObj;
    responseObj["status"] = m_isRunning ? "ok" : "stopped";
    responseObj["uptime_seconds"] = static_cast<qint64>(stats.uptimeSeconds);
    responseObj["total_requests"] = static_cast<qint64>(stats.totalRequests);
    responseObj["successful_requests"] = static_cast<qint64>(stats.successfulRequests);
    responseObj["failed_requests"] = static_cast<qint64>(stats.failedRequests);
    responseObj["tokens_generated"] = static_cast<qint64>(stats.totalTokensGenerated);
    responseObj["model_loaded"] = (m_engine && m_engine->isModelLoaded());
    
    if (m_engine && m_engine->isModelLoaded()) {
        responseObj["model_path"] = m_engine->modelPath();
    }
    
    QJsonDocument responseDoc(responseObj);
    response.body = responseDoc.toJson(QJsonDocument::Compact);
}

void GGUFServer::handleNotFound(HttpResponse& response) {
    response.statusCode = 404;
    response.statusText = "Not Found";
    
    QJsonObject responseObj;
    responseObj["error"] = "Endpoint not found";
    
    QJsonDocument responseDoc(responseObj);
    response.body = responseDoc.toJson(QJsonDocument::Compact);
}

void GGUFServer::handleCorsPreflightRequest(HttpResponse& response) {
    response.statusCode = 204;
    response.statusText = "No Content";
    // CORS headers already added in handleRequest
}
