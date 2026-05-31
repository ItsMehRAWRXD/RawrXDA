#include "SovereignHotpatchBridge.h"
#include <iostream>
#include <mutex>
#include <vector>
#include <windows.h>

extern "C" {
    // Forward declarations for MASM Hotpatcher functions
    void RawrXD_Hotpatch_ApplyAtomic(void* target, void* data, size_t size);
    bool RawrXD_Hotpatch_VerifyIntegrity(void* target, void* expected, size_t size);
}

namespace RawrXD::Runtime {

static std::mutex g_patchMutex;

SovereignHotpatchBridge& SovereignHotpatchBridge::instance() {
    static SovereignHotpatchBridge instance;
    return instance;
}

bool SovereignHotpatchBridge::initialize() {
    std::lock_guard<std::mutex> lock(g_patchMutex);
    if (m_active) return true;

    // Initialization of the Sovereign Hotpatcher subsystem.
    // This allows for the atomic replacement of kernel functions.
    m_active = true;
    std::cout << "[Hotpatch] Sovereign V2 Hot-Swap Infrastructure Initialized." << std::endl;
    return true;
}

void SovereignHotpatchBridge::guardMemory(void* addr, size_t size, uint32_t protectType) {
    DWORD oldProtect;
    VirtualProtect(addr, size, protectType, &oldProtect);
}

bool SovereignHotpatchBridge::applyPatch(void* target, void* data, size_t size) {
    std::lock_guard<std::mutex> lock(g_patchMutex);
    if (!m_active) return false;

    // 1. Check for overlap or existing patch
    if (m_patchRegistry.count(target)) {
        return false; 
    }

    // 2. Prepare Backup (for revert)
    HotpatchBlock block;
    block.targetAddr = target;
    block.size = size;
    block.originalData = malloc(size);
    memcpy(block.originalData, target, size);
    block.newData = malloc(size);
    memcpy(block.newData, data, size);

    // 3. Hot-Swap (requires temporary write access)
    guardMemory(target, size, PAGE_EXECUTE_READWRITE);
    RawrXD_Hotpatch_ApplyAtomic(target, data, size);
    guardMemory(target, size, PAGE_EXECUTE_READ);

    // 4. Registry Logging
    block.isActive = true;
    m_patchRegistry[target] = block;

    std::cout << "[Hotpatch] ATOMIC SUCCESS: Redirected memory at 0x" << std::hex << target << std::endl;
    return true;
}

bool SovereignHotpatchBridge::revertPatch(void* target) {
    std::lock_guard<std::mutex> lock(g_patchMutex);
    if (!m_active || !m_patchRegistry.count(target)) return false;

    HotpatchBlock& block = m_patchRegistry[target];
    
    // Reverse Apply
    guardMemory(target, block.size, PAGE_EXECUTE_READWRITE);
    RawrXD_Hotpatch_ApplyAtomic(target, block.originalData, block.size);
    guardMemory(target, block.size, PAGE_EXECUTE_READ);

    // Cleanup
    free(block.originalData);
    free(block.newData);
    m_patchRegistry.erase(target);

    std::cout << "[Hotpatch] REVERT SUCCESS: Memory at 0x" << std::hex << target << " restored to original state." << std::endl;
    return true;
}

bool SovereignHotpatchBridge::verifyPatch(void* target) {
    if (!m_active || !m_patchRegistry.count(target)) return false;
    HotpatchBlock& block = m_patchRegistry[target];
    return RawrXD_Hotpatch_VerifyIntegrity(target, block.newData, block.size);
}

void SovereignHotpatchBridge::shutdown() {
    std::lock_guard<std::mutex> lock(g_patchMutex);
    for (auto& pair : m_patchRegistry) {
        // [Cleanup logic for any still-active patches]
    }
    m_active = false;
    std::cout << "[Hotpatch] Sovereign Offline." << std::endl;
}

} // namespace RawrXD::Runtime
