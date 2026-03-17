/**
 * \file lsp_client.cpp
 * \brief LSP client implementation with JSON-RPC communication
 * \author RawrXD Team
 * \date 2025-12-07
 */

#include "../include/lsp_client.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>
#include <QFileInfo>
#include <QDebug>
#include <QUrl>
#include <QCoreApplication>

namespace RawrXD {

LSPClient::LSPClient(const LSPServerConfig& config, QObject* parent)
    : QObject(parent)
    , m_config(config)
{
    // Lightweight constructor - defers process creation to initialize()
}

LSPClient::~LSPClient()
{
    stopServer();
}

void LSPClient::initialize() {
    if (m_serverProcess) return;  // Already initialized
    
    m_serverProcess = new QProcess(this);
    
    connect(m_serverProcess, &QProcess::readyReadStandardOutput,
            this, &LSPClient::onServerReadyRead);
    connect(m_serverProcess, &QProcess::errorOccurred,
            this, &LSPClient::onServerError);
    connect(m_serverProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &LSPClient::onServerFinished);
    
    if (m_config.autoStart) {
        startServer();
    }
}

bool LSPClient::startServer()
{
    if (m_serverRunning) {
        qWarning() << "[LSPClient] Server already running";
        return true;
    }
    
    qInfo() << "[LSPClient] Starting" << m_config.language << "server:" << m_config.command;
    
    try {
        m_serverProcess->setProgram(m_config.command);
        m_serverProcess->setArguments(m_config.arguments);
        m_serverProcess->setWorkingDirectory(m_config.workspaceRoot);
        
        // Set environment variables
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("RUST_BACKTRACE", "1");  // For better error reporting
        m_serverProcess->setProcessEnvironment(env);
        
        m_serverProcess->start();
        
        if (!m_serverProcess->waitForStarted(5000)) {
            QString errorMsg = QString("Failed to start %1 server: %2")
                .arg(m_config.language)
                .arg(m_serverProcess->errorString());
            qCritical() << "[LSPClient]" << errorMsg;
            emit serverError(errorMsg);
            return false;
        }
        
        m_serverRunning = true;
        qInfo() << "[LSPClient] Server process started (PID:" << m_serverProcess->processId() << ")";
        
        // Send initialize request
        QJsonObject initializeParams;
        initializeParams["processId"] = static_cast<qint64>(QCoreApplication::applicationPid());
        initializeParams["rootUri"] = buildDocumentUri(m_config.workspaceRoot);
        
        QJsonObject capabilities;
        QJsonObject textDocument;
        
        // Completion support with snippet support
        QJsonObject completion;
        completion["dynamicRegistration"] = false;
        QJsonObject completionItem;
        completionItem["snippetSupport"] = true;
        completionItem["commitCharactersSupport"] = true;
        completionItem["tagSupport"] = QJsonObject{{"valueSet", QJsonArray({1})}};  // Deprecated tag
        completion["completionItem"] = completionItem;
        textDocument["completion"] = completion;
        
        // Hover support
        QJsonObject hover;
        hover["dynamicRegistration"] = false;
        hover["contentFormat"] = QJsonArray({"markdown", "plaintext"});
        textDocument["hover"] = hover;
        
        // Signature help support
        QJsonObject signatureHelp;
        signatureHelp["dynamicRegistration"] = false;
        QJsonObject signatureHelpItem;
        signatureHelpItem["labelOffsetSupport"] = true;
        signatureHelp["signatureInformation"] = signatureHelpItem;
        textDocument["signatureHelp"] = signatureHelp;
        
        // Definition support
        QJsonObject definition;
        definition["dynamicRegistration"] = false;
        definition["linkSupport"] = true;
        textDocument["definition"] = definition;
        
        // References support
        QJsonObject references;
        references["dynamicRegistration"] = false;
        textDocument["references"] = references;
        
        // Rename support
        QJsonObject rename;
        rename["dynamicRegistration"] = false;
        rename["prepareSupport"] = true;
        rename["prepareSupportDefaultBehavior"] = 1;
        textDocument["rename"] = rename;
        
        // Diagnostics support
        QJsonObject publishDiagnostics;
        publishDiagnostics["relatedInformation"] = true;
        publishDiagnostics["tagSupport"] = QJsonObject{{"valueSet", QJsonArray({1, 2})}};
        textDocument["publishDiagnostics"] = publishDiagnostics;
        
        // Formatting support
        QJsonObject formatting;
        formatting["dynamicRegistration"] = false;
        textDocument["formatting"] = formatting;
        
        // Code action support
        QJsonObject codeAction;
        codeAction["dynamicRegistration"] = false;
        QJsonObject codeActionLiteralSupport;
        codeActionLiteralSupport["codeActionKind"] = QJsonObject{{"valueSet", 
            QJsonArray({"quickfix", "refactor", "refactor.extract", "source.organizeImports"})}};
        codeAction["codeActionLiteralSupport"] = codeActionLiteralSupport;
        textDocument["codeAction"] = codeAction;
        
        capabilities["textDocument"] = textDocument;
        
        // Workspace capabilities
        QJsonObject workspace;
        QJsonObject workspaceEdit;
        workspaceEdit["documentChanges"] = true;
        workspaceEdit["resourceOperations"] = QJsonArray({"create", "rename", "delete"});
        workspace["workspaceEdit"] = workspaceEdit;
        capabilities["workspace"] = workspace;
        
        initializeParams["capabilities"] = capabilities;
        
        QJsonObject initRequest;
        initRequest["jsonrpc"] = "2.0";
        initRequest["id"] = m_nextRequestId++;
        initRequest["method"] = "initialize";
        initRequest["params"] = initializeParams;
        
        sendMessage(initRequest);
        
        qInfo() << "[LSPClient] Initialize request sent with full capabilities";
        return true;
    } catch (const std::exception& e) {
        QString errorMsg = QString("Exception starting %1 server: %2")
            .arg(m_config.language)
            .arg(e.what());
        qCritical() << "[LSPClient]" << errorMsg;
        emit serverError(errorMsg);
        m_serverRunning = false;
        return false;
    }
}

void LSPClient::stopServer()
{
    if (!m_serverRunning) return;
    
    qInfo() << "[LSPClient] Stopping server for" << m_config.language;
    
    try {
        // Send shutdown request
        QJsonObject shutdownRequest;
        shutdownRequest["jsonrpc"] = "2.0";
        shutdownRequest["id"] = m_nextRequestId++;
        shutdownRequest["method"] = "shutdown";
        shutdownRequest["params"] = QJsonObject();
        sendMessage(shutdownRequest);
        
        // Send exit notification
        QJsonObject exitNotification;
        exitNotification["jsonrpc"] = "2.0";
        exitNotification["method"] = "exit";
        sendMessage(exitNotification);
        
        // Wait gracefully for shutdown
        if (!m_serverProcess->waitForFinished(2000)) {
            qWarning() << "[LSPClient] Server did not shut down gracefully, terminating";
            m_serverProcess->terminate();
            if (!m_serverProcess->waitForFinished(1000)) {
                qCritical() << "[LSPClient] Server did not terminate, killing";
                m_serverProcess->kill();
                m_serverProcess->waitForFinished(500);
            }
        }
    } catch (const std::exception& e) {
        qCritical() << "[LSPClient] Exception during shutdown:" << e.what();
        if (m_serverProcess && m_serverProcess->state() == QProcess::Running) {
            m_serverProcess->kill();
        }
    }
    
    m_serverRunning = false;
    m_initialized = false;
    
    // Clear all resources
    m_receiveBuffer.clear();
    m_pendingRequests.clear();
    m_diagnostics.clear();
    m_completionCache.clear();
    m_documentVersions.clear();
    
    qInfo() << "[LSPClient] Server stopped and resources cleared";
}

void LSPClient::sendRequest(const QString& method, const QJsonObject& params, ResponseCallback callback)
{
    if (!m_initialized) {
        qWarning() << "[LSPClient] Server not initialized, cannot send request:" << method;
        if (callback) {
            QJsonObject errorResponse;
            errorResponse["error"] = "Server not initialized";
            callback(errorResponse);
        }
        return;
    }
    
    int requestId = m_nextRequestId++;
    
    QJsonObject request;
    request["jsonrpc"] = "2.0";
    request["id"] = requestId;
    request["method"] = method;
    request["params"] = params;
    
    // Track pending request with callback
    PendingRequest pending;
    pending.type = "generic";
    pending.uri = params.value("textDocument").toObject().value("uri").toString();
    pending.line = params.value("position").toObject().value("line").toInt();
    pending.character = params.value("position").toObject().value("character").toInt();
    pending.callback = callback;
    
    m_pendingRequests.insert(requestId, pending);
    
    sendMessage(request);
    
    qDebug() << "[LSPClient] Sent generic request:" << method << "with ID:" << requestId;
}

void LSPClient::openDocument(const QString& uri, const QString& languageId, const QString& text)
{
    if (!m_initialized) {
        qWarning() << "[LSPClient] Server not initialized";
        return;
    }
    
    QJsonObject textDocumentItem;
    textDocumentItem["uri"] = buildDocumentUri(uri);
    textDocumentItem["languageId"] = languageId;
    textDocumentItem["version"] = 1;
    textDocumentItem["text"] = text;
    
    QJsonObject params;
    params["textDocument"] = textDocumentItem;
    
    QJsonObject notification;
    notification["jsonrpc"] = "2.0";
    notification["method"] = "textDocument/didOpen";
    notification["params"] = params;
    
    sendMessage(notification);
    m_documentVersions[uri] = 1;
    
    qDebug() << "[LSPClient] Opened document:" << uri;
}

void LSPClient::closeDocument(const QString& uri)
{
    QJsonObject textDocumentId;
    textDocumentId["uri"] = buildDocumentUri(uri);
    
    QJsonObject params;
    params["textDocument"] = textDocumentId;
    
    QJsonObject notification;
    notification["jsonrpc"] = "2.0";
    notification["method"] = "textDocument/didClose";
    notification["params"] = params;
    
    sendMessage(notification);
    m_documentVersions.remove(uri);
    m_diagnostics.remove(uri);
    
    qDebug() << "[LSPClient] Closed document:" << uri;
}

void LSPClient::updateDocument(const QString& uri, const QString& text, int version)
{
    QJsonObject textDocumentId;
    textDocumentId["uri"] = buildDocumentUri(uri);
    textDocumentId["version"] = version;
    
    QJsonArray contentChanges;
    QJsonObject change;
    change["text"] = text;  // Full document sync
    contentChanges.append(change);
    
    QJsonObject params;
    params["textDocument"] = textDocumentId;
    params["contentChanges"] = contentChanges;
    
    QJsonObject notification;
    notification["jsonrpc"] = "2.0";
    notification["method"] = "textDocument/didChange";
    notification["params"] = params;
    
    sendMessage(notification);
    m_documentVersions[uri] = version;
}

void LSPClient::getCodeActions(const QString& uri, int line, int character)
{
    if (!m_serverRunning) return;
    
    int requestId = m_nextRequestId++;
    
    QJsonObject textDocumentId;
    textDocumentId["uri"] = buildDocumentUri(uri);
    
    QJsonObject range;
    QJsonObject pos;
    pos["line"] = line;
    pos["character"] = character;
    range["start"] = pos;
    range["end"] = pos;
    
    QJsonObject context;
    context["diagnostics"] = QJsonArray(); // Could include diagnostics here
    
    QJsonObject params;
    params["textDocument"] = textDocumentId;
    params["range"] = range;
    params["context"] = context;
    
    QJsonObject request;
    request["jsonrpc"] = "2.0";
    request["id"] = requestId;
    request["method"] = "textDocument/codeAction";
    request["params"] = params;
    
    sendMessage(request);
    
    PendingRequest pending;
    pending.type = "codeAction";
    pending.uri = uri;
    pending.line = line;
    pending.character = character;
    m_pendingRequests[requestId] = pending;
}

void LSPClient::executeCodeAction(const QJsonObject& action)
{
    if (!m_serverRunning) return;
    
    // Some code actions are commands, some are workspace edits
    if (action.contains("command")) {
        QJsonObject command = action["command"].toObject();
        
        int requestId = m_nextRequestId++;
        QJsonObject request;
        request["jsonrpc"] = "2.0";
        request["id"] = requestId;
        request["method"] = "workspace/executeCommand";
        request["params"] = command;
        
        sendMessage(request);
    }
}

void LSPClient::requestHover(const QString& uri, int line, int character)
{
    if (!m_initialized) return;
    
    int requestId = m_nextRequestId++;
    
    QJsonObject textDocumentId;
    textDocumentId["uri"] = buildDocumentUri(uri);
    
    QJsonObject position;
    position["line"] = line;
    position["character"] = character;
    
    QJsonObject params;
    params["textDocument"] = textDocumentId;
    params["position"] = position;
    
    QJsonObject request;
    request["jsonrpc"] = "2.0";
    request["id"] = requestId;
    request["method"] = "textDocument/hover";
    request["params"] = params;
    
    sendMessage(request);
    
    PendingRequest pending;
    pending.type = "hover";
    pending.uri = uri;
    pending.line = line;
    pending.character = character;
    m_pendingRequests[requestId] = pending;
}

void LSPClient::requestDefinition(const QString& uri, int line, int character)
{
    if (!m_initialized) return;
    
    int requestId = m_nextRequestId++;
    
    QJsonObject textDocumentId;
    textDocumentId["uri"] = buildDocumentUri(uri);
    
    QJsonObject position;
    position["line"] = line;
    position["character"] = character;
    
    QJsonObject params;
    params["textDocument"] = textDocumentId;
    params["position"] = position;
    
    QJsonObject request;
    request["jsonrpc"] = "2.0";
    request["id"] = requestId;
    request["method"] = "textDocument/definition";
    request["params"] = params;
    
    sendMessage(request);
    
    PendingRequest pending;
    pending.type = "definition";
    pending.uri = uri;
    pending.line = line;
    pending.character = character;
    m_pendingRequests[requestId] = pending;
}

void LSPClient::requestReferences(const QString& uri, int line, int character)
{
    if (!m_initialized) return;
    
    int requestId = m_nextRequestId++;
    
    QJsonObject textDocumentId;
    textDocumentId["uri"] = buildDocumentUri(uri);
    
    QJsonObject position;
    position["line"] = line;
    position["character"] = character;
    
    QJsonObject context;
    context["includeDeclaration"] = true;
    
    QJsonObject params;
    params["textDocument"] = textDocumentId;
    params["position"] = position;
    params["context"] = context;
    
    QJsonObject request;
    request["jsonrpc"] = "2.0";
    request["id"] = requestId;
    request["method"] = "textDocument/references";
    request["params"] = params;
    
    sendMessage(request);
    
    PendingRequest pending;
    pending.type = "references";
    pending.uri = uri;
    pending.line = line;
    pending.character = character;
    m_pendingRequests[requestId] = pending;
}

void LSPClient::requestRename(const QString& uri, int line, int character, const QString& newName)
{
    if (!m_initialized) {
        qWarning() << "[LSPClient] Cannot request rename - server not initialized";
        return;
    }
    
    int requestId = m_nextRequestId++;
    
    QJsonObject textDocumentId;
    textDocumentId["uri"] = buildDocumentUri(uri);
    
    QJsonObject position;
    position["line"] = line;
    position["character"] = character;
    
    QJsonObject params;
    params["textDocument"] = textDocumentId;
    params["position"] = position;
    params["newName"] = newName;
    
    QJsonObject request;
    request["jsonrpc"] = "2.0";
    request["id"] = requestId;
    request["method"] = "textDocument/rename";
    request["params"] = params;
    
    PendingRequest pending;
    pending.type = "rename";
    pending.uri = uri;
    pending.line = line;
    pending.character = character;
    pending.metadata = newName;
    m_pendingRequests[requestId] = pending;
    
    sendMessage(request);
    qDebug() << "[LSPClient] Sent rename request to" << newName;
}

void LSPClient::formatDocument(const QString& uri)
{
    if (!m_initialized) return;
    
    int requestId = m_nextRequestId++;
    
    QJsonObject textDocumentId;
    textDocumentId["uri"] = buildDocumentUri(uri);
    
    QJsonObject options;
    options["tabSize"] = 4;
    options["insertSpaces"] = true;
    
    QJsonObject params;
    params["textDocument"] = textDocumentId;
    params["options"] = options;
    
    QJsonObject request;
    request["jsonrpc"] = "2.0";
    request["id"] = requestId;
    request["method"] = "textDocument/formatting";
    request["params"] = params;
    
    sendMessage(request);
}

QVector<Diagnostic> LSPClient::getDiagnostics(const QString& uri) const
{
    return m_diagnostics.value(uri);
}

void LSPClient::onServerReadyRead()
{
    m_receiveBuffer += m_serverProcess->readAllStandardOutput();
    
    // Process all complete messages in buffer
    while (true) {
        // LSP uses Content-Length header
        int headerEnd = m_receiveBuffer.indexOf("\r\n\r\n");
        if (headerEnd == -1) break;
        
        QString header = QString::fromUtf8(m_receiveBuffer.left(headerEnd));
        int contentLength = 0;
        
        for (const QString& line : header.split("\r\n")) {
            if (line.startsWith("Content-Length:")) {
                contentLength = line.mid(15).trimmed().toInt();
                break;
            }
        }
        
        if (contentLength == 0) {
            m_receiveBuffer.remove(0, headerEnd + 4);
            continue;
        }
        
        int messageStart = headerEnd + 4;
        if (m_receiveBuffer.size() < messageStart + contentLength) {
            break;  // Incomplete message
        }
        
        QByteArray messageData = m_receiveBuffer.mid(messageStart, contentLength);
        m_receiveBuffer.remove(0, messageStart + contentLength);
        
        QJsonDocument doc = QJsonDocument::fromJson(messageData);
        if (doc.isObject()) {
            processMessage(doc.object());
        }
    }
}

void LSPClient::onServerError(QProcess::ProcessError error)
{
    QString errorStr;
    switch (error) {
        case QProcess::FailedToStart:
            errorStr = QString("Failed to start %1 server: %2")
                .arg(m_config.language)
                .arg(m_serverProcess->errorString());
            break;
        case QProcess::Crashed:
            errorStr = QString("%1 server crashed").arg(m_config.language);
            break;
        case QProcess::Timedout:
            errorStr = QString("%1 server communication timeout").arg(m_config.language);
            break;
        case QProcess::WriteError:
            errorStr = "LSP server write error";
            break;
        case QProcess::ReadError:
            errorStr = "LSP server read error";
            break;
        default:
            errorStr = QString("LSP server error: %1").arg(m_serverProcess->errorString());
    }
    
    qCritical() << "[LSPClient]" << errorStr;
    
    // Log stderr if available
    QString stdErrLog = QString::fromUtf8(m_serverProcess->readAllStandardError());
    if (!stdErrLog.isEmpty()) {
        qCritical() << "[LSPClient] Server stderr:" << stdErrLog;
    }
    
    emit serverError(errorStr);
    m_serverRunning = false;
    m_initialized = false;
}

void LSPClient::onServerFinished(int exitCode, QProcess::ExitStatus status)
{
    qInfo() << "[LSPClient] Server finished with exit code:" << exitCode;
    m_serverRunning = false;
    m_initialized = false;
}

void LSPClient::sendMessage(const QJsonObject& message)
{
    if (!m_serverProcess || !m_serverRunning) {
        qWarning() << "[LSPClient] Cannot send message - server not running";
        return;
    }
    
    try {
        QJsonDocument doc(message);
        QByteArray json = doc.toJson(QJsonDocument::Compact);
        
        if (json.isEmpty()) {
            qWarning() << "[LSPClient] Failed to serialize JSON message";
            return;
        }
        
        QString header = QString("Content-Length: %1\r\n\r\n").arg(json.size());
        QByteArray headerBytes = header.toUtf8();
        
        // Write header
        qint64 headerWritten = m_serverProcess->write(headerBytes);
        if (headerWritten != headerBytes.size()) {
            qWarning() << "[LSPClient] Failed to write complete header";
            return;
        }
        
        // Write message body
        qint64 bodyWritten = m_serverProcess->write(json);
        if (bodyWritten != json.size()) {
            qWarning() << "[LSPClient] Failed to write complete message body";
            return;
        }
        
        if (!m_serverProcess->waitForBytesWritten(1000)) {
            qWarning() << "[LSPClient] Timeout waiting for bytes to be written";
            return;
        }
        
        qDebug() << "[LSPClient] Sent message:" << message["method"].toString() 
                 << "(size:" << (headerBytes.size() + json.size()) << "bytes)";
    } catch (const std::exception& e) {
        qCritical() << "[LSPClient] Exception sending message:" << e.what();
        emit serverError(QString("Failed to send message: %1").arg(e.what()));
    }
}

void LSPClient::processMessage(const QJsonObject& message)
{
    // Check if it's a response or notification
    if (message.contains("id")) {
        // Response to our request
        int id = message["id"].toInt();
        
        if (message.contains("result")) {
            QJsonValue result = message["result"];
            
            if (m_pendingRequests.contains(id)) {
                PendingRequest req = m_pendingRequests.take(id);
                
                // Check if this is a generic request with callback
                if (req.type == "generic" && req.callback) {
                    // Execute callback with result
                    QJsonObject response;
                    response["result"] = result;
                    req.callback(response);
                } else if (req.type == "completion") {
                    handleCompletionResponse(result, id);
                } else if (req.type == "hover") {
                    handleHoverResponse(result, id);
                } else if (req.type == "signatureHelp") {
                    handleSignatureHelpResponse(result, id);
                } else if (req.type == "definition") {
                    handleDefinitionResponse(result, id);
                } else if (req.type == "references") {
                    handleReferencesResponse(result, id);
                } else if (req.type == "rename") {
                    handleRenameResponse(result, id);
                } else if (req.type == "codeAction") {
                    handleCodeActionResponse(result, id);
                } else if (req.type == "organizeImports") {
                    handleCodeActionResponse(result, id);
                }
            } else if (id == 1) {
                // Initialize response
                handleInitializeResponse(result);
            }
        } else if (message.contains("error")) {
            QJsonObject error = message["error"].toObject();
            qWarning() << "[LSPClient] Error response:" << error["message"].toString();
            
            // Check if this was a generic request with callback
            if (m_pendingRequests.contains(id)) {
                PendingRequest req = m_pendingRequests.take(id);
                if (req.type == "generic" && req.callback) {
                    // Execute callback with error
                    QJsonObject response;
                    response["error"] = error;
                    req.callback(response);
                }
            }
        }
    } else if (message.contains("method")) {
        // Server notification
        QString method = message["method"].toString();
        QJsonObject params = message["params"].toObject();
        
        if (method == "textDocument/publishDiagnostics") {
            handleDiagnostics(params);
        }
    }
}

void LSPClient::handleInitializeResponse(const QJsonValue& result)
{
    qInfo() << "[LSPClient] Server initialized successfully";
    m_initialized = true;
    
    // Send initialized notification
    QJsonObject notification;
    notification["jsonrpc"] = "2.0";
    notification["method"] = "initialized";
    notification["params"] = QJsonObject();
    sendMessage(notification);
    
    emit serverReady();
}

void LSPClient::handleCompletionResponse(const QJsonValue& result, int requestId)
{
    if (!m_pendingRequests.contains(requestId)) return;
    
    PendingRequest req = m_pendingRequests[requestId];
    QVector<CompletionItem> items;
    
    // Result can be CompletionList or CompletionItem[]
    QJsonArray itemsArray;
    if (result.isObject()) {
        QJsonObject obj = result.toObject();
        if (obj.contains("items")) {
            QJsonValue itemsValue = obj.value("items");
            if (itemsValue.isArray()) {
                itemsArray = itemsValue.toArray();
            }
        }
    } else if (result.isArray()) {
        itemsArray = result.toArray();
    }
    
    QSet<QString> seen;  // Deduplication
    
    for (const QJsonValue& val : itemsArray) {
        QJsonObject itemObj = val.toObject();
        
        CompletionItem item;
        item.label = itemObj["label"].toString();
        
        // Skip duplicates
        if (seen.contains(item.label)) continue;
        seen.insert(item.label);
        
        item.insertText = itemObj.contains("insertText") 
            ? itemObj["insertText"].toString() 
            : item.label;
        item.detail = itemObj["detail"].toString();
        item.kind = itemObj["kind"].toInt(1);
        item.sortText = itemObj["sortText"].toString();
        item.filterText = itemObj["filterText"].toString();
        
        if (itemObj.contains("documentation")) {
            QJsonValue docVal = itemObj["documentation"];
            if (docVal.isString()) {
                item.documentation = docVal.toString();
            } else if (docVal.isObject()) {
                item.documentation = docVal.toObject()["value"].toString();
            }
        }
        
        // Compute relevance score
        item.score = computeCompletionScore(item, item.filterText);
        
        items.append(item);
    }
    
    // Sort by score (higher scores first)
    std::sort(items.begin(), items.end(), 
        [](const CompletionItem& a, const CompletionItem& b) {
            return a.score > b.score;
        });
    
    // Cache results
    QString cacheKey = QString("%1:%2:%3").arg(req.uri).arg(req.line).arg(req.character);
    m_completionCache[cacheKey] = items;
    
    qDebug() << "[LSPClient] Received" << items.size() << "completions (deduplicated and scored)";
    emit completionsReceived(req.uri, req.line, req.character, items);
}

void LSPClient::handleHoverResponse(const QJsonValue& result, int requestId)
{
    if (!m_pendingRequests.contains(requestId)) return;
    
    PendingRequest req = m_pendingRequests[requestId];
    QString markdown;
    
    if (result.isObject()) {
        QJsonObject obj = result.toObject();
        if (obj.contains("contents")) {
            QJsonValue contents = obj["contents"];
        if (contents.isString()) {
            markdown = contents.toString();
        } else if (contents.isObject()) {
            markdown = contents.toObject()["value"].toString();
        } else if (contents.isArray()) {
            QJsonArray arr = contents.toArray();
            for (const QJsonValue& val : arr) {
                if (val.isString()) {
                    markdown += val.toString() + "\n";
                } else if (val.isObject()) {
                    markdown += val.toObject()["value"].toString() + "\n";
                }
            }
        }
    }
    }
    
    // Truncate very large hover content
    if (markdown.length() > 5000) {
        markdown = markdown.left(5000) + "\n... (truncated)";
    }
    
    qDebug() << "[LSPClient] Hover info received:" << markdown.length() << "chars";
    emit hoverReceived(req.uri, markdown);
}

void LSPClient::handleSignatureHelpResponse(const QJsonValue& result, int requestId)
{
    if (!m_pendingRequests.contains(requestId)) return;
    
    PendingRequest req = m_pendingRequests[requestId];
    SignatureHelp help;
    
    if (result.isObject()) {
        QJsonObject obj = result.toObject();
        if (obj.contains("signatures")) {
            QJsonArray signatures = obj["signatures"].toArray();
        for (const QJsonValue& sig : signatures) {
            QJsonObject sigObj = sig.toObject();
            QString label = sigObj["label"].toString();
            help.signatures.append(label);
            
            // Extract parameters
            if (sigObj.contains("parameters")) {
                QJsonArray params = sigObj["parameters"].toArray();
                for (const QJsonValue& param : params) {
                    QJsonObject paramObj = param.toObject();
                    ParameterInfo pinfo;
                    pinfo.label = paramObj["label"].toString();
                    
                    if (paramObj.contains("documentation")) {
                        pinfo.documentation = paramObj["documentation"].toString();
                    }
                    
                    help.parameters.append(pinfo);
                }
            }
        }
        
        help.activeSignature = obj["activeSignature"].toInt(0);
        help.activeParameter = obj["activeParameter"].toInt(0);
    }
    }
    
    qDebug() << "[LSPClient] Signature help received:" << help.signatures.size() << "signatures";
    emit signatureHelpReceived(req.uri, help);
}

void LSPClient::handleDefinitionResponse(const QJsonValue& result, int requestId)
{
    if (!m_pendingRequests.contains(requestId)) return;
    
    PendingRequest req = m_pendingRequests[requestId];
    
    // Result can be Location or Location[]
    QJsonObject location;
    if (result.isObject()) {
        QJsonObject obj = result.toObject();
        if (obj.contains("uri")) {
            location = obj;
        }
    }
    
    if (location.isEmpty() && result.isArray()) {
        QJsonArray arr = result.toArray();
        if (!arr.isEmpty()) {
            location = arr.first().toObject();
        }
    }
    
    if (location.contains("uri")) {
        QString uri = location["uri"].toString();
        QJsonObject range = location["range"].toObject();
        QJsonObject start = range["start"].toObject();
        
        int line = start["line"].toInt();
        int character = start["character"].toInt();
        
        emit definitionReceived(uri, line, character);
    }
}

void LSPClient::handleReferencesResponse(const QJsonValue& result, int requestId)
{
    // Result is Location[]
    QVector<Diagnostic> refDiags;
    if (result.isArray()) {
        QJsonArray locations = result.toArray();
        for (const QJsonValue& val : locations) {
            QJsonObject loc = val.toObject();
            QJsonObject range = loc["range"].toObject();
            QJsonObject start = range["start"].toObject();
            
            Diagnostic diag;
            diag.line = start["line"].toInt();
            diag.column = start["character"].toInt();
            diag.message = loc["uri"].toString();
            refDiags.append(diag);
        }
    }
    
    emit referencesReceived(refDiags);
}

void LSPClient::handleRenameResponse(const QJsonValue& result, int requestId)
{
    // Result is WorkspaceEdit
    // Structure: { changes: { uri: [{ range, newText }, ...], ... } }
    
    if (result.isObject()) {
        QJsonObject obj = result.toObject();
        if (obj.contains("changes")) {
            QJsonObject changes = obj["changes"].toObject();
        
        qDebug() << "[LSPClient] Rename affects" << changes.size() << "files";
        
        // Validate all edits before applying
        for (auto it = changes.begin(); it != changes.end(); ++it) {
            QString fileUri = it.key();
            QJsonArray edits = it.value().toArray();
            
            qDebug() << "[LSPClient] File" << fileUri << ":" << edits.size() << "edits";
            
            // Check for conflicts (overlapping edits on same line)
            QMap<int, int> lineRanges;
            for (const QJsonValue& edit : edits) {
                QJsonObject editObj = edit.toObject();
                QJsonObject range = editObj["range"].toObject();
                QJsonObject start = range["start"].toObject();
                int line = start["line"].toInt();
                
                if (lineRanges.contains(line)) {
                    qWarning() << "[LSPClient] Potential rename conflict on line" << line;
                }
                lineRanges[line]++;
            }
        }
    }
    }
    
    qDebug() << "[LSPClient] Rename workspace edit received";
    emit renameReceived(result.toObject());
}

void LSPClient::handleCodeActionResponse(const QJsonValue& result, int requestId)
{
    // Result is (Command | CodeAction)[]
    QVector<QJsonObject> actions;
    if (result.isArray()) {
        QJsonArray actionsArray = result.toArray();
        for (const QJsonValue& val : actionsArray) {
            actions.append(val.toObject());
        }
    }
    
    emit codeActionsReceived(actions);
}

void LSPClient::handleDiagnostics(const QJsonObject& params)
{
    QString uri = params["uri"].toString();
    QJsonArray diagnosticsArray = params["diagnostics"].toArray();
    
    QVector<Diagnostic> diagnostics;
    QMap<QString, int> lineErrors;  // Track error count per line for optimization
    
    for (const QJsonValue& val : diagnosticsArray) {
        QJsonObject diagObj = val.toObject();
        
        Diagnostic diag;
        QJsonObject range = diagObj["range"].toObject();
        QJsonObject start = range["start"].toObject();
        
        diag.line = start["line"].toInt();
        diag.column = start["character"].toInt();
        diag.severity = diagObj["severity"].toInt();
        diag.message = diagObj["message"].toString();
        diag.source = diagObj.contains("source") ? diagObj["source"].toString() : "LSP";
        diag.code = diagObj.contains("code") ? diagObj["code"].toString() : "";
        
        // Truncate long messages for performance
        if (diag.message.length() > 500) {
            diag.message = diag.message.left(497) + "...";
        }
        
        diagnostics.append(diag);
        lineErrors[QString::number(diag.line)]++;
    }
    
    // Filter out duplicate errors on same line (keep most severe)
    QMap<int, Diagnostic> filteredDiags;
    for (const Diagnostic& diag : diagnostics) {
        int key = diag.line * 10000 + diag.column;
        if (!filteredDiags.contains(key) || diag.severity < filteredDiags[key].severity) {
            filteredDiags[key] = diag;
        }
    }
    
    diagnostics.clear();
    for (const Diagnostic& diag : filteredDiags.values()) {
        diagnostics.append(diag);
    }
    
    m_diagnostics[uri] = diagnostics;
    
    qDebug() << "[LSPClient] Diagnostics updated for" << uri 
             << ":" << diagnostics.size() << "issues";
    
    emit diagnosticsUpdated(uri, diagnostics);
}

QString LSPClient::buildDocumentUri(const QString& filePath) const
{
    QFileInfo info(filePath);
    QString absolutePath = info.isRelative() 
        ? QFileInfo(m_config.workspaceRoot + "/" + filePath).absoluteFilePath()
        : info.absoluteFilePath();
    
    return QUrl::fromLocalFile(absolutePath).toString();
}

int LSPClient::computeCompletionScore(const CompletionItem& item, const QString& filter) const
{
    int score = 100;  // Base score
    
    // Prioritize by kind
    if (item.kind == 3) score += 50;  // Function
    if (item.kind == 2) score += 40;  // Method
    if (item.kind == 13) score += 30; // Variable
    
    // Boost exact prefix matches
    if (item.label.startsWith(filter, Qt::CaseInsensitive)) {
        score += 100;
    }
    
    // Boost exact matches
    if (item.label == filter) {
        score += 200;
    }
    
    // Penalize lower case labels (prefer camelCase/PascalCase)
    if (!item.label.isEmpty() && item.label[0].isLower()) {
        score -= 10;
    }
    
    return score;
}

void LSPClient::requestCompletions(const QString& uri, int line, int character)
{
    if (!m_serverRunning || !m_initialized) {
        qWarning() << "[LSPClient] Cannot request completions - server not ready";
        return;
    }
    
    // Check cache first
    QString cacheKey = QString("%1:%2:%3").arg(uri).arg(line).arg(character);
    if (m_completionCache.contains(cacheKey)) {
        emit completionsReceived(uri, line, character, m_completionCache[cacheKey]);
        return;
    }
    
    int requestId = m_nextRequestId++;
    
    QJsonObject textDocumentId;
    textDocumentId["uri"] = buildDocumentUri(uri);
    
    QJsonObject position;
    position["line"] = line;
    position["character"] = character;
    
    // Add completion context
    QJsonObject context;
    context["triggerKind"] = 1;  // Invoked
    
    QJsonObject params;
    params["textDocument"] = textDocumentId;
    params["position"] = position;
    params["context"] = context;
    
    QJsonObject request;
    request["jsonrpc"] = "2.0";
    request["id"] = requestId;
    request["method"] = "textDocument/completion";
    request["params"] = params;
    
    PendingRequest pending;
    pending.type = "completion";
    pending.uri = uri;
    pending.line = line;
    pending.character = character;
    m_pendingRequests[requestId] = pending;
    
    sendMessage(request);
    qDebug() << "[LSPClient] Sent completion request for" << uri << "at" << line << ":" << character;
}

void LSPClient::requestSignatureHelp(const QString& uri, int line, int character)
{
    if (!m_serverRunning || !m_initialized) {
        qWarning() << "[LSPClient] Cannot request signature help - server not ready";
        return;
    }
    
    int requestId = m_nextRequestId++;
    
    QJsonObject textDocumentId;
    textDocumentId["uri"] = buildDocumentUri(uri);
    
    QJsonObject position;
    position["line"] = line;
    position["character"] = character;
    
    QJsonObject params;
    params["textDocument"] = textDocumentId;
    params["position"] = position;
    
    QJsonObject request;
    request["jsonrpc"] = "2.0";
    request["id"] = requestId;
    request["method"] = "textDocument/signatureHelp";
    request["params"] = params;
    
    PendingRequest pending;
    pending.type = "signatureHelp";
    pending.uri = uri;
    pending.line = line;
    pending.character = character;
    m_pendingRequests[requestId] = pending;
    
    sendMessage(request);
    qDebug() << "[LSPClient] Sent signature help request for" << uri;
}

void LSPClient::requestExtractMethod(const QString& uri, int startLine, int endLine, const QString& methodName)
{
    if (!m_serverRunning || !m_initialized) return;
    
    int requestId = m_nextRequestId++;
    
    QJsonObject textDocumentId;
    textDocumentId["uri"] = buildDocumentUri(uri);
    
    QJsonObject range;
    QJsonObject startPos, endPos;
    startPos["line"] = startLine;
    startPos["character"] = 0;
    endPos["line"] = endLine;
    endPos["character"] = 0;
    range["start"] = startPos;
    range["end"] = endPos;
    
    QJsonObject params;
    params["textDocument"] = textDocumentId;
    params["range"] = range;
    
    QJsonObject request;
    request["jsonrpc"] = "2.0";
    request["id"] = requestId;
    request["method"] = "codeAction";
    request["params"] = params;
    
    PendingRequest pending;
    pending.type = "extractMethod";
    pending.uri = uri;
    pending.line = startLine;
    pending.character = 0;
    pending.metadata = methodName;
    m_pendingRequests[requestId] = pending;
    
    sendMessage(request);
    qDebug() << "[LSPClient] Sent extract method request";
}

void LSPClient::requestExtractVariable(const QString& uri, int line, int startChar, int endChar, const QString& varName)
{
    if (!m_serverRunning || !m_initialized) return;
    
    int requestId = m_nextRequestId++;
    
    QJsonObject textDocumentId;
    textDocumentId["uri"] = buildDocumentUri(uri);
    
    QJsonObject range;
    QJsonObject startPos, endPos;
    startPos["line"] = line;
    startPos["character"] = startChar;
    endPos["line"] = line;
    endPos["character"] = endChar;
    range["start"] = startPos;
    range["end"] = endPos;
    
    QJsonObject params;
    params["textDocument"] = textDocumentId;
    params["range"] = range;
    
    QJsonObject request;
    request["jsonrpc"] = "2.0";
    request["id"] = requestId;
    request["method"] = "codeAction";
    request["params"] = params;
    
    PendingRequest pending;
    pending.type = "extractVariable";
    pending.uri = uri;
    pending.line = line;
    pending.character = startChar;
    pending.metadata = varName;
    m_pendingRequests[requestId] = pending;
    
    sendMessage(request);
    qDebug() << "[LSPClient] Sent extract variable request";
}

void LSPClient::requestOrganizeImports(const QString& uri)
{
    if (!m_serverRunning || !m_initialized) return;
    
    int requestId = m_nextRequestId++;
    
    QJsonObject textDocumentId;
    textDocumentId["uri"] = buildDocumentUri(uri);
    
    QJsonObject params;
    params["textDocument"] = textDocumentId;
    
    QJsonObject request;
    request["jsonrpc"] = "2.0";
    request["id"] = requestId;
    request["method"] = "codeAction";
    request["params"] = params;
    
    // Add filter for organize imports command
    QJsonObject command;
    command["title"] = "Organize Imports";
    command["command"] = "editor.action.organizeImport";
    
    PendingRequest pending;
    pending.type = "organizeImports";
    pending.uri = uri;
    pending.line = 0;
    pending.character = 0;
    m_pendingRequests[requestId] = pending;
    
    sendMessage(request);
    qDebug() << "[LSPClient] Sent organize imports request";
}

} // namespace RawrXD
