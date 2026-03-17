/**
 * @file InlayHintProvider.h
 * @brief Complete LSP Inlay Hints Implementation for RawrXD Agentic IDE
 * 
 * Provides inline type annotations, parameter names, and other hints
 * displayed directly in the editor without modifying source code.
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
 * @brief Types of inlay hints
 */
enum class InlayHintKind {
    Type,               ///< Type annotation (e.g., ": string" after variable)
    Parameter,          ///< Parameter name hint (e.g., "name:" before argument)
    ChainingHint,       ///< Return type for method chains
    EnumMember,         ///< Enum value name
    ClosingLabel,       ///< Label at end of long blocks (e.g., "// end if")
    ImplicitConversion, ///< Implicit type conversion warning
    LifetimeElision,    ///< Rust lifetime elision hints
    BindingMode,        ///< Rust binding mode hints (ref, mut)
    GenericType,        ///< Generic type parameter inference
    Designator,         ///< C designated initializer hints
    Default             ///< Default/fallback kind
};

/**
 * @brief Position within the text (can be before or after a position)
 */
enum class InlayHintPosition {
    Before,             ///< Display hint before the position
    After               ///< Display hint after the position
};

/**
 * @brief Represents a single inlay hint
 */
class InlayHintItem {
public:
    InlayHintItem();
    InlayHintItem(int line, int column, const QString& label, InlayHintKind kind);
    
    // Comparison for sorting
    bool operator<(const InlayHintItem& other) const;
    
    // Accessors
    int line() const { return m_line; }
    void setLine(int line) { m_line = line; }
    
    int column() const { return m_column; }
    void setColumn(int column) { m_column = column; }
    
    QString label() const { return m_label; }
    void setLabel(const QString& label) { m_label = label; }
    
    InlayHintKind kind() const { return m_kind; }
    void setKind(InlayHintKind kind) { m_kind = kind; }
    
    InlayHintPosition position() const { return m_position; }
    void setPosition(InlayHintPosition pos) { m_position = pos; }
    
    QString tooltip() const { return m_tooltip; }
    void setTooltip(const QString& tooltip) { m_tooltip = tooltip; }
    
    bool paddingLeft() const { return m_paddingLeft; }
    void setPaddingLeft(bool padding) { m_paddingLeft = padding; }
    
    bool paddingRight() const { return m_paddingRight; }
    void setPaddingRight(bool padding) { m_paddingRight = padding; }
    
    QColor foregroundColor() const { return m_foregroundColor; }
    void setForegroundColor(const QColor& color) { m_foregroundColor = color; }
    
    QColor backgroundColor() const { return m_backgroundColor; }
    void setBackgroundColor(const QColor& color) { m_backgroundColor = color; }
    
    bool isClickable() const { return m_isClickable; }
    void setClickable(bool clickable) { m_isClickable = clickable; }
    
    QString command() const { return m_command; }
    void setCommand(const QString& cmd) { m_command = cmd; }
    
    QStringList commandArgs() const { return m_commandArgs; }
    void setCommandArgs(const QStringList& args) { m_commandArgs = args; }
    
    bool isValid() const;
    
    /**
     * @brief Get formatted display string with styling
     */
    QString toDisplayString() const;
    
private:
    int m_line;
    int m_column;
    QString m_label;
    InlayHintKind m_kind;
    InlayHintPosition m_position;
    QString m_tooltip;
    bool m_paddingLeft;
    bool m_paddingRight;
    QColor m_foregroundColor;
    QColor m_backgroundColor;
    bool m_isClickable;
    QString m_command;
    QStringList m_commandArgs;
};

/**
 * @brief Represents a function parameter for hint generation
 */
struct ParameterInfo {
    QString name;
    QString type;
    QString defaultValue;
    bool isOptional;
    bool isVariadic;
    int index;
    
    ParameterInfo() : isOptional(false), isVariadic(false), index(0) {}
};

/**
 * @brief Represents a function/method signature
 */
struct FunctionSignature {
    QString name;
    QString returnType;
    QList<ParameterInfo> parameters;
    QString documentation;
    int line;
    int column;
    bool isMethod;
    bool isConstructor;
    bool isAsync;
    bool isGeneric;
    QString containerClass;
    
    FunctionSignature() : line(0), column(0), isMethod(false), 
                          isConstructor(false), isAsync(false), isGeneric(false) {}
};

/**
 * @brief Represents a variable declaration
 */
