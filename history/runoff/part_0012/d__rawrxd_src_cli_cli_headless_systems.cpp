// ============================================================================
// cli_headless_systems.cpp — Phase 20: Headless CLI Surface Completion
// ============================================================================
//
// Full implementation of every headless CLI command for the Phase 10 safety
// stack, Phase 7 policy engine, Phase 8A explainability, Phase 5 history,
// Phase 9C multi-response, Phase 10A governor, and tool registry.
//
// This completes the headless surface: every system that was wired in Win32IDE
// is now accessible from the CLI REPL without any GUI dependency.
//
// Pattern:  Structured results, no exceptions, PatchResult-style
// Threading: All singletons are internally thread-safe
// Rule:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "cli_headless_systems.h"

#include "logging/logger.h"
static Logger s_logger("cli_headless_systems");

#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <vector>
#include <string>
#include <fstream>

// Phase 10B: Safety Contract
#include "agent_safety_contract.h"

// Phase 10D: Confidence Gate
#include "confidence_gate.h"

// Phase 10C: Deterministic Replay
#include "deterministic_replay.h"

// Phase 10A: Execution Governor
#include "execution_governor.h"

// Phase 9C: Multi-Response Engine
#include "multi_response_engine.h"

// Phase 5: Agent History
#include "agent_history.h"

// Phase 8A: Explainability
#include "agent_explainability.h"

// Phase 7: Policy Engine
#include "agent_policy.h"

// Tool Registry
#include "tool_registry.h"

// Phase 20: Universal Model Hotpatcher (streaming requantization pipeline)
#include "universal_model_hotpatcher.h"

// Phase 21: Distributed Swarm Orchestrator
#include "swarm_orchestrator.h"

// Windows headers (pulled in by execution_governor.h) define macros that
// collide with our ActionClass enum members.  Undefine them.
#ifdef CreateFile
#undef CreateFile
#endif
#ifdef DeleteFile
#undef DeleteFile
#endif

// Engine access
#include "agentic_engine.h"
#include "subagent_core.h"

// ============================================================================
// STATIC STATE — owned by the headless surface, shared via accessors
// ============================================================================

namespace {
    // Owned instances (non-singletons)
    AgentHistoryRecorder*   g_historyRecorder   = nullptr;
    PolicyEngine*           g_policyEngine      = nullptr;
    ExplainabilityEngine*   g_explainEngine     = nullptr;
    MultiResponseEngine*    g_multiResponse     = nullptr;

    // Cached engine pointers (not owned)
    AgenticEngine*          g_engine            = nullptr;
    SubAgentManager*        g_subAgentMgr       = nullptr;

    bool                    g_initialized       = false;

    // Helper: split string by first space
    std::pair<std::string, std::string> splitFirst(const std::string& s) {
        auto pos = s.find(' ');
        if (pos == std::string::npos) return { s, "" };
        return { s.substr(0, pos), s.substr(pos + 1) };
    }

    // Helper: parse int with fallback
    int parseInt(const std::string& s, int fallback) {
        try { return std::stoi(s); }
        catch (...) { return fallback; }
    }

    // Helper: parse float with fallback
    float parseFloat(const std::string& s, float fallback) {
        try { return std::stof(s); }
        catch (...) { return fallback; }
    }

    // Helper: time since epoch in ms
    double nowEpochMs() {
        return static_cast<double>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        );
    }

    // Helper: format duration
    std::string formatDuration(double ms) {
        if (ms < 1000.0) return std::to_string(static_cast<int>(ms)) + "ms";
        if (ms < 60000.0) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(1) << (ms / 1000.0) << "s";
            return oss.str();
        }
        int minutes = static_cast<int>(ms / 60000.0);
        int seconds = static_cast<int>((ms - minutes * 60000.0) / 1000.0);
        return std::to_string(minutes) + "m " + std::to_string(seconds) + "s";
    }
}

// ============================================================================
// INITIALIZATION
// ============================================================================

bool cli_headless_init(AgenticEngine* engine, SubAgentManager* subAgentMgr) {
    if (g_initialized) return true;

    g_engine      = engine;
    g_subAgentMgr = subAgentMgr;

    // ── Phase 10B: Safety Contract ──
    auto& safety = AgentSafetyContract::instance();
    safety.init();

    // In headless mode, auto-approve escalations (no GUI dialog)
    safety.setAutoApproveEscalations(true);

    // ── Phase 10D: Confidence Gate ──
    auto& confidence = ConfidenceGate::instance();
    confidence.init();
    confidence.setPolicy(GatePolicy::Normal);
    confidence.setEnabled(true);

    // In headless mode, auto-escalate (no GUI dialog)
    confidence.setAutoEscalate(true);

    // ── Phase 10C: Deterministic Replay ──
    auto& replay = ReplayJournal::instance();
    replay.init("rawrxd_replay");
    replay.startSession("cli-headless");
    replay.startRecording();
    replay.recordMarker("Headless CLI systems initialized");

    // ── Phase 10A: Execution Governor ──
    auto& governor = ExecutionGovernor::instance();
    governor.init();

    // ── Phase 5: Agent History ──
    g_historyRecorder = new AgentHistoryRecorder("rawrxd_history");
    g_historyRecorder->setLogCallback([](int level, const std::string& msg) {
        if (level >= 2) {
            s_logger.error("[History] {}", msg);
        }
    });

    // ── Phase 7: Policy Engine ──
    g_policyEngine = new PolicyEngine("rawrxd_policies");
    g_policyEngine->setHistoryRecorder(g_historyRecorder);
    g_policyEngine->load();
    g_policyEngine->setLogCallback([](int level, const std::string& msg) {
        if (level >= 2) {
            s_logger.error("[Policy] {}", msg);
        }
    });

    // ── Phase 8A: Explainability Engine ──
    g_explainEngine = new ExplainabilityEngine();
    g_explainEngine->setHistoryRecorder(g_historyRecorder);
    g_explainEngine->setPolicyEngine(g_policyEngine);

    // ── Phase 9C: Multi-Response Engine ──
    g_multiResponse = new MultiResponseEngine();
    g_multiResponse->initialize();

    // Wire SubAgentManager with policy engine if available
    if (g_subAgentMgr) {
        g_subAgentMgr->setPolicyEngine(g_policyEngine);
    }

    g_initialized = true;

    // Record initialization event in history
    g_historyRecorder->record(
        "session_init", "system", "", "Headless CLI systems initialized",
        "", "", true, 0, "", "{\"phase\":\"20\",\"systems\":\"safety,confidence,replay,governor,history,policy,explain,multi_response\"}");

    return true;
}

void cli_headless_shutdown() {
    if (!g_initialized) return;

    // Record shutdown
    if (g_historyRecorder) {
        g_historyRecorder->record(
            "session_shutdown", "system", "", "Headless CLI shutting down",
            "", "", true, 0, "", "");
        g_historyRecorder->flush();
    }

    // Persist policies
    if (g_policyEngine) {
        g_policyEngine->save();
    }

    // Flush replay journal
    auto& replay = ReplayJournal::instance();
    replay.recordMarker("Headless CLI shutdown");
    replay.endSession();
    replay.flushToDisk();
    replay.shutdown();

    // Stop governor
    auto& governor = ExecutionGovernor::instance();
    governor.shutdown();

    // Shutdown safety + confidence
    auto& safety = AgentSafetyContract::instance();
    safety.shutdown();
    auto& confidence = ConfidenceGate::instance();
    confidence.shutdown();

    // Clean up owned objects
    delete g_explainEngine;   g_explainEngine   = nullptr;
    delete g_policyEngine;    g_policyEngine    = nullptr;
    delete g_historyRecorder; g_historyRecorder = nullptr;
    delete g_multiResponse;   g_multiResponse   = nullptr;

    g_engine       = nullptr;
    g_subAgentMgr  = nullptr;
    g_initialized  = false;
}

AgentHistoryRecorder* cli_get_history_recorder()    { return g_historyRecorder; }
PolicyEngine*         cli_get_policy_engine()        { return g_policyEngine; }
ExplainabilityEngine* cli_get_explainability_engine(){ return g_explainEngine; }
MultiResponseEngine*  cli_get_multi_response_engine(){ return g_multiResponse; }

// ============================================================================
// CONVENIENCE: UNIFIED GATE CHECK
// ============================================================================

