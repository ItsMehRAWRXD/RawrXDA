#include "lsp/lsp_wiring.h"
#include <sstream>
#include <iostream>
#include <windows.h>

namespace RawrXD::LSP {

// StdioTransport implementation
StdioTransport::StdioTransport() = default;

StdioTransport::~StdioTransport() {
    disconnect();
}

bool StdioTransport::connect(const std::string& command) {
    SECURITY_ATTRIBUTES sa = {sizeof(sa), nullptr, TRUE};

    HANDLE stdinRead, stdinWrite;
    HANDLE stdoutRead, stdoutWrite;

    CreatePipe(&stdinRead, &stdinWrite, &sa, 0);
    CreatePipe(&stdoutRead, &stdoutWrite, &sa, 0);

    SetHandleInformation(stdinWrite, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stdoutRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {sizeof(si)};
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = stdinRead;
    si.hStdOutput = stdoutWrite;
    si.hStdError = stdoutWrite;

    PROCESS_INFORMATION pi = {};

    std::string cmd = "cmd /c " + command;
    BOOL success = CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, TRUE,
                                  CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);

    CloseHandle(stdinRead);
    CloseHandle(stdoutWrite);

    if (!success) {
        CloseHandle(stdinWrite);
        CloseHandle(stdoutRead);
        return false;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    m_stdinWrite = stdinWrite;
    m_stdoutRead = stdoutRead;
    m_connected = true;

    m_readThread = std::thread(&StdioTransport::readLoop, this);
    return true;
}

void StdioTransport::disconnect() {
    m_connected = false;
    if (m_readThread.joinable()) {
        m_readThread.join();
    }
    if (m_stdinWrite) {
        CloseHandle(m_stdinWrite);
        m_stdinWrite = nullptr;
    }
    if (m_stdoutRead) {
        CloseHandle(m_stdoutRead);
        m_stdoutRead = nullptr;
    }
}

bool StdioTransport::isConnected() const {
    return m_connected;
}

bool StdioTransport::send(const std::string& message) {
    if (!m_connected || !m_stdinWrite) return false;

    std::string header = "Content-Length: " + std::to_string(message.size()) + "\r\n\r\n";
    std::string full = header + message;

    DWORD written;
    return WriteFile(m_stdinWrite, full.c_str(), static_cast<DWORD>(full.size()), &written, nullptr);
}

std::optional<std::string> StdioTransport::receive() {
    if (!m_connected || !m_stdoutRead) return std::nullopt;

    // Read header
    std::string header;
    char c;
    DWORD read;
    while (ReadFile(m_stdoutRead, &c, 1, &read, nullptr) && read > 0) {
        header += c;
        if (header.size() >= 4 &&
            header.substr(header.size() - 4) == "\r\n\r\n") {
            break;
        }
    }

    // Parse Content-Length
    size_t contentLength = 0;
    auto pos = header.find("Content-Length: ");
    if (pos != std::string::npos) {
        contentLength = std::stoull(header.substr(pos + 16));
    }

    // Read body
    std::string body;
    body.resize(contentLength);
    DWORD totalRead = 0;
    while (totalRead < contentLength) {
        DWORD bytesRead;
        if (!ReadFile(m_stdoutRead, body.data() + totalRead,
                      static_cast<DWORD>(contentLength - totalRead), &bytesRead, nullptr)) {
            break;
        }
        totalRead += bytesRead;
    }

    return body;
}

void StdioTransport::setReceiveCallback(std::function<void(const std::string&)> callback) {
    m_callback = callback;
}

void StdioTransport::readLoop() {
    while (m_connected) {
        auto msg = receive();
        if (msg && m_callback) {
            m_callback(*msg);
        }
    }
}

// LSPWiring implementation
LSPWiring::LSPWiring() = default;

LSPWiring::~LSPWiring() {
    disconnect();
}

bool LSPWiring::connectStdio(const std::string& serverCommand) {
    auto transport = std::make_unique<StdioTransport>();
    if (!transport->connect(serverCommand)) {
        return false;
    }

    m_transport = std::move(transport);
    m_transport->setReceiveCallback([this](const std::string& msg) {
        processMessage(msg);
    });

    m_state = LSPState::Connecting;
    m_running = true;
    m_receiveThread = std::thread(&LSPWiring::receiveLoop, this);

    return true;
}

bool LSPWiring::connectSocket(const std::string& host, int port) {
    // TODO: Implement socket transport
    return false;
}

bool LSPWiring::connectPipe(const std::string& pipeName) {
    // TODO: Implement named pipe transport
    return false;
}

void LSPWiring::disconnect() {
    m_running = false;
    if (m_receiveThread.joinable()) {
        m_receiveThread.join();
    }
    if (m_transport) {
        m_transport->disconnect();
        m_transport.reset();
    }
    m_state = LSPState::Disconnected;
}

bool LSPWiring::isConnected() const {
    return m_transport && m_transport->isConnected();
}

LSPState LSPWiring::getState() const {
    return m_state;
}

bool LSPWiring::initialize(const std::string& rootUri,
                           const std::vector<WorkspaceFolder>& workspaces) {
    if (!isConnected()) return false;

    m_state = LSPState::Initializing;

    json params;
    params["processId"] = GetCurrentProcessId();
    params["rootUri"] = rootUri;
    params["capabilities"] = {
        {"textDocument", {
            {"synchronization", {
                {"dynamicRegistration", false},
                {"willSave", true},
                {"willSaveWaitUntil", true},
                {"didSave", true}
            }},
            {"completion", {
                {"dynamicRegistration", false},
                {"completionItem", {
                    {"snippetSupport", true},
                    {"commitCharactersSupport", true},
                    {"documentationFormat", {"markdown", "plaintext"}},
                    {"deprecatedSupport", true},
                    {"preselectSupport", true}
                }}
            }},
            {"hover", {
                {"dynamicRegistration", false},
                {"contentFormat", {"markdown", "plaintext"}}
            }},
            {"definition", {
                {"dynamicRegistration", false},
                {"linkSupport", true}
            }},
            {"documentSymbol", {
                {"dynamicRegistration", false},
                {"hierarchicalDocumentSymbolSupport", true}
            }},
            {"codeAction", {
                {"dynamicRegistration", false},
                {"codeActionLiteralSupport", {
                    {"codeActionKind", {
                        {"valueSet", {"", "quickfix", "refactor", "source"}}
                    }}
                }}
            }},
            {"formatting", {"dynamicRegistration", false}},
            {"rename", {"dynamicRegistration", false}},
            {"foldingRange", {"dynamicRegistration", false}}
        }},
        {"workspace", {
            {"applyEdit", true},
            {"workspaceEdit", {"documentChanges", true}},
            {"didChangeConfiguration", {"dynamicRegistration", false}},
            {"didChangeWatchedFiles", {"dynamicRegistration", false}},
            {"workspaceFolders", true},
            {"configuration", true}
        }}
    };

    if (!workspaces.empty()) {
        params["workspaceFolders"] = json::array();
        for (const auto& ws : workspaces) {
            params["workspaceFolders"].push_back({
                {"uri", ws.uri},
                {"name", ws.name}
            });
        }
    }

    auto result = sendRequest("initialize", params);
    if (result) {
        updateCapabilities((*result)["capabilities"]);
        m_state = LSPState::Initialized;

        // Send initialized notification
        sendNotification("initialized", json::object());
        return true;
    }

    m_state = LSPState::Error;
    return false;
}

bool LSPWiring::shutdown() {
    if (m_state != LSPState::Initialized) return false;

    auto result = sendRequest("shutdown", json::object());
    if (result) {
        m_state = LSPState::Shutdown;
        return true;
    }
    return false;
}

bool LSPWiring::exit() {
    sendNotification("exit", json::object());
    disconnect();
    return true;
}

std::optional<json> LSPWiring::sendRequest(const std::string& method,
                                            const json& params,
                                            int timeoutMs) {
    if (!m_transport) return std::nullopt;

    int id = m_nextId++;

    LSPMessage msg;
    msg.id = id;
    msg.method = method;
    msg.params = params;

    std::promise<json> promise;
    auto future = promise.get_future();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingRequests[id] = std::move(promise);
    }

    std::string serialized = serializeMessage(msg);
    if (!m_transport->send(serialized)) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingRequests.erase(id);
        return std::nullopt;
    }

