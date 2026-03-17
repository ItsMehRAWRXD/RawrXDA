//=============================================================================
// AgenticEngineSovereignHook.cpp
// Bridges agentic_engine.cpp to SovereignCore
//=============================================================================

#include "AgenticEngineSovereignHook.h"
#include <sstream>
#include <iomanip>

namespace RawrXD {

AgenticEngineSovereignHook* AgenticEngineSovereignHook::s_instance = nullptr;
static std::mutex s_hookMutex;

AgenticEngineSovereignHook& AgenticEngineSovereignHook::getInstance() {
    if (!s_instance) {
        std::lock_guard<std::mutex> lock(s_hookMutex);
        if (!s_instance) {
            s_instance = new AgenticEngineSovereignHook();
        }
    }
    return *s_instance;
}

AgenticEngineSovereignHook::AgenticEngineSovereignHook()
    : m_core(Sovereign::SovereignCore::getInstance()),
      m_bridge(Sovereign::SovereignIDEBridge::getInstance()),
      m_enabled(true),
      m_lastStatus("Sovereign: initialized")
{
}

AgenticEngineSovereignHook::~AgenticEngineSovereignHook() {
}

void AgenticEngineSovereignHook::initialize() {
    std::lock_guard<std::mutex> lock(s_hookMutex);
    
    // Initialize sovereign core with 1 agent
    m_core.initialize(1);
    
    // Register UI callback
    m_bridge.setUIUpdateCallback(
        [this](const std::string& statusLine) {
            this->updateUI(statusLine);
        }
    );
    
    m_lastStatus = "Sovereign: ready [1 agent]";
}

std::string AgenticEngineSovereignHook::processWithSovereign(
    const std::string& userPrompt,
    std::function<std::string(const std::string&)> originalChatFn
)
{
    if (!m_enabled) {
        return originalChatFn(userPrompt);
    }
    
    // 1. Call original chat function
    std::string llmResponse = originalChatFn(userPrompt);
    
    // 2. Trigger sovereign pipeline cycle
    m_bridge.onEngineCycle(userPrompt);
    
    // 3. Augment response with sovereign status
    auto stats = m_core.getStats();
    
    std::ostringstream oss;
    oss << llmResponse << "\n\n";
    oss << "--- [SOVEREIGN AUTONOMOUS CYCLE] ---\n";
    oss << "Cycle #" << stats.cycleCount << " | ";
    
    const char* statusStr = "IDLE";
    switch (stats.status) {
        case Sovereign::SovereignCore::Status::COMPILING:
            statusStr = "COMPILING"; break;
        case Sovereign::SovereignCore::Status::FIXING:
            statusStr = "FIXING"; break;
        case Sovereign::SovereignCore::Status::SYNCING:
            statusStr = "SYNCING"; break;
        default: break;
    }
    oss << "Status: " << statusStr << " | ";
    oss << "Heals: " << stats.healCount << "\n";
    
    // List agent states
    auto agents = m_core.getAgentStates();
    if (!agents.empty()) {
        oss << "Agents: ";
        for (size_t i = 0; i < agents.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << "A" << agents[i].agentIdx 
                << (agents[i].isAlive ? "(live)" : "(dead)");
        }
        oss << "\n";
    }
    
    oss << "--- [END AUTONOMOUS] ---\n";
    
    return oss.str();
}

void AgenticEngineSovereignHook::updateUI(const std::string& statusLine) {
    std::lock_guard<std::mutex> lock(s_hookMutex);
    m_lastStatus = statusLine;
    
    // TODO: Wire to IDE UI panel
    // This could call agenticEngine.updateStatusPanel(statusLine)
    // or post a message to the Win32 window
}

bool AgenticEngineSovereignHook::isSovereignEnabled() const {
    return m_enabled;
}

void AgenticEngineSovereignHook::setSovereignEnabled(bool enabled) {
    m_enabled = enabled;
    if (enabled && !m_core.isRunning()) {
        m_core.startAutonomousLoop();
    } else if (!enabled && m_core.isRunning()) {
        m_core.stopAutonomousLoop();
    }
}

std::string AgenticEngineSovereignHook::getSovereignDiagnostics() const {
    std::ostringstream oss;
    
    oss << "=== SOVEREIGN DIAGNOSTICS ===\n";
    oss << "Enabled: " << (m_enabled ? "YES" : "NO") << "\n";
    oss << "Running: " << (m_core.isRunning() ? "YES" : "NO") << "\n";
    oss << "Last Status: " << m_lastStatus << "\n";
    
    auto stats = m_core.getStats();
    oss << "Cycles: " << stats.cycleCount << "\n";
    oss << "Heals: " << stats.healCount << "\n";
    
    auto agents = m_core.getAgentStates();
    oss << "Agents: " << agents.size() << " alive\n";
    
    return oss.str();
}

} // namespace RawrXD
