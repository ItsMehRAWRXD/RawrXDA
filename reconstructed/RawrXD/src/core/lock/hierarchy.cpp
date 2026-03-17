// ============================================================================
// lock_hierarchy.cpp — Strict Lock Hierarchy Enforcement
// ============================================================================
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "lock_hierarchy.hpp"
#include <cstring>
#include <sstream>
#include <chrono>
#include <cassert>

namespace RawrXD {
namespace Locking {

// ============================================================================
// Thread-local lock state
// ============================================================================
static thread_local ThreadLockState t_lockState;

// ============================================================================
// String table
// ============================================================================
static const char* s_lockLevelNames[] = {
    "Process", "MemoryPatch", "BytePatch", "ServerPatch",
    "ProxyPatch", "UnifiedHotpatch", "RecoveryJournal", "StateMachine",
    "Scheduler", "ThreadPool", "KVCache", "ModelLoader",
    "InferenceEngine", "HealthMonitor", "Telemetry", "Config", "Logging"
};

const char* lockLevelName(LockLevel level) {
    auto idx = static_cast<uint16_t>(level);
    if (idx < static_cast<uint16_t>(LockLevel::_COUNT))
        return s_lockLevelNames[idx];
    if (level == LockLevel::UNRANKED) return "UNRANKED";
    return "UNKNOWN";
}

// ============================================================================
// Global Locks — defined in hierarchy order
// ============================================================================
HierarchicalMutex g_processLock         (LockLevel::Process,         "g_processLock");
HierarchicalMutex g_memoryPatchLock     (LockLevel::MemoryPatch,     "g_memoryPatchLock");
HierarchicalMutex g_bytePatchLock       (LockLevel::BytePatch,       "g_bytePatchLock");
HierarchicalMutex g_serverPatchLock     (LockLevel::ServerPatch,     "g_serverPatchLock");
HierarchicalMutex g_proxyPatchLock      (LockLevel::ProxyPatch,      "g_proxyPatchLock");
HierarchicalMutex g_unifiedHotpatchLock (LockLevel::UnifiedHotpatch, "g_unifiedHotpatchLock");
HierarchicalMutex g_recoveryJournalLock (LockLevel::RecoveryJournal, "g_recoveryJournalLock");
HierarchicalMutex g_stateMachineLock    (LockLevel::StateMachine,    "g_stateMachineLock");
HierarchicalMutex g_schedulerLock       (LockLevel::Scheduler,       "g_schedulerLock");
HierarchicalMutex g_threadPoolLock      (LockLevel::ThreadPool,      "g_threadPoolLock");
HierarchicalMutex g_kvCacheLock         (LockLevel::KVCache,         "g_kvCacheLock");
HierarchicalMutex g_modelLoaderLock     (LockLevel::ModelLoader,     "g_modelLoaderLock");
HierarchicalMutex g_inferenceEngineLock (LockLevel::InferenceEngine, "g_inferenceEngineLock");
HierarchicalMutex g_healthMonitorLock   (LockLevel::HealthMonitor,   "g_healthMonitorLock");
HierarchicalMutex g_telemetryLock       (LockLevel::Telemetry,       "g_telemetryLock");
HierarchicalMutex g_configLock          (LockLevel::Config,          "g_configLock");
HierarchicalMutex g_logLock             (LockLevel::Logging,         "g_logLock");

// ============================================================================
// HierarchicalMutex
// ============================================================================
HierarchicalMutex::HierarchicalMutex(LockLevel level, const char* name)
    : m_level(level)
    , m_name(name ? name : lockLevelName(level))
{}

HierarchicalMutex::~HierarchicalMutex() = default;

void HierarchicalMutex::lock() {
    checkHierarchy();
    m_mutex.lock();
    pushLock();

    auto& auditor = LockHierarchyAuditor::instance();
    auditor.getStats().totalAcquisitions.fetch_add(1, std::memory_order_relaxed);
}

bool HierarchicalMutex::try_lock() {
    if (!m_mutex.try_lock()) return false;
    pushLock();
    return true;
}

void HierarchicalMutex::unlock() {
    popLock();
    m_mutex.unlock();

    auto& auditor = LockHierarchyAuditor::instance();
    auditor.getStats().totalReleases.fetch_add(1, std::memory_order_relaxed);
}

void HierarchicalMutex::checkHierarchy() {
    auto& auditor = LockHierarchyAuditor::instance();
    if (!auditor.isEnabled()) return;

    ThreadLockState& state = t_lockState;

    // Check: all currently held locks must have LOWER level number
    for (size_t i = 0; i < state.heldCount; ++i) {
        if (static_cast<uint16_t>(state.heldLocks[i]) >=
            static_cast<uint16_t>(m_level)) {
            // VIOLATION: trying to acquire a lock at same or lower level
            LockViolation v;
            v.threadId = std::this_thread::get_id();
            v.heldLevel = state.heldLocks[i];
            v.requestedLevel = m_level;
            v.heldLockName = state.heldNames[i];
            v.requestedLockName = m_name;

            auto now = std::chrono::system_clock::now();
            v.timestamp = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()).count());

            auditor.recordViolation(v);

            if (auditor.getAbortOnViolation()) {
                // In debug builds, trap immediately
                assert(false && "Lock hierarchy violation detected");
            }
            return;
        }
    }
}

