#pragma once

#include <QString>
#include <QStringList>
#include <QMap>
#include <QVector>
#include <QDateTime>
#include <memory>

namespace RawrXD {

// ============================================================
// Agent Execution State and Memory
// ============================================================

enum class AgentActionType {
    Analyze,
    Plan,
    Execute,
    Validate,
    Correct,
    Learn,
    Feedback,
    Unknown
};

struct AgentMemoryEntry {
    QString taskId;
    AgentActionType actionType;
    QString actionDescription;
    QString result;
    bool success = false;
    float confidence = 0.5f;
    QDateTime timestamp;
    QString lessons;
};

struct AgentState {
    QString agentId;
    QString currentTask;
    int stepCount = 0;
    int maxRetries = 3;
    int retryCount = 0;
    bool isRunning = false;
    float successRate = 0.0f;
    QVector<AgentMemoryEntry> executionHistory;
    QMap<QString, float> taskPriors;  // Success probability per task type
};

// ============================================================
// Self-Correction System
// ============================================================

struct CorrectionStep {
    QString problemDescription;
    QString diagnosis;
    QString correctionAction;
    float confidenceScore = 0.0f;
    bool applied = false;
    QString result;
};

class SelfCorrectionEngine {
public:
    static SelfCorrectionEngine& instance();

    /**
     * Analyze failure and generate correction
     * @param taskDescription What was being attempted
     * @param errorMessage The error that occurred
     * @param previousAttempts History of previous attempts
     * @return Correction step to apply
     */
    CorrectionStep analyzeFailure(const QString& taskDescription, const QString& errorMessage, const QStringList& previousAttempts);

    /**
     * Apply a correction
     * @param correction The correction to apply
     * @return true if correction was successful
     */
    bool applyCorrection(CorrectionStep& correction);

    /**
     * Validate correction result
     * @param correction The correction that was applied
     * @param validationResult The result of validation
     * @return true if correction was effective
     */
    bool validateCorrection(const CorrectionStep& correction, const QString& validationResult);

    /**
     * Pattern-based error recognition
     * @param error The error message
     * @return Common error pattern match
     */
    QString recognizeErrorPattern(const QString& error);

    /**
     * Learn from failed correction
     * @param correction The failed correction
     * @param lesson What was learned
     */
    void recordFailedCorrection(const CorrectionStep& correction, const QString& lesson);

private:
    SelfCorrectionEngine() = default;
    QMap<QString, QString> m_correctionPatterns;
    QStringList m_failedCorrections;
};

// ============================================================
// Iterative Planning System
// ============================================================

enum class PlanState {
    Initial,
    Refined,
    Validated,
    InProgress,
    Completed,
    Failed
};

struct RefinedPlan {
    QString planId;
    QString originalTask;
    QVector<QString> steps;
    QVector<QString> dependencies;
    QVector<float> stepConfidences;
    PlanState currentState = PlanState::Initial;
    int iterationCount = 0;
    int maxIterations = 5;
    QString rationale;
    QString riskAssessment;
};

class IterativePlanningEngine {
public:
    static IterativePlanningEngine& instance();

    /**
     * Create initial plan
     * @param task The task to plan for
     * @param constraints Any constraints on the plan
     * @return Initial plan
     */
    RefinedPlan createInitialPlan(const QString& task, const QStringList& constraints = QStringList());

    /**
     * Refine an existing plan based on feedback
     * @param plan The plan to refine
     * @param feedback Why the refinement is needed
     * @return Refined plan
     */
    RefinedPlan refinePlan(const RefinedPlan& plan, const QString& feedback);

    /**
     * Validate plan feasibility
     * @param plan The plan to validate
     * @return Validation result with issues if any
     */
    QString validatePlan(const RefinedPlan& plan);

    /**
     * Add contingency steps to plan
     * @param plan The plan
     * @return Plan with contingency steps added
     */
    RefinedPlan addContingencySteps(const RefinedPlan& plan);

    /**
     * Estimate step duration
     * @param stepDescription The step description
     * @return Estimated duration in seconds
     */
    int estimateStepDuration(const QString& stepDescription);

    /**
     * Calculate plan success probability
     * @param plan The plan
     * @return Overall success probability (0.0 to 1.0)
     */
    float calculateSuccessProbability(const RefinedPlan& plan);

private:
    IterativePlanningEngine() = default;
    QMap<QString, float> m_stepDifficulties;
    QMap<QString, float> m_taskSuccessRates;
};

// ============================================================
// Autonomous Agent System
// ============================================================

class AutonomousAgent {
public:
    static AutonomousAgent& instance();

    AutonomousAgent() = default;

    /**
     * Initialize agent with configuration
     * @param agentId Unique identifier for this agent
     * @param maxRetries Maximum retry attempts for failed tasks
     */
    void initialize(const QString& agentId = "RawrXD-Agent-001", int maxRetries = 3);

    /**
     * Execute a task autonomously with self-correction
     * @param task The task to execute
     * @param constraints Any constraints on execution
     * @return true if task completed successfully
     */
    bool executeTaskAutonomously(const QString& task, const QStringList& constraints = QStringList());

