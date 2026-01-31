#pragma once

class MultiTabEditor;
class ChatInterface;
class TerminalPool;
class AgenticEngine;

namespace RawrXD {
    class LSPClient;
    class PlanOrchestrator;
}

class AgenticIDE : public void {
public:
    explicit AgenticIDE(void *parent = nullptr);
    ~AgenticIDE();

protected:
    void showEvent(QShowEvent *ev) override;

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
};

