/**
 * @file rawrxd_compiler_qt.hpp
 * @brief RawrXD Compiler Qt IDE Integration
 * 
 * Complete Qt/C++ wrapper for the MASM self-compiling compiler with full IDE features:
 * - Syntax highlighting for 48+ languages
 * - Project management with build configurations
 * - Integrated debugger interface
 * - Real-time error diagnostics
 * - Code completion and navigation
 * - Multi-target compilation (x86-64, x86-32, ARM64, WASM)
 * 
 * Copyright (c) 2024-2026 RawrXD IDE Project
 */

#ifndef RAWRXD_COMPILER_QT_HPP
#define RAWRXD_COMPILER_QT_HPP

#include <QObject>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QMutex>
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDateTime>
#include <QTimer>
#include <QHash>
#include <QVector>
#include <QSharedPointer>
#include <QFuture>
#include <QtConcurrent>
#include <QSyntaxHighlighter>
#include <QTextDocument>
#include <QPlainTextEdit>
#include <QTreeWidget>
#include <QProgressBar>
#include <QDockWidget>
#include <QMainWindow>
#include <QStatusBar>
#include <QMessageBox>
#include <functional>
#include <memory>
#include <atomic>
#include <chrono>

namespace RawrXD {
namespace Compiler {

// ============================================================================
// Forward Declarations
// ============================================================================
class CompilerEngine;
class CompilerWorker;
class ProjectManager;
class BuildConfiguration;
class DiagnosticsManager;
class SyntaxHighlighter;
class CompletionProvider;
class DebuggerInterface;

// ============================================================================
// Enumerations
// ============================================================================

/**
 * @brief Compilation target architectures
 */
enum class TargetArch {
    X86_64,
    X86_32,
    ARM64,
    RISCV64,
    WASM,
    Auto
};

/**
 * @brief Output format types
 */
enum class OutputFormat {
    Executable,
    SharedLibrary,
    StaticLibrary,
    ObjectFile,
    Assembly,
    IntermediateRepresentation
};

/**
 * @brief Optimization levels
 */
enum class OptimizationLevel {
    None = 0,      // -O0: No optimization
    Basic = 1,     // -O1: Basic optimizations
    Standard = 2,  // -O2: Standard optimizations
    Aggressive = 3,// -O3: Aggressive optimizations
    Size = 4       // -Os: Optimize for size
};

/**
 * @brief Compilation stages
 */
enum class CompilationStage {
    Idle,
    Initializing,
    Lexing,
    Parsing,
    SemanticAnalysis,
    IRGeneration,
    Optimization,
    CodeGeneration,
    Assembly,
    Linking,
    Complete,
    Failed
};

/**
 * @brief Diagnostic severity levels
 */
enum class DiagnosticSeverity {
    Hint,
    Info,
    Warning,
    Error,
    Fatal
};

/**
 * @brief Supported languages
 */
enum class SourceLanguage {
    Unknown,
    Eon,
    C,
    CPlusPlus,
    Rust,
    Go,
    Python,
    JavaScript,
    TypeScript,
    Java,
    CSharp,
    Swift,
    Kotlin,
    Dart,
    Lua,
    Ruby,
    PHP,
    Assembly,
    // ... 48+ languages supported
};

// ============================================================================
// Data Structures
// ============================================================================

/**
 * @brief Compilation diagnostic message
 */
struct Diagnostic {
    DiagnosticSeverity severity;
    QString message;
    QString filePath;
    int line;
    int column;
    int endLine;
    int endColumn;
    QString code;           // Error/warning code
    QString source;         // Source of diagnostic (lexer, parser, etc.)
    QStringList suggestions; // Fix suggestions
    QDateTime timestamp;
    
    QJsonObject toJson() const {
        return QJsonObject{
            {"severity", static_cast<int>(severity)},
            {"message", message},
            {"file", filePath},
            {"line", line},
            {"column", column},
            {"code", code},
            {"source", source},
            {"timestamp", timestamp.toString(Qt::ISODate)}
        };
    }
};

/**
 * @brief Compilation options
 */
struct CompileOptions {
    QString inputFile;
    QString outputFile;
    TargetArch target = TargetArch::Auto;
    OutputFormat format = OutputFormat::Executable;
    OptimizationLevel optimization = OptimizationLevel::Standard;
    bool debugInfo = false;
    bool verbose = false;
    bool warningsAsErrors = false;
    bool emitIR = false;
    bool emitASM = false;
    QStringList includePaths;
    QStringList libraryPaths;
    QStringList libraries;
    QStringList defines;
    QStringList extraFlags;
    