void HierarchicalMutex::pushLock() {
    ThreadLockState& state = t_lockState;
    if (state.heldCount < ThreadLockState::MAX_HELD) {
        state.heldLocks[state.heldCount] = m_level;
        state.heldNames[state.heldCount] = m_name;
        state.heldCount++;

        if (static_cast<uint16_t>(m_level) >
            static_cast<uint16_t>(state.highestHeld) ||
            state.highestHeld == LockLevel::UNRANKED) {
            state.highestHeld = m_level;
        }

        // Track max concurrent
        auto& auditor = LockHierarchyAuditor::instance();
        uint64_t maxHeld = auditor.getStats().maxConcurrentHeld.load(
            std::memory_order_relaxed);
        if (state.heldCount > maxHeld) {
            auditor.getStats().maxConcurrentHeld.store(
                state.heldCount, std::memory_order_relaxed);
        }
    }
}

void HierarchicalMutex::popLock() {
    ThreadLockState& state = t_lockState;
    if (state.heldCount > 0) {
        // Find and remove this lock
        for (size_t i = 0; i < state.heldCount; ++i) {
            if (state.heldLocks[i] == m_level &&
                state.heldNames[i] == m_name) {
                // Shift remaining down
                for (size_t j = i; j + 1 < state.heldCount; ++j) {
                    state.heldLocks[j] = state.heldLocks[j + 1];
                    state.heldNames[j] = state.heldNames[j + 1];
                }
                state.heldCount--;
                break;
            }
        }

        // Recompute highest
        state.highestHeld = LockLevel::UNRANKED;
        for (size_t i = 0; i < state.heldCount; ++i) {
            if (state.highestHeld == LockLevel::UNRANKED ||
                static_cast<uint16_t>(state.heldLocks[i]) >
                    static_cast<uint16_t>(state.highestHeld)) {
                state.highestHeld = state.heldLocks[i];
            }
        }
    }
}

// ============================================================================
// LockHierarchyAuditor
// ============================================================================
LockHierarchyAuditor& LockHierarchyAuditor::instance() {
    static LockHierarchyAuditor s_instance;
    return s_instance;
}

LockHierarchyAuditor::LockHierarchyAuditor() = default;

PatchResult LockHierarchyAuditor::enable(bool active) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_enabled = active;
    return PatchResult::ok(active ? "Auditor enabled" : "Auditor disabled");
}

bool LockHierarchyAuditor::isEnabled() const {
    return m_enabled;
}

PatchResult LockHierarchyAuditor::setAbortOnViolation(bool abort) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_abortOnViolation = abort;
    return PatchResult::ok(abort ? "Will abort on violation" : "Log-only mode");
}

bool LockHierarchyAuditor::getAbortOnViolation() const {
    return m_abortOnViolation;
}

void LockHierarchyAuditor::recordViolation(const LockViolation& v) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_violations.push_back(v);
    m_stats.violations.fetch_add(1, std::memory_order_relaxed);
}

std::vector<LockViolation> LockHierarchyAuditor::getViolations() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_violations;
}

size_t LockHierarchyAuditor::violationCount() const {
    return m_stats.violations.load(std::memory_order_relaxed);
}

void LockHierarchyAuditor::clearViolations() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_violations.clear();
    m_stats.violations.store(0, std::memory_order_relaxed);
}

