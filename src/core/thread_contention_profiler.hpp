// ============================================================================
// thread_contention_profiler.hpp — Thread Contention Profiler
// ============================================================================
// Instruments all mutex acquisitions to measure contention: how long threads
// wait for locks, which locks are most contested, and which thread pairs
// contend most frequently. Produces a contention heat-map and per-lock stats.
//
// Architecture:
//   - Wraps lock/unlock with timing instrumentation
//   - Samples via RDTSC or QPC for sub-microsecond resolution
//   - Per-lock contention counters (wait time, hold time, waiter count)
//   - Per-thread contention profile (total blocked time per thread)
//   - Lock-pair contention matrix (which locks block other locks)
//   - Ring-buffer of top-N contention events for post-mortem
//
// Integration:
//   - Works alongside lock_hierarchy.hpp HierarchicalMutex
//   - Can profile any std::mutex via ContentiousLock wrapper
//   - Zero overhead when profiling is disabled (compile-time or runtime)
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include "model_memory_hotpatch.hpp"   // PatchResult
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <atomic>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <unordered_map>

namespace RawrXD {
namespace Profiling {

// ============================================================================
// Contention Event — a single lock-wait record
// ============================================================================
struct ContentionEvent {
    uint64_t        eventId;
    uint64_t        timestampUs;       // Microseconds since epoch
    std::thread::id waitingThread;
    std::thread::id holdingThread;     // If known
    const char*     lockName;
    uint16_t        lockLevel;         // From LockLevel hierarchy
    double          waitTimeUs;        // How long the thread waited
    double          holdTimeUs;        // How long the lock was held
    bool            wasContended;      // True if another thread held it
};

// ============================================================================
// Per-Lock Contention Stats
// ============================================================================
struct LockContentionStats {
    const char*     lockName;
    uint16_t        lockLevel;
    std::atomic<uint64_t> totalAcquisitions{0};
    std::atomic<uint64_t> contentedAcquisitions{0}; // Had to wait
    std::atomic<uint64_t> uncontentedAcquisitions{0};
    double          totalWaitTimeUs = 0;
    double          totalHoldTimeUs = 0;
    double          maxWaitTimeUs = 0;
    double          maxHoldTimeUs = 0;
    double          avgWaitTimeUs = 0;
    double          avgHoldTimeUs = 0;
    uint64_t        maxConcurrentWaiters = 0;
    std::atomic<uint64_t> currentWaiters{0};
};

// ============================================================================
// Per-Thread Contention Profile
// ============================================================================
struct ThreadContentionProfile {
    std::thread::id threadId;
    uint64_t        totalAcquisitions = 0;
    double          totalBlockedTimeUs = 0;
    double          maxSingleWaitUs = 0;
    const char*     mostContestedLock = nullptr;
    double          mostContestedWaitUs = 0;
};

// ============================================================================
// Contention Heat Cell — lock-pair contention matrix entry
// ============================================================================
struct ContentionHeatCell {
    const char*     lockA;
    const char*     lockB;
    uint64_t        contentionCount;       // Times lockA blocked waiting for lockB
    double          totalContentionTimeUs;
};

// ============================================================================
// Profiler Configuration
// ============================================================================
struct ProfilerConfig {
    bool        enabled = false;            // Master enable
    uint32_t    sampleRatePercent = 100;    // 100 = every acquisition; 10 = 10%
    size_t      maxEventHistory = 8192;     // Ring buffer size
    double      contentionThresholdUs = 10; // Only record waits > threshold
    bool        trackHoldTime = true;
    bool        trackWaiters = true;
    bool        buildHeatMap = true;
};

// ============================================================================
// Profiler Stats
// ============================================================================
struct ProfilerStats {
    std::atomic<uint64_t> totalSampled{0};
    std::atomic<uint64_t> contentionEvents{0};
    std::atomic<uint64_t> droppedEvents{0};   // Ring buffer overflow
    double                totalBlockedTimeUs = 0;
    size_t                trackedLocks = 0;
    size_t                trackedThreads = 0;
};

// ============================================================================
// ContentionProfiler — the main profiling engine
// ============================================================================
class ContentionProfiler {
public:
    static ContentionProfiler& instance();

    // ---- Configuration ----
    PatchResult configure(const ProfilerConfig& config);
    ProfilerConfig getConfig() const;
    PatchResult enable(bool on);
    bool isEnabled() const;

    // ---- Lock Registration ----
    PatchResult registerLock(const char* name, uint16_t level = 0xFFFF);
    PatchResult unregisterLock(const char* name);

    // ---- Instrumentation Hooks ----
    // Called by ContentionMutex or manually around lock operations
    void onLockAttempt(const char* lockName);
    void onLockAcquired(const char* lockName, double waitTimeUs);
    void onLockReleased(const char* lockName, double holdTimeUs);

    // ---- Queries ----
    // Per-lock stats
    LockContentionStats getLockStats(const char* name) const;
    std::vector<LockContentionStats> allLockStats() const;
    std::vector<LockContentionStats> topContentedLocks(size_t n) const;

    // Per-thread profiles
    ThreadContentionProfile getThreadProfile(std::thread::id tid) const;
    std::vector<ThreadContentionProfile> allThreadProfiles() const;

    // Heat map
    std::vector<ContentionHeatCell> getHeatMap() const;

    // Event history
    std::vector<ContentionEvent> recentEvents(size_t count) const;

    // ---- Stats ----
    ProfilerStats getStats() const;
    void resetStats();
    void reset();   // Clear everything (stats + events + registrations)

    // ---- Export ----
    std::string exportStatsJson() const;
    std::string exportHeatMapJson() const;
    std::string exportSummary() const;
    std::string exportDot() const;  // Graphviz of hot contention paths

private:
    ContentionProfiler();
    ~ContentionProfiler() = default;

    void recordEvent(const ContentionEvent& event);
    LockContentionStats* findLock(const char* name);
    ThreadContentionProfile* findOrCreateThread(std::thread::id tid);

    mutable std::mutex m_mutex;

    ProfilerConfig m_config;

    // Per-lock data (keyed by name)
    std::unordered_map<std::string, LockContentionStats> m_lockStats;

    // Per-thread data
    std::unordered_map<size_t, ThreadContentionProfile> m_threadProfiles;

    // Heat map
    std::unordered_map<std::string, ContentionHeatCell> m_heatMap;

    // Event ring buffer
    std::vector<ContentionEvent> m_events;
    size_t m_eventHead = 0;

    std::atomic<uint64_t> m_nextEventId{0};
    ProfilerStats m_stats;
};

// ============================================================================
// ContentionMutex — instrumented mutex wrapper
// ============================================================================
class ContentionMutex {
public:
    explicit ContentionMutex(const char* name, uint16_t level = 0xFFFF);
    ~ContentionMutex() = default;

    // Non-copyable
    ContentionMutex(const ContentionMutex&) = delete;
    ContentionMutex& operator=(const ContentionMutex&) = delete;

    void lock();
    bool try_lock();
    void unlock();

    const char* name() const { return m_name; }

private:
    std::mutex  m_mutex;
    const char* m_name;
    uint16_t    m_level;

    // Per-acquisition timing
    std::chrono::steady_clock::time_point m_acquireTime;
};

// ============================================================================
// RAII guard for ContentionMutex
// ============================================================================
using ContentionLockGuard = std::lock_guard<ContentionMutex>;

} // namespace Profiling
} // namespace RawrXD
