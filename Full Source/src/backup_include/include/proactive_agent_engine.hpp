#pragma once
/**
 * @file proactive_agent_engine.hpp
 * @brief Proactive agent engine — stub for Win32_IDE / agentic flows.
 */
#include <string>
#include <vector>
#include <functional>
#include <memory>

class ProactiveAgentEngine {
public:
    static ProactiveAgentEngine* instance() {
        static ProactiveAgentEngine s_instance;
        return &s_instance;
    }
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }
    void suggestActions(const std::string& context, std::function<void(const std::vector<std::string>&)> cb) {
        if (cb) cb({});
    }
private:
    ProactiveAgentEngine() = default;
    bool m_enabled = false;
};
