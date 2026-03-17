#ifndef ADVANCED_REFACTORING_H
#define ADVANCED_REFACTORING_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutex>
#include <QRegularExpression>

/**
 * @brief Advanced code refactoring engine with intelligent transformations
 * 
 * Features:
 * - Extract method/function with automatic parameter detection
 * - Inline variable/method with usage tracking
 * - Rename symbol with scope-aware replacement
 * - Move class/method between files
 * - Convert between types (loop types, conditionals, etc.)
 * - Change method signature
 * - Extract interface/base class
 * - Introduce parameter object
 * - Replace magic numbers with named constants
 * - Optimize imports/includes
 */
class AdvancedRefactoring : public QObject
{
    Q_OBJECT

public:
    // Refactoring types
    enum class RefactoringType {
        ExtractMethod,
        ExtractFunction,
        InlineVariable,
        InlineMethod,
        RenameSymbol,
        MoveClass,
        MoveMethod,
        ChangeSignature,
        ExtractInterface,
        ExtractBaseClass,
        IntroduceParameterObject,
        ReplaceMagicNumber,
        ConvertLoopType,
        ConvertConditional,
        OptimizeIncludes,
        RemoveUnusedCode,
        Custom
    };
    Q_ENUM(RefactoringType)

    // Symbol types
    enum class SymbolType {
        Variable,
        Function,
        Method,
        Class,
        Struct,
        Enum,
        Typedef,
        Namespace,
        Macro,
        Unknown
    };
    Q_ENUM(SymbolType)

    // Scope types
    enum class ScopeType {
        Global,
        Namespace,
        Class,
        Function,
        Block,
        File
    };
    Q_ENUM(ScopeType)

    // Data structures
    struct CodeRange {
        QString filePath;
        int startLine;
        int startColumn;
        int endLine;
        int endColumn;
        QString originalText;
        
        QJsonObject toJson() const;
        static CodeRange fromJson(const QJsonObject& obj);
    };

    struct SymbolInfo {
        QString name;
        SymbolType type;
        ScopeType scope;
        QString filePath;
        int line;
        int column;
        QString signature;
        QStringList references;  // List of file:line:column references
        QMap<QString, QString> metadata;
        
        QJsonObject toJson() const;
        static SymbolInfo fromJson(const QJsonObject& obj);
    };

    struct RefactoringOperation {
        QString operationId;
        RefactoringType type;
        QString description;
        CodeRange sourceRange;
        CodeRange targetRange;
        QMap<QString, QVariant> parameters;
        QDateTime timestamp;
        bool canUndo;
        
        QJsonObject toJson() const;
        static RefactoringOperation fromJson(const QJsonObject& obj);
    };

    struct RefactoringResult {
        bool success;
        QString message;
        RefactoringOperation operation;
        QList<QPair<QString, QString>> fileChanges;  // filePath -> newContent
        QStringList warnings;
        int linesChanged;
        
        QJsonObject toJson() const;
    };

    struct MethodSignature {
        QString returnType;
        QString methodName;
        QList<QPair<QString, QString>> parameters;  // type, name
        QStringList modifiers;  // const, static, virtual, etc.
        
        QString toString() const;
        QJsonObject toJson() const;
        static MethodSignature fromString(const QString& signature);
    };

    explicit AdvancedRefactoring(QObject* parent = nullptr);
    ~AdvancedRefactoring();

    // Extract refactorings
    RefactoringResult extractMethod(const CodeRange& selection, const QString& methodName,
                                   const QString& returnType = "void");
    RefactoringResult extractFunction(const CodeRange& selection, const QString& functionName,
                                     const QString& returnType = "void");
    RefactoringResult extractInterface(const QString& className, const QString& interfaceName,
                                      const QStringList& methodNames);
    RefactoringResult extractBaseClass(const QString& className, const QString& baseName,
                                      const QStringList& memberNames);
    RefactoringResult introduceParameterObject(const QString& functionName,
                                              const QStringList& parameterNames,
                                              const QString& objectName);

