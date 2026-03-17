// ============================================================================
// Win32IDE_ExecutionGovernor.cpp — Phase 10: IDE Integration
// ============================================================================
//
// Wires all Phase 10 subsystems into the Win32IDE:
//   - 10A: ExecutionGovernor + TerminalWatchdog
//   - 10B: AgentSafetyContract
//   - 10C: DeterministicReplay / ReplayJournal
//   - 10D: ConfidenceGate
//
// Contents:
//   1. Lifecycle (init/shutdown)
//   2. Command handlers (IDM_GOV_* 5118-5131)
//   3. HTTP endpoint handlers (/api/governor/*, /api/safety/*, etc.)
//
// Pattern:  PatchResult-style results, no exceptions
// Rule:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "Win32IDE.h"
#include "../core/execution_governor.h"
#include "../core/agent_safety_contract.h"
#include "../core/deterministic_replay.h"
#include "../core/confidence_gate.h"
#include <sstream>
#include <iomanip>

// ============================================================================
// 1. LIFECYCLE
// ============================================================================

void Win32IDE::initPhase10() {
    // ── 10A: Execution Governor ─────────────────────────────────────────
    ExecutionGovernor::instance().init();
    m_phase10Initialized = true;

    // ── 10B: Safety Contracts ───────────────────────────────────────────
    AgentSafetyContract::instance().init();

    // ── 10C: Replay Journal ─────────────────────────────────────────────
    std::string journalDir = ".\\replay_journal";
    CreateDirectoryA(journalDir.c_str(), nullptr);
    ReplayJournal::instance().init(journalDir);
    ReplayJournal::instance().startSession("IDE Session");
    ReplayJournal::instance().startRecording();

    // ── 10D: Confidence Gate ────────────────────────────────────────────
    ConfidenceGate::instance().init();

    logMessage("[Phase10] All subsystems initialized (Governor, Safety, Replay, Confidence)");
}

void Win32IDE::shutdownPhase10() {
    if (!m_phase10Initialized) return;

    // Reverse order shutdown
    ConfidenceGate::instance().shutdown();

    ReplayJournal::instance().stopRecording();
    ReplayJournal::instance().endSession();
    ReplayJournal::instance().flushToDisk();
    ReplayJournal::instance().shutdown();

    AgentSafetyContract::instance().shutdown();
    ExecutionGovernor::instance().shutdown();

    m_phase10Initialized = false;
    logMessage("[Phase10] All subsystems shut down");
}

// ============================================================================
// 2. COMMAND HANDLERS — Governor (5118-5121)
// ============================================================================

void Win32IDE::cmdGovernorStatus() {
    std::string status = ExecutionGovernor::instance().getStatusString();
    showOutputPanel(status);
    ReplayJournal::instance().recordAction(
        ReplayActionType::MenuCommand, "governor", "status", "", status);
}

void Win32IDE::cmdGovernorSubmitCommand() {
    // Show input dialog for command
    std::string command = showInputDialog("Governor: Submit Command",
        "Enter command to execute under governor supervision:");
    if (command.empty()) return;

    // Safety check first
    auto safetyResult = AgentSafetyContract::instance().checkAndConsume(
        ActionClass::RunCommand, SafetyRiskTier::High, command);

    if (safetyResult.verdict == ContractVerdict::Denied) {
        showOutputPanel("[SAFETY DENIED] " + safetyResult.reason + "\n" + safetyResult.suggestion);
        ReplayJournal::instance().recordSafetyEvent(
            ReplayActionType::SafetyDenial, safetyResult.reason);
        return;
    }

    if (safetyResult.verdict == ContractVerdict::Escalated) {
        int result = MessageBoxA(m_hwnd,
            ("Safety escalation required:\n\n" + safetyResult.reason + "\n\nProceed?").c_str(),
            "RawrXD Governor - Safety Escalation", MB_YESNO | MB_ICONWARNING);
        if (result != IDYES) {
            ReplayJournal::instance().recordSafetyEvent(
                ReplayActionType::SafetyDenial, "User declined escalation: " + command);
            return;
        }
    }

    // Confidence gate
    auto confEval = ConfidenceGate::instance().evaluateAndRecord(
        0.85f, ActionClass::RunCommand, SafetyRiskTier::High, command);

    if (confEval.decision == ConfidenceDecision::Abort) {
        showOutputPanel("[CONFIDENCE ABORT] " + confEval.reason);
        return;
    }

    // Submit to governor
    GovernorTaskId taskId = ExecutionGovernor::instance().submitCommand(
        command,
        TerminalWatchdog::DEFAULT_AGENT_TIMEOUT_MS,
        GovernorRiskTier::High,
        command,
        [this, command](const GovernorCommandResult& result) {
            std::string msg = "Governor Task Complete:\n"
                              "  Command: " + command + "\n"
                              "  Exit Code: " + std::to_string(result.exitCode) + "\n"
                              "  Duration: " + std::to_string((int)result.durationMs) + "ms\n"
                              "  Timed Out: " + std::string(result.timedOut ? "YES" : "NO") + "\n"
                              "  Output:\n" + result.output;
            PostMessage(m_hwnd, WM_USER + 200, 0, 0);
        });

    showOutputPanel("[Governor] Task " + std::to_string(taskId) + " submitted: " + command);

    ReplayJournal::instance().recordGovernorEvent(
        ReplayActionType::GovernorSubmit, command, taskId);
}

