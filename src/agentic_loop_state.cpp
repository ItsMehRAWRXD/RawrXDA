// AgenticLoopState Implementation (Qt-free)
#include "agentic_loop_state.h"
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <sstream>
#include <iomanip>

namespace {
int64_t toUnixMillis(const TimePoint& tp)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
}

TimePoint fromUnixMillis(const nlohmann::json& value, const TimePoint& fallback = std::chrono::system_clock::now())
{
    if (!value.is_number_integer()) {
        return fallback;
    }

    return TimePoint(std::chrono::milliseconds(value.get<int64_t>()));
}
}

// ===== STATIC HELPERS =====

// Const overloads (actual implementation)
std::string AgenticLoopState::timePointToISO(const TimePoint& tp) const
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

std::string AgenticLoopState::timePointToHMS(const TimePoint& tp) const
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
}

AgenticLoopState::~AgenticLoopState()
{
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
        iteration.goal = goal;
    iteration.contextSnapshot = getAllMemory();
    iteration.successScore = 0.0f;
    iteration.errorCount = 0;

    m_iterations.push_back(iteration);
    m_currentStatus = IterationStatus::InProgress;
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
}

AgenticLoopState::Iteration* AgenticLoopState::getCurrentIteration()
{
    if (m_iterations.empty()) return nullptr;
    return &m_iterations.back();
}

// ===== PHASE MANAGEMENT =====

