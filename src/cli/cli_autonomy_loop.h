// ============================================================================
// cli_autonomy_loop.h — Phase 19: Headless Autonomy Loop for CLI
// ============================================================================
//
// Wraps the AgenticDecisionTree into a self-driving background event loop
// for headless (non-GUI) operation. The loop continuously:
//
//   1. Polls inference output for quality
//   2. Detects failures via the decision tree
//   3. Autonomously decides on corrective action
//   4. Applies hotpatches (memory, byte, server)
//   5. Verifies corrections
//
// Safety:
//   - Rate-limited (configurable actions per minute)
//   - Integrated with AgentSafetyContract for risk gating
//   - Execution governor awareness (no blocked commands)
//   - Graceful shutdown (cooperative cancellation)
//
// Pattern: Structured results (PatchResult-style), no exceptions.
// Threading: Background thread with cooperative cancellation.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#include "agentic_decision_tree.h"
#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <functional>
#include <deque>

// Forward declarations
class AgenticEngine;
class SubAgentManager;

// ============================================================================
// ENUMS
// ============================================================================

enum class AutonomyLoopState : uint8_t {
    Idle        = 0,    // Not running
    Starting    = 1,    // Initializing
    Running     = 2,    // Active autonomy loop
    Paused      = 3,    // Temporarily paused
    Stopping    = 4,    // Shutting down
    Error       = 5,    // Loop encountered critical error
};

enum class AutonomyEvent : uint8_t {
    LoopStarted     = 0,
    LoopStopped     = 1,
    LoopPaused      = 2,
    LoopResumed     = 3,
    TickStarted     = 4,
    TickCompleted   = 5,
    FailureDetected = 6,
    CorrectionApplied = 7,
    CorrectionFailed  = 8,
    Escalation      = 9,
    RateLimited     = 10,
    SafetyBlocked   = 11,
    Error           = 12,
};

// ============================================================================
// STRUCTS
// ============================================================================

// Configuration for the autonomy loop
struct AutonomyLoopConfig {
    int     tickIntervalMs;         // How often to poll (default: 2000ms)
    int     maxActionsPerMinute;    // Rate limit (default: 30)
    int     maxConsecutiveFailures; // Stop after this many failures (default: 5)
    float   confidenceThreshold;    // Min confidence to act (default: 0.6)
    bool    autoStartOnLoad;        // Start loop when model loads
    bool    enableSSALift;          // Allow SSA lifting in autonomy
    bool    enableMemoryPatch;      // Allow memory hotpatching
    bool    enableBytePatch;        // Allow byte-level patching
    bool    enableServerPatch;      // Allow server-layer patching
    bool    pauseOnEscalation;      // Pause loop when escalation happens
    bool    verboseTracing;         // Log every decision node

    AutonomyLoopConfig()
        : tickIntervalMs(2000)
        , maxActionsPerMinute(30)
        , maxConsecutiveFailures(5)
        , confidenceThreshold(0.6f)
        , autoStartOnLoad(false)
        , enableSSALift(true)
        , enableMemoryPatch(true)
        , enableBytePatch(false)    // Off by default (high risk)
        , enableServerPatch(true)
        , pauseOnEscalation(true)
        , verboseTracing(false)
    {}
};

// A record of one autonomy tick
struct AutonomyTickRecord {
    uint64_t    tickId;
    std::chrono::steady_clock::time_point timestamp;
    int         durationMs;
    bool        failureDetected;
    bool        correctionAttempted;
    bool        correctionSucceeded;
    std::string summary;
};

// Autonomy loop statistics
struct AutonomyLoopStats {
    std::atomic<uint64_t> totalTicks{0};
    std::atomic<uint64_t> failureDetections{0};
    std::atomic<uint64_t> correctionsAttempted{0};
    std::atomic<uint64_t> correctionsSucceeded{0};
    std::atomic<uint64_t> correctionsFailed{0};
    std::atomic<uint64_t> escalations{0};
    std::atomic<uint64_t> rateLimitHits{0};
    std::atomic<uint64_t> safetyBlocks{0};
    std::atomic<uint64_t> errors{0};
    std::atomic<uint64_t> uptimeMs{0};
};

