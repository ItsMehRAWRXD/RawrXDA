/**
 * @file mcp_server_host.hpp
 * @brief Model Context Protocol (MCP) Server for RawrXD
 *
 * Allows external tools/agents to use RawrXD tools via standard protocol.
 */

#pragma once

#include "tool_execution_engine.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <nlohmann/json.hpp>

namespace RawrXD {

/**
 * @class MCPServerHost
 * @brief Simple MCP server host using local Unix/Named pipes
 */
class MCPServerHost {
public:
    explicit MCPServerHost(ToolExecutionEngine* engine);
    ~MCPServerHost();

    /**
     * @brief Start listening for MCP connections
     */
    bool start(const std::string& serverName = "RawrXD_MCP");

    /**
     * @brief Stop the server
     */
    void stop();

    /**
     * @brief Check if server is running
     */
    bool isRunning() const { return m_running.load(); }

    // Callbacks (replacing Qt signals)
    using ClientConnectedCallback = std::function<void(const std::string& clientId)>;
    using ClientDisconnectedCallback = std::function<void(const std::string& clientId)>;
    using MessageReceivedCallback = std::function<void(const nlohmann::json& message)>;
    using ErrorOccurredCallback = std::function<void(const std::string& error)>;
    
    void setClientConnectedCallback(ClientConnectedCallback cb) { m_onClientConnected = cb; }
    void setClientDisconnectedCallback(ClientDisconnectedCallback cb) { m_onClientDisconnected = cb; }
    void setMessageReceivedCallback(MessageReceivedCallback cb) { m_onMessageReceived = cb; }
    void setErrorOccurredCallback(ErrorOccurredCallback cb) { m_onErrorOccurred = cb; }

private:
    void processRequest(void* handle, const nlohmann::json& request);
    void sendResponse(void* handle, const nlohmann::json& response);
    void sendError(void* handle, const nlohmann::json& id, int code, const std::string& message);

    class Impl;
    std::unique_ptr<Impl> m_impl;
    
    ToolExecutionEngine* m_engine;
    std::atomic<bool> m_running{false};
    std::string m_serverName;
    
    // Callbacks
    ClientConnectedCallback m_onClientConnected;
    ClientDisconnectedCallback m_onClientDisconnected;
    MessageReceivedCallback m_onMessageReceived;
    ErrorOccurredCallback m_onErrorOccurred;
};

} // namespace RawrXD
