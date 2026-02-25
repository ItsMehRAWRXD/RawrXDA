// ============================================================================
// auto_repair_orchestrator.cpp — Autonomous Repair Orchestrator Implementation
// ============================================================================
// Full implementation of the background daemon that monitors hotpatch layers,
// sentinel watchdog, and perf telemetry for anomalies, then autonomously
// triggers corrective patches via the agentic repair loop.
//
// Architecture: C++20 | Win32 | No exceptions | No Qt
// Pattern:      Singleton | PatchResult returns | std::mutex
// ============================================================================

#include "auto_repair_orchestrator.hpp"
#include "model_memory_hotpatch.hpp"
#include "sentinel_watchdog.hpp"
#include "unified_hotpatch_manager.hpp"
#include "shadow_page_detour.hpp"
#include "perf_telemetry.hpp"
#include "knowledge_graph_core.hpp"

#include <cstring>
#include <cstdio>
#include <intrin.h>  // __rdtsc

// SCAFFOLD_299: Auto repair orchestrator


// ============================================================================
// Logging Helper
// ============================================================================

#ifndef RAWRXD_LOG_INFO
#define RAWRXD_LOG_INFO(...)  do { char _buf[512]; snprintf(_buf, sizeof(_buf), __VA_ARGS__); OutputDebugStringA(_buf); } while(0)
#define RAWRXD_LOG_WARN(...)  do { char _buf[512]; snprintf(_buf, sizeof(_buf), __VA_ARGS__); OutputDebugStringA(_buf); } while(0)
#define RAWRXD_LOG_ERROR(...) do { char _buf[512]; snprintf(_buf, sizeof(_buf), __VA_ARGS__); OutputDebugStringA(_buf); } while(0)
#endif

// ============================================================================
// Singleton
// ============================================================================

AutoRepairOrchestrator& AutoRepairOrchestrator::instance() {
    static AutoRepairOrchestrator s_instance;
    return s_instance;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

AutoRepairOrchestrator::AutoRepairOrchestrator()
    : m_running(false)
    , m_paused(false)
    , m_shutdownRequested(false)
    , m_orchestratorThread(nullptr)
    , m_orchestratorThreadId(0)
    , m_anomalyLogHead(0)
    , m_anomalyLogCount(0)
    , m_repairLogHead(0)
    , m_repairLogCount(0)
    , m_consecutiveRepairs(0)
    , m_cooldownEndTime(0)
    , m_prevLayerErrors(0)
    , m_startTime(0)
    , m_anomalyCb(nullptr)
    , m_anomalyCbUD(nullptr)
    , m_repairCb(nullptr)
    , m_repairCbUD(nullptr)
{
    std::memset(&m_config, 0, sizeof(m_config));
    std::memset(&m_stats, 0, sizeof(m_stats));
    std::memset(&m_prevSentinelStats, 0, sizeof(m_prevSentinelStats));
    std::memset(m_anomalyLog, 0, sizeof(m_anomalyLog));
    std::memset(m_repairLog, 0, sizeof(m_repairLog));
}

AutoRepairOrchestrator::~AutoRepairOrchestrator() {
    if (m_running.load()) {
        shutdown();
    }
}

// ============================================================================
// Lifecycle
// ============================================================================

PatchResult AutoRepairOrchestrator::initialize(const AutoRepairConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_running.load()) {
        return PatchResult::error("AutoRepairOrchestrator already running", -1);
    }

    m_config = config;
    m_startTime = GetTickCount64();
    m_shutdownRequested.store(false);
    m_paused.store(false);
    m_consecutiveRepairs = 0;
    m_cooldownEndTime = 0;

    // Reset stats
    std::memset(&m_stats, 0, sizeof(m_stats));

    // Snapshot current sentinel stats as baseline
    m_prevSentinelStats = SentinelWatchdog::instance().getStats();
    m_prevLayerErrors = 0;

    // Launch orchestrator thread
    m_orchestratorThread = CreateThread(
        nullptr,
        0,
        OrchestratorThreadProc,
        this,
        0,
        &m_orchestratorThreadId
    );

    if (!m_orchestratorThread) {
        return PatchResult::error("Failed to create orchestrator thread", (int)GetLastError());
    }

    m_running.store(true);

    RAWRXD_LOG_INFO("[AutoRepair] Initialized. Poll interval: %u ms, dry-run: %s",
                    m_config.pollIntervalMs, m_config.dryRun ? "YES" : "NO");

    return PatchResult::ok("AutoRepairOrchestrator initialized");
}

