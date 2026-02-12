// MCP Transport Hook Implementation
// Hot-patches Cursor-style MCP transport layer for raw buffer visibility

#include "Win32IDE_MCPHooks.h"
#include "IDELogger.h"

#include <chrono>
#include <cstring>
#include <sstream>
#include <iomanip>

// Singleton instance
MCPHookManager& MCPHookManager::GetInstance() {
    static MCPHookManager instance;
    return instance;
}

MCPHookManager::MCPHookManager()
    : m_initialized(false)
    , m_targetModule(nullptr)
    , m_moduleBase(0)
    , m_messageHead(0)
    , m_totalIntercepted(0)
    , m_bytesRead(0)
    , m_bytesWritten(0)
{
    InitializeCriticalSection(&m_cs);
    m_messageBuffer.reserve(MESSAGE_BUFFER_SIZE);
}

MCPHookManager::~MCPHookManager() {
    Shutdown();
    DeleteCriticalSection(&m_cs);
}

bool MCPHookManager::Initialize(HMODULE targetModule) {
    if (m_initialized) {
        LOG_WARNING("MCPHookManager already initialized");
        return true;
    }
    
    // If no target module specified, use current process
    if (targetModule == nullptr) {
        m_targetModule = GetModuleHandleA(nullptr);
    } else {
        m_targetModule = targetModule;
    }
    
    if (!m_targetModule) {

        return false;
    }
    
    m_moduleBase = reinterpret_cast<uintptr_t>(m_targetModule);


    m_initialized = true;
    return true;
}

void MCPHookManager::Shutdown() {
    if (!m_initialized) return;
    
    UninstallAllHooks();
    
    EnterCriticalSection(&m_cs);
    m_messageBuffer.clear();
    m_hooks.clear();
    LeaveCriticalSection(&m_cs);
    
    m_initialized = false;

}

bool MCPHookManager::WriteJumpHook(uintptr_t targetAddr, uintptr_t hookAddr, 
                                    uint8_t* savedBytes, size_t* savedLen) {
    // x64 absolute jump: FF 25 00 00 00 00 [8-byte address]
    // Total: 14 bytes
    constexpr size_t JUMP_SIZE = 14;
    
    // Save original bytes
    DWORD oldProtect;
    if (!VirtualProtect(reinterpret_cast<void*>(targetAddr), JUMP_SIZE, 
                        PAGE_EXECUTE_READWRITE, &oldProtect)) {

        return false;
    }
    
    memcpy(savedBytes, reinterpret_cast<void*>(targetAddr), JUMP_SIZE);
    *savedLen = JUMP_SIZE;
    
    // Write jump instruction
    uint8_t jumpCode[14] = {
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,  // jmp qword ptr [rip+0]
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // 8-byte address
    };
    memcpy(&jumpCode[6], &hookAddr, sizeof(hookAddr));
    memcpy(reinterpret_cast<void*>(targetAddr), jumpCode, JUMP_SIZE);
    
    // Restore protection
    VirtualProtect(reinterpret_cast<void*>(targetAddr), JUMP_SIZE, oldProtect, &oldProtect);
    
    // Flush instruction cache
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(targetAddr), JUMP_SIZE);
    
    return true;
}