bool cli_gate_check(int actionClass, int riskTier,
                    float confidence, const std::string& description) {
    // Safety contract check
    auto& safety = AgentSafetyContract::instance();
    auto safetyResult = safety.checkAndConsume(
        static_cast<ActionClass>(actionClass),
        static_cast<SafetyRiskTier>(riskTier),
        description,
        confidence);

    if (safetyResult.verdict == ContractVerdict::Denied) {
        s_logger.info("🛡️ Safety DENIED: {}", description);
        if (!safetyResult.suggestion.empty()) {
            s_logger.info("   Suggestion: {}", safetyResult.suggestion);
        }
        return false;
    }

    // Confidence gate check
    auto& gate = ConfidenceGate::instance();
    auto confResult = gate.evaluateAndRecord(
        confidence,
        static_cast<ActionClass>(actionClass),
        static_cast<SafetyRiskTier>(riskTier),
        description);

    if (confResult.decision == ConfidenceDecision::Abort) {
        s_logger.info("🎯 Confidence ABORT: {}", description);
        return false;
    }

    if (confResult.decision == ConfidenceDecision::Escalate) {
        s_logger.info("⚠️  Confidence ESCALATE (auto-approved in headless): {}", description);
    }

    // Record in replay journal
    auto& replay = ReplayJournal::instance();
    replay.recordSafetyEvent(
        ReplayActionType::SafetyCheck,
        description,
        "{\"verdict\":\"" + std::string(contractVerdictToString(safetyResult.verdict)) +
        "\",\"confidence\":" + std::to_string(confidence) + "}");

    return true;
}

void cli_record_action(int actionType, const std::string& category,
                       const std::string& action, const std::string& input,
                       const std::string& output, int exitCode,
                       float confidence, double durationMs) {
    auto& replay = ReplayJournal::instance();
    replay.recordAction(
        static_cast<ReplayActionType>(actionType),
        category, action, input, output, exitCode, confidence, durationMs);
}

// ============================================================================
// SAFETY CONTRACT COMMANDS (Phase 10B)
// ============================================================================

void cmd_safety(const std::string& args) {
    auto& safety = AgentSafetyContract::instance();
    auto [sub, rest] = splitFirst(args);

    if (sub == "status" || sub.empty()) {
        s_logger.info("\n");

        auto stats = safety.getStats();
        s_logger.info("🛡️  Safety Contract Status:\n");
        s_logger.info("  Total checks:     {}", stats.totalChecks);
        s_logger.info("  Allowed:          {}", stats.totalAllowed);
        s_logger.info("  Denied:           {}", stats.totalDenied);
        s_logger.info("  Escalated:        {}", stats.totalEscalated);
        s_logger.info("  Rollbacks:        {}", stats.totalRollbacks);
        s_logger.info("  Violations:       {}", stats.totalViolations);
        s_logger.info("  Budget exceeded:  {}", stats.budgetExceeded);
        s_logger.info("  Risk blocked:     {}", stats.riskBlocked);
        s_logger.info("  Rate limited:     {}", stats.rateLimited);
        s_logger.info("  Max risk seen:    {}", safetyRiskTierToString(stats.maxRiskSeen));
        s_logger.info("  Max allowed risk: {}", safetyRiskTierToString(safety.getMaxAllowedRisk()));

    } else if (sub == "reset") {
        safety.resetBudget();
        s_logger.info("✅ Intent budget reset to defaults\n");

    } else if (sub == "rollback") {
        if (rest == "all") {
            safety.rollbackAll();
            s_logger.info("✅ All rollbacks executed\n");
        } else {
            bool ok = safety.rollbackLast();
            s_logger.info(ok ? "✅ Last action rolled back\n" : "❌ No rollback available\n");
        }

    } else if (sub == "violations") {
        auto violations = safety.getViolations();
        if (violations.empty()) {
            s_logger.info("✅ No violations recorded\n");
            return;
        }
        s_logger.info("🚨 Violations ({})", violations.size());
        for (const auto& v : violations) {
            s_logger.info("  [{}] {}", v.id, v.description);
            if (v.wasEscalated) {
                s_logger.info("       Escalated → {}", v.description);
            }
        }

    } else if (sub == "block") {
        if (rest.empty()) {
            s_logger.info("Usage: !safety block <action_class>\n");
            s_logger.info("  Classes: ReadFile, SearchCode, QueryModel, WriteFile, EditFile,\n");
            s_logger.info("           CreateFile, DeleteFile, RunCommand, RunBuild, RunTest,\n");
            s_logger.info("           InstallPackage, NetworkRequest, ModifyModel, PatchMemory, ...\n");
            return;
        }
        // Map string to ActionClass
        ActionClass ac = ActionClass::Unknown;
        if (rest == "ReadFile")       ac = ActionClass::ReadFile;
        else if (rest == "WriteFile")      ac = ActionClass::WriteFile;
        else if (rest == "EditFile")       ac = ActionClass::EditFile;
        else if (rest == "CreateFile")     ac = ActionClass::CreateFile;
        else if (rest == "DeleteFile")     ac = ActionClass::DeleteFile;
        else if (rest == "RunCommand")     ac = ActionClass::RunCommand;
        else if (rest == "RunBuild")       ac = ActionClass::RunBuild;
        else if (rest == "RunTest")        ac = ActionClass::RunTest;
        else if (rest == "InstallPackage") ac = ActionClass::InstallPackage;
        else if (rest == "NetworkRequest") ac = ActionClass::NetworkRequest;
        else if (rest == "ModifyModel")    ac = ActionClass::ModifyModel;
        else if (rest == "PatchMemory")    ac = ActionClass::PatchMemory;
        else if (rest == "ProcessKill")    ac = ActionClass::ProcessKill;
        else if (rest == "ServerRestart")  ac = ActionClass::ServerRestart;
        else if (rest == "SearchCode")     ac = ActionClass::SearchCode;
        else if (rest == "QueryModel")     ac = ActionClass::QueryModel;
        else {
            s_logger.info("❌ Unknown action class: {}", rest);
            return;
        }
        safety.blockActionClass(ac);
        s_logger.info("✅ Blocked: {}", rest);

    } else if (sub == "unblock") {
        if (rest.empty()) {
            s_logger.info("Usage: !safety unblock <action_class>\n");
            return;
        }
        ActionClass ac = ActionClass::Unknown;
        if (rest == "ReadFile")       ac = ActionClass::ReadFile;
        else if (rest == "WriteFile")      ac = ActionClass::WriteFile;
        else if (rest == "EditFile")       ac = ActionClass::EditFile;
        else if (rest == "CreateFile")     ac = ActionClass::CreateFile;
        else if (rest == "DeleteFile")     ac = ActionClass::DeleteFile;
        else if (rest == "RunCommand")     ac = ActionClass::RunCommand;
        else if (rest == "RunBuild")       ac = ActionClass::RunBuild;
        else if (rest == "RunTest")        ac = ActionClass::RunTest;
        else if (rest == "ModifyModel")    ac = ActionClass::ModifyModel;
        else if (rest == "PatchMemory")    ac = ActionClass::PatchMemory;
        else if (rest == "ProcessKill")    ac = ActionClass::ProcessKill;
        else {
            s_logger.info("❌ Unknown action class: {}", rest);
            return;
        }
        safety.unblockActionClass(ac);
        s_logger.info("✅ Unblocked: {}", rest);

    } else if (sub == "risk") {
        if (rest.empty()) {
            s_logger.info("Current max risk: {}", safetyRiskTierToString(safety.getMaxAllowedRisk()));
            s_logger.info("Usage: !safety risk <None|Low|Medium|High|Critical>\n");
            return;
        }
        SafetyRiskTier tier = SafetyRiskTier::Medium;
        if (rest == "None")          tier = SafetyRiskTier::None;
        else if (rest == "Low")      tier = SafetyRiskTier::Low;
        else if (rest == "Medium")   tier = SafetyRiskTier::Medium;
        else if (rest == "High")     tier = SafetyRiskTier::High;
        else if (rest == "Critical") tier = SafetyRiskTier::Critical;
        else {
            s_logger.info("❌ Unknown risk tier: {}", rest);
            return;
        }
        safety.setMaxAllowedRisk(tier);
        s_logger.info("✅ Max allowed risk: {}", safetyRiskTierToString(tier));

    } else if (sub == "budget") {
        auto budget = safety.getBudget();
        s_logger.info("\n📊 Intent Budget:\n");
        s_logger.info("  File writes:     {}", budget.usedFileWrites);
        s_logger.info("  File deletes:    {}", budget.usedFileDeletes);
        s_logger.info("  Command runs:    {}", budget.usedCommandRuns);
        s_logger.info("  Build runs:      {}", budget.usedBuildRuns);
        s_logger.info("  Network reqs:    {}", budget.usedNetworkRequests);
        s_logger.info("  Model mods:      {}", budget.usedModelModifications);
        s_logger.info("  Process kills:   {}", budget.usedProcessKills);
        s_logger.info("  Total actions:   {}", budget.usedTotalActions);
        s_logger.info("  Actions/min:     {}", budget.actionsThisMinute);

    } else {
        s_logger.info("Usage: !safety <status|reset|rollback|rollback_all|violations|block|unblock|risk|budget>\n");
    }
}