void Win32IDE::cmdGovernorKillAll() {
    int result = MessageBoxA(m_hwnd,
        "Kill ALL running governor tasks?",
        "RawrXD Governor - Kill All", MB_YESNO | MB_ICONEXCLAMATION);

    if (result == IDYES) {
        ExecutionGovernor::instance().killAll();
        showOutputPanel("[Governor] All tasks killed");
        ReplayJournal::instance().recordGovernorEvent(
            ReplayActionType::GovernorKill, "kill-all");
    }
}

void Win32IDE::cmdGovernorTaskList() {
    auto descriptions = ExecutionGovernor::instance().getActiveTaskDescriptions();
    std::ostringstream oss;
    oss << "════════════════════════════════════════════\n"
        << "  Active Governor Tasks\n"
        << "════════════════════════════════════════════\n";
    if (descriptions.empty()) {
        oss << "  (no active tasks)\n";
    } else {
        for (const auto& desc : descriptions) {
            oss << "  " << desc << "\n";
        }
    }
    oss << "════════════════════════════════════════════";
    showOutputPanel(oss.str());
}

// ============================================================================
// 2. COMMAND HANDLERS — Safety (5122-5125)
// ============================================================================

void Win32IDE::cmdSafetyStatus() {
    std::string status = AgentSafetyContract::instance().getStatusString();
    showOutputPanel(status);
}

void Win32IDE::cmdSafetyResetBudget() {
    int result = MessageBoxA(m_hwnd,
        "Reset all intent budgets to default?\n\n"
        "This will allow the agent to perform actions that may have been budget-limited.",
        "RawrXD Safety - Reset Budget", MB_YESNO | MB_ICONQUESTION);

    if (result == IDYES) {
        AgentSafetyContract::instance().resetBudget();
        showOutputPanel("[Safety] Intent budgets reset to default");
        ReplayJournal::instance().recordSafetyEvent(
            ReplayActionType::SafetyCheck, "Budget reset by user");
    }
}

void Win32IDE::cmdSafetyRollbackLast() {
    bool success = AgentSafetyContract::instance().rollbackLast();
    if (success) {
        showOutputPanel("[Safety] Last action rolled back successfully");
        ReplayJournal::instance().recordSafetyEvent(
            ReplayActionType::SafetyRollback, "Rollback last action");
    } else {
        showOutputPanel("[Safety] No rollback available");
    }
}

void Win32IDE::cmdSafetyShowViolations() {
    auto violations = AgentSafetyContract::instance().getViolations();
    std::ostringstream oss;
    oss << "════════════════════════════════════════════\n"
        << "  Safety Violation Log (" << violations.size() << " records)\n"
        << "════════════════════════════════════════════\n";

    size_t startIdx = violations.size() > 50 ? violations.size() - 50 : 0;
    for (size_t i = startIdx; i < violations.size(); i++) {
        const auto& v = violations[i];
        oss << "  [" << v.id << "] "
            << violationTypeToString(v.type) << " — "
            << actionClassToString(v.attemptedAction) << " (risk: "
            << safetyRiskTierToString(v.riskTier) << ")\n"
            << "         " << v.description << "\n";
    }
    if (violations.empty()) {
        oss << "  (no violations recorded)\n";
    }
    oss << "════════════════════════════════════════════";
    showOutputPanel(oss.str());
}

// ============================================================================
// 2. COMMAND HANDLERS — Replay (5126-5129)
// ============================================================================

void Win32IDE::cmdReplayStatus() {
    std::string status = ReplayJournal::instance().getStatusString();
    showOutputPanel(status);
}

void Win32IDE::cmdReplayShowLast() {
    auto records = ReplayJournal::instance().getLastN(30);
    std::ostringstream oss;
    oss << "════════════════════════════════════════════\n"
        << "  Replay Journal — Last 30 Actions\n"
        << "════════════════════════════════════════════\n";

    for (const auto& rec : records) {
        oss << "  [" << rec.sequenceId << "] "
            << replayActionTypeToString(rec.type) << " — "
            << rec.category << ": " << rec.action;
        if (!rec.input.empty()) {
            std::string truncInput = rec.input.substr(0, 60);
            if (rec.input.length() > 60) truncInput += "...";
            oss << "\n         Input: " << truncInput;
        }
        if (rec.durationMs > 0) {
            oss << " (" << std::fixed << std::setprecision(0) << rec.durationMs << "ms)";
        }
        oss << "\n";
    }
    if (records.empty()) {
        oss << "  (no records)\n";
    }
    oss << "════════════════════════════════════════════";
    showOutputPanel(oss.str());
}

