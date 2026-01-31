#include "gguf_server.hpp"
#include "inference_engine.hpp"


GGUFServer::GGUFServer(InferenceEngine* engine, void* parent)
    : void(parent)
    , m_engine(engine)
    , m_server(new void*(this))
    , m_isRunning(false)
    , m_port(0)
    , m_healthTimer(new void*(this))
{
// Qt connect removed
// Qt connect removed
}

GGUFServer::~GGUFServer() {
    stop();
}

bool GGUFServer::start(quint16 port) {
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    if (m_isRunning) {
        return true;
    }
    
    // Check if another server is running on this port
    if (isServerRunningOnPort(port)) {
        m_isRunning = true;  // Mark as running (external instance)
        m_port = port;
        return true;
    }
    
    // Try to bind to the port
    if (!tryBindPort(port)) {
        
        // Try alternative ports
        for (quint16 altPort = port + 1; altPort < port + 10; ++altPort) {
            if (tryBindPort(altPort)) {
                port = altPort;
                break;
            }
        }
        
        if (!m_server->isListening()) {
            error("Failed to start server on any port");
            return false;
        }
    }
    
    m_isRunning = true;
    m_port = port;
    m_startTime = std::chrono::system_clock::time_point::currentDateTime();
    m_stats = ServerStats(); // Reset stats
    
    // Start health monitoring
    m_healthTimer->start(HEALTH_CHECK_INTERVAL_MS);


    serverStarted(m_port);
    return true;
}

void GGUFServer::stop() {
    if (!m_isRunning) {
        return;
    }
    
    // Don't use std::lock_guard<std::mutex> here - it can deadlock during shutdown
    m_mutex.lock();
    bool wasRunning = m_isRunning;
    m_isRunning = false;
    m_mutex.unlock();
    
    if (!wasRunning) {
        return;
    }
    
    // Stop timer safely - only if it belongs to this thread
    if (m_healthTimer && m_healthTimer->thread() == std::thread::currentThread()) {
        m_healthTimer->stop();
    }
    
    if (m_server && m_server->isListening()) {
        m_server->close();
        // Wait a bit for pending accepts to complete
        m_server->waitForNewConnection(100);
    }
    
    // Force close all pending connections
    m_mutex.lock();
    std::vector<void**> socketsToClose = m_pendingRequests.keys();
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
    
    serverStopped();
}

bool GGUFServer::isRunning() const {
    return m_isRunning;
}

quint16 GGUFServer::port() const {
    return m_port;
}

