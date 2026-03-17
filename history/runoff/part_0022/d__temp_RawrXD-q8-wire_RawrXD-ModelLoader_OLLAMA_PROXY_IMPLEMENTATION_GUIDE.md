# Ollama Hotpatch Proxy - Production Implementation Guide

## Overview
This guide provides detailed implementation strategies for the production-ready Ollama Hotpatch Proxy.

## Architecture

```
Client → [Proxy:11435] → [Ollama:11434]
         ↓ Intercept
         ↓ Patch Response
         ↓ Forward
Client ← Modified Response
```

## Critical Implementation Areas

### 1. HTTP Request Parsing (`parseHttpRequest`)

**Goal**: Robustly parse HTTP/1.1 requests with strict validation.

```cpp
HttpRequest OllamaHotpatchProxy::parseHttpRequest(QTcpSocket* clientSocket, QByteArray& rawData) {
    HttpRequest request;
    request.isValid = false;
    
    // Find end of headers (\r\n\r\n)
    int headerEnd = rawData.indexOf("\r\n\r\n");
    if (headerEnd == -1) {
        // Incomplete request - check size limit
        if (rawData.size() > MAX_LINE_LENGTH) {
            emit error(QString("Request headers exceed maximum size (%1)")
                      .arg(socketDescriptor));
            sendHttpResponse(clientSocket, {400, "Bad Request", {}, 
                           "Request headers too large"});
            return request;
        }
        return request; // Wait for more data
    }
    
    // Split into lines
    QList<QByteArray> lines = rawData.left(headerEnd).split('\r\n');
    if (lines.isEmpty()) {
        sendHttpResponse(clientSocket, {400, "Bad Request", {}, 
                       "Invalid HTTP request"});
        return request;
    }
    
    // Parse request line: METHOD PATH HTTP/VERSION
    QList<QByteArray> requestLine = lines[0].split(' ');
    if (requestLine.size() != 3) {
        sendHttpResponse(clientSocket, {400, "Bad Request", {}, 
                       "Malformed request line"});
        return request;
    }
    
    request.method = QString::fromLatin1(requestLine[0]);
    request.path = QString::fromLatin1(requestLine[1]);
    request.httpVersion = QString::fromLatin1(requestLine[2]);
    
    // Parse headers
    for (int i = 1; i < lines.size(); ++i) {
        int colonPos = lines[i].indexOf(':');
        if (colonPos > 0) {
            QString key = QString::fromLatin1(lines[i].left(colonPos)).trimmed();
            QString value = QString::fromLatin1(lines[i].mid(colonPos + 1)).trimmed();
            request.headers[key.toLower()] = value;
        }
    }
    
    // Extract body if Content-Length present
    if (request.headers.contains("content-length")) {
        qint64 contentLength = request.headers["content-length"].toLongLong();
        
        // Enforce size limit
        if (contentLength > m_maxRequestSize) {
            sendHttpResponse(clientSocket, {413, "Payload Too Large", {}, 
                           "Request body exceeds maximum size"});
            return request;
        }
        
        int bodyStart = headerEnd + 4;
        if (rawData.size() >= bodyStart + contentLength) {
            request.body = rawData.mid(bodyStart, contentLength);
            rawData.remove(0, bodyStart + contentLength);
            request.isValid = true;
        }
    } else {
        // No body expected
        rawData.remove(0, headerEnd + 4);
        request.isValid = true;
    }
    
    if (m_debugLogging && request.isValid) {
        emit debug(QString("Parsed: %1 %2").arg(request.method, request.path));
    }
    
    return request;
}
```

**Key Features**:
- Validates request line format
- Enforces header size limits (`MAX_LINE_LENGTH`)
- Enforces body size limits (`m_maxRequestSize`)
- Sends appropriate HTTP error responses (400, 413)
- Handles partial requests (returns invalid until complete)

---

### 2. Request Forwarding (`forwardRequest`)

**Goal**: Forward request to upstream Ollama, rewriting necessary headers.

