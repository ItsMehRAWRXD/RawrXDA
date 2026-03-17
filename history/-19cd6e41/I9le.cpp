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
#include "../core/agent_safety_contract.h"

// Phase 10D: Confidence Gate
#include "../core/confidence_gate.h"

// Phase 10C: Deterministic Replay
#include "../core/deterministic_replay.h"

// Phase 10A: Execution Governor
#include "../core/execution_governor.h"

// Phase 9C: Multi-Response Engine
#include "../core/multi_response_engine.h"

// Phase 5: Agent History
#include "../agent_history.h"

// Phase 8A: Explainability
#include "../agent_explainability.h"

// Phase 7: Policy Engine
#include "../agent_policy.h"

// Tool Registry
#include "../tool_registry.h"

// Engine access
#include "../agentic_engine.h"
#include "../subagent_core.h"

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
            std::cerr << "[History] " << msg << "\n";
        }
    });

    // ── Phase 7: Policy Engine ──
    g_policyEngine = new PolicyEngine("rawrxd_policies");
    g_policyEngine->setHistoryRecorder(g_historyRecorder);
    g_policyEngine->load();
    g_policyEngine->setLogCallback([](int level, const std::string& msg) {
        if (level >= 2) {
            std::cerr << "[Policy] " << msg << "\n";
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
        std::cout << "🛡️ Safety DENIED: " << safetyResult.reason << "\n";
        if (!safetyResult.suggestion.empty()) {
            std::cout << "   Suggestion: " << safetyResult.suggestion << "\n";
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
        std::cout << "🎯 Confidence ABORT: " << confResult.reason
                  << " (confidence=" << confResult.rawConfidence
                  << ", threshold=" << confResult.effectiveThreshold << ")\n";
        return false;
    }

    if (confResult.decision == ConfidenceDecision::Escalate) {
        std::cout << "⚠️  Confidence ESCALATE (auto-approved in headless): "
                  << confResult.reason << "\n";
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
        std::cout << "\n" << safety.getStatusString() << "\n";

        auto stats = safety.getStats();
        std::cout << "🛡️  Safety Contract Status:\n";
        std::cout << "  Total checks:     " << stats.totalChecks << "\n";
        std::cout << "  Allowed:          " << stats.totalAllowed << "\n";
        std::cout << "  Denied:           " << stats.totalDenied << "\n";
        std::cout << "  Escalated:        " << stats.totalEscalated << "\n";
        std::cout << "  Rollbacks:        " << stats.totalRollbacks << "\n";
        std::cout << "  Violations:       " << stats.totalViolations << "\n";
        std::cout << "  Budget exceeded:  " << stats.budgetExceeded << "\n";
        std::cout << "  Risk blocked:     " << stats.riskBlocked << "\n";
        std::cout << "  Rate limited:     " << stats.rateLimited << "\n";
        std::cout << "  Max risk seen:    " << safetyRiskTierToString(stats.maxRiskSeen) << "\n";
        std::cout << "  Max allowed risk: " << safetyRiskTierToString(safety.getMaxAllowedRisk()) << "\n\n";

    } else if (sub == "reset") {
        safety.resetBudget();
        std::cout << "✅ Intent budget reset to defaults\n";

    } else if (sub == "rollback") {
        if (rest == "all") {
            safety.rollbackAll();
            std::cout << "✅ All rollbacks executed\n";
        } else {
            bool ok = safety.rollbackLast();
            std::cout << (ok ? "✅ Last action rolled back\n" : "❌ No rollback available\n");
        }

    } else if (sub == "violations") {
        auto violations = safety.getViolations();
        if (violations.empty()) {
            std::cout << "✅ No violations recorded\n";
            return;
        }
        std::cout << "🚨 Violations (" << violations.size() << "):\n";
        for (const auto& v : violations) {
            std::cout << "  [" << v.id << "] "
                      << violationTypeToString(v.type) << " — "
                      << actionClassToString(v.attemptedAction) << " (risk="
                      << safetyRiskTierToString(v.riskTier) << ")\n";
            std::cout << "       " << v.description << "\n";
            if (v.wasEscalated) {
                std::cout << "       Escalated → "
                          << (v.userApproved ? "approved" : "denied") << "\n";
            }
        }

    } else if (sub == "block") {
        if (rest.empty()) {
            std::cout << "Usage: !safety block <action_class>\n";
            std::cout << "  Classes: ReadFile, SearchCode, QueryModel, WriteFile, EditFile,\n";
            std::cout << "           CreateFile, DeleteFile, RunCommand, RunBuild, RunTest,\n";
            std::cout << "           InstallPackage, NetworkRequest, ModifyModel, PatchMemory, ...\n";
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
            std::cout << "❌ Unknown action class: " << rest << "\n";
            return;
        }
        safety.blockActionClass(ac);
        std::cout << "✅ Blocked: " << rest << "\n";

    } else if (sub == "unblock") {
        if (rest.empty()) {
            std::cout << "Usage: !safety unblock <action_class>\n";
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
            std::cout << "❌ Unknown action class: " << rest << "\n";
            return;
        }
        safety.unblockActionClass(ac);
        std::cout << "✅ Unblocked: " << rest << "\n";

    } else if (sub == "risk") {
        if (rest.empty()) {
            std::cout << "Current max risk: " << safetyRiskTierToString(safety.getMaxAllowedRisk()) << "\n";
            std::cout << "Usage: !safety risk <None|Low|Medium|High|Critical>\n";
            return;
        }
        SafetyRiskTier tier = SafetyRiskTier::Medium;
        if (rest == "None")          tier = SafetyRiskTier::None;
        else if (rest == "Low")      tier = SafetyRiskTier::Low;
        else if (rest == "Medium")   tier = SafetyRiskTier::Medium;
        else if (rest == "High")     tier = SafetyRiskTier::High;
        else if (rest == "Critical") tier = SafetyRiskTier::Critical;
        else {
            std::cout << "❌ Unknown risk tier: " << rest << "\n";
            return;
        }
        safety.setMaxAllowedRisk(tier);
        std::cout << "✅ Max allowed risk: " << safetyRiskTierToString(tier) << "\n";

    } else if (sub == "budget") {
        auto budget = safety.getBudget();
        std::cout << "\n📊 Intent Budget:\n";
        std::cout << "  File writes:     " << budget.usedFileWrites << " / " << budget.maxFileWrites << "\n";
        std::cout << "  File deletes:    " << budget.usedFileDeletes << " / " << budget.maxFileDeletes << "\n";
        std::cout << "  Command runs:    " << budget.usedCommandRuns << " / " << budget.maxCommandRuns << "\n";
        std::cout << "  Build runs:      " << budget.usedBuildRuns << " / " << budget.maxBuildRuns << "\n";
        std::cout << "  Network reqs:    " << budget.usedNetworkRequests << " / " << budget.maxNetworkRequests << "\n";
        std::cout << "  Model mods:      " << budget.usedModelModifications << " / " << budget.maxModelModifications << "\n";
        std::cout << "  Process kills:   " << budget.usedProcessKills << " / " << budget.maxProcessKills << "\n";
        std::cout << "  Total actions:   " << budget.usedTotalActions << " / " << budget.maxTotalActions << "\n";
        std::cout << "  Actions/min:     " << budget.actionsThisMinute << " / " << budget.maxActionsPerMinute << "\n\n";

    } else {
        std::cout << "Usage: !safety <status|reset|rollback|rollback_all|violations|block|unblock|risk|budget>\n";
    }
}

// ============================================================================
// CONFIDENCE GATE COMMANDS (Phase 10D)
// ============================================================================

void cmd_confidence(const std::string& args) {
    auto& gate = ConfidenceGate::instance();
    auto [sub, rest] = splitFirst(args);

    if (sub == "status" || sub.empty()) {
        std::cout << "\n" << gate.getStatusString() << "\n";

        auto stats = gate.getStats();
        std::cout << "🎯 Confidence Gate Status:\n";
        std::cout << "  Policy:            " << gatePolicyToString(stats.currentPolicy) << "\n";
        std::cout << "  Enabled:           " << (gate.isEnabled() ? "yes" : "no") << "\n";
        std::cout << "  Total evaluations: " << stats.totalEvaluations << "\n";
        std::cout << "  Executed:          " << stats.totalExecuted << "\n";
        std::cout << "  Escalated:         " << stats.totalEscalated << "\n";
        std::cout << "  Aborted:           " << stats.totalAborted << "\n";
        std::cout << "  Deferred:          " << stats.totalDeferred << "\n";
        std::cout << "  Overridden:        " << stats.totalOverridden << "\n";
        std::cout << "  Avg confidence:    " << std::fixed << std::setprecision(3)
                  << stats.avgConfidence << "\n";
        std::cout << "  Min confidence:    " << stats.minConfidence << "\n";
        std::cout << "  Max confidence:    " << stats.maxConfidence << "\n";
        std::cout << "  Recent avg:        " << stats.recentAvgConfidence << "\n";
        std::cout << "  Overall trend:     " << confidenceTrendToString(stats.overallTrend) << "\n";
        std::cout << "  Self-abort:        " << (gate.isSelfAbortTriggered() ? "⚠️ TRIGGERED" : "inactive") << "\n\n";

    } else if (sub == "policy") {
        if (rest.empty()) {
            std::cout << "Current policy: " << gatePolicyToString(gate.getPolicy()) << "\n";
            std::cout << "Usage: !confidence policy <strict|normal|relaxed|disabled>\n";
            return;
        }
        GatePolicy pol = GatePolicy::Normal;
        if (rest == "strict")        pol = GatePolicy::Strict;
        else if (rest == "normal")   pol = GatePolicy::Normal;
        else if (rest == "relaxed")  pol = GatePolicy::Relaxed;
        else if (rest == "disabled") pol = GatePolicy::Disabled;
        else {
            std::cout << "❌ Unknown policy: " << rest << "\n";
            return;
        }
        gate.setPolicy(pol);
        std::cout << "✅ Confidence gate policy: " << gatePolicyToString(pol) << "\n";

    } else if (sub == "threshold") {
        // Parse: <execute> <escalate> <abort>
        std::istringstream iss(rest);
        float exec_t, esc_t, abort_t;
        if (!(iss >> exec_t >> esc_t >> abort_t)) {
            auto thresholds = gate.getThresholds();
            std::cout << "Current thresholds:\n";
            std::cout << "  Execute:  " << thresholds.executeThreshold << "\n";
            std::cout << "  Escalate: " << thresholds.escalateThreshold << "\n";
            std::cout << "  Abort:    " << thresholds.abortThreshold << "\n";
            std::cout << "Usage: !confidence threshold <execute> <escalate> <abort>\n";
            return;
        }
        gate.setExecuteThreshold(exec_t);
        gate.setEscalateThreshold(esc_t);
        gate.setAbortThreshold(abort_t);
        std::cout << "✅ Thresholds set: execute=" << exec_t
                  << " escalate=" << esc_t << " abort=" << abort_t << "\n";

    } else if (sub == "history") {
        auto history = gate.getHistory(20);
        if (history.empty()) {
            std::cout << "No confidence evaluations recorded yet.\n";
            return;
        }
        std::cout << "\n🎯 Recent Confidence Evaluations (last " << history.size() << "):\n";
        std::cout << std::setw(6) << "Seq" << std::setw(10) << "Conf"
                  << std::setw(12) << "Decision" << std::setw(16) << "Action" << "\n";
        std::cout << std::string(44, '-') << "\n";
        for (const auto& h : history) {
            std::cout << std::setw(6) << h.sequenceId
                      << std::setw(10) << std::fixed << std::setprecision(3) << h.confidence
                      << std::setw(12) << confidenceDecisionToString(h.decision)
                      << std::setw(16) << actionClassToString(h.actionClass) << "\n";
        }
        std::cout << "\n";

    } else if (sub == "trend") {
        auto trend = gate.analyzeTrend();
        double slope = gate.getTrendSlope();
        float recentAvg = gate.getRecentAverage(50);
        std::cout << "\n📈 Confidence Trend:\n";
        std::cout << "  Trend:      " << confidenceTrendToString(trend) << "\n";
        std::cout << "  Slope:      " << std::fixed << std::setprecision(4) << slope << "\n";
        std::cout << "  Recent avg: " << std::setprecision(3) << recentAvg << "\n";
        std::cout << "  Degrading:  " << (gate.isConfidenceDegrading() ? "⚠️ YES" : "no") << "\n\n";

    } else if (sub == "selfabort") {
        std::cout << "Self-abort threshold:  " << gate.getSelfAbortThreshold() << " consecutive low actions\n";
        std::cout << "Self-abort triggered:  " << (gate.isSelfAbortTriggered() ? "⚠️ YES" : "no") << "\n";
        if (gate.isSelfAbortTriggered()) {
            std::cout << "  Use '!confidence reset' to clear self-abort state.\n";
        }

    } else if (sub == "reset") {
        gate.reset();
        gate.resetSelfAbort();
        std::cout << "✅ Confidence gate reset\n";

    } else {
        std::cout << "Usage: !confidence <status|policy|threshold|history|trend|selfabort|reset>\n";
    }
}

// ============================================================================
// REPLAY JOURNAL COMMANDS (Phase 10C)
// ============================================================================

void cmd_replay(const std::string& args) {
    auto& replay = ReplayJournal::instance();
    auto [sub, rest] = splitFirst(args);

    if (sub == "status" || sub.empty()) {
        std::cout << "\n" << replay.getStatusString() << "\n";

        auto stats = replay.getStats();
        std::cout << "📼 Replay Journal Status:\n";
        std::cout << "  State:              " << replayStateToString(stats.state) << "\n";
        std::cout << "  Total records:      " << stats.totalRecords << "\n";
        std::cout << "  In memory:          " << stats.recordsInMemory << "\n";
        std::cout << "  Flushed to disk:    " << stats.recordsFlushedToDisk << "\n";
        std::cout << "  Total sessions:     " << stats.totalSessions << "\n";
        std::cout << "  Active session:     " << stats.activeSessionId << "\n";
        std::cout << "  Current sequence:   " << stats.currentSequenceId << "\n";
        std::cout << "  Memory usage:       " << (stats.memoryUsageBytes / 1024) << " KB\n";
        std::cout << "  Disk usage:         " << (stats.diskUsageBytes / 1024) << " KB\n\n";

    } else if (sub == "last") {
        int count = parseInt(rest, 10);
        auto records = replay.getLastN(static_cast<uint64_t>(count));
        if (records.empty()) {
            std::cout << "No records in journal.\n";
            return;
        }
        std::cout << "\n📼 Last " << records.size() << " Actions:\n";
        std::cout << std::string(80, '-') << "\n";
        for (const auto& r : records) {
            std::cout << "[" << std::setw(6) << r.sequenceId << "] "
                      << std::setw(20) << std::left << replayActionTypeToString(r.type) << " "
                      << std::setw(10) << std::left << r.category << " ";
            if (!r.action.empty()) {
                std::string truncAction = r.action.substr(0, 40);
                if (r.action.size() > 40) truncAction += "...";
                std::cout << truncAction;
            }
            std::cout << "\n";
            if (!r.input.empty()) {
                std::string truncInput = r.input.substr(0, 60);
                if (r.input.size() > 60) truncInput += "...";
                std::cout << "          in:  " << truncInput << "\n";
            }
            if (r.durationMs > 0) {
                std::cout << "          dur: " << formatDuration(r.durationMs) << "\n";
            }
        }
        std::cout << std::string(80, '-') << "\n\n";

    } else if (sub == "session") {
        auto sessionId = replay.getActiveSessionId();
        auto snapshot = replay.getSessionSnapshot(sessionId);
        std::cout << "\n📼 Current Session:\n";
        std::cout << "  Session ID:     " << snapshot.sessionId << "\n";
        std::cout << "  Label:          " << snapshot.sessionLabel << "\n";
        std::cout << "  Action count:   " << snapshot.actionCount << "\n";
        std::cout << "  Start:          " << snapshot.startTime << "\n";
        std::cout << "  Duration:       " << formatDuration(snapshot.totalDurationMs) << "\n";
        std::cout << "  Agent queries:  " << snapshot.agentQueries << "\n";
        std::cout << "  Commands run:   " << snapshot.commandsRun << "\n";
        std::cout << "  Files modified: " << snapshot.filesModified << "\n";
        std::cout << "  Safety denials: " << snapshot.safetyDenials << "\n";
        std::cout << "  Gov. timeouts:  " << snapshot.governorTimeouts << "\n";
        std::cout << "  Errors:         " << snapshot.errors << "\n\n";

    } else if (sub == "sessions") {
        auto sessions = replay.getAllSessions();
        if (sessions.empty()) {
            std::cout << "No sessions recorded.\n";
            return;
        }
        std::cout << "\n📼 All Sessions (" << sessions.size() << "):\n";
        for (const auto& s : sessions) {
            std::cout << "  [" << s.sessionId << "] " << s.sessionLabel
                      << " (" << s.actionCount << " actions, "
                      << formatDuration(s.totalDurationMs) << ")\n";
        }
        std::cout << "\n";

    } else if (sub == "checkpoint") {
        std::string label = rest.empty() ? "cli-checkpoint" : rest;
        uint64_t id = replay.recordCheckpoint(label);
        std::cout << "✅ Checkpoint inserted: " << label << " (seq=" << id << ")\n";

    } else if (sub == "export") {
        if (rest.empty()) {
            std::cout << "Usage: !replay export <filepath>\n";
            return;
        }
        auto sessionId = replay.getActiveSessionId();
        bool ok = replay.exportSession(sessionId, rest);
        std::cout << (ok ? "✅ Session exported to: " : "❌ Export failed: ") << rest << "\n";

    } else if (sub == "filter") {
        if (rest.empty()) {
            std::cout << "Usage: !replay filter <type>\n";
            std::cout << "  Types: AgentQuery, AgentResponse, AgentToolCall, CommandExecution,\n";
            std::cout << "         FileWrite, ModelInference, SafetyCheck, SafetyDenial,\n";
            std::cout << "         GovernorSubmit, GovernorTimeout, Checkpoint, ...\n";
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
            std::cout << "❌ Unknown action type: " << rest << "\n";
            return;
        }

        auto records = replay.filter(filter);
        std::cout << "\n📼 Filtered: " << rest << " (" << records.size() << " records)\n";
        for (const auto& r : records) {
            std::cout << "  [" << r.sequenceId << "] " << r.action;
            if (r.durationMs > 0) std::cout << " (" << formatDuration(r.durationMs) << ")";
            std::cout << "\n";
        }
        std::cout << "\n";

    } else if (sub == "start") {
        replay.startRecording();
        std::cout << "✅ Recording started\n";

    } else if (sub == "pause") {
        replay.pauseRecording();
        std::cout << "⏸️  Recording paused\n";

    } else if (sub == "stop") {
        replay.stopRecording();
        std::cout << "⏹️  Recording stopped\n";

    } else {
        std::cout << "Usage: !replay <status|last|session|sessions|checkpoint|export|filter|start|pause|stop>\n";
    }
}

// ============================================================================
// EXECUTION GOVERNOR COMMANDS (Phase 10A)
// ============================================================================

void cmd_governor(const std::string& args) {
    auto& governor = ExecutionGovernor::instance();
    auto [sub, rest] = splitFirst(args);

    if (sub == "status" || sub.empty()) {
        std::cout << "\n" << governor.getStatusString() << "\n";

        auto stats = governor.getStats();
        std::cout << "⚙️  Execution Governor Status:\n";
        std::cout << "  Initialized:    " << (governor.isInitialized() ? "yes" : "no") << "\n";
        std::cout << "  Active tasks:   " << stats.activeTaskCount << "\n";
        std::cout << "  Peak concurrent:" << stats.peakConcurrent << "\n";
        std::cout << "  Total submitted:" << stats.totalSubmitted << "\n";
        std::cout << "  Completed:      " << stats.totalCompleted << "\n";
        std::cout << "  Timed out:      " << stats.totalTimedOut << "\n";
        std::cout << "  Killed:         " << stats.totalKilled << "\n";
        std::cout << "  Cancelled:      " << stats.totalCancelled << "\n";
        std::cout << "  Failed:         " << stats.totalFailed << "\n";
        std::cout << "  Rollbacks:      " << stats.totalRollbacks << "\n";
        std::cout << "  Avg duration:   " << formatDuration(stats.avgTaskDurationMs) << "\n";
        std::cout << "  Longest task:   " << formatDuration(static_cast<double>(stats.longestTaskMs)) << "\n\n";

    } else if (sub == "run") {
        if (rest.empty()) {
            std::cout << "Usage: !governor run <command> [--timeout <ms>]\n";
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

        std::cout << "⚙️  Submitting to governor: " << command << " (timeout=" << timeout << "ms)\n";

        auto taskId = governor.submitCommand(command, timeout,
                                             GovernorRiskTier::Medium,
                                             "CLI: " + command);

        // Record in replay
        auto& replay = ReplayJournal::instance();
        replay.recordGovernorEvent(ReplayActionType::GovernorSubmit,
                                   "CLI governor run: " + command, taskId);

        // Wait synchronously for result
        auto result = governor.waitForTask(taskId, timeout + 5000);

        std::cout << "\n─── Governor Result ───\n";
        if (result.timedOut) {
            std::cout << "⏰ TIMED OUT after " << formatDuration(result.durationMs) << "\n";
            replay.recordGovernorEvent(ReplayActionType::GovernorTimeout,
                                       "Timed out: " + command, taskId);
        } else if (result.cancelled) {
            std::cout << "🚫 CANCELLED\n";
        } else {
            std::cout << "Exit code: " << result.exitCode << "\n";
            replay.recordGovernorEvent(ReplayActionType::GovernorComplete,
                                       "Completed: " + command, taskId);
        }
        if (!result.output.empty()) {
            std::cout << result.output;
            if (result.output.back() != '\n') std::cout << "\n";
        }
        std::cout << "Duration: " << formatDuration(result.durationMs)
                  << " (" << result.bytesRead << " bytes captured)\n";
        std::cout << "───────────────────────\n\n";

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
            std::cout << "No active tasks.\n";
            return;
        }
        std::cout << "⚙️  Active Tasks (" << descriptions.size() << "):\n";
        for (size_t i = 0; i < descriptions.size(); i++) {
            std::cout << "  [" << i << "] " << descriptions[i] << "\n";
        }
        std::cout << "\n";

    } else if (sub == "kill") {
        if (rest.empty()) {
            std::cout << "Usage: !governor kill <task_id>\n";
            return;
        }
        auto taskId = static_cast<GovernorTaskId>(parseInt(rest, 0));
        bool ok = governor.killTask(taskId);
        std::cout << (ok ? "✅ Task killed" : "❌ Task not found or already complete")
                  << " (id=" << taskId << ")\n";

    } else if (sub == "kill_all") {
        governor.killAll();
        std::cout << "✅ All tasks killed\n";

    } else if (sub == "wait") {
        if (rest.empty()) {
            std::cout << "Usage: !governor wait <task_id>\n";
            return;
        }
        auto taskId = static_cast<GovernorTaskId>(parseInt(rest, 0));
        std::cout << "⏳ Waiting for task " << taskId << "...\n";
        auto result = governor.waitForTask(taskId, 60000);
        std::cout << "Exit: " << result.exitCode << " Duration: "
                  << formatDuration(result.durationMs) << "\n";
        if (!result.output.empty()) std::cout << result.output << "\n";

    } else {
        std::cout << "Usage: !governor <status|run|tasks|kill|kill_all|wait>\n";
    }
}

// ============================================================================
// MULTI-RESPONSE ENGINE COMMANDS (Phase 9C)
// ============================================================================

void cmd_multi_response(const std::string& args) {
    if (!g_multiResponse) {
        std::cout << "❌ MultiResponseEngine not initialized\n";
        return;
    }
    auto [sub, rest] = splitFirst(args);

    if (sub == "templates") {
        auto templates = g_multiResponse->getAllTemplates();
        std::cout << "\n📝 Response Templates:\n";
        for (const auto& t : templates) {
            std::cout << "  [" << static_cast<int>(t.id) << "] " << t.name
                      << " (" << t.shortLabel << ") "
                      << (t.enabled ? "✅" : "❌")
                      << " temp=" << t.temperature
                      << " max=" << t.maxTokens << "\n";
            if (t.description[0] != '\0') {
                std::cout << "      " << t.description << "\n";
            }
        }
        std::cout << "\n";

    } else if (sub == "stats") {
        auto stats = g_multiResponse->getStats();
        std::cout << "\n📊 Multi-Response Stats:\n";
        std::cout << "  Total sessions:     " << stats.totalSessions << "\n";
        std::cout << "  Responses generated: " << stats.totalResponsesGenerated << "\n";
        std::cout << "  Preferences recorded:" << stats.totalPreferencesRecorded << "\n";
        std::cout << "  Errors:             " << stats.errorCount << "\n";
        std::cout << "  Preference counts:  ";
        const char* names[] = { "Strategic", "Grounded", "Creative", "Concise" };
        for (int i = 0; i < 4; i++) {
            std::cout << names[i] << "=" << stats.preferenceCount[i];
            if (i < 3) std::cout << ", ";
        }
        std::cout << "\n  Avg latency:        ";
        for (int i = 0; i < 4; i++) {
            std::cout << names[i] << "=" << formatDuration(stats.avgLatencyMs[i]);
            if (i < 3) std::cout << ", ";
        }
        std::cout << "\n\n";

    } else if (sub == "toggle") {
        if (rest.empty()) {
            std::cout << "Usage: !multi toggle <0|1|2|3>\n";
            std::cout << "  0=Strategic, 1=Grounded, 2=Creative, 3=Concise\n";
            return;
        }
        int id = parseInt(rest, -1);
        if (id < 0 || id > 3) {
            std::cout << "❌ Invalid template ID (0-3)\n";
            return;
        }
        auto tmpl = g_multiResponse->getTemplate(static_cast<ResponseTemplateId>(id));
        g_multiResponse->setTemplateEnabled(static_cast<ResponseTemplateId>(id), !tmpl.enabled);
        std::cout << "✅ " << tmpl.name << " → "
                  << (!tmpl.enabled ? "enabled" : "disabled") << "\n";

    } else if (sub == "prefer") {
        if (rest.empty()) {
            std::cout << "Usage: !multi prefer <index 0-3> [reason]\n";
            return;
        }
        auto [idxStr, reason] = splitFirst(rest);
        int idx = parseInt(idxStr, -1);
        if (idx < 0 || idx > 3) {
            std::cout << "❌ Invalid response index (0-3)\n";
            return;
        }
        auto* session = g_multiResponse->getLatestSession();
        if (!session) {
            std::cout << "❌ No multi-response session available. Run !multi <prompt> first.\n";
            return;
        }
        auto result = g_multiResponse->setPreference(session->sessionId, idx, reason);
        std::cout << (result.success ? "✅" : "❌") << " " << result.detail << "\n";

    } else if (sub == "recommend") {
        std::string rec = g_multiResponse->getRecommendedTemplate();
        std::cout << "🎯 Recommended template: " << rec << "\n";

    } else if (sub == "compare") {
        auto* session = g_multiResponse->getLatestSession();
        if (!session || session->responses.empty()) {
            std::cout << "❌ No multi-response session to compare. Run !multi <prompt> first.\n";
            return;
        }
        std::cout << "\n" << std::string(80, '=') << "\n";
        std::cout << "Multi-Response Comparison — Prompt: \"" 
                  << session->prompt.substr(0, 60) << "\"\n";
        std::cout << std::string(80, '=') << "\n";
        for (const auto& resp : session->responses) {
            if (!resp.complete && !resp.error) continue;
            std::cout << "\n── [" << resp.index << "] " << resp.templateName
                      << " (" << resp.tokenCount << " tokens, "
                      << formatDuration(resp.latencyMs) << ") ";
            if (resp.error) std::cout << "❌ ERROR: " << resp.errorDetail;
            std::cout << " ──\n";
            if (!resp.content.empty()) {
                // Truncate for display
                std::string display = resp.content;
                if (display.size() > 500) {
                    display = display.substr(0, 500) + "\n... [truncated]";
                }
                std::cout << display << "\n";
            }
        }
        std::cout << std::string(80, '=') << "\n";
        if (session->preferredIndex >= 0) {
            std::cout << "✅ Preferred: [" << session->preferredIndex << "] "
                      << session->responses[session->preferredIndex].templateName << "\n";
        }
        std::cout << "\n";

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
        std::cout << "🔀 Generating " << maxResp << " responses...\n\n";

        auto sessionId = g_multiResponse->startSession(prompt, maxResp);

        // Per-response callback for live display
        auto perCb = [](const GeneratedResponse& resp, void* /*userData*/) {
            std::cout << "  [" << resp.index << "] " << resp.templateName << ": "
                      << resp.tokenCount << " tokens, "
                      << static_cast<int>(resp.latencyMs) << "ms";
            if (resp.error) std::cout << " ❌ " << resp.errorDetail;
            else std::cout << " ✅";
            std::cout << "\n";
        };

        auto result = g_multiResponse->generateAll(sessionId, perCb, nullptr);

        if (!result.success) {
            std::cout << "❌ Generation failed: " << result.detail << "\n";
            return;
        }

        std::cout << "\n✅ All responses generated. Use !multi compare to review.\n";
        std::cout << "   Use !multi prefer <0-3> to record your preference.\n\n";

        // Record in history
        if (g_historyRecorder) {
            g_historyRecorder->record("multi_response", "system", "",
                "Generated " + std::to_string(maxResp) + " responses",
                prompt, "", true, 0, "", "");
        }

    } else {
        std::cout << "Usage: !multi <prompt>        Generate multi-style responses\n";
        std::cout << "       !multi compare         Compare last responses\n";
        std::cout << "       !multi prefer <0-3>    Record preference\n";
        std::cout << "       !multi templates       Show all templates\n";
        std::cout << "       !multi toggle <0-3>    Toggle template on/off\n";
        std::cout << "       !multi recommend       Show recommended template\n";
        std::cout << "       !multi stats           Show generation stats\n";
    }
}

// ============================================================================
// AGENT HISTORY COMMANDS (Phase 5)
// ============================================================================

void cmd_history(const std::string& args) {
    if (!g_historyRecorder) {
        std::cout << "❌ AgentHistoryRecorder not initialized\n";
        return;
    }
    auto [sub, rest] = splitFirst(args);

    if (sub == "show" || sub.empty()) {
        int count = parseInt(rest, 20);
        HistoryQuery q;
        q.limit = count;
        auto events = g_historyRecorder->query(q);
        if (events.empty()) {
            std::cout << "No history events.\n";
            return;
        }
        std::cout << "\n📜 Agent History (last " << events.size() << "):\n";
        std::cout << std::string(80, '-') << "\n";
        for (const auto& e : events) {
            std::cout << "[" << std::setw(6) << e.id << "] "
                      << std::setw(20) << std::left << e.eventType
                      << (e.success ? "✅" : "❌") << " ";
            if (!e.agentId.empty()) std::cout << "agent=" << e.agentId << " ";
            if (e.durationMs > 0) std::cout << "(" << e.durationMs << "ms) ";
            std::cout << "\n";
            if (!e.description.empty()) {
                std::string desc = e.description.substr(0, 70);
                if (e.description.size() > 70) desc += "...";
                std::cout << "       " << desc << "\n";
            }
            if (!e.success && !e.errorMessage.empty()) {
                std::cout << "       ❌ " << e.errorMessage.substr(0, 60) << "\n";
            }
        }
        std::cout << std::string(80, '-') << "\n\n";

    } else if (sub == "session") {
        auto timeline = g_historyRecorder->getSessionTimeline(g_historyRecorder->sessionId());
        std::cout << "\n📜 Session Timeline (" << timeline.size() << " events):\n";
        for (const auto& e : timeline) {
            std::cout << "  [" << e.id << "] " << e.eventType
                      << (e.success ? " ✅" : " ❌") << " " << e.description.substr(0, 50) << "\n";
        }
        std::cout << "\n";

    } else if (sub == "agent") {
        if (rest.empty()) {
            std::cout << "Usage: !history agent <agent_id>\n";
            return;
        }
        auto timeline = g_historyRecorder->getAgentTimeline(rest);
        if (timeline.empty()) {
            std::cout << "No events for agent: " << rest << "\n";
            return;
        }
        std::cout << "\n📜 Agent Timeline: " << rest << " (" << timeline.size() << " events)\n";
        for (const auto& e : timeline) {
            std::cout << "  [" << e.id << "] " << e.eventType
                      << (e.success ? " ✅" : " ❌");
            if (e.durationMs > 0) std::cout << " (" << e.durationMs << "ms)";
            std::cout << "\n";
            if (!e.description.empty()) {
                std::cout << "       " << e.description.substr(0, 60) << "\n";
            }
        }
        std::cout << "\n";

    } else if (sub == "type") {
        if (rest.empty()) {
            std::cout << "Usage: !history type <event_type>\n";
            std::cout << "  Types: agent_spawn, agent_complete, agent_fail, tool_invoke,\n";
            std::cout << "         chain_start, chain_step, chain_complete, swarm_start,\n";
            std::cout << "         swarm_task, swarm_complete, chat_request, chat_response, ...\n";
            return;
        }
        auto events = g_historyRecorder->getEventsByType(rest, 50);
        std::cout << "\n📜 Events of type '" << rest << "' (" << events.size() << "):\n";
        for (const auto& e : events) {
            std::cout << "  [" << e.id << "] " << (e.success ? "✅" : "❌") << " "
                      << e.description.substr(0, 60) << "\n";
        }
        std::cout << "\n";

    } else if (sub == "stats") {
        std::cout << "\n" << g_historyRecorder->getStatsSummary() << "\n";

    } else if (sub == "flush") {
        g_historyRecorder->flush();
        std::cout << "✅ Events flushed to disk\n";

    } else if (sub == "clear") {
        g_historyRecorder->clear();
        std::cout << "✅ In-memory events cleared\n";

    } else if (sub == "export") {
        if (rest.empty()) {
            std::cout << "Usage: !history export <filepath>\n";
            return;
        }
        auto events = g_historyRecorder->allEvents();
        std::string json = g_historyRecorder->toJSON(events);
        std::ofstream out(rest);
        if (out.is_open()) {
            out << json;
            out.close();
            std::cout << "✅ Exported " << events.size() << " events to: " << rest << "\n";
        } else {
            std::cout << "❌ Failed to write: " << rest << "\n";
        }

    } else {
        std::cout << "Usage: !history <show|session|agent|type|stats|flush|clear|export>\n";
    }
}

// ============================================================================
// EXPLAINABILITY COMMANDS (Phase 8A)
// ============================================================================

void cmd_explain(const std::string& args) {
    if (!g_explainEngine) {
        std::cout << "❌ ExplainabilityEngine not initialized\n";
        return;
    }
    auto [sub, rest] = splitFirst(args);

    if (sub == "agent") {
        if (rest.empty()) {
            std::cout << "Usage: !explain agent <agent_id>\n";
            return;
        }
        auto trace = g_explainEngine->traceAgent(rest);
        std::cout << "\n" << trace.toJSON() << "\n";
        std::cout << "\n🔍 Agent Trace: " << trace.rootAgentId << "\n";
        std::cout << "  Nodes: " << trace.nodeCount
                  << " Failures: " << trace.failureCount
                  << " Policies: " << trace.policyFireCount
                  << " Duration: " << trace.totalDurationMs << "ms\n";
        std::cout << "  Result: " << (trace.overallSuccess ? "✅" : "❌") << "\n";
        if (!trace.summary.empty()) {
            std::cout << "  Summary: " << trace.summary << "\n";
        }
        for (const auto& node : trace.nodes) {
            std::cout << "    [" << node.eventId << "] " << node.eventType
                      << " " << (node.success ? "✅" : "❌")
                      << " " << node.description.substr(0, 50) << "\n";
            if (!node.policyId.empty()) {
                std::cout << "      Policy: " << node.policyName << " → " << node.policyEffect << "\n";
            }
        }
        std::cout << "\n";

    } else if (sub == "chain") {
        if (rest.empty()) {
            std::cout << "Usage: !explain chain <parent_id>\n";
            return;
        }
        auto trace = g_explainEngine->traceChain(rest);
        std::cout << "\n🔗 Chain Trace: " << trace.rootAgentId << "\n";
        std::cout << "  Nodes: " << trace.nodeCount << " Duration: " << trace.totalDurationMs << "ms\n";
        if (!trace.summary.empty()) std::cout << "  " << trace.summary << "\n";
        for (const auto& node : trace.nodes) {
            std::cout << "    [" << node.eventId << "] " << node.eventType
                      << " " << (node.success ? "✅" : "❌")
                      << " " << node.description.substr(0, 50) << "\n";
        }
        std::cout << "\n";

    } else if (sub == "swarm") {
        if (rest.empty()) {
            std::cout << "Usage: !explain swarm <parent_id>\n";
            return;
        }
        auto trace = g_explainEngine->traceSwarm(rest);
        std::cout << "\n🐝 Swarm Trace: " << trace.rootAgentId << "\n";
        std::cout << "  Nodes: " << trace.nodeCount << " Duration: " << trace.totalDurationMs << "ms\n";
        if (!trace.summary.empty()) std::cout << "  " << trace.summary << "\n";
        for (const auto& node : trace.nodes) {
            std::cout << "    [" << node.eventId << "] " << node.eventType
                      << " " << (node.success ? "✅" : "❌")
                      << " " << node.description.substr(0, 50) << "\n";
        }
        std::cout << "\n";

    } else if (sub == "failures") {
        auto failures = g_explainEngine->explainFailures();
        if (failures.empty()) {
            std::cout << "✅ No failures to explain in current session.\n";
            return;
        }
        std::cout << "\n🔍 Failure Attributions (" << failures.size() << "):\n";
        for (const auto& f : failures) {
            std::cout << "  [" << f.failureEventId << "] " << f.failureType
                      << " agent=" << f.agentId << "\n";
            std::cout << "    Error: " << f.errorMessage.substr(0, 60) << "\n";
            std::cout << "    Strategy: " << f.correctionStrategy;
            if (f.wasRetried) {
                std::cout << " → retry " << (f.retrySucceeded ? "✅" : "❌");
            }
            std::cout << "\n";
            if (!f.policyId.empty()) {
                std::cout << "    Policy: " << f.policyName << " — " << f.policyRationale << "\n";
            }
            if (f.historicalSuccessRate >= 0) {
                std::cout << "    Historical success rate: " << std::fixed
                          << std::setprecision(1) << (f.historicalSuccessRate * 100) << "% ("
                          << f.historicalOccurrences << " occurrences)\n";
            }
        }
        std::cout << "\n";

    } else if (sub == "policies") {
        auto attrs = g_explainEngine->explainPolicies();
        if (attrs.empty()) {
            std::cout << "No policy firings in current session.\n";
            return;
        }
        std::cout << "\n📜 Policy Attributions (" << attrs.size() << "):\n";
        for (const auto& p : attrs) {
            std::cout << "  " << p.policyName << " (priority=" << p.policyPriority << ")\n";
            std::cout << "    Trigger: " << p.triggerEventType;
            if (!p.triggerPattern.empty()) std::cout << " pattern=" << p.triggerPattern;
            if (p.triggerFailureRate > 0) std::cout << " failRate=" << p.triggerFailureRate;
            std::cout << "\n";
            std::cout << "    Effect: " << p.effectDescription << "\n";
            std::cout << "    Applied " << p.policyAppliedCount << " times"
                      << " est.improvement=" << std::fixed << std::setprecision(1)
                      << (p.estimatedImprovement * 100) << "%\n";
        }
        std::cout << "\n";

    } else if (sub == "session") {
        auto explanation = g_explainEngine->explainSession();
        std::cout << "\n📊 Session Explanation:\n";
        std::cout << "  Events:    " << explanation.totalEvents << "\n";
        std::cout << "  Agents:    " << explanation.agentSpawns << "\n";
        std::cout << "  Chains:    " << explanation.chainExecutions << "\n";
        std::cout << "  Swarms:    " << explanation.swarmExecutions << "\n";
        std::cout << "  Failures:  " << explanation.failures << "\n";
        std::cout << "  Retries:   " << explanation.retries << "\n";
        std::cout << "  Policies:  " << explanation.policyFirings << "\n";
        if (!explanation.narrative.empty()) {
            std::cout << "\n  Narrative:\n  " << explanation.narrative << "\n";
        }
        std::cout << "\n";

    } else if (sub == "snapshot") {
        if (rest.empty()) {
            std::cout << "Usage: !explain snapshot <filepath>\n";
            return;
        }
        bool ok = g_explainEngine->exportSnapshot(rest);
        std::cout << (ok ? "✅ Snapshot exported to: " : "❌ Export failed: ") << rest << "\n";

    } else {
        std::cout << "Usage: !explain <agent|chain|swarm|failures|policies|session|snapshot>\n";
    }
}

// ============================================================================
// POLICY ENGINE COMMANDS (Phase 7)
// ============================================================================

void cmd_policy(const std::string& args) {
    if (!g_policyEngine) {
        std::cout << "❌ PolicyEngine not initialized\n";
        return;
    }
    auto [sub, rest] = splitFirst(args);

    if (sub == "list" || sub.empty()) {
        auto policies = g_policyEngine->getAllPolicies();
        if (policies.empty()) {
            std::cout << "No policies defined. Use !policy suggest to generate suggestions.\n";
            return;
        }
        std::cout << "\n📜 Policies (" << policies.size() << "):\n";
        for (const auto& p : policies) {
            std::cout << "  " << (p.enabled ? "✅" : "❌") << " [" << p.id.substr(0, 8) << "] "
                      << p.name << " (v" << p.version << ", priority=" << p.priority
                      << ", applied=" << p.appliedCount << ")\n";
            if (!p.description.empty()) {
                std::cout << "     " << p.description.substr(0, 60) << "\n";
            }
        }
        std::cout << "\n";

    } else if (sub == "show") {
        if (rest.empty()) {
            std::cout << "Usage: !policy show <policy_id>\n";
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
            std::cout << "❌ Policy not found: " << rest << "\n";
            return;
        }
        std::cout << "\n" << p->toJSON() << "\n\n";

    } else if (sub == "enable") {
        if (rest.empty()) { std::cout << "Usage: !policy enable <id>\n"; return; }
        bool ok = g_policyEngine->setEnabled(rest, true);
        std::cout << (ok ? "✅ Policy enabled" : "❌ Policy not found") << "\n";

    } else if (sub == "disable") {
        if (rest.empty()) { std::cout << "Usage: !policy disable <id>\n"; return; }
        bool ok = g_policyEngine->setEnabled(rest, false);
        std::cout << (ok ? "✅ Policy disabled" : "❌ Policy not found") << "\n";

    } else if (sub == "remove") {
        if (rest.empty()) { std::cout << "Usage: !policy remove <id>\n"; return; }
        bool ok = g_policyEngine->removePolicy(rest);
        std::cout << (ok ? "✅ Policy removed" : "❌ Policy not found") << "\n";

    } else if (sub == "heuristics") {
        g_policyEngine->computeHeuristics();
        auto heuristics = g_policyEngine->getAllHeuristics();
        if (heuristics.empty()) {
            std::cout << "No heuristics computed (need more history data).\n";
            return;
        }
        std::cout << "\n📊 Computed Heuristics (" << heuristics.size() << "):\n";
        std::cout << std::setw(25) << std::left << "Key"
                  << std::setw(8) << "Total"
                  << std::setw(8) << "Pass"
                  << std::setw(8) << "Fail"
                  << std::setw(10) << "Rate"
                  << std::setw(10) << "Avg ms" << "\n";
        std::cout << std::string(69, '-') << "\n";
        for (const auto& h : heuristics) {
            std::cout << std::setw(25) << std::left << h.key
                      << std::setw(8) << h.totalEvents
                      << std::setw(8) << h.successCount
                      << std::setw(8) << h.failCount
                      << std::setw(10) << std::fixed << std::setprecision(1)
                      << (h.successRate * 100) << "%"
                      << std::setw(10) << static_cast<int>(h.avgDurationMs) << "\n";
        }
        std::cout << "\n";

    } else if (sub == "suggest") {
        std::cout << "🧠 Analyzing history for policy suggestions...\n";
        g_policyEngine->computeHeuristics();
        auto suggestions = g_policyEngine->generateSuggestions();
        if (suggestions.empty()) {
            std::cout << "No suggestions at this time (need more diverse history data).\n";
            return;
        }
        std::cout << "\n💡 Policy Suggestions (" << suggestions.size() << "):\n";
        for (const auto& s : suggestions) {
            std::cout << "  [" << s.id.substr(0, 8) << "] " << s.proposedPolicy.name << "\n";
            std::cout << "    " << s.rationale << "\n";
            std::cout << "    Est. improvement: " << std::fixed << std::setprecision(1)
                      << (s.estimatedImprovement * 100) << "% ("
                      << s.supportingEvents << " supporting events)\n";
            std::cout << "    State: " << s.stateString() << "\n";
        }
        std::cout << "\n  Use !policy accept <id> or !policy reject <id>\n\n";

    } else if (sub == "accept") {
        if (rest.empty()) { std::cout << "Usage: !policy accept <suggestion_id>\n"; return; }
        bool ok = g_policyEngine->acceptSuggestion(rest);
        std::cout << (ok ? "✅ Suggestion accepted and policy activated"
                        : "❌ Suggestion not found") << "\n";

    } else if (sub == "reject") {
        if (rest.empty()) { std::cout << "Usage: !policy reject <suggestion_id>\n"; return; }
        bool ok = g_policyEngine->rejectSuggestion(rest);
        std::cout << (ok ? "✅ Suggestion rejected" : "❌ Suggestion not found") << "\n";

    } else if (sub == "pending") {
        auto pending = g_policyEngine->getPendingSuggestions();
        if (pending.empty()) {
            std::cout << "No pending suggestions.\n";
            return;
        }
        std::cout << "\n💡 Pending Suggestions (" << pending.size() << "):\n";
        for (const auto& s : pending) {
            std::cout << "  [" << s.id.substr(0, 8) << "] " << s.proposedPolicy.name
                      << " — " << s.rationale.substr(0, 50) << "\n";
        }
        std::cout << "\n";

    } else if (sub == "export") {
        if (rest.empty()) { std::cout << "Usage: !policy export <filepath>\n"; return; }
        bool ok = g_policyEngine->exportToFile(rest);
        std::cout << (ok ? "✅ Policies exported to: " : "❌ Export failed: ") << rest << "\n";

    } else if (sub == "import") {
        if (rest.empty()) { std::cout << "Usage: !policy import <filepath>\n"; return; }
        int count = g_policyEngine->importFromFile(rest);
        std::cout << "✅ Imported " << count << " policies from: " << rest << "\n";

    } else if (sub == "stats") {
        std::cout << "\n" << g_policyEngine->getStatsSummary() << "\n";

    } else {
        std::cout << "Usage: !policy <list|show|enable|disable|remove|heuristics|suggest|accept|reject|pending|export|import|stats>\n";
    }
}

// ============================================================================
// TOOL REGISTRY COMMANDS
// ============================================================================

void cmd_tools(const std::string& args) {
    std::string toolList = ToolRegistry::list_tools();
    if (toolList.empty()) {
        std::cout << "No tools registered.\n";
        return;
    }
    std::cout << "\n🔧 Registered Tools:\n" << toolList << "\n";
}
