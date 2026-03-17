#pragma once

#include <QMainWindow>
#include <QString>
#include <QStringList>

// Forward declarations
class FileBrowser;
class MultiTabEditor;
class ChatInterface;
class TerminalPool;
class AgenticEngine;
class InferenceEngine;
class Settings;
class Telemetry;
class PlanningAgent;
class TodoManager;
class TodoDock;

class AgenticIDE : public QMainWindow
{
    Q_OBJECT

public:
    explicit AgenticIDE(QWidget *parent = nullptr);
    ~AgenticIDE();

private slots:
    // File operations
    void newFile();
    void openFile();
    void saveFile();
    
    // Agent operations
    void startChat();
    void analyzeCode();
    void generateCode();
    void createPlan();
    void hotPatchModel();
    
    // View operations
    void toggleFileBrowser();
    void toggleChat();
    void toggleTerminals();
    void showSettings();
    
    // Edit operations
    void undo();
    void redo();
    void find();
    void replace();

private:
    void setupUI();
    void setupMenus();
    void setupToolbar();
    void setupConnections();
    void loadSettings();
    void saveSettings();
    void addToRecentFiles(const QString &filePath);

    // Core components
    FileBrowser *m_fileBrowser;
    MultiTabEditor *m_multiTabEditor;
    ChatInterface *m_chatInterface;
    TerminalPool *m_terminalPool;
    AgenticEngine *m_agenticEngine;
    InferenceEngine *m_inferenceEngine;
    PlanningAgent *m_planningAgent;
    TodoManager *m_todoManager;
    TodoDock *m_todoDock;
    Settings *m_settings;
    Telemetry *m_telemetry;
    
    // Dock widgets for toggle functionality
    class QDockWidget *m_fileDock;
    class QDockWidget *m_chatDock;
    class QDockWidget *m_terminalDock;
    class QDockWidget *m_todoDockWidget;
    
    QStringList m_recentFiles;
};