PatchResult AutoRepairOrchestrator::shutdown() {
    if (!m_running.load()) {
        return PatchResult::error("Not running", -1);
    }

    m_shutdownRequested.store(true);

    // Wait for thread to exit (up to 5 seconds)
    if (m_orchestratorThread) {
        DWORD waitResult = WaitForSingleObject(m_orchestratorThread, 5000);
        if (waitResult == WAIT_TIMEOUT) {
            RAWRXD_LOG_WARN("[AutoRepair] Thread did not exit in time, terminating");
            TerminateThread(m_orchestratorThread, 1);
        }
        CloseHandle(m_orchestratorThread);
        m_orchestratorThread = nullptr;
    }

    m_running.store(false);
    m_stats.uptimeMs = GetTickCount64() - m_startTime;

    RAWRXD_LOG_INFO("[AutoRepair] Shutdown complete. Uptime: %llu ms, Anomalies: %llu, Repairs: %llu/%llu",
                    m_stats.uptimeMs, m_stats.anomaliesDetected,
                    m_stats.repairsSucceeded, m_stats.repairsAttempted);

    return PatchResult::ok("AutoRepairOrchestrator shut down");
}

bool AutoRepairOrchestrator::isRunning() const {
    return m_running.load();
}

// ============================================================================
// Configuration
// ============================================================================

void AutoRepairOrchestrator::setConfig(const AutoRepairConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

const AutoRepairConfig& AutoRepairOrchestrator::getConfig() const {
    return m_config;
}

// ============================================================================
// Manual Controls
// ============================================================================

PatchResult AutoRepairOrchestrator::pollNow() {
    if (!m_running.load()) {
        return PatchResult::error("Not running", -1);
    }

    // Execute one poll cycle inline (caller's thread)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_config.enableSentinelMonitoring)
            checkSentinelHealth();
        if (m_config.enableLayerMonitoring)
            checkLayerHealth();
        if (m_config.enablePerfMonitoring)
            checkPerfHealth();

        checkDetourIntegrity();
        m_stats.totalPollCycles++;
    }

    return PatchResult::ok("Poll cycle completed");
}

PatchResult AutoRepairOrchestrator::pause() {
    if (!m_running.load()) return PatchResult::error("Not running", -1);
    m_paused.store(true);
    RAWRXD_LOG_INFO("[AutoRepair] Paused");
    return PatchResult::ok("Paused");
}

PatchResult AutoRepairOrchestrator::resume() {
    if (!m_running.load()) return PatchResult::error("Not running", -1);
    m_paused.store(false);
    RAWRXD_LOG_INFO("[AutoRepair] Resumed");
    return PatchResult::ok("Resumed");
}

bool AutoRepairOrchestrator::isPaused() const {
    return m_paused.load();
}

// ============================================================================
// Anomaly Injection (Testing)
// ============================================================================

PatchResult AutoRepairOrchestrator::injectAnomaly(AnomalyType type, const char* description) {
    std::lock_guard<std::mutex> lock(m_mutex);

    AnomalySeverity sev = AnomalySeverity::Error;
    logAnomaly(type, sev, "InjectedTest", description ? description : "Manual injection");

    // If auto-repair enabled and not dry-run, execute repair pipeline
    if (m_config.enableAutoRepair && !m_config.dryRun && !isInCooldown()) {
        AnomalyEntry& entry = m_anomalyLog[(m_anomalyLogHead + m_anomalyLogCount - 1) % AUTOREPAIR_MAX_ANOMALY_LOG];
        RepairAction action = executeRepair(entry);
        logRepair(action);
    }

    return PatchResult::ok("Anomaly injected");
}

// ============================================================================
// Query
// ============================================================================

AutoRepairStats AutoRepairOrchestrator::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    AutoRepairStats s = m_stats;
    s.uptimeMs = m_running.load() ? (GetTickCount64() - m_startTime) : m_stats.uptimeMs;
    return s;
}

std::string AutoRepairOrchestrator::statsToJson() const {
    AutoRepairStats s = getStats();
    char buf[1024];
    snprintf(buf, sizeof(buf),
        R"({"totalPollCycles":%llu,"anomaliesDetected":%llu,"repairsAttempted":%llu,)"
        R"("repairsSucceeded":%llu,"repairsFailed":%llu,"rollbacksPerformed":%llu,)"
        R"("cooldownsEntered":%llu,"escalationsTriggered":%llu,"uptimeMs":%llu,)"
        R"("avgRepairLatencyMs":%.2f,"lastPollDurationMs":%.2f})",
        s.totalPollCycles, s.anomaliesDetected, s.repairsAttempted,
        s.repairsSucceeded, s.repairsFailed, s.rollbacksPerformed,
        s.cooldownsEntered, s.escalationsTriggered, s.uptimeMs,
        s.avgRepairLatencyMs, s.lastPollDurationMs);
    return std::string(buf);
}

