#pragma once

#include <string>
#include <memory>
#include <vector>
#include <filesystem>
#include <atomic>

// Logic Components
#include "multi_tab_editor.h"
#include "chat_interface.h" // Assuming this exists or will exist

class TerminalPool;
class AgenticEngine;
class ZeroDayAgenticEngine;
class UniversalModelRouter;
class ToolRegistry;
class CPUInferenceEngine; // Forward Decl

namespace RawrXD {
    class LSPClient;
    class PlanOrchestrator;
    class Editor; // Forward decl
}

/**
 * @class AgenticIDE
 * @brief Main coordinator for the Agentic IDE systems (Pure C++ / No Qt)
 */
class AgenticIDE {
public:
    explicit AgenticIDE();
    ~AgenticIDE();

    void initialize();
    void run();
    void stop();
    void setEditor(RawrXD::Editor* editor);

    // Accessors for Agents
    MultiTabEditor* getEditor() { return m_multiTabEditor.get(); }
    TerminalPool* getTerminalPool() { return m_terminalPool.get(); }

private:
    std::atomic<bool> m_running{false};

    // Core systems
    std::unique_ptr<AgenticEngine> m_agenticEngine;
    std::unique_ptr<ZeroDayAgenticEngine> m_zeroDayAgent;
    std::unique_ptr<UniversalModelRouter> m_modelRouter;
    std::unique_ptr<ToolRegistry> m_toolRegistry;
    std::unique_ptr<CPUInferenceEngine> m_inferenceEngine;

    // Orchestration
    std::unique_ptr<RawrXD::PlanOrchestrator> m_planOrchestrator;
    std::unique_ptr<RawrXD::LSPClient> m_lspClient;
    
    // UI / Editor Logic
    std::unique_ptr<MultiTabEditor> m_multiTabEditor;
    std::unique_ptr<ChatInterface> m_chatInterface;
    std::unique_ptr<TerminalPool> m_terminalPool; // Assuming logic-only pool

    RawrXD::Editor* m_guiEditor = nullptr;

    void processConsoleInput();
};