// Callback types (function pointers for hot path)
typedef void (*AutonomyEventCallback)(AutonomyEvent event, const char* detail, void* userData);

// ============================================================================
// CLIAutonomyLoop — Main class
// ============================================================================

class CLIAutonomyLoop {
public:
    static CLIAutonomyLoop& instance();

    // ---- Engine Wiring ----
    void setAgenticEngine(AgenticEngine* engine);
    void setSubAgentManager(SubAgentManager* mgr);

    // ---- Lifecycle ----
    void start();                   // Start the background loop
    void stop();                    // Stop gracefully
    void pause();                   // Pause (keep thread alive)
    void resume();                  // Resume from pause
    AutonomyLoopState getState() const;

    // ---- Manual Tick ----
    // Execute a single decision tree evaluation cycle outside the loop.
    DecisionOutcome tick();

    // Execute a single tick with specific output to analyze.
    DecisionOutcome tickWithOutput(const std::string& output,
                                    const std::string& prompt);

    // ---- Output Queue ----
    // Push an inference output for the loop to analyze on its next tick.
    void enqueueOutput(const std::string& output, const std::string& prompt);

    // ---- SSA Lift (Direct API) ----
    // Run SSA lifter outside the loop for manual analysis.
    std::string runSSALift(const std::string& binaryPath,
                            const std::string& functionName,
                            uint64_t functionAddr = 0);

    // ---- Auto-Patch (Direct API) ----
    // Manually trigger the auto-patch pipeline on the last failure.
    DecisionOutcome autoPatch();

    // ---- Configuration ----
    void setConfig(const AutonomyLoopConfig& cfg);
    AutonomyLoopConfig getConfig() const;

    // ---- Statistics ----
    const AutonomyLoopStats& getStats() const;
    void resetStats();

    // ---- History ----
    std::vector<AutonomyTickRecord> getRecentTicks(int count = 20) const;

    // ---- Callbacks ----
    void registerEventCallback(AutonomyEventCallback cb, void* userData);
    void unregisterEventCallback(AutonomyEventCallback cb);

    // ---- Status ----
    std::string getStatusString() const;
    std::string getDetailedStatus() const;

private:
    CLIAutonomyLoop();
    ~CLIAutonomyLoop();
    CLIAutonomyLoop(const CLIAutonomyLoop&) = delete;
    CLIAutonomyLoop& operator=(const CLIAutonomyLoop&) = delete;

    // Background loop function
    void loopFunction();

    // Rate limiting
    bool rateLimitAllow();
    void rateLimitRecord();

    // Event notification
    void emitEvent(AutonomyEvent event, const std::string& detail);

    // ---- State ----
    mutable std::mutex                  m_mutex;
    std::atomic<AutonomyLoopState>      m_state;
    std::thread                         m_loopThread;
    std::atomic<bool>                   m_stopRequested;

    AutonomyLoopConfig                  m_config;
    AutonomyLoopStats                   m_stats;

    // Engine pointers (non-owning)
    AgenticEngine*                      m_engine;
    SubAgentManager*                    m_subAgentMgr;

    // Output queue (pending outputs to analyze)
    struct PendingOutput {
        std::string output;
        std::string prompt;
    };
    std::deque<PendingOutput>           m_outputQueue;
    std::mutex                          m_queueMutex;

    // Tick history (circular buffer)
    static constexpr size_t MAX_TICK_HISTORY = 100;
    std::deque<AutonomyTickRecord>      m_tickHistory;

    // Last failure context (for !auto_patch)
    TreeContext                         m_lastFailureCtx;
    bool                                m_hasLastFailure;

    // Rate limiting state
    int                                 m_actionsThisMinute;
    std::chrono::steady_clock::time_point m_minuteWindowStart;

    // Consecutive failure tracking
    int                                 m_consecutiveFailures;

    // Callbacks
    struct EventCB { AutonomyEventCallback fn; void* userData; };
    std::vector<EventCB>                m_eventCallbacks;

    // Uptime tracking
    std::chrono::steady_clock::time_point m_startTime;
};