    // Inline refactorings
    RefactoringResult inlineVariable(const QString& variableName, const CodeRange& scope);
    RefactoringResult inlineMethod(const QString& methodName, const CodeRange& scope);
    RefactoringResult inlineConstant(const QString& constantName);

    // Rename refactorings
    RefactoringResult renameSymbol(const QString& oldName, const QString& newName,
                                  SymbolType type, const CodeRange& scope);
    RefactoringResult renameFile(const QString& oldPath, const QString& newPath);
    RefactoringResult renameClass(const QString& oldName, const QString& newName);
    RefactoringResult renameMethod(const QString& oldName, const QString& newName,
                                  const QString& className);

    // Move refactorings
    RefactoringResult moveClass(const QString& className, const QString& sourceFile,
                               const QString& targetFile);
    RefactoringResult moveMethod(const QString& methodName, const QString& sourceClass,
                                const QString& targetClass);
    RefactoringResult moveToNamespace(const QString& symbolName, const QString& targetNamespace);

    // Signature changes
    RefactoringResult changeMethodSignature(const QString& methodName, const QString& className,
                                          const MethodSignature& newSignature);
    RefactoringResult addParameter(const QString& methodName, const QString& className,
                                  const QString& paramType, const QString& paramName,
                                  const QString& defaultValue = "");
    RefactoringResult removeParameter(const QString& methodName, const QString& className,
                                     const QString& paramName);
    RefactoringResult reorderParameters(const QString& methodName, const QString& className,
                                       const QStringList& newOrder);

    // Code improvements
    RefactoringResult replaceMagicNumbers(const CodeRange& scope, bool createConstants = true);
    RefactoringResult convertLoopType(const CodeRange& loopRange, const QString& targetType);
    RefactoringResult convertConditional(const CodeRange& condRange, bool useternary);
    RefactoringResult optimizeIncludes(const QString& filePath);
    RefactoringResult removeUnusedCode(const QString& filePath);
    RefactoringResult removeDeadCode(const QString& filePath);

    // Analysis and suggestions
    QList<RefactoringOperation> suggestRefactorings(const CodeRange& range);
    QList<QString> findRefactoringOpportunities(const QString& filePath);
    bool canRefactor(RefactoringType type, const CodeRange& range);
    QStringList getRequiredParameters(RefactoringType type);

    // Symbol analysis
    SymbolInfo analyzeSymbol(const QString& symbolName, const CodeRange& context);
    QList<SymbolInfo> findSymbolReferences(const QString& symbolName, SymbolType type);
    QList<QString> findSymbolUsages(const QString& symbolName, const QString& filePath);
    bool isSymbolUnused(const QString& symbolName, const QString& filePath);

    // Dependency analysis
    QStringList analyzeFileDependencies(const QString& filePath);
    QList<QString> findCircularDependencies(const QString& filePath);
    bool canMoveSymbol(const QString& symbolName, const QString& targetFile);

    // Undo/Redo
    bool undoRefactoring(const QString& operationId);
    bool redoRefactoring(const QString& operationId);
    RefactoringOperation getLastOperation() const;
    QList<RefactoringOperation> getOperationHistory() const;
    void clearHistory();

    // Configuration
    void setAutoDetectParameters(bool enable);
    void setPreserveComments(bool preserve);
    void setAutoFormat(bool format);
    void setCreateBackups(bool backup);
    void setMaxHistorySize(int size);

    // Validation
    bool validateSymbolName(const QString& name, SymbolType type) const;
    bool validateMethodSignature(const MethodSignature& signature) const;
    QStringList getNameConflicts(const QString& newName, const CodeRange& scope) const;

