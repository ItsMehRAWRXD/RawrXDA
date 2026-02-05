/**
 * \file lsp_client.cpp
 * \brief LSP client implementation with JSON-RPC communication
 * \author RawrXD Team
 * \date 2025-12-07
 */

#include "lsp_client.h"
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
    
    m_serverProcess->setProgram(m_config.command);
    m_serverProcess->setArguments(m_config.arguments);
    m_serverProcess->setWorkingDirectory(m_config.workspaceRoot);
    m_serverProcess->start();
    
    if (!m_serverProcess->waitForStarted(5000)) {
        qCritical() << "[LSPClient] Failed to start server";
        emit serverError("Failed to start LSP server");
        return false;
    }
    
    m_serverRunning = true;
    
    // Send initialize request
    QJsonObject initializeParams;
    initializeParams["processId"] = static_cast<qint64>(QCoreApplication::applicationPid());
    initializeParams["rootUri"] = buildDocumentUri(m_config.workspaceRoot);
    
    QJsonObject capabilities;
    QJsonObject textDocument;
    
    // Completion support
    QJsonObject completion;
    completion["dynamicRegistration"] = false;
    QJsonObject completionItem;
    completionItem["snippetSupport"] = false;
    completion["completionItem"] = completionItem;
    textDocument["completion"] = completion;
    
    // Hover support
    textDocument["hover"] = QJsonObject{{"dynamicRegistration", false}};
    
    // Definition support
    textDocument["definition"] = QJsonObject{{"dynamicRegistration", false}};
    
    // Diagnostics support
    textDocument["publishDiagnostics"] = QJsonObject{{"relatedInformation", true}};
    
    // Formatting support
    textDocument["formatting"] = QJsonObject{{"dynamicRegistration", false}};
    
    capabilities["textDocument"] = textDocument;
    initializeParams["capabilities"] = capabilities;
    
    QJsonObject initRequest;
    initRequest["jsonrpc"] = "2.0";
    initRequest["id"] = m_nextRequestId++;
    initRequest["method"] = "initialize";
    initRequest["params"] = initializeParams;
    
    sendMessage(initRequest);
    
    qInfo() << "[LSPClient] Initialize request sent";
    return true;
}

void LSPClient::stopServer()
{
    if (!m_serverRunning) return;
    
    // Send shutdown request
    QJsonObject shutdownRequest;
    shutdownRequest["jsonrpc"] = "2.0";
    shutdownRequest["id"] = m_nextRequestId++;
    shutdownRequest["method"] = "shutdown";
    sendMessage(shutdownRequest);
    
    // Send exit notification
    QJsonObject exitNotification;
    exitNotification["jsonrpc"] = "2.0";
    exitNotification["method"] = "exit";
    sendMessage(exitNotification);
    
    m_serverProcess->waitForFinished(2000);
    m_serverProcess->kill();
    
    m_serverRunning = false;
    m_initialized = false;
    
    qInfo() << "[LSPClient] Server stopped";
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

void LSPClient::requestCompletions(const QString& uri, int line, int character)
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
    request["method"] = "textDocument/completion";
    request["params"] = params;
    
    sendMessage(request);
    
    PendingRequest pending;
    pending.type = "completion";
    pending.uri = uri;
    pending.line = line;
    pending.character = character;
    m_pendingRequests[requestId] = pending;
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
            errorStr = "Failed to start LSP server";
            break;
        case QProcess::Crashed:
            errorStr = "LSP server crashed";
            break;
        default:
            errorStr = "LSP server error";
    }
    
    qCritical() << "[LSPClient]" << errorStr;
    emit serverError(errorStr);
    m_serverRunning = false;
}

void LSPClient::onServerFinished(int exitCode, QProcess::ExitStatus status)
{
    qInfo() << "[LSPClient] Server finished with exit code:" << exitCode;
    m_serverRunning = false;
    m_initialized = false;
}

