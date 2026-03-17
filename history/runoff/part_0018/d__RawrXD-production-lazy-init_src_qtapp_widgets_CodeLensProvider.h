/**
 * @file CodeLensProvider.h
 * @brief Complete LSP CodeLens Implementation for RawrXD Agentic IDE
 * 
 * Provides actionable code hints displayed inline above functions, classes, and other
 * symbols. Implements reference counting, test detection, git blame integration,
 * performance annotations, and custom lens types.
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
#include <QDateTime>
#include <QMutex>
#include <memory>
#include <functional>

namespace RawrXD {

/**
 * @brief Types of code lenses that can be displayed
 */
enum class CodeLensType {
    Reference,          ///< Reference count lens (e.g., "5 references")
    Implementation,     ///< Implementation count for interfaces/classes
    Test,               ///< Test run/debug buttons
    GitBlame,           ///< Git blame information
    Performance,        ///< Performance hints (complexity, line count)
    Documentation,      ///< Documentation status/generation
    Debug,              ///< Debug-related actions
    Custom              ///< Custom provider lens
};

/**
 * @brief Symbol types extracted from source code
 */
enum class SymbolType {
    Unknown,
    Class,
    Struct,
    Interface,
    Enum,
    Function,
    Method,
    Property,
    Variable,
    Constant,
    Namespace,
    Module
};

/**
 * @brief Represents a single code lens item
 * 
 * CodeLensItem contains all information needed to display an actionable
 * hint above a line of code in the editor.
 */
class CodeLensItem {
public:
    CodeLensItem();
    CodeLensItem(int line, int column, const QString& text, CodeLensType type);
    
    // Comparison for sorting
    bool operator<(const CodeLensItem& other) const;
    
    // Accessors
    int line() const { return m_line; }
    void setLine(int line) { m_line = line; }
    
    int column() const { return m_column; }
    void setColumn(int column) { m_column = column; }
    
    QString text() const { return m_text; }
    void setText(const QString& text) { m_text = text; }
    
    CodeLensType type() const { return m_type; }
    void setType(CodeLensType type) { m_type = type; }
    
    QString command() const { return m_command; }
    void setCommand(const QString& command) { m_command = command; }
    
    QStringList commandArgs() const { return m_commandArgs; }
    void setCommandArgs(const QStringList& args) { m_commandArgs = args; }
    
    QString tooltip() const { return m_tooltip; }
    void setTooltip(const QString& tooltip) { m_tooltip = tooltip; }
    
    QString symbolName() const { return m_symbolName; }
    void setSymbolName(const QString& name) { m_symbolName = name; }
    
    int referenceCount() const { return m_referenceCount; }
    void setReferenceCount(int count) { m_referenceCount = count; }
    
    int priority() const { return m_priority; }
    void setPriority(int priority) { m_priority = priority; }
    
    bool isClickable() const { return m_isClickable; }
    void setClickable(bool clickable) { m_isClickable = clickable; }
    
    bool isVisible() const { return m_isVisible; }
    void setVisible(bool visible) { m_isVisible = visible; }
    
    bool isValid() const;
    
    /**
     * @brief Get display string with type-specific prefix icon
     */
    QString toDisplayString() const;
    
private:
    int m_line;
    int m_column;
    QString m_text;
    CodeLensType m_type;
    QString m_command;
    QStringList m_commandArgs;
    QString m_tooltip;
    QString m_symbolName;
    int m_referenceCount;
    int m_priority;
    bool m_isClickable;
    bool m_isVisible;
};

/**
 * @brief Information about a code symbol
 */
struct SymbolInfo {
    QString name;
    QString signature;
    QString documentation;
    SymbolType type;
    int line;
    int column;
    int endLine;
    int endColumn;
    QString containerName; // For methods: the class name
    
    SymbolInfo();
    bool isValid() const;
};

/**
 * @brief Git blame information for a line
 */
struct GitBlameInfo {
    QString commitHash;
    QString author;
    QString email;
    QDateTime date;
    QString summary;
};

/**
 * @brief Language-specific regex patterns for symbol extraction
 */
struct LanguagePatterns {
    QRegularExpression functionPattern;
    QRegularExpression classPattern;
    QRegularExpression methodPattern;
    QRegularExpression namespacePattern;
    QRegularExpression testPattern;
    QRegularExpression propertyPattern;
};

/**
 * @brief Thread-safe cache for code lens results
 */
class CodeLensCache {
public:
    explicit CodeLensCache(int maxSize = 100);
    
    void insert(const QString& key, const QList<CodeLensItem>& items, const QString& contentHash);
    bool lookup(const QString& key, const QString& contentHash, QList<CodeLensItem>& outItems) const;
    void invalidate(const QString& key);
    void clear();
    int size() const;
    
private:
    struct CacheEntry {
        QList<CodeLensItem> items;
        QString contentHash;
        qint64 timestamp;
    };
    
    void evictOldest();
    
    mutable QMutex m_mutex;
    QHash<QString, CacheEntry> m_cache;
    int m_maxSize;
};

/**
 * @brief Complete Code Lens Provider for RawrXD IDE
 * 
 * Provides rich code intelligence features including:
 * - Reference counting for functions, classes, and variables
 * - Test detection and run/debug buttons
 * - Git blame integration showing last modifier
 * - Performance hints for complex functions
 * - Documentation status and generation triggers
 * - Custom provider extension points
 * 
 * Thread-safe with built-in caching for performance.
 * 
 * Usage:
 * @code
 * CodeLensProvider provider;
 * provider.setEnabled(CodeLensType::GitBlame, true);
 * 
 * QList<CodeLensItem> lenses = provider.getCodeLenses(filePath, content);
 * for (const CodeLensItem& lens : lenses) {
 *     // Render lens at lens.line() with text lens.toDisplayString()
 * }
 * @endcode
 */
