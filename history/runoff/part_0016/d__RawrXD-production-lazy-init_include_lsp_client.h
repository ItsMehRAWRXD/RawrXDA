/**
 * \file lsp_client.h
 * \brief Language Server Protocol client with streaming completions
 * \author RawrXD Team
 * \date 2025-12-07
 * 
 * STABLE API — frozen as of v1.0.0 (January 22, 2026)
 * Breaking changes require MAJOR version bump (v2.0.0+)
 * See FEATURE_FLAGS.md for API stability guarantees
 */

#pragma once

#include <QObject>
#include <QProcess>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QMap>
#include <QTimer>
#include <QSet>
#include <QVector>
#include <functional>

namespace RawrXD {

/**
 * \brief LSP server configuration
 */
struct LSPServerConfig {
    QString language;           // "cpp", "python", "javascript", etc.
    QString command;            // "clangd", "pylsp", "typescript-language-server"
    QStringList arguments;      // Server-specific args
    QString workspaceRoot;      // Workspace root directory
    bool autoStart = true;      // Auto-start on initialization
};

/**
 * \brief Completion item from LSP server
 */
struct CompletionItem {
    QString label;              // Display label
    QString insertText;         // Text to insert
    QString detail;             // Additional details
    QString documentation;      // Documentation string
    int kind = 1;              // CompletionItemKind (1=Text, 2=Method, 3=Function, etc.)
    QString sortText;          // Sort order
    QString filterText;        // Filter text
    int score = 0;             // Relevance score (computed)
    int insertTextFormat = 1;  // 1=PlainText, 2=Snippet
    bool preselect = false;    // Should this item be preselected
    bool deprecated = false;   // Is this item deprecated
};

/**
 * \brief Parameter information for signature help
 */
struct ParameterInfo {
    QString label;             // Parameter label
    QString documentation;     // Parameter documentation
    int start = 0;            // Start offset in signature
    int end = 0;              // End offset in signature
};

/**
 * \brief Signature information
 */
struct SignatureInfo {
    QString label;
    QString documentation;
    QVector<ParameterInfo> parameters;
};

/**
 * \brief Signature help result
 */
struct SignatureHelp {
    QVector<QString> signatures;     // Function signatures
    int activeSignature = 0;         // Current signature index
    int activeParameter = 0;         // Current parameter index
    QVector<ParameterInfo> parameters; // Parameter details
};

/**
 * \brief LSP diagnostic message
 */
struct Diagnostic {
    int line;                  // Line number (0-indexed)
    int column;                // Column number (0-indexed)
    int severity;              // 1=Error, 2=Warning, 3=Info, 4=Hint
    QString message;           // Diagnostic message
    QString source;            // Source (e.g., "clangd")
    QString code;              // Diagnostic code
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
class LSPClient : public QObject
{
    Q_OBJECT

public:
    using ResponseCallback = std::function<void(const QJsonObject&)>;

    explicit LSPClient(const LSPServerConfig& config, QObject* parent = nullptr);
    ~LSPClient() override;

    /**
     * Two-phase initialization - call after QApplication ready
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
     * Check if server is connected and initialized
     */
    bool isConnected() const { return m_serverRunning && m_initialized; }

    void openDocument(const QString& uri, const QString& languageId, const QString& text);
    void closeDocument(const QString& uri);
    void updateDocument(const QString& uri, const QString& text, int version);

    void requestCompletions(const QString& uri, int line, int character);
    void requestHover(const QString& uri, int line, int character);
    void requestDefinition(const QString& uri, int line, int character);
    void requestReferences(const QString& uri, int line, int character);
    void requestSignatureHelp(const QString& uri, int line, int character);
    void requestRename(const QString& uri, int line, int character, const QString& newName);
    
    // Code actions and Refactoring
    void getCodeActions(const QString& uri, int line, int character);
    void executeCodeAction(const QJsonObject& action);
    void requestExtractMethod(const QString& uri, int startLine, int endLine, const QString& methodName);
    void requestExtractVariable(const QString& uri, int line, int startChar, int endChar, const QString& varName);
    void requestOrganizeImports(const QString& uri);

    void sendRequest(const QString& method, const QJsonObject& params, ResponseCallback callback = nullptr);
    void formatDocument(const QString& uri);
    QVector<Diagnostic> getDiagnostics(const QString& uri) const;

signals:
    void serverReady();
    void completionsReceived(const QString& uri, int line, int character, const QVector<CompletionItem>& items);
    void codeActionsReceived(const QVector<QJsonObject>& actions);
    void hoverReceived(const QString& uri, const QString& markdown);
    void signatureHelpReceived(const QString& uri, const SignatureHelp& help);
    void definitionReceived(const QString& uri, int line, int character);
    void referencesReceived(const QVector<Diagnostic>& locations);
    void renameReceived(const QJsonObject& workspaceEdit);
    void diagnosticsUpdated(const QString& uri, const QVector<Diagnostic>& diagnostics);
    void formatEditsReceived(const QString& uri, const QString& formattedText);
    void serverError(const QString& error);

private slots:
    void onServerReadyRead();
    void onServerError(QProcess::ProcessError error);
    void onServerFinished(int exitCode, QProcess::ExitStatus status);

private:
    void sendMessage(const QJsonObject& message);
    void processMessage(const QJsonObject& message);
    void handleInitializeResponse(const QJsonValue& result);
    void handleCompletionResponse(const QJsonValue& result, int requestId);
    void handleHoverResponse(const QJsonValue& result, int requestId);
    void handleSignatureHelpResponse(const QJsonValue& result, int requestId);
    void handleDefinitionResponse(const QJsonValue& result, int requestId);
    void handleReferencesResponse(const QJsonValue& result, int requestId);
    void handleRenameResponse(const QJsonValue& result, int requestId);
    void handleCodeActionResponse(const QJsonValue& result, int requestId);
    void handleDiagnostics(const QJsonObject& params);
    
    QString buildDocumentUri(const QString& filePath) const;
    int computeCompletionScore(const CompletionItem& item, const QString& filter) const;
    
    LSPServerConfig m_config;
    QProcess* m_serverProcess{};
    bool m_serverRunning = false;
    bool m_initialized = false;
    int m_nextRequestId = 1;
    
    QByteArray m_receiveBuffer;
    QMap<QString, int> m_documentVersions;
    QMap<QString, QVector<Diagnostic>> m_diagnostics;
    QMap<QString, QVector<CompletionItem>> m_completionCache;
    
    struct PendingRequest {
        QString type;
        QString uri;
        int line;
        int character;
        QString metadata;
        ResponseCallback callback;
    };
    QMap<int, PendingRequest> m_pendingRequests;
};

} // namespace RawrXD
