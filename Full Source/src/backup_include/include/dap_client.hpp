// ============================================================================
// dap_client.hpp — DAP (Debug Adapter Protocol) client scaffold
// ============================================================================
// Minimal scaffold for integrating a DAP debug adapter (e.g. node, python).
// The existing debugger UI (Win32IDE_Debugger.cpp) already has:
//   - Breakpoints list (IDC_DEBUGGER_BREAKPOINT_LIST)
//   - Watch list (IDC_DEBUGGER_WATCH_LIST)
//   - Stack trace (IDC_DEBUGGER_STACK_LIST)
//   - Variables tree (IDC_DEBUGGER_VARIABLE_TREE)
// This client can connect to a DAP server (stdio), send initialize/launch,
// and forward events to populate those panels. Full implementation TBD.
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace RawrXD {
namespace DAP {

struct DAPClientConfig {
    std::string adapterPath;       // e.g. "node", "python"
    std::vector<std::string> args; // e.g. ["path/to/adapter.js"]
    std::string cwd;
};

class DAPClient {
public:
    DAPClient() = default;
    ~DAPClient();

    bool connect(const DAPClientConfig& config);
    void disconnect();

    bool sendInitialize();
    bool sendLaunch(const std::string& program, const std::vector<std::string>& programArgs);
    bool sendConfigurationDone();

    bool isConnected() const { return m_connected; }

    using MessageCallback = std::function<void(const std::string& method, const std::string& params)>;
    void setEventCallback(MessageCallback cb) { m_eventCb = std::move(cb); }

private:
    bool sendRequest(const std::string& method, const std::string& params);
    bool readResponse(std::string& out);
    bool m_connected = false;
    void* m_processStdin = nullptr;
    void* m_processStdout = nullptr;
    void* m_processHandle = nullptr;
    MessageCallback m_eventCb;
};

} // namespace DAP
} // namespace RawrXD
