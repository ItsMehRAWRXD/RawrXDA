#include "agentic/tool_registry.h"

namespace RawrXD::Agentic
{
ToolRegistry& ToolRegistry::getInstance()
{
    static ToolRegistry inst;
    return inst;
}

bool ToolRegistry::registerTool(const ToolDefinition& def)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_tools.count(def.name))
        return false;
    m_tools[def.name] = def;
    return true;
}

bool ToolRegistry::unregisterTool(const std::string& name)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_tools.erase(name) > 0;
}

ToolResult ToolRegistry::invoke(const std::string& name, const nlohmann::json& params)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_tools.find(name);
    if (it == m_tools.end())
    {
        return ToolResult{false, {}, "Tool not found: " + name};
    }
    if (!it->second.handler)
    {
        return ToolResult{false, {}, "Tool handler null: " + name};
    }
    return it->second.handler(params);
}

bool ToolRegistry::getToolDefinition(const std::string& name, ToolDefinition& outDef) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_tools.find(name);
    if (it == m_tools.end())
        return false;
    outDef = it->second;
    return true;
}

std::vector<std::string> ToolRegistry::listTools() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> names;
    for (const auto& [name, _] : m_tools)
        names.push_back(name);
    return names;
}

nlohmann::json ToolRegistry::getSchema(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_tools.find(name);
    if (it != m_tools.end())
        return it->second.parameterSchema;
    return {};
}

bool ToolRegistry::hasTool(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_tools.count(name) > 0;
}

void ToolRegistry::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tools.clear();
}
}  // namespace RawrXD::Agentic
