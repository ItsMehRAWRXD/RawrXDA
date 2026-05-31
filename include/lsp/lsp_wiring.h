#pragma once
/**
 * @file lsp_wiring.h
 * @brief LSP client-server wiring and connection management
 * Batch 3 - Item 31: LSP wiring stubs
 */

#include <string>
#include <memory>
#include <functional>
#include <map>
#include <vector>
#include <nlohmann/json.hpp>

namespace RawrXD::LSP {

using json = nlohmann::json;

enum class LSPState {
    Disconnected,
    Connecting,
    Initializing,
    Initialized,
    Shutdown,
    Error
};

enum class MessageType {
    Request,
    Response,
    Notification
};

struct LSPMessage {
    std::string jsonrpc = "2.0";
    std::optional<int> id;
    std::optional<std::string> method;
    std::optional<json> params;
    std::optional<json> result;
    std::optional<json> error;

    bool isRequest() const { return method.has_value() && !id.has_value(); }
    bool isResponse() const { return id.has_value() && (result.has_value() || error.has_value()); }
    bool isNotification() const { return method.has_value() && !id.has_value(); }
};

struct ServerCapabilities {
    bool textDocumentSync = false;
    bool hoverProvider = false;
    bool completionProvider = false;
    bool signatureHelpProvider = false;
    bool definitionProvider = false;
    bool referencesProvider = false;
    bool documentHighlightProvider = false;
    bool documentSymbolProvider = false;
    bool codeActionProvider = false;
    bool codeLensProvider = false;
    bool documentFormattingProvider = false;
    bool documentRangeFormattingProvider = false;
    bool documentOnTypeFormattingProvider = false;
    bool renameProvider = false;
    bool foldingRangeProvider = false;
    bool executeCommandProvider = false;
    bool selectionRangeProvider = false;
    bool semanticTokensProvider = false;
    bool inlayHintProvider = false;
    bool workspaceSymbolProvider = false;
    bool callHierarchyProvider = false;
    bool typeHierarchyProvider = false;
    bool inlineValueProvider = false;
    bool monikerProvider = false;
    bool linkedEditingRangeProvider = false;
};

struct WorkspaceFolder {
    std::string uri;
    std::string name;
};

class LSPTransport {
public:
    virtual ~LSPTransport() = default;
    virtual bool connect(const std::string& endpoint) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual bool send(const std::string& message) = 0;
    virtual std::optional<std::string> receive() = 0;
    virtual void setReceiveCallback(std::function<void(const std::string&)> callback) = 0;
};

class StdioTransport : public LSPTransport {
public:
    StdioTransport();
    ~StdioTransport() override;
    bool connect(const std::string& command) override;
    void disconnect() override;
    bool isConnected() const override;
    bool send(const std::string& message) override;
    std::optional<std::string> receive() override;
    void setReceiveCallback(std::function<void(const std::string&)> callback) override;

private:
    void* m_process{nullptr};
    void* m_stdinWrite{nullptr};
    void* m_stdoutRead{nullptr};
    std::thread m_readThread;
    std::atomic<bool> m_connected{false};
    std::function<void(const std::string&)> m_callback;

    void readLoop();
};

class SocketTransport : public LSPTransport {
public:
    SocketTransport();
    ~SocketTransport() override;
    bool connect(const std::string& address) override;
    void disconnect() override;
    bool isConnected() const override;
    bool send(const std::string& message) override;
    std::optional<std::string> receive() override;
    void setReceiveCallback(std::function<void(const std::string&)> callback) override;

private:
    int m_socket{-1};
    std::atomic<bool> m_connected{false};
    std::thread m_readThread;
    std::function<void(const std::string&)> m_callback;

    void readLoop();
};

class LSPWiring {
public:
    LSPWiring();
    ~LSPWiring();

    // Connection management
    bool connectStdio(const std::string& serverCommand);
    bool connectSocket(const std::string& host, int port);
    bool connectPipe(const std::string& pipeName);
    void disconnect();
    bool isConnected() const;
    LSPState getState() const;