struct VariableDeclaration {
    QString name;
    QString inferredType;
    QString explicitType;
    int line;
    int column;
    bool isConst;
    bool hasExplicitType;
    
    VariableDeclaration() : line(0), column(0), isConst(false), hasExplicitType(false) {}
};

/**
 * @brief Language-specific patterns for inlay hint generation
 */
struct InlayHintLanguagePatterns {
    QRegularExpression functionCallPattern;
    QRegularExpression functionDefPattern;
    QRegularExpression variablePattern;
    QRegularExpression methodChainPattern;
    QRegularExpression closingBracePattern;
    QRegularExpression genericPattern;
    QRegularExpression lambdaPattern;
};

/**
 * @brief Thread-safe cache for inlay hints
 */
class InlayHintCache {
public:
    explicit InlayHintCache(int maxSize = 100);
    
    void insert(const QString& key, const QList<InlayHintItem>& items, const QString& contentHash);
    bool lookup(const QString& key, const QString& contentHash, QList<InlayHintItem>& outItems) const;
    void invalidate(const QString& key);
    void clear();
    int size() const;
    
private:
    struct CacheEntry {
        QList<InlayHintItem> items;
        QString contentHash;
        qint64 timestamp;
    };
    
    void evictOldest();
    
    mutable QMutex m_mutex;
    QHash<QString, CacheEntry> m_cache;
    int m_maxSize;
};

/**
 * @brief Complete Inlay Hint Provider for RawrXD IDE
 * 
 * Provides inline hints without modifying source code:
 * - Type annotations for variables with inferred types
 * - Parameter name hints at function call sites
 * - Return type hints for method chains
 * - Closing labels for long code blocks
 * - Generic type inference hints
 * - Implicit conversion warnings
 * 
 * Thread-safe with built-in caching for performance.
 * 
 * Usage:
 * @code
 * InlayHintProvider provider;
 * provider.setEnabled(InlayHintKind::Type, true);
 * provider.setEnabled(InlayHintKind::Parameter, true);
 * 
 * QList<InlayHintItem> hints = provider.getInlayHints(filePath, content);
 * for (const InlayHintItem& hint : hints) {
 *     // Render hint at (hint.line(), hint.column()) with text hint.label()
 * }
 * @endcode
 */
class InlayHintProvider : public QObject {
    Q_OBJECT
    
public:
    /**
     * @brief Construct an InlayHintProvider
     * @param parent Parent QObject
     */
    explicit InlayHintProvider(QObject* parent = nullptr);
    
    /**
     * @brief Destructor
     */
    ~InlayHintProvider() override;
    
    /**
     * @brief Get all inlay hints for a file
     * @param filePath Path to the source file
     * @param content File content
     * @return List of InlayHintItem for the file
     */
    QList<InlayHintItem> getInlayHints(const QString& filePath, const QString& content);
    
    /**
     * @brief Get inlay hints for a specific line range
     * @param filePath Path to the source file
     * @param content File content
     * @param startLine Start line (0-based)
     * @param endLine End line (0-based, inclusive)
     * @return List of InlayHintItem in the range
     */
    QList<InlayHintItem> getInlayHintsForRange(
        const QString& filePath,
        const QString& content,
        int startLine,
        int endLine
    );
    
    /**
     * @brief Get inlay hints asynchronously
     * @param filePath Path to the source file
     * @param content File content
     * @param callback Callback invoked with results
     */
    void getInlayHintsAsync(
        const QString& filePath,
        const QString& content,
        std::function<void(const QList<InlayHintItem>&)> callback
    );
    
    /**
     * @brief Invalidate cached hints for a file
     * @param filePath Path to invalidate
     */
    void invalidateFile(const QString& filePath);
    
    /**
     * @brief Clear all cached hints
     */
    void clearCache();
    
    /**
     * @brief Enable or disable a hint kind
     * @param kind Kind to enable/disable
     * @param enabled Whether to enable
     */
    void setEnabled(InlayHintKind kind, bool enabled);
    
    /**
     * @brief Check if a hint kind is enabled
     * @param kind Kind to check
     * @return true if enabled
     */
    bool isEnabled(InlayHintKind kind) const;
    
    /**
     * @brief Set maximum hint length before truncation
     * @param length Maximum characters (0 = no limit)
     */
    void setMaxHintLength(int length);
    
    /**
     * @brief Get maximum hint length
     */
    int maxHintLength() const;
    