bool MCPHookManager::RestoreOriginalBytes(uintptr_t targetAddr, const uint8_t* savedBytes, size_t savedLen) {
    DWORD oldProtect;
    if (!VirtualProtect(reinterpret_cast<void*>(targetAddr), savedLen, 
                        PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }
    
    memcpy(reinterpret_cast<void*>(targetAddr), savedBytes, savedLen);
    
    VirtualProtect(reinterpret_cast<void*>(targetAddr), savedLen, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(targetAddr), savedLen);
    
    return true;
}

uintptr_t MCPHookManager::AllocateTrampoline(size_t size) {
    // Allocate near the module base for shorter jumps if possible
    void* mem = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    return reinterpret_cast<uintptr_t>(mem);
}

HookInstallResult MCPHookManager::InstallReadMessageHook() {
    HookInstallResult result = {};
    result.hookName = "readMessage";
    result.originalAddress = m_moduleBase + MCPHookRVA::READ_MESSAGE;
    
    if (!m_initialized) {
        result.errorCode = ERROR_NOT_READY;
        return result;
    }
    
    HookState state = {};
    state.rva = MCPHookRVA::READ_MESSAGE;
    state.absoluteAddr = result.originalAddress;
    
    // For now, we'll use a C++ callback instead of MASM trampoline
    // Real implementation would write hook to point to MCP_ReadMessageHook
    uintptr_t hookTarget = reinterpret_cast<uintptr_t>(&MCP_ReadMessageHook);
    
    if (WriteJumpHook(state.absoluteAddr, hookTarget, state.originalBytes, &state.patchedLen)) {
        state.installed = true;
        result.success = true;
        
        EnterCriticalSection(&m_cs);
        m_hooks.push_back(state);
        LeaveCriticalSection(&m_cs);


    } else {
        result.errorCode = GetLastError();

    }
    
    return result;
}

HookInstallResult MCPHookManager::InstallWriteMessageHook() {
    HookInstallResult result = {};
    result.hookName = "writeMessage";
    result.originalAddress = m_moduleBase + MCPHookRVA::WRITE_MESSAGE;
    
    if (!m_initialized) {
        result.errorCode = ERROR_NOT_READY;
        return result;
    }
    
    HookState state = {};
    state.rva = MCPHookRVA::WRITE_MESSAGE;
    state.absoluteAddr = result.originalAddress;
    
    uintptr_t hookTarget = reinterpret_cast<uintptr_t>(&MCP_WriteMessageHook);
    
    if (WriteJumpHook(state.absoluteAddr, hookTarget, state.originalBytes, &state.patchedLen)) {
        state.installed = true;
        result.success = true;
        
        EnterCriticalSection(&m_cs);
        m_hooks.push_back(state);
        LeaveCriticalSection(&m_cs);


    } else {
        result.errorCode = GetLastError();

    }
    
    return result;
}

HookInstallResult MCPHookManager::InstallOnSocketDataHook() {
    HookInstallResult result = {};
    result.hookName = "onSocketData";
    result.originalAddress = m_moduleBase + MCPHookRVA::ON_SOCKET_DATA;
    
    if (!m_initialized) {
        result.errorCode = ERROR_NOT_READY;
        return result;
    }
    
    HookState state = {};
    state.rva = MCPHookRVA::ON_SOCKET_DATA;
    state.absoluteAddr = result.originalAddress;
    
    uintptr_t hookTarget = reinterpret_cast<uintptr_t>(&MCP_OnSocketDataHook);
    
    if (WriteJumpHook(state.absoluteAddr, hookTarget, state.originalBytes, &state.patchedLen)) {
        state.installed = true;
        result.success = true;
        
        EnterCriticalSection(&m_cs);
        m_hooks.push_back(state);
        LeaveCriticalSection(&m_cs);


    } else {
        result.errorCode = GetLastError();

    }
    
    return result;
}

HookInstallResult MCPHookManager::InstallWebSocketHook() {
    HookInstallResult result = {};
    result.hookName = "establishWebSocketConnection";
    result.originalAddress = m_moduleBase + MCPHookRVA::ESTABLISH_WS;
    
    if (!m_initialized) {
        result.errorCode = ERROR_NOT_READY;
        return result;
    }
    
    HookState state = {};
    state.rva = MCPHookRVA::ESTABLISH_WS;
    state.absoluteAddr = result.originalAddress;
    
    uintptr_t hookTarget = reinterpret_cast<uintptr_t>(&MCP_WebSocketFrameHook);
    
    if (WriteJumpHook(state.absoluteAddr, hookTarget, state.originalBytes, &state.patchedLen)) {
        state.installed = true;
        result.success = true;
        
        EnterCriticalSection(&m_cs);
        m_hooks.push_back(state);
        LeaveCriticalSection(&m_cs);


    } else {
        result.errorCode = GetLastError();

    }
    
    return result;
}

HookInstallResult MCPHookManager::InstallAllTransportHooks() {
    HookInstallResult result = {};
    result.hookName = "AllTransportHooks";
    
    int successCount = 0;
    
    auto r1 = InstallReadMessageHook();
    if (r1.success) successCount++;
    
    auto r2 = InstallWriteMessageHook();
    if (r2.success) successCount++;
    
    auto r3 = InstallOnSocketDataHook();
    if (r3.success) successCount++;
    
    auto r4 = InstallWebSocketHook();
    if (r4.success) successCount++;
    
    result.success = (successCount > 0);

    return result;
}

void MCPHookManager::UninstallAllHooks() {
    EnterCriticalSection(&m_cs);
    
    for (auto& hook : m_hooks) {
        if (hook.installed) {
            RestoreOriginalBytes(hook.absoluteAddr, hook.originalBytes, hook.patchedLen);
            hook.installed = false;

        }
    }
    
    m_hooks.clear();
    LeaveCriticalSection(&m_cs);
}

void MCPHookManager::UninstallHook(uintptr_t rva) {
    EnterCriticalSection(&m_cs);
    
    for (auto& hook : m_hooks) {
        if (hook.rva == rva && hook.installed) {
            RestoreOriginalBytes(hook.absoluteAddr, hook.originalBytes, hook.patchedLen);
            hook.installed = false;

            break;
        }
    }
    
    LeaveCriticalSection(&m_cs);
}

MCPMessage MCPHookManager::ParseMCPBuffer(const uint8_t* buffer, size_t length, uintptr_t hookRVA) {
    MCPMessage msg = {};
    msg.hookRVA = hookRVA;
    
    // Get timestamp
    LARGE_INTEGER pc;
    QueryPerformanceCounter(&pc);
    msg.timestamp = pc.QuadPart;
    
    // Try to parse as JSON-RPC
    std::string data(reinterpret_cast<const char*>(buffer), length);
    
    // Quick scan for jsonrpc field
    size_t jsonrpcPos = data.find("\"jsonrpc\"");
    if (jsonrpcPos != std::string::npos) {
        msg.jsonrpc = "2.0";
    }
    
    // Extract method if present
    size_t methodPos = data.find("\"method\"");
    if (methodPos != std::string::npos) {
        size_t colonPos = data.find(':', methodPos);
        size_t quoteStart = data.find('"', colonPos + 1);
        size_t quoteEnd = data.find('"', quoteStart + 1);
        if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
            msg.method = data.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
        }
    }
    
    // Extract id if present
    size_t idPos = data.find("\"id\"");
    if (idPos != std::string::npos) {
        size_t colonPos = data.find(':', idPos);
        if (colonPos != std::string::npos) {
            // Skip whitespace
            size_t numStart = colonPos + 1;
            while (numStart < data.size() && (data[numStart] == ' ' || data[numStart] == '\t'))
                numStart++;
            // Parse number
            msg.id = std::strtoll(&data[numStart], nullptr, 10);
        }
    }
    
    // Store raw params/result
    size_t paramsPos = data.find("\"params\"");
    if (paramsPos != std::string::npos) {
        msg.params = "[params present]";
    }
    
    size_t resultPos = data.find("\"result\"");
    if (resultPos != std::string::npos) {
        msg.result = "[result present]";
    }
    
    size_t errorPos = data.find("\"error\"");
    if (errorPos != std::string::npos) {
        msg.error = "[error present]";
    }
    
    return msg;
}

