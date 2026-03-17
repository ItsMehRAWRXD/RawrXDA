/**
 * \file lsp_client.cpp
 * \brief Implementation of LSPClient using Win32 API
 * \author RawrXD Team
 */

#include "lsp_client.h"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <windows.h>

namespace RawrXD {

using json = nlohmann::json;

// Helper to convert std::string to std::wstring
static std::wstring toWString(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

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

    createChildProcess();
    if (!m_serverRunning) return false;

    // Send initialize request
    json params = json::object();
    params["processId"] = GetCurrentProcessId();
    params["rootPath"] = m_config.workspaceRoot;
    if (m_config.workspaceRoot.empty()) {
        params["rootUri"] = nullptr;
    } else {
        params["rootUri"] = buildDocumentUri(m_config.workspaceRoot);
    }
    params["capabilities"] = json::object(); 

    json request = json::object();
    request["jsonrpc"] = "2.0";
    request["method"] = "initialize";
    request["id"] = 0; 
    request["params"] = params;

    sendMessage(request);

    return true;
}

void LSPClient::createChildProcess()
{
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    HANDLE hStdInRead = NULL;
    HANDLE hStdInWrite = NULL;
    HANDLE hStdOutRead = NULL;
    HANDLE hStdOutWrite = NULL;

    // Create StdIn pipe
    if (!CreatePipe(&hStdInRead, &hStdInWrite, &saAttr, 0)) return;
    // Create StdOut pipe
    if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &saAttr, 0)) {
        CloseHandle(hStdInRead); CloseHandle(hStdInWrite);
        return;
    }

    // Ensure the write handle to stdin and read handle from stdout are NOT inherited
    if (!SetHandleInformation(hStdInWrite, HANDLE_FLAG_INHERIT, 0)) return;
    if (!SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0)) return;

    PROCESS_INFORMATION piProcInfo;
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

    STARTUPINFOW siStartInfo;
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFOW));
    siStartInfo.cb = sizeof(STARTUPINFOW);
    siStartInfo.hStdError = hStdOutWrite; // Merge stderr into stdout for now
    siStartInfo.hStdOutput = hStdOutWrite;
    siStartInfo.hStdInput = hStdInRead;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    std::wstring cmdLine = toWString(m_config.command);
    for (const auto& arg : m_config.arguments) {
        cmdLine += L" " + toWString(arg);
    }

    // Create Process
    std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back(0);

    BOOL bSuccess = CreateProcessW(NULL,
        cmdBuf.data(),
        NULL,
        NULL,
        TRUE, // request handles inheritance
        CREATE_NO_WINDOW, 
        NULL,
        m_config.workspaceRoot.empty() ? NULL : toWString(m_config.workspaceRoot).c_str(), 
        &siStartInfo,
        &piProcInfo);

    // Close pipe handles that are now owned by child process
    CloseHandle(hStdOutWrite);
    CloseHandle(hStdInRead);

    if (!bSuccess) {
        CloseHandle(hStdOutRead);
        CloseHandle(hStdInWrite);
        return;
    }

    m_hProcess = piProcInfo.hProcess;
    m_hThread = piProcInfo.hThread;
    m_hStdInWrite = hStdInWrite;
    m_hStdOutRead = hStdOutRead;
    m_serverRunning = true;

    // Start reading thread
    m_stopThread = false;
    m_readThread = std::thread(&LSPClient::readFromChild, this);
}

void LSPClient::stopServer()
{
    if (!m_serverRunning) return;

    m_stopThread = true;

    // Close handles to force ReadFile to error out
    if (m_hStdInWrite) { CloseHandle((HANDLE)m_hStdInWrite); m_hStdInWrite = nullptr; }
    if (m_hStdOutRead) { CloseHandle((HANDLE)m_hStdOutRead); m_hStdOutRead = nullptr; }

    if (m_readThread.joinable()) {
        m_readThread.join();
    }

    if (m_hProcess) {
        TerminateProcess((HANDLE)m_hProcess, 0);
        CloseHandle((HANDLE)m_hProcess);
        m_hProcess = nullptr;
    }
    if (m_hThread) {
        CloseHandle((HANDLE)m_hThread);
        m_hThread = nullptr;
    }

    m_serverRunning = false;
    m_initialized = false;
}

