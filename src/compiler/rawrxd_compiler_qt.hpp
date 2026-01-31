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
    std::string message;
    std::string filePath;
    int line;
    int column;
    int endLine;
    int endColumn;
    std::string code;           // Error/warning code
    std::string source;         // Source of diagnostic (lexer, parser, etc.)
    std::stringList suggestions; // Fix suggestions
    // DateTime timestamp;
    
    void* toJson() const {
        return void*{
            {"severity", static_cast<int>(severity)},
            {"message", message},
            {"file", filePath},
            {"line", line},
            {"column", column},
            {"code", code},
            {"source", source},
            {"timestamp", timestamp.toString(ISODate)}
        };
    }
};

/**
 * @brief Compilation options
 */
struct CompileOptions {
    std::string inputFile;
    std::string outputFile;
    TargetArch target = TargetArch::Auto;
    OutputFormat format = OutputFormat::Executable;
    OptimizationLevel optimization = OptimizationLevel::Standard;
    bool debugInfo = false;
    bool verbose = false;
    bool warningsAsErrors = false;
    bool emitIR = false;
    bool emitASM = false;
    std::stringList includePaths;
    std::stringList libraryPaths;
    std::stringList libraries;
    std::stringList defines;
    std::stringList extraFlags;
    
    void* toJson() const {
        return void*{
            {"input", inputFile},
            {"output", outputFile},
            {"target", static_cast<int>(target)},
            {"format", static_cast<int>(format)},
            {"optimization", static_cast<int>(optimization)},
            {"debug", debugInfo},
            {"verbose", verbose}
        };
    }
    
