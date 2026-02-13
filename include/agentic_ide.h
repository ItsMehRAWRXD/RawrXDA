#pragma once

// C++20 / Win32. Agentic IDE main window; no Qt. Opaque main window handle.

class MultiTabEditor;
class ChatInterface;
class TerminalPool;
class AgenticEngine;
namespace RawrXD { class LSPClient; class PlanOrchestrator; }

class AgenticIDE
{
public:
    AgenticIDE() = default;
    ~AgenticIDE();

    void show();
    void* getMainWindowHandle() const { return m_mainWindow; }

    MultiTabEditor* multiTabEditor() const { return m_multiTabEditor; }
    ChatInterface* chatInterface() const { return m_chatInterface; }
    TerminalPool* terminalPool() const { return m_terminalPool; }
    AgenticEngine* agenticEngine() const { return m_agenticEngine; }
    RawrXD::PlanOrchestrator* planOrchestrator() const { return m_planOrchestrator; }
    RawrXD::LSPClient* lspClient() const { return m_lspClient; }

private:
    void* m_mainWindow = nullptr;
    MultiTabEditor* m_multiTabEditor = nullptr;
    ChatInterface* m_chatInterface = nullptr;
    TerminalPool* m_terminalPool = nullptr;
    void* m_chatDock = nullptr;
    void* m_terminalDock = nullptr;
    AgenticEngine* m_agenticEngine = nullptr;
    RawrXD::PlanOrchestrator* m_planOrchestrator = nullptr;
    RawrXD::LSPClient* m_lspClient = nullptr;
};
