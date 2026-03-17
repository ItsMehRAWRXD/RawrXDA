#include "lsp_client.h"
#include <iostream>
#include <windows.h>

namespace RawrXD {

using json = nlohmann::json;

LSPClient::LSPClient(const LSPServerConfig& config)
    : m_config(config)
{
}

LSPClient::~LSPClient()
{
    stopServer();
}

void LSPClient::initialize()
{
    if (m_config.autoStart) {
        startServer();
    }
}

bool LSPClient::startServer()
{
    if (m_serverRunning) return true;
    m_serverRunning = true;
    // Stub: simulate server start
    serverReady();
    return true;
}

void LSPClient::stopServer()
{
    m_serverRunning = false;
    m_initialized = false;
}

void LSPClient::openDocument(const std::string& uri, const std::string& languageId, const std::string& text)
{
    json params = json::object();
    params["textDocument"] = json::object();
    params["textDocument"]["uri"] = uri;
    params["textDocument"]["languageId"] = languageId;
    params["textDocument"]["version"] = 1;
    params["textDocument"]["text"] = text;
    
    m_documentVersions[uri] = 1;
    
    json notification = json::object();
    notification["jsonrpc"] = "2.0";
    notification["method"] = "textDocument/didOpen";
    notification["params"] = params;
    
    sendMessage(notification);
}

void LSPClient::closeDocument(const std::string& uri)
{
    json params = json::object();
    params["textDocument"] = json::object();
    params["textDocument"]["uri"] = uri;
    
    json notification = json::object();
    notification["jsonrpc"] = "2.0";
    notification["method"] = "textDocument/didClose";
    notification["params"] = params;
    
    sendMessage(notification);
    m_documentVersions.erase(uri);
}

void LSPClient::updateDocument(const std::string& uri, const std::string& text, int version)
{
    int newVersion = (version > 0) ? version : (m_documentVersions[uri] + 1);
    m_documentVersions[uri] = newVersion;
    
    json change = json::object();
    change["text"] = text;
    
    json params = json::object();
    params["textDocument"] = json::object();
    params["textDocument"]["uri"] = uri;
    params["textDocument"]["version"] = newVersion;
    params["contentChanges"] = json::array();
    params["contentChanges"].push_back(change);
    
    json notification = json::object();
    notification["jsonrpc"] = "2.0";
    notification["method"] = "textDocument/didChange";
    notification["params"] = params;
    
    sendMessage(notification);
}

void LSPClient::requestCompletions(const std::string& uri, int line, int character)
{
    int reqId = m_nextRequestId++;
    PendingRequest pending;
    pending.type = "completion";
    pending.uri = uri;
    pending.line = line;
    pending.character = character;
    m_pendingRequests[reqId] = pending;
    
    json params = json::object();
    params["textDocument"] = json::object();
    params["textDocument"]["uri"] = uri;
    params["position"] = json::object();
    params["position"]["line"] = line;
    params["position"]["character"] = character;
    
    json request = json::object();
    request["jsonrpc"] = "2.0";
    request["id"] = reqId;
    request["method"] = "textDocument/completion";
    request["params"] = params;
    
    sendMessage(request);
}

void LSPClient::requestHover(const std::string& uri, int line, int character)
{
    int reqId = m_nextRequestId++;
    PendingRequest pending;
    pending.type = "hover";
    pending.uri = uri;
    pending.line = line;
    pending.character = character;
    m_pendingRequests[reqId] = pending;
    
    json params = json::object();
    params["textDocument"] = json::object();
    params["textDocument"]["uri"] = uri;
    params["position"] = json::object();
    params["position"]["line"] = line;
    params["position"]["character"] = character;
    
    json request = json::object();
    request["jsonrpc"] = "2.0";
    request["id"] = reqId;
    request["method"] = "textDocument/hover";
    request["params"] = params;
    
    sendMessage(request);
}

void LSPClient::requestDefinition(const std::string& uri, int line, int character)
{
    int reqId = m_nextRequestId++;
    PendingRequest pending;
    pending.type = "definition";
    pending.uri = uri;
    pending.line = line;
    pending.character = character;
    m_pendingRequests[reqId] = pending;
    
    json params = json::object();
    params["textDocument"] = json::object();
    params["textDocument"]["uri"] = uri;
    params["position"] = json::object();
    params["position"]["line"] = line;
    params["position"]["character"] = character;
    
    json request = json::object();
    request["jsonrpc"] = "2.0";
    request["id"] = reqId;
    request["method"] = "textDocument/definition";
    request["params"] = params;
    
    sendMessage(request);
}

void LSPClient::formatDocument(const std::string& uri)
{
    // Stub
}

std::vector<Diagnostic> LSPClient::getDiagnostics(const std::string& uri) const
{
    auto it = m_diagnostics.find(uri);
    if (it != m_diagnostics.end()) {
        return it->second;
    }
    return {};
}

// Private methods
void LSPClient::sendMessage(const json& message)
{
    // Stub: write to process stdin
    // std::cout << "LSP Send: " << message.dump() << std::endl;
}

void LSPClient::processMessage(const json& message)
{
    if (message.count("result")) {
        int id = message.value("id", 0);
        if (m_pendingRequests.count(id)) {
            std::string type = m_pendingRequests[id].type;
            if (type == "completion") handleCompletionResponse(message["result"], id);
            else if (type == "hover") handleHoverResponse(message["result"], id);
            else if (type == "definition") handleDefinitionResponse(message["result"], id);
            m_pendingRequests.erase(id);
        }
    } else if (message.count("method")) {
        // Notification
        std::string method = message["method"];
        if (method == "textDocument/publishDiagnostics") {
            handleDiagnostics(message["params"]);
        }
    }
}

void LSPClient::handleInitializeResponse(const json& result)
{
    m_initialized = true;
    serverReady();
}

void LSPClient::handleCompletionResponse(const json& result, int requestId)
{
    std::vector<CompletionItem> items;
    // Parse result...
    PendingRequest req = m_pendingRequests[requestId];
    completionsReceived(req.uri, req.line, req.character, items);
}

void LSPClient::handleHoverResponse(const json& result, int requestId)
{
    PendingRequest req = m_pendingRequests[requestId];
    std::string markdown = "Hover info"; 
    // Parse result...
    hoverReceived(req.uri, markdown);
}

void LSPClient::handleDefinitionResponse(const json& result, int requestId)
{
    PendingRequest req = m_pendingRequests[requestId];
    // Parse result...
    definitionReceived(req.uri, 0, 0); 
}

void LSPClient::handleDiagnostics(const json& params)
{
    std::string uri = params.value("uri", std::string(""));
    std::vector<Diagnostic> diags;
    // Parse params["diagnostics"]...
    m_diagnostics[uri] = diags;
    diagnosticsUpdated(uri, diags);
}

void LSPClient::createChildProcess() {
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    // Create a pipe for the child process's STDOUT.
    if (!CreatePipe(&m_hChildStd_OUT_Rd, &m_hChildStd_OUT_Wr, &saAttr, 0)) return;
    if (!SetHandleInformation(m_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) return;

    // Create a pipe for the child process's STDIN.
    if (!CreatePipe(&m_hChildStd_IN_Rd, &m_hChildStd_IN_Wr, &saAttr, 0)) return;
    if (!SetHandleInformation(m_hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0)) return;

    // Create the child process.
    STARTUPINFOA siStartInfo;
    ZeroMemory(&m_piProcInfo, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdError = m_hChildStd_OUT_Wr; // Redirect stderr to stdout for now
    siStartInfo.hStdOutput = m_hChildStd_OUT_Wr;
    siStartInfo.hStdInput = m_hChildStd_IN_Rd;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    std::string cmd = m_config.command;
    for (const auto& arg : m_config.arguments) {
        cmd += " " + arg;
    }

    // Mutable command string for CreateProcess
    std::vector<char> cmdVec(cmd.begin(), cmd.end());
    cmdVec.push_back(0);

    bool bSuccess = CreateProcessA(NULL,
        cmdVec.data(),     // command line
        NULL,          // process security attributes
        NULL,          // primary thread security attributes
        TRUE,          // handles are inherited
        0,             // creation flags
        NULL,          // use parent's environment
        m_config.workspaceRoot.empty() ? NULL : m_config.workspaceRoot.c_str(), 
        &siStartInfo,  // STARTUPINFO pointer
        &m_piProcInfo); // receives PROCESS_INFORMATION

    if (!bSuccess) {
        serverError("Failed to create process");
    } else {
        m_serverRunning = true;
        // Start reading thread
        std::thread([this]() {
            readFromChild();
        }).detach();
    }
}

void LSPClient::writeToChild(const std::string& message) {
    if (!m_serverRunning) return;
    DWORD dwWritten;
    // JSON-RPC requires content-length header
    std::string fullMsg = "Content-Length: " + std::to_string(message.length()) + "\r\n\r\n" + message;
    WriteFile(m_hChildStd_IN_Wr, fullMsg.c_str(), (DWORD)fullMsg.length(), &dwWritten, NULL);
}

void LSPClient::readFromChild() {
    DWORD dwRead;
    const int BUFSIZE = 4096;
    CHAR chBuf[BUFSIZE];
    std::string buffer;

    while (m_serverRunning) {
        if (!ReadFile(m_hChildStd_OUT_Rd, chBuf, BUFSIZE, &dwRead, NULL) || dwRead == 0) break;
        
        buffer.append(chBuf, dwRead);
        
        // Basic parsing of Content-Length loop
        while (true) {
            size_t headerEnd = buffer.find("\r\n\r\n");
            if (headerEnd == std::string::npos) break;
            
            size_t lenPos = buffer.find("Content-Length: ");
            if (lenPos == std::string::npos || lenPos > headerEnd) {
                // Invalid header? discard?
                buffer.erase(0, headerEnd + 4);
                continue;
            }
            
            int contentLength = std::stoi(buffer.substr(lenPos + 16, headerEnd - (lenPos + 16)));
            if (buffer.length() >= headerEnd + 4 + contentLength) {
                std::string body = buffer.substr(headerEnd + 4, contentLength);
                buffer.erase(0, headerEnd + 4 + contentLength);
                
                try {
                    auto j = json::parse(body);
                    processMessage(j);
                } catch (...) {}
            } else {
                break; // Wait for more data
            }
        }
    }
}

// Signals (Stubs)
void LSPClient::serverReady() {}
void LSPClient::completionsReceived(const std::string& uri, int line, int character, const std::vector<CompletionItem>& items) {}
void LSPClient::hoverReceived(const std::string& uri, const std::string& markdown) {}
void LSPClient::definitionReceived(const std::string& uri, int line, int character) {}
void LSPClient::diagnosticsUpdated(const std::string& uri, const std::vector<Diagnostic>& diagnostics) {}
void LSPClient::formatEditsReceived(const std::string& uri, const std::string& formattedText) {}
void LSPClient::serverError(const std::string& error) {}

// Private internal slots (stubs)
void LSPClient::onServerReadyRead() {}
void LSPClient::onServerError(int error) {}
void LSPClient::onServerFinished(int exitCode, int status) {}

std::string LSPClient::buildDocumentUri(const std::string& filePath) const
{
    return "file://" + filePath;
}

} // namespace RawrXD
