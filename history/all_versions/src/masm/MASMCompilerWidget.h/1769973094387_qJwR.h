// ============================================================================
// MASMCompilerWidget.h
// Qt IDE Integration for MASM Self-Compiling Compiler
// Provides full IDE features: syntax highlighting, error markers, build output
// ============================================================================

#pragma once

#include <vector>
#include <string>
#include <map>
#include <memory>
#include <unordered_map>

typedef std::vector<std::string> stringList;

// Forward declarations
class MASMSyntaxHighlighter;
class MASMCodeEditor;
class MASMProjectExplorer;
class MASMBuildOutput;
class MASMSymbolBrowser;
class MASMDebugger;

// ============================================================================
// MASM Compilation Error Structure
// ============================================================================
struct MASMError {
    std::string filename;
    int line;
    int column;
    std::string errorType;      // "error", "warning", "info"
    std::string message;
    std::string sourceSnippet;
    
    MASMError() : line(0), column(0) {}
    MASMError(const std::string& file, int ln, int col, const std::string& type, const std::string& msg)
        : filename(file), line(ln), column(col), errorType(type), message(msg) {}
};

// ============================================================================
// MASM Symbol Information
// ============================================================================
struct MASMSymbol {
    std::string name;
    std::string type;           // "label", "proc", "macro", "constant"
    std::string section;        // ".data", ".code", etc.
    int line;
    int64_t address;
    std::string signature;      // For procedures: parameter list
    
    MASMSymbol() : line(0), address(0) {}
};

// ============================================================================
// MASM Project Settings
// ============================================================================
struct MASMProjectSettings {
    std::string projectName;
    std::string projectPath;
    std::string outputPath;
    std::string mainFile;
    std::stringList sourceFiles;
    std::stringList includePaths;
    std::stringList libraries;
    std::string targetArchitecture;     // "x86", "x64", "arm64"
    std::string outputFormat;           // "exe", "dll", "lib"
    int optimizationLevel;          // 0-3
    bool generateDebugInfo;
    bool warnings;
    std::stringList defines;
    
    MASMProjectSettings()
        : targetArchitecture("x64")
        , outputFormat("exe")
        , optimizationLevel(2)
        , generateDebugInfo(true)
        , warnings(true) {}
    
    void save(std::map<std::string, std::string>& settings) const;
    void load(std::map<std::string, std::string>& settings);
};

// ============================================================================
// MASM Compilation Statistics
// ============================================================================
struct MASMCompilationStats {
    int64_t startTime;
    int64_t endTime;
    int sourceLines;
    int tokenCount;
    int astNodeCount;
    int symbolCount;
    int machineCodeSize;
    int errorCount;
    int warningCount;
    std::string stage;          // Current compilation stage
    
    MASMCompilationStats() { reset(); }
    
    void reset() {
        startTime = endTime = 0;
        sourceLines = tokenCount = astNodeCount = symbolCount = 0;
        machineCodeSize = errorCount = warningCount = 0;
        stage = "Idle";
    }
    
    int64_t duration() const { return endTime - startTime; }
};

// ============================================================================
// MASM Code Editor with Enhanced Features
// ============================================================================
class MASMCodeEditor {public:
    explicit MASMCodeEditor(void* parent = nullptr);
    ~MASMCodeEditor();
    
    // Line number management
    void lineNumberAreaPaintEvent(void* event);
    int lineNumberAreaWidth();
    
    // Error markers
    void setErrors(const std::vector<MASMError>& errors);
    void clearErrors();
    
    // Breakpoints
    void toggleBreakpoint(int line);
    void clearBreakpoints();
    const std::unordered_set<int>& getBreakpoints() const { return m_breakpoints; }
    
    // Code folding
    void foldBlock(int line);
    void unfoldBlock(int line);
    
    // Auto-completion
    void setCompletionModel(const stringList& completions);
    
public:
    void breakpointToggled(int line, bool enabled);
    void errorClicked(const MASMError& error);
    
protected:
    void resizeEvent(void* event);
    void keyPressEvent(void* event);
    void mousePressEvent(void* event);
    
\nprivate:\n    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const struct { int x; int y; int w; int h; }& rect, int dy);
    
private:
    class LineNumberArea;
    std::unique_ptr<LineNumberArea> m_lineNumberArea;
    std::unique_ptr<MASMSyntaxHighlighter> m_highlighter;
    
    std::vector<MASMError> m_errors;
    std::unordered_set<int> m_breakpoints;
    std::unordered_set<int> m_foldedBlocks;
    
    void paintLineNumberArea(void* event);
    void paintErrorMarkers(void* event);
    void paintBreakpoints(void* event);
};

// ============================================================================
// MASM Syntax Highlighter
// ============================================================================
class MASMSyntaxHighlighter {public:
    explicit MASMSyntaxHighlighter(QTextDocument* parent = nullptr);
    
protected:
    void highlightBlock(const std::string& text) override;
    
private:
    struct HighlightingRule {
        std::regex pattern;
        QTextCharFormat format;
    };
    