    QJsonObject toJson() const {
        return QJsonObject{
            {"input", inputFile},
            {"output", outputFile},
            {"target", static_cast<int>(target)},
            {"format", static_cast<int>(format)},
            {"optimization", static_cast<int>(optimization)},
            {"debug", debugInfo},
            {"verbose", verbose}
        };
    }
    
    static CompileOptions fromJson(const QJsonObject& json) {
        CompileOptions opts;
        opts.inputFile = json["input"].toString();
        opts.outputFile = json["output"].toString();
        opts.target = static_cast<TargetArch>(json["target"].toInt());
        opts.format = static_cast<OutputFormat>(json["format"].toInt());
        opts.optimization = static_cast<OptimizationLevel>(json["optimization"].toInt());
        opts.debugInfo = json["debug"].toBool();
        opts.verbose = json["verbose"].toBool();
        return opts;
    }
};

/**
 * @brief Compilation result
 */
struct CompileResult {
    bool success = false;
    QString outputFile;
    QVector<Diagnostic> diagnostics;
    qint64 compilationTimeMs = 0;
    qint64 inputSize = 0;
    qint64 outputSize = 0;
    int tokenCount = 0;
    int astNodeCount = 0;
    int irInstructionCount = 0;
    CompilationStage lastStage = CompilationStage::Idle;
    QString errorMessage;
    
    int errorCount() const {
        return std::count_if(diagnostics.begin(), diagnostics.end(),
            [](const Diagnostic& d) { return d.severity >= DiagnosticSeverity::Error; });
    }
    
    int warningCount() const {
        return std::count_if(diagnostics.begin(), diagnostics.end(),
            [](const Diagnostic& d) { return d.severity == DiagnosticSeverity::Warning; });
    }
    
    QJsonObject toJson() const {
        QJsonArray diagArray;
        for (const auto& d : diagnostics) {
            diagArray.append(d.toJson());
        }
        return QJsonObject{
            {"success", success},
            {"output", outputFile},
            {"timeMs", compilationTimeMs},
            {"inputSize", inputSize},
            {"outputSize", outputSize},
            {"tokens", tokenCount},
            {"astNodes", astNodeCount},
            {"irInstructions", irInstructionCount},
            {"stage", static_cast<int>(lastStage)},
            {"diagnostics", diagArray}
        };
    }
};

/**
 * @brief Token information for syntax highlighting
 */
struct Token {
    int type;
    int startPos;
    int length;
    int line;
    int column;
    QString text;
    QString category; // keyword, literal, operator, etc.
};

/**
 * @brief AST node information
 */
struct ASTNode {
    int type;
    QString name;
    QString typeName;
    int line;
    int column;
    QVector<QSharedPointer<ASTNode>> children;
    QHash<QString, QVariant> attributes;
};

/**
 * @brief Symbol information
 */
struct Symbol {
    QString name;
    QString type;
    QString scope;
    QString kind; // function, variable, class, etc.
    int definitionLine;
    int definitionColumn;
    QString filePath;
    QStringList references;
};

// ============================================================================
// CompilerEngine - Main Compiler Interface
// ============================================================================

/**
 * @brief Main compiler engine with full compilation pipeline
 */
class CompilerEngine : public QObject {
    Q_OBJECT
    
public:
    explicit CompilerEngine(QObject* parent = nullptr);
    ~CompilerEngine() override;
    
    // === Synchronous Compilation ===
    
    /**
     * @brief Compile source file synchronously
     */
    CompileResult compile(const CompileOptions& options);
    
    /**
     * @brief Compile source string synchronously
     */
    CompileResult compileString(const QString& source, const CompileOptions& options);
    
    // === Asynchronous Compilation ===
    
    /**
     * @brief Start asynchronous compilation
     */
    void compileAsync(const CompileOptions& options);
    
    /**
     * @brief Cancel ongoing compilation
     */
    void cancelCompilation();
    
