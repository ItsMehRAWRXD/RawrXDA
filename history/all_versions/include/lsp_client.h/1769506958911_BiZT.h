/**
 * \file lsp_client.h
 * \brief Language Server Protocol client with streaming completions
 * \author RawrXD Team
 * \date 2025-12-07
 */

#pragma once

#include <QObject>
#include <QProcess>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QMap>
#include <QTimer>
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
     * Open document for tracking
     */
    void openDocument(const QString& uri, const QString& languageId, const QString& text);

    /**
     * Close document
     */
    void closeDocument(const QString& uri);

    /**
     * Update document content (incremental sync)
     */
    void updateDocument(const QString& uri, const QString& text, int version);

    /**
     * Request completions at cursor position
     * \param uri Document URI
     * \param line Line number (0-indexed)
     * \param character Column number (0-indexed)
     */
    void requestCompletions(const QString& uri, int line, int character);

    /**
     * Request hover information
     */
    void requestHover(const QString& uri, int line, int character);

    /**
     * Request go-to-definition
     */
    void requestDefinition(const QString& uri, int line, int character);

    /**
     * Format document
     */
    void formatDocument(const QString& uri);

    /**
     * Get current diagnostics for document
     */
    QVector<Diagnostic> getDiagnostics(const QString& uri) const;

signals:
    /**
     * Server initialized and ready
     */
    void serverReady();

    /**
     * Completions received
     */
    void completionsReceived(const QString& uri, int line, int character, const QVector<CompletionItem>& items);

    /**
     * Hover information received
     */
    void hoverReceived(const QString& uri, const QString& markdown);

    /**
     * Definition location received
     */
    void definitionReceived(const QString& uri, int line, int character);

    /**
     * Diagnostics updated
     */
    void diagnosticsUpdated(const QString& uri, const QVector<Diagnostic>& diagnostics);

    /**
     * Format edits received
     */
    void formatEditsReceived(const QString& uri, const QString& formattedText);

    /**
     * Server error occurred
     */
    void serverError(const QString& error);

private slots:
    void onServerReadyRead();
    void onServerError(QProcess::ProcessError error);
    void onServerFinished(int exitCode, QProcess::ExitStatus status);

private:
    void sendMessage(const QJsonObject& message);
    void sendNotification(const QString& method, const QJsonObject& params);
    void processMessage(const QJsonObject& message);
    void handleInitializeResponse(const QJsonObject& result);
    void handleCompletionResponse(const QJsonObject& result, int requestId);
    void handleHoverResponse(const QJsonObject& result, int requestId);
    void handleDefinitionResponse(const QJsonObject& result, int requestId);
    void handleDiagnostics(const QJsonObject& params);
    
    void sendIncrementalUpdate(const QString& uri, qint64 version,
                              const QString& oldContent, const QString& newContent);
    void cancelRequest(const QString& id);
    
    struct Position { int line; int character; };
    Position offsetToPosition(const QString& text, int offset);
    
    QString buildDocumentUri(const QString& filePath) const;
    
    LSPServerConfig m_config;
    QProcess* m_serverProcess{};
    bool m_serverRunning = false;
    bool m_initialized = false;
    int m_nextRequestId = 1;
    
    QByteArray m_receiveBuffer;
    QMap<QString, int> m_documentVersions;  // uri -> version
    QMap<QString, QVector<Diagnostic>> m_diagnostics;  // uri -> diagnostics
    QSet<QString> m_pendingCancellations;  // Cancellation tracking
    
    // Request tracking
    struct PendingRequest {
        QString type;  // "completion", "hover", "definition", etc.
        QString uri;
        int line;
        int character;
    };
    QMap<int, PendingRequest> m_pendingRequests;
};

} // namespace RawrXD