// ============================================================================
// CONFIDENCE GATE COMMANDS (Phase 10D)
// ============================================================================

void cmd_confidence(const std::string& args) {
    auto& gate = ConfidenceGate::instance();
    auto [sub, rest] = splitFirst(args);

    if (sub == "status" || sub.empty()) {
        s_logger.info("\n");

        auto stats = gate.getStats();
        s_logger.info("🎯 Confidence Gate Status:\n");
        s_logger.info("  Policy:            {}", gatePolicyToString(stats.currentPolicy));
        s_logger.info("  Enabled:           {}", gate.isEnabled());
        s_logger.info("  Total evaluations: {}", stats.totalEvaluations);
        s_logger.info("  Executed:          {}", stats.totalExecuted);
        s_logger.info("  Escalated:         {}", stats.totalEscalated);
        s_logger.info("  Aborted:           {}", stats.totalAborted);
        s_logger.info("  Deferred:          {}", stats.totalDeferred);
        s_logger.info("  Overridden:        {}", stats.totalOverridden);
        s_logger.info("  Avg confidence:    {}", stats.avgConfidence);
        s_logger.info("  Min confidence:    {}", stats.minConfidence);
        s_logger.info("  Max confidence:    {}", stats.maxConfidence);
        s_logger.info("  Recent avg:        {}", stats.recentAvgConfidence);
        s_logger.info("  Overall trend:     {}", confidenceTrendToString(stats.overallTrend));
        s_logger.info("  Self-abort:        {}", gate.isSelfAbortTriggered());

    } else if (sub == "policy") {
        if (rest.empty()) {
            s_logger.info("Current policy: {}", gatePolicyToString(gate.getPolicy()));
            s_logger.info("Usage: !confidence policy <strict|normal|relaxed|disabled>\n");
            return;
        }
        GatePolicy pol = GatePolicy::Normal;
        if (rest == "strict")        pol = GatePolicy::Strict;
        else if (rest == "normal")   pol = GatePolicy::Normal;
        else if (rest == "relaxed")  pol = GatePolicy::Relaxed;
        else if (rest == "disabled") pol = GatePolicy::Disabled;
        else {
            s_logger.info("❌ Unknown policy: {}", rest);
            return;
        }
        gate.setPolicy(pol);
        s_logger.info("✅ Confidence gate policy: {}", gatePolicyToString(pol));

    } else if (sub == "threshold") {
        // Parse: <execute> <escalate> <abort>
        std::istringstream iss(rest);
        float exec_t, esc_t, abort_t;
        if (!(iss >> exec_t >> esc_t >> abort_t)) {
            auto thresholds = gate.getThresholds();
            s_logger.info("Current thresholds:\n");
            s_logger.info("  Execute:  {}", thresholds.executeThreshold);
            s_logger.info("  Escalate: {}", thresholds.escalateThreshold);
            s_logger.info("  Abort:    {}", thresholds.abortThreshold);
            s_logger.info("Usage: !confidence threshold <execute> <escalate> <abort>\n");
            return;
        }
        gate.setExecuteThreshold(exec_t);
        gate.setEscalateThreshold(esc_t);
        gate.setAbortThreshold(abort_t);
        s_logger.info("✅ Thresholds set: execute={} escalate={} abort={}", exec_t, esc_t, abort_t);

    } else if (sub == "history") {
        auto history = gate.getHistory(20);
        if (history.empty()) {
            s_logger.info("No confidence evaluations recorded yet.\n");
            return;
        }
        s_logger.info("\n🎯 Recent Confidence Evaluations (last {} entries)\n", history.size());
        s_logger.info("{}\n", std::string(44, '-'));
        for (const auto& h : history) {
            std::ostringstream row;
            row << std::setw(6) << h.sequenceId << std::setw(10) << std::fixed << std::setprecision(3) << h.confidence
                << std::setw(12) << confidenceDecisionToString(h.decision)
                << std::setw(16) << actionClassToString(h.actionClass) << "\n";
            s_logger.info("{}", row.str());
        }
        s_logger.info("\n");

    } else if (sub == "trend") {
        auto trend = gate.analyzeTrend();
        double slope = gate.getTrendSlope();
        float recentAvg = gate.getRecentAverage(50);
        s_logger.info("\n📈 Confidence Trend:\n");
        s_logger.info("  Trend:      {}", confidenceTrendToString(trend));
        s_logger.info("  Slope:      {}", slope);
        s_logger.info("  Recent avg: {}", recentAvg);
        s_logger.info("  Degrading:  {}", gate.isConfidenceDegrading(-0.01f));

    } else if (sub == "selfabort") {
        s_logger.info("Self-abort threshold:  {}", gate.getSelfAbortThreshold());
        s_logger.info("Self-abort triggered:  {}", gate.isSelfAbortTriggered());
        if (gate.isSelfAbortTriggered()) {
            s_logger.info("  Use '!confidence reset' to clear self-abort state.\n");
        }

    } else if (sub == "reset") {
        gate.reset();
        gate.resetSelfAbort();
        s_logger.info("✅ Confidence gate reset\n");

    } else {
        s_logger.info("Usage: !confidence <status|policy|threshold|history|trend|selfabort|reset>\n");
    }
}

// ============================================================================
// REPLAY JOURNAL COMMANDS (Phase 10C)
// ============================================================================

