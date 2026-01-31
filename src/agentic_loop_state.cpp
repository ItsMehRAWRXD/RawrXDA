// AgenticLoopState Implementation
#include "agentic_loop_state.h"


#include <algorithm>

AgenticLoopState::AgenticLoopState()
    : m_currentPhase(ReasoningPhase::Analysis)
    , m_currentStatus(IterationStatus::NotStarted)
    , m_stateStartTime(std::chrono::system_clock::time_point::currentDateTime())
    , m_lastUpdateTime(std::chrono::system_clock::time_point::currentDateTime())
{
}

AgenticLoopState::~AgenticLoopState()
{
}

// ===== ITERATION MANAGEMENT =====

void AgenticLoopState::startIteration(const std::string& goal)
{
    Iteration iteration;
    iteration.iterationNumber = m_iterations.size() + 1;
    iteration.startTime = std::chrono::system_clock::time_point::currentDateTime();
    iteration.currentPhase = ReasoningPhase::Analysis;
    iteration.status = IterationStatus::InProgress;
    iteration.goalStatement = goal;
    iteration.contextSnapshot = getAllMemory();
    iteration.successScore = 0.0f;
    iteration.errorCount = 0;

    m_iterations.push_back(iteration);
    m_currentStatus = IterationStatus::InProgress;

    if (m_debugMode) {
    }
}

void AgenticLoopState::endIteration(IterationStatus status, const std::string& result)
{
    if (m_iterations.empty()) return;

    Iteration& iteration = m_iterations.back();
    iteration.endTime = std::chrono::system_clock::time_point::currentDateTime();
    iteration.status = status;
    iteration.resultSummary = result;

    // Update context window
    m_contextWindow.push_back(iteration);
    if (m_contextWindow.size() > m_contextWindowSize) {
        m_contextWindow.pop_front();
    }

    m_currentStatus = status;
    m_lastUpdateTime = std::chrono::system_clock::time_point::currentDateTime();

    if (m_debugMode) {
                 << "- Status:" << statusToString(status);
    }
}

AgenticLoopState::Iteration* AgenticLoopState::getCurrentIteration()
{
    if (m_iterations.empty()) return nullptr;
    return &m_iterations.back();
}

// ===== PHASE MANAGEMENT =====

void AgenticLoopState::setCurrentPhase(ReasoningPhase phase)
{
    m_currentPhase = phase;
    m_lastUpdateTime = std::chrono::system_clock::time_point::currentDateTime();

    if (m_debugMode) {
    }
}

float AgenticLoopState::getPhaseProgress() const
{
    // Calculate progress based on iterations completed
    if (m_iterations.empty()) return 0.0f;

    int completed = getCompletedIterations();
    float baseProgress = (completed * 100.0f) / m_iterations.size();

    // Add partial progress for current iteration
    auto curr = m_iterations.back();
    float phaseWeight = 0.0f;
    switch (curr.currentPhase) {
        case ReasoningPhase::Analysis: phaseWeight = 0.1f; break;
        case ReasoningPhase::Planning: phaseWeight = 0.3f; break;
        case ReasoningPhase::Execution: phaseWeight = 0.6f; break;
        case ReasoningPhase::Verification: phaseWeight = 0.8f; break;
        case ReasoningPhase::Reflection: phaseWeight = 0.9f; break;
        case ReasoningPhase::Adjustment: phaseWeight = 0.95f; break;
    }

    float iterationProgress = phaseWeight / m_iterations.size() * 100.0f;
    return std::min(99.9f, baseProgress + iterationProgress);
}

std::vector<std::string> AgenticLoopState::getAllPhaseTransitions() const
{
    std::vector<std::string> transitions;
    for (const auto& iteration : m_iterations) {
        transitions.append(phaseToString(iteration.currentPhase));
    }
    return transitions;
}

// ===== DECISION TRACKING =====

