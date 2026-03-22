// failure_intelligence_orchestrator.cpp
// Implementation: failure detection, root cause analysis, and autonomous recovery

#include "failure_intelligence_orchestrator.hpp"
#include <algorithm>
#include <sstream>
#include <regex>
#include <ctime>

namespace Agentic {

// ============================================================================
// JSON Serialization
// ============================================================================

nlohmann::json FailureSignal::toJson() const {
    nlohmann::json j;
    j["signal_id"] = signal_id;
    j["source"] = source_component;
    j["step_id"] = step_id;
    j["action"] = action_descriptor;
    j["exit_code"] = exit_code;
    j["duration_ms"] = duration_ms;
    j["timeout_ms"] = timeout_ms;
    j["category"] = static_cast<int>(category);
    j["severity"] = static_cast<int>(severity);
    j["confidence"] = confidence_score;
    if (!error_message.empty()) j["error"] = error_message;
    if (!stderr_output.empty()) j["stderr"] = stderr_output.substr(0, 500); // Truncate
    return j;
}

nlohmann::json RootCauseAnalysis::toJson() const {
    nlohmann::json j;
    j["analysis_id"] = analysis_id;
    j["failure_signal_id"] = failure_signal_id;
    j["category"] = static_cast<int>(primary_category);
    j["severity"] = static_cast<int>(assessed_severity);
    j["confidence"] = analysis_confidence;
    j["root_cause"] = root_cause_description;
    j["patterns_matched"] = pattern_match_count;
    nlohmann::json causes = nlohmann::json::array();
    for (const auto& c : probable_causes) causes.push_back(c);
    j["probable_causes"] = causes;
    return j;
}

nlohmann::json RecoveryPlan::toJson() const {
    nlohmann::json j;
    j["recovery_id"] = recovery_id;
    j["failure_signal_id"] = failure_signal_id;
    j["strategy"] = static_cast<int>(strategy);
    j["status"] = static_cast<int>(status);
    j["attempt"] = current_attempt;
    j["max_attempts"] = max_retry_attempts;
    nlohmann::json steps = nlohmann::json::array();
    for (const auto& s : recovery_steps) steps.push_back(s);
    j["steps"] = steps;
    if (!status_reason.empty()) j["reason"] = status_reason;
    return j;
}

// ============================================================================
// Failure Pattern Database Initialization
// ============================================================================

void FailureIntelligenceOrchestrator::initializePatterns() {
    // Transient failures
    m_failurePatterns.push_back({"connection refused|connection reset|network unreachable", 
                                 FailureCategory::Transient, 0.85f});
    m_failurePatterns.push_back({"timeout|timed out|deadline exceeded", 
                                 FailureCategory::Timeout, 0.90f});
    m_failurePatterns.push_back({"temporarily unavailable|retry later|service busy", 
                                 FailureCategory::Transient, 0.8f});
    
    // Logic errors
    m_failurePatterns.push_back({"segmentation fault|access violation|null pointer", 
                                 FailureCategory::Logic, 0.95f});
    m_failurePatterns.push_back({"syntax error|parse error|invalid|unexpected", 
                                 FailureCategory::Logic, 0.88f});
    m_failurePatterns.push_back({"assertion failed|assert violation", 
                                 FailureCategory::Logic, 0.92f});
    
    // Dependency issues
    m_failurePatterns.push_back({"not found|cannot find|no such file|missing dependency", 
                                 FailureCategory::Dependency, 0.87f});
    m_failurePatterns.push_back({"undefined reference|symbol not found|link error", 
                                 FailureCategory::Dependency, 0.89f});
    
    // Permission issues
    m_failurePatterns.push_back({"permission denied|access denied|forbidden|unauthorized", 
                                 FailureCategory::Permission, 0.93f});
    m_failurePatterns.push_back({"cannot open|write protected|read-only file system", 
                                 FailureCategory::Permission, 0.85f});
    
    // Configuration issues
    m_failurePatterns.push_back({"configuration error|invalid config|bad setting", 
                                 FailureCategory::Configuration, 0.82f});
    m_failurePatterns.push_back({"schema mismatch|version mismatch|incompatible", 
                                 FailureCategory::Configuration, 0.80f});
    
    // Environmental issues
    m_failurePatterns.push_back({"out of memory|memory exhausted|heap corrupt", 
                                 FailureCategory::Environmental, 0.94f});
    m_failurePatterns.push_back({"disk full|no space|I/O error", 
                                 FailureCategory::Environmental, 0.91f});
}

// ============================================================================
// Constructor & Lifecycle
// ============================================================================

FailureIntelligenceOrchestrator::FailureIntelligenceOrchestrator()
    : m_autoRetryThreshold(SeverityLevel::Error),
      m_maxRecoveryAttempts(3),
      m_escalationCategory(FailureCategory::Fatal) {
    initializePatterns();
}

FailureIntelligenceOrchestrator::~FailureIntelligenceOrchestrator() = default;

// ============================================================================
// Failure Reporting & Analysis
// ============================================================================

void FailureIntelligenceOrchestrator::reportFailure(const FailureSignal& signal) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto sig = std::make_unique<FailureSignal>(signal);
    if (sig->signal_id.empty()) {
        sig->signal_id = "failure_" + std::to_string(std::time(nullptr)) + 
                         "_" + std::to_string(m_failureCounter++);
    }
    sig->detected_at = std::chrono::system_clock::now();

