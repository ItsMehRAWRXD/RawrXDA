// RawrXD IDE MainWindow Header
#pragma once

#include <QMainWindow>
#include <memory>

class QProcess;
class QPlainTextEdit;
class QLineEdit;
class QFileSystemModel;
class PowerShellHighlighter;
class TerminalWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

    void setAppState(std::shared_ptr<void> state);

private slots:
    void onEditorTextChanged();
    void updateLineColumnInfo();
    void onFileTreeDoubleClicked(const QModelIndex& index);
    void onTerminalCommand();
    void onPowerShellOutput();
    void onPowerShellError();
    void onApplyClicked();
    void onResetClicked();
    void onRunScript();
    void onNewFile();
    void onOpenFile();
    void onSaveFile();
    void onAbout();

private:
    void createCentralEditor();
    void createFileExplorer();
    void createTerminalPanel();
    void createOutputPanel();
    void createOverclockPanel();
    void setupSyntaxHighlighting();
    void createToolBars();
    void createMenus();
    void createStatusBar();
    void executeCommand(const QString& command);

private:
    QSplitter* m_mainSplitter = nullptr;
    QSplitter* m_editorSplitter = nullptr;
    QTabWidget* m_editorTabs = nullptr;
    QTextEdit* m_editor = nullptr;
    QPlainTextEdit* m_terminalOutput = nullptr;
    QLineEdit* m_commandInput = nullptr;
    QTextEdit* m_outputPanel = nullptr;
    QWidget* m_overclockWidget = nullptr;
    QLabel* m_cpuTelemetryLabel = nullptr;
    QLabel* m_gpuTelemetryLabel = nullptr;
    QLabel* m_offsetLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_applyButton = nullptr;
    QPushButton* m_resetButton = nullptr;
    PowerShellHighlighter* m_highlighter = nullptr;
    QFileSystemModel* m_fileSystemModel = nullptr;
    QTreeView* m_fileExplorer = nullptr;

    QProcess* m_powerShellProcess = nullptr; // kept for compatibility
    TerminalWidget* m_terminalWidget = nullptr; // new reusable terminal
};
// RawrXD IDE MainWindow Header (Stub for Qt Integration)
#pragma once

// Placeholder header for when Qt is properly installed
// This will be replaced with full Qt MainWindow implementation

class MainWindow
{
public:
    MainWindow() = default;
    ~MainWindow() = default;
    
    void show() {
        // Stub for Qt show() method
    }
    
private:
    // Future Qt widgets will go here
};