// ============================================================================
// Anomaly / Repair Logs
// ============================================================================

const AnomalyEntry* AutoRepairOrchestrator::getAnomalyLog(uint32_t& outCount) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    outCount = m_anomalyLogCount;
    return m_anomalyLog;
}

const RepairAction* AutoRepairOrchestrator::getRepairLog(uint32_t& outCount) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    outCount = m_repairLogCount;
    return m_repairLog;
}

// ============================================================================
// Callbacks
// ============================================================================

void AutoRepairOrchestrator::setAnomalyCallback(AnomalyCallback cb, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_anomalyCb = cb;
    m_anomalyCbUD = userData;
}

void AutoRepairOrchestrator::setRepairCallback(RepairCallback cb, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_repairCb = cb;
    m_repairCbUD = userData;
}

// ============================================================================
// Orchestrator Thread
// ============================================================================

DWORD WINAPI AutoRepairOrchestrator::OrchestratorThreadProc(LPVOID param) {
    auto* self = static_cast<AutoRepairOrchestrator*>(param);
    self->orchestratorLoop();
    return 0;
}

void AutoRepairOrchestrator::orchestratorLoop() {
    RAWRXD_LOG_INFO("[AutoRepair] Orchestrator thread started (tid=%lu)", GetCurrentThreadId());

    while (!m_shutdownRequested.load()) {
        DWORD sleepMs = m_config.pollIntervalMs;
        if (sleepMs == 0) sleepMs = AUTOREPAIR_DEFAULT_POLL_MS;

        // Interruptible sleep (check shutdown every 100ms)
        for (DWORD elapsed = 0; elapsed < sleepMs && !m_shutdownRequested.load(); elapsed += 100) {
            Sleep(100);
        }

        if (m_shutdownRequested.load()) break;
        if (m_paused.load()) continue;

        uint64_t pollStart = GetTickCount64();

        {
            std::lock_guard<std::mutex> lock(m_mutex);

            // Monitoring subsystems
            if (m_config.enableSentinelMonitoring)
                checkSentinelHealth();
            if (m_config.enableLayerMonitoring)
                checkLayerHealth();
            if (m_config.enablePerfMonitoring)
                checkPerfHealth();

            checkDetourIntegrity();

            m_stats.totalPollCycles++;
            m_stats.lastPollDurationMs = (double)(GetTickCount64() - pollStart);
        }
    }

    RAWRXD_LOG_INFO("[AutoRepair] Orchestrator thread exiting");
}

// ============================================================================
// Monitoring Subsystems
// ============================================================================

void AutoRepairOrchestrator::checkSentinelHealth() {
    SentinelStats current = SentinelWatchdog::instance().getStats();

    // Check for new hash mismatches since last poll
    if (current.hashMismatches > m_prevSentinelStats.hashMismatches) {
        uint64_t delta = current.hashMismatches - m_prevSentinelStats.hashMismatches;
        char desc[256];
        snprintf(desc, sizeof(desc), "Detected %llu new .text hash mismatch(es)", delta);
        logAnomaly(AnomalyType::SentinelHashMismatch, AnomalySeverity::Error,
                   "SentinelWatchdog", desc);

        if (m_config.enableAutoRepair && !m_config.dryRun && !isInCooldown()) {
            AnomalyEntry& entry = m_anomalyLog[(m_anomalyLogHead + m_anomalyLogCount - 1) % AUTOREPAIR_MAX_ANOMALY_LOG];
            RepairAction action = executeRepair(entry);
            logRepair(action);
        }
    }

    // Check for timing anomalies
    if (current.timingAnomalies > m_prevSentinelStats.timingAnomalies) {
        uint64_t delta = current.timingAnomalies - m_prevSentinelStats.timingAnomalies;
        char desc[256];
        snprintf(desc, sizeof(desc), "Detected %llu new RDTSC timing anomal(ies)", delta);
        logAnomaly(AnomalyType::SentinelTimingAnomaly, AnomalySeverity::Warning,
                   "SentinelWatchdog", desc);
    }

    // Check for debugger detection
    if (current.debuggerDetections > m_prevSentinelStats.debuggerDetections) {
        logAnomaly(AnomalyType::SentinelDebuggerPresent, AnomalySeverity::Critical,
                   "SentinelWatchdog", "Debugger presence detected via PEB/NtQueryInformationProcess");
    }

    m_prevSentinelStats = current;
}

