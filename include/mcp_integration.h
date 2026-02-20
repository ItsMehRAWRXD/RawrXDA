// MCP Integration — Model Context Protocol for RawrXD IDE
// Full implementation of MCP server/client for tool-based agent interactions
// Generated: 2026-01-25 06:34:12 | Completed: 2026-02-08

#ifndef MCP_INTEGRATION_H_
#define MCP_INTEGRATION_H_

#include <string>
#include <vector>
#include <functional>
#include <map>
#include <mutex>
#include <memory>
#include <cstdint>

namespace RawrXD {
namespace MCP {

// ============================================================================
// MCP Protocol Types (JSON-RPC 2.0 based)
// ============================================================================

enum class MCPMessageType {
    Request,
    Response,
    Notification
};

enum class MCPErrorCode : int {
    ParseError      = -32700,
    InvalidRequest  = -32600,
    MethodNotFound  = -32601,
    InvalidParams   = -32602,
    InternalError   = -32603,
    ServerNotReady  = -32000,
    ToolNotFound    = -32001,
    ResourceNotFound = -32002
};

struct MCPError {
    MCPErrorCode code;
    std::string message;
    std::string data;
};

struct MCPRequest {
    std::string jsonrpc = "2.0";
    int64_t id = 0;
    std::string method;
    std::string params;   // Raw JSON string
};

struct MCPResponse {
    std::string jsonrpc = "2.0";
    int64_t id = 0;
    bool success = false;
    std::string result;   // Raw JSON string
    MCPError error;
};

struct MCPNotification {
    std::string jsonrpc = "2.0";
    std::string method;
    std::string params;
};

// ============================================================================
// Tool Definition
// ============================================================================

struct ToolParameter {
    std::string name;
    std::string type;         // "string", "number", "boolean", "object", "array"
    std::string description;
    bool required = false;
    std::string defaultValue;
};

struct ToolDefinition {
    std::string name;
    std::string description;
    std::vector<ToolParameter> parameters;
    std::string returnType;
};

struct ToolResult {
    bool success = false;
    std::string content;      // JSON or plain text
    std::string contentType;  // "text/plain", "application/json"
    bool isError = false;
    std::string errorMessage;

    static ToolResult ok(const std::string& content, const std::string& type = "application/json") {
        ToolResult r;
        r.success = true;
        r.content = content;
        r.contentType = type;
        return r;
    }
    static ToolResult fail(const std::string& msg) {
        ToolResult r;
        r.success = false;
        r.isError = true;
        r.errorMessage = msg;
        return r;
    }
};

using ToolHandler = std::function<ToolResult(const std::string& paramsJson)>;

// ============================================================================
// Resource Definition
// ============================================================================

struct ResourceDefinition {
    std::string uri;          // e.g. "file:///workspace/src/main.cpp"
    std::string name;
    std::string description;
    std::string mimeType;
};

struct ResourceContent {
    std::string uri;
    std::string content;
    std::string mimeType;
};

using ResourceHandler = std::function<ResourceContent(const std::string& uri)>;

// ============================================================================
// Prompt Template
// ============================================================================

struct PromptArgument {
    std::string name;
    std::string description;
    bool required = false;
};

struct PromptTemplate {
    std::string name;
    std::string description;
    std::vector<PromptArgument> arguments;
};

struct PromptMessage {
    std::string role;     // "user", "assistant", "system"
    std::string content;
};

using PromptHandler = std::function<std::vector<PromptMessage>(const std::string& argsJson)>;

// ============================================================================
// Server Capabilities
// ============================================================================

struct ServerCapabilities {
    bool supportsTools      = true;
    bool supportsResources  = true;
    bool supportsPrompts    = true;
    bool supportsLogging    = true;
    bool supportsSampling   = false;
    std::string protocolVersion = "2024-11-05";
};

struct ServerInfo {
    std::string name    = "RawrXD-MCP-Server";
    std::string version = "1.0.0";
};

// ============================================================================
// MCP Server — Hosts tools, resources, prompts
// ============================================================================

class MCPServer {
public:
    MCPServer();
    ~MCPServer();