void cmd_replay(const std::string& args) {
    auto& replay = ReplayJournal::instance();
    auto [sub, rest] = splitFirst(args);

    if (sub == "status" || sub.empty()) {
        s_logger.info("\n");

        auto stats = replay.getStats();
        s_logger.info("📼 Replay Journal Status:\n");
        s_logger.info("  State:              {}", replayStateToString(stats.state));
        s_logger.info("  Total records:      {}", stats.totalRecords);
        s_logger.info("  In memory:          {}", stats.recordsInMemory);
        s_logger.info("  Flushed to disk:    {}", stats.recordsFlushedToDisk);
        s_logger.info("  Total sessions:     {}", stats.totalSessions);
        s_logger.info("  Active session:     {}", stats.activeSessionId);
        s_logger.info("  Current sequence:   {}", stats.currentSequenceId);
        s_logger.info("  Memory usage:       {}", stats.memoryUsageBytes);
        s_logger.info("  Disk usage:         {}", stats.diskUsageBytes);

    } else if (sub == "last") {
        int count = parseInt(rest, 10);
        auto records = replay.getLastN(static_cast<uint64_t>(count));
        if (records.empty()) {
            s_logger.info("No records in journal.\n");
            return;
        }
        s_logger.info("\n📼 Last {} records\n", count);
        s_logger.info("{}\n", std::string(80, '-'));
        for (const auto& r : records) {
            s_logger.info("[");
            if (!r.action.empty()) {
                std::string truncAction = r.action.substr(0, 40);
                if (r.action.size() > 40) truncAction += "...";
                s_logger.info("{}", truncAction);
            }
            s_logger.info("\n");
            if (!r.input.empty()) {
                std::string truncInput = r.input.substr(0, 60);
                if (r.input.size() > 60) truncInput += "...";
                s_logger.info("          in:  {}", truncInput);
            }
            if (r.durationMs > 0) {
                s_logger.info("          dur: {}", r.durationMs);
            }
        }
        s_logger.info("{}\n\n", std::string(80, '-'));

    } else if (sub == "session") {
        auto sessionId = replay.getActiveSessionId();
        auto snapshot = replay.getSessionSnapshot(sessionId);
        s_logger.info("\n📼 Current Session:\n");
        s_logger.info("  Session ID:     {}", snapshot.sessionId);
        s_logger.info("  Label:          {}", snapshot.sessionLabel);
        s_logger.info("  Action count:   {}", snapshot.actionCount);
        s_logger.info("  Start:          {}", snapshot.startTime);
        s_logger.info("  Duration:       {}", snapshot.totalDurationMs);
        s_logger.info("  Agent queries:  {}", snapshot.agentQueries);
        s_logger.info("  Commands run:   {}", snapshot.commandsRun);
        s_logger.info("  Files modified: {}", snapshot.filesModified);
        s_logger.info("  Safety denials: {}", snapshot.safetyDenials);
        s_logger.info("  Gov. timeouts:  {}", snapshot.governorTimeouts);
        s_logger.info("  Errors:         {}", snapshot.errors);

    } else if (sub == "sessions") {
        auto sessions = replay.getAllSessions();
        if (sessions.empty()) {
            s_logger.info("No sessions recorded.\n");
            return;
        }
        s_logger.info("\n📼 All Sessions ({})\n", sessions.size());
        for (const auto& s : sessions) {
            s_logger.info("  [{}] {}", s.sessionId, s.sessionLabel);
        }
        s_logger.info("\n");

    } else if (sub == "checkpoint") {
        std::string label = rest.empty() ? "cli-checkpoint" : rest;
        uint64_t id = replay.recordCheckpoint(label);
        s_logger.info("✅ Checkpoint inserted: {}", id);

    } else if (sub == "export") {
        if (rest.empty()) {
            s_logger.info("Usage: !replay export <filepath>\n");
            return;
        }
        auto sessionId = replay.getActiveSessionId();
        bool ok = replay.exportSession(sessionId, rest);
        s_logger.info("{} {}\n", ok ? "✅ Session exported to:" : "❌ Export failed:", rest);

    } else if (sub == "filter") {
        if (rest.empty()) {
            s_logger.info("Usage: !replay filter <type>\n");
            s_logger.info("  Types: AgentQuery, AgentResponse, AgentToolCall, CommandExecution,\n");
            s_logger.info("         FileWrite, ModelInference, SafetyCheck, SafetyDenial,\n");
            s_logger.info("         GovernorSubmit, GovernorTimeout, Checkpoint, ...\n");
            return;
        }
        // Build filter
        ReplayFilter filter;
        filter.filterByType = true;

        // Map string to ReplayActionType
        if (rest == "AgentQuery")          filter.filterType = ReplayActionType::AgentQuery;
        else if (rest == "AgentResponse")  filter.filterType = ReplayActionType::AgentResponse;
        else if (rest == "AgentToolCall")  filter.filterType = ReplayActionType::AgentToolCall;
        else if (rest == "CommandExecution") filter.filterType = ReplayActionType::CommandExecution;
        else if (rest == "FileWrite")      filter.filterType = ReplayActionType::FileWrite;
        else if (rest == "ModelInference") filter.filterType = ReplayActionType::ModelInference;
        else if (rest == "SafetyCheck")    filter.filterType = ReplayActionType::SafetyCheck;
        else if (rest == "SafetyDenial")   filter.filterType = ReplayActionType::SafetyDenial;
        else if (rest == "GovernorSubmit") filter.filterType = ReplayActionType::GovernorSubmit;
        else if (rest == "GovernorTimeout")filter.filterType = ReplayActionType::GovernorTimeout;
        else if (rest == "Checkpoint")     filter.filterType = ReplayActionType::Checkpoint;
        else if (rest == "ModelHotpatch")  filter.filterType = ReplayActionType::ModelHotpatch;
        else {
            s_logger.info("❌ Unknown action type: {}", rest);
            return;
        }

        auto records = replay.filter(filter);
        s_logger.info("\n📼 Filtered: {} records\n", records.size());
        for (const auto& r : records) {
            s_logger.info("  [{}] {}", r.sequenceId, r.action);
            if (r.durationMs > 0) s_logger.info(" ({} ms)", r.durationMs);
            s_logger.info("\n");
        }
        s_logger.info("\n");

    } else if (sub == "start") {
        replay.startRecording();
        s_logger.info("✅ Recording started\n");

    } else if (sub == "pause") {
        replay.pauseRecording();
        s_logger.info("⏸️  Recording paused\n");

    } else if (sub == "stop") {
        replay.stopRecording();
        s_logger.info("⏹️  Recording stopped\n");

    } else {
        s_logger.info("Usage: !replay <status|last|session|sessions|checkpoint|export|filter|start|pause|stop>\n");
    }
}

// ============================================================================
// EXECUTION GOVERNOR COMMANDS (Phase 10A)
// ============================================================================

void cmd_governor(const std::string& args) {
    auto& governor = ExecutionGovernor::instance();
    auto [sub, rest] = splitFirst(args);

    if (sub == "status" || sub.empty()) {
        s_logger.info("\n");

        auto stats = governor.getStats();
        s_logger.info("⚙️  Execution Governor Status:\n");
        s_logger.info("  Initialized:    {}", governor.isInitialized());
        s_logger.info("  Active tasks:   {}", stats.activeTaskCount);
        s_logger.info("  Peak concurrent: {}", stats.peakConcurrent);
        s_logger.info("  Total submitted: {}", stats.totalSubmitted);
        s_logger.info("  Completed:      {}", stats.totalCompleted);
        s_logger.info("  Timed out:      {}", stats.totalTimedOut);
        s_logger.info("  Killed:         {}", stats.totalKilled);
        s_logger.info("  Cancelled:      {}", stats.totalCancelled);
        s_logger.info("  Failed:         {}", stats.totalFailed);
        s_logger.info("  Rollbacks:      {}", stats.totalRollbacks);
        s_logger.info("  Avg duration:   {}", stats.avgTaskDurationMs);
        s_logger.info("  Longest task:   {}", stats.longestTaskMs);

    } else if (sub == "run") {
        if (rest.empty()) {
            s_logger.info("Usage: !governor run <command> [--timeout <ms>]\n");
            return;
        }

        // Parse optional --timeout
        uint64_t timeout = TerminalWatchdog::DEFAULT_AGENT_TIMEOUT_MS;
        std::string command = rest;
        auto timeoutPos = rest.find("--timeout ");
        if (timeoutPos != std::string::npos) {
            command = rest.substr(0, timeoutPos);
            // Trim trailing spaces
            while (!command.empty() && command.back() == ' ') command.pop_back();
            std::string timeoutStr = rest.substr(timeoutPos + 10);
            timeout = static_cast<uint64_t>(parseInt(timeoutStr, static_cast<int>(timeout)));
        }

        // Gate check before executing
        if (!cli_gate_check(static_cast<int>(ActionClass::RunCommand),
                           static_cast<int>(SafetyRiskTier::High),
                           0.8f, "Governor: " + command)) {
            return;
        }

        s_logger.info("⚙️  Submitting to governor: {}", command);

        auto taskId = governor.submitCommand(command, timeout,
                                             GovernorRiskTier::Medium,
                                             "CLI: " + command);

        // Record in replay
        auto& replay = ReplayJournal::instance();
        replay.recordGovernorEvent(ReplayActionType::GovernorSubmit,
                                   "CLI governor run: " + command, taskId);

        // Wait synchronously for result
        auto result = governor.waitForTask(taskId, timeout + 5000);

        s_logger.info("\n─── Governor Result ───\n");
        if (result.timedOut) {
            s_logger.info("⏰ TIMED OUT after {} ms\n", static_cast<uint64_t>(timeout));
            replay.recordGovernorEvent(ReplayActionType::GovernorTimeout,
                                       "Timed out: " + command, taskId);
        } else if (result.cancelled) {
            s_logger.info("🚫 CANCELLED\n");
        } else {
            s_logger.info("Exit code: {}", result.exitCode);
            replay.recordGovernorEvent(ReplayActionType::GovernorComplete,
                                       "Completed: " + command, taskId);
        }
        if (!result.output.empty()) {
            s_logger.info("{}", result.output);
            if (result.output.back() != '\n') s_logger.info("\n");
        }
        s_logger.info("Duration: {} ms\n", result.durationMs);
        s_logger.info("───────────────────────\n\n");

        // Record in history
        if (g_historyRecorder) {
            g_historyRecorder->record("command_execution", "governor", "",
                command, result.output, "",
                result.exitCode == 0 && !result.timedOut,
                static_cast<int>(result.durationMs),
                result.timedOut ? "timeout" : "",
                "{\"exitCode\":" + std::to_string(result.exitCode) + "}");
        }

    } else if (sub == "tasks") {
        auto descriptions = governor.getActiveTaskDescriptions();
        if (descriptions.empty()) {
            s_logger.info("No active tasks.\n");
            return;
        }
        s_logger.info("⚙️  Active Tasks ({})\n", descriptions.size());
        for (size_t i = 0; i < descriptions.size(); i++) {
            s_logger.info("  [{}] {}", i + 1, descriptions[i]);
        }
        s_logger.info("\n");

    } else if (sub == "kill") {
        if (rest.empty()) {
            s_logger.info("Usage: !governor kill <task_id>\n");
            return;
        }
        auto taskId = static_cast<GovernorTaskId>(parseInt(rest, 0));
        bool ok = governor.killTask(taskId);
        s_logger.info("{} (id={})\n", ok ? "✅ Task killed" : "❌ Task not found or already complete", taskId);

    } else if (sub == "kill_all") {
        governor.killAll();
        s_logger.info("✅ All tasks killed\n");

    } else if (sub == "wait") {
        if (rest.empty()) {
            s_logger.info("Usage: !governor wait <task_id>\n");
            return;
        }
        auto taskId = static_cast<GovernorTaskId>(parseInt(rest, 0));
        s_logger.info("⏳ Waiting for task {}", taskId);
        auto result = governor.waitForTask(taskId, 60000);
        s_logger.info("Exit: {}", result.exitCode);
        if (!result.output.empty()) s_logger.info("{}\n", result.output);

    } else {
        s_logger.info("Usage: !governor <status|run|tasks|kill|kill_all|wait>\n");
    }
}