    // Classify and store
    sig->category = classifyFailure(*sig);
    m_failureHistory[sig->signal_id] = std::move(sig);
    m_stats.total_failures_seen++;

    if (m_failureCallback) {
        m_failureCallback(*m_failureHistory[signal.signal_id]);
    }

    if (m_analyzeLogFn) {
        m_analyzeLogFn("Failure reported: " + signal.signal_id + 
                      " from " + signal.source_component);
    }
}

FailureCategory FailureIntelligenceOrchestrator::classifyFailure(
    const FailureSignal& signal) const {

    // Check exit code heuristics
    if (signal.exit_code == 0) return FailureCategory::Logic; // Why reported as failure?
    if (signal.exit_code == 124 || signal.exit_code == 137) return FailureCategory::Timeout;
    if (signal.exit_code == 126 || signal.exit_code == 127) return FailureCategory::Permission;

    // Check timeout
    if (signal.timeout_ms > 0 && signal.duration_ms >= signal.timeout_ms) {
        return FailureCategory::Timeout;
    }

    // Pattern matching on error messages
    std::vector<FailurePattern> matches = matchPatterns(signal);
    if (!matches.empty()) {
        return matches[0].suggested_category; // Highest confidence match
    }

    // Default: assume logic error
    return FailureCategory::Logic;
}

std::vector<FailureIntelligenceOrchestrator::FailurePattern>
FailureIntelligenceOrchestrator::matchPatterns(const FailureSignal& signal) const {
    std::vector<FailurePattern> result;

    std::string combined = signal.error_message + "\n" + 
                          signal.stderr_output + "\n" + 
                          signal.stdout_output;
    std::string combined_lower;
    std::transform(combined.begin(), combined.end(), 
                  std::back_inserter(combined_lower), ::tolower);

    for (const auto& pattern : m_failurePatterns) {
        try {
            std::regex re(pattern.regex_pattern, std::regex::icase);
            if (std::regex_search(combined_lower, re)) {
                result.push_back(pattern);
            }
        } catch (...) {
            // Regex error, skip
        }
    }

    // Sort by confidence descending
    std::sort(result.begin(), result.end(),
             [](const FailurePattern& a, const FailurePattern& b) {
                 return a.confidence > b.confidence;
             });

    return result;
}

