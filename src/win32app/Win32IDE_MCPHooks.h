#pragma once
// MCP Transport Hook Infrastructure
// Intercepts message framing for raw buffer visibility before JS parsing

#include <windows.h>
#include <cstdint>
#include <string>
#include <functional>
#include <vector>

// RVA offsets extracted from Cursor JS analysis (mcp_entrypoint_snippets.txt)
// These are virtual offsets into the beautified JS bundle mapping
namespace MCPHookRVA {
    constexpr uintptr_t READ_MESSAGE      = 0x4d5193;  // Score 10: readMessage, jsonrpc framing
    constexpr uintptr_t WRITE_MESSAGE     = 0x4d526b;  // Score 10: writeMessage, content-length
    constexpr uintptr_t MAKE_REQUEST      = 0x72c86;   // Score 10: makeRequest, params, jsonrpc
    constexpr uintptr_t ESTABLISH_WS      = 0x37bd9a;  // Score 10: establishWebSocketConnection
    constexpr uintptr_t ON_SOCKET_DATA    = 0x1ab2c8;  // Score 10: onSocketData, readMessage
    constexpr uintptr_t START_LISTENING   = 0x1ab17f;  // Score 10: startListening
    constexpr uintptr_t TRANSPORT_START   = 0x33ac7e;  // Score 10: transport.start, upgrade
    constexpr uintptr_t ON_MESSAGE        = 0x30c51;   // Score 10: onMessage, jsonrpc
    constexpr uintptr_t HANDLE_MESSAGE    = 0x4dff11;  // Score 5: handleMessage, methodHandler
    constexpr uintptr_t SEND_NOTIFICATION = 0x4d53c9;  // Score 3: sendNotification, no response
}

// Hook callback types for each transport layer
using ReadMessageCallback = std::function<void(const uint8_t* buffer, size_t length, const char* source)>;
using WriteMessageCallback = std::function<void(const uint8_t* buffer, size_t length, const char* destination)>;
using SocketDataCallback = std::function<void(const uint8_t* rawData, size_t length, SOCKET sock)>;
using WebSocketFrameCallback = std::function<void(const uint8_t* frame, size_t frameLen, uint8_t opcode)>;

// MCP Message structure for parsed intercepts
struct MCPMessage {
    std::string jsonrpc;       // "2.0"
    std::string method;        // e.g., "tools/call", "initialize"
    int64_t id;                // Request ID
    std::string params;        // Raw JSON params
    std::string result;        // Response result
    std::string error;         // Error if present
    uint64_t timestamp;        // Intercept timestamp (QueryPerformanceCounter)
    uintptr_t hookRVA;         // Which hook captured this
};

// Hook installation result
struct HookInstallResult {
    bool success;
    uintptr_t originalAddress;
    uintptr_t trampolineAddress;
    const char* hookName;
    DWORD errorCode;
};

// MCP Hook Manager class
class MCPHookManager {
public:
    static MCPHookManager& GetInstance();
    
    // Initialize hook infrastructure (call before any hooks)
    bool Initialize(HMODULE targetModule = nullptr);
    void Shutdown();
    bool IsInitialized() const { return m_initialized; }
    
    // Install individual hooks by RVA
    HookInstallResult InstallReadMessageHook();
    HookInstallResult InstallWriteMessageHook();
    HookInstallResult InstallOnSocketDataHook();
    HookInstallResult InstallWebSocketHook();
    HookInstallResult InstallAllTransportHooks();
    
    // Uninstall hooks (restore original bytes)
    void UninstallAllHooks();
    void UninstallHook(uintptr_t rva);
    
    // Register callbacks for hook events
    void SetReadMessageCallback(ReadMessageCallback cb) { m_readCallback = cb; }
    void SetWriteMessageCallback(WriteMessageCallback cb) { m_writeCallback = cb; }
    void SetSocketDataCallback(SocketDataCallback cb) { m_socketCallback = cb; }
    void SetWebSocketFrameCallback(WebSocketFrameCallback cb) { m_wsCallback = cb; }
    
    // Get intercepted messages (ring buffer of last N)
    std::vector<MCPMessage> GetRecentMessages(size_t count = 100) const;
    void ClearMessageBuffer();
    
    // Statistics
    uint64_t GetTotalMessagesIntercepted() const { return m_totalIntercepted; }
    uint64_t GetBytesRead() const { return m_bytesRead; }
    uint64_t GetBytesWritten() const { return m_bytesWritten; }
    
    // Called by assembly hooks (do not call directly)
    void OnReadMessage(const uint8_t* buffer, size_t length);
    void OnWriteMessage(const uint8_t* buffer, size_t length);
    void OnSocketData(const uint8_t* data, size_t length, SOCKET sock);
    void OnWebSocketFrame(const uint8_t* frame, size_t length, uint8_t opcode);

private:
    MCPHookManager();
    ~MCPHookManager();
    MCPHookManager(const MCPHookManager&) = delete;
    MCPHookManager& operator=(const MCPHookManager&) = delete;
    
    // Hot-patcher implementation
    bool WriteJumpHook(uintptr_t targetAddr, uintptr_t hookAddr, uint8_t* savedBytes, size_t* savedLen);
    bool RestoreOriginalBytes(uintptr_t targetAddr, const uint8_t* savedBytes, size_t savedLen);
    uintptr_t AllocateTrampoline(size_t size);
    
    // Parse MCP message from raw buffer
    MCPMessage ParseMCPBuffer(const uint8_t* buffer, size_t length, uintptr_t hookRVA);
    
    bool m_initialized;
    HMODULE m_targetModule;
    uintptr_t m_moduleBase;
    
    // Saved original bytes for each hook (for restoration)
    struct HookState {
        uintptr_t rva;
        uintptr_t absoluteAddr;
        uint8_t originalBytes[16];  // Enough for worst-case patching
        size_t patchedLen;
        uintptr_t trampolineAddr;
        bool installed;
    };
    std::vector<HookState> m_hooks;
    
    // Callbacks
    ReadMessageCallback m_readCallback;
    WriteMessageCallback m_writeCallback;
    SocketDataCallback m_socketCallback;
    WebSocketFrameCallback m_wsCallback;
    
    // Message ring buffer
    static constexpr size_t MESSAGE_BUFFER_SIZE = 1024;
    std::vector<MCPMessage> m_messageBuffer;
    size_t m_messageHead;
    
    // Statistics
    uint64_t m_totalIntercepted;
    uint64_t m_bytesRead;
    uint64_t m_bytesWritten;
    
    // Critical section for thread safety
    CRITICAL_SECTION m_cs;
};

// Convenience function for AgenticBridge integration
bool InstallMCPHooks();
void UninstallMCPHooks();

// Assembly hook entry points (implemented in mcp_hooks.asm)
extern "C" {
    void __fastcall MCP_ReadMessageHook(const uint8_t* buffer, size_t length);
    void __fastcall MCP_WriteMessageHook(const uint8_t* buffer, size_t length);
    void __fastcall MCP_OnSocketDataHook(const uint8_t* data, size_t length, SOCKET sock);
    void __fastcall MCP_WebSocketFrameHook(const uint8_t* frame, size_t length, uint8_t opcode);
}