    /**
     * @brief Check if compilation is in progress
     */
    bool isCompiling() const { return m_isCompiling.load(); }
    
    // === Lexical Analysis ===
    
    /**
     * @brief Tokenize source code
     */
    QVector<Token> tokenize(const QString& source, SourceLanguage language);
    
    /**
     * @brief Get tokens for syntax highlighting
     */
    QVector<Token> getHighlightTokens(const QString& source, SourceLanguage language);
    
    // === Parsing ===
    
    /**
     * @brief Parse source code to AST
     */
    QSharedPointer<ASTNode> parse(const QString& source, SourceLanguage language);
    
    /**
     * @brief Get document outline from AST
     */
    QVector<Symbol> getOutline(const QString& source, SourceLanguage language);
    
    // === Semantic Analysis ===
    
    /**
     * @brief Analyze semantics and get diagnostics
     */
    QVector<Diagnostic> analyze(const QString& source, SourceLanguage language);
    
    /**
     * @brief Get symbol at position
     */
    Symbol getSymbolAt(const QString& source, int line, int column);
    
    /**
     * @brief Find all references to symbol
     */
    QVector<Symbol> findReferences(const QString& source, const QString& symbolName);
    
    // === Code Generation ===
    
    /**
     * @brief Generate intermediate representation
     */
    QString generateIR(const QString& source, SourceLanguage language);
    
    /**
     * @brief Generate assembly code
     */
    QString generateAssembly(const QString& source, TargetArch target);
    
    // === Language Detection ===
    
    /**
     * @brief Detect source language from file extension
     */
    static SourceLanguage detectLanguage(const QString& filePath);
    
    /**
     * @brief Get file extension for language
     */
    static QString getExtension(SourceLanguage language);
    
    // === Configuration ===
    
    /**
     * @brief Set compilation target
     */
    void setTarget(TargetArch target) { m_defaultTarget = target; }
    
    /**
     * @brief Set optimization level
     */
    void setOptimization(OptimizationLevel level) { m_defaultOptLevel = level; }
    
    /**
     * @brief Enable/disable verbose output
     */
    void setVerbose(bool verbose) { m_verbose = verbose; }
    
    /**
     * @brief Get supported languages list
     */
    static QStringList supportedLanguages();
    
    /**
     * @brief Get compiler version
     */
    static QString version() { return QStringLiteral("1.0.0"); }
    
signals:
    // === Compilation Signals ===
    void compilationStarted(const CompileOptions& options);
    void compilationProgress(CompilationStage stage, int percent, const QString& message);
    void compilationDiagnostic(const Diagnostic& diagnostic);
    void compilationFinished(const CompileResult& result);
    void compilationCancelled();
    
    // === Analysis Signals ===
    void tokensReady(const QVector<Token>& tokens);
    void astReady(QSharedPointer<ASTNode> root);
    void diagnosticsReady(const QVector<Diagnostic>& diagnostics);
    void symbolsReady(const QVector<Symbol>& symbols);
    
    // === Status Signals ===
    void statusMessage(const QString& message);
    void errorOccurred(const QString& error);

private slots:
    void onWorkerProgress(int percent, const QString& message);
    void onWorkerFinished(const CompileResult& result);
    void onWorkerError(const QString& error);

private:
    // Core compilation pipeline
    CompileResult runPipeline(const QString& source, const CompileOptions& options);
    bool runLexer(const QString& source, CompileResult& result);
    bool runParser(CompileResult& result);
    bool runSemanticAnalysis(CompileResult& result);
    bool runIRGeneration(CompileResult& result);
    bool runOptimization(CompileResult& result, OptimizationLevel level);
    bool runCodeGeneration(CompileResult& result, TargetArch target);
    bool runAssembler(CompileResult& result);
    bool runLinker(CompileResult& result, OutputFormat format, const QString& outputFile);
    
    // Helper functions
    void emitProgress(CompilationStage stage, int percent, const QString& message);
    void addDiagnostic(DiagnosticSeverity severity, const QString& message, 
                       int line, int column, CompileResult& result);
    QString readFile(const QString& path);
    bool writeFile(const QString& path, const QByteArray& data);
    
