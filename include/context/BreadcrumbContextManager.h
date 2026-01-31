#pragma once
/*  BreadcrumbContextManager.h  -  Comprehensive Context with Breadcrumb Navigation
    
    This system provides breadcrumb-style navigation and context tracking for:
    - Tools and executables
    - Symbols (functions, classes, variables)
    - Files and file hierarchies
    - Source control information
    - Screenshots and annotations
    - Inline instructions/documentation
    - Related files and dependencies
    - Open editors and buffers
    
    Designed to integrate with a custom drawing engine for rich visualization.
*/

#include <QString>
#include <QList>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <memory>
#include <functional>

namespace RawrXD {
namespace Context {

// Forward declarations
class DrawingContext;
class BreadcrumbRenderer;

// ============================================================================
// CONTEXT TYPES
// ============================================================================

enum class ContextType {
    Tool,               // External tool/executable
    Symbol,             // Code symbol (function, class, variable, etc.)
    File,               // File system path
    SourceControl,      // Git/version control info
    Screenshot,         // Visual capture with annotations
    Instruction,        // Embedded guidance/documentation
    Relationship,       // Dependencies/relationships
    OpenEditor,         // Currently open file in editor
};

enum class SymbolKind {
    Function,
    Class,
    Struct,
    Enum,
    Variable,
    Macro,
    Namespace,
    Interface,
    Module,
};

enum class SCStatus {
    Untracked,
    Modified,
    Staged,
    Committed,
    Pushed,
    Conflicted,
};

// ============================================================================
// DATA STRUCTURES
// ============================================================================

struct Breadcrumb {
    QString id;
    QString label;
    QString displayName;
    ContextType type;
    QString iconPath;
    bool isClickable;
    QJsonObject metadata;
    QDateTime timestamp;
    
    Breadcrumb() : isClickable(true), timestamp(QDateTime::currentDateTime()) {}
};

struct SymbolContext {
    QString name;
    SymbolKind kind;
    QString filePath;
    int lineNumber;
    int columnNumber;
    QString signature;
    QString documentation;
    QList<QString> relatedSymbols;
    QMap<QString, QString> attributes;
    int scopeStart;
    int scopeEnd;
    
    SymbolContext() : lineNumber(0), columnNumber(0), scopeStart(0), scopeEnd(0) {}
};

struct ToolContext {
    QString toolName;
    QString executablePath;
    QString version;
    QList<QString> arguments;
    QMap<QString, QString> environment;
    QString workingDirectory;
    QString description;
    bool isAvailable;
    
    ToolContext() : isAvailable(false) {}
};

struct FileContext {
    QString absolutePath;
    QString relativePath;
    QString fileName;
    QString fileExtension;
    qint64 fileSize;
    QDateTime lastModified;
    QList<QString> relatedFiles;
    QList<SymbolContext> containedSymbols;
    QString projectRoot;
    
    FileContext() : fileSize(0) {}
};

struct SourceControlContext {
    QString repository;
    QString branch;
    QString commitHash;
    QString commitMessage;
    QString author;
    QDateTime commitDate;
    SCStatus status;
    QList<QString> changedFiles;
    QString diff;
    int addedLines;
    int removedLines;
    
    SourceControlContext() : addedLines(0), removedLines(0), status(SCStatus::Untracked) {}
};

struct ScreenshotAnnotation {
    QString id;
    int x, y, width, height;
    QString text;
    QString highlightColor;
    QDateTime captureTime;
    
    ScreenshotAnnotation() : x(0), y(0), width(0), height(0) {}
};

struct InstructionBlock {
    QString id;
    QString title;
    QString content;
    QList<QString> tags;
    QString relatedFile;
    int lineNumber;
    bool isVisible;
    QString backgroundColor;
    QString borderColor;
    
    InstructionBlock() : lineNumber(0), isVisible(true) {}
};

struct RelationshipContext {
    QString sourceId;
    QString targetId;
    QString relationshipType;  // "depends_on", "calls", "references", "inherits", etc.
    QString description;
    QString strength;          // "strong", "weak", "optional"
    
    RelationshipContext() {}
};

struct OpenEditorContext {
    QString filePath;
    int cursorLine;
    int cursorColumn;
    int scrollPosition;
    QString selectedText;
    QList<int> breakpoints;
    bool isModified;
    QDateTime lastAccessed;
    
    OpenEditorContext() : cursorLine(0), cursorColumn(0), scrollPosition(0), isModified(false) {}
};

// ============================================================================
// BREADCRUMB CHAIN
// ============================================================================

class BreadcrumbChain {
public:
    BreadcrumbChain();
    ~BreadcrumbChain();
    
    // Navigation
    void push(const Breadcrumb& crumb);
    void pop();
    void jump(int index);
    void clear();
    
    // Query
    QList<Breadcrumb> getChain() const;
    Breadcrumb getCurrentBreadcrumb() const;
    int getCurrentIndex() const;
    int getChainLength() const;
    
