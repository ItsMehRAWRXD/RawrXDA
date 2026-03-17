// AgenticLoopState Implementation (Qt-free)
#include "agentic_loop_state.h"
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <sstream>
#include <iomanip>

// ===== STATIC HELPERS =====

std::string AgenticLoopState::timePointToISO(const TimePoint& tp)
{
    auto tt = std::chrono::system_clock::to_time_t(tp);
    struct tm tmBuf;
#ifdef _WIN32
    localtime_s(&tmBuf, &tt);
#else
    localtime_r(&tt, &tmBuf);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tmBuf);
    return std::string(buf);
}

std::string AgenticLoopState::timePointToHMS(const TimePoint& tp)
{
    auto tt = std::chrono::system_clock::to_time_t(tp);
    struct tm tmBuf;
#ifdef _WIN32
    localtime_s(&tmBuf, &tt);
#else
    localtime_r(&tt, &tmBuf);
#endif
    char buf[16];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", &tmBuf);
    return std::string(buf);
}

AgenticLoopState::AgenticLoopState()
    : m_currentPhase(ReasoningPhase::Analysis)
    , m_currentStatus(IterationStatus::NotStarted)
    , m_stateStartTime(std::chrono::system_clock::now())
    , m_lastUpdateTime(std::chrono::system_clock::now())
    , m_constraints(nlohmann::json::object())
    , m_lastSnapshot(nlohmann::json::object())
{
    fprintf(stderr, "[AgenticLoopState] Initialized - Ready for iterative reasoning\n");
}

AgenticLoopState::~AgenticLoopState()
{
    fprintf(stderr, "[AgenticLoopState] Destroyed - Cleaned up %zu iterations\n", m_iterations.size());
}

// ===== ITERATION MANAGEMENT =====

void AgenticLoopState::startIteration(const std::string& goal)
{
    Iteration iteration;
    iteration.iterationNumber = static_cast<int>(m_iterations.size()) + 1;
    iteration.startTime = std::chrono::system_clock::now();
    iteration.currentPhase = ReasoningPhase::Analysis;
    iteration.status = IterationStatus::InProgress;
    iteration.goalStatement = goal;
    iteration.contextSnapshot = getAllMemory();
    iteration.successScore = 0.0f;
    iteration.errorCount = 0;

    m_iterations.push_back(iteration);
    m_currentStatus = IterationStatus::InProgress;

    if (m_debugMode) {
        fprintf(stderr, "[AgenticLoopState] Started iteration %d - %s\n",
                iteration.iterationNumber, goal.c_str());
    }
}

