#include "autonomous_agent.hpp"
#include <QDebug>
#include <QUuid>
#include <QRandomGenerator>
#include <algorithm>
#include <cmath>

namespace RawrXD {

// ============================================================
// SelfCorrectionEngine Implementation
// ============================================================

SelfCorrectionEngine& SelfCorrectionEngine::instance() {
    static SelfCorrectionEngine s_instance;
    return s_instance;
}

CorrectionStep SelfCorrectionEngine::analyzeFailure(const QString& taskDescription, const QString& errorMessage, const QStringList& previousAttempts) {
    CorrectionStep correction;
    correction.problemDescription = errorMessage;
    
    // Pattern-based diagnosis
    QString pattern = recognizeErrorPattern(errorMessage);
    
    if (pattern.contains("file", Qt::CaseInsensitive)) {
        correction.diagnosis = "File or path error detected";
        correction.correctionAction = "Verify file path, check permissions, ensure file exists";
        correction.confidenceScore = 0.85f;
    }
    else if (pattern.contains("permission", Qt::CaseInsensitive)) {
        correction.diagnosis = "Access permission denied";
        correction.correctionAction = "Request elevated privileges or modify access rights";
        correction.confidenceScore = 0.90f;
    }
    else if (pattern.contains("timeout", Qt::CaseInsensitive)) {
        correction.diagnosis = "Operation timed out";
        correction.correctionAction = "Increase timeout, optimize process, break into smaller tasks";
        correction.confidenceScore = 0.80f;
    }
    else if (pattern.contains("memory", Qt::CaseInsensitive)) {
        correction.diagnosis = "Memory allocation failure";
        correction.correctionAction = "Free unused resources, optimize memory usage, increase available memory";
        correction.confidenceScore = 0.88f;
    }
    else if (pattern.contains("network", Qt::CaseInsensitive) || pattern.contains("connection", Qt::CaseInsensitive)) {
        correction.diagnosis = "Network connectivity issue";
        correction.correctionAction = "Check network connection, verify endpoints, retry with backoff";
        correction.confidenceScore = 0.75f;
    }
    else if (pattern.contains("type", Qt::CaseInsensitive) || pattern.contains("mismatch", Qt::CaseInsensitive)) {
        correction.diagnosis = "Type mismatch or invalid input";
        correction.correctionAction = "Validate input types, apply necessary conversions, review contracts";
        correction.confidenceScore = 0.82f;
    }
    else {
        correction.diagnosis = "Unknown error pattern, analyzing context";
        correction.correctionAction = "Review task requirements, check dependencies, try alternative approach";
        correction.confidenceScore = 0.50f;
    }
    
    // Adjust confidence based on retry count
    if (previousAttempts.size() > 2) {
        correction.confidenceScore *= 0.7f;  // Lower confidence on repeated failures
    }
    
    return correction;
}

bool SelfCorrectionEngine::applyCorrection(CorrectionStep& correction) {
    qDebug() << "Applying correction:" << correction.correctionAction;
    
    // Simulate correction application
    correction.applied = true;
    
    return true;
}

bool SelfCorrectionEngine::validateCorrection(const CorrectionStep& correction, const QString& validationResult) {
    qDebug() << "Validating correction with result:" << validationResult;
    
    // Simple validation: if result contains success keywords
    if (validationResult.contains("success", Qt::CaseInsensitive) ||
        validationResult.contains("completed", Qt::CaseInsensitive) ||
        validationResult.contains("ok", Qt::CaseInsensitive)) {
        return true;
    }
    
    return false;
}

QString SelfCorrectionEngine::recognizeErrorPattern(const QString& error) {
    QString errorLower = error.toLower();
    
    if (errorLower.contains("file") || errorLower.contains("path") || errorLower.contains("directory")) {
        return "file";
    }
    else if (errorLower.contains("permission") || errorLower.contains("access denied")) {
        return "permission";
    }
    else if (errorLower.contains("timeout") || errorLower.contains("deadline")) {
        return "timeout";
    }
    else if (errorLower.contains("memory") || errorLower.contains("allocation")) {
        return "memory";
    }
    else if (errorLower.contains("network") || errorLower.contains("connection") || errorLower.contains("socket")) {
        return "network";
    }
    else if (errorLower.contains("type") || errorLower.contains("mismatch")) {
        return "type";
    }
    else if (errorLower.contains("null") || errorLower.contains("nullptr")) {
        return "null";
    }
    
    return "unknown";
}

void SelfCorrectionEngine::recordFailedCorrection(const CorrectionStep& correction, const QString& lesson) {
    qDebug() << "Recording failed correction:" << lesson;
    m_failedCorrections.append(correction.correctionAction);
}

// ============================================================
// IterativePlanningEngine Implementation
// ============================================================

IterativePlanningEngine& IterativePlanningEngine::instance() {
    static IterativePlanningEngine s_instance;
    return s_instance;
}

RefinedPlan IterativePlanningEngine::createInitialPlan(const QString& task, const QStringList& constraints) {
    RefinedPlan plan;
    plan.planId = QUuid::createUuid().toString();
    plan.originalTask = task;
    plan.currentState = PlanState::Initial;
    plan.iterationCount = 0;
    
    // Break down task into steps
    QString taskLower = task.toLower();
    
    if (taskLower.contains("analyze")) {
        plan.steps.append("1. Parse input data");
        plan.steps.append("2. Identify key patterns");
        plan.steps.append("3. Extract metrics");
        plan.steps.append("4. Generate summary");
        plan.stepConfidences = {0.95f, 0.85f, 0.90f, 0.80f};
    }
    else if (taskLower.contains("refactor") || taskLower.contains("optimize")) {
        plan.steps.append("1. Profile current implementation");
        plan.steps.append("2. Identify optimization opportunities");
        plan.steps.append("3. Design improved version");
        plan.steps.append("4. Implement changes");
        plan.steps.append("5. Validate improvements");
        plan.stepConfidences = {0.90f, 0.85f, 0.80f, 0.75f, 0.85f};
    }
    else if (taskLower.contains("test")) {
        plan.steps.append("1. Identify test scenarios");
        plan.steps.append("2. Create test cases");
        plan.steps.append("3. Execute tests");
        plan.steps.append("4. Analyze results");
        plan.stepConfidences = {0.88f, 0.90f, 0.92f, 0.85f};
    }
    else {
        plan.steps.append("1. Understand requirements");
        plan.steps.append("2. Plan approach");
        plan.steps.append("3. Execute implementation");
        plan.steps.append("4. Validate results");
        plan.stepConfidences = {0.85f, 0.80f, 0.75f, 0.80f};
    }
    
    // Apply constraints
    for (const QString& constraint : constraints) {
        if (constraint.contains("time") || constraint.contains("fast")) {
            plan.rationale += "Optimized for speed. ";
        }
        if (constraint.contains("quality") || constraint.contains("accurate")) {
            plan.rationale += "Prioritizing accuracy. ";
        }
    }
    
    plan.riskAssessment = "Low to medium risk. Standard practices applied.";
    
    return plan;
}

RefinedPlan IterativePlanningEngine::refinePlan(const RefinedPlan& plan, const QString& feedback) {
    RefinedPlan refined = plan;
    refined.iterationCount++;
    refined.currentState = PlanState::Refined;
    
    qDebug() << "Refining plan based on feedback:" << feedback;
    
    // Add new steps based on feedback
    if (feedback.contains("error") || feedback.contains("failed")) {
        // Add error recovery steps
        refined.steps.insert(refined.steps.size() - 1, "Recovery: Analyze failure point");
        refined.stepConfidences.insert(refined.stepConfidences.size() - 1, 0.75f);
    }
    
    if (feedback.contains("slow") || feedback.contains("performance")) {
        // Add optimization steps
        refined.steps.insert(0, "Optimization: Profile for bottlenecks");
        refined.stepConfidences.insert(0, 0.80f);
    }
    
    if (feedback.contains("quality") || feedback.contains("missing")) {
        // Add verification steps
        refined.steps.append("Additional Verification: Comprehensive testing");
        refined.stepConfidences.append(0.85f);
    }
    
    // Adjust rationale
    refined.rationale = "Plan refined based on feedback: " + feedback;
    
    return refined;
}

QString IterativePlanningEngine::validatePlan(const RefinedPlan& plan) {
    QString validation;
    
    if (plan.steps.isEmpty()) {
        validation = "ERROR: Plan has no steps";
        return validation;
    }
    
    // Check for circular dependencies
    bool hasIssues = false;
    for (int i = 0; i < plan.dependencies.size(); i++) {
        for (int j = 0; j < plan.dependencies.size(); j++) {
            if (i != j && plan.dependencies[i] == plan.dependencies[j]) {
                validation += "WARNING: Possible circular dependency\n";
                hasIssues = true;
            }
        }
    }
    
    // Validate confidence scores
    float avgConfidence = 0.0f;
    if (!plan.stepConfidences.isEmpty()) {
        for (float conf : plan.stepConfidences) {
            avgConfidence += conf;
        }
        avgConfidence /= plan.stepConfidences.size();
    }
    
    if (avgConfidence < 0.6f) {
        validation += "WARNING: Low average confidence in plan steps\n";
        hasIssues = true;
    }
    
    if (!hasIssues) {
        validation = "VALID: Plan is well-formed and feasible";
    }
    
    return validation;
}

RefinedPlan IterativePlanningEngine::addContingencySteps(const RefinedPlan& plan) {
    RefinedPlan contingent = plan;
    
    // Add contingency steps at the end
    contingent.steps.append("CONTINGENCY: If error occurs, retry with backoff");
    contingent.stepConfidences.append(0.80f);
    
    contingent.steps.append("CONTINGENCY: If timeout, increase resource allocation");
    contingent.stepConfidences.append(0.75f);
    
    contingent.steps.append("CONTINGENCY: If validation fails, revert and analyze");
    contingent.stepConfidences.append(0.85f);
    
    return contingent;
}

int IterativePlanningEngine::estimateStepDuration(const QString& stepDescription) {
    QString stepLower = stepDescription.toLower();
    
    if (stepLower.contains("parse") || stepLower.contains("read")) {
        return 5;  // 5 seconds
    }
    else if (stepLower.contains("analyze") || stepLower.contains("check")) {
        return 10;
    }
    else if (stepLower.contains("implement") || stepLower.contains("execute")) {
        return 30;
    }
    else if (stepLower.contains("test") || stepLower.contains("validate")) {
        return 15;
    }
    else if (stepLower.contains("optimize")) {
        return 20;
    }
    
    return 5;  // Default
}

float IterativePlanningEngine::calculateSuccessProbability(const RefinedPlan& plan) {
    if (plan.stepConfidences.isEmpty()) {
        return 0.5f;
    }
    
    float overallProbability = 1.0f;
    for (float conf : plan.stepConfidences) {
        overallProbability *= conf;
    }
    
    return overallProbability;
}

// ============================================================
// AutonomousAgent Implementation
// ============================================================

AutonomousAgent& AutonomousAgent::instance() {
    static AutonomousAgent s_instance;
    return s_instance;
}

void AutonomousAgent::initialize(const QString& agentId, int maxRetries) {
    m_state.agentId = agentId;
    m_state.maxRetries = maxRetries;
    m_state.retryCount = 0;
    m_state.successRate = 0.0f;
    
    qDebug() << "Autonomous Agent initialized:" << agentId << "with max retries:" << maxRetries;
}

bool AutonomousAgent::executeTaskAutonomously(const QString& task, const QStringList& constraints) {
    m_state.isRunning = true;
    m_state.currentTask = task;
    m_state.retryCount = 0;
    
    qDebug() << "Executing task autonomously:" << task;
    
    // Step 1: Create iterative plan
    IterativePlanningEngine& planner = IterativePlanningEngine::instance();
    RefinedPlan plan = planner.createInitialPlan(task, constraints);
    
    // Add contingency steps
    plan = planner.addContingencySteps(plan);
    
    // Step 2: Validate plan
    QString validation = planner.validatePlan(plan);
    qDebug() << "Plan validation:" << validation;
    
    // Step 3: Execute plan steps
    QString lastError;
    for (int stepIdx = 0; stepIdx < plan.steps.size(); ++stepIdx) {
        const QString& step = plan.steps[stepIdx];
        float confidence = stepIdx < plan.stepConfidences.size() ? plan.stepConfidences[stepIdx] : 0.5f;
        
        m_state.stepCount++;
        
        qDebug() << "Executing step" << stepIdx + 1 << ":" << step;
        
        // Simulate step execution with probabilistic success
        float randomValue = QRandomGenerator::global()->generateDouble();
        if (randomValue > confidence) {
            lastError = "Step failed with confidence threshold";
            qWarning() << "Step failed:" << lastError;
            
            // Attempt correction
            if (m_state.retryCount < m_state.maxRetries) {
                m_state.retryCount++;
                if (attemptWithCorrection(task, lastError)) {
                    return true;
                }
            }
            return false;
        }
    }
    
    m_state.isRunning = false;
    
    // Record success
    AgentMemoryEntry entry;
    entry.taskId = QUuid::createUuid().toString();
    entry.actionType = AgentActionType::Execute;
    entry.actionDescription = task;
    entry.success = true;
    entry.confidence = 0.9f;
    entry.timestamp = QDateTime::currentDateTime();
    m_state.executionHistory.append(entry);
    
    // Update success rate
    int successCount = 0;
    for (const auto& entry : m_state.executionHistory) {
        if (entry.success) successCount++;
    }
    m_state.successRate = (float)successCount / m_state.executionHistory.size();
    
    qDebug() << "Task completed successfully. Success rate:" << m_state.successRate;
    
    return true;
}

QString AutonomousAgent::executeWithIterativeRefinement(const QString& task) {
    IterativePlanningEngine& planner = IterativePlanningEngine::instance();
    RefinedPlan plan = planner.createInitialPlan(task);
    
    QString result = "Plan: " + plan.planId + "\n";
    result += "Original Task: " + plan.originalTask + "\n";
    result += "Steps:\n";
    
    for (int i = 0; i < plan.steps.size(); ++i) {
        result += QString("  %1 (confidence: %2)\n").arg(plan.steps[i]).arg(plan.stepConfidences[i]);
    }
    
    result += "Rationale: " + plan.rationale + "\n";
    result += "Success Probability: " + QString::number(planner.calculateSuccessProbability(plan)) + "\n";
    
    return result;
}

bool AutonomousAgent::attemptWithCorrection(const QString& task, const QString& lastError) {
    SelfCorrectionEngine& correctionEngine = SelfCorrectionEngine::instance();
    
    QStringList previousAttempts;
    for (const auto& entry : m_state.executionHistory) {
        previousAttempts.append(entry.actionDescription);
    }
    
    CorrectionStep correction = correctionEngine.analyzeFailure(task, lastError, previousAttempts);
    
    qDebug() << "Applying correction:" << correction.correctionAction;
    
    if (correctionEngine.applyCorrection(correction)) {
        // Record correction attempt
        AgentMemoryEntry entry;
        entry.taskId = QUuid::createUuid().toString();
        entry.actionType = AgentActionType::Correct;
        entry.actionDescription = correction.correctionAction;
        entry.confidence = correction.confidenceScore;
        entry.timestamp = QDateTime::currentDateTime();
        m_state.executionHistory.append(entry);
        
        // Retry task
        qDebug() << "Retrying task with correction";
        return executeTaskAutonomously(task);
    }
    
    return false;
}

QString AutonomousAgent::monitorExecution(const QString& executionId) {
    QString status = "Execution ID: " + executionId + "\n";
    status += "Agent: " + m_state.agentId + "\n";
    status += "Current Task: " + m_state.currentTask + "\n";
    status += "Steps Completed: " + QString::number(m_state.stepCount) + "\n";
    status += "Retry Count: " + QString::number(m_state.retryCount) + "/" + QString::number(m_state.maxRetries) + "\n";
    status += "Success Rate: " + QString::number(m_state.successRate, 'f', 2) + "\n";
    status += "Is Running: " + QString(m_state.isRunning ? "Yes" : "No") + "\n";
    
    return status;
}

void AutonomousAgent::learnFromSuccess(const QString& task, int duration, const QString& approach) {
    m_state.taskPriors[task] = std::min(1.0f, m_state.taskPriors[task] + 0.05f);
    
    qDebug() << "Learning from success:" << task << "Duration:" << duration << "Approach:" << approach;
    
    AgentMemoryEntry entry;
    entry.actionType = AgentActionType::Learn;
    entry.actionDescription = approach;
    entry.lessons = "Task succeeded in " + QString::number(duration) + " seconds";
    entry.timestamp = QDateTime::currentDateTime();
    m_state.executionHistory.append(entry);
}

void AutonomousAgent::learnFromFailure(const QString& task, const QString& reason, const QString& correction) {
    m_state.taskPriors[task] = std::max(0.0f, m_state.taskPriors[task] - 0.1f);
    
    qDebug() << "Learning from failure:" << task << "Reason:" << reason << "Correction:" << correction;
    
    AgentMemoryEntry entry;
    entry.actionType = AgentActionType::Learn;
    entry.actionDescription = reason;
    entry.lessons = "Failed due to: " + reason + ". Correction applied: " + correction;
    entry.timestamp = QDateTime::currentDateTime();
    m_state.executionHistory.append(entry);
}

float AutonomousAgent::getTaskSuccessProbability(const QString& task) {
    return m_state.taskPriors.value(task, 0.5f);
}

QStringList AutonomousAgent::generateRecommendations(const QString& task) {
    QStringList recommendations;
    
    float successProb = getTaskSuccessProbability(task);
    
    if (successProb < 0.3f) {
        recommendations.append("Task has low success rate. Consider alternative approaches.");
    }
    else if (successProb < 0.6f) {
        recommendations.append("Task has moderate success rate. May require additional setup or prerequisites.");
    }
    else {
        recommendations.append("Task has good success probability. Ready to execute.");
    }
    
    // Check history for patterns
    if (m_state.executionHistory.size() > 5) {
        recommendations.append("Agent has sufficient execution history for pattern analysis.");
    }
    
    return recommendations;
}

// ============================================================
// AgentLearningSystem Implementation
// ============================================================

AgentLearningSystem& AgentLearningSystem::instance() {
    static AgentLearningSystem s_instance;
    return s_instance;
}

void AgentLearningSystem::recordFeedback(const ExecutionFeedback& feedback) {
    m_feedbackHistory.append(feedback);
    qDebug() << "Feedback recorded for execution:" << feedback.executionId << "Success:" << feedback.success;
}

QString AgentLearningSystem::analyzePatterns(const QString& taskType) {
    QString analysis = "Pattern Analysis for: " + taskType + "\n";
    
    int successCount = 0;
    int totalCount = 0;
    int totalDuration = 0;
    
    for (const auto& feedback : m_feedbackHistory) {
        if (feedback.executionId.contains(taskType)) {
            totalCount++;
            totalDuration += (int)feedback.timestamp.secsTo(QDateTime::currentDateTime());
            if (feedback.success) {
                successCount++;
            }
        }
    }
    
    if (totalCount > 0) {
        float successRate = (float)successCount / totalCount;
        int avgDuration = totalDuration / totalCount;
        
        analysis += QString("Success Rate: %1%\n").arg((int)(successRate * 100));
        analysis += QString("Average Duration: %1 seconds\n").arg(avgDuration);
        analysis += QString("Total Attempts: %1\n").arg(totalCount);
    } else {
        analysis += "No execution history available\n";
    }
    
    return analysis;
}

QStringList AgentLearningSystem::generateImprovements(const QString& taskType) {
    QStringList improvements;
    
    QString analysis = analyzePatterns(taskType);
    
    if (analysis.contains("low success", Qt::CaseInsensitive)) {
        improvements.append("Improve error handling for this task type");
        improvements.append("Consider adding validation steps");
    }
    
    improvements.append("Optimize for performance");
    improvements.append("Add more comprehensive logging");
    improvements.append("Consider parallel execution if applicable");
    
    return improvements;
}

void AgentLearningSystem::updateTaskModel(const QString& task, bool success, int duration) {
    m_taskDurations[task].append(duration);
    m_taskAttemptCounts[task] = m_taskAttemptCounts.value(task, 0) + 1;
    
    if (success) {
        m_taskSuccessCounts[task] = m_taskSuccessCounts.value(task, 0) + 1;
    }
    
    qDebug() << "Task model updated:" << task << "Success:" << success << "Duration:" << duration;
}

float AgentLearningSystem::getTaskDifficulty(const QString& task) {
    int attempts = m_taskAttemptCounts.value(task, 0);
    if (attempts == 0) {
        return 0.5f;  // Default difficulty
    }
    
    int successes = m_taskSuccessCounts.value(task, 0);
    float successRate = (float)successes / attempts;
    
    // Difficulty is inverse of success rate
    return 1.0f - successRate;
}

int AgentLearningSystem::predictExecutionTime(const QString& task) {
    auto durations = m_taskDurations.value(task);
    if (durations.isEmpty()) {
        return 10;  // Default prediction
    }
    
    int totalDuration = 0;
    for (int duration : durations) {
        totalDuration += duration;
    }
    
    return totalDuration / durations.size();
}

// ============================================================
// AgentCoordinator Implementation
// ============================================================

AgentCoordinator& AgentCoordinator::instance() {
    static AgentCoordinator s_instance;
    return s_instance;
}

bool AgentCoordinator::createAgent(const QString& agentId) {
    if (m_agents.contains(agentId)) {
        qWarning() << "Agent already exists:" << agentId;
        return false;
    }
    
    AutonomousAgent agent;
    agent.initialize(agentId);
    m_agents[agentId] = agent;
    
    qDebug() << "Agent created:" << agentId;
    return true;
}

QString AgentCoordinator::assignTask(const QString& agentId, const QString& task) {
    if (!m_agents.contains(agentId)) {
        qWarning() << "Agent not found:" << agentId;
        return "";
    }
    
    QString executionId = QString("exec_%1_%2").arg(m_globalExecutionCounter++).arg(QUuid::createUuid().toString().left(8));
    m_executionStatus[executionId] = "PENDING";
    
    AutonomousAgent& agent = m_agents[agentId];
    
    // Execute task asynchronously (simplified)
    if (agent.executeTaskAutonomously(task)) {
        m_executionStatus[executionId] = "COMPLETED";
    } else {
        m_executionStatus[executionId] = "FAILED";
    }
    
    qDebug() << "Task assigned to agent" << agentId << "Execution ID:" << executionId;
    return executionId;
}

QString AgentCoordinator::getTaskStatus(const QString& executionId) {
    return m_executionStatus.value(executionId, "NOT_FOUND");
}

bool AgentCoordinator::cancelTask(const QString& executionId) {
    if (!m_executionStatus.contains(executionId)) {
        return false;
    }
    
    m_executionStatus[executionId] = "CANCELLED";
    return true;
}

QMap<QString, float> AgentCoordinator::getAgentMetrics(const QString& agentId) {
    QMap<QString, float> metrics;
    
    if (!m_agents.contains(agentId)) {
        return metrics;
    }
    
    const AgentState& state = m_agents[agentId].getState();
    metrics["successRate"] = state.successRate;
    metrics["stepCount"] = (float)state.stepCount;
    metrics["retryCount"] = (float)state.retryCount;
    metrics["historySize"] = (float)state.executionHistory.size();
    
    return metrics;
}

QString AgentCoordinator::coordinateMultiAgentTask(const QString& mainTask, const QStringList& subTasks) {
    QString result = "Multi-Agent Coordination: " + mainTask + "\n";
    result += "Sub-tasks:\n";
    
    int agentNum = 1;
    for (const QString& subTask : subTasks) {
        QString agentId = QString("agent_%1").arg(agentNum);
        createAgent(agentId);
        
        QString executionId = assignTask(agentId, subTask);
        result += QString("  Task %1 assigned to %2 (Execution: %3)\n").arg(subTask, agentId, executionId);
        
        agentNum++;
    }
    
    return result;
}

}  // namespace RawrXD
