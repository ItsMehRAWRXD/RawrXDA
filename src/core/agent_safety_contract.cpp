// ============================================================================
// agent_safety_contract.cpp — Phase 10B: Agent Safety Contracts
// ============================================================================
//
// Full implementation of intent budgets, risk tier enforcement, action
// classification, rollback guarantees, and violation tracking.
//
// Pattern:  Structured results (PatchResult-style), no exceptions
// Threading: All public methods are mutex-guarded
// Rule:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "agent_safety_contract.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>

// ============================================================================
// SINGLETON
// ============================================================================

AgentSafetyContract& AgentSafetyContract::instance() {
    static AgentSafetyContract inst;
    return inst;
}

AgentSafetyContract::AgentSafetyContract()
    : m_initialized(false),
      m_maxAllowedRisk(SafetyRiskTier::Critical),
      m_autoApproveEscalations(false),
      m_nextViolationId(1),
      m_nextRollbackId(1) {}

AgentSafetyContract::~AgentSafetyContract() {
    shutdown();
}

// ============================================================================
// LIFECYCLE
// ============================================================================

bool AgentSafetyContract::init() {
    if (m_initialized.load()) return true;

    std::lock_guard<std::mutex> lock(m_mutex);

    // Initialize default rules for each action class
    m_rules.clear();
    m_rules.push_back({ActionClass::ReadFile,       SafetyRiskTier::None,     false, false, "Read file contents"});
    m_rules.push_back({ActionClass::SearchCode,     SafetyRiskTier::None,     false, false, "Search codebase"});
    m_rules.push_back({ActionClass::QueryModel,     SafetyRiskTier::Low,      false, false, "Query AI model"});
    m_rules.push_back({ActionClass::ListDirectory,  SafetyRiskTier::None,     false, false, "List directory"});
    m_rules.push_back({ActionClass::InspectState,   SafetyRiskTier::None,     false, false, "Inspect system state"});
    m_rules.push_back({ActionClass::WriteFile,      SafetyRiskTier::Medium,   false, true,  "Write file"});
    m_rules.push_back({ActionClass::EditFile,       SafetyRiskTier::Medium,   false, true,  "Edit file"});
    m_rules.push_back({ActionClass::CreateFile,     SafetyRiskTier::Medium,   false, true,  "Create new file"});
    m_rules.push_back({ActionClass::DeleteFile,     SafetyRiskTier::High,     true,  true,  "Delete file"});
    m_rules.push_back({ActionClass::RenameFile,     SafetyRiskTier::Medium,   false, true,  "Rename file"});
    m_rules.push_back({ActionClass::RunCommand,     SafetyRiskTier::High,     false, false, "Run terminal command"});
    m_rules.push_back({ActionClass::RunBuild,       SafetyRiskTier::High,     false, false, "Run build"});
    m_rules.push_back({ActionClass::RunTest,        SafetyRiskTier::Medium,   false, false, "Run tests"});
    m_rules.push_back({ActionClass::InstallPackage, SafetyRiskTier::High,     true,  false, "Install package"});
    m_rules.push_back({ActionClass::NetworkRequest, SafetyRiskTier::Medium,   false, false, "Network request"});
    m_rules.push_back({ActionClass::ModifyModel,    SafetyRiskTier::Critical, true,  true,  "Modify AI model"});
    m_rules.push_back({ActionClass::SystemConfig,   SafetyRiskTier::Critical, true,  true,  "System configuration"});
    m_rules.push_back({ActionClass::ProcessKill,    SafetyRiskTier::High,     true,  false, "Kill process"});
    m_rules.push_back({ActionClass::PatchMemory,    SafetyRiskTier::Critical, true,  true,  "Patch memory"});
    m_rules.push_back({ActionClass::ServerRestart,  SafetyRiskTier::High,     true,  false, "Restart server"});

    // Default budget
    m_budget = IntentBudget();

    // Clear state
    m_violations.clear();
    m_rollbackStack.clear();
    m_blockedActions.clear();
    m_stats = SafetyStats();

    m_initialized.store(true);
    return true;
}

void AgentSafetyContract::shutdown() {
    if (!m_initialized.load()) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_initialized.store(false);
}

void AgentSafetyContract::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_budget = IntentBudget();
    m_violations.clear();
    m_rollbackStack.clear();
    m_blockedActions.clear();
    m_stats = SafetyStats();
    m_nextViolationId = 1;
    m_nextRollbackId = 1;
}

// ============================================================================
// CONTRACT CHECKS
// ============================================================================