void MCPHookManager::OnReadMessage(const uint8_t* buffer, size_t length) {
    EnterCriticalSection(&m_cs);
    
    m_totalIntercepted++;
    m_bytesRead += length;
    
    MCPMessage msg = ParseMCPBuffer(buffer, length, MCPHookRVA::READ_MESSAGE);
    
    // Add to ring buffer
    if (m_messageBuffer.size() < MESSAGE_BUFFER_SIZE) {
        m_messageBuffer.push_back(msg);
    } else {
        m_messageBuffer[m_messageHead] = msg;
        m_messageHead = (m_messageHead + 1) % MESSAGE_BUFFER_SIZE;
    }
    
    LeaveCriticalSection(&m_cs);
    
    // Call user callback if set
    if (m_readCallback) {
        m_readCallback(buffer, length, "readMessage");
    }

}

void MCPHookManager::OnWriteMessage(const uint8_t* buffer, size_t length) {
    EnterCriticalSection(&m_cs);
    
    m_totalIntercepted++;
    m_bytesWritten += length;
    
    MCPMessage msg = ParseMCPBuffer(buffer, length, MCPHookRVA::WRITE_MESSAGE);
    
    if (m_messageBuffer.size() < MESSAGE_BUFFER_SIZE) {
        m_messageBuffer.push_back(msg);
    } else {
        m_messageBuffer[m_messageHead] = msg;
        m_messageHead = (m_messageHead + 1) % MESSAGE_BUFFER_SIZE;
    }
    
    LeaveCriticalSection(&m_cs);
    
    if (m_writeCallback) {
        m_writeCallback(buffer, length, "writeMessage");
    }

}

void MCPHookManager::OnSocketData(const uint8_t* data, size_t length, SOCKET sock) {
    EnterCriticalSection(&m_cs);
    
    m_totalIntercepted++;
    m_bytesRead += length;
    
    MCPMessage msg = ParseMCPBuffer(data, length, MCPHookRVA::ON_SOCKET_DATA);
    
    if (m_messageBuffer.size() < MESSAGE_BUFFER_SIZE) {
        m_messageBuffer.push_back(msg);
    } else {
        m_messageBuffer[m_messageHead] = msg;
        m_messageHead = (m_messageHead + 1) % MESSAGE_BUFFER_SIZE;
    }
    
    LeaveCriticalSection(&m_cs);
    
    if (m_socketCallback) {
        m_socketCallback(data, length, sock);
    }

}