    // Serialization
    QJsonArray toJSON() const;
    void fromJSON(const QJsonArray& json);
    
private:
    QList<Breadcrumb> m_chain;
    int m_currentIndex;
};

// ============================================================================
// CONTEXT MANAGER
// ============================================================================

class BreadcrumbContextManager : public QObject {
    Q_OBJECT
    
public:
    explicit BreadcrumbContextManager(QObject* parent = nullptr);
    ~BreadcrumbContextManager();
    
    // ========== INITIALIZATION ==========
    void initialize(const QString& workspacePath);
    void shutdown();
    
    // ========== TOOL CONTEXT ==========
    void registerTool(const QString& toolName, const ToolContext& context);
    ToolContext getTool(const QString& toolName) const;
    QList<ToolContext> getAllTools() const;
    void unregisterTool(const QString& toolName);
    
    // ========== SYMBOL CONTEXT ==========
    void registerSymbol(const QString& filePath, const SymbolContext& symbol);
    SymbolContext getSymbol(const QString& symbolName) const;
    QList<SymbolContext> getSymbolsInFile(const QString& filePath) const;
    QList<SymbolContext> findSymbolsByKind(SymbolKind kind) const;
    void updateSymbolUsage(const QString& symbolName);
    
    // ========== FILE CONTEXT ==========
    void registerFile(const QString& filePath);
    FileContext getFileContext(const QString& filePath) const;
    QList<FileContext> getRelatedFiles(const QString& filePath) const;
    void updateFileMetadata(const QString& filePath);
    QList<FileContext> searchFiles(const QString& pattern) const;
    
    // ========== SOURCE CONTROL CONTEXT ==========
    void updateSourceControlContext(const QString& repository);
    SourceControlContext getSourceControlContext() const;
    QList<QString> getChangedFiles() const;
    QString getLatestCommitInfo() const;
    void scanRepositoryStatus();
    
    // ========== SCREENSHOT CONTEXT ==========
    void captureScreenshot(const QString& filePath);
    void addScreenshotAnnotation(const ScreenshotAnnotation& annotation);
    QList<ScreenshotAnnotation> getScreenshotAnnotations(const QString& screenshotId) const;
    
    // ========== INSTRUCTION CONTEXT ==========
    void registerInstruction(const InstructionBlock& instruction);
    InstructionBlock getInstruction(const QString& instructionId) const;
    QList<InstructionBlock> getInstructionsForFile(const QString& filePath) const;
    QList<InstructionBlock> getAllInstructions() const;
    void toggleInstructionVisibility(const QString& instructionId);
    
    // ========== RELATIONSHIP CONTEXT ==========
    void registerRelationship(const RelationshipContext& relationship);
    QList<RelationshipContext> getRelationshipsFor(const QString& entityId) const;
    QList<QString> getDependencies(const QString& entityId) const;
    QList<QString> getDependents(const QString& entityId) const;
    
    // ========== OPEN EDITOR CONTEXT ==========
    void registerOpenEditor(const QString& filePath, const OpenEditorContext& editorCtx);
    void updateEditorState(const QString& filePath, int line, int column, const QString& selectedText = "");
    QList<OpenEditorContext> getOpenEditors() const;
    OpenEditorContext getEditorContext(const QString& filePath) const;
    void closeEditor(const QString& filePath);
    
    // ========== BREADCRUMB NAVIGATION ==========
    BreadcrumbChain& getBreadcrumbChain();
    void pushContextBreadcrumb(const ContextType& type, const QString& identifier);
    void navigateToBreadcrumb(int index);
    
    // ========== QUERYING & ANALYSIS ==========
    QJsonObject getCompleteContext(const QString& identifier) const;
    QJsonObject analyzeContextRelationships() const;
    QList<QString> getContextPath(const QString& target) const;
    QJsonObject generateContextReport() const;
    
    // ========== IMPORT/EXPORT ==========
    void exportContextToJSON(const QString& filePath) const;
    void importContextFromJSON(const QString& filePath);
    
    // ========== PERFORMANCE ==========
    void indexWorkspace();
    void rebuildIndices();
    double getIndexingProgress() const;
    
signals:
    void contextChanged(const QString& contextId);
    void toolRegistered(const QString& toolName);
    void symbolRegistered(const QString& symbolName);
    void fileRegistered(const QString& filePath);
    void sourceControlUpdated();
    void breadcrumbNavigated(int index);
    void indexingProgressChanged(double progress);
    void indexingComplete();
    
private:
    // Internal data structures
    QMap<QString, ToolContext> m_toolRegistry;
    QMap<QString, SymbolContext> m_symbolRegistry;
    QMap<QString, FileContext> m_fileRegistry;
    QMap<QString, QList<SymbolContext>> m_fileSymbols;
    QMap<QString, RelationshipContext> m_relationships;
    QMap<QString, InstructionBlock> m_instructions;
    QMap<QString, OpenEditorContext> m_openEditors;
    QMap<QString, QList<ScreenshotAnnotation>> m_screenshots;
    
    SourceControlContext m_scContext;
    BreadcrumbChain m_breadcrumbs;
    QString m_workspacePath;
    
    // Helper methods
    void scanSymbols(const QString& filePath);
    void analyzeFileRelationships();
    void cacheSymbolReferences();
};

} // namespace Context
} // namespace RawrXD
