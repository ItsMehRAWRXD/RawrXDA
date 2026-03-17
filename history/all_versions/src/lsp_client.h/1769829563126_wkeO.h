/**
 * \file lsp_client.h
 * \brief Language Server Protocol client with streaming completions
 * \author RawrXD Team
 * \date 2025-12-07
 */

#pragma once


#include <functional>

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
 * 
 * Features:
 * - Multi-language support (C++, Python, TypeScript, etc.)
 * - Real-time completions with streaming
 * - Inline ghost-text preview
 * - Diagnostics (errors/warnings)
 * - Go-to-definition, hover, formatting
 * - Incremental document sync
 * - Multi-file context awareness
 */
class LSPClient
{

public:
    explicit LSPClient(const LSPServerConfig& config, void* parent = nullptr);
    ~LSPClient();

    /**
     * Two-phase initialization - call after void ready
     */
    void initialize();

    /**
     * Start the LSP server process
     */
    bool startServer();

    /**
     * Stop the LSP server
     */
    void stopServer();

    /**
     * Check if server is running
     */
    bool isRunning() const { return m_serverRunning; }

    /**
     * Open document for tracking
     */
    void openDocument(const std::string& uri, const std::string& languageId, const std::string& text);

    /**
     * Close document
     */
    void closeDocument(const std::string& uri);

    /**
     * Update document content (incremental sync)
     */
    void updateDocument(const std::string& uri, const std::string& text, int version);

    /**
     * Request completions at cursor position
     * \param uri Document URI
     * \param line Line number (0-indexed)
     * \param character Column number (0-indexed)
     */
    void requestCompletions(const std::string& uri, int line, int character);

    /**
     * Request hover information
     */
    void requestHover(const std::string& uri, int line, int character);

    /**
     * Request go-to-definition
     */
    void requestDefinition(const std::string& uri, int line, int character);

    /**
     * Format document
     */
    void formatDocument(const std::string& uri);

    /**
     * Get current diagnostics for document
     */
    std::vector<Diagnostic> getDiagnostics(const std::string& uri) const;


    /**
     * Server initialized and ready
     */
    void serverReady();

    /**
     * Completions received
     */
    void completionsReceived(const std::string& uri, int line, int character, const std::vector<CompletionItem>& items);

    /**
     * Hover information received
     */
    void hoverReceived(const std::string& uri, const std::string& markdown);

    /**
     * Definition location received
     */
    void definitionReceived(const std::string& uri, int line, int character);

    /**
     * Diagnostics updated
     */
    void diagnosticsUpdated(const std::string& uri, const std::vector<Diagnostic>& diagnostics);

    /**
     * Format edits received
     */
    void formatEditsReceived(const std::string& uri, const std::string& formattedText);

    /**
     * Server error occurred
     */
    void serverError(const std::string& error);

private:
    void onServerReadyRead();
    void onServerError(int error);
    void onServerFinished(int exitCode, int status);

private:
    void sendMessage(const void*& message);
    void processMessage(const void*& message);
    void handleInitializeResponse(const void*& result);
    void handleCompletionResponse(const void*& result, int requestId);
    void handleHoverResponse(const void*& result, int requestId);
    void handleDefinitionResponse(const void*& result, int requestId);
    void handleDiagnostics(const void*& params);
    
    std::string buildDocumentUri(const std::string& filePath) const;
    
    LSPServerConfig m_config;
    void** m_serverProcess{};
    bool m_serverRunning = false;
    bool m_initialized = false;
    int m_nextRequestId = 1;
    
    std::vector<uint8_t> m_receiveBuffer;
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

