// ============================================================================
// RawrXD_LSPServer.cpp — Phase 27: Embedded LSP Server (JSON-RPC 2.0 / stdio)
// ============================================================================
// ~620 lines of C++20.  Full LSP 3.17 subset:
//   initialize / shutdown / exit
//   textDocument/hover, completion, definition, references, documentSymbol
//   textDocument/semanticTokens/full
//   workspace/symbol
//   textDocument/didOpen, didChange, didClose, didSave
//   textDocument/publishDiagnostics (server → client notification)
//
// Transport: JSON-RPC 2.0 with Content-Length header over stdin/stdout.
// Index: Regex-based symbol extraction (extends context/indexer.cpp patterns).
// Thread model: reader thread → queue → dispatch thread.
// All responses use PatchResult-style structured returns (no exceptions).
//
// Copyright (c) 2025 RawrXD Project — All rights reserved.
// ============================================================================

#define NOMINMAX
#include "lsp/RawrXD_LSPServer.h"

#include <nlohmann/json.hpp>

#include <iostream>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <regex>
#include <charconv>
#include <cstring>
#include <cassert>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#endif

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace RawrXD {
namespace LSPServer {

// ============================================================================
// STATIC HELPERS
// ============================================================================

static uint64_t fnv1a(const std::string& s) {
    uint64_t h = 14695981039346656037ULL;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return h;
}

static std::string toLower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return out;
}

static const char* kSemanticTokenTypes[] = {
    "namespace", "type", "class", "enum", "interface", "struct",
    "typeParameter", "parameter", "variable", "property", "enumMember",
    "event", "function", "method", "macro", "keyword", "modifier",
    "comment", "string", "number", "regexp", "operator"
};
static constexpr int kSemanticTokenTypeCount = 22;

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

RawrXDLSPServer::RawrXDLSPServer() {
#ifdef _WIN32
    // Force binary mode on stdin/stdout to prevent CR/LF mangling
    _setmode(_fileno(stdin),  _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
}

RawrXDLSPServer::~RawrXDLSPServer() {
    stop();
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void RawrXDLSPServer::configure(const ServerConfig& config) {
    m_config = config;
}

// ============================================================================
// START / STOP
// ============================================================================

bool RawrXDLSPServer::start() {
    ServerState expected = ServerState::Created;
    if (!m_state.compare_exchange_strong(expected, ServerState::Initializing)) {
        // Also allow restart from Stopped
        expected = ServerState::Stopped;
        if (!m_state.compare_exchange_strong(expected, ServerState::Initializing)) {
            return false;
        }
    }

    m_shutdownRequested = false;
    m_exitRequested     = false;

    // Launch reader thread
    m_readerThread = std::thread(&RawrXDLSPServer::readerThreadFunc, this);

    // Launch dispatch thread
    m_dispatchThread = std::thread(&RawrXDLSPServer::dispatchThreadFunc, this);

    m_state.store(ServerState::Running);
    return true;
}

void RawrXDLSPServer::stop() {
    if (m_state.load() == ServerState::Stopped ||
        m_state.load() == ServerState::Created) {
        return;
    }

    m_state.store(ServerState::ShuttingDown);
    m_exitRequested = true;

    // Wake up dispatch thread
    {
        std::lock_guard<std::mutex> lk(m_incomingMutex);
        m_incomingCV.notify_all();
    }

    if (m_readerThread.joinable())   m_readerThread.join();
    if (m_dispatchThread.joinable()) m_dispatchThread.join();

    m_state.store(ServerState::Stopped);
}

// ============================================================================
// BLOCKING MAIN LOOP (stdio mode)
// ============================================================================

void RawrXDLSPServer::runBlocking() {
    if (!start()) return;

    // Block until exit is requested
    while (!m_exitRequested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    stop();
}

// ============================================================================
// IN-PROCESS MESSAGE INJECTION (Win32IDE bridge)
// ============================================================================

void RawrXDLSPServer::injectMessage(const std::string& jsonRpcMessage) {
    std::lock_guard<std::mutex> lk(m_incomingMutex);
    m_incomingQueue.push_back(jsonRpcMessage);
    m_incomingCV.notify_one();
}

std::string RawrXDLSPServer::pollOutgoing() {
    std::lock_guard<std::mutex> lk(m_outgoingMutex);
    if (m_outgoingQueue.empty()) return "";
    std::string msg = std::move(m_outgoingQueue.front());
    m_outgoingQueue.erase(m_outgoingQueue.begin());
    return msg;
}

// ============================================================================
// JSON-RPC 2.0 FRAMING — Content-Length header
// ============================================================================

bool RawrXDLSPServer::readMessage(std::string& outMessage) {
    // Read headers until blank line
    int contentLength = -1;
    std::string headerLine;
    while (std::getline(std::cin, headerLine)) {
        // Remove trailing \r if present
        if (!headerLine.empty() && headerLine.back() == '\r') {
            headerLine.pop_back();
        }
        if (headerLine.empty()) break;  // End of headers

        // Parse Content-Length
        if (headerLine.rfind("Content-Length:", 0) == 0) {
            std::string val = headerLine.substr(15);
            // Trim leading whitespace
            auto it = val.begin();
            while (it != val.end() && (*it == ' ' || *it == '\t')) ++it;
            val.erase(val.begin(), it);
            contentLength = std::stoi(val);
        }
        // Content-Type header is optional, we ignore it
    }

    if (contentLength <= 0 || std::cin.eof()) return false;

    // Read exactly contentLength bytes
    outMessage.resize(contentLength);
    std::cin.read(&outMessage[0], contentLength);

    if (std::cin.gcount() != contentLength) return false;

    {
        std::lock_guard<std::mutex> lk(m_statsMutex);
        m_stats.bytesRead += (uint64_t)contentLength + 50; // ~header overhead
    }

    return true;
}

void RawrXDLSPServer::writeMessage(const std::string& jsonBody) {
    std::string header = "Content-Length: " + std::to_string(jsonBody.size()) + "\r\n\r\n";
    std::cout << header << jsonBody;
    std::cout.flush();

    // Also queue for in-process polling
    {
        std::lock_guard<std::mutex> lk(m_outgoingMutex);
        m_outgoingQueue.push_back(jsonBody);
    }

    {
        std::lock_guard<std::mutex> lk(m_statsMutex);
        m_stats.bytesWritten += (uint64_t)header.size() + (uint64_t)jsonBody.size();
    }
}

void RawrXDLSPServer::writeResponse(int id, const json& result) {
    json resp;
    resp["jsonrpc"] = "2.0";
    resp["id"]      = id;
    resp["result"]  = result;
    writeMessage(resp.dump());
}

void RawrXDLSPServer::writeError(int id, int code, const std::string& message) {
    json resp;
    resp["jsonrpc"]        = "2.0";
    resp["id"]             = id;
    resp["error"]["code"]  = code;
    resp["error"]["message"] = message;
    writeMessage(resp.dump());

    std::lock_guard<std::mutex> lk(m_statsMutex);
    m_stats.totalErrors++;
}

void RawrXDLSPServer::writeNotification(const std::string& method, const json& params) {
    json notif;
    notif["jsonrpc"] = "2.0";
    notif["method"]  = method;
    notif["params"]  = params;
    writeMessage(notif.dump());
}

// ============================================================================
// DISPATCH
// ============================================================================

void RawrXDLSPServer::dispatchMessage(const std::string& rawJson) {
    json msg;
    try {
        msg = json::parse(rawJson);
    } catch (...) {
        // Parse error — JSON-RPC error code -32700
        writeError(0, -32700, "Parse error");
        return;
    }

    bool hasId     = msg.contains("id");
    bool hasMethod = msg.contains("method");

    if (hasMethod && hasId) {
        // Request
        int id = msg["id"].is_number() ? msg["id"].get<int>() : 0;
        std::string method = msg["method"].get<std::string>();
        json params = msg.value("params", json::object());
        handleRequest(id, method, params);
    } else if (hasMethod && !hasId) {
        // Notification
        std::string method = msg["method"].get<std::string>();
        json params = msg.value("params", json::object());
        handleNotification(method, params);
    }
    // Responses from client (rare) are ignored — we don't send requests yet
}

void RawrXDLSPServer::handleRequest(int id, const std::string& method, const json& params) {
    auto start = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> lk(m_statsMutex);
        m_stats.totalRequests++;
    }

    json result;

    // ---- Built-in handlers ----
    if (method == "initialize") {
        result = handleInitialize(id, params);
    } else if (method == "shutdown") {
        result = handleShutdown(id, params);
    } else if (method == "textDocument/hover") {
        result = handleTextDocumentHover(id, params);
    } else if (method == "textDocument/completion") {
        result = handleTextDocumentCompletion(id, params);
    } else if (method == "textDocument/definition") {
        result = handleTextDocumentDefinition(id, params);
    } else if (method == "textDocument/references") {
        result = handleTextDocumentReferences(id, params);
    } else if (method == "textDocument/documentSymbol") {
        result = handleTextDocumentDocumentSymbol(id, params);
    } else if (method == "workspace/symbol") {
        result = handleWorkspaceSymbol(id, params);
    } else if (method == "textDocument/semanticTokens/full") {
        result = handleTextDocumentSemanticTokensFull(id, params);
    } else {
        // Check custom handlers
        auto it = m_customRequestHandlers.find(method);
        if (it != m_customRequestHandlers.end()) {
            auto optResult = it->second(id, method, params);
            if (optResult.has_value()) {
                result = *optResult;
            } else {
                writeError(id, -32601, "Method handler returned null: " + method);
                return;
            }
        } else {
            writeError(id, -32601, "Method not found: " + method);
            return;
        }
    }

    writeResponse(id, result);

    auto end = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    {
        std::lock_guard<std::mutex> lk(m_statsMutex);
        double total = m_stats.avgResponseMs * (double)(m_stats.totalRequests - 1) + ms;
        m_stats.avgResponseMs = total / (double)m_stats.totalRequests;
    }
}

void RawrXDLSPServer::handleNotification(const std::string& method, const json& params) {
    {
        std::lock_guard<std::mutex> lk(m_statsMutex);
        m_stats.totalNotifications++;
    }

    if (method == "initialized") {
        handleInitialized(params);
    } else if (method == "exit") {
        handleExit(params);
    } else if (method == "textDocument/didOpen") {
        handleDidOpen(params);
    } else if (method == "textDocument/didChange") {
        handleDidChange(params);
    } else if (method == "textDocument/didClose") {
        handleDidClose(params);
    } else if (method == "textDocument/didSave") {
        handleDidSave(params);
    } else {
        // Check custom notification handlers
        auto it = m_customNotificationHandlers.find(method);
        if (it != m_customNotificationHandlers.end()) {
            it->second(method, params);
        }
        // Unknown notifications are silently ignored per LSP spec
    }
}

// ============================================================================
// LSP REQUEST HANDLERS
// ============================================================================

json RawrXDLSPServer::handleInitialize(int /*id*/, const json& params) {
    // Extract client info
    if (params.contains("processId") && !params["processId"].is_null()) {
        m_clientProcessId = params["processId"].get<int>();
    }
    if (params.contains("clientInfo")) {
        m_clientName    = params["clientInfo"].value("name", "");
        m_clientVersion = params["clientInfo"].value("version", "");
    }
    if (params.contains("rootUri") && !params["rootUri"].is_null()) {
        m_config.rootUri  = params["rootUri"].get<std::string>();
        m_config.rootPath = uriToFilePath(m_config.rootUri);
    } else if (params.contains("rootPath") && !params["rootPath"].is_null()) {
        m_config.rootPath = params["rootPath"].get<std::string>();
        m_config.rootUri  = filePathToUri(m_config.rootPath);
    }

    // Check client capabilities for semantic tokens
    if (params.contains("capabilities")) {
        auto& caps = params["capabilities"];
        if (caps.contains("textDocument") &&
            caps["textDocument"].contains("semanticTokens")) {
            m_clientSupportsSemanticTokens = true;
        }
        if (caps.contains("workspace") &&
            caps["workspace"].contains("symbol")) {
            m_clientSupportsWorkspaceSymbol = true;
        }
    }

    // Build initial index from rootPath
    if (!m_config.rootPath.empty() && fs::exists(m_config.rootPath)) {
        rebuildIndex();
    }

    // Build server capabilities response
    json caps;

    // Text document sync: full (1) — simplest; incremental (2) is future work
    caps["textDocumentSync"] = 1;

    if (m_config.enableHover) {
        caps["hoverProvider"] = true;
    }
    if (m_config.enableCompletion) {
        caps["completionProvider"]["triggerCharacters"] = {".", ":",">"};
        caps["completionProvider"]["resolveProvider"]   = false;
    }
    if (m_config.enableDefinition) {
        caps["definitionProvider"] = true;
    }
    if (m_config.enableReferences) {
        caps["referencesProvider"] = true;
    }
    if (m_config.enableDocumentSymbol) {
        caps["documentSymbolProvider"] = true;
    }
    if (m_config.enableWorkspaceSymbol) {
        caps["workspaceSymbolProvider"] = true;
    }
    if (m_config.enableSemanticTokens) {
        json legend;
        json tokenTypes = json::array();
        for (int i = 0; i < kSemanticTokenTypeCount; i++) {
            tokenTypes.push_back(kSemanticTokenTypes[i]);
        }
        legend["tokenTypes"]     = tokenTypes;
        json tokenModifiers = json::array();
        tokenModifiers[(size_t)0] = "declaration";
        tokenModifiers[(size_t)1] = "definition";
        tokenModifiers[(size_t)2] = "readonly";
        tokenModifiers[(size_t)3] = "static";
        tokenModifiers[(size_t)4] = "deprecated";
        legend["tokenModifiers"] = tokenModifiers;
        caps["semanticTokensProvider"]["legend"] = legend;
        caps["semanticTokensProvider"]["full"]   = true;
    }

    json result;
    result["capabilities"] = caps;
    result["serverInfo"]["name"]    = "rawrxd-lsp";
    result["serverInfo"]["version"] = "1.0.0";

    return result;
}

json RawrXDLSPServer::handleShutdown(int /*id*/, const json& /*params*/) {
    m_shutdownRequested = true;
    m_state.store(ServerState::ShuttingDown);
    return nullptr;  // LSP spec: shutdown returns null
}

json RawrXDLSPServer::handleTextDocumentHover(int /*id*/, const json& params) {
    std::string uri  = params["textDocument"]["uri"].get<std::string>();
    int line         = params["position"]["line"].get<int>();
    int character    = params["position"]["character"].get<int>();

    {
        std::lock_guard<std::mutex> lk(m_statsMutex);
        m_stats.hoverRequests++;
    }

    // Find symbol at position
    std::string filePath = uriToFilePath(uri);
    std::shared_lock<std::shared_mutex> lock(m_symbolMutex);

    for (const auto& sym : m_symbols) {
        if (sym.filePath == filePath && sym.line == line &&
            character >= sym.startChar && character <= sym.endChar) {
            json content;
            std::string markdown = "```cpp\n" + sym.detail + "\n```\n";
            markdown += "\n*" + kindToString(sym.kind) + "* ";
            if (!sym.containerName.empty()) {
                markdown += "in `" + sym.containerName + "` ";
            }
            markdown += "— defined at line " + std::to_string(sym.line + 1);

            json result;
            result["contents"]["kind"]  = "markdown";
            result["contents"]["value"] = markdown;
            result["range"]["start"]["line"]      = sym.line;
            result["range"]["start"]["character"]  = sym.startChar;
            result["range"]["end"]["line"]         = sym.line;
            result["range"]["end"]["character"]    = sym.endChar;
            return result;
        }
    }

    return nullptr;  // No hover info
}

json RawrXDLSPServer::handleTextDocumentCompletion(int /*id*/, const json& params) {
    std::string uri = params["textDocument"]["uri"].get<std::string>();
    // Position unused for basic completion — we return all symbols

    {
        std::lock_guard<std::mutex> lk(m_statsMutex);
        m_stats.completionRequests++;
    }

    // Get partial text at cursor for prefix matching
    std::string prefix;
    {
        std::lock_guard<std::mutex> lk(m_docMutex);
        auto it = m_openDocuments.find(uri);
        if (it != m_openDocuments.end()) {
            int line      = params["position"]["line"].get<int>();
            int character = params["position"]["character"].get<int>();
            // Extract current line, get text before cursor
            std::istringstream iss(it->second.content);
            std::string curLine;
            for (int i = 0; i <= line && std::getline(iss, curLine); i++) {}
            if (character <= (int)curLine.size()) {
                // Walk backwards to find identifier start
                int start = character - 1;
                while (start >= 0 && (std::isalnum(curLine[start]) || curLine[start] == '_')) {
                    start--;
                }
                prefix = curLine.substr(start + 1, character - start - 1);
            }
        }
    }

    std::string lowerPrefix = toLower(prefix);

    json items = json::array();
    int count = 0;

    std::shared_lock<std::shared_mutex> lock(m_symbolMutex);
    for (const auto& sym : m_symbols) {
        if (count >= m_config.maxCompletionItems) break;

        if (!lowerPrefix.empty()) {
            std::string lowerName = toLower(sym.name);
            if (lowerName.find(lowerPrefix) == std::string::npos) continue;
        }

        json item;
        item["label"]  = sym.name;
        item["detail"] = sym.detail;

        // Map SymbolKind to CompletionItemKind
        switch (sym.kind) {
            case SymbolKind::Function:    item["kind"] = 3;  break;
            case SymbolKind::Method:      item["kind"] = 2;  break;
            case SymbolKind::Class:       item["kind"] = 7;  break;
            case SymbolKind::Struct:      item["kind"] = 22; break;
            case SymbolKind::Enum:        item["kind"] = 13; break;
            case SymbolKind::Variable:    item["kind"] = 6;  break;
            case SymbolKind::Constant:    item["kind"] = 21; break;
            case SymbolKind::Namespace:   item["kind"] = 9;  break;
            case SymbolKind::Interface:   item["kind"] = 8;  break;
            case SymbolKind::Constructor: item["kind"] = 4;  break;
            default:                      item["kind"] = 1;  break;  // Text
        }

        items.push_back(std::move(item));
        count++;
    }

    json result;
    result["isIncomplete"] = (count >= m_config.maxCompletionItems);
    result["items"]        = std::move(items);
    return result;
}

json RawrXDLSPServer::handleTextDocumentDefinition(int /*id*/, const json& params) {
    std::string uri  = params["textDocument"]["uri"].get<std::string>();
    int line         = params["position"]["line"].get<int>();
    int character    = params["position"]["character"].get<int>();

    {
        std::lock_guard<std::mutex> lk(m_statsMutex);
        m_stats.definitionRequests++;
    }

    // Get word at cursor position from document content
    std::string word;
    {
        std::lock_guard<std::mutex> lk(m_docMutex);
        auto it = m_openDocuments.find(uri);
        if (it != m_openDocuments.end()) {
            std::istringstream iss(it->second.content);
            std::string curLine;
            for (int i = 0; i <= line && std::getline(iss, curLine); i++) {}
            // Extract identifier at character position
            int start = character, end = character;
            while (start > 0 && (std::isalnum(curLine[start - 1]) || curLine[start - 1] == '_')) start--;
            while (end < (int)curLine.size() && (std::isalnum(curLine[end]) || curLine[end] == '_')) end++;
            if (start < end) word = curLine.substr(start, end - start);
        }
    }

    if (word.empty()) return json::array();

    // Search symbol database for definitions
    json locations = json::array();
    std::shared_lock<std::shared_mutex> lock(m_symbolMutex);
    for (const auto& sym : m_symbols) {
        if (sym.name == word) {
            json loc;
            loc["uri"] = filePathToUri(sym.filePath);
            loc["range"]["start"]["line"]      = sym.line;
            loc["range"]["start"]["character"]  = sym.startChar;
            loc["range"]["end"]["line"]         = sym.line;
            loc["range"]["end"]["character"]    = sym.endChar;
            locations.push_back(std::move(loc));
        }
    }

    return locations;
}

json RawrXDLSPServer::handleTextDocumentReferences(int /*id*/, const json& params) {
    std::string uri  = params["textDocument"]["uri"].get<std::string>();
    int line         = params["position"]["line"].get<int>();
    int character    = params["position"]["character"].get<int>();

    {
        std::lock_guard<std::mutex> lk(m_statsMutex);
        m_stats.referenceRequests++;
    }

    // Get word at cursor
    std::string word;
    {
        std::lock_guard<std::mutex> lk(m_docMutex);
        auto it = m_openDocuments.find(uri);
        if (it != m_openDocuments.end()) {
            std::istringstream iss(it->second.content);
            std::string curLine;
            for (int i = 0; i <= line && std::getline(iss, curLine); i++) {}
            int start = character, end = character;
            while (start > 0 && (std::isalnum(curLine[start - 1]) || curLine[start - 1] == '_')) start--;
            while (end < (int)curLine.size() && (std::isalnum(curLine[end]) || curLine[end] == '_')) end++;
            if (start < end) word = curLine.substr(start, end - start);
        }
    }

    if (word.empty()) return json::array();

    // Find all occurrences of the symbol across all open documents + index
    json locations = json::array();

    // From symbol database (definitions/declarations)
    {
        std::shared_lock<std::shared_mutex> lock(m_symbolMutex);
        for (const auto& sym : m_symbols) {
            if (sym.name == word) {
                json loc;
                loc["uri"] = filePathToUri(sym.filePath);
                loc["range"]["start"]["line"]      = sym.line;
                loc["range"]["start"]["character"]  = sym.startChar;
                loc["range"]["end"]["line"]         = sym.line;
                loc["range"]["end"]["character"]    = sym.endChar;
                locations.push_back(std::move(loc));
            }
        }
    }

    // Also search open document contents for textual references
    {
        std::lock_guard<std::mutex> lk(m_docMutex);
        for (const auto& [docUri, docState] : m_openDocuments) {
            std::istringstream iss(docState.content);
            std::string ln;
            int lineNum = 0;
            while (std::getline(iss, ln)) {
                size_t pos = 0;
                while ((pos = ln.find(word, pos)) != std::string::npos) {
                    // Verify it's a word boundary
                    bool leftOk  = (pos == 0 || (!std::isalnum(ln[pos - 1]) && ln[pos - 1] != '_'));
                    bool rightOk = (pos + word.size() >= ln.size() ||
                                    (!std::isalnum(ln[pos + word.size()]) && ln[pos + word.size()] != '_'));
                    if (leftOk && rightOk) {
                        json loc;
                        loc["uri"] = docUri;
                        loc["range"]["start"]["line"]      = lineNum;
                        loc["range"]["start"]["character"]  = (int)pos;
                        loc["range"]["end"]["line"]         = lineNum;
                        loc["range"]["end"]["character"]    = (int)(pos + word.size());
                        locations.push_back(std::move(loc));
                    }
                    pos += word.size();
                }
                lineNum++;
            }
        }
    }

    return locations;
}

json RawrXDLSPServer::handleTextDocumentDocumentSymbol(int /*id*/, const json& params) {
    std::string uri      = params["textDocument"]["uri"].get<std::string>();
    std::string filePath = uriToFilePath(uri);

    {
        std::lock_guard<std::mutex> lk(m_statsMutex);
        m_stats.documentSymbolRequests++;
    }

    json symbols = json::array();
    std::shared_lock<std::shared_mutex> lock(m_symbolMutex);

    for (const auto& sym : m_symbols) {
        if (sym.filePath != filePath) continue;

        json s;
        s["name"]   = sym.name;
        s["kind"]   = kindToLSPInt(sym.kind);
        s["detail"] = sym.detail;
        s["range"]["start"]["line"]           = sym.line;
        s["range"]["start"]["character"]      = sym.startChar;
        s["range"]["end"]["line"]             = sym.line;
        s["range"]["end"]["character"]        = sym.endChar;
        s["selectionRange"]["start"]["line"]      = sym.line;
        s["selectionRange"]["start"]["character"] = sym.startChar;
        s["selectionRange"]["end"]["line"]        = sym.line;
        s["selectionRange"]["end"]["character"]   = sym.endChar;

        symbols.push_back(std::move(s));
    }

    return symbols;
}

json RawrXDLSPServer::handleWorkspaceSymbol(int /*id*/, const json& params) {
    std::string query = params.value("query", "");

    {
        std::lock_guard<std::mutex> lk(m_statsMutex);
        m_stats.workspaceSymbolRequests++;
    }

    std::string lowerQuery = toLower(query);
    json symbols = json::array();
    int count = 0;

    std::shared_lock<std::shared_mutex> lock(m_symbolMutex);
    for (const auto& sym : m_symbols) {
        if (count >= m_config.maxSymbolResults) break;

        if (!lowerQuery.empty()) {
            std::string lowerName = toLower(sym.name);
            if (lowerName.find(lowerQuery) == std::string::npos) continue;
        }

        json s;
        s["name"]          = sym.name;
        s["kind"]          = kindToLSPInt(sym.kind);
        s["containerName"] = sym.containerName;
        s["location"]["uri"] = filePathToUri(sym.filePath);
        s["location"]["range"]["start"]["line"]      = sym.line;
        s["location"]["range"]["start"]["character"]  = sym.startChar;
        s["location"]["range"]["end"]["line"]         = sym.line;
        s["location"]["range"]["end"]["character"]    = sym.endChar;

        symbols.push_back(std::move(s));
        count++;
    }

    return symbols;
}

json RawrXDLSPServer::handleTextDocumentSemanticTokensFull(int /*id*/, const json& params) {
    std::string uri = params["textDocument"]["uri"].get<std::string>();

    {
        std::lock_guard<std::mutex> lk(m_statsMutex);
        m_stats.semanticTokenRequests++;
    }

    // Get document content
    std::string content;
    {
        std::lock_guard<std::mutex> lk(m_docMutex);
        auto it = m_openDocuments.find(uri);
        if (it != m_openDocuments.end()) {
            content = it->second.content;
        }
    }

    if (content.empty()) {
        json result;
        result["data"] = json::array();
        return result;
    }

    std::vector<uint32_t> encoded = encodeSemanticTokens(content);

    json result;
    result["data"] = encoded;
    return result;
}

// ============================================================================
// LSP NOTIFICATION HANDLERS
// ============================================================================

void RawrXDLSPServer::handleInitialized(const json& /*params*/) {
    // Client has finished initialization — we can send server-initiated messages now
}

void RawrXDLSPServer::handleExit(const json& /*params*/) {
    m_exitRequested = true;
}

void RawrXDLSPServer::handleDidOpen(const json& params) {
    auto& td      = params["textDocument"];
    std::string uri        = td["uri"].get<std::string>();
    std::string languageId = td.value("languageId", "");
    int version            = td.value("version", 0);
    std::string text       = td.value("text", "");

    {
        std::lock_guard<std::mutex> lk(m_docMutex);
        auto& doc       = m_openDocuments[uri];
        doc.uri         = uri;
        doc.languageId  = languageId;
        doc.version     = version;
        doc.content     = text;
        doc.dirty       = false;
    }

    // Index the opened file
    std::string filePath = uriToFilePath(uri);
    indexFileContent(filePath, text);
}

void RawrXDLSPServer::handleDidChange(const json& params) {
    std::string uri = params["textDocument"]["uri"].get<std::string>();

    // We use textDocumentSync = 1 (Full), so contentChanges[0].text is the full content
    if (params.contains("contentChanges") && !params["contentChanges"].empty()) {
        std::string newText = params["contentChanges"][(size_t)0].value("text", "");
        {
            std::lock_guard<std::mutex> lk(m_docMutex);
            auto it = m_openDocuments.find(uri);
            if (it != m_openDocuments.end()) {
                it->second.content = newText;
                it->second.version = params["textDocument"].value("version", it->second.version + 1);
                it->second.dirty   = true;
            }
        }

        // Re-index (debounced in production; immediate here for correctness)
        std::string filePath = uriToFilePath(uri);
        indexFileContent(filePath, newText);
    }
}

void RawrXDLSPServer::handleDidClose(const json& params) {
    std::string uri = params["textDocument"]["uri"].get<std::string>();
    {
        std::lock_guard<std::mutex> lk(m_docMutex);
        m_openDocuments.erase(uri);
    }
}

void RawrXDLSPServer::handleDidSave(const json& params) {
    // On save: re-index + trigger diagnostic refresh
    std::string uri;
    if (params.contains("textDocument") && params["textDocument"].contains("uri")) {
        uri = params["textDocument"]["uri"].get<std::string>();
    }
    if (uri.empty()) return;

    std::string filePath = uriToFilePath(uri);
    std::string content;

    // If the save notification includes text (willSaveWaitUntil full-text mode),
    // use it directly; otherwise read from the open document cache or disk
    if (params.contains("text")) {
        content = params["text"].get<std::string>();
    } else {
        std::lock_guard<std::mutex> lk(m_docMutex);
        auto it = m_openDocuments.find(uri);
        if (it != m_openDocuments.end()) {
            content = it->second.content;
            it->second.dirty = false;  // Mark clean on save
        }
    }

    // Re-index the saved file to update symbol database
    if (!content.empty()) {
        indexFileContent(filePath, content);
    } else {
        // Fall back to reading from disk
        indexFileFromDisk(filePath);
    }

    // Generate basic diagnostics from the content
    // This catches common issues: unclosed brackets, very long lines, TODO markers
    std::vector<DiagnosticEntry> diagnostics;

    if (!content.empty()) {
        std::istringstream iss(content);
        std::string line;
        int lineNum = 0;
        int braceDepth = 0;
        int parenDepth = 0;

        while (std::getline(iss, line)) {
            // Line length warning
            if (line.size() > 200) {
                DiagnosticEntry d;
                d.range.start = { lineNum, 0 };
                d.range.end = { lineNum, static_cast<int>(line.size()) };
                d.severity = DiagnosticSeverity::Hint;
                d.code = "line-too-long";
                d.source = "rawrxd";
                d.message = "Line exceeds 200 characters (" + std::to_string(line.size()) + ")";
                diagnostics.push_back(std::move(d));
            }

            // TODO/FIXME/HACK markers
            for (const char* marker : {"TODO", "FIXME", "HACK", "XXX"}) {
                auto pos = line.find(marker);
                if (pos != std::string::npos) {
                    DiagnosticEntry d;
                    d.range.start = { lineNum, static_cast<int>(pos) };
                    d.range.end = { lineNum, static_cast<int>(pos + strlen(marker)) };
                    d.severity = DiagnosticSeverity::Information;
                    d.code = "task-marker";
                    d.source = "rawrxd";
                    d.message = std::string(marker) + " marker found";
                    diagnostics.push_back(std::move(d));
                }
            }

            // Track brace/paren depth (simple — not comment/string aware)
            for (char c : line) {
                if (c == '{') braceDepth++;
                else if (c == '}') braceDepth--;
                else if (c == '(') parenDepth++;
                else if (c == ')') parenDepth--;
            }
            lineNum++;
        }

        // Report unbalanced braces at end of file
        if (braceDepth != 0) {
            DiagnosticEntry d;
            d.range.start = { lineNum - 1, 0 };
            d.range.end = { lineNum - 1, 1 };
            d.severity = DiagnosticSeverity::Warning;
            d.code = "unbalanced-braces";
            d.source = "rawrxd";
            d.message = "Unbalanced braces: depth " + std::to_string(braceDepth) +
                        " at end of file";
            diagnostics.push_back(std::move(d));
        }
    }

    // Publish diagnostics (empty list clears previous diagnostics)
    publishDiagnostics(uri, diagnostics);
}

// ============================================================================
// DIAGNOSTICS PUBLISHING (server → client)
// ============================================================================

void RawrXDLSPServer::publishDiagnostics(const std::string& uri,
                                           const std::vector<DiagnosticEntry>& diagnostics) {
    json params;
    params["uri"] = uri;
    json diags = json::array();
    for (const auto& d : diagnostics) {
        json diag;
        diag["range"]["start"]["line"]      = d.range.start.line;
        diag["range"]["start"]["character"]  = d.range.start.character;
        diag["range"]["end"]["line"]         = d.range.end.line;
        diag["range"]["end"]["character"]    = d.range.end.character;
        diag["severity"] = static_cast<int>(d.severity);
        diag["code"]     = d.code;
        diag["source"]   = d.source.empty() ? "rawrxd" : d.source;
        diag["message"]  = d.message;
        diags.push_back(std::move(diag));
    }
    params["diagnostics"] = std::move(diags);
    writeNotification("textDocument/publishDiagnostics", params);
}

void RawrXDLSPServer::clearDiagnostics(const std::string& uri) {
    publishDiagnostics(uri, {});
}

// ============================================================================
// SYMBOL INDEXING ENGINE
// ============================================================================

void RawrXDLSPServer::rebuildIndex() {
    if (m_config.rootPath.empty() || !fs::exists(m_config.rootPath)) return;

    std::vector<IndexedSymbol> newSymbols;

    for (auto& entry : fs::recursive_directory_iterator(
             m_config.rootPath, fs::directory_options::skip_permission_denied)) {
        if (!entry.is_regular_file()) continue;
        std::string path = entry.path().string();
        if (!isCodeFile(path)) continue;

        // Index file from disk
        std::ifstream ifs(path);
        if (!ifs) continue;
        std::string content((std::istreambuf_iterator<char>(ifs)),
                             std::istreambuf_iterator<char>());

        // Parse symbols from content
        std::istringstream iss(content);
        std::string line;
        int lineNum = 0;

        // Regex patterns (matching context/indexer.cpp + extended patterns)
        static const std::regex re_func(
            R"((?:^|\s)(?:[\w:*&<>]+)\s+([A-Za-z_]\w*)\s*\([^;]*\)\s*\{)");
        static const std::regex re_class(
            R"((?:^|\s)class\s+([A-Za-z_]\w*)\s*)");
        static const std::regex re_struct(
            R"((?:^|\s)struct\s+([A-Za-z_]\w*)\s*)");
        static const std::regex re_enum(
            R"((?:^|\s)enum\s+(?:class\s+)?([A-Za-z_]\w*)\s*)");
        static const std::regex re_namespace(
            R"((?:^|\s)namespace\s+([A-Za-z_]\w*)\s*)");
        static const std::regex re_define(
            R"(^\s*#define\s+([A-Za-z_]\w*))");
        static const std::regex re_var(
            R"((?:^|\s)(?:int|float|double|bool|auto|size_t|uint\d+_t|int\d+_t|std::\w+)\s+([A-Za-z_]\w*)\s*(=|;))");

        while (std::getline(iss, line)) {
            std::smatch m;
            IndexedSymbol sym;
            sym.filePath = path;
            sym.line     = lineNum;

            if (std::regex_search(line, m, re_class)) {
                sym.name     = m[1].str();
                sym.kind     = SymbolKind::Class;
                sym.detail   = "class " + sym.name;
                sym.startChar = (int)m.position(1);
                sym.endChar   = sym.startChar + (int)sym.name.size();
            } else if (std::regex_search(line, m, re_struct)) {
                sym.name     = m[1].str();
                sym.kind     = SymbolKind::Struct;
                sym.detail   = "struct " + sym.name;
                sym.startChar = (int)m.position(1);
                sym.endChar   = sym.startChar + (int)sym.name.size();
            } else if (std::regex_search(line, m, re_enum)) {
                sym.name     = m[1].str();
                sym.kind     = SymbolKind::Enum;
                sym.detail   = "enum " + sym.name;
                sym.startChar = (int)m.position(1);
                sym.endChar   = sym.startChar + (int)sym.name.size();
            } else if (std::regex_search(line, m, re_namespace)) {
                sym.name     = m[1].str();
                sym.kind     = SymbolKind::Namespace;
                sym.detail   = "namespace " + sym.name;
                sym.startChar = (int)m.position(1);
                sym.endChar   = sym.startChar + (int)sym.name.size();
            } else if (std::regex_search(line, m, re_define)) {
                sym.name     = m[1].str();
                sym.kind     = SymbolKind::Constant;
                sym.detail   = "#define " + sym.name;
                sym.startChar = (int)m.position(1);
                sym.endChar   = sym.startChar + (int)sym.name.size();
            } else if (std::regex_search(line, m, re_func)) {
                sym.name     = m[1].str();
                sym.kind     = SymbolKind::Function;
                sym.detail   = line;  // Full line as detail
                // Trim leading/trailing whitespace from detail
                auto dStart = sym.detail.find_first_not_of(" \t");
                auto dEnd   = sym.detail.find_last_not_of(" \t\r\n{");
                if (dStart != std::string::npos && dEnd != std::string::npos) {
                    sym.detail = sym.detail.substr(dStart, dEnd - dStart + 1);
                }
                sym.startChar = (int)m.position(1);
                sym.endChar   = sym.startChar + (int)sym.name.size();
            } else if (std::regex_search(line, m, re_var)) {
                sym.name     = m[1].str();
                sym.kind     = SymbolKind::Variable;
                sym.detail   = line;
                auto dStart = sym.detail.find_first_not_of(" \t");
                if (dStart != std::string::npos) {
                    sym.detail = sym.detail.substr(dStart);
                }
                sym.startChar = (int)m.position(1);
                sym.endChar   = sym.startChar + (int)sym.name.size();
            } else {
                lineNum++;
                continue;
            }

            sym.hash = fnv1a(sym.name + "|" + sym.filePath + "|" + std::to_string(sym.line));
            newSymbols.push_back(std::move(sym));
            lineNum++;
        }
    }

    // Swap in the new index
    {
        std::unique_lock<std::shared_mutex> lock(m_symbolMutex);
        m_symbols = std::move(newSymbols);
    }

    {
        std::lock_guard<std::mutex> lk(m_statsMutex);
        m_stats.symbolsIndexed = m_symbols.size();
    }
}

void RawrXDLSPServer::indexSingleFile(const std::string& filePath) {
    indexFileFromDisk(filePath);
}

void RawrXDLSPServer::removeFileFromIndex(const std::string& filePath) {
    std::unique_lock<std::shared_mutex> lock(m_symbolMutex);
    m_symbols.erase(
        std::remove_if(m_symbols.begin(), m_symbols.end(),
                       [&](const IndexedSymbol& s) { return s.filePath == filePath; }),
        m_symbols.end());

    std::lock_guard<std::mutex> lk(m_statsMutex);
    m_stats.symbolsIndexed = m_symbols.size();
}

size_t RawrXDLSPServer::getIndexedSymbolCount() const {
    std::shared_lock<std::shared_mutex> lock(m_symbolMutex);
    return m_symbols.size();
}

size_t RawrXDLSPServer::getTrackedFileCount() const {
    std::lock_guard<std::mutex> lk(m_docMutex);
    return m_openDocuments.size();
}

void RawrXDLSPServer::indexFileContent(const std::string& filePath, const std::string& content) {
    // Remove old symbols for this file
    {
        std::unique_lock<std::shared_mutex> lock(m_symbolMutex);
        m_symbols.erase(
            std::remove_if(m_symbols.begin(), m_symbols.end(),
                           [&](const IndexedSymbol& s) { return s.filePath == filePath; }),
            m_symbols.end());
    }

    // Re-parse
    std::vector<IndexedSymbol> newSyms;
    std::istringstream iss(content);
    std::string line;
    int lineNum = 0;

    static const std::regex re_func(
        R"((?:^|\s)(?:[\w:*&<>]+)\s+([A-Za-z_]\w*)\s*\([^;]*\)\s*\{)");
    static const std::regex re_class(R"((?:^|\s)class\s+([A-Za-z_]\w*)\s*)");
    static const std::regex re_struct(R"((?:^|\s)struct\s+([A-Za-z_]\w*)\s*)");
    static const std::regex re_enum(R"((?:^|\s)enum\s+(?:class\s+)?([A-Za-z_]\w*)\s*)");
    static const std::regex re_define(R"(^\s*#define\s+([A-Za-z_]\w*))");

    while (std::getline(iss, line)) {
        std::smatch m;
        IndexedSymbol sym;
        sym.filePath = filePath;
        sym.line     = lineNum;

        if (std::regex_search(line, m, re_class)) {
            sym.name = m[1].str(); sym.kind = SymbolKind::Class;
            sym.detail = "class " + sym.name;
        } else if (std::regex_search(line, m, re_struct)) {
            sym.name = m[1].str(); sym.kind = SymbolKind::Struct;
            sym.detail = "struct " + sym.name;
        } else if (std::regex_search(line, m, re_enum)) {
            sym.name = m[1].str(); sym.kind = SymbolKind::Enum;
            sym.detail = "enum " + sym.name;
        } else if (std::regex_search(line, m, re_define)) {
            sym.name = m[1].str(); sym.kind = SymbolKind::Constant;
            sym.detail = "#define " + sym.name;
        } else if (std::regex_search(line, m, re_func)) {
            sym.name = m[1].str(); sym.kind = SymbolKind::Function;
            sym.detail = line;
            auto ds = sym.detail.find_first_not_of(" \t");
            auto de = sym.detail.find_last_not_of(" \t\r\n{");
            if (ds != std::string::npos && de != std::string::npos)
                sym.detail = sym.detail.substr(ds, de - ds + 1);
        } else {
            lineNum++;
            continue;
        }

        sym.startChar = (int)m.position(1);
        sym.endChar   = sym.startChar + (int)sym.name.size();
        sym.hash      = fnv1a(sym.name + "|" + sym.filePath + "|" + std::to_string(sym.line));
        newSyms.push_back(std::move(sym));
        lineNum++;
    }

    // Merge into global index
    {
        std::unique_lock<std::shared_mutex> lock(m_symbolMutex);
        m_symbols.insert(m_symbols.end(),
                         std::make_move_iterator(newSyms.begin()),
                         std::make_move_iterator(newSyms.end()));
    }

    {
        std::lock_guard<std::mutex> lk(m_statsMutex);
        m_stats.symbolsIndexed = m_symbols.size();
    }
}

void RawrXDLSPServer::indexFileFromDisk(const std::string& filePath) {
    std::ifstream ifs(filePath);
    if (!ifs) return;
    std::string content((std::istreambuf_iterator<char>(ifs)),
                         std::istreambuf_iterator<char>());
    indexFileContent(filePath, content);
}

// ============================================================================
// SEMANTIC TOKEN ENCODING
// ============================================================================

std::vector<uint32_t> RawrXDLSPServer::encodeSemanticTokens(const std::string& content) const {
    // LSP semantic tokens are encoded as relative (deltaLine, deltaStartChar, length, tokenType, modifiers)
    std::vector<uint32_t> data;

    // Collect all symbols for this content, sorted by line then startChar
    struct Token {
        int line;
        int startChar;
        int length;
        int tokenType;
        int modifiers;
    };
    std::vector<Token> tokens;

    // Use regex to find keywords, types, functions in the content
    std::istringstream iss(content);
    std::string line;
    int lineNum = 0;

    static const std::regex re_keyword(
        R"(\b(if|else|for|while|do|switch|case|break|continue|return|class|struct|enum|namespace|template|typename|typedef|using|const|static|virtual|override|inline|explicit|noexcept|constexpr|auto|void|int|float|double|bool|char|unsigned|signed|long|short|size_t|uint\d+_t|int\d+_t)\b)");
    static const std::regex re_preproc(R"(^\s*#\s*(include|define|ifdef|ifndef|endif|pragma|if|else|elif|undef)\b)");
    static const std::regex re_string(R"("(?:[^"\\]|\\.)*")");
    static const std::regex re_comment_line(R"(//.*$)");
    static const std::regex re_number(R"(\b\d+(?:\.\d+)?(?:[eE][+-]?\d+)?[fFlLuU]?\b)");

    while (std::getline(iss, line)) {
        // Comment tokens
        {
            std::sregex_iterator it(line.begin(), line.end(), re_comment_line);
            std::sregex_iterator end;
            for (; it != end; ++it) {
                tokens.push_back({lineNum, (int)it->position(), (int)it->length(),
                                  (int)SemanticTokenType::Comment, 0});
            }
        }
        // String tokens
        {
            std::sregex_iterator it(line.begin(), line.end(), re_string);
            std::sregex_iterator end;
            for (; it != end; ++it) {
                tokens.push_back({lineNum, (int)it->position(), (int)it->length(),
                                  (int)SemanticTokenType::String, 0});
            }
        }
        // Keyword tokens
        {
            std::sregex_iterator it(line.begin(), line.end(), re_keyword);
            std::sregex_iterator end;
            for (; it != end; ++it) {
                tokens.push_back({lineNum, (int)it->position(), (int)it->length(),
                                  (int)SemanticTokenType::Keyword, 0});
            }
        }
        // Preprocessor tokens
        {
            std::sregex_iterator it(line.begin(), line.end(), re_preproc);
            std::sregex_iterator end;
            for (; it != end; ++it) {
                tokens.push_back({lineNum, (int)it->position(), (int)it->length(),
                                  (int)SemanticTokenType::Macro, 0});
            }
        }
        // Number tokens
        {
            std::sregex_iterator it(line.begin(), line.end(), re_number);
            std::sregex_iterator end;
            for (; it != end; ++it) {
                tokens.push_back({lineNum, (int)it->position(), (int)it->length(),
                                  (int)SemanticTokenType::Number, 0});
            }
        }
        lineNum++;
    }

    // Sort by (line, startChar)
    std::sort(tokens.begin(), tokens.end(), [](const Token& a, const Token& b) {
        if (a.line != b.line) return a.line < b.line;
        return a.startChar < b.startChar;
    });

    // Encode as relative deltas
    int prevLine = 0;
    int prevStart = 0;
    for (const auto& tok : tokens) {
        int deltaLine  = tok.line - prevLine;
        int deltaStart = (deltaLine == 0) ? (tok.startChar - prevStart) : tok.startChar;
        data.push_back((uint32_t)deltaLine);
        data.push_back((uint32_t)deltaStart);
        data.push_back((uint32_t)tok.length);
        data.push_back((uint32_t)tok.tokenType);
        data.push_back((uint32_t)tok.modifiers);
        prevLine  = tok.line;
        prevStart = tok.startChar;
    }

    return data;
}

// ============================================================================
// HELPER METHODS
// ============================================================================

SymbolKind RawrXDLSPServer::kindFromString(const std::string& kindStr) const {
    if (kindStr == "class")     return SymbolKind::Class;
    if (kindStr == "struct")    return SymbolKind::Struct;
    if (kindStr == "function")  return SymbolKind::Function;
    if (kindStr == "method")    return SymbolKind::Method;
    if (kindStr == "variable")  return SymbolKind::Variable;
    if (kindStr == "enum")      return SymbolKind::Enum;
    if (kindStr == "namespace") return SymbolKind::Namespace;
    if (kindStr == "constant")  return SymbolKind::Constant;
    if (kindStr == "interface") return SymbolKind::Interface;
    return SymbolKind::Variable;
}

std::string RawrXDLSPServer::kindToString(SymbolKind kind) const {
    switch (kind) {
        case SymbolKind::File:          return "file";
        case SymbolKind::Module:        return "module";
        case SymbolKind::Namespace:     return "namespace";
        case SymbolKind::Class:         return "class";
        case SymbolKind::Method:        return "method";
        case SymbolKind::Function:      return "function";
        case SymbolKind::Constructor:   return "constructor";
        case SymbolKind::Enum:          return "enum";
        case SymbolKind::Interface:     return "interface";
        case SymbolKind::Variable:      return "variable";
        case SymbolKind::Constant:      return "constant";
        case SymbolKind::Struct:        return "struct";
        case SymbolKind::EnumMember:    return "enumMember";
        case SymbolKind::Property:      return "property";
        case SymbolKind::Field:         return "field";
        case SymbolKind::TypeParameter: return "typeParameter";
        default:                        return "unknown";
    }
}

int RawrXDLSPServer::kindToLSPInt(SymbolKind kind) const {
    return static_cast<int>(kind);  // SymbolKind values match LSP SymbolKind spec
}

std::string RawrXDLSPServer::filePathToUri(const std::string& path) const {
    std::string p = path;
    // Normalize backslashes to forward slashes
    std::replace(p.begin(), p.end(), '\\', '/');
    // Ensure starts with /
    if (!p.empty() && p[0] != '/') {
        p = "/" + p;
    }
    return "file://" + p;
}

std::string RawrXDLSPServer::uriToFilePath(const std::string& uri) const {
    std::string path = uri;
    // Strip file:// prefix
    if (path.rfind("file:///", 0) == 0) {
        path = path.substr(8);  // "file:///" → remove prefix, keep drive letter
    } else if (path.rfind("file://", 0) == 0) {
        path = path.substr(7);
    }
    // URL-decode %XX sequences
    std::string decoded;
    for (size_t i = 0; i < path.size(); i++) {
        if (path[i] == '%' && i + 2 < path.size()) {
            int hex = 0;
            std::from_chars(path.data() + i + 1, path.data() + i + 3, hex, 16);
            decoded += (char)hex;
            i += 2;
        } else {
            decoded += path[i];
        }
    }
    // Normalize to OS-native separators
#ifdef _WIN32
    std::replace(decoded.begin(), decoded.end(), '/', '\\');
#endif
    return decoded;
}

bool RawrXDLSPServer::isCodeFile(const std::string& path) const {
    static const char* exts[] = {
        ".cpp", ".c", ".hpp", ".h", ".cc", ".hh", ".cxx", ".hxx",
        ".py", ".ts", ".js", ".rs", ".go", ".java",
        ".asm", ".ini", ".json", ".yaml", ".yml", ".toml",
        ".md", ".txt", ".cmake"
    };
    fs::path p(path);
    auto e = p.extension().string();
    std::string lowerExt = toLower(e);
    for (auto* x : exts) {
        if (lowerExt == x) return true;
    }
    return false;
}

// ============================================================================
// THREAD ENTRY POINTS
// ============================================================================

void RawrXDLSPServer::readerThreadFunc() {
    while (!m_exitRequested) {
        std::string message;
        if (!readMessage(message)) {
            // EOF or read error — exit gracefully
            m_exitRequested = true;
            m_incomingCV.notify_all();
            break;
        }

        {
            std::lock_guard<std::mutex> lk(m_incomingMutex);
            m_incomingQueue.push_back(std::move(message));
        }
        m_incomingCV.notify_one();
    }
}

void RawrXDLSPServer::dispatchThreadFunc() {
    while (!m_exitRequested) {
        std::string message;
        {
            std::unique_lock<std::mutex> lk(m_incomingMutex);
            m_incomingCV.wait(lk, [&] {
                return !m_incomingQueue.empty() || m_exitRequested;
            });
            if (m_exitRequested && m_incomingQueue.empty()) break;
            if (!m_incomingQueue.empty()) {
                message = std::move(m_incomingQueue.front());
                m_incomingQueue.erase(m_incomingQueue.begin());
            }
        }

        if (!message.empty()) {
            dispatchMessage(message);
        }
    }
}

// ============================================================================
// CUSTOM HANDLER REGISTRATION
// ============================================================================

void RawrXDLSPServer::registerRequestHandler(const std::string& method, RequestHandler handler) {
    m_customRequestHandlers[method] = std::move(handler);
}

void RawrXDLSPServer::registerNotificationHandler(const std::string& method, NotificationHandler handler) {
    m_customNotificationHandlers[method] = std::move(handler);
}

void RawrXDLSPServer::unregisterRequestHandler(const std::string& method) {
    m_customRequestHandlers.erase(method);
}

void RawrXDLSPServer::unregisterNotificationHandler(const std::string& method) {
    m_customNotificationHandlers.erase(method);
}

// ============================================================================
// PUBLIC NOTIFICATION SENDING (for bridge/hotpatch layers)
// ============================================================================

void RawrXDLSPServer::sendNotification(const std::string& method, const json& params) {
    writeNotification(method, params);
}

// ============================================================================
// STATS
// ============================================================================

ServerStats RawrXDLSPServer::getStats() const {
    std::lock_guard<std::mutex> lk(m_statsMutex);
    return m_stats;
}

std::string RawrXDLSPServer::getStatsString() const {
    auto s = getStats();
    std::ostringstream oss;
    oss << "RawrXD LSP Server Stats\n"
        << "  State:           " << (int)m_state.load() << "\n"
        << "  Symbols indexed: " << s.symbolsIndexed << "\n"
        << "  Files tracked:   " << s.filesTracked << "\n"
        << "  Requests:        " << s.totalRequests << "\n"
        << "  Notifications:   " << s.totalNotifications << "\n"
        << "  Errors:          " << s.totalErrors << "\n"
        << "  Avg response:    " << s.avgResponseMs << " ms\n"
        << "  Hover:           " << s.hoverRequests << "\n"
        << "  Completion:      " << s.completionRequests << "\n"
        << "  Definition:      " << s.definitionRequests << "\n"
        << "  References:      " << s.referenceRequests << "\n"
        << "  DocSymbol:       " << s.documentSymbolRequests << "\n"
        << "  WkspSymbol:      " << s.workspaceSymbolRequests << "\n"
        << "  SemanticTokens:  " << s.semanticTokenRequests << "\n"
        << "  Bytes R/W:       " << s.bytesRead << " / " << s.bytesWritten << "\n";
    return oss.str();
}

} // namespace LSPServer
} // namespace RawrXD