void LSPClient::onServerReadyRead()
{
    const int bufferSize = 4096;
    char buffer[4096];
    DWORD bytesRead;

    while (m_serverRunning && !m_stopThread) {
        if (!ReadFile((HANDLE)m_hStdOutRead, buffer, bufferSize, &bytesRead, NULL) || bytesRead == 0) {
            break; // pipe broken
        }

        m_receiveBuffer.append(buffer, bytesRead);

        while (true) {
            size_t headerEnd = m_receiveBuffer.find("\r\n\r\n");
            if (headerEnd == std::string::npos) break; 

            size_t contentLengthPos = m_receiveBuffer.find("Content-Length: ");
            if (contentLengthPos == std::string::npos || contentLengthPos > headerEnd) {
                m_receiveBuffer.erase(0, headerEnd + 4);
                continue;
            }

            int contentLength = std::stoi(m_receiveBuffer.substr(contentLengthPos + 16, headerEnd - (contentLengthPos + 16)));

            if (m_receiveBuffer.size() < headerEnd + 4 + contentLength) {
                break; 
            }

            std::string body = m_receiveBuffer.substr(headerEnd + 4, contentLength);
            m_receiveBuffer.erase(0, headerEnd + 4 + contentLength);

            try {
                auto msg = json::parse(body);
                processMessage(msg);
            } catch (...) {
                std::cerr << "LSP JSON Parse Error" << std::endl;
            }
        }
    }

    if (m_serverRunning) {
        onServerFinished(0, 0);
    }
}

void LSPClient::sendMessage(const json& message)
{
    if (!m_serverRunning || !m_hStdInWrite) return;

    std::string jsonStr = message.dump();
    std::string packet = "Content-Length: " + std::to_string(jsonStr.length()) + "\r\n\r\n" + jsonStr;

    std::lock_guard<std::mutex> lock(m_transportMutex);
    DWORD bytesWritten;
    WriteFile((HANDLE)m_hStdInWrite, packet.c_str(), (DWORD)packet.length(), &bytesWritten, NULL);
}

void LSPClient::processMessage(const json& message)
{
    if (message.contains("id") && !message["id"].is_null()) {
        int id = message["id"];
        if (message.contains("result")) {
            if (id == 0) {
                handleInitializeResponse(message["result"]);
            } else if (m_pendingRequests.count(id)) {
                auto& req = m_pendingRequests[id];
                if (req.type == "completion") handleCompletionResponse(message["result"], id);
                else if (req.type == "hover") handleHoverResponse(message["result"], id);
                else if (req.type == "definition") handleDefinitionResponse(message["result"], id);
                m_pendingRequests.erase(id);
            }
        }
    } else if (message.contains("method")) {
        if (message["method"] == "textDocument/publishDiagnostics") {
            handleDiagnostics(message["params"]);
        }
    }
}

void LSPClient::handleInitializeResponse(const json& result)
{
    m_initialized = true;
    serverReady();
    
    json notification = json::object();
    notification["jsonrpc"] = "2.0";
    notification["method"] = "initialized";
    notification["params"] = json::object();
    sendMessage(notification);
}

void LSPClient::handleCompletionResponse(const json& result, int requestId)
{
    std::vector<CompletionItem> items;
    const json* itemList = nullptr;
    if (result.is_array()) itemList = &result;
    else if (result.is_object() && result.contains("items")) itemList = &result["items"];

    if (itemList) {
        for (const auto& item : *itemList) {
            CompletionItem ci;
            ci.label = item.value("label", "");
            ci.insertText = item.value("insertText", ci.label);
            ci.detail = item.value("detail", "");
            ci.documentation = item.value("documentation", "");
            ci.kind = item.value("kind", 1);
            ci.sortText = item.value("sortText", "");
            ci.filterText = item.value("filterText", "");
            items.push_back(ci);
        }
    }
    completionsReceived("", 0, 0, items); 
}

