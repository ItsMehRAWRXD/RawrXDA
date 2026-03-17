#include "Win32IDE_Autonomy.h"
#include "Win32NativeAgentAPI.h"
#include <sstream>

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
    LOG_INFO("Autonomy loop thread started");
    while (m_autoLoop.load()) {
        tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
    }
    LOG_INFO("Autonomy loop thread exiting");
}

std::string AutonomyManager::planNextAction() {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Extremely naive planner: if no goal -> idle
    if (m_goal.empty()) {
        return "NOOP";
    }
    // Example heuristic: examine memory size to decide next step
    if (m_memory.empty()) {
        return "tool:list_dir path=."; // gather context
    }
    // After some memory items, attempt summarization (placeholder prompt)
    if (m_memory.size() % 5 == 0) {
        return "prompt: Summarize recent observations concisely.";
    }
    return "prompt: Reflect on goal and propose next file to inspect.";
}

void AutonomyManager::executeAction(const std::string& action) {
    if (action == "NOOP") {
        LOG_DEBUG("Planner produced NOOP");
        return;
    }
    
    // Try native Win32 tools first
    if (action.rfind("win32:", 0) == 0) {
        std::string toolCall = action.substr(6);
        executeWin32Tool(toolCall);
        return;
    }
    
    // Fall back to agentic bridge for other actions
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
    LOG_INFO("Executed autonomy action: " + action);
}

void AutonomyManager::executeWin32Tool(const std::string& toolCall) {
    auto& win32API = RawrXD::Win32Agent::GetWin32AgentAPI();
    std::string result;
    
    // Parse tool name and arguments
    size_t colonPos = toolCall.find(':');
    if (colonPos == std::string::npos) {
        addObservation("WIN32_TOOL:" + toolCall + " => ERROR: Invalid format");
        return;
    }
    
    std::string toolName = toolCall.substr(0, colonPos);
    std::string args = toolCall.substr(colonPos + 1);
    
    try {
        if (toolName == "process") {
            // Parse arguments: action:pid or name:cmd
            size_t actionPos = args.find(':');
            if (actionPos != std::string::npos) {
                std::string action = args.substr(0, actionPos);
                std::string target = args.substr(actionPos + 1);
                
                if (action == "list") {
                    auto processes = win32API.GetProcessManager().EnumerateProcesses();
                    std::stringstream ss;
                    ss << "Running processes (" << processes.size() << "):\n";
                    for (const auto& proc : processes) {
                        ss << "- " << proc.processName << " (PID: " << proc.processId << ")\n";
                    }
                    result = ss.str();
                } else if (action == "kill") {
                    bool success = win32API.KillProcess(std::wstring(target.begin(), target.end()));
                    result = std::string("Process kill: ") + (success ? "SUCCESS" : "FAILED");
                }
            }
        } else if (toolName == "memory") {
            auto memInfo = win32API.GetMemoryManager().GetSystemMemoryInfo();
            std::stringstream ss;
            ss << "System Memory:\n";
            ss << "Total: " << memInfo.totalPhysical / (1024*1024) << " MB\n";
            ss << "Available: " << memInfo.availablePhysical / (1024*1024) << " MB\n";
            ss << "Load: " << memInfo.memoryLoad << "%\n";
            result = ss.str();
        } else if (toolName == "file") {
            // Parse: read:path or list:directory
            size_t actionPos = args.find(':');
            if (actionPos != std::string::npos) {
                std::string action = args.substr(0, actionPos);
                std::string target = args.substr(actionPos + 1);
                
                if (action == "read") {
                    std::wstring wtarget(target.begin(), target.end());
                    result = win32API.ReadTextFile(wtarget);
                } else if (action == "list") {
                    std::wstring wtarget(target.begin(), target.end());
                    auto files = win32API.ListFiles(wtarget);
                    std::stringstream ss;
                    ss << "Files in " << target << ":\n";
                    for (const auto& file : files) {
                        ss << "- " << std::string(file.begin(), file.end()) << "\n";
                    }
                    result = ss.str();
                }
            }
        } else if (toolName == "system") {
            auto info = win32API.GetSystemInfoManager().GetSystemInfo();
            std::stringstream ss;
            ss << "System Info:\n";
            ss << "OS: " << std::string(info.osVersion.begin(), info.osVersion.end()) << "\n";
            ss << "CPU Cores: " << info.processorCount << "\n";
            ss << "Memory: " << info.totalPhysicalMemory / (1024*1024*1024) << " GB\n";
            result = ss.str();
        } else {
            result = "Unknown Win32 tool: " + toolName;
        }
    } catch (const std::exception& e) {
        result = std::string("Win32 tool error: ") + e.what();
    }
    
    addObservation("WIN32_TOOL:" + toolCall + " => " + result);
    LOG_INFO("Executed Win32 tool: " + toolName);
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
