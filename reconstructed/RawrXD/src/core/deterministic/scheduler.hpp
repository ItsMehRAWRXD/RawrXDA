// ============================================================================
// deterministic_scheduler.hpp — Deterministic Scheduler Mode
// ============================================================================
// Provides replay-safe, deterministic scheduling of inference and hotpatch
// operations. In deterministic mode, all task ordering is fixed by a
// monotonic sequence number and a reproducible priority scheme — no
// wall-clock jitter, no OS scheduler non-determinism.
//
// Use cases:
//   - Regression testing of inference outputs
//   - Replay-based debugging (transition journal → replay)
//   - Certification of hotpatch application order
//
// Architecture:
//   - Tasks enqueue with a logical tick (not wall clock)
//   - Scheduler drains tasks in strict (tick, priority, insertOrder) order
//   - Optional seed-based RNG for tie-breaking in fuzz mode
//   - Barrier operations for synchronization points
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
#include <functional>
#include <chrono>

namespace RawrXD {
namespace Scheduling {

// ============================================================================
// SchedulerMode
// ============================================================================
enum class SchedulerMode : uint8_t {
    Normal          = 0,   // OS-scheduled, wall-clock timestamps
    Deterministic   = 1,   // Logical ticks, fixed ordering
    Replay          = 2,   // Replaying from a recorded journal
    Fuzz            = 3,   // Deterministic with seeded random tie-breaking
};

const char* schedulerModeName(SchedulerMode m);

// ============================================================================
// Task Priority (matches thread_pool.hpp ordering)
// ============================================================================
enum class SchedPriority : uint8_t {
    Critical = 0,
    High     = 1,
    Normal   = 2,
    Low      = 3,
    Idle     = 4,
};

// ============================================================================
// ScheduledTask
// ============================================================================
struct ScheduledTask {
    uint64_t        taskId;             // Unique, monotonic
    uint64_t        logicalTick;        // Deterministic ordering key
    SchedPriority   priority;
    uint64_t        insertOrder;        // FIFO within same (tick, priority)
    const char*     label;              // Human-readable debug label
    std::function<void()> work;

    // Strict total order: (logicalTick, priority, insertOrder)
    bool operator>(const ScheduledTask& other) const {
        if (logicalTick != other.logicalTick)
            return logicalTick > other.logicalTick;
        if (priority != other.priority)
            return static_cast<uint8_t>(priority) > static_cast<uint8_t>(other.priority);
        return insertOrder > other.insertOrder;
    }
};

// ============================================================================
// SchedulerBarrier — synchronization point
// ============================================================================
struct SchedulerBarrier {
    uint64_t    barrierId;
    uint64_t    atTick;                 // Barrier fires at this logical tick
    const char* label;
    bool        reached;
};

// ============================================================================
// Scheduler Stats
// ============================================================================
struct SchedulerStats {
    std::atomic<uint64_t> tasksEnqueued{0};
    std::atomic<uint64_t> tasksExecuted{0};
    std::atomic<uint64_t> barriersHit{0};
    std::atomic<uint64_t> ticksAdvanced{0};
    std::atomic<uint64_t> replaySteps{0};
    uint64_t              currentTick = 0;
    SchedulerMode         mode = SchedulerMode::Normal;
};

// ============================================================================
// Replay Entry — recorded task execution for replay
// ============================================================================
struct ReplayEntry {
    uint64_t    taskId;
    uint64_t    logicalTick;
    SchedPriority priority;
    const char* label;
    double      executionMs;
};

// ============================================================================
// SchedulerResult
// ============================================================================
struct SchedulerResult {
    bool        success;
    const char* detail;
    int         errorCode;
    uint64_t    taskId;
    uint64_t    currentTick;

    static SchedulerResult ok(uint64_t id, uint64_t tick,
                               const char* msg = "OK") {
        return {true, msg, 0, id, tick};
    }
    static SchedulerResult error(const char* msg, int code = -1) {
        return {false, msg, code, 0, 0};
    }
};

// ============================================================================
// DeterministicScheduler
// ============================================================================
class DeterministicScheduler {
public:
    DeterministicScheduler();
    ~DeterministicScheduler();

    // Non-copyable
    DeterministicScheduler(const DeterministicScheduler&) = delete;
    DeterministicScheduler& operator=(const DeterministicScheduler&) = delete;

    // ---- Mode Control ----
    PatchResult setMode(SchedulerMode mode);
    SchedulerMode getMode() const;
    PatchResult setSeed(uint64_t seed);  // For Fuzz mode

    // ---- Task Submission ----
    SchedulerResult submit(std::function<void()> work,
                           SchedPriority priority = SchedPriority::Normal,
                           const char* label = nullptr);

    SchedulerResult submitAtTick(uint64_t tick,
                                 std::function<void()> work,
                                 SchedPriority priority = SchedPriority::Normal,
                                 const char* label = nullptr);

    // ---- Barriers ----
    PatchResult addBarrier(uint64_t atTick, const char* label = nullptr);
    bool        isBarrierReached(uint64_t barrierId) const;

    // ---- Execution ----
    // Step one logical tick: execute all tasks at current tick, advance
    SchedulerResult stepTick();

    // Run until all tasks are drained or a barrier is hit
    SchedulerResult runUntilIdle();

    // Run until a specific tick is reached
    SchedulerResult runUntilTick(uint64_t targetTick);

    // ---- Tick Management ----
    uint64_t currentTick() const;
    PatchResult advanceTick(uint64_t delta = 1);
    PatchResult resetTick();

    // ---- Replay ----
    PatchResult startRecording();
    PatchResult stopRecording();
    std::vector<ReplayEntry> getRecording() const;
    PatchResult loadReplay(const std::vector<ReplayEntry>& entries);

    // Export/import replay as JSON
    std::string exportReplayJson() const;
    PatchResult importReplayJson(const std::string& json);

    // ---- Stats ----
    SchedulerStats getStats() const;
    void resetStats();

    // ---- Queue Inspection ----
    size_t pendingTaskCount() const;
    size_t pendingTaskCountAtTick(uint64_t tick) const;

private:
    void sortQueue();
    uint64_t nextTaskId();
    uint64_t fuzzTieBreak(uint64_t a, uint64_t b);

    mutable std::mutex m_mutex;

    SchedulerMode       m_mode = SchedulerMode::Normal;
    std::atomic<uint64_t> m_currentTick{0};
    std::atomic<uint64_t> m_nextTaskId{1};
    std::atomic<uint64_t> m_nextBarrierId{1};
    uint64_t            m_seed = 0;

    // Priority queue (sorted on drain)
    std::vector<ScheduledTask> m_queue;
    std::vector<SchedulerBarrier> m_barriers;

    // Recording
    bool m_recording = false;
    std::vector<ReplayEntry> m_replayLog;
    std::vector<ReplayEntry> m_replaySource;   // For replay mode
    size_t m_replayIndex = 0;

    // Label storage for imported replays (keeps c_str() pointers valid)
    std::vector<std::string> m_labelStorage;

    SchedulerStats m_stats;
};

} // namespace Scheduling
} // namespace RawrXD