    /**
     * Execute task with iterative refinement
     * @param task The task
     * @return Plan used and execution result
     */
    QString executeWithIterativeRefinement(const QString& task);

    /**
     * Attempt failed task with correction
     * @param task The task that failed
     * @param lastError The error that occurred
     * @return true if corrected attempt succeeded
     */
    bool attemptWithCorrection(const QString& task, const QString& lastError);

    /**
     * Monitor execution and provide feedback
     * @param executionId ID of execution to monitor
     * @return Status and any feedback
     */
    QString monitorExecution(const QString& executionId);

    /**
     * Get agent state
     * @return Current agent state
     */
    const AgentState& getState() const { return m_state; }

    /**
     * Get execution history
     * @return List of past executions
     */
    QVector<AgentMemoryEntry> getExecutionHistory() const { return m_state.executionHistory; }

    /**
     * Learn from successful execution
     * @param task The task that succeeded
     * @param duration How long it took
     * @param approach What approach was used
     */
    void learnFromSuccess(const QString& task, int duration, const QString& approach);

    /**
     * Learn from failed execution
     * @param task The task that failed
     * @param reason Why it failed
     * @param correction What correction was attempted
     */
    void learnFromFailure(const QString& task, const QString& reason, const QString& correction);

    /**
     * Get task success probability based on learning
     * @param task The task
     * @return Success probability (0.0 to 1.0)
     */
    float getTaskSuccessProbability(const QString& task);

    /**
     * Generate recommendations
     * @param task The task
     * @return List of recommendations
     */
    QStringList generateRecommendations(const QString& task);

private:
    AgentState m_state;
    QVector<RefinedPlan> m_activePlans;
    int m_executionIdCounter = 0;
};

// ============================================================
// Feedback and Learning System
// ============================================================

struct ExecutionFeedback {
    QString executionId;
    bool success = false;
    float quality = 0.5f;
    QStringList improvements;
    QString lessonsLearned;
    QDateTime timestamp;
};

class AgentLearningSystem {
public:
    static AgentLearningSystem& instance();

    /**
     * Record execution feedback
     * @param feedback The feedback to record
     */
    void recordFeedback(const ExecutionFeedback& feedback);

    /**
     * Analyze patterns in feedback
     * @param taskType The type of task
     * @return Pattern analysis
     */
    QString analyzePatterns(const QString& taskType);

    /**
     * Generate improvement suggestions
     * @param taskType The type of task
     * @return List of suggestions
     */
    QStringList generateImprovements(const QString& taskType);

    /**
     * Update task success model
     * @param task The task
     * @param success Whether it succeeded
     * @param duration How long it took
     */
    void updateTaskModel(const QString& task, bool success, int duration);

    /**
     * Get task difficulty estimate
     * @param task The task
     * @return Difficulty score (0.0 to 1.0)
     */
    float getTaskDifficulty(const QString& task);

    /**
     * Predict execution time
     * @param task The task
     * @return Predicted duration in seconds
     */
    int predictExecutionTime(const QString& task);

private:
    AgentLearningSystem() = default;
    QVector<ExecutionFeedback> m_feedbackHistory;
    QMap<QString, QVector<int>> m_taskDurations;
    QMap<QString, int> m_taskSuccessCounts;
    QMap<QString, int> m_taskAttemptCounts;
};

// ============================================================
// Agent Coordinator (Manages multiple agents)
// ============================================================

class AgentCoordinator {
public:
    static AgentCoordinator& instance();

    /**
     * Create a new agent
     * @param agentId ID for the new agent
     * @return true if agent was created
     */
    bool createAgent(const QString& agentId);

    /**
     * Assign task to agent
     * @param agentId The agent ID
     * @param task The task to assign
     * @return Execution ID
     */
    QString assignTask(const QString& agentId, const QString& task);

    /**
     * Get status of assigned task
     * @param executionId The execution ID
     * @return Status description
     */
    QString getTaskStatus(const QString& executionId);

    /**
     * Cancel task execution
     * @param executionId The execution ID
     * @return true if cancellation was successful
     */
    bool cancelTask(const QString& executionId);

    /**
     * Get all active agents
     * @return List of agent IDs
     */
    QStringList getActiveAgents() const { return m_agents.keys(); }

    /**
     * Get agent performance metrics
     * @param agentId The agent ID
     * @return Performance metrics as map
     */
    QMap<QString, float> getAgentMetrics(const QString& agentId);

    /**
     * Coordinate multiple agents for complex task
     * @param mainTask The main task
     * @param subTasks Sub-tasks to distribute
     * @return Overall execution status
     */
    QString coordinateMultiAgentTask(const QString& mainTask, const QStringList& subTasks);

private:
    AgentCoordinator() = default;
    QMap<QString, AutonomousAgent> m_agents;
    QMap<QString, QString> m_executionStatus;
    int m_globalExecutionCounter = 0;
};

}  // namespace RawrXD