void AgenticLoopState::endIteration(IterationStatus status, const std::string& result)
{
    if (m_iterations.empty()) return;

    Iteration& iteration = m_iterations.back();
    iteration.endTime = std::chrono::system_clock::now();
    iteration.status = status;
    iteration.resultSummary = result;

    // Update context window
    m_contextWindow.push_back(iteration);
    if (static_cast<int>(m_contextWindow.size()) > m_contextWindowSize) {
        m_contextWindow.pop_front();
    }

    m_currentStatus = status;
    m_lastUpdateTime = std::chrono::system_clock::now();

    if (m_debugMode) {
        fprintf(stderr, "[AgenticLoopState] Ended iteration %d - Status: %s\n",
                iteration.iterationNumber, statusToString(status).c_str());
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
    m_lastUpdateTime = std::chrono::system_clock::now();

    if (m_debugMode) {
        fprintf(stderr, "[AgenticLoopState] Phase transitioned to %s\n",
                phaseToString(phase).c_str());
    }
}

float AgenticLoopState::getPhaseProgress() const
{
    if (m_iterations.empty()) return 0.0f;

    int completed = getCompletedIterations();
    float baseProgress = (completed * 100.0f) / m_iterations.size();

    auto curr = m_iterations.back();
    float phaseWeight = 0.0f;
    switch (curr.currentPhase) {
        case ReasoningPhase::Analysis:     phaseWeight = 0.1f;  break;
        case ReasoningPhase::Planning:     phaseWeight = 0.3f;  break;
        case ReasoningPhase::Execution:    phaseWeight = 0.6f;  break;
        case ReasoningPhase::Verification: phaseWeight = 0.8f;  break;
        case ReasoningPhase::Reflection:   phaseWeight = 0.9f;  break;
        case ReasoningPhase::Adjustment:   phaseWeight = 0.95f; break;
    }

    float iterationProgress = phaseWeight / m_iterations.size() * 100.0f;
    return std::min(99.9f, baseProgress + iterationProgress);
}

std::vector<std::string> AgenticLoopState::getAllPhaseTransitions() const
{
    std::vector<std::string> transitions;
    for (const auto& iteration : m_iterations) {
        transitions.push_back(phaseToString(iteration.currentPhase));
    }
    return transitions;
}

// ===== DECISION TRACKING =====

void AgenticLoopState::recordDecision(
    const std::string& description,
    const nlohmann::json& reasoning,
    float confidence)
{
    if (m_iterations.empty()) return;

    Decision decision;
    decision.timestamp = std::chrono::system_clock::now();
    decision.phase = m_currentPhase;
    decision.description = description;
    decision.reasoning = reasoning;
    decision.confidence = confidence;
    decision.success = false;
    decision.retryCount = 0;

    m_iterations.back().decisions.push_back(decision);

    if (m_debugMode) {
        fprintf(stderr, "[AgenticLoopState] Recorded decision: %s - Confidence: %.2f\n",
                description.c_str(), confidence);
    }
}

void AgenticLoopState::recordDecisionOutcome(
    int decisionIndex,
    const nlohmann::json& outcome,
    bool success)
{
    if (m_iterations.empty() ||
        decisionIndex >= static_cast<int>(m_iterations.back().decisions.size())) {
        return;
    }

    auto& decision = m_iterations.back().decisions[decisionIndex];
    decision.outcome = outcome;
    decision.success = success;

    if (m_debugMode) {
        fprintf(stderr, "[AgenticLoopState] Decision outcome recorded - Success: %s\n",
                success ? "true" : "false");
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

    if (limit > 0 && static_cast<int>(allDecisions.size()) > limit) {
        allDecisions.erase(allDecisions.begin(),
                           allDecisions.end() - limit);
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
    error.timestamp = std::chrono::system_clock::now();
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
        fprintf(stderr, "[AgenticLoopState] Error recorded: %s - %s\n",
                errorType.c_str(), message.c_str());
    }
}

void AgenticLoopState::recordErrorRecovery(
    int errorIndex,
    const std::string& strategy,
    bool succeeded)
{
    if (errorIndex >= static_cast<int>(m_errorHistory.size())) return;

    auto& error = m_errorHistory[errorIndex];
    error.recoveryStrategy = strategy;
    error.recoverySucceeded = succeeded;

    if (m_debugMode) {
        fprintf(stderr, "[AgenticLoopState] Error recovery recorded - Strategy: %s - Success: %s\n",
                strategy.c_str(), succeeded ? "true" : "false");
    }
}

const std::deque<AgenticLoopState::ErrorRecord>& AgenticLoopState::getErrorHistory(size_t limit) const
{
    return m_errorHistory;
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
    nlohmann::json analysis;
    analysis["total_errors"] = getTotalErrorCount();
    analysis["error_rate"] = getErrorRate();

    // Group errors by type
    std::unordered_map<std::string, int> errorTypeCounts;
    for (const auto& error : m_errorHistory) {
        errorTypeCounts[error.errorType]++;
    }

    nlohmann::json errorTypes = nlohmann::json::object();
    for (const auto& pair : errorTypeCounts) {
        errorTypes[pair.first] = pair.second;
    }
    analysis["error_types"] = errorTypes;

    return analysis.dump(2);
}

// ===== STATE SNAPSHOTS =====

QJsonObject AgenticLoopState::takeSnapshot()
{
    QJsonObject snapshot;
    snapshot["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
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

bool AgenticLoopState::restoreFromSnapshot(const QJsonObject& snapshot)
{
    // Restore limited state from snapshot
    // Note: Full state restoration would require serializing entire iteration history
    m_currentPhase = stringToPhase(snapshot["phase"].toString());
    m_currentStatus = stringToStatus(snapshot["status"].toString());
    m_lastSnapshot = snapshot;

    if (m_debugMode) {
        qDebug() << "[AgenticLoopState] Restored from snapshot";
    }

    return true;
}

// ===== MEMORY MANAGEMENT =====

void AgenticLoopState::addToMemory(const QString& key, const QVariant& value)
{
    m_memory[key.toStdString()] = value;
    m_lastUpdateTime = QDateTime::currentDateTime();
}

QVariant AgenticLoopState::getFromMemory(const QString& key)
{
    auto it = m_memory.find(key.toStdString());
    if (it != m_memory.end()) {
        return it->second;
    }
    return QVariant();
}

void AgenticLoopState::removeFromMemory(const QString& key)
{
    m_memory.erase(key.toStdString());
}

QJsonObject AgenticLoopState::getAllMemory() const
{
    QJsonObject obj;
    for (const auto& pair : m_memory) {
        obj[QString::fromStdString(pair.first)] = QJsonValue::fromVariant(pair.second);
    }
    return obj;
}

void AgenticLoopState::clearMemoryExcept(const QStringList& keysToKeep)
{
    std::unordered_map<std::string, QVariant> newMemory;
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

QJsonArray AgenticLoopState::getContextWindow() const
{
    QJsonArray array;
    for (const auto& iteration : m_contextWindow) {
        QJsonObject obj;
        obj["iteration"] = iteration.iterationNumber;
        obj["goal"] = iteration.goalStatement;
        obj["status"] = statusToString(iteration.status);
        array.append(obj);
    }
    return array;
}

QString AgenticLoopState::formatContextForModel() const
{
    QString context;
    context += "=== REASONING CONTEXT ===\n";
    context += QString("Current Goal: %1\n").arg(m_currentGoal);
    context += QString("Current Phase: %1\n").arg(phaseToString(m_currentPhase));
    context += QString("Total Iterations: %1\n").arg(getTotalIterations());
    context += QString("Progress: %1%\n").arg(static_cast<int>(getProgressPercentage()));

    context += "\n=== RECENT DECISIONS ===\n";
    auto decisions = getDecisionHistory(5);
    for (const auto& decision : decisions) {
        context += QString("- %1 (Confidence: %2)\n")
                  .arg(decision.description)
                  .arg(decision.confidence);
    }

    return context;
}

// ===== GOAL AND PROGRESS =====

void AgenticLoopState::updateProgress(int current, int total)
{
    m_progressCurrent = current;
    m_progressTotal = total;
    m_lastUpdateTime = QDateTime::currentDateTime();
}

float AgenticLoopState::getProgressPercentage() const
{
    if (m_progressTotal == 0) {
        return getPhaseProgress();
    }
    return (m_progressCurrent * 100.0f) / m_progressTotal;
}

QJsonObject AgenticLoopState::getProgressInfo() const
{
    QJsonObject info;
    info["current"] = m_progressCurrent;
    info["total"] = m_progressTotal;
    info["percentage"] = getProgressPercentage();
    info["phase"] = phaseToString(m_currentPhase);
    info["status"] = statusToString(m_currentStatus);
    return info;
}

// ===== CONSTRAINTS =====

void AgenticLoopState::addConstraint(const QString& key, const QString& constraint)
{
    m_constraints[key] = constraint;
}

void AgenticLoopState::removeConstraint(const QString& key)
{
    m_constraints.remove(key);
}

bool AgenticLoopState::validateAgainstConstraints(const QJsonObject& action) const
{
    // Simple constraint validation - can be extended
    for (auto it = m_constraints.constBegin(); it != m_constraints.constEnd(); ++it) {
        const QString& constraintValue = it.value().toString();
        if (!action.contains(it.key()) && !constraintValue.isEmpty()) {
            return false;
        }
    }
    return true;
}

// ===== STRATEGY TRACKING =====

void AgenticLoopState::recordAppliedStrategy(const QString& strategy)
{
    m_appliedStrategies.append(strategy);
    if (!m_iterations.empty()) {
        m_iterations.back().appliedStrategies.append(strategy);
    }
}

void AgenticLoopState::setSuggestedStrategies(const QStringList& strategies)
{
    m_suggestedStrategies = strategies;
}

// ===== METRICS =====

QJsonObject AgenticLoopState::getMetrics() const
{
    QJsonObject metrics;
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

QString AgenticLoopState::getStateAsSummary() const
{
    QString summary;
    summary += QString("=== AGENTIC LOOP STATE ===\n");
    summary += QString("Current Phase: %1\n").arg(phaseToString(m_currentPhase));
    summary += QString("Status: %1\n").arg(statusToString(m_currentStatus));
    summary += QString("Iterations: %1/%2 completed\n")
              .arg(getCompletedIterations())
              .arg(getTotalIterations());
    summary += QString("Success Rate: %1%\n").arg(static_cast<int>(getOverallSuccessRate()));
    summary += QString("Errors: %1\n").arg(getTotalErrorCount());

    return summary;
}

// ===== SERIALIZATION =====

QString AgenticLoopState::serializeState() const
{
    QJsonObject state;
    state["phase"] = phaseToString(m_currentPhase);
    state["status"] = statusToString(m_currentStatus);
    state["goal"] = m_currentGoal;
    state["metrics"] = getMetrics();
    state["memory"] = getAllMemory();
    state["constraints"] = m_constraints;

    return QString::fromUtf8(QJsonDocument(state).toJson());
}

bool AgenticLoopState::deserializeState(const QString& jsonStr)
{
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    if (!doc.isObject()) return false;

    QJsonObject state = doc.object();
    m_currentPhase = stringToPhase(state["phase"].toString());
    m_currentStatus = stringToStatus(state["status"].toString());
    m_currentGoal = state["goal"].toString();

    return true;
}

// ===== DEBUGGING =====

QString AgenticLoopState::generateDebugReport() const
{
    QString report;
    report += "=== AGENTIC LOOP STATE DEBUG REPORT ===\n\n";

    report += "ITERATIONS:\n";
    for (const auto& iteration : m_iterations) {
        report += QString("  %1. %2 - %3\n")
                  .arg(iteration.iterationNumber)
                  .arg(iteration.goalStatement)
                  .arg(statusToString(iteration.status));
        report += QString("     Decisions: %1, Errors: %2\n")
                  .arg(iteration.decisions.size())
                  .arg(iteration.errorCount);
    }

    report += "\nERRORS:\n";
    for (const auto& error : m_errorHistory) {
        report += QString("  [%1] %2 - %3\n")
                  .arg(error.timestamp.toString("hh:mm:ss"))
                  .arg(error.errorType)
                  .arg(error.errorMessage);
    }

    report += "\nMETRICS:\n";
    QJsonObject metrics = getMetrics();
    for (auto it = metrics.constBegin(); it != metrics.constEnd(); ++it) {
        report += QString("  %1: %2\n").arg(it.key()).arg(it.value().toVariant().toString());
    }

    return report;
}

// ===== HELPER METHODS =====

QString AgenticLoopState::phaseToString(ReasoningPhase phase) const
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

AgenticLoopState::ReasoningPhase AgenticLoopState::stringToPhase(const QString& str) const
{
    if (str == "Analysis") return ReasoningPhase::Analysis;
    if (str == "Planning") return ReasoningPhase::Planning;
    if (str == "Execution") return ReasoningPhase::Execution;
    if (str == "Verification") return ReasoningPhase::Verification;
    if (str == "Reflection") return ReasoningPhase::Reflection;
    if (str == "Adjustment") return ReasoningPhase::Adjustment;
    return ReasoningPhase::Analysis;
}

QString AgenticLoopState::statusToString(IterationStatus status) const
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

AgenticLoopState::IterationStatus AgenticLoopState::stringToStatus(const QString& str) const
{
    if (str == "NotStarted") return IterationStatus::NotStarted;
    if (str == "InProgress") return IterationStatus::InProgress;
    if (str == "Completed") return IterationStatus::Completed;
    if (str == "Failed") return IterationStatus::Failed;
    if (str == "Recovering") return IterationStatus::Recovering;
    if (str == "MaxAttemptsReached") return IterationStatus::MaxAttemptsReached;
    return IterationStatus::NotStarted;
}