    std::vector<HighlightingRule> m_rules;
    
    QTextCharFormat m_keywordFormat;
    QTextCharFormat m_directiveFormat;
    QTextCharFormat m_instructionFormat;
    QTextCharFormat m_registerFormat;
    QTextCharFormat m_numberFormat;
    QTextCharFormat m_stringFormat;
    QTextCharFormat m_commentFormat;
    QTextCharFormat m_labelFormat;
    QTextCharFormat m_operatorFormat;
    
    void setupRules();
};

// ============================================================================
// MASM Project Explorer
// ============================================================================
class MASMProjectExplorer {public:
    explicit MASMProjectExplorer(void* parent = nullptr);
    
    void setProject(const MASMProjectSettings& project);
    void refresh();
    
\npublic:\n    void fileOpened(const std::string& filePath);
    void fileCreated(const std::string& filePath);
    void fileDeleted(const std::string& filePath);
    void fileRenamed(const std::string& oldPath, const std::string& newPath);
    
\nprivate:\n    void onTreeItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onTreeContextMenu(const struct { int x; int y; }& pos);
    
private:
    std::unique_ptr<QTreeWidget> m_treeWidget;
    MASMProjectSettings m_project;
    
    void populateTree();
    void addContextMenuActions(void* menu, QTreeWidgetItem* item);
};

// ============================================================================
// MASM Build Output Panel
// ============================================================================
class MASMBuildOutput {public:
    explicit MASMBuildOutput(void* parent = nullptr);
    
    void clear();
    void appendMessage(const std::string& message);
    void appendError(const MASMError& error);
    void appendStage(const std::string& stage);
    void setStats(const MASMCompilationStats& stats);
    
\npublic:\n    void errorDoubleClicked(const MASMError& error);
    
\nprivate:\n    void onOutputDoubleClicked();
    
private:
    std::unique_ptr<void> m_outputEdit;
    std::unique_ptr<void> m_statsLabel;
    std::vector<MASMError> m_errors;
    
    void formatErrorMessage(const MASMError& error, std::string& output);
};

// ============================================================================
// MASM Symbol Browser
// ============================================================================
class MASMSymbolBrowser {public:
    explicit MASMSymbolBrowser(void* parent = nullptr);
    
    void setSymbols(const std::vector<MASMSymbol>& symbols);
    void clear();
    void filter(const std::string& text);
    
\npublic:\n    void symbolSelected(const MASMSymbol& symbol);
    
\nprivate:\n    void onSymbolClicked(QTreeWidgetItem* item, int column);
    void onFilterChanged(const std::string& text);
    
private:
    std::unique_ptr<QTreeWidget> m_treeWidget;
    std::unique_ptr<voidEdit> m_filterEdit;
    std::vector<MASMSymbol> m_symbols;
    
    void populateTree();
};

// ============================================================================
// MASM Debugger Widget
// ============================================================================
class MASMDebugger {public:
    explicit MASMDebugger(void* parent = nullptr);
    
    void startDebugging(const std::string& executablePath);
    void stopDebugging();
    void stepOver();
    void stepInto();
    void stepOut();
    void continueExecution();
    void pause();
    
    void setBreakpoints(const std::unordered_set<int>& breakpoints);
    
\npublic:\n    void debuggerStarted();
    void debuggerStopped();
    void breakpointHit(int line);
    void registerChanged(const std::string& regName, uint64_t value);
    void memoryChanged(uint64_t address, const std::vector<uint8_t>& data);
    
\nprivate:\n    void onDebuggerOutput();
    void onDebuggerError();
    
private:
    std::unique_ptr<void*> m_debugProcess;
    std::unique_ptr<QTreeWidget> m_registersWidget;
    std::unique_ptr<QTreeWidget> m_stackWidget;
    std::unique_ptr<void> m_disassemblyWidget;
    
    std::unordered_set<int> m_breakpoints;
    bool m_isDebugging;
    
    void updateRegisters();
    void updateStack();
    void updateDisassembly();
};

// ============================================================================
// Main MASM Compiler Widget
// ============================================================================
class MASMCompilerWidget {public:
    explicit MASMCompilerWidget(void* parent = nullptr);
    ~MASMCompilerWidget();
    
    // Project management
    void newProject();
    void openProject(const std::string& projectPath);
    void closeProject();
    void saveProject();
    const MASMProjectSettings& getProject() const { return m_project; }
    
    // File operations
    void newFile();
    void openFile(const std::string& filePath);
    void saveFile();
    void saveFileAs(const std::string& filePath);
    void closeFile();
    
    // Build operations
    void build();
    void rebuild();
    void clean();
    void run();
    void debug();
    void stop();
    
    // Editor access
    MASMCodeEditor* getCurrentEditor() { return m_editor.get(); }
    
\npublic:\n    void compilationStarted();
    void compilationFinished(bool success);
    void executionStarted();
    void executionFinished(int exitCode);
    void errorOccurred(const std::string& error);
    
\nprivate:\n    void onCompilerFinished(int exitCode, void*::ExitStatus exitStatus);
    void onCompilerOutput();
    void onCompilerError();
    
