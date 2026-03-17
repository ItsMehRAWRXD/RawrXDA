#pragma once
#include <QMainWindow>
class QDockWidget;
class QTabWidget;
class QToolBar;
class QAction;

class QShowEvent;
class MultiTabEditor;
class ChatInterface;
class TerminalPool;
class AgenticEngine;
class ZeroDayAgenticEngine;
class UniversalModelRouter;
class ToolRegistry;
class AgenticBrowser;
class AICodeAssistant;
class AICodeAssistantPanel;
class AutonomousFeatureEngine;
class AutonomousModelManager;
class StreamingGGUFMemoryManager;
class AutonomousSuggestionWidget;
class SecurityAlertWidget;
class OptimizationPanelWidget;

namespace RawrXD {
    class LSPClient;
    class PlanOrchestrator;
}

class AgenticIDE : public QMainWindow {
public:
    explicit AgenticIDE(QWidget *parent = nullptr);
    ~AgenticIDE();
    // Singleton access for global components
    static AgenticIDE* instance();
    static AgenticIDE* s_instance;
    MultiTabEditor* getMultiTabEditor() const { return m_multiTabEditor; }
    AgenticBrowser* getBrowser() const { return m_browser; }
    AutonomousFeatureEngine* getAutonomousFeatureEngine() const { return m_autonomousFeatureEngine; }
    AutonomousModelManager* getAutonomousModelManager() const { return m_autonomousModelManager; }

protected:
    void showEvent(QShowEvent *ev) override;

private:
    // Core UI components
    MultiTabEditor *m_multiTabEditor = nullptr;
    ChatInterface *m_chatInterface = nullptr;
    TerminalPool *m_terminalPool = nullptr;
    class QDockWidget *m_chatDock = nullptr;
    class QDockWidget *m_terminalDock = nullptr;
    class QDockWidget *m_browserDock = nullptr;
    QTabWidget *m_chatTabs = nullptr; // multiple chat sessions
    QToolBar *m_mainToolbar = nullptr; // quick actions
    AgenticBrowser *m_browser = nullptr;
    // AI Assistant UI
    AICodeAssistant *m_aiCodeAssistant = nullptr;
    AICodeAssistantPanel *m_aiPanel = nullptr;
    QDockWidget *m_aiPanelDock = nullptr;
    
    // Autonomous Features UI
    AutonomousSuggestionWidget *m_suggestionWidget = nullptr;
    QDockWidget *m_suggestionDock = nullptr;
    SecurityAlertWidget *m_securityWidget = nullptr;
    QDockWidget *m_securityDock = nullptr;
    OptimizationPanelWidget *m_optimizationWidget = nullptr;
    QDockWidget *m_optimizationDock = nullptr;
    
    // Agent engine
    AgenticEngine *m_agenticEngine = nullptr;
    ZeroDayAgenticEngine *m_zeroDayAgent = nullptr;
    UniversalModelRouter *m_modelRouter = nullptr;
    ToolRegistry *m_toolRegistry = nullptr;
    
    // Autonomous engines
    AutonomousFeatureEngine *m_autonomousFeatureEngine = nullptr;
    AutonomousModelManager *m_autonomousModelManager = nullptr;
    StreamingGGUFMemoryManager *m_streamingMemoryManager = nullptr;
    
    // Multi-file orchestration
    RawrXD::PlanOrchestrator *m_planOrchestrator = nullptr;
    RawrXD::LSPClient *m_lspClient = nullptr;

    // Agentic actions
    void onAISearchWorkspace(const QString &pattern);
    void onAIGrepWorkspace(const QString &pattern);
    void onAIExecuteCommand(const QString &command);
    void onAIAnalyzeCode(const QString &context);
    void onAIAutofixError(const QString &issueDescription, const QString &codeContext);
    
    // Autonomous actions
    void onAutonomousSuggestionAccepted(const QString& suggestionId);
    void onAutonomousSuggestionRejected(const QString& suggestionId);
    void onSecurityIssueFixed(const QString& issueId);
    void onSecurityIssueIgnored(const QString& issueId);
    void onOptimizationApplied(const QString& optimizationId);
    void onOptimizationDismissed(const QString& optimizationId);
    
    // AI result handlers
    void onAISearchCompleted(const QStringList &results);
    void onAIGrepCompleted(const QJsonArray &results);
    void onAICommandCompleted(int exitCode);
    void onAIAnalysisCompleted(const QString &recommendation);
    void onAIAutofixCompleted(const QString &fixedCode);
    void onAIError(const QString &error);

    // UI actions
    void onNewChatTab();
    void onCloseChatTab();
    void onNewEditorTab();
    void onCloseEditorTab();
};