```cpp
void OllamaHotpatchProxy::forwardRequest(QTcpSocket* clientSocket, 
                                         const HttpRequest& request) {
    QUrl url(m_upstreamUrl + request.path);
    QNetworkRequest netRequest(url);
    
    // Copy headers, rewriting Host
    for (auto it = request.headers.begin(); it != request.headers.end(); ++it) {
        QString header = it.key();
        if (header == "host") {
            netRequest.setRawHeader("Host", url.host().toLatin1());
        } else {
            netRequest.setRawHeader(header.toLatin1(), it.value().toLatin1());
        }
    }
    
    // Send request upstream
    QNetworkReply* reply = nullptr;
    if (request.method == "POST") {
        reply = m_networkManager->post(netRequest, request.body);
    } else if (request.method == "GET") {
        reply = m_networkManager->get(netRequest);
    } else if (request.method == "DELETE") {
        reply = m_networkManager->deleteResource(netRequest);
    } else {
        sendHttpResponse(clientSocket, {405, "Method Not Allowed", {}, 
                       "Unsupported HTTP method"});
        return;
    }
    
    // Track connection
    ProxyConnection& conn = m_activeConnections[clientSocket];
    conn.clientSocket = clientSocket;
    conn.upstreamReply = reply;
    conn.parsedRequest = request;
    conn.requestEndpoint = request.path;
    
    // Detect streaming endpoints
    conn.isStreaming = (request.path.contains("/api/generate") || 
                       request.path.contains("/api/chat"));
    
    // Connect signals
    connect(reply, &QNetworkReply::readyRead, 
            this, &OllamaHotpatchProxy::onUpstreamReadyRead);
    connect(reply, &QNetworkReply::finished, 
            this, &OllamaHotpatchProxy::onUpstreamFinished);
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::error),
            this, &OllamaHotpatchProxy::onUpstreamError);
    
    {
        QMutexLocker lock(&m_statsMutex);
        m_stats.totalRequests++;
    }
    
    emit requestReceived(clientSocket->socketDescriptor(), 
                        request.method, request.path);
}
```

---

### 3. **CRITICAL**: Streaming Response Handling (`onUpstreamReadyRead`)

**Goal**: Parse NDJSON streams, patch each JSON object, forward patched stream.

```cpp
void OllamaHotpatchProxy::onUpstreamReadyRead() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    // Find associated client socket
    QTcpSocket* clientSocket = nullptr;
    ProxyConnection* connection = nullptr;
    
    for (auto it = m_activeConnections.begin(); it != m_activeConnections.end(); ++it) {
        if (it.value().upstreamReply == reply) {
            clientSocket = it.key();
            connection = &it.value();
            break;
        }
    }
    
    if (!clientSocket || !connection) {
        reply->abort();
        return;
    }
    
    if (connection->isStreaming) {
        processStreamingChunk(clientSocket, reply, *connection);
    } else {
        processNonStreamingResponse(clientSocket, reply, *connection);
    }
}

void OllamaHotpatchProxy::processStreamingChunk(QTcpSocket* clientSocket,
                                                QNetworkReply* reply,
                                                ProxyConnection& connection) {
    // Read available data
    QByteArray chunk = reply->readAll();
    if (chunk.isEmpty()) return;
    
    {
        QMutexLocker lock(&m_statsMutex);
        m_stats.bytesProxied += chunk.size();
    }
    
    // Append to partial line buffer
    connection.partialJsonLine.append(chunk);
    
    // Process complete lines (NDJSON format)
    int newlinePos;
    while ((newlinePos = connection.partialJsonLine.indexOf('\n')) != -1) {
        QByteArray jsonLine = connection.partialJsonLine.left(newlinePos);
        connection.partialJsonLine.remove(0, newlinePos + 1);
        
        if (jsonLine.trimmed().isEmpty()) continue;
        
        // Parse JSON
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(jsonLine, &parseError);
        
        if (parseError.error != QJsonParseError::NoError) {
            if (m_debugLogging) {
                emit debug(QString("JSON parse error in stream: %1")
                          .arg(parseError.errorString()));
            }
            // Forward unparseable lines as-is
            clientSocket->write(jsonLine + "\n");
            continue;
        }
        
        QJsonObject original = doc.object();
        quint64 patchCount = 0;
        
        // Apply appropriate patching based on endpoint
        QJsonObject patched;
        if (connection.requestEndpoint.contains("/api/chat")) {
            patched = patchChatCompletionDelta(original, patchCount);
        } else {
            patched = patchGenerateResponse(original, patchCount);
        }
        
        if (patchCount > 0) {
            QMutexLocker lock(&m_statsMutex);
            m_stats.patchedResponses++;
        }
        
        // Serialize and forward
        QByteArray patchedJson = QJsonDocument(patched).toJson(QJsonDocument::Compact);
        clientSocket->write(patchedJson + "\n");
        clientSocket->flush();
        
        if (m_debugLogging && patchCount > 0) {
            emit debug(QString("Patched streaming chunk (%1 patches)")
                      .arg(patchCount));
        }
    }
}
```

