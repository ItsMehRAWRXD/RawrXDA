// RawrXD Agentic IDE - v5.0 Clean Header
#pragma once

#include <QMainWindow>
#include <QString>

// Forward declarations
class QDockWidget;
class MultiTabEditor;
class ChatInterface;
class TerminalPool;
class FileBrowser;
class AgenticEngine;
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
    void showAbout();
    void showAIHelp();

private:
    // Initialization phases (deferred to event loop)
    void initialize();
    void initializePhase2();
    void initializePhase3();
    void initializePhase4();
    
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
    RawrXD::PlanOrchestrator *m_planOrchestrator;
    RawrXD::LSPClient *m_lspClient;
    TodoManager *m_todoManager;
    TodoDock *m_todoDock;
    
    // Dock widgets
    QDockWidget *m_chatDock;
    QDockWidget *m_terminalDock;
    QDockWidget *m_fileDock;
    QDockWidget *m_todoDockWidget;
};

} // namespace RawrXD
