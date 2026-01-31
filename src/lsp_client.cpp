/**
 * \file lsp_client.cpp
 * \brief LSP client implementation with JSON-RPC communication
 * \author RawrXD Team
 * \date 2025-12-07
 */

#include "lsp_client.h"


namespace RawrXD {

LSPClient::LSPClient(const LSPServerConfig& config, void* parent)
    : void(parent)
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
    
    m_serverProcess = new void*(this);
// Qt connect removed
// Qt connect removed
// Qt connect removed
    if (m_config.autoStart) {
        startServer();
    }
}

bool LSPClient::startServer()
{
    if (m_serverRunning) {
        return true;
    }


    m_serverProcess->setProgram(m_config.command);
    m_serverProcess->setArguments(m_config.arguments);
    m_serverProcess->setWorkingDirectory(m_config.workspaceRoot);
    m_serverProcess->start();
    
    if (!m_serverProcess->waitForStarted(5000)) {
        serverError("Failed to start LSP server");
        return false;
    }
    
    m_serverRunning = true;
    
    // Send initialize request
    void* initializeParams;
    initializeParams["processId"] = static_cast<int64_t>(QCoreApplication::applicationPid());
    initializeParams["rootUri"] = buildDocumentUri(m_config.workspaceRoot);
    
    void* capabilities;
    void* textDocument;
    
    // Completion support
    void* completion;
    completion["dynamicRegistration"] = false;
    void* completionItem;
    completionItem["snippetSupport"] = false;
    completion["completionItem"] = completionItem;
    textDocument["completion"] = completion;
    
    // Hover support
    textDocument["hover"] = void*{{"dynamicRegistration", false}};
    
    // Definition support
    textDocument["definition"] = void*{{"dynamicRegistration", false}};
    
    // Diagnostics support
    textDocument["publishDiagnostics"] = void*{{"relatedInformation", true}};
    
    // Formatting support
    textDocument["formatting"] = void*{{"dynamicRegistration", false}};
    
    capabilities["textDocument"] = textDocument;
    initializeParams["capabilities"] = capabilities;
    
    void* initRequest;
    initRequest["jsonrpc"] = "2.0";
    initRequest["id"] = m_nextRequestId++;
    initRequest["method"] = "initialize";
    initRequest["params"] = initializeParams;
    
    sendMessage(initRequest);
    
    return true;
}

void LSPClient::stopServer()
{
    if (!m_serverRunning) return;
    
    // Send shutdown request
    void* shutdownRequest;
    shutdownRequest["jsonrpc"] = "2.0";
    shutdownRequest["id"] = m_nextRequestId++;
    shutdownRequest["method"] = "shutdown";
    sendMessage(shutdownRequest);
    
    // Send exit notification
    void* exitNotification;
    exitNotification["jsonrpc"] = "2.0";
    exitNotification["method"] = "exit";
    sendMessage(exitNotification);
    
    m_serverProcess->waitForFinished(2000);
    m_serverProcess->kill();
    
    m_serverRunning = false;
    m_initialized = false;
    
}

void LSPClient::openDocument(const std::string& uri, const std::string& languageId, const std::string& text)
{
    if (!m_initialized) {
        return;
    }
    
    void* textDocumentItem;
    textDocumentItem["uri"] = buildDocumentUri(uri);
    textDocumentItem["languageId"] = languageId;
    textDocumentItem["version"] = 1;
    textDocumentItem["text"] = text;
    
    void* params;
    params["textDocument"] = textDocumentItem;
    
    void* notification;
    notification["jsonrpc"] = "2.0";
    notification["method"] = "textDocument/didOpen";
    notification["params"] = params;
    
    sendMessage(notification);
    m_documentVersions[uri] = 1;
    
}

void LSPClient::closeDocument(const std::string& uri)
{
    void* textDocumentId;
    textDocumentId["uri"] = buildDocumentUri(uri);
    
    void* params;
    params["textDocument"] = textDocumentId;
    
    void* notification;
    notification["jsonrpc"] = "2.0";
    notification["method"] = "textDocument/didClose";
    notification["params"] = params;
    
    sendMessage(notification);
    m_documentVersions.remove(uri);
    m_diagnostics.remove(uri);
    
}