    /**
     * @brief Set minimum characters for parameter hints
     * @param chars Minimum parameter name length
     * 
     * Short parameter names (like "x" or "i") won't show hints.
     */
    void setMinParameterNameLength(int chars);
    
    /**
     * @brief Get minimum parameter name length
     */
    int minParameterNameLength() const;
    
    /**
     * @brief Set minimum lines for closing labels
     * @param lines Minimum block size for labels
     */
    void setMinLinesForClosingLabel(int lines);
    
    /**
     * @brief Get minimum lines for closing labels
     */
    int minLinesForClosingLabel() const;
    
    /**
     * @brief Register a function signature database
     * @param signatures Map of function names to signatures
     * 
     * Provides parameter names for standard library and framework functions.
     */
    void registerFunctionSignatures(const QHash<QString, FunctionSignature>& signatures);
    
    /**
     * @brief Add a single function signature
     * @param name Function name
     * @param signature Function signature
     */
    void addFunctionSignature(const QString& name, const FunctionSignature& signature);
    
signals:
    /**
     * @brief Emitted when hints are ready
     * @param filePath File that was analyzed
     * @param hints Generated hints
     */
    void hintsReady(const QString& filePath, const QList<InlayHintItem>& hints);
    
    /**
     * @brief Emitted when analysis fails
     * @param filePath File that failed
     * @param error Error message
     */
    void hintError(const QString& filePath, const QString& error);
    
private:
    /**
     * @brief Initialize language-specific patterns
     */
    void initializeLanguagePatterns();
    
    /**
     * @brief Initialize built-in function signatures
     */
    void initializeBuiltinSignatures();
    
    /**
     * @brief Detect language from file extension
     */
    QString detectLanguage(const QString& filePath) const;
    
    /**
     * @brief Compute content hash for cache
     */
    QString computeContentHash(const QString& content) const;
    
    /**
     * @brief Extract function signatures from source
     */
    QList<FunctionSignature> extractFunctionSignatures(
        const QString& content,
        const QString& language
    ) const;
    
    /**
     * @brief Extract variable declarations from source
     */
    QList<VariableDeclaration> extractVariableDeclarations(
        const QString& content,
        const QString& language
    ) const;
    
    /**
     * @brief Generate type annotation hints
     */
    QList<InlayHintItem> generateTypeHints(
        const QList<VariableDeclaration>& variables,
        const QString& content,
        const QString& language
    ) const;
    
    /**
     * @brief Generate parameter name hints
     */
    QList<InlayHintItem> generateParameterHints(
        const QString& content,
        const QString& language
    ) const;
    
    /**
     * @brief Generate chaining return type hints
     */
    QList<InlayHintItem> generateChainingHints(
        const QString& content,
        const QString& language
    ) const;
    
    /**
     * @brief Generate closing label hints
     */
    QList<InlayHintItem> generateClosingLabelHints(
        const QString& content,
        const QString& language
    ) const;
    
    /**
     * @brief Generate enum member hints
     */
    QList<InlayHintItem> generateEnumHints(
        const QString& content,
        const QString& language
    ) const;
    
    /**
     * @brief Generate generic type inference hints
     */
    QList<InlayHintItem> generateGenericHints(
        const QString& content,
        const QString& language
    ) const;
    
    /**
     * @brief Infer type from expression
     */
    QString inferTypeFromExpression(
        const QString& expression,
        const QString& language,
        const QString& context
    ) const;
    
    /**
     * @brief Find function signature by name
     */
    FunctionSignature findFunctionSignature(const QString& name) const;
    
    /**
     * @brief Parse function arguments from call site
     */
    QStringList parseCallArguments(const QString& argString) const;
    
    /**
     * @brief Truncate hint label if too long
     */
    QString truncateLabel(const QString& label) const;
    
    /**
     * @brief Merge overlapping hints
     */
    QList<InlayHintItem> mergeHints(const QList<InlayHintItem>& hints) const;
    
    // Member variables
    std::unique_ptr<InlayHintCache> m_cache;
    QHash<QString, InlayHintLanguagePatterns> m_languagePatterns;
    QHash<QString, FunctionSignature> m_functionSignatures;
    
    // Configuration
    QHash<InlayHintKind, bool> m_enabledKinds;
    int m_maxHintLength;
    int m_minParameterNameLength;
    int m_minLinesForClosingLabel;
    bool m_asyncEnabled;
};

} // namespace RawrXD