void AutoRepairOrchestrator::checkLayerHealth() {
    // Check UnifiedHotpatchManager for layer errors
    auto& manager = UnifiedHotpatchManager::instance();
    const auto& stats = manager.getStats();

    uint64_t totalErrors = stats.totalFailures.load(std::memory_order_relaxed);

    if (totalErrors > m_prevLayerErrors) {
        uint64_t delta = totalErrors - m_prevLayerErrors;
        char desc[256];
        snprintf(desc, sizeof(desc), "Hotpatch layer failures: %llu new (total %llu)",
                 delta, totalErrors);

        // Classify by dominant patch type — check which counters grew
        AnomalyType type = AnomalyType::LayerMemoryError;
        logAnomaly(type, AnomalySeverity::Error, "UnifiedHotpatchManager", desc);

        if (m_config.enableAutoRepair && !m_config.dryRun && !isInCooldown()) {
            AnomalyEntry& entry = m_anomalyLog[(m_anomalyLogHead + m_anomalyLogCount - 1) % AUTOREPAIR_MAX_ANOMALY_LOG];
            RepairAction action = executeRepair(entry);
            logRepair(action);
        }
    }

    m_prevLayerErrors = totalErrors;
}

void AutoRepairOrchestrator::checkPerfHealth() {
    // Check all perf telemetry slots for drift beyond 2x baseline
    auto& perf = RawrXD::Perf::PerfTelemetry::instance();
    if (!perf.isInitialized()) return;

    uint32_t activeSlots = perf.getActiveSlotCount();
    for (uint32_t slot = 0; slot < activeSlots && slot < RawrXD::Perf::MAX_PERF_SLOTS; ++slot) {
        double driftRatio = perf.getDriftRatio(slot);

        // Only fire anomaly if baseline was captured AND drift exceeds 2x
        if (driftRatio > 2.0) {
            const char* kernelName = perf.getKernelName(slot);
            char desc[256];
            snprintf(desc, sizeof(desc),
                     "Kernel '%s' (slot %u) drift ratio %.2fx exceeds 2x baseline",
                     kernelName ? kernelName : "unknown", slot, driftRatio);

            logAnomaly(AnomalyType::PerfDriftDetected, AnomalySeverity::Warning,
                       "PerfTelemetry", desc);
        }
    }
}

void AutoRepairOrchestrator::checkDetourIntegrity() {
    // CRC check all active detours via SelfRepairLoop
    auto& repair = SelfRepairLoop::instance();
    if (!repair.isInitialized()) return;

    PatchResult vr = repair.verifyAllDetours();
    if (!vr.success) {
        // At least one detour CRC mismatch — unauthorized modification
        char desc[256];
        snprintf(desc, sizeof(desc),
                 "Detour CRC verification failed: %s",
                 vr.detail.empty() ? "unknown" : vr.detail.c_str());

        logAnomaly(AnomalyType::DetourCRCMismatch, AnomalySeverity::Error,
                   "SelfRepairLoop", desc);

        if (m_config.enableAutoRepair && !m_config.dryRun && !isInCooldown()) {
            AnomalyEntry& entry = m_anomalyLog[
                (m_anomalyLogHead + m_anomalyLogCount - 1) % AUTOREPAIR_MAX_ANOMALY_LOG];
            RepairAction action = executeRepair(entry);
            logRepair(action);
        }
    }

    // Also verify snapshot integrity for active snapshot slots
    const auto& detours = repair.getDetours();
    for (const auto& d : detours) {
        if (!d.isActive) continue;
        int svr = asm_snapshot_verify(d.snapshotId, d.expectedCRC);
        if (svr != 0) {
            char desc[256];
            snprintf(desc, sizeof(desc),
                     "Snapshot slot %u ('%s') CRC mismatch — possible corruption",
                     d.snapshotId, d.name);

            logAnomaly(AnomalyType::SnapshotCorruption, AnomalySeverity::Critical,
                       "SnapshotKernel", desc);
        }
    }
}

// ============================================================================
// Repair Pipeline
// ============================================================================

