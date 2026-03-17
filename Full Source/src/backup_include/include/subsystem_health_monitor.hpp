// ============================================================================
// subsystem_health_monitor.hpp — Subsystem Health Monitor
// ============================================================================
// Continuously monitors every RawrXD subsystem (Memory Hotpatch, Byte Patcher,
// Server Hotpatch, Inference Engine, Thread Pool, KV Cache, etc.) via periodic
// heartbeats, resource usage probes, and liveness checks.
//
// Architecture:
//   - Each subsystem registers a HealthProbe (function pointer returning status)
//   - Monitor runs a polling loop (configurable interval)
//   - Aggregates per-subsystem status into a SystemHealthSnapshot
//   - Fires callbacks on degradation / recovery transitions
//   - Integrates with agentic failure detector for auto-remediation
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
#include <chrono>
#include <thread>
#include <functional>

namespace RawrXD {
namespace Health {

// ============================================================================
// Health Status Levels
// ============================================================================
enum class HealthStatus : uint8_t {
    Healthy     = 0,   // Fully operational
    Degraded    = 1,   // Operational with reduced capacity
    Warning     = 2,   // Approaching threshold
    Critical    = 3,   // Partially non-functional
    Down        = 4,   // Completely non-functional
    Unknown     = 5,   // Probe not yet run or timed out
};

const char* healthStatusName(HealthStatus s);

// ============================================================================
// Subsystem Identity
// ============================================================================
enum class SubsystemId : uint16_t {
    MemoryHotpatch      = 0,
    ByteLevelHotpatch   = 1,
    ServerHotpatch      = 2,
    ProxyHotpatch       = 3,
    UnifiedHotpatchMgr  = 4,
    InferenceEngine     = 5,
    ExecutionScheduler  = 6,
    ThreadPool          = 7,
    KVCache             = 8,
    ModelLoader         = 9,
    StreamingEngine     = 10,
    AgenticDetector     = 11,
    AgenticPuppeteer    = 12,
    TransactionJournal  = 13,
    RecoveryJournal     = 14,
    StateMachine        = 15,
    VulkanCompute       = 16,
    LSPServer           = 17,
    APIServer           = 18,
    ExtensionHost       = 19,
    TelemetryCollector  = 20,
    _COUNT              = 21,
};

const char* subsystemName(SubsystemId id);

// ============================================================================
// Health Probe — function pointer returning subsystem status
// ============================================================================
struct SubsystemProbeResult {
    HealthStatus    status;
    const char*     detail;          // Human-readable status message
    double          latencyMs;       // Time to execute probe
    uint64_t        uptimeMs;        // Subsystem uptime since last restart
    float           loadPercent;     // 0.0 – 100.0 resource utilization
    uint64_t        errorCount;      // Accumulated errors
    uint64_t        requestCount;    // Total operations processed
};

typedef SubsystemProbeResult (*HealthProbeFn)(void* subsystemCtx);

// ============================================================================
// Subsystem Registration
// ============================================================================
struct SubsystemRegistration {
    SubsystemId     id;
    const char*     name;            // Human-readable
    HealthProbeFn   probe;           // Polling function
    void*           ctx;             // Opaque pointer passed to probe
    bool            enabled;
    uint32_t        pollingIntervalMs;  // Per-subsystem override (0 = use default)
    HealthStatus    lastStatus;
    uint64_t        lastProbeTimestamp;
    uint64_t        consecutiveFailures;
    uint64_t        totalProbes;
};

// ============================================================================
// Health Snapshot — point-in-time picture of all subsystems
// ============================================================================
struct SystemHealthSnapshot {
    uint64_t        timestamp;       // Epoch ms
    uint64_t        snapshotId;      // Monotonic
    HealthStatus    overallStatus;   // Worst-case across all subsystems
    uint32_t        healthyCount;
    uint32_t        degradedCount;
    uint32_t        criticalCount;
    uint32_t        downCount;
    uint32_t        unknownCount;
    double          totalProbeLatencyMs;

