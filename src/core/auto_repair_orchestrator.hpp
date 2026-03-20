// ============================================================================
// auto_repair_orchestrator.hpp — Autonomous Repair Orchestrator
// ============================================================================
//
// PURPOSE:
//   Background daemon thread that continuously monitors all hotpatch layers
//   and the Sentinel Watchdog for anomalies, then autonomously triggers
//   the AgentOrchestrator to generate corrective patches. Bridges the gap
//   between passive integrity monitoring (SentinelWatchdog) and the active
//   agentic coding loop (AgentOrchestrator).
//
// DESIGN:
//   - Configurable poll interval (default 2000ms)
//   - Monitors SentinelWatchdog stats (hash mismatches, timing anomalies)
//   - Monitors UnifiedHotpatchManager stats (layer error counts)
//   - Monitors PerfTelemetry for drift alerts (median >2x baseline)
//   - On anomaly: deactivates sentinel → requests LLM-driven fix via
//     AgentOrchestrator → applies via SelfRepairLoop → verifies →
//     updates baseline → reactivates sentinel
//   - Maintains an AnomalyLog ring buffer (128 entries) for forensics
//   - Integrates with KnowledgeGraphCore to persist repair decisions
//   - Thread-safe, singleton pattern, PatchResult returns
//
// Architecture: C++20 | Win32 | No exceptions | No Qt
// Pattern:      Singleton | PatchResult returns | std::mutex
// Threading:    Dedicated orchestrator thread (CreateThread)
// Rule:         NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef RAWRXD_AUTO_REPAIR_ORCHESTRATOR_HPP
#define RAWRXD_AUTO_REPAIR_ORCHESTRATOR_HPP

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <atomic>
#include <string>
#include <vector>
#include <cstring>

#include "sentinel_watchdog.hpp"

// Forward declarations
struct PatchResult;

namespace RawrXD {
namespace Knowledge { class KnowledgeGraphCore; }
namespace Perf      { class PerfTelemetry; }
}

// ============================================================================
// Configuration Constants
// ============================================================================

#define AUTOREPAIR_DEFAULT_POLL_MS           2000
#define AUTOREPAIR_MAX_ANOMALY_LOG           128
#define AUTOREPAIR_MAX_CONSECUTIVE_REPAIRS   5
#define AUTOREPAIR_COOLDOWN_MS               30000
#define AUTOREPAIR_LLM_TIMEOUT_MS            120000
#define AUTOREPAIR_INTEGRITY_RECHECK_MS      500

// ============================================================================
// Anomaly Classification
// ============================================================================

enum class AnomalyType : uint32_t {
    None                    = 0x00,
    SentinelHashMismatch    = 0x01,     // .text modified without authorization
    SentinelTimingAnomaly   = 0x02,     // RDTSC timing indicative of debugger
    SentinelDebuggerPresent = 0x04,     // PEB.BeingDebugged set
    PerfDriftDetected       = 0x08,     // Kernel latency >2x baseline median
    LayerMemoryError        = 0x10,     // Memory hotpatch layer error
    LayerByteError          = 0x20,     // Byte-level hotpatch layer error
    LayerServerError        = 0x40,     // Server hotpatch layer error
    DetourCRCMismatch       = 0x80,     // Active detour CRC no longer matches
    SnapshotCorruption      = 0x100,    // Snapshot slot integrity failure
    ConsecutiveRepairLimit  = 0x200,    // Too many repairs in succession
    Custom                  = 0x400     // User-injected goal/anomaly
};

// ============================================================================
// Anomaly Severity
// ============================================================================

enum class AnomalySeverity : uint8_t {
    Info        = 0,    // Informational, no action required
    Warning     = 1,    // Minor drift, log and continue
    Error       = 2,    // Requires autonomous repair attempt
    Critical    = 3,    // Requires immediate lockdown or escalation
    Fatal       = 4     // System integrity compromised beyond repair
};

// ============================================================================
// Anomaly Log Entry
// ============================================================================