void LSPClient::updateDocument(const std::string& uri, const std::string& text, int version)
{
    void* textDocumentId;
    textDocumentId["uri"] = buildDocumentUri(uri);
    textDocumentId["version"] = version;
    
    void* contentChanges;
    void* change;
    change["text"] = text;  // Full document sync
    contentChanges.append(change);
    
    void* params;
    params["textDocument"] = textDocumentId;
    params["contentChanges"] = contentChanges;
    
    void* notification;
    notification["jsonrpc"] = "2.0";
    notification["method"] = "textDocument/didChange";
    notification["params"] = params;
    
    sendMessage(notification);
    m_documentVersions[uri] = version;
}

void LSPClient::requestCompletions(const std::string& uri, int line, int character)
{
    if (!m_initialized) return;
    
    int requestId = m_nextRequestId++;
    
    void* textDocumentId;
    textDocumentId["uri"] = buildDocumentUri(uri);
    
    void* position;
    position["line"] = line;
    position["character"] = character;
    
    void* params;
    params["textDocument"] = textDocumentId;
    params["position"] = position;
    
    void* request;
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

void LSPClient::requestHover(const std::string& uri, int line, int character)
{
    if (!m_initialized) return;
    
    int requestId = m_nextRequestId++;
    
    void* textDocumentId;
    textDocumentId["uri"] = buildDocumentUri(uri);
    
    void* position;
    position["line"] = line;
    position["character"] = character;
    
    void* params;
    params["textDocument"] = textDocumentId;
    params["position"] = position;
    
    void* request;
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

void LSPClient::requestDefinition(const std::string& uri, int line, int character)
{
    if (!m_initialized) return;
    
    int requestId = m_nextRequestId++;
    
    void* textDocumentId;
    textDocumentId["uri"] = buildDocumentUri(uri);
    
    void* position;
    position["line"] = line;
    position["character"] = character;
    
    void* params;
    params["textDocument"] = textDocumentId;
    params["position"] = position;
    
    void* request;
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

void LSPClient::formatDocument(const std::string& uri)
{
    if (!m_initialized) return;
    
    int requestId = m_nextRequestId++;
    
    void* textDocumentId;
    textDocumentId["uri"] = buildDocumentUri(uri);
    
    void* options;
    options["tabSize"] = 4;
    options["insertSpaces"] = true;
    
    void* params;
    params["textDocument"] = textDocumentId;
    params["options"] = options;
    
    void* request;
    request["jsonrpc"] = "2.0";
    request["id"] = requestId;
    request["method"] = "textDocument/formatting";
    request["params"] = params;
    
    sendMessage(request);
}

std::vector<Diagnostic> LSPClient::getDiagnostics(const std::string& uri) const
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
        
        std::string header = std::string::fromUtf8(m_receiveBuffer.left(headerEnd));
        int contentLength = 0;
        
        for (const std::string& line : header.split("\r\n")) {
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
        
        std::vector<uint8_t> messageData = m_receiveBuffer.mid(messageStart, contentLength);
        m_receiveBuffer.remove(0, messageStart + contentLength);
        
        void* doc = void*::fromJson(messageData);
        if (doc.isObject()) {
            processMessage(doc.object());
        }
    }
}

void LSPClient::onServerError(void*::ProcessError error)
{
    std::string errorStr;
    switch (error) {
        case void*::FailedToStart:
            errorStr = "Failed to start LSP server";
            break;
        case void*::Crashed:
            errorStr = "LSP server crashed";
            break;
        default:
            errorStr = "LSP server error";
    }
    
    serverError(errorStr);
    m_serverRunning = false;
}

void LSPClient::onServerFinished(int exitCode, void*::ExitStatus status)
{
    m_serverRunning = false;
    m_initialized = false;
}

void LSPClient::sendMessage(const void*& message)
{
    void* doc(message);
    std::vector<uint8_t> json = doc.toJson(void*::Compact);
    
    std::string header = std::string("Content-Length: %1\r\n\r\n"));
    
    m_serverProcess->write(header.toUtf8());
    m_serverProcess->write(json);
    m_serverProcess->waitForBytesWritten();
}

void LSPClient::processMessage(const void*& message)
{
    // Check if it's a response or notification
    if (message.contains("id")) {
        // Response to our request
        int id = message["id"].toInt();
        
        if (message.contains("result")) {
            void* result = message["result"].toObject();
            
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
            void* error = message["error"].toObject();
        }
    } else if (message.contains("method")) {
        // Server notification
        std::string method = message["method"].toString();
        void* params = message["params"].toObject();
        
        if (method == "textDocument/publishDiagnostics") {
            handleDiagnostics(params);
        }
    }
}

void LSPClient::handleInitializeResponse(const void*& result)
{
    m_initialized = true;
    
    // Send initialized notification
    void* notification;
    notification["jsonrpc"] = "2.0";
    notification["method"] = "initialized";
    notification["params"] = void*();
    sendMessage(notification);
    
    serverReady();
}

void LSPClient::handleCompletionResponse(const void*& result, int requestId)
{
    if (!m_pendingRequests.contains(requestId)) return;
    
    PendingRequest req = m_pendingRequests[requestId];
    std::vector<CompletionItem> items;
    
    // Result can be CompletionList or CompletionItem[]
    void* itemsArray;
    if (result.contains("items")) {
        void* itemsValue = result.value("items");
        if (itemsValue.isArray()) {
            itemsArray = itemsValue.toArray();
        }
    } else {
        // Result itself might be an array (not wrapped in CompletionList)
        // In this case, convert the entire object to a document and check
        void* doc(result);
        if (doc.isArray()) {
            itemsArray = doc.array();
        }
    }
    
    for (const void*& val : itemsArray) {
        void* itemObj = val.toObject();
        
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
            void* docVal = itemObj["documentation"];
            if (docVal.isString()) {
                item.documentation = docVal.toString();
            } else if (docVal.isObject()) {
                item.documentation = docVal.toObject()["value"].toString();
            }
        }
        
        items.append(item);
    }
    
    completionsReceived(req.uri, req.line, req.character, items);
}