std::vector<LockHierarchyAuditor::ThreadLockSnapshot>
LockHierarchyAuditor::currentLockHolders() const {
    // Note: In production this would iterate a registered thread list.
    // For now, we can only report the calling thread's state.
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ThreadLockSnapshot> result;
    // Current thread snapshot
    ThreadLockSnapshot snap;
    snap.threadId = std::this_thread::get_id();
    for (size_t i = 0; i < t_lockState.heldCount; ++i) {
        snap.heldLevels.push_back(t_lockState.heldLocks[i]);
        snap.heldNames.push_back(t_lockState.heldNames[i]);
    }
    if (!snap.heldLevels.empty()) {
        result.push_back(snap);
    }
    return result;
}

LockHierarchyAuditor::AuditorStats LockHierarchyAuditor::getStats() const {
    // Return reference to internal — caller reads atomics
    return m_stats;
}

void LockHierarchyAuditor::resetStats() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.totalAcquisitions.store(0);
    m_stats.totalReleases.store(0);
    m_stats.violations.store(0);
    m_stats.maxConcurrentHeld.store(0);
}

ThreadLockState& LockHierarchyAuditor::threadState() {
    return t_lockState;
}

// ============================================================================
// Export
// ============================================================================
std::string LockHierarchyAuditor::exportHierarchyDot() const {
    std::ostringstream dot;
    dot << "digraph LockHierarchy {\n";
    dot << "  rankdir=TB;\n";
    dot << "  node [shape=box, style=rounded];\n\n";

    for (uint16_t i = 0; i < static_cast<uint16_t>(LockLevel::_COUNT); ++i) {
        auto level = static_cast<LockLevel>(i);
        dot << "  L" << i << " [label=\"Level " << i << "\\n"
            << lockLevelName(level) << "\"];\n";
    }
    dot << "\n";

    // Edges show ordering constraint
    for (uint16_t i = 0; i + 1 < static_cast<uint16_t>(LockLevel::_COUNT); ++i) {
        dot << "  L" << i << " -> L" << (i + 1)
            << " [label=\"must precede\", style=dashed];\n";
    }

    dot << "}\n";
    return dot.str();
}

std::string LockHierarchyAuditor::exportViolationsJson() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream json;
    json << "[\n";
    for (size_t i = 0; i < m_violations.size(); ++i) {
        const auto& v = m_violations[i];
        json << "  {\"heldLevel\":" << static_cast<int>(v.heldLevel)
             << ",\"heldName\":\"" << (v.heldLockName ? v.heldLockName : "") << "\""
             << ",\"requestedLevel\":" << static_cast<int>(v.requestedLevel)
             << ",\"requestedName\":\"" << (v.requestedLockName ? v.requestedLockName : "") << "\""
             << ",\"timestamp\":" << v.timestamp
             << "}";
        if (i + 1 < m_violations.size()) json << ",";
        json << "\n";
    }
    json << "]";
    return json.str();
}

std::string LockHierarchyAuditor::exportHierarchyTable() const {
    std::ostringstream table;
    table << "=== RawrXD Lock Hierarchy ===\n\n";
    table << "Level | Lock Name            | Subsystem\n";
    table << "------+----------------------+---------------------------\n";

    static const char* s_subsystems[] = {
        "Process-wide singleton init",
        "Memory Hotpatch (VirtualProtect)",
        "Byte-Level Hotpatch (mmap)",
        "Server Hotpatch (request/response)",
        "Proxy Hotpatcher (token bias/rewrite)",
        "Unified Hotpatch Manager (router)",
        "Recovery Journal (WAL writes)",
        "Inference State Machine (transitions)",
        "Deterministic Scheduler (queue)",
        "Thread Pool (submit/drain)",
        "KV Cache (read/write)",
        "Model Loader (streaming)",
        "Inference Engine (forward pass)",
        "Subsystem Health Monitor",
        "Telemetry/Metrics emission",
        "Configuration reads",
        "Logging subsystem (always last)",
    };

    for (uint16_t i = 0; i < static_cast<uint16_t>(LockLevel::_COUNT); ++i) {
        char line[128];
        snprintf(line, sizeof(line), " %2u   | %-20s | %s\n",
                 i, lockLevelName(static_cast<LockLevel>(i)),
                 s_subsystems[i]);
        table << line;
    }

    table << "\nRULES:\n";
    table << "  - Always acquire lower-numbered locks first\n";
    table << "  - Never hold more than 4 locks simultaneously\n";
    table << "  - Never use recursive mutexes\n";
    table << "  - All locks use std::lock_guard (RAII)\n";
    table << "  - Callbacks must not acquire locks\n";

    return table.str();
}

} // namespace Locking
} // namespace RawrXD
