#pragma once
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <future>
#include "nlohmann/json.hpp"

namespace RawrXD {

struct LSPConfig {
    std::string languageId;
    std::string command;
    std::vector<std::string> args;
    std::string rootPath;
};

// Abstract transport interface
class JsonRpcTransport {
public:
    virtual ~JsonRpcTransport() = default;
    virtual bool connect(const std::string& cmd, const std::vector<std::string>& args) = 0;
    virtual void send(const nlohmann::json& msg) = 0;
    virtual nlohmann::json receive() = 0;
    virtual bool isConnected() const = 0;
};

class LSPClient {
public:
    LSPClient(const LSPConfig& config);
    ~LSPClient();
    
    bool start();
    void stop();
    
    // Core LSP methods
    std::future<nlohmann::json> initialize();
    void didOpen(const std::string& uri, const std::string& text);
    void didChange(const std::string& uri, const std::string& text);
    std::future<nlohmann::json> completion(const std::string& uri, int line, int character);
    std::future<nlohmann::json> definition(const std::string& uri, int line, int character);
    
private:
    LSPConfig m_config;
    std::unique_ptr<JsonRpcTransport> m_transport;
    std::atomic<bool> m_initialized{false};
    std::atomic<int> m_requestId{0};
    std::mutex m_mutex;
    
    nlohmann::json createRequest(const std::string& method, const nlohmann::json& params);
    nlohmann::json createNotification(const std::string& method, const nlohmann::json& params);
};

} // namespace RawrXD

#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include "nlohmann/json.hpp"

namespace RawrXD {

/**
 * \brief LSP server configuration
 */
struct LSPServerConfig {
    std::string language;           // "cpp", "python", "javascript", etc.
    std::string command;            // "clangd", "pylsp", "typescript-language-server"
    std::vector<std::string> arguments;      // Server-specific args
    std::string workspaceRoot;      // Workspace root directory
    bool autoStart = true;      // Auto-start on initialization
};

/**
 * \brief Completion item from LSP server
 */
struct CompletionItem {
    std::string label;              // Display label
    std::string insertText;         // Text to insert
    std::string detail;             // Additional details
    std::string documentation;      // Documentation string
    int kind = 1;              // CompletionItemKind (1=Text, 2=Method, 3=Function, etc.)
    std::string sortText;          // Sort order
    std::string filterText;        // Filter text
    int score = 0;             // Relevance score (computed)
};

/**
 * \brief LSP diagnostic message
 */
struct Diagnostic {
    int line;                  // Line number (0-indexed)
    int column;                // Column number (0-indexed)
    int severity;              // 1=Error, 2=Warning, 3=Info, 4=Hint
    std::string message;           // Diagnostic message
    std::string source;            // Source (e.g., "clangd")
};

/**
 * \brief LSP client for language server integration
 */
class LSPClient
{

public:
    explicit LSPClient(const LSPServerConfig& config);
    ~LSPClient();

    void initialize();
    bool startServer();
    void stopServer();
    bool isRunning() const { return m_serverRunning; }

    void openDocument(const std::string& uri, const std::string& languageId, const std::string& text);
    void closeDocument(const std::string& uri);
    void updateDocument(const std::string& uri, const std::string& text, int version);
    void requestCompletions(const std::string& uri, int line, int character);
    void requestHover(const std::string& uri, int line, int character);
    void requestDefinition(const std::string& uri, int line, int character);
    void formatDocument(const std::string& uri);
    std::vector<Diagnostic> getDiagnostics(const std::string& uri) const;

    void shutdown();

    // Callbacks / Signals (Stubs for now, can be std::function callbacks in real impl)
    void serverReady();
    void completionsReceived(const std::string& uri, int line, int character, const std::vector<CompletionItem>& items);
    void hoverReceived(const std::string& uri, const std::string& markdown);
    void definitionReceived(const std::string& uri, int line, int character);
    void diagnosticsUpdated(const std::string& uri, const std::vector<Diagnostic>& diagnostics);
    void formatEditsReceived(const std::string& uri, const std::string& formattedText);
    void serverError(const std::string& error);

private:
    void onServerReadyRead();
    void onServerError(int error);
    void onServerFinished(int exitCode, int status);

    // Internal Process Management
    void createChildProcess();
    void writeToChild(const std::string& message);
    void readFromChild();

    void sendMessage(const nlohmann::json& message);
    void processMessage(const nlohmann::json& message);
    void handleInitializeResponse(const nlohmann::json& result);
    void handleCompletionResponse(const nlohmann::json& result, int requestId);
    void handleHoverResponse(const nlohmann::json& result, int requestId);
    void handleDefinitionResponse(const nlohmann::json& result, int requestId);
    void handleDiagnostics(const nlohmann::json& params);
    void handleFormattingResponse(const nlohmann::json& result, int requestId);
    
    std::string buildDocumentUri(const std::string& filePath) const;
    
    LSPServerConfig m_config;
    
    // Win32 Process Handles (void* to avoid windows.h in header or polluting namespace)
    void* m_hProcess = nullptr; 
    void* m_hThread = nullptr;
    void* m_hStdInWrite = nullptr; // Pipe write end
    void* m_hStdOutRead = nullptr; // Pipe read end
    void* m_hStdErrRead = nullptr; // Pipe read end
    
    std::thread m_readThread;
    std::atomic<bool> m_stopThread{false};
    std::mutex m_transportMutex; // Protect write access
    
    bool m_serverRunning = false;
    bool m_initialized = false;
    int m_nextRequestId = 1;
    
    std::string m_receiveBuffer;
    std::map<std::string, int> m_documentVersions;  // uri -> version
    std::map<std::string, std::vector<Diagnostic>> m_diagnostics;  // uri -> diagnostics
    
    // Request tracking
    struct PendingRequest {
        std::string type;  // "completion", "hover", "definition", etc.
        std::string uri;
        int line;
        int character;
    };
    std::map<int, PendingRequest> m_pendingRequests;
};

} // namespace RawrXD