struct AnomalyEntry {
    AnomalyType         type;
    AnomalySeverity     severity;
    uint64_t            timestamp;          // GetTickCount64()
    uint64_t            rdtsc;              // RDTSC snapshot at detection
    char                description[256];
    char                sourceLayer[64];    // Which layer/subsystem reported it
    bool                repairAttempted;
    bool                repairSucceeded;
    char                repairDetail[256];  // What the repair did
};

// ============================================================================
// Repair Action — describes what the orchestrator did to fix an anomaly
// ============================================================================

struct RepairAction {
    AnomalyType         triggerType;
    uint64_t            startTimestamp;
    uint64_t            endTimestamp;
    bool                success;
    char                agentPrompt[512];   // What was asked of the LLM
    char                agentResponse[1024]; // Summary of LLM's response
    char                patchDescription[256];
    uint32_t            rollbackSlot;       // Snapshot slot used for safety
    double              latencyMs;          // Total repair time
};

// ============================================================================
// Orchestrator Statistics
// ============================================================================

struct AutoRepairStats {
    uint64_t    totalPollCycles;
    uint64_t    anomaliesDetected;
    uint64_t    repairsAttempted;
    uint64_t    repairsSucceeded;
    uint64_t    repairsFailed;
    uint64_t    rollbacksPerformed;
    uint64_t    cooldownsEntered;
    uint64_t    escalationsTriggered;
    uint64_t    uptimeMs;
    double      avgRepairLatencyMs;
    double      lastPollDurationMs;
};

// ============================================================================
// Orchestrator Configuration
// ============================================================================

struct AutoRepairConfig {
    uint32_t    pollIntervalMs;             // How often to check (default 2000)
    uint32_t    maxConsecutiveRepairs;       // Before cooldown (default 5)
    uint32_t    cooldownMs;                 // Recovery period after max repairs (default 30s)
    uint32_t    llmTimeoutMs;               // Timeout for AgentOrchestrator call (default 120s)
    bool        enableAutoRepair;           // Master switch (default true)
    bool        enablePerfMonitoring;       // Check PerfTelemetry drift (default true)
    bool        enableSentinelMonitoring;   // Check SentinelWatchdog (default true)
    bool        enableLayerMonitoring;      // Check UnifiedHotpatchManager (default true)
    bool        enableKnowledgePersist;     // Record decisions to KnowledgeGraph (default true)
    bool        dryRun;                     // Detect anomalies but don't repair (default false)
    char        workingDirectory[260];      // CWD for agent tool execution

    static AutoRepairConfig defaults() {
        AutoRepairConfig c;
        c.pollIntervalMs            = AUTOREPAIR_DEFAULT_POLL_MS;
        c.maxConsecutiveRepairs     = AUTOREPAIR_MAX_CONSECUTIVE_REPAIRS;
        c.cooldownMs                = AUTOREPAIR_COOLDOWN_MS;
        c.llmTimeoutMs              = AUTOREPAIR_LLM_TIMEOUT_MS;
        c.enableAutoRepair          = true;
        c.enablePerfMonitoring      = true;
        c.enableSentinelMonitoring  = true;
        c.enableLayerMonitoring     = true;
        c.enableKnowledgePersist    = true;
        c.dryRun                    = false;
        std::memset(c.workingDirectory, 0, sizeof(c.workingDirectory));
        return c;
    }
};

// ============================================================================
// AutoRepairOrchestrator — Singleton Autonomous Repair Daemon
// ============================================================================

class AutoRepairOrchestrator {
public:
    static AutoRepairOrchestrator& instance();

    // ---- Lifecycle ----
    PatchResult initialize(const AutoRepairConfig& config = AutoRepairConfig::defaults());
    PatchResult shutdown();
    bool isRunning() const;

    // ---- Configuration ----
    void setConfig(const AutoRepairConfig& config);
    const AutoRepairConfig& getConfig() const;

    // ---- Manual Controls ----
    // Force an immediate poll cycle (bypasses timer)
    PatchResult pollNow();

    // Pause/resume the daemon without full shutdown
    PatchResult pause();
    PatchResult resume();
    bool isPaused() const;

    // ---- Anomaly Injection (for testing) ----
    PatchResult injectAnomaly(AnomalyType type, const char* description);