ContractCheckResult AgentSafetyContract::checkAction(
    ActionClass action,
    SafetyRiskTier risk,
    const std::string& description,
    float confidence)
{
    if (!m_initialized.load()) init();

    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.totalChecks++;

    // Track max risk seen
    if (risk > m_stats.maxRiskSeen) {
        m_stats.maxRiskSeen = risk;
    }

    // ── Check 1: Is this action class explicitly blocked? ───────────────
    auto blockIt = m_blockedActions.find((int)action);
    if (blockIt != m_blockedActions.end() && blockIt->second) {
        m_stats.totalDenied++;
        recordViolation(ViolationType::ActionClassBlocked, action, risk,
                        "Action class '" + std::string(actionClassToString(action)) + "' is blocked");
        return ContractCheckResult::denied(
            ViolationType::ActionClassBlocked,
            "Action class '" + std::string(actionClassToString(action)) + "' is blocked by policy",
            "Unblock this action class or use an alternative approach");
    }

    // ── Check 2: Risk tier enforcement ──────────────────────────────────
    if (risk > m_maxAllowedRisk) {
        m_stats.totalDenied++;
        m_stats.riskBlocked++;
        recordViolation(ViolationType::RiskTierBlocked, action, risk,
                        "Risk tier " + std::string(safetyRiskTierToString(risk)) +
                        " exceeds max " + std::string(safetyRiskTierToString(m_maxAllowedRisk)));
        return ContractCheckResult::denied(
            ViolationType::RiskTierBlocked,
            "Risk tier " + std::string(safetyRiskTierToString(risk)) +
            " exceeds maximum allowed " + std::string(safetyRiskTierToString(m_maxAllowedRisk)),
            "Lower the risk level or increase max allowed risk tier");
    }

    // ── Check 3: Confidence threshold ───────────────────────────────────
    if (confidence < MIN_CONFIDENCE_THRESHOLD) {
        m_stats.totalDenied++;
        recordViolation(ViolationType::ConfidenceTooLow, action, risk,
                        "Confidence " + std::to_string(confidence) + " below threshold " +
                        std::to_string(MIN_CONFIDENCE_THRESHOLD));
        return ContractCheckResult::denied(
            ViolationType::ConfidenceTooLow,
            "Agent confidence " + std::to_string(confidence) + " is below minimum threshold " +
            std::to_string(MIN_CONFIDENCE_THRESHOLD),
            "Increase confidence or lower the threshold");
    }

    // ── Check 4: Rate limiting ──────────────────────────────────────────
    if (!checkRateLimit()) {
        m_stats.totalDenied++;
        m_stats.rateLimited++;
        recordViolation(ViolationType::RateLimitExceeded, action, risk,
                        "Rate limit exceeded: " + std::to_string(m_budget.maxActionsPerMinute) + "/min");
        return ContractCheckResult::denied(
            ViolationType::RateLimitExceeded,
            "Rate limit exceeded (" + std::to_string(m_budget.maxActionsPerMinute) + " actions/minute)",
            "Wait before retrying, or increase rate limit");
    }

    // ── Check 5: Budget enforcement ─────────────────────────────────────
    const int* limit = getBudgetLimitForAction(action);
    int* counter = const_cast<int*>(getBudgetCounterForAction(action));
    if (limit && counter) {
        if (*counter >= *limit) {
            m_stats.totalDenied++;
            m_stats.budgetExceeded++;
            recordViolation(ViolationType::BudgetExceeded, action, risk,
                            "Budget exhausted for " + std::string(actionClassToString(action)) +
                            " (" + std::to_string(*counter) + "/" + std::to_string(*limit) + ")");
            return ContractCheckResult::denied(
                ViolationType::BudgetExceeded,
                "Intent budget exhausted for " + std::string(actionClassToString(action)) +
                " (" + std::to_string(*counter) + "/" + std::to_string(*limit) + ")",
                "Reset budget or increase limit for this action class");
        }
    }

    // ── Check 6: Total budget ───────────────────────────────────────────
    if (m_budget.usedTotalActions >= m_budget.maxTotalActions) {
        m_stats.totalDenied++;
        m_stats.budgetExceeded++;
        recordViolation(ViolationType::BudgetExceeded, action, risk,
                        "Total action budget exhausted (" +
                        std::to_string(m_budget.usedTotalActions) + "/" +
                        std::to_string(m_budget.maxTotalActions) + ")");
        return ContractCheckResult::denied(
            ViolationType::BudgetExceeded,
            "Total action budget exhausted (" +
            std::to_string(m_budget.usedTotalActions) + "/" +
            std::to_string(m_budget.maxTotalActions) + ")",
            "Reset budget to continue");
    }

    // ── Check 7: Rule-based escalation ──────────────────────────────────
    for (const auto& rule : m_rules) {
        if (rule.actionClass == action) {
            if (rule.requireConfirmation && risk >= SafetyRiskTier::High) {
                // Check if we have an escalation callback
                if (m_escalationCallback && !m_autoApproveEscalations) {
                    m_stats.totalEscalated++;
                    return ContractCheckResult::escalated(
                        "Action '" + std::string(actionClassToString(action)) +
                        "' requires user confirmation (risk: " +
                        std::string(safetyRiskTierToString(risk)) + ")");
                }
                // Auto-approve mode or no callback — fall through
            }
            break;
        }
    }

    // ── All checks passed ───────────────────────────────────────────────
    m_stats.totalAllowed++;

    int remaining = -1;
    if (limit && counter) {
        remaining = *limit - *counter;
    }

    return ContractCheckResult::allowed(
        "Action '" + std::string(actionClassToString(action)) + "' permitted",
        remaining);
}