void LSPClient::handleHoverResponse(const json& result, int requestId)
{
    std::string markdown;
    if (result.contains("contents")) {
        auto contents = result["contents"];
        if (contents.is_string()) markdown = contents;
        else if (contents.is_object() && contents.contains("value")) markdown = contents["value"];
        else if (contents.is_array()) {
            for (const auto& part : contents) {
                if (part.is_string()) markdown += part.get<std::string>() + "\n";
                else if (part.contains("value")) markdown += part["value"].get<std::string>() + "\n";
            }
        }
    }
    hoverReceived("", markdown);
}

void LSPClient::handleDefinitionResponse(const json& result, int requestId)
{
    if (result.is_array() && !result.empty()) {
        auto& loc = result[0];
        std::string uri = loc.value("uri", "");
        definitionReceived(uri, 0, 0); 
    } else if (result.is_object()) {
        std::string uri = result.value("uri", "");
        definitionReceived(uri, 0, 0);
    }
}

void LSPClient::handleDiagnostics(const json& params)
{
    std::string uri = params.value("uri", "");
    std::vector<Diagnostic> batch;

    if (params.contains("diagnostics") && params["diagnostics"].is_array()) {
        for (const auto& diag : params["diagnostics"]) {
            Diagnostic d;
            d.message = diag.value("message", "");
            d.severity = diag.value("severity", 1);
            d.source = diag.value("source", "LSP");

            if (diag.contains("range")) {
                d.line = diag["range"]["start"].value("line", 0);
                d.column = diag["range"]["start"].value("character", 0);
            }
            batch.push_back(d);
        }
    }
    m_diagnostics[uri] = batch;
    diagnosticsUpdated(uri, batch);
}

void LSPClient::serverReady() {}
void LSPClient::completionsReceived(const std::string& uri, int line, int character, const std::vector<CompletionItem>& items) {}
void LSPClient::hoverReceived(const std::string& uri, const std::string& markdown) {}
void LSPClient::definitionReceived(const std::string& uri, int line, int character) {}
void LSPClient::diagnosticsUpdated(const std::string& uri, const std::vector<Diagnostic>& diagnostics) {}
void LSPClient::formatEditsReceived(const std::string& uri, const std::string& formattedText) {}
void LSPClient::serverError(const std::string& error) { std::cerr << "LSP Error: " << error << std::endl; }
void LSPClient::onServerFinished(int exitCode, int status) { std::cout << "LSP Server exited" << std::endl; }

std::string LSPClient::buildDocumentUri(const std::string& filePath) const {
    return "file:///" + filePath; 
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

    json req = json::object();
    req["jsonrpc"] = "2.0";
    req["method"] = "textDocument/completion";
    req["id"] = reqId;
    req["params"] = params;

    sendMessage(req);
}

void LSPClient::requestHover(const std::string& uri, int line, int character)
{
    int reqId = m_nextRequestId++;
    PendingRequest pending{"hover", uri, line, character};
    m_pendingRequests[reqId] = pending;

    json params = json::object();
    params["textDocument"] = { {"uri", uri} };
    params["position"] = { {"line", line}, {"character", character} };

    json req = json::object();
    req["jsonrpc"] = "2.0";
    req["method"] = "textDocument/hover";
    req["id"] = reqId;
    req["params"] = params;

    sendMessage(req);
}

void LSPClient::requestDefinition(const std::string& uri, int line, int character)
{
    int reqId = m_nextRequestId++;
    PendingRequest pending{"definition", uri, line, character};
    m_pendingRequests[reqId] = pending;

    json params = json::object();
    params["textDocument"] = { {"uri", uri} };
    params["position"] = { {"line", line}, {"character", character} };

    json req = json::object();
    req["jsonrpc"] = "2.0";
    req["method"] = "textDocument/definition";
    req["id"] = reqId;
    req["params"] = params;
    sendMessage(req);
}

void LSPClient::formatDocument(const std::string& uri)
{
    // Stub
}

std::vector<Diagnostic> LSPClient::getDiagnostics(const std::string& uri) const
{
    if (m_diagnostics.count(uri)) return m_diagnostics.at(uri);
    return {};
}

} // namespace RawrXD