void AgenticLoopState::recordDecision(
    const std::string& description,
    const void*& reasoning,
    float confidence)
{
    if (m_iterations.empty()) return;

    Decision decision;
    decision.timestamp = std::chrono::system_clock::time_point::currentDateTime();
    decision.phase = m_currentPhase;
    decision.description = description;
    decision.reasoning = reasoning;
    decision.confidence = confidence;
    decision.success = false;
    decision.retryCount = 0;

    m_iterations.back().decisions.push_back(decision);

    if (m_debugMode) {
                 << "- Confidence:" << confidence;
    }
}

void AgenticLoopState::recordDecisionOutcome(
    int decisionIndex,
    const void*& outcome,
    bool success)
{
    if (m_iterations.empty() || decisionIndex >= m_iterations.back().decisions.size()) {
        return;
    }

    auto& decision = m_iterations.back().decisions[decisionIndex];
    decision.outcome = outcome;
    decision.success = success;

    if (m_debugMode) {
    }
}

std::vector<AgenticLoopState::Decision> AgenticLoopState::getDecisionHistory(int limit) const
{
    std::vector<Decision> allDecisions;

    for (const auto& iteration : m_iterations) {
        for (const auto& decision : iteration.decisions) {
            allDecisions.push_back(decision);
        }
    }

    if (limit > 0 && allDecisions.size() > limit) {
        allDecisions.erase(allDecisions.begin(), allDecisions.end() - limit);
    }

    return allDecisions;
}

AgenticLoopState::Decision* AgenticLoopState::getCurrentDecision()
{
    if (m_iterations.empty() || m_iterations.back().decisions.empty()) {
        return nullptr;
    }
    return &m_iterations.back().decisions.back();
}

float AgenticLoopState::getAverageDecisionConfidence() const
{
    auto decisions = getDecisionHistory();
    if (decisions.empty()) return 0.0f;

    float total = 0.0f;
    for (const auto& decision : decisions) {
        total += decision.confidence;
    }

    return total / decisions.size();
}

float AgenticLoopState::getDecisionSuccessRate() const
{
    auto decisions = getDecisionHistory();
    if (decisions.empty()) return 0.0f;

    int successful = 0;
    for (const auto& decision : decisions) {
        if (decision.success) successful++;
    }

    return (successful * 100.0f) / decisions.size();
}

// ===== ERROR TRACKING =====

void AgenticLoopState::recordError(
    const std::string& errorType,
    const std::string& message,
    const std::string& stackTrace)
{
    ErrorRecord error;
    error.timestamp = std::chrono::system_clock::time_point::currentDateTime();
    error.errorType = errorType;
    error.errorMessage = message;
    error.stackTrace = stackTrace;
    error.phase = m_currentPhase;
    error.recoveryAttempt = 0;
    error.recoverySucceeded = false;

    if (!m_iterations.empty()) {
        m_iterations.back().errorCount++;
    }

    m_errorHistory.push_back(error);

    // Keep error history bounded
    if (m_errorHistory.size() > 100) {
        m_errorHistory.pop_front();
    }

    if (m_debugMode) {
    }
}

void AgenticLoopState::recordErrorRecovery(
    int errorIndex,
    const std::string& strategy,
    bool succeeded)
{
    if (errorIndex >= m_errorHistory.size()) return;

    auto& error = m_errorHistory[errorIndex];
    error.recoveryStrategy = strategy;
    error.recoverySucceeded = succeeded;

    if (m_debugMode) {
                 << "- Success:" << succeeded;
    }
}

const std::deque<AgenticLoopState::ErrorRecord>& AgenticLoopState::getErrorHistory(size_t limit) const
{
    return m_errorHistory;
}

int AgenticLoopState::getTotalErrorCount() const
{
    return m_errorHistory.size();
}

float AgenticLoopState::getErrorRate() const
{
    if (m_iterations.empty()) return 0.0f;

    int totalErrors = 0;
    for (const auto& iteration : m_iterations) {
        totalErrors += iteration.errorCount;
    }

    return (totalErrors * 100.0f) / m_iterations.size();
}