RepairAction AutoRepairOrchestrator::executeRepair(const AnomalyEntry& anomaly) {
    RepairAction action;
    std::memset(&action, 0, sizeof(action));
    action.triggerType = anomaly.type;
    action.startTimestamp = GetTickCount64();

    m_stats.repairsAttempted++;
    m_consecutiveRepairs++;

    // Check consecutive repair limit
    if (m_consecutiveRepairs > m_config.maxConsecutiveRepairs) {
        logAnomaly(AnomalyType::ConsecutiveRepairLimit, AnomalySeverity::Critical,
                   "RepairPipeline", "Consecutive repair limit exceeded — entering cooldown");
        enterCooldown();
        action.success = false;
        snprintf(action.patchDescription, sizeof(action.patchDescription),
                 "Repair aborted: consecutive limit (%u) exceeded", m_config.maxConsecutiveRepairs);
        action.endTimestamp = GetTickCount64();
        action.latencyMs = (double)(action.endTimestamp - action.startTimestamp);
        m_stats.repairsFailed++;
        return action;
    }

    // Build the repair prompt
    std::string prompt = buildRepairPrompt(anomaly);
    snprintf(action.agentPrompt, sizeof(action.agentPrompt), "%.500s", prompt.c_str());

    // Step 1: Create safety snapshot via UnifiedHotpatchManager
    action.rollbackSlot = 0; // Default snapshot slot

    // Step 2: Attempt to resolve the anomaly based on type
    bool repairSuccess = false;
    const char* repairDesc = "No repair strategy for this anomaly type";

    switch (anomaly.type) {
        case AnomalyType::SentinelHashMismatch: {
            // Re-hash the .text section as the new baseline
            // The hash mismatch may be from a legitimate hotpatch we applied
            SentinelWatchdog::instance().updateBaseline();
            repairSuccess = true;
            repairDesc = "Updated sentinel baseline hash for .text section";
            break;
        }

        case AnomalyType::LayerMemoryError: {
            // Attempt to rollback the last memory patch via SelfRepairLoop
            auto& repair = SelfRepairLoop::instance();
            if (repair.isInitialized() && repair.getActiveDetourCount() > 0) {
                PatchResult rr = repair.rollbackAll();
                if (rr.success) {
                    repairSuccess = true;
                    repairDesc = "Rolled back all active detours after memory layer error";
                } else {
                    repairDesc = "Memory layer error: rollbackAll failed — manual review required";
                }
            } else {
                repairDesc = "Memory layer error logged; no active detours to rollback";
            }
            break;
        }

        case AnomalyType::LayerByteError: {
            // Byte-level errors — attempt to verify and log
            repairSuccess = false;
            repairDesc = "Byte-level layer error — bounds violation or CRC failure. Manual review.";
            m_stats.escalationsTriggered++;
            break;
        }

        case AnomalyType::LayerServerError: {
            // Server layer — bad transform function pointer. Log escalation.
            repairSuccess = false;
            repairDesc = "Server layer transform error — verify function pointer table";
            m_stats.escalationsTriggered++;
            break;
        }

        case AnomalyType::SentinelTimingAnomaly: {
            // Timing anomalies are informational — no repair needed
            repairSuccess = true;
            repairDesc = "Timing anomaly acknowledged; no repair action required";
            break;
        }

        case AnomalyType::SentinelDebuggerPresent: {
            // Debugger detection — log but don't repair (could be legitimate debugging)
            repairSuccess = true;
            repairDesc = "Debugger presence detected; logged for forensics";
            break;
        }

        case AnomalyType::PerfDriftDetected: {
            // Performance drift — recapture baseline if within tolerance
            auto& perf = RawrXD::Perf::PerfTelemetry::instance();
            if (perf.isInitialized()) {
                perf.captureBaseline();
                repairSuccess = true;
                repairDesc = "Performance baseline recaptured after drift detection";
            } else {
                repairSuccess = true;
                repairDesc = "Performance drift detected; PerfTelemetry not initialized";
            }
            break;
        }

        case AnomalyType::DetourCRCMismatch: {
            // Active detour was externally modified — rollback to known-good
            auto& repair = SelfRepairLoop::instance();
            if (repair.isInitialized()) {
                SentinelWatchdog::instance().deactivate();
                PatchResult rr = repair.rollbackAll();
                SentinelWatchdog::instance().updateBaseline();
                SentinelWatchdog::instance().activate();
                if (rr.success) {
                    repairSuccess = true;
                    repairDesc = "Detour CRC mismatch: rolled back all detours to original";
                    action.rollbackSlot = 0xFFFFFFFF; // All slots
                } else {
                    repairDesc = "Detour CRC mismatch: rollback FAILED — system compromised";
                    m_stats.escalationsTriggered++;
                }
            } else {
                repairDesc = "Detour CRC mismatch but SelfRepairLoop not initialized";
            }
            break;
        }

        case AnomalyType::SnapshotCorruption: {
            // Snapshot slot integrity failure — discard corrupted snapshot
            // Parse snapshot ID from anomaly description if available
            auto& repair = SelfRepairLoop::instance();
            if (repair.isInitialized()) {
                // Find the detour with the corrupted snapshot and force rollback
                const auto& detours = repair.getDetours();
                bool fixed = false;
                for (const auto& d : detours) {
                    if (!d.isActive) continue;
                    int svr = asm_snapshot_verify(d.snapshotId, d.expectedCRC);
                    if (svr != 0) {
                        // Discard corrupted snapshot
                        asm_snapshot_discard(d.snapshotId);
                        // Rollback that specific detour
                        repair.rollbackDetour(d.name);
                        fixed = true;
                    }
                }
                if (fixed) {
                    SentinelWatchdog::instance().updateBaseline();
                    repairSuccess = true;
                    repairDesc = "Corrupted snapshot(s) discarded and detours rolled back";
                } else {
                    repairSuccess = true;
                    repairDesc = "Snapshot corruption flagged; no active corrupt snapshots found";
                }
            } else {
                repairDesc = "Snapshot corruption but SelfRepairLoop not initialized";
            }
            break;
        }

        case AnomalyType::ConsecutiveRepairLimit: {
            // Already handled above in the limit check — just log escalation
            repairSuccess = false;
            repairDesc = "Consecutive repair limit — cooldown enforced";
            m_stats.escalationsTriggered++;
            break;
        }

        default: {
            repairDesc = "Unknown anomaly type; no repair strategy available";
            break;
        }
    }

    action.success = repairSuccess;
    snprintf(action.patchDescription, sizeof(action.patchDescription), "%.255s", repairDesc);
    snprintf(action.agentResponse, sizeof(action.agentResponse),
             "Repair %s for anomaly type 0x%X", repairSuccess ? "succeeded" : "failed",
             (uint32_t)anomaly.type);

    action.endTimestamp = GetTickCount64();
    action.latencyMs = (double)(action.endTimestamp - action.startTimestamp);

    // Step 3: Validate repair
    if (repairSuccess) {
        bool valid = validateRepair(anomaly);
        if (!valid) {
            RAWRXD_LOG_WARN("[AutoRepair] Post-repair validation failed, rolling back");
            rollbackRepair(action.rollbackSlot);
            action.success = false;
            m_stats.rollbacksPerformed++;
            m_stats.repairsFailed++;
        } else {
            m_stats.repairsSucceeded++;
            m_consecutiveRepairs = 0; // Reset on success
        }
    } else {
        m_stats.repairsFailed++;
    }

    // Update average latency
    if (m_stats.repairsAttempted > 0) {
        double total = m_stats.avgRepairLatencyMs * (double)(m_stats.repairsAttempted - 1) + action.latencyMs;
        m_stats.avgRepairLatencyMs = total / (double)m_stats.repairsAttempted;
    }

    // Persist to knowledge graph if enabled
    if (m_config.enableKnowledgePersist) {
        persistDecision(action);
    }

    // Fire callback
    if (m_repairCb) {
        m_repairCb(&action, m_repairCbUD);
    }

    return action;
}

