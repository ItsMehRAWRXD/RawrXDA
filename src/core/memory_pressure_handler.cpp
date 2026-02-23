// ============================================================================
// memory_pressure_handler.cpp — System Memory Pressure Monitor Implementation
// ============================================================================
// Uses Windows GlobalMemoryStatusEx / POSIX sysinfo for memory queries.
// Polling-based with configurable thresholds and callback notification.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "core/memory_pressure_handler.hpp"

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#else
#include <sys/sysinfo.h>
#include <unistd.h>
#include <fstream>
#endif

#include <algorithm>
#include <chrono>

namespace RawrXD {
namespace Memory {

// ============================================================================
// Pressure Level Names
// ============================================================================

const char* pressureLevelName(PressureLevel level) {
    switch (level) {
        case PressureLevel::NONE:     return "NONE";
        case PressureLevel::LOW:      return "LOW";
        case PressureLevel::MEDIUM:   return "MEDIUM";
        case PressureLevel::HIGH:     return "HIGH";
        case PressureLevel::CRITICAL: return "CRITICAL";
        default:                      return "UNKNOWN";
    }
}

// ============================================================================
// Singleton
// ============================================================================

MemoryPressureHandler& MemoryPressureHandler::Global() {
    static MemoryPressureHandler instance;
    return instance;
}

// ============================================================================
// Construction / Destruction
// ============================================================================

MemoryPressureHandler::MemoryPressureHandler() = default;

MemoryPressureHandler::~MemoryPressureHandler() {
    stop();
}

// ============================================================================
// Lifecycle
// ============================================================================

void MemoryPressureHandler::start() {
    if (m_running.exchange(true)) return;  // Already running

    m_monitorThread = std::thread(&MemoryPressureHandler::monitorLoop, this);

#ifdef _WIN32
    SetThreadDescription(m_monitorThread.native_handle(), L"RawrXD-MemoryMonitor");
#endif
}

void MemoryPressureHandler::stop() {
    if (!m_running.exchange(false)) return;

    if (m_monitorThread.joinable()) {
        m_monitorThread.join();
    }
}

// ============================================================================
// Snapshot
// ============================================================================

MemoryStats MemoryPressureHandler::snapshot() const {
    MemoryStats stats = querySystemMemory();
    stats.processPrivate = queryProcessMemory();
    stats.pressure = const_cast<MemoryPressureHandler*>(this)->computePressure(stats.availablePhysical);
    return stats;
}

PressureLevel MemoryPressureHandler::currentPressure() const {
    return m_currentLevel.load(std::memory_order_relaxed);
}

// ============================================================================
// Callbacks
// ============================================================================

void MemoryPressureHandler::addCallback(const char* name, PressureLevel triggerLevel,
                                         std::function<void(PressureLevel, const MemoryStats&)> cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callbacks.push_back({name, triggerLevel, std::move(cb), true});
}

void MemoryPressureHandler::removeCallback(const char* name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callbacks.erase(
        std::remove_if(m_callbacks.begin(), m_callbacks.end(),
            [name](const PressureCallback& cb) {
                return cb.name && std::strcmp(cb.name, name) == 0;
            }),
        m_callbacks.end()
    );
}

void MemoryPressureHandler::clearCallbacks() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callbacks.clear();
}

// ============================================================================
// Monitor Loop
// ============================================================================

void MemoryPressureHandler::monitorLoop() {
    PressureLevel lastLevel = PressureLevel::NONE;

    while (m_running.load(std::memory_order_relaxed)) {
        MemoryStats stats = querySystemMemory();
        stats.processPrivate = queryProcessMemory();
        PressureLevel level = computePressure(stats.availablePhysical);
        stats.pressure = level;

        m_currentLevel.store(level, std::memory_order_relaxed);

        // Notify on level change or when at elevated levels
        if (level != lastLevel || level >= PressureLevel::MEDIUM) {
            notifyCallbacks(level, stats);
            lastLevel = level;
        }

        // Sleep for polling interval
        auto sleepMs = m_pollIntervalMs;
        // Poll faster under pressure
        if (level >= PressureLevel::HIGH) sleepMs = std::min(sleepMs, 500u);
        if (level >= PressureLevel::CRITICAL) sleepMs = std::min(sleepMs, 200u);

        std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    }
}

PressureLevel MemoryPressureHandler::computePressure(uint64_t availableBytes) const {
    if (availableBytes <= m_thresholdCritical) return PressureLevel::CRITICAL;
    if (availableBytes <= m_thresholdHigh)     return PressureLevel::HIGH;
    if (availableBytes <= m_thresholdMedium)   return PressureLevel::MEDIUM;
    if (availableBytes <= m_thresholdLow)      return PressureLevel::LOW;
    return PressureLevel::NONE;
}

void MemoryPressureHandler::notifyCallbacks(PressureLevel level, const MemoryStats& stats) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& cb : m_callbacks) {
        if (cb.enabled && cb.callback && level >= cb.triggerLevel) {
            cb.callback(level, stats);
        }
    }
}

// ============================================================================
// Platform-Specific Memory Queries
// ============================================================================

#ifdef _WIN32

MemoryStats MemoryPressureHandler::querySystemMemory() {
    MemoryStats stats{};

    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(memInfo);
    if (GlobalMemoryStatusEx(&memInfo)) {
        stats.totalPhysical = memInfo.ullTotalPhys;
        stats.availablePhysical = memInfo.ullAvailPhys;
        stats.processVirtual = memInfo.ullTotalVirtual - memInfo.ullAvailVirtual;
        stats.usagePercent = static_cast<float>(memInfo.dwMemoryLoad);
    }

    return stats;
}

uint64_t MemoryPressureHandler::queryProcessMemory() {
    PROCESS_MEMORY_COUNTERS_EX pmc;
    pmc.cb = sizeof(pmc);
    if (GetProcessMemoryInfo(GetCurrentProcess(),
                              reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                              sizeof(pmc))) {
        return pmc.PrivateUsage;
    }
    return 0;
}

#else  // POSIX

MemoryStats MemoryPressureHandler::querySystemMemory() {
    MemoryStats stats{};

    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        stats.totalPhysical = static_cast<uint64_t>(si.totalram) * si.mem_unit;
        stats.availablePhysical = static_cast<uint64_t>(si.freeram) * si.mem_unit;
        stats.usagePercent = stats.totalPhysical > 0
            ? 100.0f * (1.0f - static_cast<float>(stats.availablePhysical) / stats.totalPhysical)
            : 0.0f;
    }

    // Try /proc/meminfo for MemAvailable (more accurate than free RAM)
    std::ifstream meminfo("/proc/meminfo");
    std::string line;
    while (std::getline(meminfo, line)) {
        if (line.find("MemAvailable:") == 0) {
            uint64_t kb = 0;
            std::sscanf(line.c_str(), "MemAvailable: %lu kB", &kb);
            stats.availablePhysical = kb * 1024;
            break;
        }
    }

    return stats;
}

uint64_t MemoryPressureHandler::queryProcessMemory() {
    // Read /proc/self/statm for RSS
    std::ifstream statm("/proc/self/statm");
    uint64_t pages = 0, resident = 0;
    statm >> pages >> resident;
    return resident * static_cast<uint64_t>(sysconf(_SC_PAGESIZE));
}

#endif

} // namespace Memory
} // namespace RawrXD