    auto status = future.wait_for(std::chrono::milliseconds(timeoutMs));
    if (status == std::future_status::timeout) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingRequests.erase(id);
        return std::nullopt;
    }

    try {
        return future.get();
    } catch (...) {
        return std::nullopt;
    }
}

void LSPWiring::sendNotification(const std::string& method, const json& params) {
    if (!m_transport) return;

    LSPMessage msg;
    msg.method = method;
    msg.params = params;

    std::string serialized = serializeMessage(msg);
    m_transport->send(serialized);
}

void LSPWiring::sendResponse(int id, const json& result) {
    if (!m_transport) return;

    LSPMessage msg;
    msg.id = id;
    msg.result = result;

    std::string serialized = serializeMessage(msg);
    m_transport->send(serialized);
}

void LSPWiring::sendError(int id, int code, const std::string& message) {
    if (!m_transport) return;

    LSPMessage msg;
    msg.id = id;
    msg.error = json{
        {"code", code},
        {"message", message}
    };

    std::string serialized = serializeMessage(msg);
    m_transport->send(serialized);
}

ServerCapabilities LSPWiring::getServerCapabilities() const {
    return m_capabilities;
}

bool LSPWiring::supportsMethod(const std::string& method) const {
    if (method == "textDocument/completion") return m_capabilities.completionProvider;
    if (method == "textDocument/hover") return m_capabilities.hoverProvider;
    if (method == "textDocument/definition") return m_capabilities.definitionProvider;
    if (method == "textDocument/references") return m_capabilities.referencesProvider;
    if (method == "textDocument/documentSymbol") return m_capabilities.documentSymbolProvider;
    if (method == "textDocument/formatting") return m_capabilities.documentFormattingProvider;
    if (method == "textDocument/rename") return m_capabilities.renameProvider;
    if (method == "textDocument/foldingRange") return m_capabilities.foldingRangeProvider;
    if (method == "workspace/symbol") return m_capabilities.workspaceSymbolProvider;
    return false;
}