    // Lifecycle
    bool initialize(const std::string& rootUri,
                    const std::vector<WorkspaceFolder>& workspaces = {});
    bool shutdown();
    bool exit();

    // Message handling
    std::optional<json> sendRequest(const std::string& method,
                                      const json& params,
                                      int timeoutMs = 30000);
    void sendNotification(const std::string& method, const json& params);
    void sendResponse(int id, const json& result);
    void sendError(int id, int code, const std::string& message);

    // Capabilities
    ServerCapabilities getServerCapabilities() const;
    bool supportsMethod(const std::string& method) const;

    // Registration
    void registerCapability(const std::string& id,
                            const std::string& method,
                            const json& registerOptions);
    void unregisterCapability(const std::string& id);

    // Event handlers
    using NotificationHandler = std::function<void(const std::string& method, const json& params)>;
    using RequestHandler = std::function<json(const std::string& method, const json& params)>;
    void onNotification(const std::string& method, NotificationHandler handler);
    void onRequest(const std::string& method, RequestHandler handler);

    // Progress
    using ProgressCallback = std::function<void(const std::string& token, const json& value)>;
    void onProgress(ProgressCallback callback);
    void createProgress(const std::string& token, const std::string& title);
    void notifyProgress(const std::string& token, const json& value);

    // Diagnostics
    using DiagnosticCallback = std::function<void(const std::string& uri, const json& diagnostics)>;
    void onDiagnostics(DiagnosticCallback callback);

    // Workspace
    void didChangeWorkspaceFolders(const std::vector<WorkspaceFolder>& added,
                                   const std::vector<WorkspaceFolder>& removed);
    void didChangeConfiguration(const json& settings);
    void didChangeWatchedFiles(const std::vector<std::string>& changes);

    // Synchronization
    void syncDocument(const std::string& uri, const std::string& text, int version);
    void notifyDocumentChange(const std::string& uri,
                            const std::vector<json>& contentChanges,
                            int version);

    // Status
    std::string getServerName() const;
    std::string getServerVersion() const;
    std::vector<std::string> getTriggerCharacters() const;

private:
    std::unique_ptr<LSPTransport> m_transport;
    LSPState m_state{LSPState::Disconnected};
    ServerCapabilities m_capabilities;
    std::atomic<int> m_nextId{1};

    std::map<int, std::promise<json>> m_pendingRequests;
    std::map<std::string, NotificationHandler> m_notificationHandlers;
    std::map<std::string, RequestHandler> m_requestHandlers;
    std::map<std::string, json> m_registeredCapabilities;

    ProgressCallback m_progressCallback;
    DiagnosticCallback m_diagnosticCallback;

    mutable std::mutex m_mutex;
    std::thread m_receiveThread;
    std::atomic<bool> m_running{false};

    void receiveLoop();
    void handleMessage(const LSPMessage& msg);
    void processMessage(const std::string& raw);
    LSPMessage parseMessage(const std::string& raw);
    std::string serializeMessage(const LSPMessage& msg);
    void updateCapabilities(const json& caps);
};

// Global wiring manager
class LSPWiringManager {
public:
    static LSPWiringManager& instance();

    LSPWiring* createConnection(const std::string& languageId);
    void destroyConnection(const std::string& languageId);
    LSPWiring* getConnection(const std::string& languageId);
    std::vector<std::string> getActiveConnections() const;

    void registerLanguageServer(const std::string& languageId,
                                 const std::string& command,
                                 const std::vector<std::string>& args);
    void unregisterLanguageServer(const std::string& languageId);

private:
    LSPWiringManager() = default;
    std::map<std::string, std::unique_ptr<LSPWiring>> m_connections;
    std::map<std::string, std::pair<std::string, std::vector<std::string>>> m_serverConfigs;
    mutable std::mutex m_mutex;
};

// Utility functions
std::string lspStateToString(LSPState state);
std::string messageTypeToString(MessageType type);

} // namespace RawrXD::LSP
