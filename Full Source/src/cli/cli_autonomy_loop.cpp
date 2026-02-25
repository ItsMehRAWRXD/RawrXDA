// ============================================================================
// cli_autonomy_loop.cpp — Phase 19: Headless Autonomy Loop Implementation
// ============================================================================
//
// Background thread loop: poll → detect → decide → act → verify cycle.
// All output is to stdout (headless/CLI mode). No GUI dependencies.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "cli_autonomy_loop.h"
#include "../agentic_engine.h"
#include "../subagent_core.h"
#include "../reverse_engineering/RawrCodex.hpp"

#include "logging/logger.h"
static Logger s_logger("cli_autonomy_loop");

#include <iostream>
#include <sstream>
#include <algorithm>

// ============================================================================
// Singleton
// ============================================================================

CLIAutonomyLoop& CLIAutonomyLoop::instance() {
    static CLIAutonomyLoop inst;
    return inst;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

CLIAutonomyLoop::CLIAutonomyLoop()
    : m_state(AutonomyLoopState::Idle)
    , m_stopRequested(false)
    , m_engine(nullptr)
    , m_subAgentMgr(nullptr)
    , m_hasLastFailure(false)
    , m_actionsThisMinute(0)
    , m_consecutiveFailures(0)
{
    m_minuteWindowStart = std::chrono::steady_clock::now();
}

CLIAutonomyLoop::~CLIAutonomyLoop() {
    stop();
}

// ============================================================================
// Engine Wiring
// ============================================================================

void CLIAutonomyLoop::setAgenticEngine(AgenticEngine* engine) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_engine = engine;
    AgenticDecisionTree::instance().setAgenticEngine(engine);
}

void CLIAutonomyLoop::setSubAgentManager(SubAgentManager* mgr) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_subAgentMgr = mgr;
    AgenticDecisionTree::instance().setSubAgentManager(mgr);
}

// ============================================================================
// Lifecycle
// ============================================================================

void CLIAutonomyLoop::start() {
    if (m_state.load() == AutonomyLoopState::Running ||
        m_state.load() == AutonomyLoopState::Starting) {
        return;
    }

    m_state.store(AutonomyLoopState::Starting);
    m_stopRequested.store(false);
    m_consecutiveFailures = 0;
    m_startTime = std::chrono::steady_clock::now();

    emitEvent(AutonomyEvent::LoopStarted, "Autonomy loop starting");

    m_loopThread = std::thread(&CLIAutonomyLoop::loopFunction, this);

    m_state.store(AutonomyLoopState::Running);
    s_logger.info("🤖 [AUTONOMY] Background loop started (tick=");
}

void CLIAutonomyLoop::stop() {
    if (m_state.load() == AutonomyLoopState::Idle) return;

    m_state.store(AutonomyLoopState::Stopping);
    m_stopRequested.store(true);

    if (m_loopThread.joinable()) {
        m_loopThread.join();
    }

    m_state.store(AutonomyLoopState::Idle);
    emitEvent(AutonomyEvent::LoopStopped, "Autonomy loop stopped");
    s_logger.info("⏹️  [AUTONOMY] Loop stopped\n");
}

void CLIAutonomyLoop::pause() {
    if (m_state.load() != AutonomyLoopState::Running) return;
    m_state.store(AutonomyLoopState::Paused);
    emitEvent(AutonomyEvent::LoopPaused, "Autonomy loop paused");
    s_logger.info("⏸️  [AUTONOMY] Loop paused\n");
}

void CLIAutonomyLoop::resume() {
    if (m_state.load() != AutonomyLoopState::Paused) return;
    m_state.store(AutonomyLoopState::Running);
    m_consecutiveFailures = 0;
    emitEvent(AutonomyEvent::LoopResumed, "Autonomy loop resumed");
    s_logger.info("▶️  [AUTONOMY] Loop resumed\n");
}

AutonomyLoopState CLIAutonomyLoop::getState() const {
    return m_state.load();
}

// ============================================================================
// Background Loop
// ============================================================================