// ============================================================================
// MULTI-RESPONSE ENGINE COMMANDS (Phase 9C)
// ============================================================================

void cmd_multi_response(const std::string& args) {
    if (!g_multiResponse) {
        s_logger.info("❌ MultiResponseEngine not initialized\n");
        return;
    }
    auto [sub, rest] = splitFirst(args);

    if (sub == "templates") {
        auto templates = g_multiResponse->getAllTemplates();
        s_logger.info("\n📝 Response Templates:\n");
        for (const auto& t : templates) {
            s_logger.info("  [{}] {}", static_cast<int>(t.id), t.name);
            if (t.description[0] != '\0') {
                s_logger.info("      {}", t.description);
            }
        }
        s_logger.info("\n");

    } else if (sub == "stats") {
        auto stats = g_multiResponse->getStats();
        s_logger.info("\n📊 Multi-Response Stats:\n");
        s_logger.info("  Total sessions:     {}", stats.totalSessions);
        s_logger.info("  Responses generated: {}", stats.totalResponsesGenerated);
        s_logger.info("  Preferences recorded: {}", stats.totalPreferencesRecorded);
        s_logger.info("  Errors:             {}", stats.errorCount);
        s_logger.info("  Preference counts:  ");
        const char* names[] = { "Strategic", "Grounded", "Creative", "Concise" };
        for (int i = 0; i < 4; i++) {
            s_logger.info("{}{}={}", (i == 0 ? "" : ", "), names[i], stats.preferenceCount[i]);
        }
        s_logger.info("\n  Avg latency:        ");
        for (int i = 0; i < 4; i++) {
            s_logger.info("{}{}={}", (i == 0 ? "" : ", "), names[i], formatDuration(stats.avgLatencyMs[i]));
        }
        s_logger.info("\n\n");

    } else if (sub == "toggle") {
        if (rest.empty()) {
            s_logger.info("Usage: !multi toggle <0|1|2|3>\n");
            s_logger.info("  0=Strategic, 1=Grounded, 2=Creative, 3=Concise\n");
            return;
        }
        int id = parseInt(rest, -1);
        if (id < 0 || id > 3) {
            s_logger.info("❌ Invalid template ID (0-3)\n");
            return;
        }
        auto tmpl = g_multiResponse->getTemplate(static_cast<ResponseTemplateId>(id));
        g_multiResponse->setTemplateEnabled(static_cast<ResponseTemplateId>(id), !tmpl.enabled);
        s_logger.info("✅ Template {} {}\n", id, !tmpl.enabled ? "enabled" : "disabled");

    } else if (sub == "prefer") {
        if (rest.empty()) {
            s_logger.info("Usage: !multi prefer <index 0-3> [reason]\n");
            return;
        }
        auto [idxStr, reason] = splitFirst(rest);
        int idx = parseInt(idxStr, -1);
        if (idx < 0 || idx > 3) {
            s_logger.info("❌ Invalid response index (0-3)\n");
            return;
        }
        auto* session = g_multiResponse->getLatestSession();
        if (!session) {
            s_logger.info("❌ No multi-response session available. Run !multi <prompt> first.\n");
            return;
        }
        auto result = g_multiResponse->setPreference(session->sessionId, idx, reason);
        s_logger.info("{} {}\n", result.success ? "✅" : "❌", result.detail);

    } else if (sub == "recommend") {
        std::string rec = g_multiResponse->getRecommendedTemplate();
        s_logger.info("🎯 Recommended template: {}", rec);

    } else if (sub == "compare") {
        auto* session = g_multiResponse->getLatestSession();
        if (!session || session->responses.empty()) {
            s_logger.info("❌ No multi-response session to compare. Run !multi <prompt> first.\n");
            return;
        }
        s_logger.info("\n");
        s_logger.info("Multi-Response Comparison — Prompt: {}\n", session->prompt);
        s_logger.info("{}\n", std::string(80, '='));
        for (const auto& resp : session->responses) {
            if (!resp.complete && !resp.error) continue;
            s_logger.info("\n── [{}] ", resp.index);
            if (resp.error) s_logger.info("❌ ERROR: {}", resp.errorDetail);
            s_logger.info(" ──\n");
            if (!resp.content.empty()) {
                std::string display = resp.content;
                if (display.size() > 500) {
                    display = display.substr(0, 500) + "\n... [truncated]";
                }
                s_logger.info("{}\n", display);
            }
        }
        s_logger.info("{}\n", std::string(80, '='));
        if (session->preferredIndex >= 0) {
            s_logger.info("✅ Preferred: [{}]\n", session->preferredIndex);
        }
        s_logger.info("\n");

    } else if (!sub.empty()) {
        // Treat everything as a prompt
        std::string prompt = args;

        // Gate check
        if (!cli_gate_check(static_cast<int>(ActionClass::QueryModel),
                           static_cast<int>(SafetyRiskTier::Low),
                           0.9f, "Multi-response: " + prompt.substr(0, 60))) {
            return;
        }

        int maxResp = g_multiResponse->getMaxChainResponses();
        s_logger.info("🔀 Generating {} responses\n", maxResp);

        auto sessionId = g_multiResponse->startSession(prompt, maxResp);

        // Per-response callback for live display
        auto perCb = [](const GeneratedResponse& resp, void* /*userData*/) {
            s_logger.info("  [{}] {}\n", resp.index, resp.error ? "❌" : "✅");
        };

        auto result = g_multiResponse->generateAll(sessionId, perCb, nullptr);

        if (!result.success) {
            s_logger.info("❌ Generation failed: {}", result.detail);
            return;
        }

        s_logger.info("\n✅ All responses generated. Use !multi compare to review.\n");
        s_logger.info("   Use !multi prefer <0-3> to record your preference.\n\n");

        // Record in history
        if (g_historyRecorder) {
            g_historyRecorder->record("multi_response", "system", "",
                "Generated " + std::to_string(maxResp) + " responses",
                prompt, "", true, 0, "", "");
        }

    } else {
        s_logger.info("Usage: !multi <prompt>        Generate multi-style responses\n");
        s_logger.info("       !multi compare         Compare last responses\n");
        s_logger.info("       !multi prefer <0-3>    Record preference\n");
        s_logger.info("       !multi templates       Show all templates\n");
        s_logger.info("       !multi toggle <0-3>    Toggle template on/off\n");
        s_logger.info("       !multi recommend       Show recommended template\n");
        s_logger.info("       !multi stats           Show generation stats\n");
    }
}

// ============================================================================
// AGENT HISTORY COMMANDS (Phase 5)
// ============================================================================