void LSPWiring::onNotification(const std::string& method, NotificationHandler handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_notificationHandlers[method] = handler;
}

void LSPWiring::onRequest(const std::string& method, RequestHandler handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_requestHandlers[method] = handler;
}

void LSPWiring::onProgress(ProgressCallback callback) {
    m_progressCallback = callback;
}

void LSPWiring::onDiagnostics(DiagnosticCallback callback) {
    m_diagnosticCallback = callback;
}

void LSPWiring::syncDocument(const std::string& uri,
                            const std::string& text,
                            int version) {
    json params;
    params["textDocument"] = {
        {"uri", uri},
        {"languageId", "cpp"},
        {"version", version},
        {"text", text}
    };
    sendNotification("textDocument/didOpen", params);
}

void LSPWiring::notifyDocumentChange(const std::string& uri,
                                      const std::vector<json>& contentChanges,
                                      int version) {
    json params;
    params["textDocument"] = {
        {"uri", uri},
        {"version", version}
    };
    params["contentChanges"] = contentChanges;
    sendNotification("textDocument/didChange", params);
}

void LSPWiring::receiveLoop() {
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void LSPWiring::processMessage(const std::string& raw) {
    try {
        auto msg = parseMessage(raw);
        handleMessage(msg);
    } catch (...) {
        // Log parse error
    }
}

LSPMessage LSPWiring::parseMessage(const std::string& raw) {
    LSPMessage msg;
    json j = json::parse(raw);

    msg.jsonrpc = j.value("jsonrpc", "2.0");

    if (j.contains("id")) {
        msg.id = j["id"].get<int>();
    }

    if (j.contains("method")) {
        msg.method = j["method"].get<std::string>();
    }

    if (j.contains("params")) {
        msg.params = j["params"];
    }

    if (j.contains("result")) {
        msg.result = j["result"];
    }

    if (j.contains("error")) {
        msg.error = j["error"];
    }

    return msg;
}

std::string LSPWiring::serializeMessage(const LSPMessage& msg) {
    json j;
    j["jsonrpc"] = msg.jsonrpc;

    if (msg.id.has_value()) {
        j["id"] = *msg.id;
    }

    if (msg.method.has_value()) {
        j["method"] = *msg.method;
    }

    if (msg.params.has_value()) {
        j["params"] = *msg.params;
    }

    if (msg.result.has_value()) {
        j["result"] = *msg.result;
    }

    if (msg.error.has_value()) {
        j["error"] = *msg.error;
    }

    std::string content = j.dump();
    return "Content-Length: " + std::to_string(content.size()) + "\r\n\r\n" + content;
}

void LSPWiring::handleMessage(const LSPMessage& msg) {
    if (msg.isResponse()) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_pendingRequests.find(*msg.id);
        if (it != m_pendingRequests.end()) {
            if (msg.result.has_value()) {
                it->second.set_value(*msg.result);
            } else if (msg.error.has_value()) {
                it->second.set_exception(std::make_exception_ptr(
                    std::runtime_error(msg.error->value("message", "Unknown error"))));
            }
            m_pendingRequests.erase(it);
        }
    } else if (msg.isNotification()) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_notificationHandlers.find(*msg.method);
        if (it != m_notificationHandlers.end()) {
            it->second(*msg.method, msg.params.value_or(json::object()));
        }

        // Handle standard notifications
        if (*msg.method == "textDocument/publishDiagnostics" && m_diagnosticCallback) {
            auto params = msg.params.value_or(json::object());
            m_diagnosticCallback(params.value("uri", ""), params.value("diagnostics", json::array()));
        }

        if (*msg.method == "$/progress" && m_progressCallback) {
            auto params = msg.params.value_or(json::object());
            m_progressCallback(params.value("token", ""), params.value("value", json::object()));
        }
    } else if (msg.isRequest()) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_requestHandlers.find(*msg.method);
        if (it != m_requestHandlers.end()) {
            auto result = it->second(*msg.method, msg.params.value_or(json::object()));
            sendResponse(*msg.id, result);
        }
    }
}