std::string AgenticLoopState::generateErrorAnalysis() const
{
    void* analysis;
    analysis["total_errors"] = getTotalErrorCount();
    analysis["error_rate"] = getErrorRate();

    // Group errors by type
    std::unordered_map<std::string, int> errorTypeCounts;
    for (const auto& error : m_errorHistory) {
        errorTypeCounts[error.errorType.toStdString()]++;
    }

    void* errorTypes;
    for (const auto& pair : errorTypeCounts) {
        errorTypes[std::string::fromStdString(pair.first)] = pair.second;
    }
    analysis["error_types"] = errorTypes;

    return std::string::fromUtf8(void*(analysis).toJson());
}

// ===== STATE SNAPSHOTS =====

void* AgenticLoopState::takeSnapshot()
{
    void* snapshot;
    snapshot["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    snapshot["phase"] = phaseToString(m_currentPhase);
    snapshot["status"] = statusToString(m_currentStatus);
    snapshot["iterations_completed"] = getCompletedIterations();
    snapshot["total_iterations"] = getTotalIterations();
    snapshot["progress"] = getProgressPercentage();
    snapshot["error_count"] = getTotalErrorCount();
    snapshot["average_confidence"] = getAverageDecisionConfidence();
    snapshot["decision_success_rate"] = getDecisionSuccessRate();

    m_lastSnapshot = snapshot;
    m_snapshotHistory.push_back(snapshot);

    return snapshot;
}

bool AgenticLoopState::restoreFromSnapshot(const void*& snapshot)
{
    // Restore limited state from snapshot
    // Note: Full state restoration would require serializing entire iteration history
    m_currentPhase = stringToPhase(snapshot["phase"].toString());
    m_currentStatus = stringToStatus(snapshot["status"].toString());
    m_lastSnapshot = snapshot;

    if (m_debugMode) {
    }

    return true;
}

// ===== MEMORY MANAGEMENT =====

void AgenticLoopState::addToMemory(const std::string& key, const std::any& value)
{
    m_memory[key.toStdString()] = value;
    m_lastUpdateTime = std::chrono::system_clock::time_point::currentDateTime();
}

std::any AgenticLoopState::getFromMemory(const std::string& key)
{
    auto it = m_memory.find(key.toStdString());
    if (it != m_memory.end()) {
        return it->second;
    }
    return std::any();
}

void AgenticLoopState::removeFromMemory(const std::string& key)
{
    m_memory.erase(key.toStdString());
}

void* AgenticLoopState::getAllMemory() const
{
    void* obj;
    for (const auto& pair : m_memory) {
        obj[std::string::fromStdString(pair.first)] = void*::fromVariant(pair.second);
    }
    return obj;
}

void AgenticLoopState::clearMemoryExcept(const std::vector<std::string>& keysToKeep)
{
    std::unordered_map<std::string, std::any> newMemory;
    for (const auto& key : keysToKeep) {
        auto it = m_memory.find(key.toStdString());
        if (it != m_memory.end()) {
            newMemory[it->first] = it->second;
        }
    }
    m_memory = newMemory;
}

void AgenticLoopState::setContextWindowSize(int size)
{
    m_contextWindowSize = size;
}

void* AgenticLoopState::getContextWindow() const
{
    void* array;
    for (const auto& iteration : m_contextWindow) {
        void* obj;
        obj["iteration"] = iteration.iterationNumber;
        obj["goal"] = iteration.goalStatement;
        obj["status"] = statusToString(iteration.status);
        array.append(obj);
    }
    return array;
}

std::string AgenticLoopState::formatContextForModel() const
{
    std::string context;
    context += "=== REASONING CONTEXT ===\n";
    context += std::string("Current Goal: %1\n");
    context += std::string("Current Phase: %1\n"));
    context += std::string("Total Iterations: %1\n"));
    context += std::string("Progress: %1%\n")));

    context += "\n=== RECENT DECISIONS ===\n";
    auto decisions = getDecisionHistory(5);
    for (const auto& decision : decisions) {
        context += std::string("- %1 (Confidence: %2)\n")
                  
                  ;
    }

    return context;
}

