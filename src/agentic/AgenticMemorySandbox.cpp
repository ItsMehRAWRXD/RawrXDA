// ============================================================================
// AgenticMemorySandbox.cpp — Phase 2: High-Speed Execution Guard implementation
// ============================================================================
#include "AgenticMemorySandbox.h"
#include "../p2p/EvolutionEventBus.h"
#include <iostream>
#include <mutex>

namespace SovereignAssembler {

// LAZY SINGLETON PATTERN: Avoid SIOF - std::mutex has non-trivial constructor
inline std::mutex& GetSandboxMutex() {
    static std::mutex* inst = new std::mutex();
    return *inst;
}
#define g_sandboxMutex GetSandboxMutex()

AgenticMemorySandbox& AgenticMemorySandbox::Instance() {
    static AgenticMemorySandbox instance;
    return instance;
}

void* AgenticMemorySandbox::AllocateExecutionPage(size_t size) {
    std::lock_guard<std::mutex> lock(g_sandboxMutex);

    // VirtualAlloc for PAGE_READWRITE (initial state)
    void* ptr = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (ptr) {
        m_authorizedRegions.push_back({ptr, size});
    }
    return ptr;
}

bool AgenticMemorySandbox::IsPointerAuthorized(void* ptr) {
    // Zero-dependency boundary check
    uintptr_t uPtr = (uintptr_t)ptr;
    for (const auto& region : m_authorizedRegions) {
        uintptr_t start = (uintptr_t)region.first;
        if (uPtr >= start && uPtr < start + region.second) {
            return true;
        }
    }
    return false;
}

bool AgenticMemorySandbox::SealExecutionPage(void* ptr, size_t size) {
    DWORD oldProtect;
    // Switch to PAGE_EXECUTE_READ (standard XOM hardening)
    if (VirtualProtect(ptr, size, PAGE_EXECUTE_READ, &oldProtect)) {
        char buf[128];
        sprintf(buf, "{\"address\": \"%p\", \"size\": %zu, \"protection\": \"XOM\"}", ptr, size);
        EvolutionEventBus::Instance().Emit("SealEvent", "LocalNode", buf);
        return true;
    }
    return false;
}

} // namespace SovereignAssembler