RootCauseAnalysis* FailureIntelligenceOrchestrator::analyzeFailure(
    const FailureSignal& signal) {

    std::lock_guard<std::mutex> lock(m_mutex);

    auto analysis = std::make_unique<RootCauseAnalysis>();
    analysis->analysis_id = "analysis_" + std::to_string(std::time(nullptr)) +
                           "_" + std::to_string(m_analysisCounter++);
    analysis->failure_signal_id = signal.signal_id;
    analysis->analyzed_at = std::chrono::system_clock::now();

    // Categorize
    analysis->primary_category = classifyFailure(signal);

    // Match patterns
    auto patterns = matchPatterns(signal);
    analysis->pattern_match_count = static_cast<int>(patterns.size());
    for (const auto& p : patterns) {
        analysis->identified_patterns.push_back(p.regex_pattern);
    }

    // Confidence scoring
    if (!patterns.empty()) {
        analysis->analysis_confidence = patterns[0].confidence; // Best match confidence
        analysis->recommended_recoveries.push_back(
            analysis->primary_category == FailureCategory::Transient 
                ? RecoveryStrategy::Retry 
                : RecoveryStrategy::Escalate);
    } else {
        analysis->analysis_confidence = 0.5f; // Low confidence guess
    }

    // Generate human-readable root cause
    switch (analysis->primary_category) {
    case FailureCategory::Transient:
        analysis->root_cause_description = "Temporary network or resource unavailability";
        analysis->recommended_recoveries = {RecoveryStrategy::Retry, 
                                           RecoveryStrategy::ReplanAndRecontinue};
        break;
    case FailureCategory::Timeout:
        analysis->root_cause_description = "Execution exceeded time or resource limit";
        analysis->recommended_recoveries = {RecoveryStrategy::Retry, 
                                           RecoveryStrategy::ReplanAndRecontinue};
        break;
    case FailureCategory::Logic:
        analysis->root_cause_description = "Code logic error or exception";
        analysis->recommended_recoveries = {RecoveryStrategy::Escalate, 
                                           RecoveryStrategy::Abort};
        break;
    case FailureCategory::Dependency:
        analysis->root_cause_description = "Missing required tool, library, or service";
        analysis->recommended_recoveries = {RecoveryStrategy::Fallback, 
                                           RecoveryStrategy::Escalate};
        break;
    case FailureCategory::Permission:
        analysis->root_cause_description = "Access denied or permission/authorization issue";
        analysis->recommended_recoveries = {RecoveryStrategy::Escalate};
        break;
    case FailureCategory::Configuration:
        analysis->root_cause_description = "Configuration state mismatch or invalid settings";
        analysis->recommended_recoveries = {RecoveryStrategy::Compensate, 
                                           RecoveryStrategy::ReplanAndRecontinue};
        break;
    case FailureCategory::Environmental:
        analysis->root_cause_description = "System resource exhaustion or environmental issue";
        analysis->recommended_recoveries = {RecoveryStrategy::Compensate, 
                                           RecoveryStrategy::Escalate};
        break;
    case FailureCategory::Fatal:
        analysis->root_cause_description = "Unrecoverable system failure";
        analysis->recommended_recoveries = {RecoveryStrategy::Escalate, 
                                           RecoveryStrategy::Abort};
        break;
    }

    analysis->probable_causes.push_back(analysis->root_cause_description);
    analysis->assessed_severity = signal.severity;

    RootCauseAnalysis* result = analysis.get();
    m_analyses[analysis->analysis_id] = std::move(analysis);

    if (m_analyzeLogFn) {
        m_analyzeLogFn("Analysis complete: " + result->analysis_id +
                      " → Category: " + std::to_string(static_cast<int>(analysis->primary_category)) +
                      " Confidence: " + std::to_string(static_cast<int>(analysis->analysis_confidence * 100)) + "%");
    }

    return result;
}

