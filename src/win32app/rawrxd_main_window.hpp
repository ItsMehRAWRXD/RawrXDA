#pragma once

#include <QMainWindow>
#include <QTextEdit>
#include <QTreeWidget>
#include <QSplitter>
#include <QLabel>
#include <QProgressBar>
#include <QTabWidget>
#include <QProcess>
#include <QThread>
#include <QMutex>
#include <memory>
#include <vector>
#include <unordered_map>

class AICompletionEngine;
class AgenticAgent;
class ModelInferenceEngine;
class TerminalWidget;
class ChatPanel;
class FileWatcher;
class SettingsManager;

/**
 * @class RawrXDMainWindow
 * @brief Main IDE window - the heart of RawrXD IDE
 * 
 * Features:
 * - Multi-tab code editor with syntax highlighting
 * - Real-time AI code completion
 * - Integrated file browser with recursive searching
 * - Built-in terminal with command execution
 * - Chat panel for multi-turn conversations
 * - Agentic agent system for autonomous code fixes
 * - Hot patching support for live code modification
 * - Settings and theme management
 */
class RawrXDMainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit RawrXDMainWindow(QWidget* parent = nullptr);
    ~RawrXDMainWindow() override;

    // Core initialization
    bool initialize();
    bool loadModel(const QString& modelPath);
    bool connectToOllama(const QString& host, int port);

    // File operations
    void openFile(const QString& path);
    void openProject(const QString& projectPath);
    void saveCurrentFile();
    void saveAllFiles();
    void closeFile(int tabIndex);

    // Editor operations
    QString getCurrentEditorContent() const;
    void setCurrentEditorContent(const QString& content);
    int getCurrentLineNumber() const;
    int getCurrentColumnNumber() const;

    // AI features
    void triggerCompletion();
    void triggerAgentFix();
    void triggerCodeSearch(const QString& query);
    void triggerAgentDeepThink(const QString& problem);

    // Terminal operations
    void executeCommand(const QString& cmd);
    void buildProject();
    void runTests();

    // Chat operations
    void sendChatMessage(const QString& message);
    void clearChatHistory();
    void exportChat(const QString& format);

signals:
    // File signals
    void fileOpened(const QString& path);
    void fileSaved(const QString& path);
    void fileClosed(const QString& path);
    void projectOpened(const QString& path);

    // Editor signals
    void contentChanged();
    void cursorPositionChanged(int line, int col);
    void selectionChanged(const QString& selectedText);

    // AI signals
    void completionReady(const QString& suggestion, int confidence);
    void agentFixApplied(const QString& description);
    void agentThinkingStarted();
    void agentThinkingComplete(const QString& result);

    // Terminal signals
    void commandExecuted(int exitCode);
    void outputReceived(const QString& output);

    // Chat signals
    void messageReceived(const QString& role, const QString& content);
    void chatError(const QString& error);

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    // File tree
    void onFileTreeItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onFileTreeItemRightClicked(const QPoint& pos);
    void onFileSearchTriggered(const QString& query);

    // Editor
    void onEditorTextChanged();
    void onEditorCursorPositionChanged();
    void onTabChanged(int index);
    void onTabCloseRequested(int index);

    // AI Completion
    void onCompletionTriggered();
    void onCompletionReady(const QString& suggestion, int confidence);
    void onCompletionAccepted(const QString& completion);
    void onCompletionRejected();

    // Agent system
    void onAgentStarted();
    void onAgentProgress(const QString& status);
    void onAgentComplete(const QString& result);
    void onAgentError(const QString& error);

    // Terminal
    void onTerminalOutputReceived(const QString& output);
    void onTerminalError(const QString& error);
    void onBuildTriggered();
    void onRunTriggered();
    void onDebugTriggered();

    // Chat
    void onChatMessageSent(const QString& message);
    void onChatResponseReceived(const QString& response);

    // Settings
    void onSettingsChanged();
    void onThemeChanged();
    void onModelChanged();

    // System
    void onStatusUpdate(const QString& status, int duration = 0);
    void onErrorOccurred(const QString& title, const QString& message);

private:
    // UI Setup
    void createMenuBar();
    void createToolBar();
    void createDockWidgets();
    void createCentralWidget();
    void createConnections();
    void loadSettings();
    void saveSettings();
    void applyTheme(const QString& themeName);

    // Internal helpers
    void updateRecentFiles(const QString& path);
    void updateWindowTitle();
    void loadProjectStructure(const QString& projectPath);
    void setupSyntaxHighlighting();
    void setupKeybindings();

    // UI Components
    // Central editor area
    QTabWidget* m_editorTabs;
    std::unordered_map<int, QTextEdit*> m_editors;  // tab index -> editor widget
    int m_currentEditorIndex = -1;

    // File tree widget
    QTreeWidget* m_fileTree;
    QSplitter* m_mainSplitter;
    QSplitter* m_editorSplitter;

    // AI Completion display
    QWidget* m_completionPopup;
    QLabel* m_completionLabel;
    QLabel* m_confidenceLabel;
    QPushButton* m_acceptButton;
    QPushButton* m_rejectButton;

    // Bottom panels
    QTabWidget* m_bottomTabs;
    TerminalWidget* m_terminal;
    ChatPanel* m_chatPanel;
    QTextEdit* m_outputPanel;
    QLabel* m_statusBar;
    QProgressBar* m_progressBar;

    // Right panels
    QDockWidget* m_agentDock;
    QTextEdit* m_agentOutput;
    QLabel* m_agentStatus;
    QProgressBar* m_agentProgress;

    // Status indicators
    QLabel* m_connectionStatus;
    QLabel* m_modelStatus;
    QLabel* m_lineColLabel;

    // Core logic components
    std::unique_ptr<AICompletionEngine> m_completionEngine;
    std::unique_ptr<AgenticAgent> m_agent;
    std::unique_ptr<ModelInferenceEngine> m_inferenceEngine;
    std::unique_ptr<TerminalWidget> m_terminalWidget;
    std::unique_ptr<ChatPanel> m_chat;
    std::unique_ptr<FileWatcher> m_fileWatcher;
    std::unique_ptr<SettingsManager> m_settings;

    // State management
    QMutex m_mutex;
    std::vector<QString> m_openFiles;
    QString m_currentProject;
    QString m_currentModel;
    bool m_isConnected = false;
    bool m_isCompletionActive = false;
    bool m_isAgentActive = false;
    int m_completionTimer = -1;

    // Configuration
    QString m_ollamaHost;
    int m_ollamaPort = 11434;
    int m_completionDelay = 300;  // ms
    int m_completionConfidenceThreshold = 40;  // %
};
