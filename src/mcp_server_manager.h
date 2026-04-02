// ============================================================================
// mcp_server_manager.h — MCP Server Manager Header
// Manages multiple MCP servers and integrates with agent tool registry
// ============================================================================

#pragma once

#include "mcp_client.h"
#include <string>
#include <vector>
#include <memory>

namespace RawrXD {
namespace MCP {

class MCPServerManager {
public:
    static MCPServerManager& instance();

    void initialize();
    std::vector<std::string> get_available_servers() const;
    std::vector<Tool> get_server_tools(const std::string& server_name) const;

    nlohmann::json call_tool(const std::string& server_name,
                            const std::string& tool_name,
                            const nlohmann::json& arguments = nullptr);

private:
    MCPServerManager();
    ~MCPServerManager();

    class Impl;
    std::unique_ptr<Impl> pimpl;
};

}} // namespace RawrXD::MCP