#pragma once
#include <QMainWindow>
#include <QString>

class QShowEvent;
class MultiTabEditor;
class ChatInterface;
class TerminalPool;
class AgenticEngine;
class ZeroDayAgenticEngine;
class UniversalModelRouter;
class ToolRegistry;
class AgenticFileOperations;

namespace RawrXD {
    class LSPClient;
    class PlanOrchestrator;
}

class AgenticIDE : public QMainWindow {
public:
    explicit AgenticIDE(QWidget *parent = nullptr);
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
    ZeroDayAgenticEngine *m_zeroDayAgent = nullptr;
    UniversalModelRouter *m_modelRouter = nullptr;
    ToolRegistry *m_toolRegistry = nullptr;
    
    // File operations with Keep/Undo
    AgenticFileOperations *m_fileOperations = nullptr;
    
    // Multi-file orchestration
    RawrXD::PlanOrchestrator *m_planOrchestrator = nullptr;
    RawrXD::LSPClient *m_lspClient = nullptr;
};
