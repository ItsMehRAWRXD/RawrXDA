/**
 * @file SemanticHighlighter.h
 * @brief Complete LSP Semantic Highlighting Implementation for RawrXD Agentic IDE
 * 
 * Provides advanced semantic highlighting based on token types and scopes,
 * going beyond simple syntax highlighting to understand code semantics.
 * 
 * @author RawrXD Team
 * @copyright 2024 RawrXD
 */

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QHash>
#include <QMap>
#include <QRegularExpression>
#include <QMutex>
#include <QColor>
#include <memory>
#include <functional>

namespace RawrXD {

/**
 * @brief Semantic token types (based on LSP specification)
 */
enum class SemanticTokenType {
    Namespace,           ///< Namespace/module
    Type,                ///< Type (class, struct, enum, interface)
    Class,               ///< Class
    Enum,                ///< Enum
    Interface,          ///< Interface
    Struct,              ///< Struct
    TypeParameter,       ///< Type parameter (generic)
    Parameter,           ///< Function/method parameter
    Variable,            ///< Variable
    Property,            ///< Property/field
    EnumMember,          ///< Enum member
    Event,               ///< Event
    Function,            ///< Function
    Method,              ///< Method
    Macro,               ///< Macro
    Keyword,             ///< Keyword
    Modifier,            ///< Modifier (public, private, etc.)
    Comment,             ///< Comment
    String,              ///< String literal
    Number,              ///< Number literal
    Regexp,              ///< Regex literal
    Operator,            ///< Operator
    Decorator,           ///< Decorator/annotation
    Builtin,             ///< Built-in type/function
    Unknown              ///< Unknown/fallback
};

/**
 * @brief Semantic token modifiers
 */
enum class SemanticTokenModifier {
    Declaration,         ///< Declaration of symbol
    Definition,         ///< Definition of symbol
    Readonly,           ///< Readonly/const
    Static,             ///< Static
    Deprecated,         ///< Deprecated
    Abstract,           ///< Abstract
    Async,              ///< Async
    Modification,       ///< Modification (write access)
    Documentation,      ///< Documentation
    DefaultLibrary      ///< Default library symbol
};

/**
 * @brief Represents a semantic token with position and type information
 */
struct SemanticToken {
    int line;                   ///< Line number (0-based)
    int column;                 ///< Column number (0-based)
    int length;                 ///< Token length
    SemanticTokenType type;     ///< Token type
    QList<SemanticTokenModifier> modifiers; ///< Token modifiers
    QString text;               ///< Token text
    
    SemanticToken() : line(0), column(0), length(0), type(SemanticTokenType::Unknown) {}
    SemanticToken(int l, int c, int len, SemanticTokenType t) 
        : line(l), column(c), length(len), type(t) {}
    
    bool isValid() const { return length > 0 && !text.isEmpty(); }
};

/**
 * @brief Represents a semantic scope (block of code)
 */
struct SemanticScope {
    QString name;               ///< Scope name (function/class name)
    SemanticTokenType type;     ///< Scope type
    int startLine;              ///< Start line (0-based)
    int endLine;                ///< End line (0-based)
    QList<SemanticToken> tokens; ///< Tokens within this scope
    QHash<QString, SemanticToken> symbols; ///< Symbol definitions
    
    SemanticScope() : type(SemanticTokenType::Unknown), startLine(0), endLine(0) {}
    SemanticScope(const QString& n, SemanticTokenType t, int start) 
        : name(n), type(t), startLine(start), endLine(start) {}
    
    bool isValid() const { return !name.isEmpty() && startLine <= endLine; }
};

/**
 * @brief Language-specific semantic patterns
 */
struct SemanticLanguagePatterns {
    QRegularExpression namespacePattern;
    QRegularExpression classPattern;
    QRegularExpression structPattern;
    QRegularExpression enumPattern;
    QRegularExpression interfacePattern;
    QRegularExpression functionPattern;
    QRegularExpression methodPattern;
    QRegularExpression variablePattern;
    QRegularExpression parameterPattern;
    QRegularExpression propertyPattern;
    QRegularExpression typeParameterPattern;
    QRegularExpression macroPattern;
    QRegularExpression keywordPattern;
    QRegularExpression modifierPattern;
    QRegularExpression builtinPattern;
    QRegularExpression decoratorPattern;
};

/**
 * @brief Thread-safe cache for semantic tokens
 */
class SemanticTokenCache {
public:
    explicit SemanticTokenCache(int maxSize = 100);
    
    void insert(const QString& key, const QList<SemanticToken>& tokens, const QString& contentHash);
    bool lookup(const QString& key, const QString& contentHash, QList<SemanticToken>& outTokens) const;
    void invalidate(const QString& key);
    void clear();
    int size() const;
    
private:
    struct CacheEntry {
        QList<SemanticToken> tokens;
        QString contentHash;
        qint64 timestamp;
    };
    
    void evictOldest();
    
    mutable QMutex m_mutex;
    QHash<QString, CacheEntry> m_cache;
    int m_maxSize;
};

/**
 * @brief Complete Semantic Highlighter for RawrXD IDE
 * 
 * Provides advanced semantic highlighting:
 * - Token-based semantic analysis
 * - Scope-aware highlighting
 * - Type inference and propagation
 * - Cross-reference resolution
 * - Language-specific semantic rules
 * 
 * Thread-safe with built-in caching for performance.
 * 
 * Usage:
 * @code
 * SemanticHighlighter highlighter;
 * QList<SemanticToken> tokens = highlighter.highlightSemanticTokens(content);
 * for (const SemanticToken& token : tokens) {
 *     // Apply highlighting based on token.type and token.modifiers
 * }
 * @endcode
 */
class SemanticHighlighter : public QObject {
    Q_OBJECT
    
public:
    /**
     * @brief Construct a SemanticHighlighter
     * @param parent Parent QObject
     */
    explicit SemanticHighlighter(QObject* parent = nullptr);
    