void CLIAutonomyLoop::loopFunction() {
    while (!m_stopRequested.load()) {
        // Sleep for tick interval
        for (int i = 0; i < m_config.tickIntervalMs / 100; i++) {
            if (m_stopRequested.load()) return;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Check if paused
        if (m_state.load() == AutonomyLoopState::Paused) {
            continue;
        }

        // Check consecutive failure limit
        if (m_consecutiveFailures >= m_config.maxConsecutiveFailures) {
            s_logger.info("⚠️  [AUTONOMY] Max consecutive failures reached (");
            pause();
            continue;
        }

        // Rate limit check
        if (!rateLimitAllow()) {
            m_stats.rateLimitHits++;
            emitEvent(AutonomyEvent::RateLimited, "Rate limit exceeded");
            if (m_config.verboseTracing) {
                s_logger.info("🚦 [AUTONOMY] Rate limited, skipping tick\n");
            }
            continue;
        }

        // Check for queued outputs
        PendingOutput pending;
        bool hasPending = false;
        {
            std::lock_guard<std::mutex> qlock(m_queueMutex);
            if (!m_outputQueue.empty()) {
                pending = m_outputQueue.front();
                m_outputQueue.pop_front();
                hasPending = true;
            }
        }

        if (!hasPending) {
            // No queued output — skip this tick
            continue;
        }

        // Execute a tick
        m_stats.totalTicks++;
        emitEvent(AutonomyEvent::TickStarted, "Tick " + std::to_string(m_stats.totalTicks.load()));

        auto tickStart = std::chrono::steady_clock::now();

        // Build tree context from queued output
        TreeContext ctx;
        ctx.inferenceOutput = pending.output;
        ctx.inferencePrompt = pending.prompt;

        // Run the decision tree
        auto& tree = AgenticDecisionTree::instance();
        DecisionOutcome outcome = tree.evaluate(ctx);

        auto tickEnd = std::chrono::steady_clock::now();
        int tickMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(
            tickEnd - tickStart).count();

        // Record tick
        AutonomyTickRecord record;
        record.tickId = m_stats.totalTicks.load();
        record.timestamp = tickStart;
        record.durationMs = tickMs;
        record.failureDetected = (ctx.failureType != 0);
        record.correctionAttempted = ctx.patchApplied;
        record.correctionSucceeded = outcome.success;
        record.summary = outcome.summary ? outcome.summary : "";

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_tickHistory.push_back(record);
            if (m_tickHistory.size() > MAX_TICK_HISTORY) {
                m_tickHistory.pop_front();
            }
        }

        // Update stats
        if (record.failureDetected) {
            m_stats.failureDetections++;
        }
        if (record.correctionAttempted) {
            m_stats.correctionsAttempted++;
            rateLimitRecord();
        }

        // Handle outcome
        if (outcome.success) {
            m_consecutiveFailures = 0;
            m_stats.correctionsSucceeded++;
            emitEvent(AutonomyEvent::CorrectionApplied,
                      std::string(outcome.summary ? outcome.summary : "Correction applied"));

            s_logger.info("✅ [AUTONOMY:Tick");

            // Print trace in verbose mode
            if (m_config.verboseTracing) {
                for (const auto& t : outcome.traceLog) {
                    s_logger.info("    ");
                }
            }
        } else if (outcome.finalVerdict == NodeVerdict::Escalate) {
            m_stats.escalations++;
            emitEvent(AutonomyEvent::Escalation,
                      ctx.escalationReason.empty() ? "Escalated to user" : ctx.escalationReason);

            s_logger.info("⚠️  [AUTONOMY:Tick");

            // Save for manual !auto_patch
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_lastFailureCtx = ctx;
                m_hasLastFailure = true;
            }

            if (m_config.pauseOnEscalation) {
                pause();
            }
        } else if (outcome.finalVerdict == NodeVerdict::Skip) {
            // Output was healthy, no action needed
            if (m_config.verboseTracing) {
                s_logger.info("🟢 [AUTONOMY:Tick");
            }
        } else {
            m_consecutiveFailures++;
            m_stats.correctionsFailed++;
            emitEvent(AutonomyEvent::CorrectionFailed,
                      std::string(outcome.summary ? outcome.summary : "Correction failed"));

            s_logger.info("❌ [AUTONOMY:Tick");

            // Save for manual !auto_patch
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_lastFailureCtx = ctx;
                m_hasLastFailure = true;
            }
        }

        emitEvent(AutonomyEvent::TickCompleted,
                  "Tick " + std::to_string(record.tickId) + " complete");

        // Update uptime
        auto uptime = std::chrono::steady_clock::now() - m_startTime;
        m_stats.uptimeMs.store(
            (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(uptime).count());
    }
}