// ===== GOAL AND PROGRESS =====

void AgenticLoopState::updateProgress(int current, int total)
{
    m_progressCurrent = current;
    m_progressTotal = total;
    m_lastUpdateTime = std::chrono::system_clock::time_point::currentDateTime();
}

float AgenticLoopState::getProgressPercentage() const
{
    if (m_progressTotal == 0) {
        return getPhaseProgress();
    }
    return (m_progressCurrent * 100.0f) / m_progressTotal;
}

void* AgenticLoopState::getProgressInfo() const
{
    void* info;
    info["current"] = m_progressCurrent;
    info["total"] = m_progressTotal;
    info["percentage"] = getProgressPercentage();
    info["phase"] = phaseToString(m_currentPhase);
    info["status"] = statusToString(m_currentStatus);
    return info;
}

// ===== CONSTRAINTS =====

void AgenticLoopState::addConstraint(const std::string& key, const std::string& constraint)
{
    m_constraints[key] = constraint;
}

void AgenticLoopState::removeConstraint(const std::string& key)
{
    m_constraints.erase(key);
}

bool AgenticLoopState::validateAgainstConstraints(const void*& action) const
{
    // Simple constraint validation - can be extended
    for (auto it = m_constraints.constBegin(); it != m_constraints.constEnd(); ++it) {
        const std::string& constraintValue = it.value().toString();
        if (!action.contains(it.key()) && !constraintValue.empty()) {
            return false;
        }
    }
    return true;
}

// ===== STRATEGY TRACKING =====

void AgenticLoopState::recordAppliedStrategy(const std::string& strategy)
{
    m_appliedStrategies.append(strategy);
    if (!m_iterations.empty()) {
        m_iterations.back().appliedStrategies.append(strategy);
    }
}

void AgenticLoopState::setSuggestedStrategies(const std::vector<std::string>& strategies)
{
    m_suggestedStrategies = strategies;
}

// ===== METRICS =====

void* AgenticLoopState::getMetrics() const
{
    void* metrics;
    metrics["total_iterations"] = getTotalIterations();
    metrics["completed_iterations"] = getCompletedIterations();
    metrics["failed_iterations"] = getFailedIterations();
    metrics["success_rate"] = getOverallSuccessRate();
    metrics["phase_progress"] = getPhaseProgress();
    metrics["decision_success_rate"] = getDecisionSuccessRate();
    metrics["average_confidence"] = getAverageDecisionConfidence();
    metrics["error_rate"] = getErrorRate();
    metrics["total_errors"] = getTotalErrorCount();

    return metrics;
}

int AgenticLoopState::getCompletedIterations() const
{
    int count = 0;
    for (const auto& iteration : m_iterations) {
        if (iteration.status == IterationStatus::Completed) {
            count++;
        }
    }
    return count;
}

int AgenticLoopState::getFailedIterations() const
{
    int count = 0;
    for (const auto& iteration : m_iterations) {
        if (iteration.status == IterationStatus::Failed) {
            count++;
        }
    }
    return count;
}

float AgenticLoopState::getOverallSuccessRate() const
{
    if (m_iterations.empty()) return 0.0f;
    int completed = getCompletedIterations();
    return (completed * 100.0f) / m_iterations.size();
}

std::string AgenticLoopState::getStateAsSummary() const
{
    std::string summary;
    summary += std::string("=== AGENTIC LOOP STATE ===\n");
    summary += std::string("Current Phase: %1\n"));
    summary += std::string("Status: %1\n"));
    summary += std::string("Iterations: %1/%2 completed\n")
              )
              );
    summary += std::string("Success Rate: %1%\n")));
    summary += std::string("Errors: %1\n"));

    return summary;
}