void Win32IDE::cmdReplayExportSession() {
    uint64_t sessionId = ReplayJournal::instance().getActiveSessionId();
    if (sessionId == 0) {
        showOutputPanel("[Replay] No active session to export");
        return;
    }

    std::string filename = "replay_session_" + std::to_string(sessionId) + ".log";
    bool success = ReplayJournal::instance().exportSession(sessionId, filename);
    if (success) {
        showOutputPanel("[Replay] Session " + std::to_string(sessionId) +
                        " exported to " + filename);
    } else {
        showOutputPanel("[Replay] Export failed — no records found");
    }
}

void Win32IDE::cmdReplayCheckpoint() {
    std::string label = showInputDialog("Replay: Create Checkpoint",
        "Enter a label for this checkpoint:");
    if (label.empty()) label = "manual-checkpoint";

    uint64_t seqId = ReplayJournal::instance().recordCheckpoint(label);
    showOutputPanel("[Replay] Checkpoint '" + label + "' recorded (seq " +
                    std::to_string(seqId) + ")");
}

// ============================================================================
// 2. COMMAND HANDLERS — Confidence (5130-5131)
// ============================================================================

void Win32IDE::cmdConfidenceStatus() {
    std::string status = ConfidenceGate::instance().getStatusString();
    showOutputPanel(status);
}

void Win32IDE::cmdConfidenceSetPolicy() {
    // Cycle through policies
    GatePolicy current = ConfidenceGate::instance().getPolicy();
    GatePolicy next;
    switch (current) {
        case GatePolicy::Strict:   next = GatePolicy::Normal;   break;
        case GatePolicy::Normal:   next = GatePolicy::Relaxed;  break;
        case GatePolicy::Relaxed:  next = GatePolicy::Disabled; break;
        case GatePolicy::Disabled: next = GatePolicy::Strict;   break;
        default:                   next = GatePolicy::Normal;    break;
    }
    ConfidenceGate::instance().setPolicy(next);
    showOutputPanel("[Confidence] Policy changed: " +
                    std::string(gatePolicyToString(current)) + " → " +
                    std::string(gatePolicyToString(next)));
    ReplayJournal::instance().recordAction(
        ReplayActionType::MenuCommand, "confidence", "set-policy",
        gatePolicyToString(current), gatePolicyToString(next));
}

// ============================================================================
// 3. HTTP ENDPOINT HANDLERS — Governor
// ============================================================================

void Win32IDE::handleGovernorStatusEndpoint(SOCKET client) {
    auto stats = ExecutionGovernor::instance().getStats();
    auto descriptions = ExecutionGovernor::instance().getActiveTaskDescriptions();

    std::ostringstream j;
    j << "{\"initialized\":" << (m_phase10Initialized ? "true" : "false")
      << ",\"stats\":{"
      << "\"totalSubmitted\":" << stats.totalSubmitted
      << ",\"totalCompleted\":" << stats.totalCompleted
      << ",\"totalTimedOut\":" << stats.totalTimedOut
      << ",\"totalKilled\":" << stats.totalKilled
      << ",\"totalCancelled\":" << stats.totalCancelled
      << ",\"totalFailed\":" << stats.totalFailed
      << ",\"totalRollbacks\":" << stats.totalRollbacks
      << ",\"activeTaskCount\":" << stats.activeTaskCount
      << ",\"peakConcurrent\":" << stats.peakConcurrent
      << ",\"totalElapsedMs\":" << std::fixed << std::setprecision(1) << stats.totalElapsedMs
      << ",\"avgTaskDurationMs\":" << stats.avgTaskDurationMs
      << ",\"longestTaskMs\":" << stats.longestTaskMs
      << "},\"activeTasks\":[";

    for (size_t i = 0; i < descriptions.size(); i++) {
        if (i > 0) j << ",";
        j << "\"" << LocalServerUtil::escapeJson(descriptions[i]) << "\"";
    }
    j << "]}";

    std::string response = LocalServerUtil::buildHttpResponse(200, j.str());
    send(client, response.c_str(), (int)response.size(), 0);
}

