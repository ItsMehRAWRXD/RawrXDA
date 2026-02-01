#include "Win32IDE_Autonomy.h"
#include <sstream>

AutonomyManager::AutonomyManager(AgenticBridge* bridge)
    : m_bridge(bridge), m_running(false), m_autoLoop(false),
      m_maxActionsPerMinute(30), m_actionsThisWindow(0) {
    m_windowStart = std::chrono::steady_clock::now();

}

AutonomyManager::~AutonomyManager() {
    stop();

}

void AutonomyManager::start() {
    if (m_running.load()) return;
    m_running.store(true);

}

void AutonomyManager::stop() {
    m_autoLoop.store(false);
    if (m_running.load()) {
        m_running.store(false);
    }
    if (m_loopThread.joinable()) {
        m_loopThread.join();
    }

}

void AutonomyManager::enableAutoLoop(bool enable) {
    if (enable && !m_autoLoop.load()) {
        if (!m_running.load()) start();
        m_autoLoop.store(true);
        m_loopThread = std::thread([this]{ loop(); });

    } else if (!enable && m_autoLoop.load()) {
        m_autoLoop.store(false);

    }
}

void AutonomyManager::setGoal(const std::string& goal) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_goal = goal;

}

std::string AutonomyManager::getGoal() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_goal;
}

void AutonomyManager::addObservation(const std::string& obs) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_memory.push_back(obs);
    if (m_memory.size() > 2048) {
        m_memory.erase(m_memory.begin()); // simple cap
    }

}

std::vector<std::string> AutonomyManager::getMemorySnapshot() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_memory;
}

void AutonomyManager::tick() {
    if (!m_running.load()) return;
    if (!rateLimitAllow()) {
        LOG_WARNING("Rate limit hit, skipping tick");
        return;
    }
    std::string action = planNextAction();
    executeAction(action);
}

std::string AutonomyManager::getStatus() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream oss;
    oss << "running=" << (m_running.load() ? "true" : "false")
        << " autoLoop=" << (m_autoLoop.load() ? "true" : "false")
        << " goal='" << m_goal << "' memoryItems=" << m_memory.size()
        << " actionsWindow=" << m_actionsThisWindow << "/" << m_maxActionsPerMinute;
    return oss.str();
}

void AutonomyManager::loop() {

    while (m_autoLoop.load()) {
        tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
    }

}

std::string AutonomyManager::planNextAction() {
    std::string currentGoal;
    std::vector<std::string> currentMemory;

    // Snapshot state (Critical Section)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_goal.empty()) {
            return "NOOP";
        }
        currentGoal = m_goal;
        currentMemory = m_memory;
    }
    
    // Real Intelligence: Query the Agentic Bridge (LLM)
    // This removes the simulated heuristic logic.
    if (m_bridge && m_bridge->IsInitialized()) {
        std::stringstream prompt;
        prompt << "SYSTEM: You are an autonomous coding agent.\n";
        prompt << "GOAL: " << currentGoal << "\n";
        prompt << "MEMORY:\n";
        size_t memStart = currentMemory.size() > 10 ? currentMemory.size() - 10 : 0;
        for (size_t i = memStart; i < currentMemory.size(); ++i) {
            prompt << "> " << currentMemory[i] << "\n";
        }
        prompt << "\nINSTRUCTION: Decide the next single action to make progress.\n";
        prompt << "FORMAT: 'tool:<name> <args>' or 'prompt:<thought_or_query>'.\n";
        prompt << "RESPONSE:";
        
        AgentResponse resp = m_bridge->ExecuteAgentCommand(prompt.str());
        std::string action = resp.content;
        
        // Basic sanitization
        if (action.find("tool:") == 0 || action.find("prompt:") == 0) {
            return action;
        }
        // Fallback if LLM creates unstructured output
        return "prompt: " + action;
    }

    return "NOOP";
}

void AutonomyManager::executeAction(const std::string& action) {
    if (action == "NOOP") {

        return;
    }
    if (!m_bridge || !m_bridge->IsInitialized()) {
        LOG_WARNING("Bridge not initialized; cannot execute action: " + action);
        return;
    }
    // Differentiate tool vs prompt
    if (action.rfind("tool:", 0) == 0) {
        std::string toolCall = action.substr(5);
        auto resp = m_bridge->ExecuteAgentCommand(toolCall);
        addObservation("TOOL:" + toolCall + " => " + resp.content);
    } else if (action.rfind("prompt:", 0) == 0) {
        std::string prompt = action.substr(7);
        auto resp = m_bridge->ExecuteAgentCommand(prompt);
        addObservation("ANSWER:" + resp.content);
    } else {
        auto resp = m_bridge->ExecuteAgentCommand(action);
        addObservation("RAW:" + resp.content);
    }

}

bool AutonomyManager::rateLimitAllow() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_windowStart).count();
    if (elapsed >= 60) {
        m_windowStart = now;
        m_actionsThisWindow = 0;
    }
    if (m_actionsThisWindow >= m_maxActionsPerMinute) {
        return false;
    }
    ++m_actionsThisWindow;
    return true;
}