// ===== SERIALIZATION =====

std::string AgenticLoopState::serializeState() const
{
    void* state;
    state["phase"] = phaseToString(m_currentPhase);
    state["status"] = statusToString(m_currentStatus);
    state["goal"] = m_currentGoal;
    state["metrics"] = getMetrics();
    state["memory"] = getAllMemory();
    state["constraints"] = m_constraints;

    return std::string::fromUtf8(void*(state).toJson());
}

bool AgenticLoopState::deserializeState(const std::string& jsonStr)
{
    void* doc = void*::fromJson(jsonStr.toUtf8());
    if (!doc.isObject()) return false;

    void* state = doc.object();
    m_currentPhase = stringToPhase(state["phase"].toString());
    m_currentStatus = stringToStatus(state["status"].toString());
    m_currentGoal = state["goal"].toString();

    return true;
}

// ===== DEBUGGING =====

std::string AgenticLoopState::generateDebugReport() const
{
    std::string report;
    report += "=== AGENTIC LOOP STATE DEBUG REPORT ===\n\n";

    report += "ITERATIONS:\n";
    for (const auto& iteration : m_iterations) {
        report += std::string("  %1. %2 - %3\n")


                  );
        report += std::string("     Decisions: %1, Errors: %2\n")
                  )
                  ;
    }

    report += "\nERRORS:\n";
    for (const auto& error : m_errorHistory) {
        report += std::string("  [%1] %2 - %3\n")
                  )
                  
                  ;
    }

    report += "\nMETRICS:\n";
    void* metrics = getMetrics();
    for (auto it = metrics.constBegin(); it != metrics.constEnd(); ++it) {
        report += std::string("  %1: %2\n")).toVariant().toString());
    }

    return report;
}

// ===== HELPER METHODS =====

std::string AgenticLoopState::phaseToString(ReasoningPhase phase) const
{
    switch (phase) {
        case ReasoningPhase::Analysis: return "Analysis";
        case ReasoningPhase::Planning: return "Planning";
        case ReasoningPhase::Execution: return "Execution";
        case ReasoningPhase::Verification: return "Verification";
        case ReasoningPhase::Reflection: return "Reflection";
        case ReasoningPhase::Adjustment: return "Adjustment";
        default: return "Unknown";
    }
}

AgenticLoopState::ReasoningPhase AgenticLoopState::stringToPhase(const std::string& str) const
{
    if (str == "Analysis") return ReasoningPhase::Analysis;
    if (str == "Planning") return ReasoningPhase::Planning;
    if (str == "Execution") return ReasoningPhase::Execution;
    if (str == "Verification") return ReasoningPhase::Verification;
    if (str == "Reflection") return ReasoningPhase::Reflection;
    if (str == "Adjustment") return ReasoningPhase::Adjustment;
    return ReasoningPhase::Analysis;
}

std::string AgenticLoopState::statusToString(IterationStatus status) const
{
    switch (status) {
        case IterationStatus::NotStarted: return "NotStarted";
        case IterationStatus::InProgress: return "InProgress";
        case IterationStatus::Completed: return "Completed";
        case IterationStatus::Failed: return "Failed";
        case IterationStatus::Recovering: return "Recovering";
        case IterationStatus::MaxAttemptsReached: return "MaxAttemptsReached";
        default: return "Unknown";
    }
}

AgenticLoopState::IterationStatus AgenticLoopState::stringToStatus(const std::string& str) const
{
    if (str == "NotStarted") return IterationStatus::NotStarted;
    if (str == "InProgress") return IterationStatus::InProgress;
    if (str == "Completed") return IterationStatus::Completed;
    if (str == "Failed") return IterationStatus::Failed;
    if (str == "Recovering") return IterationStatus::Recovering;
    if (str == "MaxAttemptsReached") return IterationStatus::MaxAttemptsReached;
    return IterationStatus::NotStarted;
}