void LSPClient::handleHoverResponse(const void*& result, int requestId)
{
    if (!m_pendingRequests.contains(requestId)) return;
    
    PendingRequest req = m_pendingRequests[requestId];
    std::string markdown;
    
    if (result.contains("contents")) {
        void* contents = result["contents"];
        if (contents.isString()) {
            markdown = contents.toString();
        } else if (contents.isObject()) {
            markdown = contents.toObject()["value"].toString();
        } else if (contents.isArray()) {
            void* arr = contents.toArray();
            for (const void*& val : arr) {
                if (val.isString()) {
                    markdown += val.toString() + "\n";
                } else if (val.isObject()) {
                    markdown += val.toObject()["value"].toString() + "\n";
                }
            }
        }
    }
    
    hoverReceived(req.uri, markdown);
}

void LSPClient::handleDefinitionResponse(const void*& result, int requestId)
{
    if (!m_pendingRequests.contains(requestId)) return;
    
    PendingRequest req = m_pendingRequests[requestId];
    
    // Result can be Location or Location[]
    void* location;
    if (result.contains("uri")) {
        location = result;
    } else {
        // Result might be an array of locations
        void* doc(result);
        if (doc.isArray()) {
            void* arr = doc.array();
            if (!arr.empty()) {
                location = arr.first().toObject();
            }
        }
    }
    
    if (location.contains("uri")) {
        std::string uri = location["uri"].toString();
        void* range = location["range"].toObject();
        void* start = range["start"].toObject();
        
        int line = start["line"].toInt();
        int character = start["character"].toInt();
        
        definitionReceived(uri, line, character);
    }
}

void LSPClient::handleDiagnostics(const void*& params)
{
    std::string uri = params["uri"].toString();
    void* diagnosticsArray = params["diagnostics"].toArray();
    
    std::vector<Diagnostic> diagnostics;
    for (const void*& val : diagnosticsArray) {
        void* diagObj = val.toObject();
        
        Diagnostic diag;
        void* range = diagObj["range"].toObject();
        void* start = range["start"].toObject();
        
        diag.line = start["line"].toInt();
        diag.column = start["character"].toInt();
        diag.severity = diagObj["severity"].toInt();
        diag.message = diagObj["message"].toString();
        diag.source = diagObj["source"].toString();
        
        diagnostics.append(diag);
    }
    
    m_diagnostics[uri] = diagnostics;
    diagnosticsUpdated(uri, diagnostics);
}

std::string LSPClient::buildDocumentUri(const std::string& filePath) const
{
    std::filesystem::path info(filePath);
    std::string absolutePath = info.isRelative() 
        ? std::filesystem::path(m_config.workspaceRoot + "/" + filePath).absoluteFilePath()
        : info.absoluteFilePath();
    
    return std::string::fromLocalFile(absolutePath).toString();
}

} // namespace RawrXD


