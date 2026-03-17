/**
 * @file proactive_agent_engine.hpp
 * @brief Proactive agent engine (stub for patchable build).
 */
#pragma once

#include <string>
#include <vector>
#include <functional>

class ProactiveAgentEngine {
public:
    ProactiveAgentEngine() = default;
    void start() { m_running = true; }
    void stop() { m_running = false; }
    void setCallback(std::function<void(const std::string&)> cb) { m_callback = std::move(cb); }
    void suggest(const std::string& context) {
        if (!m_running || !m_callback || context.empty()) return;
        // Generate basic contextual suggestion based on keywords
        if (context.find("error") != std::string::npos || context.find("Error") != std::string::npos)
            m_callback("Consider adding error handling or checking the return value.");
        else if (context.find("TODO") != std::string::npos)
            m_callback("There are TODO items in the current context that need attention.");
        else if (context.find("#include") != std::string::npos)
            m_callback("Tip: Check for unused includes to reduce compile times.");
    }
private:
    bool m_running = false;
    std::function<void(const std::string&)> m_callback;
};