ContractCheckResult AgentSafetyContract::checkAndConsume(
    ActionClass action,
    SafetyRiskTier risk,
    const std::string& description,
    float confidence)
{
    // First do a dry-run check
    ContractCheckResult result = checkAction(action, risk, description, confidence);

    if (result.verdict != ContractVerdict::Allowed) {
        return result;
    }

    // Consume budget
    std::lock_guard<std::mutex> lock(m_mutex);

    int* counter = const_cast<int*>(getBudgetCounterForAction(action));
    const int* limit = getBudgetLimitForAction(action);
    if (counter) {
        (*counter)++;
    }
    m_budget.usedTotalActions++;
    m_budget.actionsThisMinute++;

    int remaining = -1;
    if (limit && counter) {
        remaining = *limit - *counter;
    }

    return ContractCheckResult::budgeted(
        "Action '" + std::string(actionClassToString(action)) + "' consumed",
        remaining);
}

// ============================================================================
// BUDGET MANAGEMENT
// ============================================================================

IntentBudget AgentSafetyContract::getBudget() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_budget;
}

void AgentSafetyContract::setBudget(const IntentBudget& budget) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_budget = budget;
}

void AgentSafetyContract::resetBudget() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_budget.usedFileWrites = 0;
    m_budget.usedFileDeletes = 0;
    m_budget.usedCommandRuns = 0;
    m_budget.usedBuildRuns = 0;
    m_budget.usedNetworkRequests = 0;
    m_budget.usedModelModifications = 0;
    m_budget.usedProcessKills = 0;
    m_budget.usedTotalActions = 0;
    m_budget.actionsThisMinute = 0;
    m_budget.minuteStart = std::chrono::steady_clock::now();
}

int AgentSafetyContract::getRemainingBudget(ActionClass action) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    const int* limit = getBudgetLimitForAction(action);
    const int* counter = getBudgetCounterForAction(action);
    if (limit && counter) {
        return *limit - *counter;
    }
    return -1; // No budget tracked for this action
}

bool AgentSafetyContract::isBudgetExhausted(ActionClass action) const {
    int remaining = getRemainingBudget(action);
    return (remaining == 0);
}

// ============================================================================
// RISK TIER POLICY
// ============================================================================

void AgentSafetyContract::setMaxAllowedRisk(SafetyRiskTier maxRisk) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxAllowedRisk = maxRisk;
}

SafetyRiskTier AgentSafetyContract::getMaxAllowedRisk() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_maxAllowedRisk;
}

void AgentSafetyContract::blockActionClass(ActionClass action) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_blockedActions[(int)action] = true;
}

void AgentSafetyContract::unblockActionClass(ActionClass action) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_blockedActions.erase((int)action);
}

bool AgentSafetyContract::isActionBlocked(ActionClass action) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_blockedActions.find((int)action);
    return (it != m_blockedActions.end() && it->second);
}

// ============================================================================
// RULES
// ============================================================================

void AgentSafetyContract::addRule(const SafetyContractRule& rule) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Replace existing rule for this action class
    for (auto& existing : m_rules) {
        if (existing.actionClass == rule.actionClass) {
            existing = rule;
            return;
        }
    }
    m_rules.push_back(rule);
}

