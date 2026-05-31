#include "Sentinel.hpp"
#include "../observability/Telemetry.hpp"
#include "Detour.hpp"
#include <chrono>
#include <intrin.h>
#include <map>
#include <mutex>
#include <vector>
#include <windows.h>


namespace RawrXD::Agentic::Hotpatch
{

extern "C" uint64_t RawrXD_Sentinel_CalculateHash_MASM(void* addr, size_t size);
}

// Single definition lives in Telemetry.cpp (node_heartbeat.asm / observability lane).
extern "C" uint64_t g_last_zmm_heartbeat;

namespace RawrXD::Agentic::Hotpatch
{

SentinelSystem& SentinelSystem::instance()
{
    static SentinelSystem s;
    return s;
}

void SentinelSystem::setHeartbeatThreshold(uint64_t cycles)
{
    std::lock_guard<std::mutex> lock(mtx);
    heartbeatThreshold = cycles;
}

bool SentinelSystem::checkHealth()
{
    uint64_t current = __rdtsc();
    uintptr_t heartbeat_ptr = (uintptr_t)&g_last_zmm_heartbeat;

    // Pulse check: Has the ZMM lane emitted a heartbeat recently?
    uint64_t last = *(uint64_t*)heartbeat_ptr;
    if (last == 0)
        return true;  // Not started yet

    if (current > last && (current - last) > heartbeatThreshold)
    {
        RawrXD::Agentic::Observability::Telemetry::instance().logError(
            "Sentinel", "ZMM Lane Stall Detected! Last heartbeat " + std::to_string(current - last) + " cycles ago.");
        return false;
    }
    return true;
}

void SentinelSystem::registerRegion(void* target, size_t size, const char* name, const uint8_t* patchData)
{
    std::lock_guard<std::mutex> lock(mtx);
    PatchSentinel s;
    s.target = target;
    s.size = size;
    s.description = name ? name : "Unnamed Patch";
    s.violationCount = 0;

    if (patchData)
    {
        s.patchBytes.assign(patchData, patchData + size);
    }
    else
    {
        s.patchBytes.resize(size);
        memcpy(s.patchBytes.data(), target, size);
    }

    s.expectedHash = RawrXD_Sentinel_CalculateHash_MASM(target, size);
    activePatches[target] = std::move(s);

    RawrXD::Agentic::Observability::Telemetry::instance().logInfo("Sentinel registered region: " + s.description +
                                                                  " at " + std::to_string((uintptr_t)target));
}

void SentinelSystem::audit()
{
    std::lock_guard<std::mutex> lock(mtx);
    for (auto& pair : activePatches)
    {
        PatchSentinel& patch = pair.second;
        uint64_t currentHash = RawrXD_Sentinel_CalculateHash_MASM(patch.target, patch.size);
        if (currentHash != patch.expectedHash)
        {
            patch.violationCount++;

            RawrXD::Agentic::Observability::Telemetry::instance().logError(
                "Sentinel", "Integrity violation at " + patch.description + "! Attempting self-heal.");

            if (patch.selfHeal)
            {
                DWORD old;
                if (VirtualProtect(patch.target, patch.size, PAGE_EXECUTE_READWRITE, &old))
                {
                    memcpy(patch.target, patch.patchBytes.data(), patch.size);
                    VirtualProtect(patch.target, patch.size, old, &old);
                    FlushInstructionCache(GetCurrentProcess(), patch.target, patch.size);

                    RawrXD::Agentic::Observability::Telemetry::instance().logInfo(
                        "Sentinel successfully restored patch: " + patch.description);
                }
            }
        }
    }
}

void SentinelSystem::monitorLoop(uint32_t intervalMs)
{
    while (running)
    {
        audit();
        checkHealth();
        std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
    }
}

void SentinelSystem::startBackgroundMonitor(uint32_t intervalMs)
{
    if (running)
        return;
    running = true;
    worker = std::thread([this, intervalMs]() { this->monitorLoop(intervalMs); });
}

void SentinelSystem::stopBackgroundMonitor()
{
    if (!running)
        return;
    running = false;
    if (worker.joinable())
        worker.join();
}

std::vector<SentinelSystem::Report> SentinelSystem::getStatus()
{
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<Report> reports;
    for (auto& pair : activePatches)
    {
        PatchSentinel& patch = pair.second;
        uint64_t currentHash = RawrXD_Sentinel_CalculateHash_MASM(patch.target, patch.size);
        reports.push_back(
            {patch.target, patch.description, patch.violationCount.load(), currentHash != patch.expectedHash});
    }
    return reports;
}

}  // namespace RawrXD::Agentic::Hotpatch
