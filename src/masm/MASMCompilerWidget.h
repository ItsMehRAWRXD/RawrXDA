// ============================================================================
// MASMCompilerWidget.h
// Qt IDE Integration for MASM Self-Compiling Compiler
// Provides full IDE features: syntax highlighting, error markers, build output
// ============================================================================

#pragma once

#include <QWidget>
#include <QSplitter>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QTreeWidget>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QProcess>
#include <QSyntaxHighlighter>
#include <QTimer>
#include <QMutex>
#include <QSettings>
#include <vector>
#include <memory>
#include <unordered_map>

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
    QString filename;
    int line;
    int column;
    QString errorType;      // "error", "warning", "info"
    QString message;
    QString sourceSnippet;
    
    MASMError() : line(0), column(0) {}
    MASMError(const QString& file, int ln, int col, const QString& type, const QString& msg)
        : filename(file), line(ln), column(col), errorType(type), message(msg) {}
};

// ============================================================================
// MASM Symbol Information
// ============================================================================
struct MASMSymbol {
    QString name;
    QString type;           // "label", "proc", "macro", "constant"
    QString section;        // ".data", ".code", etc.
    int line;
    qint64 address;
    QString signature;      // For procedures: parameter list
    
    MASMSymbol() : line(0), address(0) {}
};

// ============================================================================
// MASM Project Settings
// ============================================================================
struct MASMProjectSettings {
    QString projectName;
    QString projectPath;
    QString outputPath;
    QString mainFile;
    QStringList sourceFiles;
    QStringList includePaths;
    QStringList libraries;
    QString targetArchitecture;     // "x86", "x64", "arm64"
    QString outputFormat;           // "exe", "dll", "lib"
    int optimizationLevel;          // 0-3
    bool generateDebugInfo;
    bool warnings;
    QStringList defines;
    
    MASMProjectSettings()
        : targetArchitecture("x64")
        , outputFormat("exe")
        , optimizationLevel(2)
        , generateDebugInfo(true)
        , warnings(true) {}
    
    void save(QSettings& settings) const;
    void load(QSettings& settings);
};

// ============================================================================
// MASM Compilation Statistics
// ============================================================================
struct MASMCompilationStats {
    qint64 startTime;
    qint64 endTime;
    int sourceLines;
    int tokenCount;
    int astNodeCount;
    int symbolCount;
    int machineCodeSize;
    int errorCount;
    int warningCount;
    QString stage;          // Current compilation stage
    
    MASMCompilationStats() { reset(); }
    
    void reset() {
        startTime = endTime = 0;
        sourceLines = tokenCount = astNodeCount = symbolCount = 0;
        machineCodeSize = errorCount = warningCount = 0;
        stage = "Idle";
    }
    
    qint64 duration() const { return endTime - startTime; }
};

// ============================================================================
// MASM Code Editor with Enhanced Features
// ============================================================================
class MASMCodeEditor : public QPlainTextEdit {
    Q_OBJECT
    
public:
    explicit MASMCodeEditor(QWidget* parent = nullptr);
    ~MASMCodeEditor();
    
    // Line number management
    void lineNumberAreaPaintEvent(QPaintEvent* event);
    int lineNumberAreaWidth();
    
    // Error markers
    void setErrors(const std::vector<MASMError>& errors);
    void clearErrors();
    
    // Breakpoints
    void toggleBreakpoint(int line);
    void clearBreakpoints();
    const QSet<int>& getBreakpoints() const { return m_breakpoints; }
    
    // Code folding
    void foldBlock(int line);
    void unfoldBlock(int line);
    
    // Auto-completion
    void setCompletionModel(const QStringList& completions);
    
signals:
    void breakpointToggled(int line, bool enabled);
    void errorClicked(const MASMError& error);
    
protected:
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    
private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect& rect, int dy);
    
private:
    class LineNumberArea;
    std::unique_ptr<LineNumberArea> m_lineNumberArea;
    std::unique_ptr<MASMSyntaxHighlighter> m_highlighter;
    
    std::vector<MASMError> m_errors;
    QSet<int> m_breakpoints;
    QSet<int> m_foldedBlocks;
    
    void paintLineNumberArea(QPaintEvent* event);
    void paintErrorMarkers(QPaintEvent* event);
    void paintBreakpoints(QPaintEvent* event);
};

// ============================================================================
// MASM Syntax Highlighter
// ============================================================================
class MASMSyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
    
public:
    explicit MASMSyntaxHighlighter(QTextDocument* parent = nullptr);
    