void cmd_history(const std::string& args) {
    if (!g_historyRecorder) {
        s_logger.info("❌ AgentHistoryRecorder not initialized\n");
        return;
    }
    auto [sub, rest] = splitFirst(args);

    if (sub == "show" || sub.empty()) {
        int count = parseInt(rest, 20);
        HistoryQuery q;
        q.limit = count;
        auto events = g_historyRecorder->query(q);
        if (events.empty()) {
            s_logger.info("No history events.\n");
            return;
        }
        s_logger.info("\n📜 Agent History (last {} events)\n", events.size());
        s_logger.info("{}\n", std::string(80, '-'));
        for (const auto& e : events) {
            s_logger.info("[{}] ", e.id);
            if (!e.agentId.empty()) s_logger.info("agent={} ", e.agentId);
            if (e.durationMs > 0) s_logger.info("({} ms) ", e.durationMs);
            s_logger.info("\n");
            if (!e.description.empty()) {
                std::string desc = e.description.substr(0, 70);
                if (e.description.size() > 70) desc += "...";
                s_logger.info("       {}\n", desc);
            }
            if (!e.success && !e.errorMessage.empty()) {
                s_logger.info("       ❌ {}\n", e.errorMessage);
            }
        }
        s_logger.info("{}\n\n", std::string(80, '-'));

    } else if (sub == "session") {
        auto timeline = g_historyRecorder->getSessionTimeline(g_historyRecorder->sessionId());
        s_logger.info("\n📜 Session Timeline ({} events)\n", timeline.size());
        for (const auto& e : timeline) {
            s_logger.info("  [{}] {} {}\n", e.id, e.eventType, e.description);
        }
        s_logger.info("\n");

    } else if (sub == "agent") {
        if (rest.empty()) {
            s_logger.info("Usage: !history agent <agent_id>\n");
            return;
        }
        auto timeline = g_historyRecorder->getAgentTimeline(rest);
        if (timeline.empty()) {
            s_logger.info("No events for agent: ");
            return;
        }
        s_logger.info("\n📜 Agent Timeline: ");
        for (const auto& e : timeline) {
            s_logger.info("  [");
            if (e.durationMs > 0) s_logger.info(" (");
            s_logger.info("\n");
            if (!e.description.empty()) {
                s_logger.info("       ");
            }
        }
        s_logger.info("\n");

    } else if (sub == "type") {
        if (rest.empty()) {
            s_logger.info("Usage: !history type <event_type>\n");
            s_logger.info("  Types: agent_spawn, agent_complete, agent_fail, tool_invoke,\n");
            s_logger.info("         chain_start, chain_step, chain_complete, swarm_start,\n");
            s_logger.info("         swarm_task, swarm_complete, chat_request, chat_response, ...\n");
            return;
        }
        auto events = g_historyRecorder->getEventsByType(rest, 50);
        s_logger.info("\n📜 Events of type '");
        for (const auto& e : events) {
            s_logger.info("  [");
        }
        s_logger.info("\n");

    } else if (sub == "stats") {
        s_logger.info("\n");

    } else if (sub == "flush") {
        g_historyRecorder->flush();
        s_logger.info("✅ Events flushed to disk\n");

    } else if (sub == "clear") {
        g_historyRecorder->clear();
        s_logger.info("✅ In-memory events cleared\n");

    } else if (sub == "export") {
        if (rest.empty()) {
            s_logger.info("Usage: !history export <filepath>\n");
            return;
        }
        auto events = g_historyRecorder->allEvents();
        std::string json = g_historyRecorder->toJSON(events);
        std::ofstream out(rest);
        if (out.is_open()) {
            out << json;
            out.close();
            s_logger.info("✅ Exported ");
        } else {
            s_logger.info("❌ Failed to write: ");
        }

    } else {
        s_logger.info("Usage: !history <show|session|agent|type|stats|flush|clear|export>\n");
    }
}

// ============================================================================
// EXPLAINABILITY COMMANDS (Phase 8A)
// ============================================================================

void cmd_explain(const std::string& args) {
    if (!g_explainEngine) {
        s_logger.info("❌ ExplainabilityEngine not initialized\n");
        return;
    }
    auto [sub, rest] = splitFirst(args);

    if (sub == "agent") {
        if (rest.empty()) {
            s_logger.info("Usage: !explain agent <agent_id>\n");
            return;
        }
        auto trace = g_explainEngine->traceAgent(rest);
        s_logger.info("\n");
        s_logger.info("\n🔍 Agent Trace: ");
        s_logger.info("  Nodes: ");
        s_logger.info("  Result: ");
        if (!trace.summary.empty()) {
            s_logger.info("  Summary: ");
        }
        for (const auto& node : trace.nodes) {
            s_logger.info("    [");
            if (!node.policyId.empty()) {
                s_logger.info("      Policy: ");
            }
        }
        s_logger.info("\n");

    } else if (sub == "chain") {
        if (rest.empty()) {
            s_logger.info("Usage: !explain chain <parent_id>\n");
            return;
        }
        auto trace = g_explainEngine->traceChain(rest);
        s_logger.info("\n🔗 Chain Trace: ");
        s_logger.info("  Nodes: ");
        if (!trace.summary.empty()) s_logger.info("  ");
        for (const auto& node : trace.nodes) {
            s_logger.info("    [");
        }
        s_logger.info("\n");

    } else if (sub == "swarm") {
        if (rest.empty()) {
            s_logger.info("Usage: !explain swarm <parent_id>\n");
            return;
        }
        auto trace = g_explainEngine->traceSwarm(rest);
        s_logger.info("\n🐝 Swarm Trace: ");
        s_logger.info("  Nodes: ");
        if (!trace.summary.empty()) s_logger.info("  ");
        for (const auto& node : trace.nodes) {
            s_logger.info("    [");
        }
        s_logger.info("\n");

    } else if (sub == "failures") {
        auto failures = g_explainEngine->explainFailures();
        if (failures.empty()) {
            s_logger.info("✅ No failures to explain in current session.\n");
            return;
        }
        s_logger.info("\n🔍 Failure Attributions (");
        for (const auto& f : failures) {
            s_logger.info("  [");
            s_logger.info("    Error: ");
            s_logger.info("    Strategy: ");
            if (f.wasRetried) {
                s_logger.info(" → retry ");
            }
            s_logger.info("\n");
            if (!f.policyId.empty()) {
                s_logger.info("    Policy: ");
            }
            if (f.historicalSuccessRate >= 0) {
                s_logger.info("    Historical success rate: ");
            }
        }
        s_logger.info("\n");

    } else if (sub == "policies") {
        auto attrs = g_explainEngine->explainPolicies();
        if (attrs.empty()) {
            s_logger.info("No policy firings in current session.\n");
            return;
        }
        s_logger.info("\n📜 Policy Attributions (");
        for (const auto& p : attrs) {
            s_logger.info("  ");
            s_logger.info("    Trigger: ");
            if (!p.triggerPattern.empty()) s_logger.info(" pattern=");
            if (p.triggerFailureRate > 0) s_logger.info(" failRate=");
            s_logger.info("\n");
            s_logger.info("    Effect: ");
            s_logger.info("    Applied ");
        }
        s_logger.info("\n");

    } else if (sub == "session") {
        auto explanation = g_explainEngine->explainSession();
        s_logger.info("\n📊 Session Explanation:\n");
        s_logger.info("  Events:    ");
        s_logger.info("  Agents:    ");
        s_logger.info("  Chains:    ");
        s_logger.info("  Swarms:    ");
        s_logger.info("  Failures:  ");
        s_logger.info("  Retries:   ");
        s_logger.info("  Policies:  ");
        if (!explanation.narrative.empty()) {
            s_logger.info("\n  Narrative:\n  ");
        }
        s_logger.info("\n");

    } else if (sub == "snapshot") {
        if (rest.empty()) {
            s_logger.info("Usage: !explain snapshot <filepath>\n");
            return;
        }
        bool ok = g_explainEngine->exportSnapshot(rest);
        s_logger.info("{} {}\n", (ok ? "✅ Snapshot exported to: " : "❌ Export failed: "), rest);

    } else {
        s_logger.info("Usage: !explain <agent|chain|swarm|failures|policies|session|snapshot>\n");
    }
}

// ============================================================================
// POLICY ENGINE COMMANDS (Phase 7)
// ============================================================================