void LSPWiring::updateCapabilities(const json& caps) {
    m_capabilities.textDocumentSync = caps.value("textDocumentSync", false);
    m_capabilities.hoverProvider = caps.value("hoverProvider", false);
    m_capabilities.completionProvider = caps.contains("completionProvider");
    m_capabilities.signatureHelpProvider = caps.contains("signatureHelpProvider");
    m_capabilities.definitionProvider = caps.value("definitionProvider", false);
    m_capabilities.referencesProvider = caps.value("referencesProvider", false);
    m_capabilities.documentHighlightProvider = caps.value("documentHighlightProvider", false);
    m_capabilities.documentSymbolProvider = caps.value("documentSymbolProvider", false);
    m_capabilities.codeActionProvider = caps.value("codeActionProvider", false);
    m_capabilities.codeLensProvider = caps.contains("codeLensProvider");
    m_capabilities.documentFormattingProvider = caps.value("documentFormattingProvider", false);
    m_capabilities.documentRangeFormattingProvider = caps.value("documentRangeFormattingProvider", false);
    m_capabilities.documentOnTypeFormattingProvider = caps.contains("documentOnTypeFormattingProvider");
    m_capabilities.renameProvider = caps.value("renameProvider", false);
    m_capabilities.foldingRangeProvider = caps.value("foldingRangeProvider", false);
    m_capabilities.executeCommandProvider = caps.contains("executeCommandProvider");
    m_capabilities.selectionRangeProvider = caps.value("selectionRangeProvider", false);
    m_capabilities.semanticTokensProvider = caps.contains("semanticTokensProvider");
    m_capabilities.inlayHintProvider = caps.value("inlayHintProvider", false);
    m_capabilities.workspaceSymbolProvider = caps.value("workspaceSymbolProvider", false);
}

// LSPWiringManager
LSPWiringManager& LSPWiringManager::instance() {
    static LSPWiringManager mgr;
    return mgr;
}

LSPWiring* LSPWiringManager::createConnection(const std::string& languageId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connections[languageId] = std::make_unique<LSPWiring>();
    return m_connections[languageId].get();
}

void LSPWiringManager::destroyConnection(const std::string& languageId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connections.erase(languageId);
}

LSPWiring* LSPWiringManager::getConnection(const std::string& languageId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_connections.find(languageId);
    return (it != m_connections.end()) ? it->second.get() : nullptr;
}

std::vector<std::string> LSPWiringManager::getActiveConnections() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> result;
    for (const auto& [lang, _] : m_connections) {
        result.push_back(lang);
    }
    return result;
}

void LSPWiringManager::registerLanguageServer(const std::string& languageId,
                                               const std::string& command,
                                               const std::vector<std::string>& args) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_serverConfigs[languageId] = {command, args};
}

void LSPWiringManager::unregisterLanguageServer(const std::string& languageId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_serverConfigs.erase(languageId);
}

// Utility functions
std::string lspStateToString(LSPState state) {
    switch (state) {
        case LSPState::Disconnected: return "disconnected";
        case LSPState::Connecting: return "connecting";
        case LSPState::Initializing: return "initializing";
        case LSPState::Initialized: return "initialized";
        case LSPState::Shutdown: return "shutdown";
        case LSPState::Error: return "error";
        default: return "unknown";
    }
}

} // namespace RawrXD::LSP