    struct SubsystemEntry {
        SubsystemId             id;
        HealthStatus            status;
        SubsystemProbeResult    lastResult;
    };
    std::vector<SubsystemEntry> subsystems;
};

// ============================================================================
// Health Event — fired on status transitions
// ============================================================================
struct HealthEvent {
    uint64_t        timestamp;
    SubsystemId     subsystem;
    HealthStatus    previousStatus;
    HealthStatus    newStatus;
    const char*     detail;
};

// ============================================================================
// Callback types
// ============================================================================
typedef void (*HealthEventCallback)(const HealthEvent* event, void* userData);
typedef void (*SnapshotCallback)(const SystemHealthSnapshot* snapshot, void* userData);

// ============================================================================
// Monitor Configuration
// ============================================================================
struct HealthMonitorConfig {
    uint32_t    defaultPollingIntervalMs = 5000;    // 5 seconds
    uint32_t    probeTimeoutMs          = 2000;     // 2 seconds
    uint32_t    maxConsecutiveFailures  = 3;        // Before marking Down
    bool        autoRemediation         = false;    // Trigger agentic correction
    bool        logTransitions          = true;
    size_t      maxEventHistory         = 1024;
};

// ============================================================================
// Monitor Stats
// ============================================================================
struct MonitorStats {
    std::atomic<uint64_t> totalProbes{0};
    std::atomic<uint64_t> failedProbes{0};
    std::atomic<uint64_t> statusTransitions{0};
    std::atomic<uint64_t> remediationAttempts{0};
    std::atomic<uint64_t> snapshotsTaken{0};
    uint64_t              monitorUptimeMs = 0;

    MonitorStats() = default;
    MonitorStats(const MonitorStats&) = delete;
    MonitorStats& operator=(const MonitorStats&) = delete;
};

// ============================================================================
// SubsystemHealthMonitor — the core monitor
// ============================================================================
class SubsystemHealthMonitor {
public:
    SubsystemHealthMonitor();
    ~SubsystemHealthMonitor();

    // Non-copyable
    SubsystemHealthMonitor(const SubsystemHealthMonitor&) = delete;
    SubsystemHealthMonitor& operator=(const SubsystemHealthMonitor&) = delete;

    // ---- Configuration ----
    PatchResult configure(const HealthMonitorConfig& config);
    HealthMonitorConfig getConfig() const;

    // ---- Subsystem Registration ----
    PatchResult registerSubsystem(SubsystemId id, const char* name,
                                   HealthProbeFn probe, void* ctx);
    PatchResult unregisterSubsystem(SubsystemId id);
    PatchResult enableSubsystem(SubsystemId id, bool enable);
    PatchResult setSubsystemPollingInterval(SubsystemId id, uint32_t intervalMs);

    // ---- Monitor Lifecycle ----
    PatchResult start();
    PatchResult stop();
    bool isRunning() const;

    // ---- Manual Probing ----
    SubsystemProbeResult probeSubsystem(SubsystemId id);
    SystemHealthSnapshot takeSnapshot();

    // ---- Status Queries ----
    HealthStatus overallStatus() const;
    HealthStatus subsystemStatus(SubsystemId id) const;
    bool allHealthy() const;
    bool anyDown() const;
    std::vector<SubsystemId> degradedSubsystems() const;
    std::vector<SubsystemId> downSubsystems() const;

    // ---- Callbacks ----
    void setHealthEventCallback(HealthEventCallback cb, void* userData);
    void setSnapshotCallback(SnapshotCallback cb, void* userData);

    // ---- Event History ----
    std::vector<HealthEvent> recentEvents(size_t count) const;
    std::vector<SystemHealthSnapshot> recentSnapshots(size_t count) const;

    // ---- Stats ----
    MonitorStats getStats() const;
    void resetStats();

    // ---- Export ----
    std::string exportStatusJson() const;
    std::string exportDot() const;

private:
    void monitorLoop();
    void probeAllSubsystems();
    void fireEvent(SubsystemId id, HealthStatus prev, HealthStatus curr,
                   const char* detail);
    HealthStatus computeOverallStatus() const;

    mutable std::mutex m_mutex;

    HealthMonitorConfig m_config;
    std::vector<SubsystemRegistration> m_subsystems;

    // Event history (ring buffer)
    std::vector<HealthEvent> m_eventHistory;
    std::vector<SystemHealthSnapshot> m_snapshotHistory;

    // Monitor thread
    std::thread             m_thread;
    std::atomic<bool>       m_running{false};
    std::atomic<bool>       m_stopRequested{false};

    // Callbacks
    HealthEventCallback     m_eventCallback = nullptr;
    void*                   m_eventCallbackData = nullptr;
    SnapshotCallback        m_snapshotCallback = nullptr;
    void*                   m_snapshotCallbackData = nullptr;

    // Stats
    MonitorStats m_stats;
    std::chrono::steady_clock::time_point m_startTime;
    std::atomic<uint64_t> m_snapshotSeq{0};
};

} // namespace Health
} // namespace RawrXD
