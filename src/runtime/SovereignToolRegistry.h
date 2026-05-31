#pragma once

#include <string>
#include <vector>
#include <functional>
#include <map>
#include <mutex>
#include "SovereignToolBridge.h"

namespace RawrXD::Runtime {

struct ToolDefinition {
    std::string name;
    std::string description;
    std::string parameterSchema; // JSON or descriptive
    std::function<std::string(const std::string&)> implementation;
};

class SovereignToolRegistry {
public:
    static SovereignToolRegistry& instance();

    void registerTool(const ToolDefinition& tool);
    std::string executeToolSync(const std::string& name, const std::string& args);
    
    // Asynchronous dispatch via MASM ToolEngine
    uint32_t dispatchToolAsync(const std::string& name, const std::string& args);

    const std::vector<ToolDefinition>& getTools() const { return m_tools; }

private:
    SovereignToolRegistry();
    void registerCoreTools();

    std::vector<ToolDefinition> m_tools;
    std::map<std::string, size_t> m_toolMap;
    std::mutex m_mutex;
};

} // namespace RawrXD::Runtime