// ============================================================================
// Manual Tick
// ============================================================================

DecisionOutcome CLIAutonomyLoop::tick() {
    // If there's a queued output, use it
    PendingOutput pending;
    bool hasPending = false;
    {
        std::lock_guard<std::mutex> qlock(m_queueMutex);
        if (!m_outputQueue.empty()) {
            pending = m_outputQueue.front();
            m_outputQueue.pop_front();
            hasPending = true;
        }
    }

    if (!hasPending) {
        return DecisionOutcome::error(NodeVerdict::Skip, "No output queued for analysis");
    }

    return tickWithOutput(pending.output, pending.prompt);
}

DecisionOutcome CLIAutonomyLoop::tickWithOutput(const std::string& output,
                                                  const std::string& prompt) {
    TreeContext ctx;
    ctx.inferenceOutput = output;
    ctx.inferencePrompt = prompt;

    auto& tree = AgenticDecisionTree::instance();
    DecisionOutcome outcome = tree.evaluate(ctx);

    // Save failure context for manual follow-up
    if (!outcome.success && ctx.failureType != 0) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastFailureCtx = ctx;
        m_hasLastFailure = true;
    }

    return outcome;
}

// ============================================================================
// Output Queue
// ============================================================================

void CLIAutonomyLoop::enqueueOutput(const std::string& output, const std::string& prompt) {
    std::lock_guard<std::mutex> qlock(m_queueMutex);
    m_outputQueue.push_back({output, prompt});

    // Cap queue size to prevent unbounded growth
    while (m_outputQueue.size() > 50) {
        m_outputQueue.pop_front();
    }
}

// ============================================================================
// SSA Lift (Direct API)
// ============================================================================

std::string CLIAutonomyLoop::runSSALift(const std::string& binaryPath,
                                          const std::string& functionName,
                                          uint64_t functionAddr) {
    TreeContext ctx;
    ctx.targetBinaryPath = binaryPath;
    ctx.targetFunctionName = functionName;
    ctx.targetFunctionAddr = functionAddr;

    auto& tree = AgenticDecisionTree::instance();
    bool ok = tree.runSSALiftWithAnomalyDetection(ctx);

    if (ok) {
        // Print anomaly trace
        for (const auto& t : ctx.traceLog) {
            if (t.find("[SSA:ANOMALY]") != std::string::npos) {
                s_logger.info("⚠️  ");
            }
        }
        return ctx.ssaLiftResult;
    }

    return "[SSA Lift Failed] Check binary path and function address.\n"
           "Trace:\n" + ([&]() {
               std::string s;
               for (const auto& t : ctx.traceLog) s += "  " + t + "\n";
               return s;
           })();
}

// ============================================================================
// Auto-Patch (Direct API)
// ============================================================================

DecisionOutcome CLIAutonomyLoop::autoPatch() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_hasLastFailure) {
        return DecisionOutcome::error(NodeVerdict::Skip,
                                       "No recent failure to patch. Run inference first.");
    }

    // Re-evaluate from the CorrectionPlan node using the saved context
    auto& tree = AgenticDecisionTree::instance();
    TreeContext ctx = m_lastFailureCtx;

    // Re-run from root to get full pipeline
    DecisionOutcome outcome = tree.evaluate(ctx);

    if (outcome.success) {
        m_hasLastFailure = false; // Clear on success
        s_logger.info("✅ [AUTO_PATCH] Correction applied: ");
    } else {
        s_logger.info("❌ [AUTO_PATCH] Correction failed: ");
    }

    // Print trace
    for (const auto& t : outcome.traceLog) {
        s_logger.info("    ");
    }

    return outcome;
}

