#pragma once

#include <string>
#include <memory>
#include <vector>
#include <filesystem>

class MultiTabEditor;
class ChatInterface;
class TerminalPool;
class AgenticEngine;
class ZeroDayAgenticEngine;
class UniversalModelRouter;
class ToolRegistry;

namespace RawrXD {
    class LSPClient;
    class PlanOrchestrator;
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

private:
    // Core systems
    std::unique_ptr<AgenticEngine> m_agenticEngine;
    std::unique_ptr<ZeroDayAgenticEngine> m_zeroDayAgent;
    std::unique_ptr<UniversalModelRouter> m_modelRouter;
    std::unique_ptr<ToolRegistry> m_toolRegistry;
    
    // Orchestration
    std::unique_ptr<RawrXD::PlanOrchestrator> m_planOrchestrator;
    std::unique_ptr<RawrXD::LSPClient> m_lspClient;
    
    // UI Stubs (to be replaced by Win32 or other UI if needed)
    MultiTabEditor *m_multiTabEditor = nullptr;
    ChatInterface *m_chatInterface = nullptr;
    TerminalPool *m_terminalPool = nullptr;
};

