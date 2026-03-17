#pragma once
#include <QMainWindow>

class QShowEvent;
class MultiTabEditor;
class ChatInterface;
class TerminalPool;
class AgenticEngine;
class AICodeAssistant;
class AICodeAssistantPanel;

namespace RawrXD {
    class LSPClient;
    class PlanOrchestrator;
}

class AgenticIDE : public QMainWindow {
    Q_OBJECT

public:
    explicit AgenticIDE(QWidget *parent = nullptr);
    ~AgenticIDE();

    // Agentic-exclusive methods for AI workspace automation
    void onAISearchWorkspace(const QString &pattern);
    void onAIGrepWorkspace(const QString &pattern);
    void onAIExecuteCommand(const QString &command);
    void onAIAnalyzeCode(const QString &context);
    void onAIAutofixError(const QString &issueDescription, const QString &codeContext);

protected:
    void showEvent(QShowEvent *ev) override;

private slots:
    // Handle AI Code Assistant results
    void onAISearchCompleted(const QStringList &results);
    void onAIGrepCompleted(const QJsonArray &results);
    void onAICommandCompleted(int exitCode);
    void onAIAnalysisCompleted(const QString &recommendation);
    void onAIAutofixCompleted(const QString &fixedCode);
    void onAIError(const QString &error);

private:
    // Core UI components
    MultiTabEditor *m_multiTabEditor = nullptr;
    ChatInterface *m_chatInterface = nullptr;
    TerminalPool *m_terminalPool = nullptr;
    class QDockWidget *m_chatDock = nullptr;
    class QDockWidget *m_terminalDock = nullptr;
    
    // Agent engine
    AgenticEngine *m_agenticEngine = nullptr;
    
    // Multi-file orchestration
    RawrXD::PlanOrchestrator *m_planOrchestrator = nullptr;
    RawrXD::LSPClient *m_lspClient = nullptr;

    // AI Code Assistant and Panel (Phase 4)
    AICodeAssistant *m_aiCodeAssistant = nullptr;
    AICodeAssistantPanel *m_aiPanel = nullptr;
    class QDockWidget *m_aiPanelDock = nullptr;
};
