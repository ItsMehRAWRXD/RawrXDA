#include "Win32IDE.h"
#include "Win32IDE_InlineCompletion.h"
#include "Win32IDE_AgentHUD.h"
#include "../agentic/AgentToolHandlers.h"

using namespace RawrXD::UX;

void Win32IDE::initializeAgenticParityFeatures() {
    // 1. Inline Completion (Ghost Text)
    if (!InlineCompletionEngine::instance().initialize(m_hwndEditor)) {
        LOG_WARNING("Failed to initialize InlineCompletionEngine");
    }
    
    // 2. Agent Execution HUD
    if (!AgentExecutionHUD::instance().create(m_hwndMain)) {
        LOG_WARNING("Failed to create AgentExecutionHUD");
    }
    
    // Wire HUD stop button to cancel the current agent turn
    AgentExecutionHUD::instance().onCancelRequested = [this]() {
        if (m_agenticChatSession) {
            m_agenticChatSession->CancelCurrentTurn();
        }
    };
    
    // 3. Register Tool Execution Callbacks for HUD
    // Since AgentToolHandlers is a singleton, we hook into it
    auto& handlers = RawrXD::Agentic::AgentToolHandlers::Instance();
    
    // Bridge for HUD feedback during tool execution
    // (Actual wiring for specific tools in ExplorerAgentBridge)
}

// Integration into the main agent turn loop
void Win32IDE::handleAgenticTurnStartHUD(const std::string& toolName, const std::string& args) {
    AgentExecutionHUD::instance().showToolExecuting(toolName, args);
}

void Win32IDE::handleAgenticTurnCompleteHUD(const std::string& result, bool success) {
    AgentExecutionHUD::instance().completeTool(result, success);
}