    /**
     * @brief Destructor
     */
    ~SemanticHighlighter() override;
    
    /**
     * @brief Perform semantic highlighting on content
     * @param content Source code content
     * @return List of semantic tokens
     */
    QList<SemanticToken> highlightSemanticTokens(const QString& content);
    
    /**
     * @brief Perform semantic highlighting for a specific file
     * @param filePath File path
     * @param content File content
     * @return List of semantic tokens
     */
    QList<SemanticToken> highlightSemanticTokens(const QString& filePath, const QString& content);
    
    /**
     * @brief Get semantic tokens for a specific line range
     * @param content Source code content
     * @param startLine Start line (0-based)
     * @param endLine End line (0-based, inclusive)
     * @return List of semantic tokens in range
     */
    QList<SemanticToken> highlightSemanticTokensForRange(
        const QString& content,
        int startLine,
        int endLine
    );
    
    /**
     * @brief Perform semantic highlighting asynchronously
     * @param filePath File path
     * @param content File content
     * @param callback Callback invoked with results
     */
    void highlightSemanticTokensAsync(
        const QString& filePath,
        const QString& content,
        std::function<void(const QList<SemanticToken>&)> callback
    );
    
    /**
     * @brief Invalidate cached tokens for a file
     * @param filePath File path to invalidate
     */
    void invalidateFile(const QString& filePath);
    
    /**
     * @brief Clear all cached tokens
     */
    void clearCache();
    
    /**
     * @brief Set color scheme for semantic tokens
     * @param type Token type
     * @param color Color to use
     */
    void setTokenColor(SemanticTokenType type, const QColor& color);
    
    /**
     * @brief Get color for a token type
     * @param type Token type
     * @return Color for the type
     */
    QColor tokenColor(SemanticTokenType type) const;
    
    /**
     * @brief Set modifier color
     * @param modifier Token modifier
     * @param color Color to use
     */
    void setModifierColor(SemanticTokenModifier modifier, const QColor& color);
    
    /**
     * @brief Get color for a modifier
     * @param modifier Token modifier
     * @return Color for the modifier
     */
    QColor modifierColor(SemanticTokenModifier modifier) const;
    
    /**
     * @brief Enable or disable semantic highlighting
     * @param enabled Whether to enable
     */
    void setEnabled(bool enabled);
    
    /**
     * @brief Check if semantic highlighting is enabled
     * @return true if enabled
     */
    bool isEnabled() const;
    
    /**
     * @brief Set whether to use aggressive type inference
     * @param aggressive Whether to use aggressive inference
     */
    void setAggressiveTypeInference(bool aggressive);
    
    /**
     * @brief Check if aggressive type inference is enabled
     * @return true if enabled
     */
    bool aggressiveTypeInference() const;
    
    /**
     * @brief Set whether to highlight unused symbols
     * @param highlight Whether to highlight unused symbols
     */
    void setHighlightUnusedSymbols(bool highlight);
    
    /**
     * @brief Check if unused symbols are highlighted
     * @return true if enabled
     */
    bool highlightUnusedSymbols() const;
    
signals:
    /**
     * @brief Emitted when semantic tokens are ready
     * @param filePath File that was analyzed
     * @param tokens Generated semantic tokens
     */
    void semanticTokensReady(const QString& filePath, const QList<SemanticToken>& tokens);
    
    /**
     * @brief Emitted when analysis fails
     * @param filePath File that failed
     * @param error Error message
     */
    void semanticHighlightingError(const QString& filePath, const QString& error);
    
private:
    /**
     * @brief Initialize language-specific patterns
     */
    void initializeLanguagePatterns();
    
    /**
     * @brief Initialize default color scheme
     */
    void initializeColorScheme();
    
    /**
     * @brief Detect language from file extension
     */
    QString detectLanguage(const QString& filePath) const;
    
    /**
     * @brief Compute content hash for cache
     */
    QString computeContentHash(const QString& content) const;
    
    /**
     * @brief Extract semantic tokens from content
     */
    QList<SemanticToken> extractSemanticTokens(
        const QString& content,
        const QString& language
    ) const;
    
    /**
     * @brief Extract semantic scopes from content
     */
    QList<SemanticScope> extractSemanticScopes(
        const QString& content,
        const QString& language
    ) const;
    
    /**
     * @brief Resolve symbol references within scopes
     */
    void resolveSymbolReferences(
        QList<SemanticScope>& scopes,
        const QString& content
    ) const;
    
    /**
     * @brief Infer types for variables
     */
    void inferVariableTypes(
        QList<SemanticScope>& scopes,
        const QString& content,
        const QString& language
    ) const;
    
    /**
     * @brief Merge tokens from scopes into flat list
     */
    QList<SemanticToken> mergeTokensFromScopes(const QList<SemanticScope>& scopes) const;
    
    /**
     * @brief Apply color scheme to tokens
     */
    void applyColorScheme(QList<SemanticToken>& tokens) const;
    
    /**
     * @brief Filter tokens based on enabled features
     */
    QList<SemanticToken> filterTokens(const QList<SemanticToken>& tokens) const;
    
    // Member variables
    std::unique_ptr<SemanticTokenCache> m_cache;
    QHash<QString, SemanticLanguagePatterns> m_languagePatterns;
    QHash<SemanticTokenType, QColor> m_tokenColors;
    QHash<SemanticTokenModifier, QColor> m_modifierColors;
    
    // Configuration
    bool m_enabled;
    bool m_aggressiveTypeInference;
    bool m_highlightUnusedSymbols;
    bool m_asyncEnabled;
};

} // namespace RawrXD