// ============================================================================
// Configuration
// ============================================================================

void CLIAutonomyLoop::setConfig(const AutonomyLoopConfig& cfg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = cfg;

    // Propagate to decision tree
    auto& tree = AgenticDecisionTree::instance();
    tree.setGlobalConfidenceThreshold(cfg.confidenceThreshold);
}

AutonomyLoopConfig CLIAutonomyLoop::getConfig() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

// ============================================================================
// Statistics
// ============================================================================

const AutonomyLoopStats& CLIAutonomyLoop::getStats() const {
    return m_stats;
}

void CLIAutonomyLoop::resetStats() {
    m_stats.totalTicks.store(0);
    m_stats.failureDetections.store(0);
    m_stats.correctionsAttempted.store(0);
    m_stats.correctionsSucceeded.store(0);
    m_stats.correctionsFailed.store(0);
    m_stats.escalations.store(0);
    m_stats.rateLimitHits.store(0);
    m_stats.safetyBlocks.store(0);
    m_stats.errors.store(0);
    m_stats.uptimeMs.store(0);
}

// ============================================================================
// History
// ============================================================================

std::vector<AutonomyTickRecord> CLIAutonomyLoop::getRecentTicks(int count) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<AutonomyTickRecord> result;
    int start = (int)m_tickHistory.size() - count;
    if (start < 0) start = 0;
    for (int i = start; i < (int)m_tickHistory.size(); i++) {
        result.push_back(m_tickHistory[i]);
    }
    return result;
}

// ============================================================================
// Callbacks
// ============================================================================

void CLIAutonomyLoop::registerEventCallback(AutonomyEventCallback cb, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_eventCallbacks.push_back({cb, userData});
}

void CLIAutonomyLoop::unregisterEventCallback(AutonomyEventCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_eventCallbacks.erase(
        std::remove_if(m_eventCallbacks.begin(), m_eventCallbacks.end(),
                        [cb](const EventCB& e) { return e.fn == cb; }),
        m_eventCallbacks.end());
}

void CLIAutonomyLoop::emitEvent(AutonomyEvent event, const std::string& detail) {
    for (const auto& cb : m_eventCallbacks) {
        cb.fn(event, detail.c_str(), cb.userData);
    }
}

// ============================================================================
// Rate Limiting
// ============================================================================

bool CLIAutonomyLoop::rateLimitAllow() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - m_minuteWindowStart).count();

    if (elapsed >= 60) {
        m_minuteWindowStart = now;
        m_actionsThisMinute = 0;
    }

    return (m_actionsThisMinute < m_config.maxActionsPerMinute);
}

void CLIAutonomyLoop::rateLimitRecord() {
    m_actionsThisMinute++;
}

// ============================================================================
// Status
// ============================================================================

std::string CLIAutonomyLoop::getStatusString() const {
    const char* stateStr = "unknown";
    switch (m_state.load()) {
        case AutonomyLoopState::Idle:     stateStr = "idle"; break;
        case AutonomyLoopState::Starting: stateStr = "starting"; break;
        case AutonomyLoopState::Running:  stateStr = "running"; break;
        case AutonomyLoopState::Paused:   stateStr = "paused"; break;
        case AutonomyLoopState::Stopping: stateStr = "stopping"; break;
        case AutonomyLoopState::Error:    stateStr = "error"; break;
    }

    std::ostringstream oss;
    oss << "Autonomy: " << stateStr
        << " | ticks=" << m_stats.totalTicks.load()
        << " | failures=" << m_stats.failureDetections.load()
        << " | corrections=" << m_stats.correctionsSucceeded.load()
        << "/" << m_stats.correctionsAttempted.load()
        << " | escalations=" << m_stats.escalations.load();
    return oss.str();
}

