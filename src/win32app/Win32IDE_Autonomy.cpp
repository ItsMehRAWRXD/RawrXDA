#include "Win32IDE_Autonomy.h"
#include "IDEConfig.h"
#include <sstream>

// SCAFFOLD_056: AutonomyManager executeAction


// SCAFFOLD_055: AutonomyManager planNextAction


// SCAFFOLD_021: Autonomy manager and goal loop


AutonomyManager::AutonomyManager(AgenticBridge* bridge)
    : m_bridge(bridge), m_running(false), m_autoLoop(false),
      m_maxActionsPerMinute(30), m_actionsThisWindow(0) {
    m_windowStart = std::chrono::steady_clock::now();
    LOG_INFO("AutonomyManager constructed");
}

AutonomyManager::~AutonomyManager() {
    stop();
    LOG_INFO("AutonomyManager destroyed");
}

void AutonomyManager::start() {
    if (m_running.load()) return;
    m_running.store(true);
    LOG_INFO("Autonomy started");
}

void AutonomyManager::stop() {
    m_autoLoop.store(false);
    if (m_running.load()) {
        m_running.store(false);
    }
    if (m_loopThread.joinable()) {
        m_loopThread.join();
    }
    LOG_INFO("Autonomy stopped");
}

void AutonomyManager::enableAutoLoop(bool enable) {
    if (enable && !m_autoLoop.load()) {
        if (!m_running.load()) start();
        m_autoLoop.store(true);
        m_loopThread = std::thread([this]{ loop(); });
        LOG_INFO("Autonomy auto loop enabled");
    } else if (!enable && m_autoLoop.load()) {
        m_autoLoop.store(false);
        LOG_INFO("Autonomy auto loop disabled");
    }
}

void AutonomyManager::setGoal(const std::string& goal) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_goal = goal;
    LOG_INFO("Goal set: " + goal);
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
    LOG_DEBUG("Observation added");
}

std::vector<std::string> AutonomyManager::getMemorySnapshot() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_memory;
}

void AutonomyManager::tick() {
    SCOPED_METRIC("autonomy.tick");
    if (!m_running.load()) return;
    if (!rateLimitAllow()) {
        METRICS.increment("autonomy.rate_limited");
        LOG_WARNING("Rate limit hit, skipping tick");
        return;
    }
    METRICS.increment("autonomy.ticks_executed");
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
    LOG_INFO("Autonomy loop thread started");
    while (m_autoLoop.load()) {
        tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
    }
    LOG_INFO("Autonomy loop thread exiting");
}

std::string AutonomyManager::planNextAction() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_goal.empty()) {
        return "NOOP";
    }
    
    // Use last observation to drive the loop
    if (m_memory.empty()) {
        // Initial Prompt
        return "prompt: You are an autonomous agent. Your Goal: " + m_goal + ".\n"
               "Available Tools:\n"
               "- tool:list_dir path=.\n"
               "- tool:read_file path=...\n"
               "- tool:write_file path=... content=...\n"
               "- tool:exec_cmd cmd=... (executes system command/powershell)\n"
               "- TOOL:runSubagent:{\"description\":\"...\",\"prompt\":\"...\"} (spawn a sub-agent for subtasks)\n"
               "- TOOL:chain:{\"steps\":[\"step1\",\"step2 with {{input}}\"],\"input\":\"...\"} (sequential pipeline)\n"
               "- TOOL:hexmag_swarm:{\"prompts\":[\"task1\",\"task2\"],\"strategy\":\"concatenate\",\"maxParallel\":4} (parallel fan-out)\n"
               "- TOOL:manage_todo_list:[{\"id\":1,\"title\":\"...\",\"status\":\"not-started\"},...] (task tracking)\n"
               "\n"
               "Decide the first step. Output ONLY the tool command starting with 'tool:' or 'TOOL:'.";
    }

    std::string lastObs = m_memory.back();
    
    // If the last thing we saw was an ANSWER from the Agent
    if (lastObs.rfind("ANSWER:", 0) == 0) {
        std::string content = lastObs.substr(7); // strip preamble
        // trim
        const char* ws = " \t\n\r\f\v";
        content.erase(0, content.find_first_not_of(ws));
        content.erase(content.find_last_not_of(ws) + 1);
        
        // If it gave us a tool command, execute it next
        if (content.rfind("tool:", 0) == 0) {
            return content;
        }
        
        // If it didn't give a tool command, ask for one
        // Check if it considers the task done
        if (content.find("TASK_COMPLETE") != std::string::npos) {
            LOG_INFO("Agent signalled completion.");
            return "NOOP";
        }

        return "prompt: Please format your next step as a single tool command starting with 'tool:'. If done, say 'TASK_COMPLETE'.";
    }
    
    // If the last thing was a TOOL result (or RAW), we need to show it to the Agent and ask "What next?"
    // Build brief context
    std::string context;
    int items = 0;
    for (auto it = m_memory.rbegin(); it != m_memory.rend(); ++it) {
        if (++items > 5) break; // Last 5 items
        context = *it + "\n" + context;
    }
    
    return "prompt: History:\n" + context + "\n"
           "Based on the above results, what is the next step? "
           "Output ONLY the tool command starting with 'tool:' or 'TOOL:'. "
           "You may use runSubagent, chain, hexmag_swarm, or manage_todo_list tools.";
}

void AutonomyManager::executeAction(const std::string& action) {
    SCOPED_METRIC("autonomy.execute_action");
    if (action == "NOOP") {
        LOG_DEBUG("Planner produced NOOP");
        return;
    }
    METRICS.increment("autonomy.actions_executed");
    if (!m_bridge || !m_bridge->IsInitialized()) {
        LOG_WARNING("Bridge not initialized; cannot execute action: " + action);
        return;
    }
    // Differentiate tool vs prompt
    if (action.rfind("tool:", 0) == 0) {
        std::string toolCall = action.substr(5);

        // Check if this is a subagent/chain/swarm tool call
        std::string toolResult;
        if (m_bridge->DispatchModelToolCalls(toolCall, toolResult)) {
            addObservation("TOOL_DISPATCH:" + toolCall + " => " + toolResult);
            LOG_INFO("Autonomy dispatched subagent tool: " + toolCall);
            return;
        }

        auto resp = m_bridge->ExecuteAgentCommand(toolCall);

        // Also check the response for embedded tool calls
        if (m_bridge->DispatchModelToolCalls(resp.content, toolResult)) {
            addObservation("TOOL:" + toolCall + " => " + resp.content + "\n[Tool Result] " + toolResult);
        } else {
            addObservation("TOOL:" + toolCall + " => " + resp.content);
        }
    } else if (action.rfind("prompt:", 0) == 0) {
        std::string prompt = action.substr(7);
        auto resp = m_bridge->ExecuteAgentCommand(prompt);

        // Check if the model's answer contains tool calls
        std::string toolResult;
        if (m_bridge->DispatchModelToolCalls(resp.content, toolResult)) {
            addObservation("ANSWER:" + resp.content + "\n[Tool Result] " + toolResult);
        } else {
            addObservation("ANSWER:" + resp.content);
        }
    } else {
        auto resp = m_bridge->ExecuteAgentCommand(action);
        addObservation("RAW:" + resp.content);
    }
    LOG_INFO("Executed autonomy action: " + action);
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