void Win32IDE::handleGovernorSubmitEndpoint(SOCKET client, const std::string& body) {
    // Parse command from body JSON
    std::string command;
    uint64_t timeoutMs = TerminalWatchdog::DEFAULT_AGENT_TIMEOUT_MS;

    // Manual JSON parse (our custom nlohmann::json, but this is HTTP layer)
    size_t cmdPos = body.find("\"command\":");
    if (cmdPos != std::string::npos) {
        size_t start = body.find('"', cmdPos + 10);
        if (start != std::string::npos) {
            size_t end = body.find('"', start + 1);
            while (end != std::string::npos && body[end - 1] == '\\') {
                end = body.find('"', end + 1);
            }
            if (end != std::string::npos) {
                command = body.substr(start + 1, end - start - 1);
            }
        }
    }

    size_t toPos = body.find("\"timeoutMs\":");
    if (toPos != std::string::npos) {
        size_t numStart = toPos + 12;
        while (numStart < body.length() && (body[numStart] == ' ' || body[numStart] == '\t')) numStart++;
        std::string numStr;
        while (numStart < body.length() && body[numStart] >= '0' && body[numStart] <= '9') {
            numStr += body[numStart++];
        }
        if (!numStr.empty()) timeoutMs = std::stoull(numStr);
    }

    if (command.empty()) {
        std::string err = LocalServerUtil::buildHttpResponse(400,
            "{\"error\":\"missing_command\",\"message\":\"'command' field required\"}");
        send(client, err.c_str(), (int)err.size(), 0);
        return;
    }

    // Safety check
    auto safetyResult = AgentSafetyContract::instance().checkAndConsume(
        ActionClass::RunCommand, SafetyRiskTier::High, command);

    if (safetyResult.verdict == ContractVerdict::Denied) {
        std::string err = LocalServerUtil::buildHttpResponse(403,
            "{\"error\":\"safety_denied\",\"verdict\":\"" +
            std::string(contractVerdictToString(safetyResult.verdict)) +
            "\",\"reason\":\"" + LocalServerUtil::escapeJson(safetyResult.reason) +
            "\",\"suggestion\":\"" + LocalServerUtil::escapeJson(safetyResult.suggestion) + "\"}");
        send(client, err.c_str(), (int)err.size(), 0);
        return;
    }

    GovernorTaskId taskId = ExecutionGovernor::instance().submitCommand(
        command, timeoutMs, GovernorRiskTier::High, command);

    ReplayJournal::instance().recordGovernorEvent(
        ReplayActionType::GovernorSubmit, command, taskId);

    std::string resp = LocalServerUtil::buildHttpResponse(200,
        "{\"taskId\":" + std::to_string(taskId) +
        ",\"command\":\"" + LocalServerUtil::escapeJson(command) +
        "\",\"timeoutMs\":" + std::to_string(timeoutMs) +
        ",\"status\":\"submitted\"}");
    send(client, resp.c_str(), (int)resp.size(), 0);
}

void Win32IDE::handleGovernorKillEndpoint(SOCKET client, const std::string& body) {
    // Parse taskId
    size_t idPos = body.find("\"taskId\":");
    uint64_t taskId = 0;
    if (idPos != std::string::npos) {
        size_t numStart = idPos + 9;
        while (numStart < body.length() && (body[numStart] == ' ' || body[numStart] == '\t')) numStart++;
        std::string numStr;
        while (numStart < body.length() && body[numStart] >= '0' && body[numStart] <= '9') {
            numStr += body[numStart++];
        }
        if (!numStr.empty()) taskId = std::stoull(numStr);
    }

    bool killed = false;
    if (taskId == 0) {
        // Kill all
        ExecutionGovernor::instance().killAll();
        killed = true;
        ReplayJournal::instance().recordGovernorEvent(
            ReplayActionType::GovernorKill, "kill-all-via-api");
    } else {
        killed = ExecutionGovernor::instance().killTask(taskId);
        if (killed) {
            ReplayJournal::instance().recordGovernorEvent(
                ReplayActionType::GovernorKill, "kill-task-" + std::to_string(taskId), taskId);
        }
    }

    std::string resp = LocalServerUtil::buildHttpResponse(200,
        "{\"killed\":" + std::string(killed ? "true" : "false") +
        ",\"taskId\":" + std::to_string(taskId) + "}");
    send(client, resp.c_str(), (int)resp.size(), 0);
}

void Win32IDE::handleGovernorResultEndpoint(SOCKET client, const std::string& body) {
    size_t idPos = body.find("\"taskId\":");
    uint64_t taskId = 0;
    if (idPos != std::string::npos) {
        size_t numStart = idPos + 9;
        while (numStart < body.length() && (body[numStart] == ' ' || body[numStart] == '\t')) numStart++;
        std::string numStr;
        while (numStart < body.length() && body[numStart] >= '0' && body[numStart] <= '9') {
            numStr += body[numStart++];
        }
        if (!numStr.empty()) taskId = std::stoull(numStr);
    }

    GovernorCommandResult result;
    bool found = ExecutionGovernor::instance().getTaskResult(taskId, result);

    if (!found) {
        // Task might still be running
        bool active = ExecutionGovernor::instance().isTaskActive(taskId);
        std::string resp = LocalServerUtil::buildHttpResponse(200,
            "{\"taskId\":" + std::to_string(taskId) +
            ",\"found\":false,\"active\":" + (active ? "true" : "false") + "}");
        send(client, resp.c_str(), (int)resp.size(), 0);
        return;
    }

    std::ostringstream j;
    j << "{\"taskId\":" << taskId
      << ",\"found\":true"
      << ",\"exitCode\":" << result.exitCode
      << ",\"timedOut\":" << (result.timedOut ? "true" : "false")
      << ",\"cancelled\":" << (result.cancelled ? "true" : "false")
      << ",\"durationMs\":" << std::fixed << std::setprecision(1) << result.durationMs
      << ",\"bytesRead\":" << result.bytesRead
      << ",\"statusDetail\":\"" << LocalServerUtil::escapeJson(result.statusDetail) << "\""
      << ",\"output\":\"" << LocalServerUtil::escapeJson(result.output) << "\"}";

    std::string resp = LocalServerUtil::buildHttpResponse(200, j.str());
    send(client, resp.c_str(), (int)resp.size(), 0);
}