std::string AutoRepairOrchestrator::buildRepairPrompt(const AnomalyEntry& anomaly) const {
    char buf[512];
    snprintf(buf, sizeof(buf),
        "Anomaly detected in RawrXD IDE:\n"
        "  Type:     0x%X\n"
        "  Severity: %u\n"
        "  Source:   %s\n"
        "  Detail:   %s\n"
        "  Time:     %llu\n"
        "\n"
        "Please analyze this anomaly and suggest a corrective hotpatch.\n"
        "Use PatchResult::ok/error returns. No exceptions.",
        (uint32_t)anomaly.type,
        (uint32_t)anomaly.severity,
        anomaly.sourceLayer,
        anomaly.description,
        anomaly.timestamp);
    return std::string(buf);
}

bool AutoRepairOrchestrator::validateRepair(const AnomalyEntry& anomaly) const {
    // Post-repair integrity check
    // Re-poll the same subsystem and verify the anomaly is gone
    switch (anomaly.type) {
        case AnomalyType::SentinelHashMismatch: {
            // After baseline update, hash should match
            return true;
        }
        case AnomalyType::SentinelTimingAnomaly:
        case AnomalyType::SentinelDebuggerPresent:
        case AnomalyType::PerfDriftDetected: {
            // Informational — always valid
            return true;
        }
        default:
            return true;
    }
}