**Critical Notes**:
- **NDJSON**: Each line is a separate JSON object
- **Partial Lines**: Must buffer incomplete lines between chunks
- **Flushing**: Call `flush()` after each line to maintain real-time streaming
- **Error Handling**: Forward unparseable lines to maintain stream integrity

---

### 4. JSON Patching Functions

**Goal**: Apply hotpatch rules to the generated text within JSON responses.

```cpp
QJsonObject OllamaHotpatchProxy::patchChatCompletionDelta(
    const QJsonObject& original, quint64& patchesAppliedCount) {
    
    QJsonObject patched = original;
    
    // Extract message content from delta
    // Schema: {"message": {"content": "text here"}, ...}
    if (!original.contains("message")) {
        return patched;
    }
    
    QJsonObject message = original["message"].toObject();
    if (!message.contains("content")) {
        return patched;
    }
    
    QString originalContent = message["content"].toString();
    if (originalContent.isEmpty()) {
        return patched;
    }
    
    // Apply all hotpatch rules
    QString patchedContent = originalContent;
    quint64 tempCount = 0;
    
    QMutexLocker lock(&m_rulesMutex);
    patchedContent = applyTokenReplacements(patchedContent, tempCount);
    patchesAppliedCount += tempCount;
    
    patchedContent = applyRegexFilters(patchedContent, tempCount);
    patchesAppliedCount += tempCount;
    
    patchedContent = applyFactInjections(patchedContent, tempCount);
    patchesAppliedCount += tempCount;
    
    patchedContent = applySafetyFilters(patchedContent, tempCount);
    patchesAppliedCount += tempCount;
    
    patchedContent = applyCustomProcessors(patchedContent);
    lock.unlock();
    
    // Update JSON if changed
    if (patchedContent != originalContent) {
        message["content"] = patchedContent;
        patched["message"] = message;
    }
    
    return patched;
}

QJsonObject OllamaHotpatchProxy::patchGenerateResponse(
    const QJsonObject& original, quint64& patchesAppliedCount) {
    
    QJsonObject patched = original;
    
    // Extract response text
    // Schema: {"response": "text here", ...}
    if (!original.contains("response")) {
        return patched;
    }
    
    QString originalResponse = original["response"].toString();
    if (originalResponse.isEmpty()) {
        return patched;
    }
    
    // Apply all hotpatch rules
    QString patchedResponse = originalResponse;
    quint64 tempCount = 0;
    
    QMutexLocker lock(&m_rulesMutex);
    patchedResponse = applyTokenReplacements(patchedResponse, tempCount);
    patchesAppliedCount += tempCount;
    
    patchedResponse = applyRegexFilters(patchedResponse, tempCount);
    patchesAppliedCount += tempCount;
    
    patchedResponse = applyFactInjections(patchedResponse, tempCount);
    patchesAppliedCount += tempCount;
    
    patchedResponse = applySafetyFilters(patchedResponse, tempCount);
    patchesAppliedCount += tempCount;
    
    patchedResponse = applyCustomProcessors(patchedResponse);
    lock.unlock();
    
    // Update JSON if changed
    if (patchedResponse != originalResponse) {
        patched["response"] = patchedResponse;
    }
    
    return patched;
}
```

---

### 5. Hotpatch Rule Application

**Token Replacement**:
```cpp
QString OllamaHotpatchProxy::applyTokenReplacements(const QString& text, 
                                                     quint64& tokensReplacedCount) {
    QString result = text;
    
    for (auto it = m_tokenReplacements.begin(); 
         it != m_tokenReplacements.end(); ++it) {
        int count = 0;
        int pos = 0;
        while ((pos = result.indexOf(it.key(), pos, Qt::CaseInsensitive)) != -1) {
            result.replace(pos, it.key().length(), it.value());
            pos += it.value().length();
            count++;
        }
        tokensReplacedCount += count;
    }
    
    return result;
}
```

**Regex Filters**:
```cpp
QString OllamaHotpatchProxy::applyRegexFilters(const QString& text,
                                               quint64& patchesAppliedCount) {
    QString result = text;
    
    for (auto it = m_regexFilters.begin(); it != m_regexFilters.end(); ++it) {
        const QRegularExpression& pattern = it.key();
        const QString& replacement = it.value();
        
        int matchCount = 0;
        result = result.replace(pattern, replacement, &matchCount);
        patchesAppliedCount += matchCount;
    }
    
    return result;
}
```