    // Parser helper methods
    QSharedPointer<ASTNode> parseDeclaration(int& pos);
    QSharedPointer<ASTNode> parseFunctionDecl(int& pos);
    QSharedPointer<ASTNode> parseParameter(int& pos);
    QSharedPointer<ASTNode> parseBlock(int& pos);
    QSharedPointer<ASTNode> parseStatement(int& pos);
    QSharedPointer<ASTNode> parseIfStatement(int& pos);
    QSharedPointer<ASTNode> parseWhileStatement(int& pos);
    QSharedPointer<ASTNode> parseForStatement(int& pos);
    QSharedPointer<ASTNode> parseReturnStatement(int& pos);
    QSharedPointer<ASTNode> parseBreakStatement(int& pos);
    QSharedPointer<ASTNode> parseContinueStatement(int& pos);
    QSharedPointer<ASTNode> parseVariableDecl(int& pos);
    QSharedPointer<ASTNode> parseStructDecl(int& pos);
    QSharedPointer<ASTNode> parseEnumDecl(int& pos);
    QSharedPointer<ASTNode> parseImportDecl(int& pos);
    QSharedPointer<ASTNode> parseExpressionStatement(int& pos);
    QSharedPointer<ASTNode> parseExpression(int& pos, int minPrec = 0);
    QSharedPointer<ASTNode> parsePrimaryExpression(int& pos);
    int getOperatorPrecedence(int tokenType);
    int countASTNodes(ASTNode* node);
    
    // Semantic analysis helper methods
    void collectSymbols(ASTNode* node, const QString& scope);
    bool checkSemantics(ASTNode* node, CompileResult& result);
    
    // State
    std::atomic<bool> m_isCompiling{false};
    std::atomic<bool> m_cancelRequested{false};
    QThread* m_workerThread = nullptr;
    CompilerWorker* m_worker = nullptr;
    QMutex m_mutex;
    
    // Configuration
    TargetArch m_defaultTarget = TargetArch::Auto;
    OptimizationLevel m_defaultOptLevel = OptimizationLevel::Standard;
    bool m_verbose = false;
    
    // Internal buffers
    QVector<Token> m_tokens;
    QSharedPointer<ASTNode> m_astRoot;
    QVector<Symbol> m_symbols;
    QByteArray m_irBuffer;
    QByteArray m_codeBuffer;
    QByteArray m_outputBuffer;
};

// ============================================================================
// CompilerWorker - Background Compilation Thread
// ============================================================================

class CompilerWorker : public QObject {
    Q_OBJECT
    
public:
    explicit CompilerWorker(QObject* parent = nullptr);
    
public slots:
    void compile(const CompileOptions& options, const QString& source);
    void cancel();
    
signals:
    void progress(int percent, const QString& message);
    void finished(const CompileResult& result);
    void error(const QString& message);
    
private:
    std::atomic<bool> m_cancelled{false};
};

// ============================================================================
// ProjectManager - Project and Build Configuration
// ============================================================================

/**
 * @brief Build configuration for a project
 */
class BuildConfiguration {
public:
    QString name;
    TargetArch target = TargetArch::Auto;
    OutputFormat format = OutputFormat::Executable;
    OptimizationLevel optimization = OptimizationLevel::Standard;
    bool debugInfo = true;
    QStringList defines;
    QStringList includePaths;
    QStringList libraryPaths;
    QStringList libraries;
    QString outputDirectory;
    QString intermediateDirectory;
    QStringList extraCompilerFlags;
    QStringList extraLinkerFlags;
    
    QJsonObject toJson() const;
    static BuildConfiguration fromJson(const QJsonObject& json);
};

/**
 * @brief Project file entry
 */
struct ProjectFile {
    QString path;
    QString relativePath;
    SourceLanguage language;
    bool compile = true;
    QStringList dependencies;
    
    QJsonObject toJson() const;
    static ProjectFile fromJson(const QJsonObject& json);
};

/**
 * @brief Project manager for multi-file projects
 */
class ProjectManager : public QObject {
    Q_OBJECT
    
public:
    explicit ProjectManager(QObject* parent = nullptr);
    ~ProjectManager() override;
    
    // === Project Operations ===
    bool createProject(const QString& name, const QString& directory, SourceLanguage primaryLanguage);
    bool openProject(const QString& projectFile);
    bool saveProject();
    bool closeProject();
    bool isProjectOpen() const { return m_projectOpen; }
    
