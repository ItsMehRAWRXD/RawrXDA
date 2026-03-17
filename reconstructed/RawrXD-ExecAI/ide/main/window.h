// ================================================================
// RawrXD-ExecAI Main Window - FIXED Header
// No duplicates, no undefined members, production-ready
// ================================================================
#ifndef IDE_MAIN_WINDOW_H
#define IDE_MAIN_WINDOW_H

#include <QMainWindow>
#include <QSettings>
#include <QTextEdit>
#include <QDockWidget>
#include <QString>

// Forward declarations
class CommandServer;
class AgenticEngine;
class PlanOrchestrator;
class QTabWidget;
class QToolBar;

// ================================================================
// MainWindow - Primary IDE interface
// ================================================================
class MainWindow : public QMainWindow {
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();
    
    // CLI interface methods (single declaration, no duplicates)
    Q_INVOKABLE void startChat();
    Q_INVOKABLE void analyzeCode();
    Q_INVOKABLE void refactorCode();
    
    // Model management
    Q_INVOKABLE void loadModel(const QString& model_path);
    Q_INVOKABLE void unloadModel();
    
    // State queries
    Q_INVOKABLE bool isModelLoaded() const;
    Q_INVOKABLE QString currentModelPath() const;
    
signals:
    void modelLoaded(const QString& path);
    void modelUnloaded();
    void inferenceStarted();
    void inferenceCompleted();
    
private slots:
    void onNewFile();
    void onOpenFile();
    void onSaveFile();
    void onRunInference();
    void onShowModelSelector();
    void onShowSettings();
    
private:
    // CORRECTED: Direct member access, no m_private wrapper
    CommandServer* m_commandServer;
    AgenticEngine* m_agenticEngine;
    PlanOrchestrator* m_orchestrator;
    
    // UI components
    QTabWidget* m_editorTabs;
    QTextEdit* m_outputConsole;
    QDockWidget* m_consoleDock;
    QToolBar* m_mainToolbar;
    
    // State
    QString m_currentModelPath;
    bool m_modelLoaded;
    QSettings m_settings;
    
    // Initialization phases
    void initializePhase1();  // Core systems
    void initializePhase2();  // Agentic systems
    void initializePhase3();  // UI components
    void initializePhase4();  // External connections
    
    // UI setup
    void createMenuBar();
    void createToolBar();
    void createDockWidgets();
    void createStatusBar();
    
    // Event handlers
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
};

#endif // IDE_MAIN_WINDOW_H
