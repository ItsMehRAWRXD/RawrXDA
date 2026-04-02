// ============================================================================
// mcp_server_manager.cpp — MCP Server Manager for RawrXD IDE
// Manages multiple MCP servers and integrates with agent tool registry
// ============================================================================

#include "mcp_server_manager.h"
#include "mcp_client.h"
#include "tool_registry.h"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace RawrXD {
namespace MCP {

class MCPServerManager::Impl {
public:
    std::vector<std::unique_ptr<MCPClient>> clients;
    std::unordered_map<std::string, std::vector<Tool>> server_tools;
    std::unordered_map<std::string, std::vector<Resource>> server_resources;
    std::unordered_map<std::string, std::vector<Prompt>> server_prompts;

    // Load MCP server configurations from .mcp.json files
    void load_server_configs() {
        namespace fs = std::filesystem;

        // Scan project root for .mcp.json files
        fs::path project_root = fs::current_path();
        for (const auto& entry : fs::recursive_directory_iterator(project_root)) {
            if (entry.is_regular_file() && entry.path().extension() == ".mcp.json") {
                load_server_config(entry.path());
            }
        }
    }

    void load_server_config(const std::filesystem::path& config_path) {
        try {
            std::ifstream file(config_path);
            nlohmann::json config;
            file >> config;

            std::string server_url = config["server"]["url"];
            std::string server_name = config["name"];

            std::cout << "[MCP] Loading server config: " << server_name
                      << " at " << server_url << std::endl;

            auto client = std::make_unique<MCPClient>(server_url);
            if (client->initialize()) {
                clients.push_back(std::move(client));

                // Cache server capabilities
                server_tools[server_name] = clients.back()->list_tools();
                server_resources[server_name] = clients.back()->list_resources();
                server_prompts[server_name] = clients.back()->list_prompts();

                std::cout << "[MCP] Server " << server_name << " initialized with "
                          << server_tools[server_name].size() << " tools" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "[MCP] Failed to load config " << config_path << ": " << e.what() << std::endl;
        }
    }

    // Register all MCP tools with the agent tool registry
    void register_tools_with_agent_system() {
        for (const auto& [server_name, tools] : server_tools) {
            for (const auto& tool : tools) {
                // Create agent tool wrapper
                auto tool_wrapper = [this, server_name, tool_name = tool.name]
                    (const nlohmann::json& args) -> nlohmann::json {
                    // Find the client for this server
                    for (auto& client : clients) {
                        try {
                            return client->call_tool(tool_name, args);
                        } catch (...) {
                            continue; // Try next client
                        }
                    }
                    throw std::runtime_error("No MCP server available for tool: " + tool_name);
                };

                // Register with agent system
                ToolRegistry::instance().register_tool(
                    "mcp_" + server_name + "_" + tool.name,
                    tool.description,
                    tool_wrapper
                );
            }
        }
    }
};

MCPServerManager::MCPServerManager() : pimpl(std::make_unique<Impl>()) {}

MCPServerManager::~MCPServerManager() = default;

MCPServerManager& MCPServerManager::instance() {
    static MCPServerManager instance;
    return instance;
}

void MCPServerManager::initialize() {
    std::cout << "[MCP] Initializing MCP Server Manager..." << std::endl;

    pimpl->load_server_configs();
    pimpl->register_tools_with_agent_system();

    std::cout << "[MCP] MCP integration complete. "
              << pimpl->clients.size() << " servers connected." << std::endl;
}

std::vector<std::string> MCPServerManager::get_available_servers() const {
    std::vector<std::string> servers;
    for (const auto& [name, _] : pimpl->server_tools) {
        servers.push_back(name);
    }
    return servers;
}

std::vector<Tool> MCPServerManager::get_server_tools(const std::string& server_name) const {
    auto it = pimpl->server_tools.find(server_name);
    return it != pimpl->server_tools.end() ? it->second : std::vector<Tool>{};
}

nlohmann::json MCPServerManager::call_tool(const std::string& server_name,
                                          const std::string& tool_name,
                                          const nlohmann::json& arguments) {
    // Find the appropriate client
    for (auto& client : pimpl->clients) {
        // This is a simplified implementation - in practice you'd need to track
        // which client belongs to which server
        try {
            return client->call_tool(tool_name, arguments);
        } catch (...) {
            continue;
        }
    }

    throw std::runtime_error("Tool not found: " + tool_name + " on server: " + server_name);
}

}} // namespace RawrXD::MCP