void AgentSafetyContract::removeRule(ActionClass action) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_rules.erase(
        std::remove_if(m_rules.begin(), m_rules.end(),
                        [action](const SafetyContractRule& r) {
                            return r.actionClass == action;
                        }),
        m_rules.end());
}

std::vector<SafetyContractRule> AgentSafetyContract::getRules() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_rules;
}

// ============================================================================
// ROLLBACK
// ============================================================================

uint64_t AgentSafetyContract::registerRollback(
    ActionClass action,
    const std::string& description,
    const std::string& targetPath,
    const std::string& beforeState,
    std::function<void()> undoFn)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // Enforce max rollback entries
    if (m_rollbackStack.size() >= (size_t)MAX_ROLLBACK_ENTRIES) {
        m_rollbackStack.erase(m_rollbackStack.begin());
    }

    RollbackEntry entry;
    entry.actionId = m_nextRollbackId++;
    entry.actionClass = action;
    entry.description = description;
    entry.targetPath = targetPath;
    entry.beforeState = beforeState;
    entry.undoFn = undoFn;
    entry.timestamp = std::chrono::steady_clock::now();
    entry.executed = false;

    m_rollbackStack.push_back(std::move(entry));
    return entry.actionId;
}

bool AgentSafetyContract::executeRollback(uint64_t actionId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& entry : m_rollbackStack) {
        if (entry.actionId == actionId && !entry.executed) {
            if (entry.undoFn) {
                entry.undoFn();
            }
            entry.executed = true;
            m_stats.totalRollbacks++;
            return true;
        }
    }
    return false;
}

bool AgentSafetyContract::rollbackLast() {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto it = m_rollbackStack.rbegin(); it != m_rollbackStack.rend(); ++it) {
        if (!it->executed) {
            if (it->undoFn) {
                it->undoFn();
            }
            it->executed = true;
            m_stats.totalRollbacks++;
            return true;
        }
    }
    return false;
}

bool AgentSafetyContract::rollbackAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    bool anyRolledBack = false;

    for (auto it = m_rollbackStack.rbegin(); it != m_rollbackStack.rend(); ++it) {
        if (!it->executed) {
            if (it->undoFn) {
                it->undoFn();
            }
            it->executed = true;
            m_stats.totalRollbacks++;
            anyRolledBack = true;
        }
    }
    return anyRolledBack;
}

std::vector<RollbackEntry> AgentSafetyContract::getRollbackHistory() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_rollbackStack;
}

// ============================================================================
// VIOLATIONS
// ============================================================================

std::vector<SafetyViolationRecord> AgentSafetyContract::getViolations() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_violations;
}

void AgentSafetyContract::clearViolations() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_violations.clear();
}

// ============================================================================
// ESCALATION
// ============================================================================

void AgentSafetyContract::setEscalationCallback(std::function<bool(const std::string&)> cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_escalationCallback = cb;
}

void AgentSafetyContract::setAutoApproveEscalations(bool autoApprove) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_autoApproveEscalations = autoApprove;
}

// ============================================================================
// STATS & REPORTING
// ============================================================================

SafetyStats AgentSafetyContract::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

std::string AgentSafetyContract::getStatusString() const {
    auto s = getStats();
    auto b = getBudget();

    std::ostringstream oss;
    oss << "════════════════════════════════════════════\n"
        << "  Agent Safety Contract Status (Phase 10B)\n"
        << "════════════════════════════════════════════\n"
        << "  Initialized:       " << (m_initialized.load() ? "YES" : "NO") << "\n"
        << "  Max Allowed Risk:  " << safetyRiskTierToString(m_maxAllowedRisk) << "\n"
        << "  Max Risk Seen:     " << safetyRiskTierToString(s.maxRiskSeen) << "\n"
        << "  ─────────────────────────────────────────\n"
        << "  Contract Checks:   " << s.totalChecks << "\n"
        << "  Allowed:           " << s.totalAllowed << "\n"
        << "  Denied:            " << s.totalDenied << "\n"
        << "  Escalated:         " << s.totalEscalated << "\n"
        << "  Rollbacks:         " << s.totalRollbacks << "\n"
        << "  Violations:        " << s.totalViolations << "\n"
        << "  ─────────────────────────────────────────\n"
        << "  Budget Exceeded:   " << s.budgetExceeded << "\n"
        << "  Risk Blocked:      " << s.riskBlocked << "\n"
        << "  Rate Limited:      " << s.rateLimited << "\n"
        << "  ─────────────────────────────────────────\n"
        << "  Budget Consumption:\n"
        << "    File Writes:     " << b.usedFileWrites << " / " << b.maxFileWrites << "\n"
        << "    File Deletes:    " << b.usedFileDeletes << " / " << b.maxFileDeletes << "\n"
        << "    Command Runs:    " << b.usedCommandRuns << " / " << b.maxCommandRuns << "\n"
        << "    Build Runs:      " << b.usedBuildRuns << " / " << b.maxBuildRuns << "\n"
        << "    Network Reqs:    " << b.usedNetworkRequests << " / " << b.maxNetworkRequests << "\n"
        << "    Model Mods:      " << b.usedModelModifications << " / " << b.maxModelModifications << "\n"
        << "    Process Kills:   " << b.usedProcessKills << " / " << b.maxProcessKills << "\n"
        << "    Total Actions:   " << b.usedTotalActions << " / " << b.maxTotalActions << "\n"
        << "  ─────────────────────────────────────────\n"
        << "  Blocked Actions:   " << m_blockedActions.size() << "\n"
        << "  Rules:             " << m_rules.size() << "\n"
        << "  Rollback Stack:    " << m_rollbackStack.size() << "\n"
        << "  Violation Records: " << m_violations.size() << "\n"
        << "════════════════════════════════════════════";
    return oss.str();
}