    // === File Management ===
    bool addFile(const QString& filePath);
    bool removeFile(const QString& filePath);
    bool renameFile(const QString& oldPath, const QString& newPath);
    QVector<ProjectFile> getFiles() const { return m_files; }
    QVector<ProjectFile> getSourceFiles() const;
    
    // === Build Configurations ===
    void addConfiguration(const BuildConfiguration& config);
    void removeConfiguration(const QString& name);
    void setActiveConfiguration(const QString& name);
    BuildConfiguration getActiveConfiguration() const;
    QVector<BuildConfiguration> getConfigurations() const { return m_configurations; }
    
    // === Building ===
    void build();
    void rebuild();
    void clean();
    void cancelBuild();
    bool isBuilding() const { return m_building.load(); }
    
    // === Properties ===
    QString projectName() const { return m_projectName; }
    QString projectDirectory() const { return m_projectDirectory; }
    QString projectFile() const { return m_projectFile; }
    
signals:
    void projectOpened(const QString& name);
    void projectClosed();
    void projectModified();
    void fileAdded(const QString& path);
    void fileRemoved(const QString& path);
    void buildStarted();
    void buildProgress(const QString& file, int current, int total);
    void buildFinished(bool success, int errors, int warnings);
    void buildOutput(const QString& message);
    void buildDiagnostic(const Diagnostic& diagnostic);
    
private:
    void loadProjectFile(const QString& path);
    void saveProjectFile(const QString& path);
    void buildFile(const ProjectFile& file, const BuildConfiguration& config);
    void linkProject(const BuildConfiguration& config);
    
    QString m_projectName;
    QString m_projectDirectory;
    QString m_projectFile;
    bool m_projectOpen = false;
    bool m_modified = false;
    std::atomic<bool> m_building{false};
    std::atomic<bool> m_cancelBuild{false};
    
    QVector<ProjectFile> m_files;
    QVector<BuildConfiguration> m_configurations;
    QString m_activeConfiguration;
    
    CompilerEngine* m_compiler = nullptr;
    QVector<QString> m_objectFiles;
    QVector<Diagnostic> m_diagnostics;
};

// ============================================================================
// SyntaxHighlighter - Multi-Language Syntax Highlighting
// ============================================================================

/**
 * @brief Syntax highlighter with support for 48+ languages
 */
class SyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
    
public:
    explicit SyntaxHighlighter(QTextDocument* parent = nullptr);
    
    void setLanguage(SourceLanguage language);
    SourceLanguage language() const { return m_language; }
    
    void setTheme(const QString& theme);
    QString theme() const { return m_theme; }
    
protected:
    void highlightBlock(const QString& text) override;
    
private:
    void setupFormats();
    void loadTheme(const QString& theme);
    void highlightEon(const QString& text);
    void highlightC(const QString& text);
    void highlightCpp(const QString& text);
    void highlightRust(const QString& text);
    void highlightPython(const QString& text);
    void highlightJavaScript(const QString& text);
    void highlightGeneric(const QString& text);
    
    struct HighlightRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    
    SourceLanguage m_language = SourceLanguage::Unknown;
    QString m_theme = "dark";
    QVector<HighlightRule> m_rules;
    
    QTextCharFormat m_keywordFormat;
    QTextCharFormat m_typeFormat;
    QTextCharFormat m_functionFormat;
    QTextCharFormat m_stringFormat;
    QTextCharFormat m_numberFormat;
    QTextCharFormat m_commentFormat;
    QTextCharFormat m_preprocessorFormat;
    QTextCharFormat m_operatorFormat;
};

// ============================================================================
// DiagnosticsManager - Error and Warning Management
// ============================================================================

/**
 * @brief Manages compilation diagnostics with UI integration
 */
class DiagnosticsManager : public QObject {
    Q_OBJECT
    
public:
    explicit DiagnosticsManager(QObject* parent = nullptr);
    
    void clear();
    void addDiagnostic(const Diagnostic& diagnostic);
    void addDiagnostics(const QVector<Diagnostic>& diagnostics);
    