protected:
    void highlightBlock(const QString& text) override;
    
private:
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    
    QVector<HighlightingRule> m_rules;
    
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
class MASMProjectExplorer : public QWidget {
    Q_OBJECT
    
public:
    explicit MASMProjectExplorer(QWidget* parent = nullptr);
    
    void setProject(const MASMProjectSettings& project);
    void refresh();
    
signals:
    void fileOpened(const QString& filePath);
    void fileCreated(const QString& filePath);
    void fileDeleted(const QString& filePath);
    void fileRenamed(const QString& oldPath, const QString& newPath);
    
private slots:
    void onTreeItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onTreeContextMenu(const QPoint& pos);
    
private:
    std::unique_ptr<QTreeWidget> m_treeWidget;
    MASMProjectSettings m_project;
    
    void populateTree();
    void addContextMenuActions(QMenu* menu, QTreeWidgetItem* item);
};

// ============================================================================
// MASM Build Output Panel
// ============================================================================
class MASMBuildOutput : public QWidget {
    Q_OBJECT
    
public:
    explicit MASMBuildOutput(QWidget* parent = nullptr);
    
    void clear();
    void appendMessage(const QString& message);
    void appendError(const MASMError& error);
    void appendStage(const QString& stage);
    void setStats(const MASMCompilationStats& stats);
    
signals:
    void errorDoubleClicked(const MASMError& error);
    
private slots:
    void onOutputDoubleClicked();
    
private:
    std::unique_ptr<QTextEdit> m_outputEdit;
    std::unique_ptr<QLabel> m_statsLabel;
    std::vector<MASMError> m_errors;
    
    void formatErrorMessage(const MASMError& error, QString& output);
};

// ============================================================================
// MASM Symbol Browser
// ============================================================================
class MASMSymbolBrowser : public QWidget {
    Q_OBJECT
    
public:
    explicit MASMSymbolBrowser(QWidget* parent = nullptr);
    
    void setSymbols(const std::vector<MASMSymbol>& symbols);
    void clear();
    void filter(const QString& text);
    
signals:
    void symbolSelected(const MASMSymbol& symbol);
    
private slots:
    void onSymbolClicked(QTreeWidgetItem* item, int column);
    void onFilterChanged(const QString& text);
    
private:
    std::unique_ptr<QTreeWidget> m_treeWidget;
    std::unique_ptr<QLineEdit> m_filterEdit;
    std::vector<MASMSymbol> m_symbols;
    
    void populateTree();
};

// ============================================================================
// MASM Debugger Widget
// ============================================================================
class MASMDebugger : public QWidget {
    Q_OBJECT
    
public:
    explicit MASMDebugger(QWidget* parent = nullptr);
    
    void startDebugging(const QString& executablePath);
    void stopDebugging();
    void stepOver();
    void stepInto();
    void stepOut();
    void continueExecution();
    void pause();
    
    void setBreakpoints(const QSet<int>& breakpoints);
    
signals:
    void debuggerStarted();
    void debuggerStopped();
    void breakpointHit(int line);
    void registerChanged(const QString& regName, quint64 value);
    void memoryChanged(quint64 address, const QByteArray& data);
    
private slots:
    void onDebuggerOutput();
    void onDebuggerError();
    
private:
    std::unique_ptr<QProcess> m_debugProcess;
    std::unique_ptr<QTreeWidget> m_registersWidget;
    std::unique_ptr<QTreeWidget> m_stackWidget;
    std::unique_ptr<QTextEdit> m_disassemblyWidget;
    
    QSet<int> m_breakpoints;
    bool m_isDebugging;
    
    void updateRegisters();
    void updateStack();
    void updateDisassembly();
};

// ============================================================================
// Main MASM Compiler Widget
// ============================================================================
class MASMCompilerWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit MASMCompilerWidget(QWidget* parent = nullptr);
    ~MASMCompilerWidget();
    
    // Project management
    void newProject();
    void openProject(const QString& projectPath);
    void closeProject();
    void saveProject();
    const MASMProjectSettings& getProject() const { return m_project; }
    
    // File operations
    void newFile();
    void openFile(const QString& filePath);
    void saveFile();
    void saveFileAs(const QString& filePath);
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
    
signals:
    void compilationStarted();
    void compilationFinished(bool success);
    void executionStarted();
    void executionFinished(int exitCode);
    void errorOccurred(const QString& error);
    