RecoveryStrategy FailureIntelligenceOrchestrator::selectBestRecovery(
    const RootCauseAnalysis* analysis) const {

    if (!analysis || analysis->recommended_recoveries.empty()) {
        return RecoveryStrategy::Escalate;
    }
    return analysis->recommended_recoveries[0]; // Highest priority
}

// ============================================================================
// Recovery Planning & Execution
// ============================================================================

RecoveryPlan* FailureIntelligenceOrchestrator::generateRecoveryPlan(
    const FailureSignal& signal,
    const RootCauseAnalysis* analysis) {

    std::lock_guard<std::mutex> lock(m_mutex);

    // Ensure analysis exists
    const RootCauseAnalysis* rca = analysis;
    std::unique_ptr<RootCauseAnalysis> temp_analysis;
    if (!rca) {
        // Generate on-the-fly (without storing in history)
        FailureSignal sig_copy = signal;
        sig_copy.category = classifyFailure(signal);
        // Would need analyzeFailure here, but it's thread-locked
        // For now, use only classifyFailure result
    }

    auto plan = std::make_unique<RecoveryPlan>();
    plan->recovery_id = "recovery_" + std::to_string(std::time(nullptr)) +
                       "_" + std::to_string(m_recoveryCounter++);
    plan->failure_signal_id = signal.signal_id;
    plan->created_at = std::chrono::system_clock::now();
    plan->max_retry_attempts = m_maxRecoveryAttempts;

    // Select recovery strategy
    plan->strategy = selectBestRecovery(rca);

    // Build recovery steps based on strategy
    switch (plan->strategy) {
    case RecoveryStrategy::Retry:
        plan->strategy_description = "Retry with exponential backoff";
        plan->recovery_steps.push_back("Wait " + std::to_string(plan->backoff_delay_ms) + "ms");
        plan->recovery_steps.push_back("Retry: " + signal.action_descriptor);
        plan->recovery_params["backoff_multiplier"] = "2.0";
        break;

    case RecoveryStrategy::Fallback:
        plan->strategy_description = "Use alternative tool or implementation";
        if (signal.source_component == "compiler_gcc") {
            plan->recovery_steps.push_back("Use alternative compiler: clang");
            plan->recovery_params["alt_tool"] = "clang";
        } else if (signal.source_component == "network_curl") {
            plan->recovery_steps.push_back("Use alternative: wget or powershell");
            plan->recovery_params["alt_tool"] = "wget";
        } else {
            plan->recovery_steps.push_back("Try alternative implementation");
        }
        break;

    case RecoveryStrategy::Compensate:
        plan->strategy_description = "Execute compensating action and retry";
        plan->recovery_steps.push_back("Rollback: undo last change");
        plan->recovery_steps.push_back("Clear temporary files and reset state");
        plan->recovery_steps.push_back("Reconfigure environment");
        plan->recovery_steps.push_back("Retry: " + signal.action_descriptor);
        break;

    case RecoveryStrategy::ReplanAndRecontinue:
        plan->strategy_description = "Regenerate plan from current state";
        plan->recovery_steps.push_back("Analyze current system state");
        plan->recovery_steps.push_back("Generate new plan avoiding failed action");
        plan->recovery_steps.push_back("Re-Execute from this point");
        plan->recovery_params["replanning_enabled"] = "true";
        break;

    case RecoveryStrategy::Escalate:
        plan->strategy_description = "Escalate to human for manual intervention";
        plan->recovery_steps.push_back("Wait for human approval");
        plan->recovery_steps.push_back("Execute human-approved recovery action");
        break;

    case RecoveryStrategy::Abort:
        plan->strategy_description = "Terminate execution and rollback";
        plan->recovery_steps.push_back("Abort current execution");
        plan->recovery_steps.push_back("Rollback all changes");
        plan->recovery_steps.push_back("Report failure to user");
        break;
    }

    RecoveryPlan* result = plan.get();
    m_recoveryPlans[plan->recovery_id] = std::move(plan);
    m_pendingRecoveries.push_back(result);

    if (m_analyzeLogFn) {
        m_analyzeLogFn("Recovery plan generated: " + result->recovery_id +
                      " Strategy: " + result->strategy_description);
    }

    return result;
}