// ============================================================================
// 3. HTTP ENDPOINT HANDLERS — Safety
// ============================================================================

void Win32IDE::handleSafetyStatusEndpoint(SOCKET client) {
    auto stats = AgentSafetyContract::instance().getStats();
    auto budget = AgentSafetyContract::instance().getBudget();

    std::ostringstream j;
    j << "{\"stats\":{"
      << "\"totalChecks\":" << stats.totalChecks
      << ",\"totalAllowed\":" << stats.totalAllowed
      << ",\"totalDenied\":" << stats.totalDenied
      << ",\"totalEscalated\":" << stats.totalEscalated
      << ",\"totalRollbacks\":" << stats.totalRollbacks
      << ",\"totalViolations\":" << stats.totalViolations
      << ",\"budgetExceeded\":" << stats.budgetExceeded
      << ",\"riskBlocked\":" << stats.riskBlocked
      << ",\"rateLimited\":" << stats.rateLimited
      << ",\"maxRiskSeen\":\"" << safetyRiskTierToString(stats.maxRiskSeen) << "\""
      << "},\"budget\":{"
      << "\"fileWrites\":{\"used\":" << budget.usedFileWrites << ",\"max\":" << budget.maxFileWrites << "}"
      << ",\"fileDeletes\":{\"used\":" << budget.usedFileDeletes << ",\"max\":" << budget.maxFileDeletes << "}"
      << ",\"commandRuns\":{\"used\":" << budget.usedCommandRuns << ",\"max\":" << budget.maxCommandRuns << "}"
      << ",\"buildRuns\":{\"used\":" << budget.usedBuildRuns << ",\"max\":" << budget.maxBuildRuns << "}"
      << ",\"networkRequests\":{\"used\":" << budget.usedNetworkRequests << ",\"max\":" << budget.maxNetworkRequests << "}"
      << ",\"modelModifications\":{\"used\":" << budget.usedModelModifications << ",\"max\":" << budget.maxModelModifications << "}"
      << ",\"processKills\":{\"used\":" << budget.usedProcessKills << ",\"max\":" << budget.maxProcessKills << "}"
      << ",\"total\":{\"used\":" << budget.usedTotalActions << ",\"max\":" << budget.maxTotalActions << "}"
      << "},\"maxAllowedRisk\":\"" << safetyRiskTierToString(AgentSafetyContract::instance().getMaxAllowedRisk()) << "\""
      << "}";

    std::string response = LocalServerUtil::buildHttpResponse(200, j.str());
    send(client, response.c_str(), (int)response.size(), 0);
}

void Win32IDE::handleSafetyCheckEndpoint(SOCKET client, const std::string& body) {
    // Parse action, risk, confidence from body
    int actionInt = 99;
    int riskInt = 2;
    float confidence = 1.0f;
    std::string description;

    auto extractInt = [&body](const std::string& key) -> int {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = body.find(searchKey);
        if (pos == std::string::npos) return -1;
        pos += searchKey.length();
        while (pos < body.length() && (body[pos] == ' ' || body[pos] == '\t')) pos++;
        std::string numStr;
        while (pos < body.length() && (body[pos] >= '0' && body[pos] <= '9')) {
            numStr += body[pos++];
        }
        if (numStr.empty()) return -1;
        return std::stoi(numStr);
    };

    auto extractFloat = [&body](const std::string& key) -> float {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = body.find(searchKey);
        if (pos == std::string::npos) return -1.0f;
        pos += searchKey.length();
        while (pos < body.length() && (body[pos] == ' ' || body[pos] == '\t')) pos++;
        std::string numStr;
        while (pos < body.length() && (body[pos] >= '0' && body[pos] <= '9' || body[pos] == '.' || body[pos] == '-')) {
            numStr += body[pos++];
        }
        if (numStr.empty()) return -1.0f;
        return std::stof(numStr);
    };

    int val = extractInt("action");
    if (val >= 0) actionInt = val;
    val = extractInt("risk");
    if (val >= 0) riskInt = val;
    float fval = extractFloat("confidence");
    if (fval >= 0) confidence = fval;

    auto result = AgentSafetyContract::instance().checkAction(
        (ActionClass)actionInt,
        (SafetyRiskTier)riskInt,
        description,
        confidence);

    std::ostringstream j;
    j << "{\"verdict\":\"" << contractVerdictToString(result.verdict) << "\""
      << ",\"reason\":\"" << LocalServerUtil::escapeJson(result.reason) << "\""
      << ",\"suggestion\":\"" << LocalServerUtil::escapeJson(result.suggestion) << "\""
      << ",\"remainingBudget\":" << result.remainingBudget
      << ",\"riskScore\":" << std::fixed << std::setprecision(3) << result.riskScore
      << "}";

    std::string response = LocalServerUtil::buildHttpResponse(200, j.str());
    send(client, response.c_str(), (int)response.size(), 0);
}