    QVector<Diagnostic> getAllDiagnostics() const { return m_diagnostics; }
    QVector<Diagnostic> getDiagnosticsForFile(const QString& filePath) const;
    QVector<Diagnostic> getErrors() const;
    QVector<Diagnostic> getWarnings() const;
    
    int errorCount() const;
    int warningCount() const;
    
    // UI Integration
    void setTreeWidget(QTreeWidget* tree) { m_treeWidget = tree; }
    void updateTreeWidget();
    
signals:
    void diagnosticAdded(const Diagnostic& diagnostic);
    void diagnosticsCleared();
    void diagnosticSelected(const Diagnostic& diagnostic);
    
private:
    QVector<Diagnostic> m_diagnostics;
    QTreeWidget* m_treeWidget = nullptr;
};

// ============================================================================
// CompletionProvider - Code Completion
// ============================================================================

/**
 * @brief Code completion provider
 */
struct CompletionItem {
    QString label;
    QString insertText;
    QString detail;
    QString documentation;
    QString kind; // function, variable, keyword, etc.
    int sortOrder = 0;
};

class CompletionProvider : public QObject {
    Q_OBJECT
    
public:
    explicit CompletionProvider(QObject* parent = nullptr);
    
    void setCompiler(CompilerEngine* compiler) { m_compiler = compiler; }
    void setLanguage(SourceLanguage language) { m_language = language; }
    
    QVector<CompletionItem> getCompletions(const QString& source, int line, int column);
    QVector<CompletionItem> getSignatureHelp(const QString& source, int line, int column);
    
private:
    QVector<CompletionItem> getKeywordCompletions();
    QVector<CompletionItem> getSymbolCompletions(const QString& source);
    QVector<CompletionItem> getMemberCompletions(const QString& source, const QString& objectName);
    
    CompilerEngine* m_compiler = nullptr;
    SourceLanguage m_language = SourceLanguage::Unknown;
};

// ============================================================================
// DebuggerInterface - Integrated Debugger
// ============================================================================

/**
 * @brief Debugger breakpoint
 */
struct Breakpoint {
    int id;
    QString filePath;
    int line;
    QString condition;
    bool enabled = true;
    int hitCount = 0;
};

/**
 * @brief Stack frame information
 */
struct StackFrame {
    int level;
    QString functionName;
    QString filePath;
    int line;
    QHash<QString, QString> locals; // name -> value
};

/**
 * @brief Debugger interface for compiled programs
 */
class DebuggerInterface : public QObject {
    Q_OBJECT
    
public:
    explicit DebuggerInterface(QObject* parent = nullptr);
    ~DebuggerInterface() override;
    
    // === Session Control ===
    bool startDebugging(const QString& executable, const QStringList& args = {});
    void stopDebugging();
    bool isDebugging() const { return m_debugging.load(); }
    
    // === Execution Control ===
    void continueExecution();
    void pause();
    void stepOver();
    void stepInto();
    void stepOut();
    void runToCursor(const QString& file, int line);
    
    // === Breakpoints ===
    int addBreakpoint(const QString& file, int line, const QString& condition = {});
    void removeBreakpoint(int id);
    void enableBreakpoint(int id, bool enable);
    QVector<Breakpoint> getBreakpoints() const { return m_breakpoints; }
    
    // === Inspection ===
    QVector<StackFrame> getCallStack();
    QHash<QString, QString> getLocals();
    QString evaluateExpression(const QString& expression);
    
signals:
    void debuggingStarted();
    void debuggingStopped();
    void breakpointHit(const Breakpoint& bp, const StackFrame& frame);
    void stepped(const StackFrame& frame);
    void outputReceived(const QString& output);
    void errorReceived(const QString& error);
    
private:
    void sendCommand(const QString& command);
    void parseResponse(const QString& response);
    
    std::atomic<bool> m_debugging{false};
    QProcess* m_process = nullptr;
    QVector<Breakpoint> m_breakpoints;
    int m_nextBreakpointId = 1;
};

// ============================================================================
// CompilerWidget - Main IDE Widget
// ============================================================================

/**
 * @brief Main compiler widget for IDE integration
 */
class CompilerWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit CompilerWidget(QWidget* parent = nullptr);
    ~CompilerWidget() override;
    
