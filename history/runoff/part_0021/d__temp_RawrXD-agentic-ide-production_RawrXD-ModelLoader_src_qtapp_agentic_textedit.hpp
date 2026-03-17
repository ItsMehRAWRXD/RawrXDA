/**
 * @file agentic_textedit.hpp
 * @brief AI-powered code editor widget with LSP integration
 */

#pragma once

#include <QTextEdit>
#include <QSyntaxHighlighter>

namespace RawrXD {

class LSPClient;
class CodeSyntaxHighlighter;

/**
 * @class AgenticTextEdit
 * @brief Advanced text editor with syntax highlighting, LSP completion, and AI features
 */
// Legacy alternate implementation to avoid duplicate class symbols.
// The primary implementation lives in `src/agentic_text_edit.h`.
class AgenticTextEditLegacy : public QTextEdit {
    Q_OBJECT

public:
    explicit AgenticTextEditLegacy(QWidget* parent = nullptr);
    
    /**
     * @brief Initialize the editor (set up highlighting, connections, etc.)
     */
    void initialize();
    
    /**
     * @brief Set the LSP client for code intelligence features
     */
    void setLSPClient(LSPClient* client);
    
    /**
     * @brief Set the document URI for LSP tracking
     */
    void setDocumentUri(const QString& uri);
    
    /**
     * @brief Apply a completion suggestion
     */
    void applyCompletion(const QString& completion);
    
    /**
     * @brief Apply syntax fixes (reformatting, corrections)
     */
    void applySyntaxFix(const QString& fixedCode);
    
    /**
     * @brief Show code lens information
     */
    void showCodeLens(const QString& hint);
    
    // Query methods
    QString getSelectedText() const;
    int getCurrentLineNumber() const;
    int getCurrentColumnNumber() const;
    QString getLineText(int lineNumber) const;
    
    // Editing methods
    void insertAtLine(int lineNumber, const QString& text);
    void replaceLine(int lineNumber, const QString& text);
    void deleteRange(int startLine, int startCol, int endLine, int endCol);

signals:
    void contentChanged();
    void syntaxFixed();
    void codeLensUpdated(const QString& hint);

private:
    LSPClient* m_lspClient;
    QString m_documentUri;
    CodeSyntaxHighlighter* m_highlighter;
};

} // namespace RawrXD
