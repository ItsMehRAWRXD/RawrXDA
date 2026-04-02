// ============================================================================
// mcp_client.h — Model Context Protocol Client Header
// JSON-RPC 2.0 based MCP client with tool/resource/prompt support
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace RawrXD {
namespace MCP {

struct Tool {
    std::string name;
    std::string description;
    nlohmann::json input_schema;
};

struct Resource {
    std::string uri;
    std::string name;
    std::string description;
    std::string mime_type;
};

struct Prompt {
    std::string name;
    std::string description;
    nlohmann::json arguments;
};

class MCPClient {
public:
    explicit MCPClient(const std::string& server_url);
    ~MCPClient();

    // Initialize connection to MCP server
    bool initialize();

    // Tool operations
    std::vector<Tool> list_tools();
    nlohmann::json call_tool(const std::string& name, const nlohmann::json& arguments = nullptr);

    // Resource operations
    std::vector<Resource> list_resources();
    std::string read_resource(const std::string& uri);

    // Prompt operations
    std::vector<Prompt> list_prompts();
    nlohmann::json get_prompt(const std::string& name, const nlohmann::json& arguments = nullptr);

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

}} // namespace RawrXD::MCP