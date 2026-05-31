#pragma once
// SovereignMCPBridge.h — Phase 48: MCP JSON-RPC stdio transport
// Spawns an external MCP server (Node.js/Python/any) as a child process
// and exchanges JSON-RPC 2.0 messages over its stdin/stdout pipes.

#include <windows.h>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <nlohmann/json.hpp>

namespace RawrXD::Runtime {

using json = nlohmann::json;

class SovereignMCPBridge {
public:
    static SovereignMCPBridge& instance();

    // Spawn an external MCP-compatible server process.
    // command: full command line, e.g. "node mcp-github-server.js"
    // Returns true if process started and handshake succeeded.
    bool spawnServer(const std::string& command, DWORD timeoutMs = 5000);

    // Send a JSON-RPC tools/call request and synchronously wait for response.
    // Blocks up to timeoutMs (default 30 s). Returns empty object on error.
    json callTool(const std::string& toolName, const json& args,
                  DWORD timeoutMs = 30000);

    // Query MCP tools/list to discover available external tools.
    json listTools(DWORD timeoutMs = 10000);

    // Gracefully terminate the child process and release all handles.
    void shutdown();

    bool isRunning() const { return m_running.load(); }
    std::string lastError() const;

private:
    SovereignMCPBridge() = default;
    ~SovereignMCPBridge() { shutdown(); }
    SovereignMCPBridge(const SovereignMCPBridge&) = delete;
    SovereignMCPBridge& operator=(const SovereignMCPBridge&) = delete;

    bool writeRequest(const json& req);
    bool readMessage(json& out, DWORD timeoutMs);
    bool readResponseForId(int64_t id, json& out, DWORD timeoutMs);
    bool readLine(std::string& out, DWORD timeoutMs);
    bool readExact(char* dst, size_t size, DWORD timeoutMs);
    std::string readAvailableOutput(DWORD maxBytes);
    void setLastError(const std::string& msg);

    PROCESS_INFORMATION m_procInfo{};
    HANDLE m_stdinWrite  = INVALID_HANDLE_VALUE;
    HANDLE m_stdoutRead  = INVALID_HANDLE_VALUE;
    std::atomic<bool>   m_running{false};
    std::atomic<int64_t> m_nextId{1};
    std::mutex m_callMutex;     // serialize concurrent callTool() calls
    mutable std::mutex m_errorMutex;
    std::string m_lastError;
};

} // namespace RawrXD::Runtime
