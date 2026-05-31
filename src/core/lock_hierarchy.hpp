// ============================================================================
// lock_hierarchy.hpp — Strict Lock Hierarchy Documentation & Enforcement
// ============================================================================
// Defines the complete lock acquisition ordering for every mutex in the
// RawrXD-Shell system. Acquiring locks out of order is a deadlock risk.
// This file provides:
//   1. A numbered hierarchy (lower number = acquired first)
//   2. A HierarchicalMutex wrapper that asserts acquisition order at runtime
//   3. A global registry for auditing which threads hold which locks
//
// LOCK ACQUISITION ORDER (must always be respected):
//
//  Level  0: g_processLock          — Process-wide singleton init
//  Level  1: g_memoryPatchLock      — Memory Hotpatch layer (VirtualProtect)
//  Level  2: g_bytePatchLock        — Byte-Level Hotpatch layer (mmap)
//  Level  3: g_serverPatchLock      — Server Hotpatch layer
//  Level  4: g_proxyPatchLock       — Proxy Hotpatcher
//  Level  5: g_unifiedHotpatchLock  — Unified Hotpatch Manager (routes to 1-4)
//  Level  6: g_recoveryJournalLock  — Recovery Journal writes
//  Level  7: g_stateMachineLock     — Inference State Machine transitions
//  Level  8: g_schedulerLock        — Deterministic Scheduler queue
//  Level  9: g_threadPoolLock       — Thread Pool submission/drain
//  Level 10: g_kvCacheLock          — KV Cache read/write
//  Level 11: g_modelLoaderLock      — Model loading/streaming
//  Level 12: g_inferenceEngineLock  — Forward pass execution
//  Level 13: g_healthMonitorLock    — Subsystem Health Monitor
//  Level 14: g_telemetryLock        — Telemetry/metrics emission
//  Level 15: g_configLock           — Configuration reads
//  Level 16: g_logLock              — Logging subsystem (LAST — always safe)
//
// RULES:
//   - Never acquire a lower-numbered lock while holding a higher-numbered one
//   - Never hold more than 4 locks simultaneously
//   - Never use recursive mutexes
//   - All locks use std::lock_guard (RAII) — no manual unlock
//   - Callbacks must not acquire locks (caller provides lock-free context)
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
#include <array>

namespace RawrXD {
namespace Locking {

// ============================================================================
// Lock Level Constants
// ============================================================================
enum class LockLevel : uint16_t {
    Process             = 0,
    MemoryPatch         = 1,
    BytePatch           = 2,
    ServerPatch         = 3,
    ProxyPatch          = 4,
    UnifiedHotpatch     = 5,
    RecoveryJournal     = 6,
    StateMachine        = 7,
    Scheduler           = 8,
    ThreadPool          = 9,
    KVCache             = 10,
    ModelLoader         = 11,
    InferenceEngine     = 12,
    HealthMonitor       = 13,
    Telemetry           = 14,
    Config              = 15,
    Logging             = 16,

    _COUNT              = 17,
    UNRANKED            = 0xFFFF,  // For locks not yet integrated
};

const char* lockLevelName(LockLevel level);

// ============================================================================
// Lock Violation Info
// ============================================================================
struct LockViolation {
    std::thread::id threadId;
    LockLevel       heldLevel;
    LockLevel       requestedLevel;
    const char*     heldLockName;
    const char*     requestedLockName;
    uint64_t        timestamp;
};

// ============================================================================
// Per-Thread Lock State (thread_local)
// ============================================================================
struct ThreadLockState {
    static constexpr size_t MAX_HELD = 8;
    LockLevel   heldLocks[MAX_HELD] = {};
    const char* heldNames[MAX_HELD] = {};
    size_t      heldCount = 0;
    LockLevel   highestHeld = LockLevel::UNRANKED;
};

// ============================================================================
// HierarchicalMutex — drop-in mutex replacement with order enforcement
// ============================================================================
class HierarchicalMutex {
public:
    explicit HierarchicalMutex(LockLevel level, const char* name = nullptr);
    ~HierarchicalMutex();