void Win32IDE::handleSafetyViolationsEndpoint(SOCKET client) {
    auto violations = AgentSafetyContract::instance().getViolations();

    std::ostringstream j;
    j << "{\"count\":" << violations.size() << ",\"violations\":[";

    size_t startIdx = violations.size() > 100 ? violations.size() - 100 : 0;
    bool first = true;
    for (size_t i = startIdx; i < violations.size(); i++) {
        if (!first) j << ",";
        first = false;
        const auto& v = violations[i];
        j << "{\"id\":" << v.id
          << ",\"type\":\"" << violationTypeToString(v.type) << "\""
          << ",\"action\":\"" << actionClassToString(v.attemptedAction) << "\""
          << ",\"risk\":\"" << safetyRiskTierToString(v.riskTier) << "\""
          << ",\"description\":\"" << LocalServerUtil::escapeJson(v.description) << "\""
          << ",\"agentId\":\"" << LocalServerUtil::escapeJson(v.agentId) << "\""
          << ",\"wasEscalated\":" << (v.wasEscalated ? "true" : "false")
          << ",\"userApproved\":" << (v.userApproved ? "true" : "false")
          << "}";
    }
    j << "]}";

    std::string response = LocalServerUtil::buildHttpResponse(200, j.str());
    send(client, response.c_str(), (int)response.size(), 0);
}

void Win32IDE::handleSafetyRollbackEndpoint(SOCKET client, const std::string& body) {
    // Parse optional actionId (0 = rollback last)
    uint64_t actionId = 0;
    size_t idPos = body.find("\"actionId\":");
    if (idPos != std::string::npos) {
        size_t numStart = idPos + 11;
        while (numStart < body.length() && (body[numStart] == ' ' || body[numStart] == '\t')) numStart++;
        std::string numStr;
        while (numStart < body.length() && body[numStart] >= '0' && body[numStart] <= '9') {
            numStr += body[numStart++];
        }
        if (!numStr.empty()) actionId = std::stoull(numStr);
    }

    bool success = false;
    if (actionId > 0) {
        success = AgentSafetyContract::instance().executeRollback(actionId);
    } else {
        success = AgentSafetyContract::instance().rollbackLast();
    }

    if (success) {
        ReplayJournal::instance().recordSafetyEvent(
            ReplayActionType::SafetyRollback,
            "Rollback via API (actionId=" + std::to_string(actionId) + ")");
    }

    std::string resp = LocalServerUtil::buildHttpResponse(200,
        "{\"success\":" + std::string(success ? "true" : "false") +
        ",\"actionId\":" + std::to_string(actionId) + "}");
    send(client, resp.c_str(), (int)resp.size(), 0);
}

// ============================================================================
// 3. HTTP ENDPOINT HANDLERS — Replay
// ============================================================================

void Win32IDE::handleReplayStatusEndpoint(SOCKET client) {
    auto stats = ReplayJournal::instance().getStats();

    std::ostringstream j;
    j << "{\"state\":\"" << replayStateToString(stats.state) << "\""
      << ",\"activeSessionId\":" << stats.activeSessionId
      << ",\"totalRecords\":" << stats.totalRecords
      << ",\"recordsInMemory\":" << stats.recordsInMemory
      << ",\"recordsFlushedToDisk\":" << stats.recordsFlushedToDisk
      << ",\"totalSessions\":" << stats.totalSessions
      << ",\"currentSequenceId\":" << stats.currentSequenceId
      << ",\"memoryUsageBytes\":" << stats.memoryUsageBytes
      << "}";

    std::string response = LocalServerUtil::buildHttpResponse(200, j.str());
    send(client, response.c_str(), (int)response.size(), 0);
}