std::string CLIAutonomyLoop::getDetailedStatus() const {
    std::ostringstream oss;

    const char* stateStr = "unknown";
    switch (m_state.load()) {
        case AutonomyLoopState::Idle:     stateStr = "IDLE"; break;
        case AutonomyLoopState::Starting: stateStr = "STARTING"; break;
        case AutonomyLoopState::Running:  stateStr = "RUNNING"; break;
        case AutonomyLoopState::Paused:   stateStr = "PAUSED"; break;
        case AutonomyLoopState::Stopping: stateStr = "STOPPING"; break;
        case AutonomyLoopState::Error:    stateStr = "ERROR"; break;
    }

    oss << "\n╔══════════════════════════════════════════════════╗\n";
    oss << "║        CLI AUTONOMY LOOP — " << stateStr << "\n";
    oss << "╚══════════════════════════════════════════════════╝\n\n";

    oss << "📊 Statistics:\n";
    oss << "  Total ticks:       " << m_stats.totalTicks.load() << "\n";
    oss << "  Failures detected: " << m_stats.failureDetections.load() << "\n";
    oss << "  Corrections OK:    " << m_stats.correctionsSucceeded.load() << "\n";
    oss << "  Corrections FAIL:  " << m_stats.correctionsFailed.load() << "\n";
    oss << "  Escalations:       " << m_stats.escalations.load() << "\n";
    oss << "  Rate limit hits:   " << m_stats.rateLimitHits.load() << "\n";
    oss << "  Safety blocks:     " << m_stats.safetyBlocks.load() << "\n";
    oss << "  Uptime:            " << (m_stats.uptimeMs.load() / 1000) << "s\n";

    oss << "\n⚙️  Configuration:\n";
    oss << "  Tick interval:     " << m_config.tickIntervalMs << "ms\n";
    oss << "  Rate limit:        " << m_config.maxActionsPerMinute << "/min\n";
    oss << "  Max consecutive:   " << m_config.maxConsecutiveFailures << "\n";
    oss << "  Confidence min:    " << m_config.confidenceThreshold << "\n";
    oss << "  SSA lift:          " << (m_config.enableSSALift ? "ON" : "OFF") << "\n";
    oss << "  Memory patch:      " << (m_config.enableMemoryPatch ? "ON" : "OFF") << "\n";
    oss << "  Byte patch:        " << (m_config.enableBytePatch ? "ON" : "OFF") << "\n";
    oss << "  Server patch:      " << (m_config.enableServerPatch ? "ON" : "OFF") << "\n";
    oss << "  Pause on escalate: " << (m_config.pauseOnEscalation ? "YES" : "NO") << "\n";
    oss << "  Verbose tracing:   " << (m_config.verboseTracing ? "ON" : "OFF") << "\n";

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        oss << "\n📋 Recent Ticks (last "
            << std::min((size_t)10, m_tickHistory.size()) << "):\n";
        int start = (int)m_tickHistory.size() - 10;
        if (start < 0) start = 0;
        for (int i = start; i < (int)m_tickHistory.size(); i++) {
            const auto& t = m_tickHistory[i];
            oss << "  [" << t.tickId << "] "
                << (t.correctionSucceeded ? "✅" : (t.failureDetected ? "❌" : "🟢"))
                << " " << t.summary << " (" << t.durationMs << "ms)\n";
        }

        size_t queueSize;
        {
            std::lock_guard<std::mutex> qlock(const_cast<std::mutex&>(m_queueMutex));
            queueSize = m_outputQueue.size();
        }
        oss << "\n📬 Output queue: " << queueSize << " pending\n";

        if (m_hasLastFailure) {
            oss << "\n⚠️  Last failure available for !auto_patch: "
                << m_lastFailureCtx.failureDescription << "\n";
        }
    }

    // Decision tree info
    auto& tree = AgenticDecisionTree::instance();
    const auto& treeStats = tree.getStats();
    oss << "\n🌳 Decision Tree Stats:\n";
    oss << "  Trees evaluated:   " << treeStats.treesEvaluated.load() << "\n";
    oss << "  Nodes visited:     " << treeStats.nodesVisited.load() << "\n";
    oss << "  SSA lifts:         " << treeStats.ssaLiftsPerformed.load() << "\n";
    oss << "  Patches applied:   " << treeStats.patchesApplied.load() << "\n";
    oss << "  Patches reverted:  " << treeStats.patchesReverted.load() << "\n";

    return oss.str();
}

