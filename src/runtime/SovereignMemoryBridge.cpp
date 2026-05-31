#include "SovereignMemoryBridge.h"
#include <iostream>
#include <filesystem>
#include <mutex>

namespace RawrXD::Runtime {

static std::mutex g_memoryMutex;

SovereignMemoryBridge& SovereignMemoryBridge::instance() {
    static SovereignMemoryBridge instance;
    return instance;
}

bool SovereignMemoryBridge::initialize(const std::string& projectPath) {
    std::lock_guard<std::mutex> lock(g_memoryMutex);
    if (m_initialized) return true;

    // Call into the MASM MemorySystem_Initialize module
    // This maps the MEMORY.md, Topic Files, and Transcript layers
    if (MemorySystem_Initialize(nullptr, projectPath.c_str(), &m_context)) {
        m_initialized = true;
        std::cout << "[Sovereign] Memory System Initialized. Base path: " << projectPath << std::endl;
        return true;
    }

    std::cerr << "[Sovereign] ERROR: Critical failure during Memory System initialization (MASM Layer)." << std::endl;
    return false;
}

bool SovereignMemoryBridge::recordDecision(const std::string& title, const std::string& summary, int priority) {
    if (!m_initialized) return false;
    
    // MEM_TYPE_DECISION = 2 (per MASM constant)
    void* entry = MemorySystem_AddEntry(&m_context, 2, static_cast<uint32_t>(priority), title.c_str(), summary.c_str());
    return entry != nullptr;
}

bool SovereignMemoryBridge::recordPattern(const std::string& title, const std::string& pattern, int priority) {
    if (!m_initialized) return false;

    // MEM_TYPE_PATTERN = 3 (per MASM constant)
    void* entry = MemorySystem_AddEntry(&m_context, 3, static_cast<uint32_t>(priority), title.c_str(), pattern.c_str());
    return entry != nullptr;
}

bool SovereignMemoryBridge::shutdown() {
    std::lock_guard<std::mutex> lock(g_memoryMutex);
    if (!m_initialized) return false;

    // Trigger the auto-save event in MASM to flush final state
    SetEvent(m_context.hIndexEvent);
    
    // Clean up our context handles
    if (m_context.hMutex) {
        CloseHandle(m_context.hMutex);
    }
    if (m_context.hIndexEvent) {
        CloseHandle(m_context.hIndexEvent);
    }

    m_initialized = false;
    return true;
}

} // namespace RawrXD::Runtime