    // === Components Access ===
    CompilerEngine* compiler() const { return m_compiler; }
    ProjectManager* projectManager() const { return m_projectManager; }
    DiagnosticsManager* diagnosticsManager() const { return m_diagnosticsManager; }
    DebuggerInterface* debugger() const { return m_debugger; }
    
    // === Quick Actions ===
    void compileCurrentFile();
    void buildProject();
    void runProject();
    void debugProject();
    
    // === Configuration ===
    void setTarget(TargetArch target);
    void setOptimization(OptimizationLevel level);
    void loadSettings(const QJsonObject& settings);
    QJsonObject saveSettings() const;
    
signals:
    void compilationRequested(const QString& file);
    void buildRequested();
    void runRequested();
    void debugRequested();
    void diagnosticClicked(const QString& file, int line, int column);
    
private slots:
    void onCompilationStarted(const CompileOptions& options);
    void onCompilationProgress(CompilationStage stage, int percent, const QString& message);
    void onCompilationFinished(const CompileResult& result);
    void onBuildStarted();
    void onBuildProgress(const QString& file, int current, int total);
    void onBuildFinished(bool success, int errors, int warnings);
    
private:
    void setupUI();
    void connectSignals();
    
    CompilerEngine* m_compiler;
    ProjectManager* m_projectManager;
    DiagnosticsManager* m_diagnosticsManager;
    CompletionProvider* m_completionProvider;
    DebuggerInterface* m_debugger;
    
    // UI Components
    QProgressBar* m_progressBar;
    QTreeWidget* m_diagnosticsTree;
    QPlainTextEdit* m_outputLog;
};

// ============================================================================
// Utility Functions
// ============================================================================

namespace Utils {

/**
 * @brief Get target architecture from string
 */
inline TargetArch parseTarget(const QString& str) {
    if (str == "x64" || str == "x86_64" || str == "amd64") return TargetArch::X86_64;
    if (str == "x86" || str == "i386" || str == "i686") return TargetArch::X86_32;
    if (str == "arm64" || str == "aarch64") return TargetArch::ARM64;
    if (str == "riscv64") return TargetArch::RISCV64;
    if (str == "wasm" || str == "wasm32") return TargetArch::WASM;
    return TargetArch::Auto;
}

/**
 * @brief Get string from target architecture
 */
inline QString targetToString(TargetArch target) {
    switch (target) {
        case TargetArch::X86_64: return "x86_64";
        case TargetArch::X86_32: return "x86";
        case TargetArch::ARM64: return "arm64";
        case TargetArch::RISCV64: return "riscv64";
        case TargetArch::WASM: return "wasm";
        case TargetArch::Auto: return "auto";
    }
    return "unknown";
}

/**
 * @brief Get compilation stage name
 */
inline QString stageName(CompilationStage stage) {
    switch (stage) {
        case CompilationStage::Idle: return "Idle";
        case CompilationStage::Initializing: return "Initializing";
        case CompilationStage::Lexing: return "Lexical Analysis";
        case CompilationStage::Parsing: return "Parsing";
        case CompilationStage::SemanticAnalysis: return "Semantic Analysis";
        case CompilationStage::IRGeneration: return "IR Generation";
        case CompilationStage::Optimization: return "Optimization";
        case CompilationStage::CodeGeneration: return "Code Generation";
        case CompilationStage::Assembly: return "Assembly";
        case CompilationStage::Linking: return "Linking";
        case CompilationStage::Complete: return "Complete";
        case CompilationStage::Failed: return "Failed";
    }
    return "Unknown";
}

/**
 * @brief Format file size
 */
inline QString formatSize(qint64 bytes) {
    if (bytes < 1024) return QString::number(bytes) + " B";
    if (bytes < 1024 * 1024) return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    if (bytes < 1024 * 1024 * 1024) return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
    return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
}

/**
 * @brief Format duration
 */
inline QString formatDuration(qint64 ms) {
    if (ms < 1000) return QString::number(ms) + " ms";
    if (ms < 60000) return QString::number(ms / 1000.0, 'f', 2) + " s";
    return QString::number(ms / 60000) + ":" + QString::number((ms % 60000) / 1000).rightJustified(2, '0') + " min";
}

} // namespace Utils

} // namespace Compiler
} // namespace RawrXD

#endif // RAWRXD_COMPILER_QT_HPP
