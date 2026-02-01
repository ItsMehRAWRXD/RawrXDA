#pragma once

#include <string>
#include <memory>
#include <vector>
#include <filesystem>
#include <atomic>

// Logic Components
#include "multi_tab_editor.h"
#include "chat_interface.h"

class TerminalPool;
class AgenticEngine;
class ZeroDayAgenticEngine;
class AutonomousModelManager;
class AutonomousIntelligenceOrchestrator; // Added forward declaration

namespace RawrXD {
    class ToolRegistry;
    class LSPClient;
    class PlanOrchestrator;
    class Editor; // Forward decl
    class CPUInferenceEngine; // Forward Decl
    class UniversalModelRouter;
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
    void startOrchestrator(); // Added

    // Accessors for Agents
    MultiTabEditor* getEditor() { return m_multiTabEditor.get(); }
    TerminalPool* getTerminalPool() { return m_terminalPool.get(); }
    AutonomousModelManager* getModelManager() { return m_modelManager.get(); }

private:
    std::atomic<bool> m_running{false};

    // Core systems
    std::unique_ptr<AgenticEngine> m_agenticEngine;
    std::unique_ptr<ZeroDayAgenticEngine> m_zeroDayAgent;
    std::unique_ptr<RawrXD::UniversalModelRouter> m_modelRouter;
    std::unique_ptr<RawrXD::ToolRegistry> m_toolRegistry;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_inferenceEngine;
    std::unique_ptr<AutonomousModelManager> m_modelManager;
    std::unique_ptr<AutonomousIntelligenceOrchestrator> m_orchestrator; // Added member

    // Orchestration
    std::unique_ptr<RawrXD::PlanOrchestrator> m_planOrchestrator;
    std::unique_ptr<RawrXD::LSPClient> m_lspClient;
    
    // UI / Editor Logic
    std::unique_ptr<MultiTabEditor> m_multiTabEditor;
    std::unique_ptr<ChatInterface> m_chatInterface;
    std::unique_ptr<TerminalPool> m_terminalPool; 

    RawrXD::Editor* m_guiEditor = nullptr;

    void processConsoleInput();
};