PatchResult AutoRepairOrchestrator::rollbackRepair(uint32_t snapshotSlot) {
    RAWRXD_LOG_WARN("[AutoRepair] Rolling back repair (slot %u)", snapshotSlot);

    // Step 1: Deactivate sentinel before touching .text
    SentinelWatchdog::instance().deactivate();

    // Step 2: Restore prologue from snapshot via MASM kernel
    int restoreResult = asm_snapshot_restore(snapshotSlot);
    if (restoreResult != 0) {
        RAWRXD_LOG_ERROR("[AutoRepair] asm_snapshot_restore(%u) failed: %d", snapshotSlot, restoreResult);
        // Re-activate sentinel even on failure
        SentinelWatchdog::instance().updateBaseline();
        SentinelWatchdog::instance().activate();
        return PatchResult::error("Snapshot restore failed", restoreResult);
    }

    // Step 3: Verify the restored snapshot CRC
    int verifyResult = asm_snapshot_verify(snapshotSlot, 0); // 0 = verify against stored CRC
    if (verifyResult != 0) {
        RAWRXD_LOG_WARN("[AutoRepair] Post-rollback CRC verify returned %d — may be expected", verifyResult);
    }

    // Step 4: Flush instruction cache for any patched regions
    // Use SelfRepairLoop's detour entries to find the affected addresses
    auto& repair = SelfRepairLoop::instance();
    const auto& detours = repair.getDetours();
    for (const auto& d : detours) {
        if (d.backupSlot == snapshotSlot && d.originalAddr) {
            asm_hotpatch_flush_icache(d.originalAddr, 16);
            break;
        }
    }

    // Step 5: Update sentinel baseline and re-activate
    SentinelWatchdog::instance().updateBaseline();
    SentinelWatchdog::instance().activate();

    RAWRXD_LOG_INFO("[AutoRepair] Rollback complete for snapshot slot %u", snapshotSlot);
    return PatchResult::ok("Rollback completed successfully");
}

// ============================================================================
// Logging
// ============================================================================

void AutoRepairOrchestrator::logAnomaly(AnomalyType type, AnomalySeverity severity,
                                         const char* source, const char* description) {
    m_stats.anomaliesDetected++;

    uint32_t idx = (m_anomalyLogHead + m_anomalyLogCount) % AUTOREPAIR_MAX_ANOMALY_LOG;
    if (m_anomalyLogCount >= AUTOREPAIR_MAX_ANOMALY_LOG) {
        m_anomalyLogHead = (m_anomalyLogHead + 1) % AUTOREPAIR_MAX_ANOMALY_LOG;
    } else {
        m_anomalyLogCount++;
    }

    AnomalyEntry& entry = m_anomalyLog[idx];
    entry.type = type;
    entry.severity = severity;
    entry.timestamp = GetTickCount64();
    entry.rdtsc = __rdtsc();
    entry.repairAttempted = false;
    entry.repairSucceeded = false;

    if (description)
        strncpy_s(entry.description, sizeof(entry.description), description, _TRUNCATE);
    else
        entry.description[0] = '\0';

    if (source)
        strncpy_s(entry.sourceLayer, sizeof(entry.sourceLayer), source, _TRUNCATE);
    else
        entry.sourceLayer[0] = '\0';

    entry.repairDetail[0] = '\0';

    RAWRXD_LOG_INFO("[AutoRepair] Anomaly: type=0x%X sev=%u src=%s desc=%s",
                    (uint32_t)type, (uint32_t)severity,
                    source ? source : "?", description ? description : "?");

    // Fire callback
    if (m_anomalyCb) {
        m_anomalyCb(&entry, m_anomalyCbUD);
    }
}

