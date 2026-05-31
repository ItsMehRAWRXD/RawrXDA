#pragma once
#include <functional>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>


namespace RawrXD::Agentic
{

struct ToolResult
{
    bool success = false;
    nlohmann::json data;
    std::string errorMessage;
};

using ToolHandler = std::function<ToolResult(const nlohmann::json& params)>;

struct ToolDefinition
{
    std::string name;
    std::string description;
    nlohmann::json parameterSchema;
    ToolHandler handler;
    bool requiresConfirmation = false;
    int32_t timeoutMs = 30000;
};

class ToolRegistry
{
  public:
    static ToolRegistry& getInstance();

    bool registerTool(const ToolDefinition& def);
    bool unregisterTool(const std::string& name);

    ToolResult invoke(const std::string& name, const nlohmann::json& params);

    /// Returns a copy of the registered tool definition. Intended for executors/validators.
    bool getToolDefinition(const std::string& name, ToolDefinition& outDef) const;

    std::vector<std::string> listTools() const;
    nlohmann::json getSchema(const std::string& name) const;
    bool hasTool(const std::string& name) const;

    void clear();

  private:
    ToolRegistry() = default;
    ~ToolRegistry() = default;

    ToolRegistry(const ToolRegistry&) = delete;
    ToolRegistry& operator=(const ToolRegistry&) = delete;

    mutable std::mutex m_mutex;
    std::map<std::string, ToolDefinition> m_tools;
};

}  // namespace RawrXD::Agentic