bool FailureIntelligenceOrchestrator::executeRecovery(
    RecoveryPlan* plan, std::string& output) {

    if (!plan) return false;

    std::lock_guard<std::mutex> lock(m_mutex);

    plan->status = RecoveryStatus::InProgress;
    plan->executed_at = std::chrono::system_clock::now();
    plan->current_attempt++;

    if (m_recoveryStartCallback) {
        m_recoveryStartCallback(*plan);
    }

    if (m_analyzeLogFn) {
        m_analyzeLogFn("Executing recovery: " + plan->recovery_id +
                      " Attempt " + std::to_string(plan->current_attempt) +
                      "/" + std::to_string(plan->max_retry_attempts));
    }

    // Execute via callback if available
    bool success = false;
    if (m_recoveryExecutorFn) {
        success = m_recoveryExecutorFn(*plan, output);
    } else {
        // Simulated execution
        output = "Simulated recovery execution of: " + plan->strategy_description;
        success = true;
    }

    plan->completed_at = std::chrono::system_clock::now();
    plan->status = success ? RecoveryStatus::Succeeded : RecoveryStatus::Failed;
    plan->status_reason = success ? "Recovery successful" : "Recovery failed";

    m_stats.total_recoveries_attempted++;
    if (success) m_stats.total_recoveries_succeeded++;

    if (m_recoveryCompleteCallback) {
        m_recoveryCompleteCallback(*plan, success);
    }

    if (m_analyzeLogFn) {
        m_analyzeLogFn(std::string("Recovery ") + (success ? "SUCCEEDED" : "FAILED") +
                       ": " + plan->recovery_id);
    }
    // Remove from pending
    m_pendingRecoveries.erase(
        std::remove(m_pendingRecoveries.begin(), m_pendingRecoveries.end(), plan),
        m_pendingRecoveries.end());

    return success;
}

bool FailureIntelligenceOrchestrator::autonomousRecover(
    const FailureSignal& signal, std::string& output) {

    // Step 1: Report
    reportFailure(signal);

    // Step 2: Analyze
    RootCauseAnalysis* rca = analyzeFailure(signal);

    // Step 3: Plan
    RecoveryPlan* plan = generateRecoveryPlan(signal, rca);

    // Step 4: Execute
    if (plan->strategy == RecoveryStrategy::Escalate) {
        plan->status = RecoveryStatus::Escalated;
        output = "Failure requires human escalation";
        if (m_analyzeLogFn) {
            m_analyzeLogFn("Recovery escalated (requires human): " + plan->recovery_id);
        }
        return false; // Escalated, not automatically recovered
    }

    return executeRecovery(plan, output);
}

// ============================================================================
// Learning & Statistics
// ============================================================================

void FailureIntelligenceOrchestrator::learnFromFailure(
    const FailureSignal& signal, FailureCategory actual_category) {

    std::lock_guard<std::mutex> lock(m_mutex);

    // Update pattern matches for the actual category
    auto patterns = matchPatterns(signal);
    for (auto& p : m_failurePatterns) {
        // Boost patterns matching actual category
        for (const auto& matched : patterns) {
            if (p.regex_pattern == matched.regex_pattern && 
                p.suggested_category == actual_category) {
                p.confidence = std::min(1.0f, p.confidence + 0.05f);
                p.match_count++;
            }
        }
    }

    if (m_analyzeLogFn) {
        m_analyzeLogFn("Learning: Signal classified as " + 
                      std::to_string(static_cast<int>(actual_category)));
    }
}