class CodeLensProvider : public QObject {
    Q_OBJECT
    
public:
    /**
     * @brief Construct a CodeLensProvider
     * @param parent Parent QObject
     */
    explicit CodeLensProvider(QObject* parent = nullptr);
    
    /**
     * @brief Destructor
     */
    ~CodeLensProvider() override;
    
    /**
     * @brief Get code lenses for a file (synchronous)
     * @param filePath Path to the source file
     * @param content File content
     * @return List of CodeLensItem for the file
     * 
     * This method extracts symbols from the content, generates various
     * types of code lenses, and returns them sorted by line.
     */
    QList<CodeLensItem> getCodeLenses(const QString& filePath, const QString& content);
    
    /**
     * @brief Get code lenses for a file (asynchronous)
     * @param filePath Path to the source file
     * @param content File content
     * @param callback Callback invoked with results
     * 
     * Performs analysis on a background thread to avoid blocking the UI.
     */
    void getCodeLensesAsync(
        const QString& filePath,
        const QString& content,
        std::function<void(const QList<CodeLensItem>&)> callback
    );
    
    /**
     * @brief Invalidate cached lenses for a file
     * @param filePath Path to invalidate
     */
    void invalidateFile(const QString& filePath);
    
    /**
     * @brief Clear all cached lenses
     */
    void clearCache();
    
    /**
     * @brief Enable or disable a lens type
     * @param type Type to enable/disable
     * @param enabled Whether to enable
     */
    void setEnabled(CodeLensType type, bool enabled);
    
    /**
     * @brief Check if a lens type is enabled
     * @param type Type to check
     * @return true if enabled
     */
    bool isEnabled(CodeLensType type) const;
    
    /**
     * @brief Set minimum reference count for display
     * @param count Minimum count (default: 1)
     * 
     * Symbols with fewer references than this won't show a lens.
     */
    void setMinReferencesForDisplay(int count);
    
    /**
     * @brief Get minimum reference count for display
     */
    int minReferencesForDisplay() const;
    
    /**
     * @brief Register a custom lens provider
     * @param name Unique provider name
     * @param provider Function that generates lenses
     * 
     * Custom providers can add domain-specific lenses.
     */
    void registerCustomLensProvider(
        const QString& name,
        std::function<QList<CodeLensItem>(const QString&, const QString&)> provider
    );
    
    /**
     * @brief Unregister a custom lens provider
     * @param name Provider name to remove
     */
    void unregisterCustomLensProvider(const QString& name);
    
signals:
    /**
     * @brief Emitted when code lenses are ready
     * @param filePath File that was analyzed
     * @param items Generated code lenses
     */
    void codeLensesReady(const QString& filePath, const QList<CodeLensItem>& items);
    
    /**
     * @brief Emitted when analysis fails
     * @param filePath File that failed
     * @param error Error message
     */
    void codeLensError(const QString& filePath, const QString& error);
    
private:
    /**
     * @brief Initialize language-specific regex patterns
     */
    void initializeLanguagePatterns();
    
    /**
     * @brief Detect programming language from file extension
     */
    QString detectLanguage(const QString& filePath) const;
    
    /**
     * @brief Compute content hash for cache validation
     */
    QString computeContentHash(const QString& content) const;
    
    /**
     * @brief Extract symbols from source code
     */
    QList<SymbolInfo> extractSymbols(const QString& content, const QString& language) const;
    
    /**
     * @brief Generate reference count lenses
     */
    QList<CodeLensItem> generateReferenceLenses(
        const QList<SymbolInfo>& symbols,
        const QString& content,
        const QString& filePath
    ) const;
    
    /**
     * @brief Count references to a symbol
     */
    int countReferences(const QString& symbolName, const QString& content) const;
    
    /**
     * @brief Count implementations of a class/interface
     */
    int countImplementations(const QString& symbolName, const QString& content) const;
    
    /**
     * @brief Generate test-related lenses
     */
    QList<CodeLensItem> generateTestLenses(const QString& content, const QString& language) const;
    
    /**
     * @brief Generate git blame lenses
     */
    QList<CodeLensItem> generateGitBlameLenses(
        const QString& filePath,
        const QList<SymbolInfo>& symbols
    ) const;
    
    /**
     * @brief Generate performance hint lenses
     */
    QList<CodeLensItem> generatePerformanceLenses(
        const QList<SymbolInfo>& symbols,
        const QString& content,
        const QString& language
    ) const;
    
    /**
     * @brief Calculate cyclomatic complexity of a function
     */
    int calculateCyclomaticComplexity(const SymbolInfo& symbol, const QString& content) const;
    
    /**
     * @brief Generate documentation lenses
     */
    QList<CodeLensItem> generateDocumentationLenses(
        const QList<SymbolInfo>& symbols,
        const QString& content
    ) const;
    
    /**
     * @brief Merge lenses on the same line
     */
    QList<CodeLensItem> mergeCodeLenses(const QList<CodeLensItem>& items) const;
    
    // Member variables
    std::unique_ptr<CodeLensCache> m_cache;
    QHash<QString, LanguagePatterns> m_languagePatterns;
    QHash<QString, std::function<QList<CodeLensItem>(const QString&, const QString&)>> m_customProviders;
    
    // Configuration
    bool m_enableReferenceCounting;
    bool m_enableTestDetection;
    bool m_enableGitBlame;
    bool m_enablePerformanceHints;
    bool m_enableDocumentationLinks;
    bool m_asyncEnabled;
    int m_minReferencesForDisplay;
};

} // namespace RawrXD