private slots:
    void onCompilerFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onCompilerOutput();
    void onCompilerError();
    
    void onExecutableFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onExecutableOutput();
    void onExecutableError();
    
    void onEditorChanged();
    void onErrorClicked(const MASMError& error);
    void onSymbolSelected(const MASMSymbol& symbol);
    void onBreakpointToggled(int line, bool enabled);
    
private:
    // UI Components
    std::unique_ptr<QSplitter> m_mainSplitter;
    std::unique_ptr<QSplitter> m_leftSplitter;
    std::unique_ptr<QSplitter> m_rightSplitter;
    
    std::unique_ptr<MASMCodeEditor> m_editor;
    std::unique_ptr<MASMProjectExplorer> m_projectExplorer;
    std::unique_ptr<MASMBuildOutput> m_buildOutput;
    std::unique_ptr<MASMSymbolBrowser> m_symbolBrowser;
    std::unique_ptr<MASMDebugger> m_debugger;
    
    // Toolbar
    std::unique_ptr<QToolBar> m_toolbar;
    QAction* m_actionBuild;
    QAction* m_actionRebuild;
    QAction* m_actionClean;
    QAction* m_actionRun;
    QAction* m_actionDebug;
    QAction* m_actionStop;
    
    // Compilation
    std::unique_ptr<QProcess> m_compilerProcess;
    std::unique_ptr<QProcess> m_executableProcess;
    
    MASMProjectSettings m_project;
    MASMCompilationStats m_stats;
    std::vector<MASMError> m_errors;
    std::vector<MASMSymbol> m_symbols;
    
    QString m_currentFile;
    bool m_isCompiling;
    bool m_isRunning;
    bool m_isDebugging;
    
    // Private methods
    void setupUI();
    void setupToolbar();
    void connectSignals();
    
    void compileFile(const QString& sourceFile, const QString& outputFile);
    void parseCompilerOutput(const QString& output);
    void extractErrors(const QString& output);
    void extractSymbols(const QString& output);
    void updateUIAfterCompilation(bool success);
    
    QString getCompilerExecutable() const;
    QStringList getCompilerArguments(const QString& sourceFile, const QString& outputFile) const;
};

// ============================================================================
// MASM Compiler Backend Interface
// ============================================================================
class MASMCompilerBackend : public QObject {
    Q_OBJECT
    
public:
    static MASMCompilerBackend& instance();
    
    struct CompilationResult {
        bool success;
        QString outputFile;
        QStringList messages;
        std::vector<MASMError> errors;
        std::vector<MASMSymbol> symbols;
        MASMCompilationStats stats;
    };
    
    // Synchronous compilation
    CompilationResult compile(const QString& sourceFile, const MASMProjectSettings& settings);
    
    // Asynchronous compilation
    void compileAsync(const QString& sourceFile, const MASMProjectSettings& settings);
    
signals:
    void compilationProgress(const QString& stage, int percentage);
    void compilationCompleted(const CompilationResult& result);
    
private:
    MASMCompilerBackend();
    ~MASMCompilerBackend();
    
    MASMCompilerBackend(const MASMCompilerBackend&) = delete;
    MASMCompilerBackend& operator=(const MASMCompilerBackend&) = delete;
    
    // Internal compilation stages
    bool lexicalAnalysis(const QString& source, CompilationResult& result);
    bool syntaxAnalysis(CompilationResult& result);
    bool semanticAnalysis(CompilationResult& result);
    bool codeGeneration(CompilationResult& result);
    bool peGeneration(const QString& outputFile, CompilationResult& result);
    
    // Helper functions
    QString readSourceFile(const QString& filePath);
    void writeOutputFile(const QString& filePath, const QByteArray& data);
    void buildSymbolTable(const QString& source);
    void generateMachineCode();
    
    QMutex m_mutex;
    QString m_workingDir;
};

// ============================================================================
// MASM Integration Utilities
// ============================================================================
namespace MASMIntegration {
    // Check if MASM compiler is available
    bool isCompilerAvailable();
    
    // Get compiler version
    QString getCompilerVersion();
    
    // Validate MASM source syntax
    bool validateSyntax(const QString& source, QString& error);
    
    // Format MASM source code
    QString formatCode(const QString& source);
    
    // Get instruction documentation
    QString getInstructionHelp(const QString& instruction);
    
    // Get register documentation
    QString getRegisterHelp(const QString& regName);
    
    // Disassemble binary
    QString disassemble(const QByteArray& machineCode);
    
    // Assemble single instruction
    QByteArray assembleInstruction(const QString& instruction);
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
    
    static QString generate(TemplateType type, const QString& projectName);
    static QStringList getTemplateList();
    static QString getTemplateDescription(TemplateType type);
};