void Win32IDE::handleReplayRecordsEndpoint(SOCKET client, const std::string& body) {
    uint64_t count = 50;
    size_t countPos = body.find("\"count\":");
    if (countPos != std::string::npos) {
        size_t numStart = countPos + 8;
        while (numStart < body.length() && (body[numStart] == ' ' || body[numStart] == '\t')) numStart++;
        std::string numStr;
        while (numStart < body.length() && body[numStart] >= '0' && body[numStart] <= '9') {
            numStr += body[numStart++];
        }
        if (!numStr.empty()) count = std::stoull(numStr);
    }
    if (count > 500) count = 500;

    auto records = ReplayJournal::instance().getLastN(count);

    std::ostringstream j;
    j << "{\"count\":" << records.size() << ",\"records\":[";

    for (size_t i = 0; i < records.size(); i++) {
        if (i > 0) j << ",";
        const auto& r = records[i];
        j << "{\"sequenceId\":" << r.sequenceId
          << ",\"sessionId\":" << r.sessionId
          << ",\"type\":\"" << replayActionTypeToString(r.type) << "\""
          << ",\"category\":\"" << LocalServerUtil::escapeJson(r.category) << "\""
          << ",\"action\":\"" << LocalServerUtil::escapeJson(r.action) << "\""
          << ",\"exitCode\":" << r.exitCode
          << ",\"confidence\":" << std::fixed << std::setprecision(3) << r.confidence
          << ",\"durationMs\":" << std::setprecision(1) << r.durationMs
          << ",\"agentId\":\"" << LocalServerUtil::escapeJson(r.agentId) << "\""
          << "}";
    }
    j << "]}";

    std::string response = LocalServerUtil::buildHttpResponse(200, j.str());
    send(client, response.c_str(), (int)response.size(), 0);
}

void Win32IDE::handleReplaySessionsEndpoint(SOCKET client) {
    auto sessions = ReplayJournal::instance().getAllSessions();

    std::ostringstream j;
    j << "{\"count\":" << sessions.size() << ",\"sessions\":[";

    for (size_t i = 0; i < sessions.size(); i++) {
        if (i > 0) j << ",";
        const auto& s = sessions[i];
        j << "{\"sessionId\":" << s.sessionId
          << ",\"label\":\"" << LocalServerUtil::escapeJson(s.sessionLabel) << "\""
          << ",\"startTime\":\"" << s.startTime << "\""
          << ",\"endTime\":\"" << s.endTime << "\""
          << ",\"actionCount\":" << s.actionCount
          << ",\"agentQueries\":" << s.agentQueries
          << ",\"commandsRun\":" << s.commandsRun
          << ",\"filesModified\":" << s.filesModified
          << ",\"safetyDenials\":" << s.safetyDenials
          << ",\"errors\":" << s.errors
          << "}";
    }
    j << "]}";

    std::string response = LocalServerUtil::buildHttpResponse(200, j.str());
    send(client, response.c_str(), (int)response.size(), 0);
}

// ============================================================================
// 3. HTTP ENDPOINT HANDLERS — Confidence
// ============================================================================

void Win32IDE::handleConfidenceStatusEndpoint(SOCKET client) {
    auto stats = ConfidenceGate::instance().getStats();
    auto thresholds = ConfidenceGate::instance().getThresholds();

    std::ostringstream j;
    j << "{\"enabled\":" << (ConfidenceGate::instance().isEnabled() ? "true" : "false")
      << ",\"policy\":\"" << gatePolicyToString(stats.currentPolicy) << "\""
      << ",\"selfAbortTriggered\":" << (ConfidenceGate::instance().isSelfAbortTriggered() ? "true" : "false")
      << ",\"thresholds\":{"
      << "\"execute\":" << std::fixed << std::setprecision(3) << thresholds.executeThreshold
      << ",\"escalate\":" << thresholds.escalateThreshold
      << ",\"abort\":" << thresholds.abortThreshold
      << "},\"stats\":{"
      << "\"totalEvaluations\":" << stats.totalEvaluations
      << ",\"totalExecuted\":" << stats.totalExecuted
      << ",\"totalEscalated\":" << stats.totalEscalated
      << ",\"totalAborted\":" << stats.totalAborted
      << ",\"totalDeferred\":" << stats.totalDeferred
      << ",\"totalOverridden\":" << stats.totalOverridden
      << ",\"avgConfidence\":" << stats.avgConfidence
      << ",\"recentAvgConfidence\":" << stats.recentAvgConfidence
      << ",\"minConfidence\":" << stats.minConfidence
      << ",\"maxConfidence\":" << stats.maxConfidence
      << ",\"trend\":\"" << confidenceTrendToString(stats.overallTrend) << "\""
      << "}}";

    std::string response = LocalServerUtil::buildHttpResponse(200, j.str());
    send(client, response.c_str(), (int)response.size(), 0);
}

