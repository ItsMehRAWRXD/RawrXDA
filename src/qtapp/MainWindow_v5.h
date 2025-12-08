// RawrXD Agentic IDE - v5.0 Clean Header
#pragma once

#include <QMainWindow>
#include <QString>

// Forward declarations
class QDockWidget;
class QLabel;
class QProgressBar;
class MultiTabEditor;
class ChatInterface;
class TerminalPool;
class FileBrowser;
class AgenticEngine;
class InferenceEngine;
class TodoManager;
class TodoDock;

namespace RawrXD {
    class PlanOrchestrator;
    class LSPClient;
}

namespace RawrXD {

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // File operations
    void newFile();
    void openFile();
    void saveFile();
    void undo();
    void redo();
    void find();
    void replace();
    
    // View operations
    void toggleFileBrowser();
    void toggleChat();
    void toggleTerminals();
    void toggleTodos();
    
    // AI operations
    void startChat();
    void analyzeCode();
    void generateCode();
    void refactorCode();
    void loadModel();
    void onModelSelected(const QString &ggufPath);
    void onChatMessageSent(const QString &message);
    void applyInferenceSettings();
    void showInferenceSettings();
    void showAbout();
    void showAIHelp();
    
    // LSP operations
    void startLSPServer();
    void stopLSPServer();
    void restartLSPServer();
    void showLSPStatus();
    
    // Settings
    void showPreferences();
    
    // TODO operations
    void addTodo();
    void scanCodeForTodos();
    
    // Terminal operations
    void newTerminal();
    void closeTerminal();
    void nextTerminal();
    void previousTerminal();

private:
    // Initialization phases (deferred to event loop)
    void initialize();
    void initializePhase2();
    void initializePhase3();
    void initializePhase4();
    void updateSplashProgress(const QString& message, int percent);
    
    // Setup methods
    void setupMenuBar();
    void setupToolBars();
    void setupStatusBar();
    void loadSettings();
    void saveSettings();
    
    // Core components (created in phases)
    MultiTabEditor *m_multiTabEditor;
    ChatInterface *m_chatInterface;
    TerminalPool *m_terminalPool;
    FileBrowser *m_fileBrowser;
    AgenticEngine *m_agenticEngine;
    InferenceEngine *m_inferenceEngine;
    RawrXD::PlanOrchestrator *m_planOrchestrator;
    RawrXD::LSPClient *m_lspClient;
    TodoManager *m_todoManager;
    TodoDock *m_todoDock;
    
    // Dock widgets
    QDockWidget *m_chatDock;
    QDockWidget *m_terminalDock;
    QDockWidget *m_fileDock;
    QDockWidget *m_todoDockWidget;
    
    // Splash screen for initialization
    QWidget *m_splashWidget;
    QLabel *m_splashLabel;
    QProgressBar *m_splashProgress;
};

} // namespace RawrXD