void LSPClient::sendMessage(const QJsonObject& message)
{
    QJsonDocument doc(message);
    QByteArray json = doc.toJson(QJsonDocument::Compact);
    
    QString header = QString("Content-Length: %1\r\n\r\n").arg(json.size());
    
    m_serverProcess->write(header.toUtf8());
    m_serverProcess->write(json);
    m_serverProcess->waitForBytesWritten();
}

void LSPClient::processMessage(const QJsonObject& message)
{
    // Check if it's a response or notification
    if (message.contains("id")) {
        // Response to our request
        int id = message["id"].toInt();
        
        if (message.contains("result")) {
            QJsonObject result = message["result"].toObject();
            
            if (m_pendingRequests.contains(id)) {
                PendingRequest req = m_pendingRequests.take(id);
                
                if (req.type == "completion") {
                    handleCompletionResponse(result, id);
                } else if (req.type == "hover") {
                    handleHoverResponse(result, id);
                } else if (req.type == "definition") {
                    handleDefinitionResponse(result, id);
                }
            } else if (id == 1) {
                // Initialize response
                handleInitializeResponse(result);
            }
        } else if (message.contains("error")) {
            QJsonObject error = message["error"].toObject();
            qWarning() << "[LSPClient] Error response:" << error["message"].toString();
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

void LSPClient::handleInitializeResponse(const QJsonObject& result)
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

void LSPClient::handleCompletionResponse(const QJsonObject& result, int requestId)
{
    if (!m_pendingRequests.contains(requestId)) return;
    
    PendingRequest req = m_pendingRequests[requestId];
    QVector<CompletionItem> items;
    
    // Result can be CompletionList or CompletionItem[]
    QJsonArray itemsArray;
    if (result.contains("items")) {
        QJsonValue itemsValue = result.value("items");
        if (itemsValue.isArray()) {
            itemsArray = itemsValue.toArray();
        }
    } else {
        // Result itself might be an array (not wrapped in CompletionList)
        // In this case, convert the entire object to a document and check
        QJsonDocument doc(result);
        if (doc.isArray()) {
            itemsArray = doc.array();
        }
    }
    
    for (const QJsonValue& val : itemsArray) {
        QJsonObject itemObj = val.toObject();
        
        CompletionItem item;
        item.label = itemObj["label"].toString();
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
        
        items.append(item);
    }
    
    qDebug() << "[LSPClient] Received" << items.size() << "completions";
    emit completionsReceived(req.uri, req.line, req.character, items);
}

void LSPClient::handleHoverResponse(const QJsonObject& result, int requestId)
{
    if (!m_pendingRequests.contains(requestId)) return;
    
    PendingRequest req = m_pendingRequests[requestId];
    QString markdown;
    
    if (result.contains("contents")) {
        QJsonValue contents = result["contents"];
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
    
    emit hoverReceived(req.uri, markdown);
}

void LSPClient::handleDefinitionResponse(const QJsonObject& result, int requestId)
{
    if (!m_pendingRequests.contains(requestId)) return;
    
    PendingRequest req = m_pendingRequests[requestId];
    
    // Result can be Location or Location[]
    QJsonObject location;
    if (result.contains("uri")) {
        location = result;
    } else {
        // Result might be an array of locations
        QJsonDocument doc(result);
        if (doc.isArray()) {
            QJsonArray arr = doc.array();
            if (!arr.isEmpty()) {
                location = arr.first().toObject();
            }
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

void LSPClient::handleDiagnostics(const QJsonObject& params)
{
    QString uri = params["uri"].toString();
    QJsonArray diagnosticsArray = params["diagnostics"].toArray();
    
    QVector<Diagnostic> diagnostics;
    for (const QJsonValue& val : diagnosticsArray) {
        QJsonObject diagObj = val.toObject();
        
        Diagnostic diag;
        QJsonObject range = diagObj["range"].toObject();
        QJsonObject start = range["start"].toObject();
        
        diag.line = start["line"].toInt();
        diag.column = start["character"].toInt();
        diag.severity = diagObj["severity"].toInt();
        diag.message = diagObj["message"].toString();
        diag.source = diagObj["source"].toString();
        
        diagnostics.append(diag);
    }
    
    m_diagnostics[uri] = diagnostics;
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

} // namespace RawrXD