void Win32IDE::handleConfidenceEvaluateEndpoint(SOCKET client, const std::string& body) {
    float confidence = 0.5f;
    int actionInt = 0;
    int riskInt = 2;

    auto extractFloat = [&body](const std::string& key) -> float {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = body.find(searchKey);
        if (pos == std::string::npos) return -1.0f;
        pos += searchKey.length();
        while (pos < body.length() && (body[pos] == ' ' || body[pos] == '\t')) pos++;
        std::string numStr;
        while (pos < body.length() && (body[pos] >= '0' && body[pos] <= '9' || body[pos] == '.' || body[pos] == '-')) {
            numStr += body[pos++];
        }
        if (numStr.empty()) return -1.0f;
        return std::stof(numStr);
    };

    auto extractInt = [&body](const std::string& key) -> int {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = body.find(searchKey);
        if (pos == std::string::npos) return -1;
        pos += searchKey.length();
        while (pos < body.length() && (body[pos] == ' ' || body[pos] == '\t')) pos++;
        std::string numStr;
        while (pos < body.length() && body[pos] >= '0' && body[pos] <= '9') {
            numStr += body[pos++];
        }
        if (numStr.empty()) return -1;
        return std::stoi(numStr);
    };

    float fval = extractFloat("confidence");
    if (fval >= 0) confidence = fval;
    int val = extractInt("action");
    if (val >= 0) actionInt = val;
    val = extractInt("risk");
    if (val >= 0) riskInt = val;

    auto eval = ConfidenceGate::instance().evaluateAndRecord(
        confidence, (ActionClass)actionInt, (SafetyRiskTier)riskInt);

    std::ostringstream j;
    j << "{\"decision\":\"" << confidenceDecisionToString(eval.decision) << "\""
      << ",\"rawConfidence\":" << std::fixed << std::setprecision(3) << eval.rawConfidence
      << ",\"adjustedConfidence\":" << eval.adjustedConfidence
      << ",\"effectiveThreshold\":" << eval.effectiveThreshold
      << ",\"reason\":\"" << LocalServerUtil::escapeJson(eval.reason) << "\""
      << ",\"suggestion\":\"" << LocalServerUtil::escapeJson(eval.suggestion) << "\""
      << ",\"trend\":\"" << confidenceTrendToString(eval.trend) << "\""
      << ",\"trendSlope\":" << std::setprecision(4) << eval.trendSlope
      << "}";

    std::string response = LocalServerUtil::buildHttpResponse(200, j.str());
    send(client, response.c_str(), (int)response.size(), 0);
}

void Win32IDE::handleConfidenceHistoryEndpoint(SOCKET client) {
    auto history = ConfidenceGate::instance().getHistory(100);

    std::ostringstream j;
    j << "{\"count\":" << history.size() << ",\"history\":[";

    for (size_t i = 0; i < history.size(); i++) {
        if (i > 0) j << ",";
        const auto& h = history[i];
        j << "{\"sequenceId\":" << h.sequenceId
          << ",\"confidence\":" << std::fixed << std::setprecision(3) << h.confidence
          << ",\"decision\":\"" << confidenceDecisionToString(h.decision) << "\""
          << "}";
    }
    j << "]}";

    std::string response = LocalServerUtil::buildHttpResponse(200, j.str());
    send(client, response.c_str(), (int)response.size(), 0);
}

// ============================================================================
// 3. HTTP ENDPOINT HANDLER — Unified Phase 10 Status
// ============================================================================

void Win32IDE::handlePhase10StatusEndpoint(SOCKET client) {
    auto govStats = ExecutionGovernor::instance().getStats();
    auto safetyStats = AgentSafetyContract::instance().getStats();
    auto replayStats = ReplayJournal::instance().getStats();
    auto confStats = ConfidenceGate::instance().getStats();

    std::ostringstream j;
    j << "{\"phase10\":{\"initialized\":" << (m_phase10Initialized ? "true" : "false")
      << ",\"governor\":{\"tasks\":" << govStats.totalSubmitted
      << ",\"active\":" << govStats.activeTaskCount
      << ",\"completed\":" << govStats.totalCompleted
      << ",\"timedOut\":" << govStats.totalTimedOut << "}"
      << ",\"safety\":{\"checks\":" << safetyStats.totalChecks
      << ",\"denied\":" << safetyStats.totalDenied
      << ",\"violations\":" << safetyStats.totalViolations << "}"
      << ",\"replay\":{\"records\":" << replayStats.totalRecords
      << ",\"sessions\":" << replayStats.totalSessions
      << ",\"state\":\"" << replayStateToString(replayStats.state) << "\"}"
      << ",\"confidence\":{\"evaluations\":" << confStats.totalEvaluations
      << ",\"avgConfidence\":" << std::fixed << std::setprecision(3) << confStats.avgConfidence
      << ",\"trend\":\"" << confidenceTrendToString(confStats.overallTrend) << "\""
      << ",\"selfAbort\":" << (ConfidenceGate::instance().isSelfAbortTriggered() ? "true" : "false")
      << "}}}";

    std::string response = LocalServerUtil::buildHttpResponse(200, j.str());
    send(client, response.c_str(), (int)response.size(), 0);
}
