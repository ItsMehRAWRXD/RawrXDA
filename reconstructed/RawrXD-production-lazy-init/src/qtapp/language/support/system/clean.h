/**
 * \file language_support_system.h
 * \brief Comprehensive language support system for 50+ languages
 * \author RawrXD AI Engineering Team
 * \date January 14, 2026
 * 
 * This system provides COMPLETE support for 50+ programming languages:
 * - Syntax highlighting (via TextMate grammars)
 * - Code completion (via LSP servers)
 * - Code formatting (via language-specific formatters)
 * - Debugging (via Debug Adapter Protocol)
 * - Linting & analysis (via language tools)
 * - Build system integration
 * 
 * NO STUBS - COMPLETE IMPLEMENTATIONS ONLY
 */

#pragma once

#include <lsp_client.h>
#include <QObject>
#include <QMap>
#include <QVector>
#include <QString>
#include <QStringList>
#include <QProcess>
#include <QTcpSocket>
#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <functional>

namespace RawrXD {
namespace Language {

// ============================================================================
// LANGUAGE DEFINITIONS - All 50+ supported languages
// ============================================================================

enum class LanguageID {
    C, CPP, ObjectiveC, ObjectiveCPP,
    CSharp, FSharp, VBNet,
    Java, Kotlin, Scala, Groovy, Clojure,
    Python,
    JavaScript, TypeScript, HTML, CSS, SCSS, SASS, Less, JSON, XML, YAML, Vue, Svelte, JSX, TSX,
    Rust, Go, Zig, D,
    Haskell, Elixir, Erlang, LISP, Scheme, Racket,
    PHP, Ruby, Perl, Lua,
    VHDL, Verilog, SystemVerilog,
    MASM, NASM, GAS, ARM,
    SQL, Shell, PowerShell, Bash, Dart, Swift, R, MATLAB, Octave, Julia, Fortran, COBOL, Ada, Pascal, Delphi,
    PlainText, Unknown
};

// ============================================================================
// LANGUAGE CONFIGURATION - Each language fully defined
// ============================================================================

struct LanguageConfiguration {
    LanguageID id;
    QString name;
    QString fileExtension;
    QStringList allExtensions;
    QString mimeType;
    
    QString lspServerCommand;
    QStringList lspServerArgs;
    QString lspLanguageId;
    
    QString formatterCommand;
    QStringList formatterArgs;
    bool supportsFormatOnSave;
    bool supportsRangeFormatting;
    
    QString debugAdapterCommand;
    QStringList debugAdapterArgs;
    QString debugLanguageId;
    
    QString linterCommand;
    QStringList linterArgs;
    
    QString buildSystemType;
    QString buildCommand;
    QString runCommand;
    QString testCommand;
    
    QString commentLineStart;
    QString commentBlockStart;
    QString commentBlockEnd;
    QStringList keywords;
    QStringList builtins;
    
    int indentSize;
    bool useSpaces;
    QString lineEndingStyle;
    
    bool supportsDebugger;
    bool supportsFormatter;
    bool supportsLinter;
    bool supportsLanguageServer;
    bool supportsBracketMatching;
    bool supportsCodeLens;
    bool supportsInlayHints;
    bool supportsSemanticTokens;
};

// ============================================================================
// LANGUAGE SUPPORT MANAGER - Orchestrates all language features
// ============================================================================

class LanguageSupportManager : public QObject {
    Q_OBJECT

public:
    explicit LanguageSupportManager(QObject* parent = nullptr);
    ~LanguageSupportManager() override;

    bool initialize();
    
    LanguageID detectLanguageFromFile(const QString& filePath) const;
    LanguageID detectLanguageFromContent(const QString& filePath) const;
    
    const LanguageConfiguration* getLanguageConfig(LanguageID id) const;
    const LanguageConfiguration* getLanguageConfig(const QString& fileName) const;
    
    std::string getLanguageName(LanguageID id) const;
    
    bool isLanguageSupported(LanguageID id) const;
    bool isFormatterAvailable(LanguageID id) const;
    bool isDebuggerAvailable(LanguageID id) const;
    bool isLSPAvailable(LanguageID id) const;
    
    void requestCompletion(const QString& filePath, int line, int column,
                          std::function<void(const QVector<RawrXD::CompletionItem>&)> callback);
    
    void requestFormatting(const QString& filePath, 
                          std::function<void(const QString&)> callback);
    
    void requestHover(const QString& filePath, int line, int column,
                     std::function<void(const QString&)> callback);
    
    void requestDefinition(const QString& filePath, int line, int column,
                          std::function<void(const QString&, int, int)> callback);
    
    void requestRename(const QString& filePath, int line, int column,
                      const QString& newName,
                      std::function<void(const QVector<QPair<QString, QVector<QPair<int, int>>>>&)> callback);
    
    void requestReferences(const QString& filePath, int line, int column,
                          std::function<void(const QVector<QPair<QString, QVector<QPair<int, int>>>>&)> callback);
    
    QVector<LanguageConfiguration> getSupportedLanguages() const;
    
    struct LanguageStats {
        int totalLanguages;
        int supportedWithLSP;
        int supportedWithFormatter;
        int supportedWithDebugger;
        int supportedWithLinter;
    };
    LanguageStats getStatistics() const;

private slots:
    void onLSPServerStarted(LanguageID id);
    void onLSPServerError(LanguageID id, const QString& error);
    void onFormatterFinished(LanguageID id, const QString& output);

private:
    void initializeLanguageConfigs();
    bool startLSPServer(LanguageID id);
    void stopLSPServer(LanguageID id);
    bool isToolAvailable(const QString& command);
    QString findToolInPath(const QString& toolName);

    QMap<LanguageID, LanguageConfiguration> m_languages;
    QMap<LanguageID, std::unique_ptr<QProcess>> m_lspServers;
    QMap<LanguageID, std::unique_ptr<QProcess>> m_formatters;

signals:
    void languageSupported(LanguageID id);
    void languageNotSupported(LanguageID id);
    void lspServerStarted(LanguageID id);
    void lspServerError(LanguageID id, const QString& error);
    void formattingCompleted(LanguageID id);
    void formattingError(LanguageID id, const QString& error);
};

// ============================================================================
// SYNTAX HIGHLIGHTING
// ============================================================================

struct HighlightRule {
    QRegularExpression pattern;
    QTextCharFormat format;
};

struct SemanticToken {
    int line;
    int column;
    int length;
    QString tokenType;
    QString tokenModifiers;
};

/**
 * TextMate grammar-based syntax highlighter
 */
class SyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit SyntaxHighlighter(QTextDocument* parent = nullptr);
    
    void setLanguage(LanguageID language);
    void applySemanticTokens(const QVector<SemanticToken>& tokens);
    void getSemanticTokens(const QString& filePath,
                          std::function<void(const QString&)> callback);

protected:
    void highlightBlock(const QString& text) override;

private:
    void initializeHighlightingRules();
    void loadGrammarForLanguage(LanguageID id);
    void applyHighlighting(const QString& text);
    void loadCppGrammar();
    void loadPythonGrammar();
    void loadRustGrammar();
    void loadJavaScriptGrammar();
    
    LanguageID m_currentLanguage;
    QVector<HighlightRule> m_currentRules;
};

}}  // namespace RawrXD::Language

