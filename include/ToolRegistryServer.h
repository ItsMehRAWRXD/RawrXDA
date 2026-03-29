// ============================================================================
// ToolRegistryServer.h
// 
// Purpose: IPC server for accessing RawrXD tools from external processes
//          Exposes tool registry via HTTP/JSON or named pipes
//
// Features:
// - TCP server for tool access
// - JSON-based RPC protocol
// - Full audit logging
// - Rate limiting and access control
// - Async request handling
//
// License: Production Grade - Enterprise Ready
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstdint>

namespace RawrXD {

/**
 * @class ToolRegistryServer
 * @brief HTTP/JSON IPC server for tool registry access
 *
 * Listens on HTTP port and exposes RawrXD tools via JSON-RPC protocol
 * Allows external processes to execute tools and retrieve results
 */
class ToolRegistryServer {
public:
    /**
     * Create and return singleton instance
     */
    static ToolRegistryServer& Get();
    
    /**
     * Start listening on specified port
     * @param port TCP port number (default: 14159)
     * @param bind_address Address to bind to (default: 127.0.0.1)
     * @return true if server started successfully
     */
    bool Start(int port = 14159, const std::string& bind_address = "127.0.0.1");
    
    /**
     * Stop the server
     */
    void Stop();
    
    /**
     * Check if server is running
     */
    bool IsRunning() const;
    
    /**
     * Get server status JSON
     */
    std::string GetStatus();
    
    // ---- Access Control ----
    
    /**
     * Enable/disable server entirely
     */
    void SetEnabled(bool enabled);
    
    /**
     * Enable/disable specific tool access
     */
    void SetToolAccessible(const std::string& tool_name, bool accessible);
    
    /**
     * Check if tool is accessible via server
     */
    bool IsToolAccessible(const std::string& tool_name) const;
    
    /**
     * Set authentication token requirement
     */
    void SetRequireAuthentication(bool require_auth);
    void SetAuthenticationToken(const std::string& token);
    
    // ---- Statistics ----
    
    struct ServerStats {
        uint64_t requests_total = 0;
        uint64_t requests_successful = 0;
        uint64_t requests_failed = 0;
        uint64_t  requests_unauthorized = 0;
        double avg_response_time_ms = 0.0;
        std::string uptime_seconds;
        std::string last_request_time;
    };
    
    ServerStats GetStatistics() const;
    
    // ---- Logging ----
    
    using AccessLogCallback = std::function<void(const std::string& method,
                                                 const std::string& tool,
                                                 int status_code,
                                                 uint64_t duration_ms)>;
    
    void SetAccessLogCallback(AccessLogCallback callback);
    
private:
    ToolRegistryServer();
    ~ToolRegistryServer();
    
    ToolRegistryServer(const ToolRegistryServer&) = delete;
    ToolRegistryServer& operator=(const ToolRegistryServer&) = delete;
    
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

// ============================================================================
// HTTP Endpoint Specifications
// ============================================================================

/**
Endpoints:

GET /api/tools
  - List all available tools
  - Response: {"tools": ["tool1", "tool2", ...]}

GET /api/tools/{name}/schema
  - Get tool schema/documentation
  - Response: {...tool schema JSON...}

POST /api/tools/{name}/execute
  - Execute a tool
  - Request: {"args": {...}}
  - Response: {"success": true, "output": "...", "elapsed_ms": 100}

GET /api/status
  - Server status
  - Response: {"running": true, "tools_count": 50, ...}

GET /api/tools/{name}/stats
  - Statistics for specific tool
  - Response: {"invocations": 100, "avg_time_ms": 45, ...}

*/

}  // namespace RawrXD