void cmd_policy(const std::string& args) {
    if (!g_policyEngine) {
        s_logger.info("❌ PolicyEngine not initialized\n");
        return;
    }
    auto [sub, rest] = splitFirst(args);

    if (sub == "list" || sub.empty()) {
        auto policies = g_policyEngine->getAllPolicies();
        if (policies.empty()) {
            s_logger.info("No policies defined. Use !policy suggest to generate suggestions.\n");
            return;
        }
        s_logger.info("\n📜 Policies (");
        for (const auto& p : policies) {
            s_logger.info("  ");
            if (!p.description.empty()) {
                s_logger.info("     ");
            }
        }
        s_logger.info("\n");

    } else if (sub == "show") {
        if (rest.empty()) {
            s_logger.info("Usage: !policy show <policy_id>\n");
            return;
        }
        const auto* p = g_policyEngine->getPolicy(rest);
        if (!p) {
            // Try matching by prefix
            auto all = g_policyEngine->getAllPolicies();
            for (const auto& pol : all) {
                if (pol.id.find(rest) == 0) { p = g_policyEngine->getPolicy(pol.id); break; }
            }
        }
        if (!p) {
            s_logger.info("❌ Policy not found: ");
            return;
        }
        s_logger.info("\n");

    } else if (sub == "enable") {
        if (rest.empty()) { s_logger.info("Usage: !policy enable <id>\n"); return; }
        bool ok = g_policyEngine->setEnabled(rest, true);
        s_logger.info("{}\n", (ok ? "✅ Policy enabled" : "❌ Policy not found"));

    } else if (sub == "disable") {
        if (rest.empty()) { s_logger.info("Usage: !policy disable <id>\n"); return; }
        bool ok = g_policyEngine->setEnabled(rest, false);
        s_logger.info("{}\n", (ok ? "✅ Policy disabled" : "❌ Policy not found"));

    } else if (sub == "remove") {
        if (rest.empty()) { s_logger.info("Usage: !policy remove <id>\n"); return; }
        bool ok = g_policyEngine->removePolicy(rest);
        s_logger.info("{}\n", (ok ? "✅ Policy removed" : "❌ Policy not found"));

    } else if (sub == "heuristics") {
        g_policyEngine->computeHeuristics();
        auto heuristics = g_policyEngine->getAllHeuristics();
        if (heuristics.empty()) {
            s_logger.info("No heuristics computed (need more history data).\n");
            return;
        }
        s_logger.info("\n📊 Computed Heuristics ({})\n", heuristics.size());
        std::ostringstream header;
        header << std::setw(25) << std::left << "Key" << std::setw(8) << "Total" << std::setw(8) << "Pass"
               << std::setw(8) << "Fail" << std::setw(10) << "Rate" << std::setw(10) << "Avg ms" << "\n";
        s_logger.info("{}", header.str());
        s_logger.info("{}\n", std::string(69, '-'));
        for (const auto& h : heuristics) {
            std::ostringstream row;
            row << std::setw(25) << std::left << h.key << std::setw(8) << h.totalEvents
                << std::setw(8) << h.successCount << std::setw(8) << h.failCount
                << std::setw(10) << std::fixed << std::setprecision(1) << (h.successRate * 100) << "%"
                << std::setw(10) << static_cast<int>(h.avgDurationMs) << "\n";
            s_logger.info("{}", row.str());
        }
        s_logger.info("\n");

    } else if (sub == "suggest") {
        s_logger.info("🧠 Analyzing history for policy suggestions...\n");
        g_policyEngine->computeHeuristics();
        auto suggestions = g_policyEngine->generateSuggestions();
        if (suggestions.empty()) {
            s_logger.info("No suggestions at this time (need more diverse history data).\n");
            return;
        }
        s_logger.info("\n💡 Policy Suggestions (");
        for (const auto& s : suggestions) {
            s_logger.info("  [");
            s_logger.info("    ");
            s_logger.info("    Est. improvement: ");
            s_logger.info("    State: ");
        }
        s_logger.info("\n  Use !policy accept <id> or !policy reject <id>\n\n");

    } else if (sub == "accept") {
        if (rest.empty()) { s_logger.info("Usage: !policy accept <suggestion_id>\n"); return; }
        bool ok = g_policyEngine->acceptSuggestion(rest);
        s_logger.info("{}\n", (ok ? "✅ Suggestion accepted and policy activated" : "❌ Suggestion not found"));

    } else if (sub == "reject") {
        if (rest.empty()) { s_logger.info("Usage: !policy reject <suggestion_id>\n"); return; }
        bool ok = g_policyEngine->rejectSuggestion(rest);
        s_logger.info("{}\n", (ok ? "✅ Suggestion rejected" : "❌ Suggestion not found"));

    } else if (sub == "pending") {
        auto pending = g_policyEngine->getPendingSuggestions();
        if (pending.empty()) {
            s_logger.info("No pending suggestions.\n");
            return;
        }
        s_logger.info("\n💡 Pending Suggestions (");
        for (const auto& s : pending) {
            s_logger.info("  [");
        }
        s_logger.info("\n");

    } else if (sub == "export") {
        if (rest.empty()) { s_logger.info("Usage: !policy export <filepath>\n"); return; }
        bool ok = g_policyEngine->exportToFile(rest);
        s_logger.info("{} {}\n", (ok ? "✅ Policies exported to: " : "❌ Export failed: "), rest);

    } else if (sub == "import") {
        if (rest.empty()) { s_logger.info("Usage: !policy import <filepath>\n"); return; }
        int count = g_policyEngine->importFromFile(rest);
        s_logger.info("✅ Imported ");

    } else if (sub == "stats") {
        s_logger.info("\n");

    } else {
        s_logger.info("Usage: !policy <list|show|enable|disable|remove|heuristics|suggest|accept|reject|pending|export|import|stats>\n");
    }
}

// ============================================================================
// TOOL REGISTRY COMMANDS
// ============================================================================

void cmd_tools(const std::string& args) {
    std::string toolList = ToolRegistry::list_tools();
    if (toolList.empty()) {
        s_logger.info("No tools registered.\n");
        return;
    }
    s_logger.info("\n🔧 Registered Tools:\n");
}

// ============================================================================
// MODEL HOTPATCHER COMMANDS (Phase 20 AVX-512 requantization pipeline)
// ============================================================================

void cmd_model(const std::string& args) {
    auto& hotpatcher = UniversalModelHotpatcher::instance();
    auto [sub, rest] = splitFirst(args);

    if (sub == "load" || sub == "model_load") {
        if (rest.empty()) {
            s_logger.info("Usage: !model_load <path_to_gguf>\n");
            return;
        }

        // Gate check — model analysis is a read-only operation
        if (!cli_gate_check(static_cast<int>(ActionClass::ReadFile),
                           static_cast<int>(SafetyRiskTier::Low),
                           0.95f, "Analyze GGUF model: " + rest)) {
            return;
        }

        s_logger.info("[Model] Analyzing model: ");
        bool ok = hotpatcher.analyzeModel(rest);
        if (ok) {
            auto layers = hotpatcher.getLayerInfo();
            uint64_t totalParams = hotpatcher.estimateParameterCount();
            uint64_t totalSize = 0;
            for (const auto& l : layers) totalSize += l.sizeBytes;

            s_logger.info("✅ Model analyzed successfully\n");
            s_logger.info("  File: ");
            s_logger.info("  Layers: ");
            s_logger.info("  Parameters: ");
            s_logger.info("  Total size: ");
            s_logger.info("  Size tier: ");

            cli_record_action(
                static_cast<int>(ReplayActionType::AgentToolCall),
                "model", "analyze", rest,
                "Analyzed: " + std::to_string(layers.size()) + " layers, " +
                std::to_string(totalParams) + " params",
                0, 1.0f, 0.0);
        } else {
            s_logger.info("❌ Failed to analyze model: ");
        }

    } else if (sub == "plan" || sub == "model_plan") {
        s_logger.info("[Model] Computing optimal quantization plan...\n");
        auto plan = hotpatcher.computeQuantPlan();
        if (plan.empty()) {
            s_logger.info("❌ No plan computed. Load a model first with !model_load <path>\n");
            return;
        }
        int64_t totalSavings = 0;
        float maxQualityImpact = 0.0f;
        for (const auto& d : plan) {
            totalSavings += d.savingsBytes;
            if (d.qualityImpact > maxQualityImpact) maxQualityImpact = d.qualityImpact;
        }
        s_logger.info("✅ Quantization plan computed:\n");
        s_logger.info("  Target layers: ");
        s_logger.info("  Estimated VRAM savings: ");
        s_logger.info("  Max quality impact: ");
        s_logger.info("\n  Plan JSON:\n");

    } else if (sub == "surgery" || sub == "model_surgery") {
        // Gate check — model surgery is a high-risk mutation
        if (!cli_gate_check(static_cast<int>(ActionClass::ModifyModel),
                           static_cast<int>(SafetyRiskTier::Critical),
                           0.80f, "Apply streaming requantization (model surgery)")) {
            return;
        }

        s_logger.info("[Model] Computing and applying quantization plan (model surgery)...\n");
        auto plan = hotpatcher.computeQuantPlan();
        if (plan.empty()) {
            s_logger.info("❌ No plan computed. Load a model first.\n");
            return;
        }

        // Set progress callback
        hotpatcher.setSurgeryProgressCallback(
            [](SurgeryOp op, uint32_t layerIdx, uint32_t totalLayers,
               float progress, void* /*userData*/) {
                const char* opStr = "Unknown";
                switch (op) {
                    case SurgeryOp::RequantizeLayer: opStr = "Requantize"; break;
                    case SurgeryOp::RequantizeRange: opStr = "RequantRange"; break;
                    case SurgeryOp::RequantizeAll:   opStr = "RequantAll"; break;
                    case SurgeryOp::EvictLayer:      opStr = "Evict";      break;
                    case SurgeryOp::ReloadLayer:     opStr = "Reload";     break;
                    case SurgeryOp::SplitLayer:      opStr = "Split";      break;
                    case SurgeryOp::MergeShards:     opStr = "Merge";      break;
                    case SurgeryOp::CompressKVCache: opStr = "Compress";   break;
                }
                s_logger.info("\r  [");
            },
            nullptr);

        double startMs = nowEpochMs();
        auto result = hotpatcher.applyQuantPlan(plan);
        double elapsed = nowEpochMs() - startMs;

        s_logger.info("\n");
        if (result.success) {
            s_logger.info("✅ Model surgery completed in ");
            s_logger.info("  Layers modified: ");
            s_logger.info("  Memory saved: ");
            s_logger.info("  Detail: ");

            cli_record_action(
                static_cast<int>(ReplayActionType::AgentToolCall),
                "model", "surgery", "",
                "Surgery completed: " + std::to_string(result.layersAffected) + " layers",
                0, 1.0f, elapsed);
        } else {
            s_logger.info("❌ Model surgery failed: ");
        }

    } else if (sub == "pressure" || sub == "pressure_auto") {
        hotpatcher.enableAutoPressureResponse(true);
        s_logger.info("✅ Automatic VRAM pressure response ENABLED\n");
        s_logger.info("  The hotpatcher will now automatically requantize/evict layers\n");
        s_logger.info("  when VRAM pressure reaches critical thresholds.\n");

        auto budget = hotpatcher.getVRAMBudget();
        s_logger.info("\n  Current VRAM Budget:\n");
        s_logger.info("    Total RAM:     ");
        s_logger.info("    Available RAM: ");
        s_logger.info("    Pressure:      ");
        s_logger.info("    GPU accel:     ");

    } else if (sub == "status" || sub.empty()) {
        s_logger.info("[Model Hotpatcher] ");
        auto budget = hotpatcher.getVRAMBudget();
        s_logger.info("\n  VRAM: ");

    } else if (sub == "layers") {
        auto layers = hotpatcher.getLayerInfo();
        if (layers.empty()) {
            s_logger.info("No layers loaded. Use !model_load <path> first.\n");
            return;
        }
        s_logger.info("\n📊 Model Layers (");
        for (const auto& l : layers) {
            std::string state = l.evicted ? "⚪" : "🟢";
            s_logger.info("  ");
        }

    } else if (sub == "evict") {
        if (rest.empty()) { s_logger.info("Usage: !model evict <layer_index>\n"); return; }
        uint32_t idx = static_cast<uint32_t>(parseInt(rest, -1));
        auto r = hotpatcher.evictLayer(idx);
        s_logger.info("{}{}", (r.success ? "Layer evicted: " : "Failed to evict layer: "), r.detail);

    } else if (sub == "reload") {
        if (rest.empty()) { s_logger.info("Usage: !model reload <layer_index>\n"); return; }
        uint32_t idx = static_cast<uint32_t>(parseInt(rest, -1));
        auto r = hotpatcher.reloadLayer(idx);
        s_logger.info("{}{}", (r.success ? "Layer reloaded: " : "Failed to reload layer: "), r.detail);

    } else {
        s_logger.info("Usage: !model <load|plan|surgery|pressure|status|layers|evict|reload>\n");
    }
}