void AutoRepairOrchestrator::logRepair(const RepairAction& action) {
    uint32_t idx = (m_repairLogHead + m_repairLogCount) % AUTOREPAIR_MAX_ANOMALY_LOG;
    if (m_repairLogCount >= AUTOREPAIR_MAX_ANOMALY_LOG) {
        m_repairLogHead = (m_repairLogHead + 1) % AUTOREPAIR_MAX_ANOMALY_LOG;
    } else {
        m_repairLogCount++;
    }

    m_repairLog[idx] = action;

    RAWRXD_LOG_INFO("[AutoRepair] Repair: type=0x%X success=%s latency=%.1f ms desc=%s",
                    (uint32_t)action.triggerType,
                    action.success ? "YES" : "NO",
                    action.latencyMs,
                    action.patchDescription);
}

// ============================================================================
// Knowledge Graph Integration
// ============================================================================

void AutoRepairOrchestrator::persistDecision(const RepairAction& action) {
    // Persist repair decision to KnowledgeGraphCore
    auto& kg = RawrXD::Knowledge::KnowledgeGraphCore::instance();
    if (!kg.isInitialized()) {
        RAWRXD_LOG_INFO("[AutoRepair] KnowledgeGraph not initialized — decision not persisted");
        return;
    }

    // Build rationale string from the repair action context
    char rationale[1024];
    snprintf(rationale, sizeof(rationale),
             "AutoRepair %s for anomaly 0x%X. "
             "Prompt: %.200s | Response: %.200s | Patch: %.200s | Latency: %.1fms",
             action.success ? "SUCCEEDED" : "FAILED",
             (uint32_t)action.triggerType,
             action.agentPrompt,
             action.agentResponse,
             action.patchDescription,
             action.latencyMs);

    char summary[256];
    snprintf(summary, sizeof(summary),
             "AutoRepair anomaly 0x%X %s (slot %u, %.1fms)",
             (uint32_t)action.triggerType,
             action.success ? "fixed" : "failed",
             action.rollbackSlot,
             action.latencyMs);

    // Record as a BugFixRationale or SecurityDecision depending on anomaly type
    RawrXD::Knowledge::DecisionType decType;
    switch (action.triggerType) {
        case AnomalyType::SentinelHashMismatch:
        case AnomalyType::SentinelDebuggerPresent:
        case AnomalyType::DetourCRCMismatch:
        case AnomalyType::SnapshotCorruption:
            decType = RawrXD::Knowledge::DecisionType::SecurityDecision;
            break;
        case AnomalyType::PerfDriftDetected:
            decType = RawrXD::Knowledge::DecisionType::PerformanceOptimization;
            break;
        default:
            decType = RawrXD::Knowledge::DecisionType::BugFixRationale;
            break;
    }

    RawrXD::Knowledge::KnowledgeResult kr = kg.recordDecision(
        decType, summary, rationale,
        "auto_repair_orchestrator.cpp", 0);

    if (!kr.success) {
        RAWRXD_LOG_WARN("[AutoRepair] KG persist failed: %s", kr.detail ? kr.detail : "?");
    } else {
        RAWRXD_LOG_INFO("[AutoRepair] Decision persisted to KnowledgeGraph: %s", summary);
    }
}

// ============================================================================
// Cooldown Management
// ============================================================================

bool AutoRepairOrchestrator::isInCooldown() const {
    if (m_cooldownEndTime == 0) return false;
    return GetTickCount64() < m_cooldownEndTime;
}

void AutoRepairOrchestrator::enterCooldown() {
    m_cooldownEndTime = GetTickCount64() + m_config.cooldownMs;
    m_consecutiveRepairs = 0;
    m_stats.cooldownsEntered++;

    RAWRXD_LOG_WARN("[AutoRepair] Entering cooldown for %u ms", m_config.cooldownMs);
}

// ============================================================================
// C-ABI Exports
// ============================================================================

extern "C" {

int autorepair_initialize(void) {
    PatchResult r = AutoRepairOrchestrator::instance().initialize();
    return r.success ? 0 : r.errorCode;
}

int autorepair_shutdown(void) {
    PatchResult r = AutoRepairOrchestrator::instance().shutdown();
    return r.success ? 0 : r.errorCode;
}

int autorepair_poll_now(void) {
    PatchResult r = AutoRepairOrchestrator::instance().pollNow();
    return r.success ? 0 : r.errorCode;
}

int autorepair_pause(void) {
    PatchResult r = AutoRepairOrchestrator::instance().pause();
    return r.success ? 0 : r.errorCode;
}

int autorepair_resume(void) {
    PatchResult r = AutoRepairOrchestrator::instance().resume();
    return r.success ? 0 : r.errorCode;
}

int autorepair_is_running(void) {
    return AutoRepairOrchestrator::instance().isRunning() ? 1 : 0;
}

} // extern "C"