    // ---- Query ----
    AutoRepairStats getStats() const;
    std::string statsToJson() const;

    // ---- Anomaly Log (ring buffer) ----
    const AnomalyEntry* getAnomalyLog(uint32_t& outCount) const;
    const RepairAction* getRepairLog(uint32_t& outCount) const;

    // ---- Callbacks ----
    typedef void (*AnomalyCallback)(const AnomalyEntry* entry, void* userData);
    typedef void (*RepairCallback)(const RepairAction* action, void* userData);
    void setAnomalyCallback(AnomalyCallback cb, void* userData);
    void setRepairCallback(RepairCallback cb, void* userData);

private:
    AutoRepairOrchestrator();
    ~AutoRepairOrchestrator();
    AutoRepairOrchestrator(const AutoRepairOrchestrator&) = delete;
    AutoRepairOrchestrator& operator=(const AutoRepairOrchestrator&) = delete;

    // ---- Orchestrator Thread ----
    static DWORD WINAPI OrchestratorThreadProc(LPVOID param);
    void orchestratorLoop();

    // ---- Monitoring Subsystems ----
    void checkSentinelHealth();
    void checkLayerHealth();
    void checkPerfHealth();
    void checkDetourIntegrity();

    // ---- Repair Pipeline ----
    // Orchestrates: snapshot → deactivate sentinel → LLM fix → apply →
    //               verify vectors → update baseline → activate sentinel
    RepairAction executeRepair(const AnomalyEntry& anomaly);

    // Generate a repair prompt from the anomaly context
    std::string buildRepairPrompt(const AnomalyEntry& anomaly) const;

    // Validate post-repair integrity
    bool validateRepair(const AnomalyEntry& anomaly) const;

    // Rollback if repair failed
    PatchResult rollbackRepair(uint32_t snapshotSlot);

    // ---- Logging ----
    void logAnomaly(AnomalyType type, AnomalySeverity severity,
                    const char* source, const char* description);
    void logRepair(const RepairAction& action);

    // ---- Knowledge Graph Integration ----
    void persistDecision(const RepairAction& action);

    // ---- Cooldown Management ----
    bool isInCooldown() const;
    void enterCooldown();

    // ---- State ----
    mutable std::mutex              m_mutex;
    std::atomic<bool>               m_running;
    std::atomic<bool>               m_paused;
    std::atomic<bool>               m_shutdownRequested;
    HANDLE                          m_orchestratorThread;
    DWORD                           m_orchestratorThreadId;

    AutoRepairConfig                m_config;
    AutoRepairStats                 m_stats;

    // Anomaly log (ring buffer)
    AnomalyEntry                    m_anomalyLog[AUTOREPAIR_MAX_ANOMALY_LOG];
    uint32_t                        m_anomalyLogHead;
    uint32_t                        m_anomalyLogCount;

    // Repair log (ring buffer)
    RepairAction                    m_repairLog[AUTOREPAIR_MAX_ANOMALY_LOG];
    uint32_t                        m_repairLogHead;
    uint32_t                        m_repairLogCount;

    // Consecutive repair tracking
    uint32_t                        m_consecutiveRepairs;
    uint64_t                        m_cooldownEndTime;

    // Previous stats snapshots (for delta detection)
    SentinelStats                   m_prevSentinelStats;
    uint64_t                        m_prevLayerErrors;

    // Startup timestamp
    uint64_t                        m_startTime;

    // Callbacks
    AnomalyCallback                 m_anomalyCb;    void* m_anomalyCbUD;
    RepairCallback                  m_repairCb;     void* m_repairCbUD;
};

// ============================================================================
// C-ABI Exports (for cross-module / ASM callers)
// ============================================================================

extern "C" {
    int  autorepair_initialize(void);
    int  autorepair_shutdown(void);
    int  autorepair_poll_now(void);
    int  autorepair_pause(void);
    int  autorepair_resume(void);
    int  autorepair_is_running(void);
}

#endif // RAWRXD_AUTO_REPAIR_ORCHESTRATOR_HPP