    // Lifecycle
    bool initialize(const ServerInfo& info = {});
    void shutdown();
    bool isRunning() const { return m_running; }

    // Tool registration
    void registerTool(const ToolDefinition& def, ToolHandler handler);
    void unregisterTool(const std::string& name);
    std::vector<ToolDefinition> listTools() const;

    // Resource registration
    void registerResource(const ResourceDefinition& def, ResourceHandler handler);
    void unregisterResource(const std::string& uri);
    std::vector<ResourceDefinition> listResources() const;

    // Prompt registration
    void registerPrompt(const PromptTemplate& tmpl, PromptHandler handler);
    std::vector<PromptTemplate> listPrompts() const;

    // Message handling (JSON-RPC dispatch)
    std::string handleMessage(const std::string& rawJson);

    // Transport: stdio-based (for subprocess MCP servers)
    bool startStdioTransport();
    void stopTransport();

    // Stats
    uint64_t totalRequests() const { return m_totalRequests; }
    uint64_t totalErrors()   const { return m_totalErrors; }

private:
    // JSON-RPC dispatch
    MCPResponse dispatch(const MCPRequest& req);
    MCPResponse handleInitialize(const MCPRequest& req);
    MCPResponse handleToolsList(const MCPRequest& req);
    MCPResponse handleToolsCall(const MCPRequest& req);
    MCPResponse handleResourcesList(const MCPRequest& req);
    MCPResponse handleResourcesRead(const MCPRequest& req);
    MCPResponse handlePromptsList(const MCPRequest& req);
    MCPResponse handlePromptsGet(const MCPRequest& req);
    MCPResponse handlePing(const MCPRequest& req);

    // JSON helpers
    std::string serializeResponse(const MCPResponse& resp) const;
    MCPRequest  parseRequest(const std::string& json) const;
    std::string makeErrorResponse(int64_t id, MCPErrorCode code, const std::string& msg) const;

    ServerInfo m_serverInfo;
    ServerCapabilities m_capabilities;
    bool m_running = false;

    std::map<std::string, std::pair<ToolDefinition, ToolHandler>> m_tools;
    std::map<std::string, std::pair<ResourceDefinition, ResourceHandler>> m_resources;
    std::map<std::string, std::pair<PromptTemplate, PromptHandler>> m_prompts;

    mutable std::mutex m_mutex;
    uint64_t m_totalRequests = 0;
    uint64_t m_totalErrors   = 0;
    int64_t  m_nextId        = 1;
};

// ============================================================================
// MCP Client — Connects to external MCP servers
// ============================================================================

class MCPClient {
public:
    MCPClient();
    ~MCPClient();

    // Connect to an MCP server via stdio subprocess
    bool connectStdio(const std::string& command, const std::vector<std::string>& args = {});
    // Connect via HTTP/SSE (future)
    bool connectHttp(const std::string& url);
    void disconnect();
    bool isConnected() const { return m_connected; }

    // Capabilities
    ServerCapabilities getServerCapabilities() const { return m_serverCapabilities; }

    // Tool interaction
    std::vector<ToolDefinition> listTools();
    ToolResult callTool(const std::string& name, const std::string& argsJson);

    // Resource interaction
    std::vector<ResourceDefinition> listResources();
    ResourceContent readResource(const std::string& uri);

    // Prompt interaction
    std::vector<PromptTemplate> listPrompts();
    std::vector<PromptMessage> getPrompt(const std::string& name, const std::string& argsJson);

private:
    std::string sendRequest(const std::string& method, const std::string& params);
    MCPResponse parseResponse(const std::string& rawJson);

    bool m_connected = false;
    ServerCapabilities m_serverCapabilities;

    // Subprocess handle (stdio transport)
    void* m_processHandle = nullptr;  // HANDLE on Windows
    void* m_stdinWrite    = nullptr;
    void* m_stdoutRead    = nullptr;

    mutable std::mutex m_mutex;
    int64_t m_nextId = 1;
};

// ============================================================================
// Built-in Tool Registrations for RawrXD IDE
// ============================================================================

void registerBuiltinTools(MCPServer& server);

} // namespace MCP
} // namespace RawrXD

#endif // MCP_INTEGRATION_H_
