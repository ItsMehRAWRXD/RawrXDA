#pragma once
#include <string>
#include <functional>
#include <map>
#include <mutex>
#include <vector>
#include <iostream>

namespace RawrXD {

class ToolRegistry {
public:
    using ToolFunction = std::function<std::string(const std::string&)>;
    
    void registerTool(const std::string& name, ToolFunction func) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_tools[name] = func;
    }
    
    std::string executeTool(const std::string& name, const std::string& args) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_tools.find(name) != m_tools.end()) {
            return m_tools[name](args);
        }
        return "Error: Tool not found: " + name;
    }
    
    std::vector<std::string> getAvailableTools() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::string> tools;
        for (const auto& [name, _] : m_tools) {
            tools.push_back(name);
        }
        return tools;
    }

private:
    std::map<std::string, ToolFunction> m_tools;
    mutable std::mutex m_mutex;
};

} // namespace RawrXD