    static CompileOptions fromJson(const void*& json) {
        CompileOptions opts;
        opts.inputFile = json["input"].toString();
        opts.outputFile = json["output"].toString();
        opts.target = static_cast<TargetArch>(json["target"]);
        opts.format = static_cast<OutputFormat>(json["format"]);
        opts.optimization = static_cast<OptimizationLevel>(json["optimization"]);
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
    std::string outputFile;
    std::vector<Diagnostic> diagnostics;
    int64_t compilationTimeMs = 0;
    int64_t inputSize = 0;
    int64_t outputSize = 0;
    int tokenCount = 0;
    int astNodeCount = 0;
    int irInstructionCount = 0;
    CompilationStage lastStage = CompilationStage::Idle;
    std::string errorMessage;
    
    int errorCount() const {
        return std::count_if(diagnostics.begin(), diagnostics.end(),
            [](const Diagnostic& d) { return d.severity >= DiagnosticSeverity::Error; });
    }
    
    int warningCount() const {
        return std::count_if(diagnostics.begin(), diagnostics.end(),
            [](const Diagnostic& d) { return d.severity == DiagnosticSeverity::Warning; });
    }
    
    void* toJson() const {
        void* diagArray;
        for (const auto& d : diagnostics) {
            diagArray.append(d.toJson());
        }
        return void*{
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
    std::string text;
    std::string category; // keyword, literal, operator, etc.
};

/**
 * @brief AST node information
 */
struct ASTNode {
    int type;
    std::string name;
    std::string typeName;
    int line;
    int column;
    std::vector<QSharedPointer<ASTNode>> children;
    std::map<std::string, std::any> attributes;
};

/**
 * @brief Symbol information
 */
struct Symbol {
    std::string name;
    std::string type;
    std::string scope;
    std::string kind; // function, variable, class, etc.
    int definitionLine;
    int definitionColumn;
    std::string filePath;
    std::stringList references;
};

// ============================================================================
// CompilerEngine - Main Compiler Interface
// ============================================================================

/**
 * @brief Main compiler engine with full compilation pipeline
 */
class CompilerEngine  {public:
    explicit CompilerEngine( = nullptr);
    ~CompilerEngine() override;
    
    // === Synchronous Compilation ===
    
    /**
     * @brief Compile source file synchronously
     */
    CompileResult compile(const CompileOptions& options);
    
    /**
     * @brief Compile source string synchronously
     */
    CompileResult compileString(const std::string& source, const CompileOptions& options);
    
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
    std::vector<Token> tokenize(const std::string& source, SourceLanguage language);
    
    /**
     * @brief Get tokens for syntax highlighting
     */
    std::vector<Token> getHighlightTokens(const std::string& source, SourceLanguage language);
    
    // === Parsing ===
    
    /**
     * @brief Parse source code to AST
     */
    QSharedPointer<ASTNode> parse(const std::string& source, SourceLanguage language);
    
    /**
     * @brief Get document outline from AST
     */
    std::vector<Symbol> getOutline(const std::string& source, SourceLanguage language);
    
    // === Semantic Analysis ===
    
    /**
     * @brief Analyze semantics and get diagnostics
     */
    std::vector<Diagnostic> analyze(const std::string& source, SourceLanguage language);
    
    /**
     * @brief Get symbol at position
     */
    Symbol getSymbolAt(const std::string& source, int line, int column);
    
    /**
     * @brief Find all references to symbol
     */
    std::vector<Symbol> findReferences(const std::string& source, const std::string& symbolName);
    
    // === Code Generation ===
    
    /**
     * @brief Generate intermediate representation
     */
    std::string generateIR(const std::string& source, SourceLanguage language);
    
    /**
     * @brief Generate assembly code
     */
    std::string generateAssembly(const std::string& source, TargetArch target);
    
    // === Language Detection ===
    
    /**
     * @brief Detect source language from file extension
     */
    static SourceLanguage detectLanguage(const std::string& filePath);
    
    /**
     * @brief Get file extension for language
     */
    static std::string getExtension(SourceLanguage language);
    
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
    static std::stringList supportedLanguages();
    
    /**
     * @brief Get compiler version
     */
    static std::string version() { return std::stringLiteral("1.0.0"); }
    \npublic:\n    // === Compilation Signals ===
    void compilationStarted(const CompileOptions& options);
    void compilationProgress(CompilationStage stage, int percent, const std::string& message);
    void compilationDiagnostic(const Diagnostic& diagnostic);
    void compilationFinished(const CompileResult& result);
    void compilationCancelled();
    
    // === Analysis Signals ===
    void tokensReady(const std::vector<Token>& tokens);
    void astReady(QSharedPointer<ASTNode> root);
    void diagnosticsReady(const std::vector<Diagnostic>& diagnostics);
    void symbolsReady(const std::vector<Symbol>& symbols);
    
    // === Status Signals ===
    void statusMessage(const std::string& message);
    void errorOccurred(const std::string& error);
\nprivate:\n    void onWorkerProgress(int percent, const std::string& message);
    void onWorkerFinished(const CompileResult& result);
    void onWorkerError(const std::string& error);

private:
    // Core compilation pipeline
    CompileResult runPipeline(const std::string& source, const CompileOptions& options);
    bool runLexer(const std::string& source, CompileResult& result);
    bool runParser(CompileResult& result);
    bool runSemanticAnalysis(CompileResult& result);
    bool runIRGeneration(CompileResult& result);
    bool runOptimization(CompileResult& result, OptimizationLevel level);
    bool runCodeGeneration(CompileResult& result, TargetArch target);
    bool runAssembler(CompileResult& result);
    bool runLinker(CompileResult& result, OutputFormat format, const std::string& outputFile);
    
    // Helper functions
    void emitProgress(CompilationStage stage, int percent, const std::string& message);
    void addDiagnostic(DiagnosticSeverity severity, const std::string& message, 
                       int line, int column, CompileResult& result);
    std::string readFile(const std::string& path);
    bool writeFile(const std::string& path, const std::vector<uint8_t>& data);
    
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
    void collectSymbols(ASTNode* node, const std::string& scope);
    bool checkSemantics(ASTNode* node, CompileResult& result);
    
    // IR generation helper methods
    void generateIRForFunction(ASTNode* func, std::stringstream& ir);
    void generateIRForBlock(ASTNode* block, std::stringstream& ir, int indent);
    void generateIRForStatement(ASTNode* stmt, std::stringstream& ir, int indent);
    std::string generateIRForExpression(ASTNode* expr, std::stringstream& ir, int indent);
    
    // Optimization methods
    void optimizeConstantFolding();
    void optimizeDeadCodeElimination();
    void optimizeInlining();
    
    // Code generation methods
    void generateX86_64Assembly(std::stringstream& out);
    void generateX86_32Assembly(std::stringstream& out);
    void generateARM64Assembly(std::stringstream& out);
    std::string resolveOperand(const std::string& op, const std::map<std::string, std::string>& regMap);
    
    // State
    std::atomic<bool> m_isCompiling{false};
    std::atomic<bool> m_cancelRequested{false};
    std::thread* m_workerThread = nullptr;
    CompilerWorker* m_worker = nullptr;
    std::mutex m_mutex;
    
    // Configuration
    TargetArch m_defaultTarget = TargetArch::Auto;
    OptimizationLevel m_defaultOptLevel = OptimizationLevel::Standard;
    bool m_verbose = false;
    
    // Internal buffers
    std::vector<Token> m_tokens;
    QSharedPointer<ASTNode> m_astRoot;
    std::vector<Symbol> m_symbols;
    std::vector<uint8_t> m_irBuffer;
    std::vector<uint8_t> m_codeBuffer;
    std::vector<uint8_t> m_outputBuffer;
    
    // Code generation state
    int m_tempCounter = 0;
    int m_labelCounter = 0;
    int m_stringCounter = 0;
    std::map<std::string, std::string> m_varMap;
    std::map<std::string, std::string> m_strings;
};

// ============================================================================
// CompilerWorker - Background Compilation Thread
// ============================================================================

class CompilerWorker  {public:
    explicit CompilerWorker( = nullptr);
    \npublic:\n    void compile(const CompileOptions& options, const std::string& source);
    void cancel();
    \npublic:\n    void progress(int percent, const std::string& message);
    void finished(const CompileResult& result);
    void error(const std::string& message);
    
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
    std::string name;
    TargetArch target = TargetArch::Auto;
    OutputFormat format = OutputFormat::Executable;
    OptimizationLevel optimization = OptimizationLevel::Standard;
    bool debugInfo = true;
    std::stringList defines;
    std::stringList includePaths;
    std::stringList libraryPaths;
    std::stringList libraries;
    std::string outputDirectory;
    std::string intermediateDirectory;
    std::stringList extraCompilerFlags;
    std::stringList extraLinkerFlags;
    
    void* toJson() const;
    static BuildConfiguration fromJson(const void*& json);
};

/**
 * @brief Project file entry
 */
struct ProjectFile {
    std::string path;
    std::string relativePath;
    SourceLanguage language;
    bool compile = true;
    std::stringList dependencies;
    
    void* toJson() const;
    static ProjectFile fromJson(const void*& json);
};

/**
 * @brief Project manager for multi-file projects
 */
class ProjectManager  {public:
    explicit ProjectManager( = nullptr);
    ~ProjectManager() override;
    
    // === Project Operations ===
    bool createProject(const std::string& name, const std::string& directory, SourceLanguage primaryLanguage);
    bool openProject(const std::string& projectFile);
    bool saveProject();
    bool closeProject();
    bool isProjectOpen() const { return m_projectOpen; }
    
    // === File Management ===
    bool addFile(const std::string& filePath);
    bool removeFile(const std::string& filePath);
    bool renameFile(const std::string& oldPath, const std::string& newPath);
    std::vector<ProjectFile> getFiles() const { return m_files; }
    std::vector<ProjectFile> getSourceFiles() const;
    
    // === Build Configurations ===
    void addConfiguration(const BuildConfiguration& config);
    void removeConfiguration(const std::string& name);
    void setActiveConfiguration(const std::string& name);
    BuildConfiguration getActiveConfiguration() const;
    std::vector<BuildConfiguration> getConfigurations() const { return m_configurations; }
    
    // === Building ===
    void build();
    void rebuild();
    void clean();
    void cancelBuild();
    bool isBuilding() const { return m_building.load(); }
    
    // === Properties ===
    std::string projectName() const { return m_projectName; }
    std::string projectDirectory() const { return m_projectDirectory; }
    std::string projectFile() const { return m_projectFile; }
    \npublic:\n    void projectOpened(const std::string& name);
    void projectClosed();
    void projectModified();
    void fileAdded(const std::string& path);
    void fileRemoved(const std::string& path);
    void buildStarted();
    void buildProgress(const std::string& file, int current, int total);
    void buildFinished(bool success, int errors, int warnings);
    void buildOutput(const std::string& message);
    void buildDiagnostic(const Diagnostic& diagnostic);
    
private:
    void loadProjectFile(const std::string& path);
    void saveProjectFile(const std::string& path);
    void buildFile(const ProjectFile& file, const BuildConfiguration& config);
    void linkProject(const BuildConfiguration& config);
    
    std::string m_projectName;
    std::string m_projectDirectory;
    std::string m_projectFile;
    bool m_projectOpen = false;
    bool m_modified = false;
    std::atomic<bool> m_building{false};
    std::atomic<bool> m_cancelBuild{false};
    
    std::vector<ProjectFile> m_files;
    std::vector<BuildConfiguration> m_configurations;
    std::string m_activeConfiguration;
    
    CompilerEngine* m_compiler = nullptr;
    std::vector<std::string> m_objectFiles;
    std::vector<Diagnostic> m_diagnostics;
};

// ============================================================================
// SyntaxHighlighter - Multi-Language Syntax Highlighting
// ============================================================================

/**
 * @brief Syntax highlighter with support for 48+ languages
 */
class SyntaxHighlighter {public:
    explicit SyntaxHighlighter(QTextDocument* parent = nullptr);
    
    void setLanguage(SourceLanguage language);
    SourceLanguage language() const { return m_language; }
    
    void setTheme(const std::string& theme);
    std::string theme() const { return m_theme; }
    
protected:
    void highlightBlock(const std::string& text) override;
    
private:
    void setupFormats();
    void loadTheme(const std::string& theme);
    void highlightEon(const std::string& text);
    void highlightC(const std::string& text);
    void highlightCpp(const std::string& text);
    void highlightRust(const std::string& text);
    void highlightPython(const std::string& text);
    void highlightJavaScript(const std::string& text);
    void highlightGeneric(const std::string& text);
    
    struct HighlightRule {
        std::regex pattern;
        QTextCharFormat format;
    };
    
    SourceLanguage m_language = SourceLanguage::Unknown;
    std::string m_theme = "dark";
    std::vector<HighlightRule> m_rules;
    
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
class DiagnosticsManager  {public:
    explicit DiagnosticsManager( = nullptr);
    
    void clear();
    void addDiagnostic(const Diagnostic& diagnostic);
    void addDiagnostics(const std::vector<Diagnostic>& diagnostics);
    
    std::vector<Diagnostic> getAllDiagnostics() const { return m_diagnostics; }
    std::vector<Diagnostic> getDiagnosticsForFile(const std::string& filePath) const;
    std::vector<Diagnostic> getErrors() const;
    std::vector<Diagnostic> getWarnings() const;
    
    int errorCount() const;
    int warningCount() const;
    
    // UI Integration
    void setTreeWidget(QTreeWidget* tree) { m_treeWidget = tree; }
    void updateTreeWidget();
    \npublic:\n    void diagnosticAdded(const Diagnostic& diagnostic);
    void diagnosticsCleared();
    void diagnosticSelected(const Diagnostic& diagnostic);
    
private:
    std::vector<Diagnostic> m_diagnostics;
    QTreeWidget* m_treeWidget = nullptr;
};

// ============================================================================
// CompletionProvider - Code Completion
// ============================================================================

/**
 * @brief Code completion provider
 */
struct CompletionItem {
    std::string label;
    std::string insertText;
    std::string detail;
    std::string documentation;
    std::string kind; // function, variable, keyword, etc.
    int sortOrder = 0;
};

class CompletionProvider  {public:
    explicit CompletionProvider( = nullptr);
    
    void setCompiler(CompilerEngine* compiler) { m_compiler = compiler; }
    void setLanguage(SourceLanguage language) { m_language = language; }
    
    std::vector<CompletionItem> getCompletions(const std::string& source, int line, int column);
    std::vector<CompletionItem> getSignatureHelp(const std::string& source, int line, int column);
    
private:
    std::vector<CompletionItem> getKeywordCompletions();
    std::vector<CompletionItem> getSymbolCompletions(const std::string& source);
    std::vector<CompletionItem> getMemberCompletions(const std::string& source, const std::string& objectName);
    
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
    std::string filePath;
    int line;
    std::string condition;
    bool enabled = true;
    int hitCount = 0;
};

/**
 * @brief Stack frame information
 */
struct StackFrame {
    int level;
    std::string functionName;
    std::string filePath;
    int line;
    std::map<std::string, std::string> locals; // name -> value
};

/**
 * @brief Debugger interface for compiled programs
 */
class DebuggerInterface  {public:
    explicit DebuggerInterface( = nullptr);
    ~DebuggerInterface() override;
    
    // === Session Control ===
    bool startDebugging(const std::string& executable, const std::stringList& args = {});
    void stopDebugging();
    bool isDebugging() const { return m_debugging.load(); }
    
    // === Execution Control ===
    void continueExecution();
    void pause();
    void stepOver();
    void stepInto();
    void stepOut();
    void runToCursor(const std::string& file, int line);
    
    // === Breakpoints ===
    int addBreakpoint(const std::string& file, int line, const std::string& condition = {});
    void removeBreakpoint(int id);
    void enableBreakpoint(int id, bool enable);
    std::vector<Breakpoint> getBreakpoints() const { return m_breakpoints; }
    
    // === Inspection ===
    std::vector<StackFrame> getCallStack();
    std::map<std::string, std::string> getLocals();
    std::string evaluateExpression(const std::string& expression);
    \npublic:\n    void debuggingStarted();
    void debuggingStopped();
    void breakpointHit(const Breakpoint& bp, const StackFrame& frame);
    void stepped(const StackFrame& frame);
    void outputReceived(const std::string& output);
    void errorReceived(const std::string& error);
    
private:
    void sendCommand(const std::string& command);
    void parseResponse(const std::string& response);
    
    std::atomic<bool> m_debugging{false};
    void** m_process = nullptr;
    std::vector<Breakpoint> m_breakpoints;
    int m_nextBreakpointId = 1;
};

// ============================================================================
// CompilerWidget - Main IDE Widget
// ============================================================================

/**
 * @brief Main compiler widget for IDE integration
 */
class CompilerWidget {public:
    explicit CompilerWidget(void* parent = nullptr);
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
    void loadSettings(const void*& settings);
    void* saveSettings() const;
    \npublic:\n    void compilationRequested(const std::string& file);
    void buildRequested();
    void runRequested();
    void debugRequested();
    void diagnosticClicked(const std::string& file, int line, int column);
    \nprivate:\n    void onCompilationStarted(const CompileOptions& options);
    void onCompilationProgress(CompilationStage stage, int percent, const std::string& message);
    void onCompilationFinished(const CompileResult& result);
    void onBuildStarted();
    void onBuildProgress(const std::string& file, int current, int total);
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
    void* m_progressBar;
    QTreeWidget* m_diagnosticsTree;
    void* m_outputLog;
};

// ============================================================================
// Utility Functions
// ============================================================================

namespace Utils {

/**
 * @brief Get target architecture from string
 */
inline TargetArch parseTarget(const std::string& str) {
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
inline std::string targetToString(TargetArch target) {
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
inline std::string stageName(CompilationStage stage) {
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
inline std::string formatSize(int64_t bytes) {
    if (bytes < 1024) return std::string::number(bytes) + " B";
    if (bytes < 1024 * 1024) return std::string::number(bytes / 1024.0, 'f', 1) + " KB";
    if (bytes < 1024 * 1024 * 1024) return std::string::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
    return std::string::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
}

/**
 * @brief Format duration
 */
inline std::string formatDuration(int64_t ms) {
    if (ms < 1000) return std::string::number(ms) + " ms";
    if (ms < 60000) return std::string::number(ms / 1000.0, 'f', 2) + " s";
    return std::string::number(ms / 60000) + ":" + std::string::number((ms % 60000) / 1000).rightJustified(2, '0') + " min";
}

} // namespace Utils

} // namespace Compiler
} // namespace RawrXD

#endif // RAWRXD_COMPILER_QT_HPP