    void onExecutableFinished(int exitCode, void*::ExitStatus exitStatus);
    void onExecutableOutput();
    void onExecutableError();
    
    void onEditorChanged();
    void onErrorClicked(const MASMError& error);
    void onSymbolSelected(const MASMSymbol& symbol);
    void onBreakpointToggled(int line, bool enabled);
    
private:
    // UI Components
    std::unique_ptr<void> m_mainSplitter;
    std::unique_ptr<void> m_leftSplitter;
    std::unique_ptr<void> m_rightSplitter;
    
    std::unique_ptr<MASMCodeEditor> m_editor;
    std::unique_ptr<MASMProjectExplorer> m_projectExplorer;
    std::unique_ptr<MASMBuildOutput> m_buildOutput;
    std::unique_ptr<MASMSymbolBrowser> m_symbolBrowser;
    std::unique_ptr<MASMDebugger> m_debugger;
    
    // Toolbar
    std::unique_ptr<void> m_toolbar;
    void* m_actionBuild;
    void* m_actionRebuild;
    void* m_actionClean;
    void* m_actionRun;
    void* m_actionDebug;
    void* m_actionStop;
    
    // Compilation
    std::unique_ptr<void*> m_compilerProcess;
    std::unique_ptr<void*> m_executableProcess;
    
    MASMProjectSettings m_project;
    MASMCompilationStats m_stats;
    std::vector<MASMError> m_errors;
    std::vector<MASMSymbol> m_symbols;
    
    std::string m_currentFile;
    bool m_isCompiling;
    bool m_isRunning;
    bool m_isDebugging;
    
    // Private methods
    void setupUI();
    void setupToolbar();
    void connectSignals();
    
    void compileFile(const std::string& sourceFile, const std::string& outputFile);
    void parseCompilerOutput(const std::string& output);
    void extractErrors(const std::string& output);
    void extractSymbols(const std::string& output);
    void updateUIAfterCompilation(bool success);
    
    std::string getCompilerExecutable() const;
    std::stringList getCompilerArguments(const std::string& sourceFile, const std::string& outputFile) const;
};

// ============================================================================
// MASM Compiler Backend Interface
// ============================================================================
class MASMCompilerBackend  {public:
    static MASMCompilerBackend& instance();
    
    struct CompilationResult {
        bool success;
        std::string outputFile;
        std::stringList messages;
        std::vector<MASMError> errors;
        std::vector<MASMSymbol> symbols;
        MASMCompilationStats stats;
    };
    
    // Synchronous compilation
    CompilationResult compile(const std::string& sourceFile, const MASMProjectSettings& settings);
    
    // Asynchronous compilation
    void compileAsync(const std::string& sourceFile, const MASMProjectSettings& settings);
    
\npublic:\n    void compilationProgress(const std::string& stage, int percentage);
    void compilationCompleted(const CompilationResult& result);
    
private:
    MASMCompilerBackend();
    ~MASMCompilerBackend();
    
    MASMCompilerBackend(const MASMCompilerBackend&) = delete;
    MASMCompilerBackend& operator=(const MASMCompilerBackend&) = delete;
    
    // Internal compilation stages
    bool lexicalAnalysis(const std::string& source, CompilationResult& result);
    bool syntaxAnalysis(CompilationResult& result);
    bool semanticAnalysis(CompilationResult& result);
    bool codeGeneration(CompilationResult& result);
    bool peGeneration(const std::string& outputFile, CompilationResult& result);
    
    // Helper functions
    std::string readSourceFile(const std::string& filePath);
    void writeOutputFile(const std::string& filePath, const std::vector<uint8_t>& data);
    void buildSymbolTable(const std::string& source);
    void generateMachineCode();
    
    std::mutex m_mutex;
    std::string m_workingDir;
};

// ============================================================================
// MASM Integration Utilities
// ============================================================================
namespace MASMIntegration {
    // Check if MASM compiler is available
    bool isCompilerAvailable();
    
    // Get compiler version
    std::string getCompilerVersion();
    
    // Validate MASM source syntax
    bool validateSyntax(const std::string& source, std::string& error);
    
    // Format MASM source code
    std::string formatCode(const std::string& source);
    
    // Get instruction documentation
    std::string getInstructionHelp(const std::string& instruction);
    
    // Get register documentation
    std::string getRegisterHelp(const std::string& regName);
    
    // Disassemble binary
    std::string disassemble(const std::vector<uint8_t>& machineCode);
    
    // Assemble single instruction
    std::vector<uint8_t> assembleInstruction(const std::string& instruction);
}

// ============================================================================
// MASM Template Generator
// ============================================================================
class MASMTemplateGenerator {
public:
    enum TemplateType {
        HelloWorld,
        ConsoleApp,
        WindowsGUI,
        DLL,
        StaticLib,
        KernelDriver
    };
    
    static std::string generate(TemplateType type, const std::string& projectName);
    static std::stringList getTemplateList();
    static std::string getTemplateDescription(TemplateType type);
};