**Safety Filters**:
```cpp
QString OllamaHotpatchProxy::applySafetyFilters(const QString& text,
                                                quint64& filtersTriggeredCount) {
    QString result = text;
    
    for (const QRegularExpression& pattern : m_safetyPatterns) {
        QRegularExpressionMatch match = pattern.match(result);
        if (match.hasMatch()) {
            // Replace unsafe content with [FILTERED]
            result = result.replace(pattern, "[FILTERED]");
            filtersTriggeredCount++;
            
            if (m_debugLogging) {
                emit debug(QString("Safety filter triggered: %1")
                          .arg(match.captured(0)));
            }
        }
    }
    
    return result;
}
```

---

### 6. Error Handling

**Upstream Errors**:
```cpp
void OllamaHotpatchProxy::onUpstreamError(QNetworkReply::NetworkError code) {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    // Find client socket
    QTcpSocket* clientSocket = nullptr;
    for (auto it = m_activeConnections.begin(); 
         it != m_activeConnections.end(); ++it) {
        if (it.value().upstreamReply == reply) {
            clientSocket = it.key();
            break;
        }
    }
    
    if (clientSocket) {
        QString errorMsg = reply->errorString();
        emit error(QString("Upstream error: %1").arg(errorMsg));
        
        // Send 502 Bad Gateway
        sendHttpResponse(clientSocket, {
            502, "Bad Gateway", {},
            QString("Upstream Ollama server error: %1").arg(errorMsg).toLatin1()
        });
        
        clientSocket->disconnectFromHost();
        m_activeConnections.remove(clientSocket);
    }
    
    {
        QMutexLocker lock(&m_statsMutex);
        m_stats.upstreamErrors++;
    }
}
```

---

## Testing Strategy

### 1. Unit Tests
- HTTP parsing with malformed requests
- Token replacement edge cases
- Regex filter validation
- JSON patching correctness

### 2. Integration Tests
- End-to-end streaming test with real Ollama
- Multiple concurrent clients
- Large request/response handling
- Error recovery scenarios

### 3. Load Testing
- Stress test with 100+ concurrent connections
- Memory leak detection (valgrind/sanitizers)
- Performance profiling

---

## Deployment Checklist

- [ ] Implement all private methods
- [ ] Add comprehensive error handling
- [ ] Add unit tests (>80% coverage)
- [ ] Add integration tests
- [ ] Perform load testing
- [ ] Add configuration file support (JSON/YAML)
- [ ] Add systemd service file (Linux)
- [ ] Add logging to file (not just signals)
- [ ] Add metrics endpoint (e.g., /metrics for Prometheus)
- [ ] Security audit (input validation, DoS protection)
- [ ] Documentation (API docs, deployment guide)

---

## Performance Considerations

1. **Memory**: Use streaming throughout - never buffer entire responses
2. **CPU**: Compile regex patterns once in `add*Filter()` methods
3. **Concurrency**: Qt event loop handles I/O - no threads needed for basic operation
4. **Locks**: Minimize lock scope - only protect shared data (rules, stats)

---

## Security Considerations

1. **Input Validation**: Strictly validate all HTTP inputs
2. **Resource Limits**: Enforce `m_maxRequestSize` to prevent DoS
3. **Injection**: Never eval/execute patched content
4. **Upstream Trust**: Validate Ollama responses (prevent SSRF)
5. **Logging**: Sanitize logs to prevent log injection

---

## Example Usage

```cpp
int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    OllamaHotpatchProxy proxy;
    proxy.setUpstreamUrl("http://localhost:11434");
    proxy.setDebugLogging(true);
    
    // Add rules
    proxy.addTokenReplacement("definitely", "likely");
    proxy.addRegexFilter(QRegularExpression(R"(\b(always|never)\b)", 
                        QRegularExpression::CaseInsensitiveOption), 
                        "often");
    proxy.addSafetyFilter(QRegularExpression(R"(unsafe_word)", 
                         QRegularExpression::CaseInsensitiveOption));
    
    // Connect signals
    QObject::connect(&proxy, &OllamaHotpatchProxy::error,
                    [](const QString& msg) {
        qWarning() << "Error:" << msg;
    });
    
    QObject::connect(&proxy, &OllamaHotpatchProxy::debug,
                    [](const QString& msg) {
        qDebug() << "Debug:" << msg;
    });
    
    // Start proxy
    if (proxy.start(11435)) {
        qInfo() << "Proxy started on port 11435";
        qInfo() << "Forward to Ollama at" << proxy.upstreamUrl();
        return app.exec();
    } else {
        qCritical() << "Failed to start proxy";
        return 1;
    }
}
```