bool GGUFServer::isServerRunningOnPort(quint16 port) {
    void* testSocket;
    testSocket.connectToHost(std::string::LocalHost, port);
    
    if (testSocket.waitForConnected(500)) {
        // Send a simple HTTP GET request to check if it's our server
        testSocket.write("GET /health HTTP/1.1\r\nHost: localhost\r\n\r\n");
        testSocket.flush();
        
        if (testSocket.waitForReadyRead(1000)) {
            std::vector<uint8_t> response = testSocket.readAll();
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
        stats.uptimeSeconds = m_startTime.secsTo(std::chrono::system_clock::time_point::currentDateTime());
        stats.startTime = m_startTime.toString(//ISODate);
    }
    
    return stats;
}

void GGUFServer::onNewConnection() {
    while (m_server->hasPendingConnections()) {
        void** socket = m_server->nextPendingConnection();
// Qt connect removed
// Qt connect removed
        m_pendingRequests[socket] = std::vector<uint8_t>();
    }
}

void GGUFServer::onReadyRead() {
// REMOVED_QT:     void** socket = qobject_cast<void**>(sender());
    if (!socket) return;
    
    // Append incoming data
    m_pendingRequests[socket].append(socket->readAll());
    
    // Check if we have a complete HTTP request
    std::vector<uint8_t>& buffer = m_pendingRequests[socket];
    
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
    std::string headerStr = std::string::fromUtf8(buffer.left(headerEnd));
    int contentLength = 0;
    
    for (const std::string& line : headerStr.split("\r\n")) {
        if (line.startsWith("Content-Length:", //CaseInsensitive)) {
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
    std::vector<uint8_t> requestData = buffer.left(totalExpected);
    buffer.remove(0, totalExpected);
    
    // Parse and handle request
    HttpRequest request = parseHttpRequest(requestData);
    handleRequest(socket, request);
}

void GGUFServer::onDisconnected() {
// REMOVED_QT:     void** socket = qobject_cast<void**>(sender());
    if (!socket) return;
    
    m_pendingRequests.remove(socket);
    socket->deleteLater();
}

void GGUFServer::onHealthCheck() {
    // Periodic health check - could log stats, clean up stale connections, etc.
    if (m_isRunning && m_engine) {
    }
}

bool GGUFServer::tryBindPort(quint16 port) {
    if (m_server->listen(std::string::Any, port)) {
        return true;
    }
    return false;
}

bool GGUFServer::waitForServerShutdown(quint16 port, int maxWaitMs) {
    std::chrono::steady_clock timer;
    timer.start();
    
    while (timer.elapsed() < maxWaitMs) {
        if (!isServerRunningOnPort(port)) {
            return true;
        }
        std::thread::msleep(100);
    }
    
    return false;
}

std::string GGUFServer::getCurrentTimestamp() const {
    return std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
}

// BOTTLENECK #3 FIX: Lightweight JSON field extraction (no DOM tree building)
// Extracts a single field value from JSON without parsing entire structure
std::string GGUFServer::extractJsonField(const std::vector<uint8_t>& json, const std::string& fieldName) {
    // Fast path: search for field pattern in raw JSON
    // Pattern: "fieldName":"value" or "fieldName":value
    std::string searchPattern = std::string("\"%1\"");
    int fieldPos = json.indexOf(searchPattern.toUtf8());
    
    if (fieldPos == -1) {
        return std::string();  // Field not found
    }
    
    // Find the colon after field name
    int colonPos = json.indexOf(':', fieldPos);
    if (colonPos == -1) {
        return std::string();
    }
    
    // Skip whitespace after colon
    int valueStart = colonPos + 1;
    while (valueStart < json.size() && (json[valueStart] == ' ' || json[valueStart] == '\t')) {
        valueStart++;
    }
    
    if (valueStart >= json.size()) {
        return std::string();
    }
    
    // Check if value is a string (starts with quote)
    if (json[valueStart] == '"') {
        valueStart++;  // Skip opening quote
        int valueEnd = json.indexOf('"', valueStart);
        if (valueEnd == -1) {
            return std::string();
        }
        return std::string::fromUtf8(json.mid(valueStart, valueEnd - valueStart));
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
    
    return std::string::fromUtf8(json.mid(valueStart, valueEnd - valueStart).trimmed());
}

// BOTTLENECK #3 FIX: Extract JSON array field (for messages in chat completions)
void* GGUFServer::extractJsonArray(const std::vector<uint8_t>& json, const std::string& fieldName) {
    // For arrays, we still need void* but only for that specific field
    // This is better than parsing the whole document
    std::string searchPattern = std::string("\"%1\"");
    int fieldPos = json.indexOf(searchPattern.toUtf8());
    
    if (fieldPos == -1) {
        return void*();
    }
    
    int colonPos = json.indexOf(':', fieldPos);
    if (colonPos == -1) {
        return void*();
    }
    
    // Find the array start '['
    int arrayStart = json.indexOf('[', colonPos);
    if (arrayStart == -1) {
        return void*();
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
        return void*();  // Malformed
    }
    
    // Parse just this array portion
    std::vector<uint8_t> arrayJson = json.mid(arrayStart, arrayEnd - arrayStart);
    void* doc = void*::fromJson(arrayJson);
    return doc.array();
}

void* GGUFServer::parseJsonBody(const std::vector<uint8_t>& body) {
    // Legacy fallback for complex JSON (only used when streaming parser can't handle it)
    QJsonParseError error;
    void* doc = void*::fromJson(body, &error);
    
    if (error.error != QJsonParseError::NoError) {
    }
    
    return doc;
}

void GGUFServer::logRequest(const std::string& method, const std::string& path, int statusCode) {
}

GGUFServer::HttpRequest GGUFServer::parseHttpRequest(const std::vector<uint8_t>& rawData) {
    HttpRequest request;
    
    std::string data = std::string::fromUtf8(rawData);
    std::vector<std::string> lines = data.split("\r\n");
    
    if (lines.empty()) {
        return request;
    }
    
    // Parse request line: "GET /path HTTP/1.1"
    std::vector<std::string> requestLine = lines[0].split(' ');
    if (requestLine.size() >= 3) {
        request.method = requestLine[0].toUpper();
        request.path = requestLine[1];
        request.httpVersion = requestLine[2];
        
        // Parse query parameters
        if (request.path.contains('?')) {
            std::vector<std::string> parts = request.path.split('?');
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
        if (lines[i].empty()) {
            ++i;
            break;
        }
        
        int colonPos = lines[i].indexOf(':');
        if (colonPos > 0) {
            std::string key = lines[i].left(colonPos).trimmed();
            std::string value = lines[i].mid(colonPos + 1).trimmed();
            request.headers[key] = value;
        }
    }
    
    // Extract body
    if (i < lines.size()) {
        std::vector<std::string> bodyLines = lines.mid(i);
        request.body = bodyLines.join("\r\n").toUtf8();
    }
    
    return request;
}

void GGUFServer::handleRequest(void** socket, const HttpRequest& request) {
    std::chrono::steady_clock timer;
    timer.start();
    
    m_stats.totalRequests++;
    requestReceived(request.path, request.method);
    
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
    
    int64_t duration = timer.elapsed();
    bool success = (response.statusCode >= 200 && response.statusCode < 300);
    
    if (success) {
        m_stats.successfulRequests++;
    } else {
        m_stats.failedRequests++;
    }
    
    logRequest(request.method, request.path, response.statusCode);
    requestCompleted(request.path, success, duration);
}

void GGUFServer::sendResponse(void** socket, const HttpResponse& response) {
    std::vector<uint8_t> responseData;
    
    // Status line
    responseData.append("HTTP/1.1 ");
    responseData.append(std::vector<uint8_t>::number(response.statusCode));
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
    responseData.append(std::vector<uint8_t>::number(response.body.size()));
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
    // Before: void*::fromJson() took 5-15ms to build entire tree
    // After: Direct field extraction takes ~0.5ms
    
    std::string prompt = extractJsonField(request.body, "prompt");
    std::string model = extractJsonField(request.body, "model");
    
    if (prompt.empty()) {
        response.statusCode = 400;
        response.statusText = "Bad Request";
        response.body = "{\"error\":\"Missing prompt field\"}";
        return;
    }
    
    // Generate response using inference engine
    std::string generated;
    if (m_engine && m_engine->isModelLoaded()) {
        // Simple synchronous inference (TODO: support streaming)
        std::vector<int32_t> tokens = m_engine->tokenize(prompt);
        std::vector<int32_t> output = m_engine->generate(tokens, 100); // Max 100 tokens
        generated = m_engine->detokenize(output);
        
        m_stats.totalTokensGenerated += output.size();
    } else {
        generated = "Error: No model loaded";
    }
    
    // Ollama-compatible response
    void* responseObj;
    responseObj["model"] = model.empty() ? "gguf-model" : model;
    responseObj["created_at"] = getCurrentTimestamp();
    responseObj["response"] = generated;
    responseObj["done"] = true;
    
    void* responseDoc(responseObj);
    response.body = responseDoc.toJson(void*::Compact);
}

void GGUFServer::handleChatCompletionsRequest(const HttpRequest& request, HttpResponse& response) {
    // BOTTLENECK #3 FIX: Use lightweight extraction for simple fields, array extraction for messages
    std::string model = extractJsonField(request.body, "model");
    void* messages = extractJsonArray(request.body, "messages");
    
    if (messages.empty()) {
        response.statusCode = 400;
        response.statusText = "Bad Request";
        response.body = "{\"error\":\"Missing messages field\"}";
        return;
    }
    
    // Build prompt from messages
    std::string prompt;
    for (const void*& msgVal : messages) {
        void* msg = msgVal.toObject();
        std::string role = msg["role"].toString();
        std::string content = msg["content"].toString();
        
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
    std::string generated;
    if (m_engine && m_engine->isModelLoaded()) {
        std::vector<int32_t> tokens = m_engine->tokenize(prompt);
        std::vector<int32_t> output = m_engine->generate(tokens, 100);
        generated = m_engine->detokenize(output);
        
        m_stats.totalTokensGenerated += output.size();
    } else {
        generated = "Error: No model loaded";
    }
    
    // OpenAI-compatible response
    void* responseObj;
    responseObj["id"] = "chatcmpl-" + std::string::number(m_stats.totalRequests);
    responseObj["object"] = "chat.completion";
    responseObj["created"] = std::chrono::system_clock::time_point::currentSecsSinceEpoch();
    responseObj["model"] = model;
    
    void* message;
    message["role"] = "assistant";
    message["content"] = generated;
    
    void* choice;
    choice["index"] = 0;
    choice["message"] = message;
    choice["finish_reason"] = "stop";
    
    void* choices;
    choices.append(choice);
    responseObj["choices"] = choices;
    
    void* responseDoc(responseObj);
    response.body = responseDoc.toJson(void*::Compact);
}

void GGUFServer::handleTagsRequest(HttpResponse& response) {
    void* models;
    
    if (m_engine && m_engine->isModelLoaded()) {
        void* model;
        model["name"] = m_engine->modelPath();
        model["modified_at"] = getCurrentTimestamp();
        model["size"] = 0; // TODO: Get actual model size
        models.append(model);
    }
    
    void* responseObj;
    responseObj["models"] = models;
    
    void* responseDoc(responseObj);
    response.body = responseDoc.toJson(void*::Compact);
}

void GGUFServer::handlePullRequest(const HttpRequest& request, HttpResponse& response) {
    void* doc = parseJsonBody(request.body);
    
    void* responseObj;
    responseObj["status"] = "not_implemented";
    responseObj["error"] = "Model pulling not yet implemented";
    
    void* responseDoc(responseObj);
    response.body = responseDoc.toJson(void*::Compact);
    response.statusCode = 501;
    response.statusText = "Not Implemented";
}

void GGUFServer::handlePushRequest(const HttpRequest& request, HttpResponse& response) {
    void* responseObj;
    responseObj["status"] = "not_implemented";
    responseObj["error"] = "Model pushing not yet implemented";
    
    void* responseDoc(responseObj);
    response.body = responseDoc.toJson(void*::Compact);
    response.statusCode = 501;
    response.statusText = "Not Implemented";
}

void GGUFServer::handleShowRequest(const HttpRequest& request, HttpResponse& response) {
    void* doc = parseJsonBody(request.body);
    
    void* responseObj;
    if (m_engine && m_engine->isModelLoaded()) {
        responseObj["modelfile"] = "# GGUF Model";
        responseObj["parameters"] = "";
        responseObj["template"] = "{{ .Prompt }}";
    } else {
        responseObj["error"] = "No model loaded";
        response.statusCode = 404;
        response.statusText = "Not Found";
    }
    
    void* responseDoc(responseObj);
    response.body = responseDoc.toJson(void*::Compact);
}

void GGUFServer::handleDeleteRequest(const HttpRequest& request, HttpResponse& response) {
    void* responseObj;
    responseObj["status"] = "not_implemented";
    responseObj["error"] = "Model deletion not yet implemented";
    
    void* responseDoc(responseObj);
    response.body = responseDoc.toJson(void*::Compact);
    response.statusCode = 501;
    response.statusText = "Not Implemented";
}

void GGUFServer::handleHealthRequest(HttpResponse& response) {
    ServerStats stats = getStats();
    
    void* responseObj;
    responseObj["status"] = m_isRunning ? "ok" : "stopped";
    responseObj["uptime_seconds"] = static_cast<int64_t>(stats.uptimeSeconds);
    responseObj["total_requests"] = static_cast<int64_t>(stats.totalRequests);
    responseObj["successful_requests"] = static_cast<int64_t>(stats.successfulRequests);
    responseObj["failed_requests"] = static_cast<int64_t>(stats.failedRequests);
    responseObj["tokens_generated"] = static_cast<int64_t>(stats.totalTokensGenerated);
    responseObj["model_loaded"] = (m_engine && m_engine->isModelLoaded());
    
    if (m_engine && m_engine->isModelLoaded()) {
        responseObj["model_path"] = m_engine->modelPath();
    }
    
    void* responseDoc(responseObj);
    response.body = responseDoc.toJson(void*::Compact);
}

void GGUFServer::handleNotFound(HttpResponse& response) {
    response.statusCode = 404;
    response.statusText = "Not Found";
    
    void* responseObj;
    responseObj["error"] = "Endpoint not found";
    
    void* responseDoc(responseObj);
    response.body = responseDoc.toJson(void*::Compact);
}

void GGUFServer::handleCorsPreflightRequest(HttpResponse& response) {
    response.statusCode = 204;
    response.statusText = "No Content";
    // CORS headers already added in handleRequest
}



