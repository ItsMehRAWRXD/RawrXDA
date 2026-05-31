#include "SovereignStabilityLayer.h"
#include <chrono>
#include <iostream>
#include <mutex>

namespace RawrXD::Runtime {

static std::mutex g_stabilityMutex;

SovereignStabilityLayer& SovereignStabilityLayer::instance() {
    static SovereignStabilityLayer instance;
    return instance;
}

bool SovereignStabilityLayer::registerKernelVersion(void* addr, uint64_t version, uint32_t crc) {
    std::lock_guard<std::mutex> lock(g_stabilityMutex);
    
    // Check for cooldown violations (anti-thrashing)
    auto now = std::chrono::steady_clock::now().time_since_epoch().count() / 1000000;
    if (m_registry.count(addr)) {
        if (now - m_registry[addr].lastMutationTime < COOLDOWN_MS) {
            std::cerr << "[Stability] REJECTED: Cooldown violation for function 0x" << std::hex << addr << std::endl;
            return false;
        }
    }

    FunctionMetadata meta;
    meta.version = { version, crc, static_cast<uint64_t>(now), LOCAL_TEST };
    meta.lastMutationTime = now;
    meta.mutationCount = m_registry.count(addr) ? m_registry[addr].mutationCount + 1 : 1;

    m_registry[addr] = meta;
    std::cout << "[Stability] REGISTERED: Function 0x" << std::hex << addr << " v" << version << " trust=" << meta.version.trust << std::endl;
    return true;
}

bool SovereignStabilityLayer::validatePatchTrust(void* addr, PatchTrust required) {
    std::lock_guard<std::mutex> lock(g_stabilityMutex);
    if (!m_registry.count(addr)) return false;
    return m_registry[addr].version.trust >= required;
}

bool SovereignStabilityLayer::checkCooldown(void* addr) {
    std::lock_guard<std::mutex> lock(g_stabilityMutex);
    if (!m_registry.count(addr)) return true;
    auto now = std::chrono::steady_clock::now().time_since_epoch().count() / 1000000;
    return (now - m_registry[addr].lastMutationTime) > COOLDOWN_MS;
}

} // namespace RawrXD::Runtime
