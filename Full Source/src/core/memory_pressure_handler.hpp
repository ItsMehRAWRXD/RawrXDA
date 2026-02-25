// ============================================================================
// memory_pressure_handler.hpp — System Memory Pressure Monitor
// ============================================================================
// Monitors available physical memory and triggers eviction callbacks when
// memory falls below configurable thresholds.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#include <string>

namespace RawrXD {
namespace Memory {

// ============================================================================
// Pressure Level
// ============================================================================

enum class PressureLevel : uint8_t {
    NONE     = 0,   // Plenty of RAM
    LOW      = 1,   // Below 4GB free — pause indexing
    MEDIUM   = 2,   // Below 2GB free — evict embedding cache
    HIGH     = 3,   // Below 1GB free — evict all caches, stop inference
    CRITICAL = 4    // Below 512MB — emergency OOM avoidance
};

const char* pressureLevelName(PressureLevel level);

// ============================================================================
// Memory Stats
// ============================================================================

struct MemoryStats {
    uint64_t totalPhysical;      // Total physical RAM (bytes)
    uint64_t availablePhysical;  // Available physical RAM (bytes)
    uint64_t processPrivate;     // Process private bytes
    uint64_t processVirtual;     // Process virtual bytes
    float usagePercent;          // Physical usage percentage (0-100)
    PressureLevel pressure;      // Computed pressure level
};

// ============================================================================
// Pressure Callback
// ============================================================================

struct PressureCallback {
    const char* name;
    PressureLevel triggerLevel;  // Invoke when at or above this level
    std::function<void(PressureLevel, const MemoryStats&)> callback;
    bool enabled;
};

// ============================================================================
// MemoryPressureHandler
// ============================================================================

class MemoryPressureHandler {
public:
    MemoryPressureHandler();
    ~MemoryPressureHandler();

    // Non-copyable
    MemoryPressureHandler(const MemoryPressureHandler&) = delete;
    MemoryPressureHandler& operator=(const MemoryPressureHandler&) = delete;

    // ---- Configuration ----
    void setPollingIntervalMs(uint32_t ms) { m_pollIntervalMs = ms; }
    void setThresholdLow(uint64_t bytes)      { m_thresholdLow = bytes; }
    void setThresholdMedium(uint64_t bytes)    { m_thresholdMedium = bytes; }
    void setThresholdHigh(uint64_t bytes)      { m_thresholdHigh = bytes; }
    void setThresholdCritical(uint64_t bytes)  { m_thresholdCritical = bytes; }

    // ---- Lifecycle ----
    void start();
    void stop();
    bool isRunning() const { return m_running.load(std::memory_order_relaxed); }

    // ---- Snapshot ----
    MemoryStats snapshot() const;
    PressureLevel currentPressure() const;

    // ---- Callbacks ----
    void addCallback(const char* name, PressureLevel triggerLevel,
                     std::function<void(PressureLevel, const MemoryStats&)> cb);
    void removeCallback(const char* name);
    void clearCallbacks();

    // ---- Singleton ----
    static MemoryPressureHandler& Global();

private:
    void monitorLoop();
    PressureLevel computePressure(uint64_t availableBytes) const;
    void notifyCallbacks(PressureLevel level, const MemoryStats& stats);

    // Platform-specific
    static MemoryStats querySystemMemory();
    static uint64_t queryProcessMemory();

    // ---- State ----
    std::atomic<bool> m_running{false};
    std::atomic<PressureLevel> m_currentLevel{PressureLevel::NONE};
    std::thread m_monitorThread;
    mutable std::mutex m_mutex;
    std::vector<PressureCallback> m_callbacks;
    uint32_t m_pollIntervalMs = 2000;

    // Thresholds (bytes of available RAM)
    uint64_t m_thresholdLow      = 4ULL * 1024 * 1024 * 1024; // 4GB
    uint64_t m_thresholdMedium   = 2ULL * 1024 * 1024 * 1024; // 2GB
    uint64_t m_thresholdHigh     = 1ULL * 1024 * 1024 * 1024; // 1GB
    uint64_t m_thresholdCritical = 512ULL * 1024 * 1024;       // 512MB
};

} // namespace Memory
} // namespace RawrXD