    // Non-copyable
    HierarchicalMutex(const HierarchicalMutex&) = delete;
    HierarchicalMutex& operator=(const HierarchicalMutex&) = delete;

    // Standard mutex interface
    void lock();
    bool try_lock();
    void unlock();

    // Accessors
    LockLevel level() const { return m_level; }
    const char* name() const { return m_name; }

private:
    void checkHierarchy();
    void pushLock();
    void popLock();

    std::mutex  m_mutex;
    LockLevel   m_level;
    const char* m_name;
};

// ============================================================================
// LockHierarchyAuditor — global audit facility
// ============================================================================
class LockHierarchyAuditor {
public:
    static LockHierarchyAuditor& instance();

    // ---- Configuration ----
    PatchResult enable(bool active);
    bool isEnabled() const;

    // Set to true to abort on violation (debug builds); false to log only
    PatchResult setAbortOnViolation(bool abort);
    bool getAbortOnViolation() const;

    // ---- Violation Tracking ----
    void recordViolation(const LockViolation& v);
    std::vector<LockViolation> getViolations() const;
    size_t violationCount() const;
    void clearViolations();

    // ---- Lock Census ----
    // Returns the current set of held locks across all threads
    struct ThreadLockSnapshot {
        std::thread::id threadId;
        std::vector<LockLevel> heldLevels;
        std::vector<const char*> heldNames;
    };
    std::vector<ThreadLockSnapshot> currentLockHolders() const;

    // ---- Stats ----
    struct AuditorStats {
        std::atomic<uint64_t> totalAcquisitions{0};
        std::atomic<uint64_t> totalReleases{0};
        std::atomic<uint64_t> violations{0};
        std::atomic<uint64_t> maxConcurrentHeld{0};  // High watermark
    };
    AuditorStats getStats() const;
    void resetStats();

    // ---- Export ----
    // Dump the lock hierarchy as Graphviz DOT for documentation
    std::string exportHierarchyDot() const;
    // Export violations as JSON
    std::string exportViolationsJson() const;
    // Export a human-readable hierarchy table
    std::string exportHierarchyTable() const;

    // Thread-local state accessor (used by HierarchicalMutex)
    ThreadLockState& threadState();

private:
    LockHierarchyAuditor();
    ~LockHierarchyAuditor() = default;

    mutable std::mutex      m_mutex;
    bool                    m_enabled = true;
    bool                    m_abortOnViolation = false;
    std::vector<LockViolation> m_violations;
    AuditorStats            m_stats;
};

// ============================================================================
// Convenience RAII guard using HierarchicalMutex
// ============================================================================
using HierarchicalLockGuard = std::lock_guard<HierarchicalMutex>;

// ============================================================================
// Predefined Global Lock Declarations (extern — defined in .cpp)
// ============================================================================
extern HierarchicalMutex g_processLock;
extern HierarchicalMutex g_memoryPatchLock;
extern HierarchicalMutex g_bytePatchLock;
extern HierarchicalMutex g_serverPatchLock;
extern HierarchicalMutex g_proxyPatchLock;
extern HierarchicalMutex g_unifiedHotpatchLock;
extern HierarchicalMutex g_recoveryJournalLock;
extern HierarchicalMutex g_stateMachineLock;
extern HierarchicalMutex g_schedulerLock;
extern HierarchicalMutex g_threadPoolLock;
extern HierarchicalMutex g_kvCacheLock;
extern HierarchicalMutex g_modelLoaderLock;
extern HierarchicalMutex g_inferenceEngineLock;
extern HierarchicalMutex g_healthMonitorLock;
extern HierarchicalMutex g_telemetryLock;
extern HierarchicalMutex g_configLock;
extern HierarchicalMutex g_logLock;

} // namespace Locking
} // namespace RawrXD