// ============================================================================
// SERIALIZATION
// ============================================================================

std::string AgentSafetyContract::serializeBudget() const {
    auto b = getBudget();
    std::ostringstream oss;
    oss << "{"
        << "\"maxFileWrites\":" << b.maxFileWrites << ","
        << "\"maxFileDeletes\":" << b.maxFileDeletes << ","
        << "\"maxCommandRuns\":" << b.maxCommandRuns << ","
        << "\"maxBuildRuns\":" << b.maxBuildRuns << ","
        << "\"maxNetworkRequests\":" << b.maxNetworkRequests << ","
        << "\"maxModelModifications\":" << b.maxModelModifications << ","
        << "\"maxProcessKills\":" << b.maxProcessKills << ","
        << "\"maxTotalActions\":" << b.maxTotalActions << ","
        << "\"maxActionsPerMinute\":" << b.maxActionsPerMinute << ","
        << "\"usedFileWrites\":" << b.usedFileWrites << ","
        << "\"usedFileDeletes\":" << b.usedFileDeletes << ","
        << "\"usedCommandRuns\":" << b.usedCommandRuns << ","
        << "\"usedBuildRuns\":" << b.usedBuildRuns << ","
        << "\"usedNetworkRequests\":" << b.usedNetworkRequests << ","
        << "\"usedModelModifications\":" << b.usedModelModifications << ","
        << "\"usedProcessKills\":" << b.usedProcessKills << ","
        << "\"usedTotalActions\":" << b.usedTotalActions
        << "}";
    return oss.str();
}

