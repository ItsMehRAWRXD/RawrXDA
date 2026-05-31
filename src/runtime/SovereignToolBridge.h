#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <windows.h>

extern "C" {
    // Forward declarations for MASM ToolEngine functions
    bool ToolEngine_Initialize(void* context, void* memoryContext, void* outToolState);
    uint32_t ToolEngine_Execute(void* toolState, const char* name, const char* args, uint32_t priority);
    bool ToolEngine_Cancel(void* toolState, uint32_t taskId);
    uint32_t ToolEngine_GetStatus(void* toolState, uint32_t taskId, char* outResult, uint32_t resultSize);
    bool ToolEngine_Shutdown(void* toolState);
}

namespace RawrXD::Runtime {

struct ToolStatus {
    uint32_t taskId;
    uint32_t state; // 0=Idle, 1=In-Progress, 2=Completed, 3=Failed
    std::string result;
    uint32_t exitCode;
};

// MASM-mapped ToolState structure matching RawrXD_ToolEngine.asm
struct MASM_ToolState {
    void* memoryContext;
    HANDLE hThread;
    HANDLE hStopEvent;
    HANDLE hWakeEvent;
    HANDLE hTaskMutex;
    HANDLE hCompletionPort;
    uint32_t activeToolCount;
    uint32_t maxConcurrent; // 32 slots per MASM _TOOL_SLOTS
    // ... remaining internal slots (32x size as per assembly)
    uint8_t _padding[4096]; // Buffer for internal state/slots
};

class SovereignToolBridge {
public:
    static SovereignToolBridge& instance();

    bool initialize();
    uint32_t dispatchTool(const std::string& name, const std::string& args, uint32_t priority = 5);
    bool cancelTool(uint32_t taskId);
    ToolStatus getToolStatus(uint32_t taskId);
    bool shutdown();

    bool isInitialized() const { return m_initialized; }

private:
    SovereignToolBridge() : m_initialized(false) {}
    ~SovereignToolBridge() { shutdown(); }

    bool m_initialized;
    MASM_ToolState m_state;
};

} // namespace RawrXD::Runtime
