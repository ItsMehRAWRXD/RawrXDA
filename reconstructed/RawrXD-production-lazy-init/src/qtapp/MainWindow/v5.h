#ifndef MAINWINDOW_V5_H
#define MAINWINDOW_V5_H

#include <QMainWindow>
#include <QTabWidget>
#include <QTreeWidget>
#include <QPlainTextEdit>
#include <QInputDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonObject>
#include <QLineEdit>

class AIDigestionPanel;
class AIChatPanel;
class InferenceEngine;
class AgenticExecutor;
class AutonomousIntelligenceOrchestrator;
class AutonomousFeatureEngine;
class AutonomousSuggestionWidget;
class SecurityAlertWidget;
class OptimizationPanelWidget;
class QDockWidget;
class QComboBox;
class QPushButton;
class QProgressBar;
class QLabel;
class QTextEdit;
class TerminalWidget;
class CommandPalette; 
class DebuggerPanel;
class TestExplorerPanel;

class MainWindow_v5 : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow_v5(QWidget* parent = nullptr);
    ~MainWindow_v5() override = default;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onNewFile();
    void onOpenFile();
    void onSaveFile();
    void openSettings();
    void toggleAIChatPanel();
    void toggleFileBrowser();
    void toggleTerminal();
    void toggleModelManager();
    void toggleProblemsPanel();
    void toggleTodoDock();
    void toggleDebugger();
    void toggleTestExplorer();
    void generateCode();
    void onWorkflowBrowseOutput();
    void onRunWorkflow();
    void onWorkflowStatusUpdate(const QString& phase, const QString& detail, int step, int total);
    void onWorkflowComplete(const QJsonObject& result);
    void appendWorkflowLog(const QString& message);

private:
    void setupUI();
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupDockWidgets();
    void applyDarkTheme();
    void restoreWindowState();
    void saveWindowState();
    void setupWorkflowPanel();
    void refreshCompilerCombo();
    void updateIntelligenceDashboard(const QJsonObject& result);
    void updateCapabilitiesDashboard(const QJsonObject& capabilities);

private:
    QTabWidget* m_tabWidget = nullptr;
    AIDigestionPanel* m_digestionPanel = nullptr;
    AIChatPanel* m_aiChatPanel = nullptr;
    QPlainTextEdit* m_codeEditor = nullptr;
    QTreeWidget* m_fileBrowser = nullptr;
    TerminalWidget* m_terminal = nullptr;
    InferenceEngine* m_inferenceEngine = nullptr;
    AgenticExecutor* m_agenticExecutor = nullptr;
    AutonomousIntelligenceOrchestrator* m_autonomousOrchestrator = nullptr;
    AutonomousFeatureEngine* m_featureEngine = nullptr;
    AutonomousSuggestionWidget* m_suggestionWidget = nullptr;
    SecurityAlertWidget* m_securityWidget = nullptr;
    OptimizationPanelWidget* m_optimizationWidget = nullptr;
    class ModelLoaderWidget* m_modelLoader = nullptr;
    class TodoManager* m_todoManager = nullptr;
    class TodoDock* m_todoDock = nullptr;
    class CommandPalette* m_commandPalette = nullptr;
    class ProblemsPanel* m_problemsPanel = nullptr;
    DebuggerPanel* m_debuggerPanel = nullptr;
    TestExplorerPanel* m_testExplorer = nullptr;
    QDockWidget* m_debuggerDock = nullptr;
    QDockWidget* m_testExplorerDock = nullptr;
    QDockWidget* m_workflowDock = nullptr;
    QPlainTextEdit* m_workflowSpec = nullptr;
    QLineEdit* m_workflowOutputPath = nullptr;
    QPushButton* m_workflowBrowseButton = nullptr;
    QComboBox* m_workflowCompiler = nullptr;
    QPushButton* m_workflowStartButton = nullptr;
    QProgressBar* m_workflowProgress = nullptr;
    QLabel* m_workflowStatus = nullptr;
    QTextEdit* m_workflowLog = nullptr;
};

#endif // MAINWINDOW_V5_H


