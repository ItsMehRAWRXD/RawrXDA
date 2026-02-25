// ============================================================================
// agentic_bridge_headless.cpp — Headless AgenticBridge (stub holder; see UNFINISHED_FEATURES.md)
// ============================================================================
// RawrEngine has no Win32IDE; Win32IDE_AgenticBridge requires Win32IDE*.
// Minimal implementations so AgentLoop and other agentic code link without the full Win32 GUI stack.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "Win32IDE_AgenticBridge.h"

// Forward-declared; never dereferenced in headless
struct Win32IDE;

AgenticBridge::AgenticBridge(Win32IDE* ide)
    : m_ide(ide)
    , m_initialized(false)
    , m_agentLoopRunning(false)
    , m_hProcess(nullptr)
    , m_hStdoutRead(nullptr)
    , m_hStdoutWrite(nullptr)
    , m_hStdinRead(nullptr)
    , m_hStdinWrite(nullptr)
{
    (void)ide;
}

AgenticBridge::~AgenticBridge() {}

bool AgenticBridge::Initialize(const std::string&, const std::string&) { return false; }

AgentResponse AgenticBridge::ExecuteAgentCommand(const std::string& prompt) {
    (void)prompt;
    return {AgentResponseType::AGENT_ERROR, "Headless: AgenticBridge not available"};
}

bool AgenticBridge::StartAgentLoop(const std::string&, int) { return false; }
void AgenticBridge::StopAgentLoop() {}
std::vector<std::string> AgenticBridge::GetAvailableTools() { return {}; }
std::string AgenticBridge::GetAgentStatus() { return "Headless"; }
void AgenticBridge::SetModel(const std::string&) {}
void AgenticBridge::SetOllamaServer(const std::string&) {}
void AgenticBridge::SetMaxMode(bool) {}
void AgenticBridge::SetDeepThinking(bool) {}
void AgenticBridge::SetDeepResearch(bool) {}
void AgenticBridge::SetNoRefusal(bool) {}
void AgenticBridge::SetAutoCorrect(bool) {}
void AgenticBridge::SetContextSize(const std::string&) {}
bool AgenticBridge::LoadModel(const std::string&) { return false; }
void AgenticBridge::SetLanguageContext(const std::string&, const std::string&) {}
void AgenticBridge::SetOutputCallback(OutputCallback) {}

std::string AgenticBridge::RunDumpbin(const std::string&, const std::string&) { return ""; }
std::string AgenticBridge::RunCodex(const std::string&) { return ""; }
std::string AgenticBridge::RunCompiler(const std::string&) { return ""; }
SubAgentManager* AgenticBridge::GetSubAgentManager() { return nullptr; }
std::string AgenticBridge::RunSubAgent(const std::string&, const std::string&) { return ""; }
std::string AgenticBridge::ExecuteChain(const std::vector<std::string>&, const std::string&) { return ""; }
std::string AgenticBridge::ExecuteSwarm(const std::vector<std::string>&, const std::string&, int) { return ""; }
void AgenticBridge::CancelAllSubAgents() {}
std::string AgenticBridge::GetSubAgentStatus() const { return ""; }
bool AgenticBridge::DispatchModelToolCalls(const std::string&, std::string&) { return false; }
