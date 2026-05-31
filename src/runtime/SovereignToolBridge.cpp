#include "SovereignToolBridge.h"
#include "SovereignMemoryBridge.h"
#include <iostream>
#include <mutex>
#include <vector>

namespace RawrXD::Runtime {

static std::mutex g_toolMutex;

SovereignToolBridge& SovereignToolBridge::instance() {
    static SovereignToolBridge instance;
    return instance;
}

bool SovereignToolBridge::initialize() {
    std::lock_guard<std::mutex> lock(g_toolMutex);
    if (m_initialized) return true;

    // Tool engine needs memory connectivity for decision logging
    void* memoryContext = SovereignMemoryBridge::instance().getContext();
    if (!memoryContext) return false;

    // MASM Initialize: sets up 32 concurrent slots and IOCP worker thread
    if (ToolEngine_Initialize(nullptr, memoryContext, &m_state)) {
        m_initialized = true;
        m_state.maxConcurrent = 32;
        std::cout << "[ToolEngine] Parallel Multi-Agent Tool Infrastructure Active." << std::endl;
        return true;
    }

    std::cerr << "[ToolEngine] ERROR: Critical failure during ToolEngine initialization." << std::endl;
    return false;
}

uint32_t SovereignToolBridge::dispatchTool(const std::string& name, const std::string& args, uint32_t priority) {
    if (!m_initialized) return 0;

    // Call into MASM ToolEngine_Execute, which finds an idle slot and triggers execution
    return ToolEngine_Execute(&m_state, name.c_str(), args.c_str(), priority);
}

bool SovereignToolBridge::cancelTool(uint32_t taskId) {
    if (!m_initialized) return false;
    return ToolEngine_Cancel(&m_state, taskId);
}

ToolStatus SovereignToolBridge::getToolStatus(uint32_t taskId) {
    ToolStatus status = { taskId, 0, "", 0 };
    if (!m_initialized) return status;

    char buffer[8192]; // Result/Error buffer from MASM
    uint32_t result = ToolEngine_GetStatus(&m_state, taskId, buffer, sizeof(buffer));
    
    // Status encoding: Bottom 2 bits are state, top bits are exitCode
    status.state = result & 0x03;
    status.exitCode = result >> 2;
    status.result = std::string(buffer);

    return status;
}

bool SovereignToolBridge::shutdown() {
    std::lock_guard<std::mutex> lock(g_toolMutex);
    if (!m_initialized) return false;

    // Signal terminal shutdown to all slots
    ToolEngine_Shutdown(&m_state);

    if (m_state.hThread) {
        WaitForSingleObject(m_state.hThread, 1000);
        CloseHandle(m_state.hThread);
    }

    // Clean up handle chain
    if (m_state.hStopEvent) CloseHandle(m_state.hStopEvent);
    if (m_state.hWakeEvent) CloseHandle(m_state.hWakeEvent);
    if (m_state.hTaskMutex) CloseHandle(m_state.hTaskMutex);
    if (m_state.hCompletionPort) CloseHandle(m_state.hCompletionPort);

    m_initialized = false;
    return true;
}

} // namespace RawrXD::Runtime