FailureIntelligenceOrchestrator::FailureStats
FailureIntelligenceOrchestrator::getFailureStatistics() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    FailureStats stats = m_stats;
    if (stats.total_recoveries_attempted > 0) {
        stats.overall_recovery_success_rate = 
            static_cast<float>(stats.total_recoveries_succeeded) / 
            stats.total_recoveries_attempted;
    }

    // Count by category
    for (const auto& fs : m_failureHistory) {
        if (fs.second) {
            stats.category_counts[fs.second->category]++;
        }
    }

    return stats;
}

// ============================================================================
// State Access
// ============================================================================

FailureSignal* FailureIntelligenceOrchestrator::getFailure(
    const std::string& signal_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_failureHistory.find(signal_id);
    return (it != m_failureHistory.end()) ? it->second.get() : nullptr;
}

RootCauseAnalysis* FailureIntelligenceOrchestrator::getAnalysis(
    const std::string& analysis_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_analyses.find(analysis_id);
    return (it != m_analyses.end()) ? it->second.get() : nullptr;
}

RecoveryPlan* FailureIntelligenceOrchestrator::getRecoveryPlan(
    const std::string& plan_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_recoveryPlans.find(plan_id);
    return (it != m_recoveryPlans.end()) ? it->second.get() : nullptr;
}

std::vector<FailureSignal*> FailureIntelligenceOrchestrator::getRecentFailures(
    int count) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<FailureSignal*> result;
    int seen = 0;
    // Iterate in reverse (most recent first)
    for (auto it = m_failureHistory.rbegin(); 
         it != m_failureHistory.rend() && seen < count; ++it, ++seen) {
        result.push_back(it->second.get());
    }
    return result;
}

std::vector<RecoveryPlan*> FailureIntelligenceOrchestrator::getPendingRecoveries() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pendingRecoveries;
}

std::vector<RecoveryPlan*> FailureIntelligenceOrchestrator::getFailedRecoveries() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<RecoveryPlan*> result;
    for (const auto& rp : m_recoveryPlans) {
        if (rp.second->status == RecoveryStatus::Failed) {
            result.push_back(rp.second.get());
        }
    }
    return result;
}

// ============================================================================
// JSON Export
// ============================================================================

nlohmann::json FailureIntelligenceOrchestrator::getFailureQueueJson() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& fs : m_failureHistory) {
        if (fs.second) {
            arr.push_back(fs.second->toJson());
        }
    }
    return arr;
}

nlohmann::json FailureIntelligenceOrchestrator::getRecoveryQueueJson() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& rp : m_pendingRecoveries) {
        arr.push_back(rp->toJson());
    }
    return arr;
}

nlohmann::json FailureIntelligenceOrchestrator::getStatisticsJson() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    FailureStats stats = m_stats;
    auto counts = nlohmann::json::object();
    for (const auto& fc : stats.category_counts) {
        counts[std::to_string(static_cast<int>(fc.first))] = fc.second;
    }

    nlohmann::json j;
    j["total_failures"] = stats.total_failures_seen;
    j["total_recovery_attempts"] = stats.total_recoveries_attempted;
    j["total_recovery_successes"] = stats.total_recoveries_succeeded;
    j["recovery_success_rate"] = stats.overall_recovery_success_rate;
    j["failure_categories"] = counts;
    return j;
}

nlohmann::json FailureIntelligenceOrchestrator::getSystemHealthJson() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    nlohmann::json j;
    j["failure_history_size"] = m_failureHistory.size();
    j["pending_recoveries"] = m_pendingRecoveries.size();
    j["total_analyses"] = m_analyses.size();
    
    auto stats = m_stats;
    j["health_score"] = (stats.total_failures_seen == 0) 
        ? 1.0f 
        : (1.0f - (static_cast<float>(stats.total_failures_seen) / 
                  std::max(stats.total_recoveries_attempted, 1)) * 0.5f);
    
    return j;
}

} // namespace Agentic
