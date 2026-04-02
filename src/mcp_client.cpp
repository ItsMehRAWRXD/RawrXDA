// ============================================================================
// mcp_client.cpp — Model Context Protocol Client Implementation
// JSON-RPC 2.0 based MCP client with tool/resource/prompt support
// ============================================================================

#include "mcp_client.h"
#include <nlohmann/json.hpp>
#include <httplib.h>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <memory>

namespace RawrXD {
namespace MCP {

class MCPClient::Impl {
public:
    std::string server_url;
    httplib::Client http_client;
    int next_id = 1;
    std::unordered_map<std::string, std::function<void(const nlohmann::json&)>> pending_requests;

    Impl(const std::string& url) : server_url(url), http_client(url) {}

    nlohmann::json send_request(const std::string& method, const nlohmann::json& params = nullptr) {
        nlohmann::json request = {
            {"jsonrpc", "2.0"},
            {"id", next_id++},
            {"method", method}
        };

        if (!params.is_null()) {
            request["params"] = params;
        }

        auto response = http_client.Post("/jsonrpc", request.dump(),
            "application/json");

        if (response && response->status == 200) {
            auto resp_json = nlohmann::json::parse(response->body);
            if (resp_json.contains("error")) {
                throw std::runtime_error("MCP Error: " + resp_json["error"]["message"].get<std::string>());
            }
            return resp_json["result"];
        }

        throw std::runtime_error("MCP request failed");
    }
};

MCPClient::MCPClient(const std::string& server_url)
    : pimpl(std::make_unique<Impl>(server_url)) {}

MCPClient::~MCPClient() = default;

// Initialize MCP connection
bool MCPClient::initialize() {
    try {
        // Send initialize request
        nlohmann::json init_params = {
            {"protocolVersion", "2024-11-05"},
            {"capabilities", {
                {"tools", {{"listChanged", true}}},
                {"resources", {{"listChanged", true}}},
                {"prompts", {{"listChanged", true}}}
            }},
            {"clientInfo", {
                {"name", "RawrXD IDE"},
                {"version", "v23.800B-Swarm"}
            }}
        };

        auto result = pimpl->send_request("initialize", init_params);
        std::cout << "[MCP] Initialized with server: " << result.dump() << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[MCP] Initialization failed: " << e.what() << std::endl;
        return false;
    }
}

// List available tools
std::vector<Tool> MCPClient::list_tools() {
    try {
        auto result = pimpl->send_request("tools/list");
        std::vector<Tool> tools;

        for (const auto& tool_json : result["tools"]) {
            Tool tool;
            tool.name = tool_json["name"];
            tool.description = tool_json["description"];
            tool.input_schema = tool_json["inputSchema"];
            tools.push_back(tool);
        }

        return tools;
    } catch (const std::exception& e) {
        std::cerr << "[MCP] Failed to list tools: " << e.what() << std::endl;
        return {};
    }
}

// Call a tool
nlohmann::json MCPClient::call_tool(const std::string& name, const nlohmann::json& arguments) {
    try {
        nlohmann::json params = {
            {"name", name},
            {"arguments", arguments}
        };

        return pimpl->send_request("tools/call", params);
    } catch (const std::exception& e) {
        std::cerr << "[MCP] Tool call failed: " << e.what() << std::endl;
        return nullptr;
    }
}

// List resources
std::vector<Resource> MCPClient::list_resources() {
    try {
        auto result = pimpl->send_request("resources/list");
        std::vector<Resource> resources;

        for (const auto& res_json : result["resources"]) {
            Resource res;
            res.uri = res_json["uri"];
            res.name = res_json["name"];
            res.description = res_json.value("description", "");
            res.mime_type = res_json.value("mimeType", "");
            resources.push_back(res);
        }

        return resources;
    } catch (const std::exception& e) {
        std::cerr << "[MCP] Failed to list resources: " << e.what() << std::endl;
        return {};
    }
}

// Read resource
std::string MCPClient::read_resource(const std::string& uri) {
    try {
        nlohmann::json params = {{"uri", uri}};
        auto result = pimpl->send_request("resources/read", params);

        return result["contents"][0]["text"];
    } catch (const std::exception& e) {
        std::cerr << "[MCP] Failed to read resource: " << e.what() << std::endl;
        return "";
    }
}

// List prompts
std::vector<Prompt> MCPClient::list_prompts() {
    try {
        auto result = pimpl->send_request("prompts/list");
        std::vector<Prompt> prompts;

        for (const auto& prompt_json : result["prompts"]) {
            Prompt prompt;
            prompt.name = prompt_json["name"];
            prompt.description = prompt_json.value("description", "");
            prompt.arguments = prompt_json.value("arguments", nlohmann::json::array());
            prompts.push_back(prompt);
        }

        return prompts;
    } catch (const std::exception& e) {
        std::cerr << "[MCP] Failed to list prompts: " << e.what() << std::endl;
        return {};
    }
}

// Get prompt
nlohmann::json MCPClient::get_prompt(const std::string& name, const nlohmann::json& arguments) {
    try {
        nlohmann::json params = {{"name", name}};
        if (!arguments.is_null()) {
            params["arguments"] = arguments;
        }

        return pimpl->send_request("prompts/get", params);
    } catch (const std::exception& e) {
        std::cerr << "[MCP] Failed to get prompt: " << e.what() << std::endl;
        return nullptr;
    }
}

}} // namespace RawrXD::MCP