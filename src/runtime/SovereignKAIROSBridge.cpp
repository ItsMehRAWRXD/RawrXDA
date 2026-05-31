#include "SovereignKAIROSBridge.h"
#include "SovereignMemoryBridge.h"
#include <iostream>
#include <mutex>

namespace RawrXD::Runtime {

static std::mutex g_kairosMutex;

SovereignKAIROSBridge& SovereignKAIROSBridge::instance() {
    static SovereignKAIROSBridge instance;
    return instance;
}

bool SovereignKAIROSBridge::initialize() {
    std::lock_guard<std::mutex> lock(g_kairosMutex);
    if (m_initialized) return true;

    // KAIROS requires the MemorySystem context to bind correctly for its "AutoDream" consolidation
    void* memoryContext = SovereignMemoryBridge::instance().getContext();
    if (!memoryContext) return false;

    // Call into the MASM KAIROS_Initialize module
    // This starts the background observation daemon
    if (KAIROS_Initialize(nullptr, memoryContext, &m_context)) {
        m_initialized = true;
        std::cout << "[KAIROS] Background Observation Engine Initialized." << std::endl;
        return true;
    }

    std::cerr << "[KAIROS] ERROR: Critical failure during KAIROS initialization (MASM Layer)." << std::endl;
    return false;
}

bool SovereignKAIROSBridge::startWatching(const std::wstring& projectDir) {
    if (!m_initialized) return false;

    // WATCH_FILE_MODIFY = 1, WATCH_FILE_CREATE = 2, WATCH_FILE_DELETE = 4
    // WATCH_DIR_CHANGE = 8 (per MASM constants)
    uint32_t flags = 15; // All active watches
    return KAIROS_AddDirectoryWatch(&m_context, projectDir.c_str(), flags) != FALSE;
}

bool SovereignKAIROSBridge::checkHealth(KairosStats& stats) {
    if (!m_initialized) return false;
    stats = m_context.stats;
    return true;
}

bool SovereignKAIROSBridge::shutdown() {
    std::lock_guard<std::mutex> lock(g_kairosMutex);
    if (!m_initialized) return false;

    // Signal the MASM background thread to stop
    SetEvent(m_context.hStopEvent);
    
    // Give it a small window to clean up
    if (m_context.hThread) {
        WaitForSingleObject(m_context.hThread, 500);
        CloseHandle(m_context.hThread);
    }

    // Clean up handles
    if (m_context.hStopEvent) CloseHandle(m_context.hStopEvent);
    if (m_context.hWakeEvent) CloseHandle(m_context.hWakeEvent);
    if (m_context.hTaskMutex) CloseHandle(m_context.hTaskMutex);
    if (m_context.hCompletionPort) CloseHandle(m_context.hCompletionPort);

    m_initialized = false;
    return true;
}

} // namespace RawrXD::Runtime
