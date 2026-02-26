# MCP Transport Hook Integration - Quick Reference

## Overview
MCP (Model Context Protocol) transport hooks provide raw buffer visibility into agent communication channels **before any JS parsing occurs**. This enables:
- Full message interception at the transport layer
- Real-time logging of JSON-RPC messages
- Hook-based modification of request/response streams
- Performance metrics collection at wire level

## Files Added

| File | Purpose |
|------|---------|
| `Win32IDE_MCPHooks.h` | Hook manager class, RVA offsets, callback types |
| `Win32IDE_MCPHooks.cpp` | Hot-patcher, message ring buffer, statistics |
| `mcp_hooks.asm` | MASM64 trampolines for register-safe hooking |

## RVA Offsets (from Cursor JS Analysis)

```cpp
namespace MCPHookRVA {
    READ_MESSAGE      = 0x4d5193   // Score 10: jsonrpc framing
    WRITE_MESSAGE     = 0x4d526b   // Score 10: content-length
    MAKE_REQUEST      = 0x72c86    // Score 10: params, jsonrpc
    ESTABLISH_WS      = 0x37bd9a   // Score 10: WebSocket connection
    ON_SOCKET_DATA    = 0x1ab2c8   // Score 10: raw socket handler
    START_LISTENING   = 0x1ab17f   // Score 10: transport start
    TRANSPORT_START   = 0x33ac7e   // Score 10: upgrade handler
    ON_MESSAGE        = 0x30c51    // Score 10: message callback
    HANDLE_MESSAGE    = 0x4dff11   // Score 5: method dispatch
    SEND_NOTIFICATION = 0x4d53c9   // Score 3: one-way messages
}
```

## Usage in AgenticBridge

```cpp
// Automatic installation in Initialize()
bool AgenticBridge::Initialize(...) {
    // ... existing init ...
    
    if (InstallMCPTransportHooks()) {
        LOG_INFO("MCP hooks installed - full message visibility");
    }
}

// Custom callbacks
bridge->SetMCPReadCallback([](const uint8_t* buf, size_t len, const char* src) {
    // Handle incoming MCP message
});

bridge->SetMCPWriteCallback([](const uint8_t* buf, size_t len, const char* dst) {
    // Handle outgoing MCP message
});

// Retrieve recent messages
auto messages = bridge->GetRecentMCPMessages(50);
for (const auto& msg : messages) {
    printf("Method: %s, ID: %lld\n", msg.method.c_str(), msg.id);
}
```

## MASM64 Trampoline Architecture

```asm
; Register-safe hooking with full context preservation
MCP_ReadMessage_Trampoline PROC
    ; Check enable flag (atomic toggle at runtime)
    mov eax, [g_EnableReadHook]
    test eax, eax
    jz @bypass
    
    ; Save ALL volatile registers (Win64 ABI)
    push rbx, rsi, rdi, r8-r15
    sub rsp, 40h    ; Shadow space
    
    ; Call C++ handler
    call MCP_ReadMessageHook
    
    ; Restore and jump to original
    ...
    jmp [g_OrigReadMessage]
```

## Hook Enable/Disable Flags

Runtime-toggleable via MASM globals:
```cpp
extern "C" {
    DWORD g_EnableReadHook;    // 1=enabled, 0=bypass
    DWORD g_EnableWriteHook;
    DWORD g_EnableSocketHook;
    DWORD g_EnableWSHook;
}
```

## MCPMessage Structure

```cpp
struct MCPMessage {
    std::string jsonrpc;       // "2.0"
    std::string method;        // "tools/call", "initialize"
    int64_t id;                // Request ID
    std::string params;        // Raw JSON params
    std::string result;        // Response result
    std::string error;         // Error if present
    uint64_t timestamp;        // QueryPerformanceCounter
    uintptr_t hookRVA;         // Which hook captured this
};
```

## Statistics API

```cpp
auto& mgr = MCPHookManager::GetInstance();
printf("Messages: %llu\n", mgr.GetTotalMessagesIntercepted());
printf("Read: %llu bytes\n", mgr.GetBytesRead());
printf("Written: %llu bytes\n", mgr.GetBytesWritten());
```

## Build Integration

CMakeLists.txt entry:
```cmake
src/win32app/Win32IDE_MCPHooks.h
src/win32app/Win32IDE_MCPHooks.cpp
src/win32app/mcp_hooks.asm
```

MASM is already enabled globally:
```cmake
enable_language(ASM_MASM)
set(CMAKE_ASM_MASM_FLAGS "/nologo /W3 /Cx /Zi")
```

## Safety Notes

1. **Hook addresses are RVAs** - must add module base at runtime
2. **Thread-safe** - all callbacks use CRITICAL_SECTION
3. **Ring buffer** - 1024 message capacity with wrap-around
4. **Bypass mode** - hooks can be disabled without uninstalling
5. **Cleanup** - `UninstallMCPTransportHooks()` restores original bytes

---
Generated: 2026-01-27 | RawrXD IDE v3.x | MCP Hook Integration