void MCPHookManager::OnWebSocketFrame(const uint8_t* frame, size_t length, uint8_t opcode) {
    EnterCriticalSection(&m_cs);
    
    m_totalIntercepted++;
    m_bytesRead += length;
    
    // WebSocket frames have header bytes, skip for MCP parsing
    const uint8_t* payload = frame;
    size_t payloadLen = length;
    
    // Simple WebSocket frame parsing (assumes unmasked server->client)
    if (length > 2) {
        uint8_t payloadLenByte = frame[1] & 0x7F;
        if (payloadLenByte <= 125) {
            payload = frame + 2;
            payloadLen = payloadLenByte;
        } else if (payloadLenByte == 126 && length > 4) {
            payload = frame + 4;
            payloadLen = (frame[2] << 8) | frame[3];
        }
    }
    
    MCPMessage msg = ParseMCPBuffer(payload, payloadLen, MCPHookRVA::ESTABLISH_WS);
    
    if (m_messageBuffer.size() < MESSAGE_BUFFER_SIZE) {
        m_messageBuffer.push_back(msg);
    } else {
        m_messageBuffer[m_messageHead] = msg;
        m_messageHead = (m_messageHead + 1) % MESSAGE_BUFFER_SIZE;
    }
    
    LeaveCriticalSection(&m_cs);
    
    if (m_wsCallback) {
        m_wsCallback(frame, length, opcode);
    }


}

std::vector<MCPMessage> MCPHookManager::GetRecentMessages(size_t count) const {
    std::vector<MCPMessage> result;
    
    EnterCriticalSection(const_cast<CRITICAL_SECTION*>(&m_cs));
    
    size_t available = m_messageBuffer.size();
    size_t toReturn = (count < available) ? count : available;
    
    // Return most recent messages
    for (size_t i = 0; i < toReturn; i++) {
        size_t idx = (m_messageHead + available - toReturn + i) % MESSAGE_BUFFER_SIZE;
        if (idx < m_messageBuffer.size()) {
            result.push_back(m_messageBuffer[idx]);
        }
    }
    
    LeaveCriticalSection(const_cast<CRITICAL_SECTION*>(&m_cs));
    
    return result;
}

void MCPHookManager::ClearMessageBuffer() {
    EnterCriticalSection(&m_cs);
    m_messageBuffer.clear();
    m_messageHead = 0;
    LeaveCriticalSection(&m_cs);
}

// Convenience functions for AgenticBridge integration
bool InstallMCPHooks() {
    auto& mgr = MCPHookManager::GetInstance();
    
    if (!mgr.Initialize()) {

        return false;
    }
    
    // Install critical transport hooks
    auto result = mgr.InstallAllTransportHooks();
    
    if (result.success) {

        // Set default logging callbacks
        mgr.SetReadMessageCallback([](const uint8_t* buf, size_t len, const char* src) {
            std::stringstream ss;
            ss << "[MCP-READ] " << len << " bytes from " << src;
            if (len > 0 && len < 256) {
                ss << ": " << std::string(reinterpret_cast<const char*>(buf), 
                                          std::min(len, size_t(64)));
            }

        });
        
        mgr.SetWriteMessageCallback([](const uint8_t* buf, size_t len, const char* dst) {
            std::stringstream ss;
            ss << "[MCP-WRITE] " << len << " bytes to " << dst;
            if (len > 0 && len < 256) {
                ss << ": " << std::string(reinterpret_cast<const char*>(buf), 
                                          std::min(len, size_t(64)));
            }

        });
    }
    
    return result.success;
}

void UninstallMCPHooks() {
    MCPHookManager::GetInstance().UninstallAllHooks();

}

// C-linkage hook entry points (called from MASM trampolines)
extern "C" {

void __fastcall MCP_ReadMessageHook(const uint8_t* buffer, size_t length) {
    MCPHookManager::GetInstance().OnReadMessage(buffer, length);
}

void __fastcall MCP_WriteMessageHook(const uint8_t* buffer, size_t length) {
    MCPHookManager::GetInstance().OnWriteMessage(buffer, length);
}

void __fastcall MCP_OnSocketDataHook(const uint8_t* data, size_t length, SOCKET sock) {
    MCPHookManager::GetInstance().OnSocketData(data, length, sock);
}

void __fastcall MCP_WebSocketFrameHook(const uint8_t* frame, size_t length, uint8_t opcode) {
    MCPHookManager::GetInstance().OnWebSocketFrame(frame, length, opcode);
}

} // extern "C"

