#include "agentic_copilot_bridge.hpp"
#include "agent_self_healing_orchestrator.hpp"
#include "agent_hot_patcher.hpp"
#include <iostream>
#include <chrono>

// Counter-Strike: ZERO-TOUCH Autonomous Bridge
// Integrates Self-Healing Orchestrator into the Chat/Agent loop
// Parity with Cursor stability, plus Agentic self-repair.

AgenticCopilotBridge::AgenticCopilotBridge() {
}

AgenticCopilotBridge::~AgenticCopilotBridge() {
}

void AgenticCopilotBridge::initialize(AgenticEngine* engine, ChatInterface* chat, MultiTabEditor* editor, TerminalPool* terminals, AgenticExecutor* executor) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_agenticEngine = engine;
    m_chatInterface = chat;
    m_multiTabEditor = editor;
    m_terminalPool = terminals;
    m_agenticExecutor = executor;

    // Zero-Touch: Initialize self-healing core on bridge startup
    PatchResult res = AgentSelfHealingOrchestrator::instance().initialize();
    if (!res.success) {
    // Self-healing initialization
    } else {
    // Autonomous orchestrator online
    }
}

std::string AgenticCopilotBridge::executeWithFailureRecovery(const std::string& prompt) {
    auto start = std::chrono::steady_clock::now();
    // Executing with autonomous recovery

    // Step 1: Run pre-execution healing cycle to ensure environment stability
    SelfHealReport preReport = AgentSelfHealingOrchestrator::instance().runHealingCycle();
    if (preReport.bugsFixed > 0) {
        // Fixed stability issues
    }

    std::string response;
    try {
        // Step 2: Invoke primary agentic engine
        if (!m_agenticEngine) return "Error: Engine offline";
        
        // Invoke primary agentic engine with fallback
        if (m_agenticEngine) {
            response = m_agenticEngine->process(prompt);
        } else {
            response = "Error: Engine not initialized";
        }

        // Step 3: Hotpatch and validate response
        JsonValue ctx = JsonValue::createObject();
        ctx["prompt"] = prompt;
        ctx["timestamp"] = (int)time(NULL);
        
        if (detectAndCorrectFailure(response, ctx)) {
             // Response corrected via AgentHotPatcher
        }

    } catch (const std::exception& e) {
        // Execution crash, attempting emergency heal
        AgentSelfHealingOrchestrator::instance().runHealingCycle();
        response = "Recovery completed. Please retry the prompt.";
    }

    auto end = std::chrono::steady_clock::now();
    long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    // Task completed

    return response;
}

bool AgenticCopilotBridge::detectAndCorrectFailure(std::string& response, const JsonValue& context) {
    // Zero-Touch: Intercept and patch via the centralized hotpatcher
    // This provides Cursor-like observability with real-time corrections
    
    // Simple self-detect logic before full integration
    if (response.empty() || response.find("Error") != std::string::npos) {
        SelfHealReport report = AgentSelfHealingOrchestrator::instance().runHealingCycle();
        if (report.bugsFixed > 0) {
            response += "\n[System Fix Applied: Found and repaired internal degradation]";
            return true;
        }
    }
    
    return false;
}

void AgenticCopilotBridge::completionReady(const std::string& result) {
    // Notify via callback if implemented
}

void AgenticCopilotBridge::analysisReady(const std::string& result) {
}

void AgenticCopilotBridge::errorOccurred(const std::string& error) {
    // Error
}