    // Statistics
    int getTotalRefactorings() const;
    int getSuccessfulRefactorings() const;
    int getFailedRefactorings() const;
    QMap<RefactoringType, int> getRefactoringsByType() const;
    QJsonObject getStatistics() const;

signals:
    // Refactoring signals
    void refactoringStarted(const RefactoringOperation& operation);
    void refactoringCompleted(const RefactoringResult& result);
    void refactoringFailed(const QString& reason);
    void refactoringProgress(int percentage, const QString& status);
    
    // Symbol signals
    void symbolRenamed(const QString& oldName, const QString& newName);
    void symbolMoved(const QString& symbolName, const QString& targetFile);
    void unusedSymbolDetected(const QString& symbolName, const QString& filePath);
    
    // Validation signals
    void nameConflictDetected(const QString& name, const QStringList& conflicts);
    void circularDependencyDetected(const QStringList& files);
    
    // Suggestion signals
    void refactoringSuggested(const RefactoringOperation& suggestion);
    void codeSmellDetected(const QString& filePath, const QString& description);

private:
    // Code parsing
    QString parseSymbolType(const QString& code, int position);
    QList<SymbolInfo> parseSymbols(const QString& code, const QString& filePath);
    MethodSignature parseMethodSignature(const QString& code, int startPos);
    QStringList parseMethodParameters(const QString& code, int startPos);
    QString parseMethodBody(const QString& code, int startPos);
    
    // Extract helpers
    QStringList detectMethodParameters(const QString& code, const CodeRange& range);
    QString generateMethodCall(const QString& methodName, const QStringList& params);
    QString generateMethodDefinition(const QString& methodName, const QString& returnType,
                                    const QStringList& params, const QString& body);
    
    // Inline helpers
    QMap<QString, QString> findVariableUsages(const QString& varName, const QString& code);
    QString replaceVariableUsages(const QString& code, const QString& varName,
                                 const QString& replacement);
    bool isVariableSafeToInline(const QString& varName, const QString& code);
    
    // Rename helpers
    QList<CodeRange> findAllOccurrences(const QString& symbolName, const QString& filePath);
    QString replaceSymbolInCode(const QString& code, const QString& oldName,
                               const QString& newName, SymbolType type);
    bool isRenameSafe(const QString& oldName, const QString& newName, const CodeRange& scope);
    
    // Move helpers
    QString extractClassDefinition(const QString& code, const QString& className);
    QString removeClassDefinition(const QString& code, const QString& className);
    QString updateIncludePaths(const QString& code, const QString& oldPath,
                              const QString& newPath);
    
    // Code analysis
    bool hasMagicNumbers(const QString& code);
    QList<int> findMagicNumbers(const QString& code);
    QString detectLoopType(const QString& code, const CodeRange& range);
    bool isCodeUnreachable(const QString& code, int line);
    
    // Dependency helpers
    QStringList parseIncludes(const QString& code);
    QStringList findSymbolDependencies(const QString& symbolName, const QString& code);
    bool createsDependencyCycle(const QString& sourceFile, const QString& targetFile);
    
    // File operations
    QString readFile(const QString& filePath) const;
    bool writeFile(const QString& filePath, const QString& content);
    QString createBackup(const QString& filePath);
    bool restoreBackup(const QString& backupPath);
    
    // Formatting
    QString formatCode(const QString& code, const QString& language = "cpp");
    QString preserveIndentation(const QString& code, int baseIndent);
    
    // History management
    void recordOperation(const RefactoringOperation& operation);
    void updateStatistics(const RefactoringResult& result);

    // Member variables
    QList<RefactoringOperation> m_operationHistory;
    QList<RefactoringOperation> m_redoStack;
    QMap<QString, QString> m_backups;  // filePath -> backupPath
    QMap<RefactoringType, int> m_refactoringCounts;
    
    bool m_autoDetectParameters;
    bool m_preserveComments;
    bool m_autoFormat;
    bool m_createBackups;
    int m_maxHistorySize;
    
    int m_totalRefactorings;
    int m_successfulRefactorings;
    int m_failedRefactorings;
    
    mutable QMutex m_mutex;
};

#endif // ADVANCED_REFACTORING_H