bool AgentSafetyContract::deserializeBudget(const std::string& json) {
    // Manual parse — our custom nlohmann::json has no array iterators
    // For budget, we do simple key-value extraction
    std::lock_guard<std::mutex> lock(m_mutex);

    auto extractInt = [&json](const std::string& key) -> int {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = json.find(searchKey);
        if (pos == std::string::npos) return -1;
        pos += searchKey.length();
        while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
        std::string numStr;
        while (pos < json.length() && (json[pos] >= '0' && json[pos] <= '9')) {
            numStr += json[pos++];
        }
        if (numStr.empty()) return -1;
        return std::stoi(numStr);
    };

    int val;
    val = extractInt("maxFileWrites");      if (val >= 0) m_budget.maxFileWrites = val;
    val = extractInt("maxFileDeletes");     if (val >= 0) m_budget.maxFileDeletes = val;
    val = extractInt("maxCommandRuns");     if (val >= 0) m_budget.maxCommandRuns = val;
    val = extractInt("maxBuildRuns");       if (val >= 0) m_budget.maxBuildRuns = val;
    val = extractInt("maxNetworkRequests"); if (val >= 0) m_budget.maxNetworkRequests = val;
    val = extractInt("maxModelModifications"); if (val >= 0) m_budget.maxModelModifications = val;
    val = extractInt("maxProcessKills");    if (val >= 0) m_budget.maxProcessKills = val;
    val = extractInt("maxTotalActions");    if (val >= 0) m_budget.maxTotalActions = val;
    val = extractInt("maxActionsPerMinute"); if (val >= 0) m_budget.maxActionsPerMinute = val;
    val = extractInt("usedFileWrites");     if (val >= 0) m_budget.usedFileWrites = val;
    val = extractInt("usedFileDeletes");    if (val >= 0) m_budget.usedFileDeletes = val;
    val = extractInt("usedCommandRuns");    if (val >= 0) m_budget.usedCommandRuns = val;
    val = extractInt("usedBuildRuns");      if (val >= 0) m_budget.usedBuildRuns = val;
    val = extractInt("usedNetworkRequests"); if (val >= 0) m_budget.usedNetworkRequests = val;
    val = extractInt("usedModelModifications"); if (val >= 0) m_budget.usedModelModifications = val;
    val = extractInt("usedProcessKills");   if (val >= 0) m_budget.usedProcessKills = val;
    val = extractInt("usedTotalActions");   if (val >= 0) m_budget.usedTotalActions = val;

    return true;
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

SafetyRiskTier AgentSafetyContract::getActionDefaultRisk(ActionClass action) const {
    int ac = (int)action;
    if (ac < 10) return SafetyRiskTier::None;
    if (ac < 20) return SafetyRiskTier::Medium;
    if (ac < 30) return SafetyRiskTier::High;
    if (ac < 40) return SafetyRiskTier::Critical;
    return SafetyRiskTier::Medium;
}

int* AgentSafetyContract::getBudgetCounterForAction(ActionClass action) {
    switch (action) {
        case ActionClass::WriteFile:
        case ActionClass::EditFile:
        case ActionClass::CreateFile:
            return &m_budget.usedFileWrites;
        case ActionClass::DeleteFile:
            return &m_budget.usedFileDeletes;
        case ActionClass::RunCommand:
            return &m_budget.usedCommandRuns;
        case ActionClass::RunBuild:
        case ActionClass::RunTest:
            return &m_budget.usedBuildRuns;
        case ActionClass::NetworkRequest:
        case ActionClass::InstallPackage:
            return &m_budget.usedNetworkRequests;
        case ActionClass::ModifyModel:
        case ActionClass::PatchMemory:
            return &m_budget.usedModelModifications;
        case ActionClass::ProcessKill:
        case ActionClass::ServerRestart:
            return &m_budget.usedProcessKills;
        default:
            return nullptr;
    }
}

const int* AgentSafetyContract::getBudgetCounterForAction(ActionClass action) const {
    return const_cast<AgentSafetyContract*>(this)->getBudgetCounterForAction(action);
}

const int* AgentSafetyContract::getBudgetLimitForAction(ActionClass action) const {
    switch (action) {
        case ActionClass::WriteFile:
        case ActionClass::EditFile:
        case ActionClass::CreateFile:
            return &m_budget.maxFileWrites;
        case ActionClass::DeleteFile:
            return &m_budget.maxFileDeletes;
        case ActionClass::RunCommand:
            return &m_budget.maxCommandRuns;
        case ActionClass::RunBuild:
        case ActionClass::RunTest:
            return &m_budget.maxBuildRuns;
        case ActionClass::NetworkRequest:
        case ActionClass::InstallPackage:
            return &m_budget.maxNetworkRequests;
        case ActionClass::ModifyModel:
        case ActionClass::PatchMemory:
            return &m_budget.maxModelModifications;
        case ActionClass::ProcessKill:
        case ActionClass::ServerRestart:
            return &m_budget.maxProcessKills;
        default:
            return nullptr;
    }
}

bool AgentSafetyContract::checkRateLimit() {
    // Already under lock
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - m_budget.minuteStart).count();

    if (elapsed >= 60) {
        // Reset minute window
        m_budget.actionsThisMinute = 0;
        m_budget.minuteStart = now;
    }

    return (m_budget.actionsThisMinute < m_budget.maxActionsPerMinute);
}

void AgentSafetyContract::recordViolation(
    ViolationType type, ActionClass action,
    SafetyRiskTier risk, const std::string& desc)
{
    // Already under lock
    m_stats.totalViolations++;

    if (m_violations.size() >= (size_t)MAX_VIOLATIONS) {
        m_violations.erase(m_violations.begin());
    }

    SafetyViolationRecord vr;
    vr.id = m_nextViolationId++;
    vr.type = type;
    vr.attemptedAction = action;
    vr.riskTier = risk;
    vr.description = desc;
    vr.agentId = "primary";
    vr.timestamp = std::chrono::steady_clock::now();
    vr.wasEscalated = false;
    vr.userApproved = false;

    m_violations.push_back(std::move(vr));
}