void AgenticLoopState::setCurrentPhase(ReasoningPhase phase)
{
    m_lastPhaseFinished = m_currentPhase;
    m_currentPhase = phase;
    m_lastPhaseStarted = phase;
    m_lastUpdateTime = std::chrono::system_clock::now();
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

const AgenticLoopState::Iteration* AgenticLoopState::getLastStartedIteration() const
{
    if (m_iterations.empty()) return nullptr;
    return &m_iterations.back();
}

const AgenticLoopState::Iteration* AgenticLoopState::getLastCompletedIteration() const
{
    if (!m_contextWindow.empty())
        return &m_contextWindow.back();
    for (auto it = m_iterations.crbegin(); it != m_iterations.crend(); ++it) {
        if (it->status == IterationStatus::Completed || it->status == IterationStatus::Failed ||
            it->status == IterationStatus::MaxAttemptsReached)
            return &(*it);
    }
    return nullptr;
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
}

void AgenticLoopState::recordDecisionOutcome(int decisionIndex, const nlohmann::json& outcome, bool success)
{
    if (m_iterations.empty() ||
        decisionIndex >= static_cast<int>(m_iterations.back().decisions.size())) {
        return;
    }

    auto& decision = m_iterations.back().decisions[decisionIndex];
    decision.outcome = outcome;
    decision.success = success;
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

    return analysis.dump();
}

// ===== SNAPSHOTS =====

nlohmann::json AgenticLoopState::takeSnapshot()
{
    nlohmann::json snapshot;
    snapshot["timestamp"] = timePointToISO(std::chrono::system_clock::now());
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

bool AgenticLoopState::restoreFromSnapshot(const nlohmann::json& snapshot)
{
    // Restore limited state from snapshot
    if (snapshot.contains("phase") && snapshot["phase"].is_string()) {
        m_currentPhase = stringToPhase(snapshot["phase"].get<std::string>());
    }
    if (snapshot.contains("status") && snapshot["status"].is_string()) {
        m_currentStatus = stringToStatus(snapshot["status"].get<std::string>());
    }
    m_lastSnapshot = snapshot;

    return true;
}

void AgenticLoopState::addToMemory(const std::string& key, const nlohmann::json& value)
{
    m_memory[key] = value;
    m_lastUpdateTime = std::chrono::system_clock::now();
}

nlohmann::json AgenticLoopState::getFromMemory(const std::string& key)
{
    auto it = m_memory.find(key);
    if (it != m_memory.end()) {
        return it->second;
    }
    return nlohmann::json();
}

void AgenticLoopState::removeFromMemory(const std::string& key)
{
    m_memory.erase(key);
}

nlohmann::json AgenticLoopState::getAllMemory() const
{
    nlohmann::json obj = nlohmann::json::object();
    for (const auto& pair : m_memory) {
        obj[pair.first] = pair.second;
    }
    return obj;
}

void AgenticLoopState::clearMemoryExcept(const std::vector<std::string>& keysToKeep)
{
    std::unordered_map<std::string, nlohmann::json> newMemory;
    for (const auto& key : keysToKeep) {
        auto it = m_memory.find(key);
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

nlohmann::json AgenticLoopState::getContextWindow() const
{
    nlohmann::json array = nlohmann::json::array();
    for (const auto& iteration : m_contextWindow) {
        nlohmann::json obj;
        obj["iteration"] = iteration.iterationNumber;
        obj["goal"] = iteration.goalStatement;
        obj["status"] = statusToString(iteration.status);
        array.push_back(obj);
    }
    return array;
}

std::string AgenticLoopState::formatContextForModel() const
{
    std::string context;
    context += "=== REASONING CONTEXT ===\n";
    context += "Current Goal: " + m_currentGoal + "\n";
    context += "Current Phase: " + phaseToString(m_currentPhase) + "\n";
    context += "Total Iterations: " + std::to_string(getTotalIterations()) + "\n";
    context += "Progress: " + std::to_string(static_cast<int>(getProgressPercentage())) + "%\n";

    context += "\n=== RECENT DECISIONS ===\n";
    auto decisions = getDecisionHistory(5);
    for (const auto& decision : decisions) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.2f", decision.confidence);
        context += "- " + decision.description + " (Confidence: " + buf + ")\n";
    }

    return context;
}

// ===== GOAL AND PROGRESS =====

void AgenticLoopState::updateProgress(int current, int total)
{
    m_progressCurrent = current;
    m_progressTotal = total;
    m_lastUpdateTime = std::chrono::system_clock::now();
}

float AgenticLoopState::getProgressPercentage() const
{
    if (m_progressTotal == 0) {
        return getPhaseProgress();
    }
    return (m_progressCurrent * 100.0f) / m_progressTotal;
}

nlohmann::json AgenticLoopState::getProgressInfo() const
{
    nlohmann::json info;
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

bool AgenticLoopState::validateAgainstConstraints(const nlohmann::json& action) const
{
    // Simple constraint validation — can be extended
    for (auto it = m_constraints.begin(); it != m_constraints.end(); ++it) {
        const std::string& constraintValue = it.value().get<std::string>();
        if (!action.contains(it.key()) && !constraintValue.empty()) {
            return false;
        }
    }
    return true;
}

// ===== STRATEGY TRACKING =====

void AgenticLoopState::recordAppliedStrategy(const std::string& strategy)
{
    m_appliedStrategies.push_back(strategy);
    if (!m_iterations.empty()) {
        m_iterations.back().appliedStrategies.push_back(strategy);
    }
}

void AgenticLoopState::setSuggestedStrategies(const std::vector<std::string>& strategies)
{
    m_suggestedStrategies = strategies;
}

// ===== METRICS =====

nlohmann::json AgenticLoopState::getMetrics() const
{
    nlohmann::json metrics;
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
    summary += "=== AGENTIC LOOP STATE ===\n";
    summary += "Current Phase: " + phaseToString(m_currentPhase) + "\n";
    summary += "Status: " + statusToString(m_currentStatus) + "\n";
    summary += "Iterations: " + std::to_string(getCompletedIterations()) + "/"
             + std::to_string(getTotalIterations()) + " completed\n";
    summary += "Success Rate: " + std::to_string(static_cast<int>(getOverallSuccessRate())) + "%\n";
    summary += "Errors: " + std::to_string(getTotalErrorCount()) + "\n";

    return summary;
}

// ===== SERIALIZATION =====

std::string AgenticLoopState::serializeState() const
{
    nlohmann::json state = nlohmann::json::object();
    state["schemaVersion"] = 2;
    state["phase"] = phaseToString(m_currentPhase);
    state["status"] = statusToString(m_currentStatus);
    state["goal"] = m_currentGoal;
    state["metrics"] = getMetrics();
    state["memory"] = getAllMemory();
    state["constraints"] = m_constraints;
    state["progress"] = {
        {"current", m_progressCurrent},
        {"total", m_progressTotal}
    };
    state["appliedStrategies"] = m_appliedStrategies;
    state["suggestedStrategies"] = m_suggestedStrategies;
    state["stateStartTimeMs"] = toUnixMillis(m_stateStartTime);
    state["lastUpdateTimeMs"] = toUnixMillis(m_lastUpdateTime);
    state["lastPhaseStarted"] = phaseToString(m_lastPhaseStarted);
    state["lastPhaseFinished"] = phaseToString(m_lastPhaseFinished);
    state["lastSnapshot"] = m_lastSnapshot;
    state["snapshotHistory"] = m_snapshotHistory;

    nlohmann::json iterations = nlohmann::json::array();
    for (const auto& iteration : m_iterations) {
        nlohmann::json iterationJson;
        iterationJson["iterationNumber"] = iteration.iterationNumber;
        iterationJson["startTimeMs"] = toUnixMillis(iteration.startTime);
        iterationJson["endTimeMs"] = toUnixMillis(iteration.endTime);
        iterationJson["currentPhase"] = phaseToString(iteration.currentPhase);
        iterationJson["status"] = statusToString(iteration.status);
        iterationJson["goalStatement"] = iteration.goalStatement;
        iterationJson["contextSnapshot"] = iteration.contextSnapshot;
        iterationJson["successScore"] = iteration.successScore;
        iterationJson["errorCount"] = iteration.errorCount;
        iterationJson["resultSummary"] = iteration.resultSummary;
        iterationJson["appliedStrategies"] = iteration.appliedStrategies;
        iterationJson["goal"] = iteration.goal;
        iterationJson["outcome"] = iteration.outcome;
        iterationJson["reasoning"] = iteration.reasoning;
        iterationJson["error"] = iteration.error;

        nlohmann::json decisions = nlohmann::json::array();
        for (const auto& decision : iteration.decisions) {
            decisions.push_back({
                {"timestampMs", toUnixMillis(decision.timestamp)},
                {"phase", phaseToString(decision.phase)},
                {"description", decision.description},
                {"reasoning", decision.reasoning},
                {"confidence", decision.confidence},
                {"success", decision.success},
                {"retryCount", decision.retryCount},
                {"outcome", decision.outcome}
            });
        }
        iterationJson["decisions"] = std::move(decisions);
        iterations.push_back(std::move(iterationJson));
    }
    state["iterations"] = std::move(iterations);

    nlohmann::json contextWindow = nlohmann::json::array();
    for (const auto& iteration : m_contextWindow) {
        contextWindow.push_back({
            {"iterationNumber", iteration.iterationNumber},
            {"goalStatement", iteration.goalStatement},
            {"status", statusToString(iteration.status)}
        });
    }
    state["contextWindow"] = std::move(contextWindow);

    nlohmann::json errorHistory = nlohmann::json::array();
    for (const auto& error : m_errorHistory) {
        errorHistory.push_back({
            {"timestampMs", toUnixMillis(error.timestamp)},
            {"errorType", error.errorType},
            {"errorMessage", error.errorMessage},
            {"stackTrace", error.stackTrace},
            {"phase", phaseToString(error.phase)},
            {"recoveryAttempt", error.recoveryAttempt},
            {"recoverySucceeded", error.recoverySucceeded},
            {"recoveryStrategy", error.recoveryStrategy}
        });
    }
    state["errorHistory"] = std::move(errorHistory);

    return state.dump();
}

bool AgenticLoopState::deserializeState(const std::string& jsonStr)
{
    try {
        nlohmann::json state = nlohmann::json::parse(jsonStr);
        if (!state.is_object()) return false;

        m_iterations.clear();
        m_contextWindow.clear();
        m_errorHistory.clear();
        m_snapshotHistory.clear();
        m_appliedStrategies.clear();
        m_suggestedStrategies.clear();

        if (state.contains("phase") && state["phase"].is_string())
            m_currentPhase = stringToPhase(state["phase"].get<std::string>());
        if (state.contains("status") && state["status"].is_string())
            m_currentStatus = stringToStatus(state["status"].get<std::string>());
        if (state.contains("goal") && state["goal"].is_string())
            m_currentGoal = state["goal"].get<std::string>();

        if (state.contains("memory") && state["memory"].is_object()) {
            m_memory.clear();
            const auto& memObj = state["memory"];
            for (const auto& [key, value] : memObj.items()) {
                m_memory[key] = value;
            }
        }

        if (state.contains("constraints") && state["constraints"].is_object()) {
            m_constraints = state["constraints"];
        }

        if (state.contains("progress") && state["progress"].is_object()) {
            m_progressCurrent = state["progress"].value("current", 0);
            m_progressTotal = state["progress"].value("total", 0);
        }

        if (state.contains("appliedStrategies") && state["appliedStrategies"].is_array()) {
            m_appliedStrategies = state["appliedStrategies"].get<std::vector<std::string>>();
        }

        if (state.contains("suggestedStrategies") && state["suggestedStrategies"].is_array()) {
            m_suggestedStrategies = state["suggestedStrategies"].get<std::vector<std::string>>();
        }

        m_stateStartTime = fromUnixMillis(state.value<int64_t>("stateStartTimeMs", 0), std::chrono::system_clock::now());
        m_lastUpdateTime = fromUnixMillis(state.value<int64_t>("lastUpdateTimeMs", 0), std::chrono::system_clock::now());

        if (state.contains("lastPhaseStarted") && state["lastPhaseStarted"].is_string()) {
            m_lastPhaseStarted = stringToPhase(state["lastPhaseStarted"].get<std::string>());
        }
        if (state.contains("lastPhaseFinished") && state["lastPhaseFinished"].is_string()) {
            m_lastPhaseFinished = stringToPhase(state["lastPhaseFinished"].get<std::string>());
        }

        m_lastSnapshot = state.value("lastSnapshot", nlohmann::json::object());
        if (state.contains("snapshotHistory") && state["snapshotHistory"].is_array()) {
            m_snapshotHistory = state["snapshotHistory"].get<std::vector<nlohmann::json>>();
        }

        if (state.contains("iterations") && state["iterations"].is_array()) {
            for (const auto& iterationJson : state["iterations"]) {
                if (!iterationJson.is_object()) {
                    continue;
                }

                Iteration iteration;
                iteration.iterationNumber = iterationJson.value("iterationNumber", 0);
                iteration.startTime = fromUnixMillis(iterationJson.value<int64_t>("startTimeMs", 0), std::chrono::system_clock::now());
                iteration.endTime = fromUnixMillis(iterationJson.value<int64_t>("endTimeMs", 0), iteration.startTime);
                iteration.currentPhase = stringToPhase(iterationJson.value("currentPhase", std::string("Analysis")));
                iteration.status = stringToStatus(iterationJson.value("status", std::string("NotStarted")));
                iteration.goalStatement = iterationJson.value("goalStatement", "");
                iteration.contextSnapshot = iterationJson.value("contextSnapshot", nlohmann::json::object());
                iteration.successScore = iterationJson.value("successScore", 0.0f);
                iteration.errorCount = iterationJson.value("errorCount", 0);
                iteration.resultSummary = iterationJson.value("resultSummary", "");
                iteration.appliedStrategies = iterationJson.value("appliedStrategies", std::vector<std::string>{});
                iteration.goal = iterationJson.value("goal", iteration.goalStatement);
                iteration.outcome = iterationJson.value("outcome", "");
                iteration.reasoning = iterationJson.value("reasoning", "");
                iteration.error = iterationJson.value("error", "");

                if (iterationJson.contains("decisions") && iterationJson["decisions"].is_array()) {
                    for (const auto& decisionJson : iterationJson["decisions"]) {
                        if (!decisionJson.is_object()) {
                            continue;
                        }

                        Decision decision;
                        decision.timestamp = fromUnixMillis(decisionJson.value<int64_t>("timestampMs", 0), std::chrono::system_clock::now());
                        decision.phase = stringToPhase(decisionJson.value("phase", std::string("Analysis")));
                        decision.description = decisionJson.value("description", "");
                        decision.reasoning = decisionJson.value("reasoning", nlohmann::json::object());
                        decision.confidence = decisionJson.value("confidence", 0.0f);
                        decision.success = decisionJson.value("success", false);
                        decision.retryCount = decisionJson.value("retryCount", 0);
                        decision.outcome = decisionJson.value("outcome", nlohmann::json::object());
                        iteration.decisions.push_back(std::move(decision));
                    }
                }

                m_iterations.push_back(std::move(iteration));
            }
        }

        if (state.contains("contextWindow") && state["contextWindow"].is_array()) {
            for (const auto& contextJson : state["contextWindow"]) {
                if (!contextJson.is_object()) {
                    continue;
                }

                Iteration contextIteration;
                contextIteration.iterationNumber = contextJson.value("iterationNumber", 0);
                contextIteration.goalStatement = contextJson.value("goalStatement", "");
                contextIteration.status = stringToStatus(contextJson.value("status", std::string("NotStarted")));
                m_contextWindow.push_back(std::move(contextIteration));
            }
        }

        if (state.contains("errorHistory") && state["errorHistory"].is_array()) {
            for (const auto& errorJson : state["errorHistory"]) {
                if (!errorJson.is_object()) {
                    continue;
                }

                ErrorRecord error;
                error.timestamp = fromUnixMillis(errorJson.value<int64_t>("timestampMs", 0), std::chrono::system_clock::now());
                error.errorType = errorJson.value("errorType", "");
                error.errorMessage = errorJson.value("errorMessage", "");
                error.stackTrace = errorJson.value("stackTrace", "");
                error.phase = stringToPhase(errorJson.value("phase", std::string("Analysis")));
                error.recoveryAttempt = errorJson.value("recoveryAttempt", 0);
                error.recoverySucceeded = errorJson.value("recoverySucceeded", false);
                error.recoveryStrategy = errorJson.value("recoveryStrategy", "");
                m_errorHistory.push_back(std::move(error));
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

// ===== HELPER METHODS =====

std::string AgenticLoopState::phaseToString(ReasoningPhase phase) const
{
    switch (phase) {
        case ReasoningPhase::Analysis:     return "Analysis";
        case ReasoningPhase::Planning:     return "Planning";
        case ReasoningPhase::Execution:    return "Execution";
        case ReasoningPhase::Verification: return "Verification";
        case ReasoningPhase::Reflection:   return "Reflection";
        case ReasoningPhase::Adjustment:   return "Adjustment";
        default:                           return "Unknown";
    }
}

ReasoningPhase AgenticLoopState::stringToPhase(const std::string& str) const
{
    if (str == "Analysis")     return ReasoningPhase::Analysis;
    if (str == "Planning")     return ReasoningPhase::Planning;
    if (str == "Execution")    return ReasoningPhase::Execution;
    if (str == "Verification") return ReasoningPhase::Verification;
    if (str == "Reflection")   return ReasoningPhase::Reflection;
    if (str == "Adjustment")   return ReasoningPhase::Adjustment;
    return ReasoningPhase::Analysis;
}

std::string AgenticLoopState::statusToString(IterationStatus status) const
{
    switch (status) {
        case IterationStatus::NotStarted:         return "NotStarted";
        case IterationStatus::InProgress:         return "InProgress";
        case IterationStatus::Completed:          return "Completed";
        case IterationStatus::Failed:             return "Failed";
        case IterationStatus::Recovering:         return "Recovering";
        case IterationStatus::MaxAttemptsReached: return "MaxAttemptsReached";
        default:                                  return "Unknown";
    }
}

IterationStatus AgenticLoopState::stringToStatus(const std::string& str) const
{
    if (str == "NotStarted")         return IterationStatus::NotStarted;
    if (str == "InProgress")         return IterationStatus::InProgress;
    if (str == "Completed")          return IterationStatus::Completed;
    if (str == "Failed")             return IterationStatus::Failed;
    if (str == "Recovering")         return IterationStatus::Recovering;
    if (str == "MaxAttemptsReached") return IterationStatus::MaxAttemptsReached;
    return IterationStatus::NotStarted;
}
