#include "Win32IDE_Autonomy.h"
#include <filesystem>
#include <sstream>
#include <windows.h>

namespace {
std::wstring GetEnvVarW(const std::wstring& name) {
    DWORD size = GetEnvironmentVariableW(name.c_str(), nullptr, 0);
    if (size == 0) return L"";
    std::wstring value(size, L'\0');
    GetEnvironmentVariableW(name.c_str(), value.data(), size);
    if (!value.empty() && value.back() == L'\0') value.pop_back();
    return value;
}

std::wstring ResolveProjectRoot() {
    std::wstring envRoot = GetEnvVarW(L"RAWRXD_PROJECT_ROOT");
    if (!envRoot.empty()) return envRoot;
    return std::filesystem::current_path().wstring();
}

std::wstring ResolveRegistryPath(const std::wstring& root) {
    std::filesystem::path path = std::filesystem::path(root) / L"skills" / L"registry.json";
    return path.wstring();
}

bool LooksLikeJson(const std::string& value) {
    for (char c : value) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
        return c == '{' || c == '[';
    }
    return false;
}

std::string EscapeJsonString(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}
}

AutonomyManager::AutonomyManager(AgenticBridge* bridge)
    : m_toolRegistry(&RawrXD::Agent::ToolRegistry::Instance())
    , m_bridge(bridge), m_running(false), m_autoLoop(false),
      m_maxActionsPerMinute(30), m_actionsThisWindow(0) {
    m_windowStart = std::chrono::steady_clock::now();
    std::wstring projectRoot = ResolveProjectRoot();
    m_toolRegistry->SetProjectRoot(projectRoot);
    std::wstring registryPath = ResolveRegistryPath(projectRoot);
    if (!m_toolRegistry->LoadFromDisk(registryPath)) {
        LOG_WARNING("ToolRegistry load failed at: " + std::string(registryPath.begin(), registryPath.end()));
    }
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
        return "tool:GitOperation:{\"command\":\"status\"}"; // gather context
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
    // Differentiate tool vs prompt
    if (action.rfind("tool:", 0) == 0) {
        std::string toolCall = action.substr(5);
        std::string toolName = toolCall;
        std::string argsJson = "{}";
        size_t sep = toolCall.find(':');
        if (sep == std::string::npos) {
            sep = toolCall.find('|');
        }
        if (sep != std::string::npos) {
            toolName = toolCall.substr(0, sep);
            argsJson = toolCall.substr(sep + 1);
        }
        if (!LooksLikeJson(argsJson)) {
            argsJson = "{\"raw\":\"" + EscapeJsonString(argsJson) + "\"}";
        }
        std::string toolOutput;
        auto result = m_toolRegistry->Execute(toolName, argsJson, toolOutput);
        addObservation("TOOL:" + toolName + " => " + toolOutput + " (" + std::to_string(static_cast<int>(result)) + ")");
    } else if (action.rfind("prompt:", 0) == 0) {
        if (!m_bridge || !m_bridge->IsInitialized()) {
            LOG_WARNING("Bridge not initialized; cannot execute action: " + action);
            return;
        }
        std::string prompt = action.substr(7);
        auto resp = m_bridge->ExecuteAgentCommand(prompt);
        if (resp.type == AgentResponseType::TOOL_CALL) {
            std::string toolOutput;
            std::string argsJson = resp.toolArgs.empty() ? "{}" : resp.toolArgs;
            if (!LooksLikeJson(argsJson)) {
                argsJson = "{\"raw\":\"" + EscapeJsonString(argsJson) + "\"}";
            }
            auto result = m_toolRegistry->Execute(resp.toolName, argsJson, toolOutput);
            addObservation("TOOL:" + resp.toolName + " => " + toolOutput + " (" + std::to_string(static_cast<int>(result)) + ")");
        } else {
            addObservation("ANSWER:" + resp.content);
        }
    } else {
        if (!m_bridge || !m_bridge->IsInitialized()) {
            LOG_WARNING("Bridge not initialized; cannot execute action: " + action);
            return;
        }
        auto resp = m_bridge->ExecuteAgentCommand(action);
        if (resp.type == AgentResponseType::TOOL_CALL) {
            std::string toolOutput;
            std::string argsJson = resp.toolArgs.empty() ? "{}" : resp.toolArgs;
            if (!LooksLikeJson(argsJson)) {
                argsJson = "{\"raw\":\"" + EscapeJsonString(argsJson) + "\"}";
            }
            auto result = m_toolRegistry->Execute(resp.toolName, argsJson, toolOutput);
            addObservation("TOOL:" + resp.toolName + " => " + toolOutput + " (" + std::to_string(static_cast<int>(result)) + ")");
        } else {
            addObservation("RAW:" + resp.content);
        }
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