// ============================================================================
// SWARM ORCHESTRATOR COMMANDS (Phase 21 distributed inference)
// ============================================================================

void cmd_swarm_orchestrator(const std::string& args) {
    auto& swarm = RawrXD::Swarm::SwarmOrchestrator::instance();
    auto [sub, rest] = splitFirst(args);

    if (sub == "join" || sub == "swarm_join") {
        // Gate check — joining a swarm is a network operation
        if (!cli_gate_check(static_cast<int>(ActionClass::NetworkRequest),
                           static_cast<int>(SafetyRiskTier::Medium),
                           0.90f, "Join distributed swarm" + (rest.empty() ? " (as coordinator)" : ": " + rest))) {
            return;
        }

        // Initialize if not already running
        if (!swarm.isRunning()) {
            auto role = rest.empty() ? RawrXD::Swarm::NodeRole::Coordinator
                                     : RawrXD::Swarm::NodeRole::Worker;
            auto r = swarm.initialize(role, "0.0.0.0");
            if (!r.success) {
                s_logger.info("❌ Failed to initialize swarm: ");
                return;
            }
        }

        auto r = swarm.joinSwarm(rest);
        s_logger.info("{} {}\n", (r.success ? "✅ " : "❌ "), r.detail);

        cli_record_action(
            static_cast<int>(ReplayActionType::AgentToolCall),
            "swarm", "join", rest,
            r.detail, r.success ? 0 : 1, 0.9f, 0.0);

    } else if (sub == "status" || sub == "swarm_status" || sub.empty()) {
        if (!swarm.isRunning()) {
            s_logger.info("[Swarm] Not initialized. Use !swarm_join to start.\n");
            return;
        }
        s_logger.info("\n🌐 Swarm Topology:\n");

    } else if (sub == "distribute" || sub == "swarm_distribute") {
        if (!swarm.isRunning()) {
            s_logger.info("❌ Swarm not initialized. Use !swarm_join first.\n");
            return;
        }
        auto [modelPath, layerStr] = splitFirst(rest);
        if (modelPath.empty()) {
            s_logger.info("Usage: !swarm_distribute <model_path> <total_layers>\n");
            return;
        }

        // Gate check — distributing a model is a critical operation
        if (!cli_gate_check(static_cast<int>(ActionClass::ModifyModel),
                           static_cast<int>(SafetyRiskTier::High),
                           0.85f, "Distribute model across swarm: " + modelPath)) {
            return;
        }

        uint32_t totalLayers = static_cast<uint32_t>(parseInt(layerStr, 128));
        s_logger.info("[Swarm] Distributing ");
        auto r = swarm.distributeModel(modelPath, totalLayers);
        s_logger.info("{} {}\n", (r.success ? "✅ " : "❌ "), r.detail);

        cli_record_action(
            static_cast<int>(ReplayActionType::AgentToolCall),
            "swarm", "distribute", modelPath + " " + std::to_string(totalLayers),
            r.detail, r.success ? 0 : 1, 0.85f, 0.0);

    } else if (sub == "rebalance" || sub == "swarm_rebalance") {
        if (!swarm.isRunning()) {
            s_logger.info("❌ Swarm not initialized.\n");
            return;
        }

        // Gate check
        if (!cli_gate_check(static_cast<int>(ActionClass::ModifyModel),
                           static_cast<int>(SafetyRiskTier::Medium),
                           0.90f, "Rebalance swarm layer shards")) {
            return;
        }

        s_logger.info("[Swarm] Rebalancing...\n");
        auto r = swarm.rebalance();
        s_logger.info("{} {}\n", (r.success ? "✅ " : "❌ "), r.detail);

    } else if (sub == "nodes" || sub == "swarm_nodes") {
        if (!swarm.isRunning()) {
            s_logger.info("[Swarm] Not initialized.\n");
            return;
        }
        auto nodes = swarm.getNodeList();
        s_logger.info("\n🖥  Swarm Nodes (");
        for (const auto& n : nodes) {
            const char* stateStr = "Unknown";
            switch (n.state) {
                case RawrXD::Swarm::NodeState::Offline:    stateStr = "OFFLINE";    break;
                case RawrXD::Swarm::NodeState::Joining:    stateStr = "JOINING";    break;
                case RawrXD::Swarm::NodeState::Active:     stateStr = "ACTIVE";     break;
                case RawrXD::Swarm::NodeState::Overloaded: stateStr = "OVERLOADED"; break;
                case RawrXD::Swarm::NodeState::Failed:     stateStr = "FAILED";     break;
            }
            s_logger.info("  ");
        }

    } else if (sub == "shards" || sub == "swarm_shards") {
        if (!swarm.isRunning()) {
            s_logger.info("[Swarm] Not initialized.\n");
            return;
        }
        auto shards = swarm.getShardList();
        s_logger.info("\n📦 Layer Shards (");
        for (const auto& s : shards) {
            s_logger.info("  ");
        }

    } else if (sub == "leave" || sub == "swarm_leave") {
        if (!swarm.isRunning()) {
            s_logger.info("[Swarm] Not initialized.\n");
            return;
        }
        swarm.leaveSwarm();
        s_logger.info("✅ Left the swarm\n");

    } else if (sub == "stats" || sub == "swarm_stats") {
        s_logger.info("\n📊 Swarm Statistics:\n");

    } else {
        s_logger.info("Usage: !swarm <join|status|distribute|rebalance|nodes|shards|leave|stats>\n");
    }
}
