#pragma once

#include <QString>
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>
#include <vector>
#include <unordered_map>

class AgenticLoopState;
class AgenticIterativeReasoning;
class AgenticEngine;
class InferenceEngine;

/**
 * @class AgenticAgentCoordinator
 * @brief Multi-agent coordination for complex problem solving
 * 
 * STABLE API — frozen as of v1.0.0 (January 22, 2026)
 * Breaking changes require MAJOR version bump (v2.0.0+)
 * See FEATURE_FLAGS.md for API stability guarantees
 * 
 * Manages multiple reasoning agents working together:
 * - Task decomposition across agents
 * - Agent specialization (analyzer, planner, executor, verifier)
 * - State synchronization between agents
 * - Resource allocation
 * - Conflict resolution
 * - Consensus building
 * 
 * Enables parallel reasoning and task execution while maintaining
 * coherent state and preventing conflicts.
 */
class AgenticAgentCoordinator : public QObject
{
    Q_OBJECT

public:
    enum class AgentRole {
        Analyzer,      // Problem analysis and context gathering
        Planner,       // Strategy generation and planning
        Executor,      // Action execution
        Verifier,      // Result verification
        Optimizer,     // Performance optimization
        Learner        // Experience management
    };

    struct AgentInstance {
        QString agentId;
        AgentRole role;
        std::unique_ptr<AgenticIterativeReasoning> reasoner;
        std::unique_ptr<AgenticLoopState> state;
        QString currentTask;
        bool isAvailable;
        float utilization;
        int tasksCompleted;
        QString lastResult;
        QDateTime lastActive;
    };

    struct TaskAssignment {
        QString taskId;
        QString assignedAgentId;
        AgentRole requiredRole;
        QString description;
        QJsonObject parameters;
        QDateTime assignedTime;
        QDateTime completionTime;
        QString status;
        QJsonObject result;
        float estimatedComplexity;
    };

    struct AgentConflict {
        QString conflictId;
        QString agent1Id;
        QString agent2Id;
        QString conflictReason;
        QJsonObject state1;
        QJsonObject state2;
        QString resolution;
        QDateTime timestamp;
    };

public:
    explicit AgenticAgentCoordinator(QObject* parent = nullptr);
    ~AgenticAgentCoordinator();

    void initialize(AgenticEngine* engine, InferenceEngine* inference);

    // ===== AGENT MANAGEMENT =====
    QString createAgent(AgentRole role);
    void removeAgent(const QString& agentId);
    AgentInstance* getAgent(const QString& agentId);
    std::vector<AgentInstance*> getAvailableAgents(AgentRole role);
    QStringList getAllAgentIds() const;
    QJsonObject getAgentStatus(const QString& agentId);
    QJsonArray getAllAgentStatuses();

    // ===== TASK ASSIGNMENT AND EXECUTION =====
    QString assignTask(
        const QString& taskDescription,
        const QJsonObject& parameters,
        AgentRole requiredRole
    );
    bool executeAssignedTask(const QString& taskId);
    QString getTaskStatus(const QString& taskId);
    QJsonObject getTaskResult(const QString& taskId);
    QJsonArray getAllTaskStatuses();

    // ===== COORDINATION MECHANISMS =====
    
    // Decompose complex task across multiple agents
    QJsonArray decomposeLargeTask(const QString& goal);
    
    // Synchronize state across agents
    void synchronizeAgentStates();
    bool detectStateConflict(const QString& agentId1, const QString& agentId2);
    QString resolveStateConflict(const QString& agentId1, const QString& agentId2);
    
    // Consensus building
    bool buildConsensus(const QStringList& agentIds, const QString& question);
    QString getAgentOpinion(const QString& agentId, const QString& question);
    QString resolveDisagreement(const QStringList& agentIds);
    
    // Resource allocation
    void allocateResources(const QString& agentId, float cpuShare, float memoryShare);
    void rebalanceResources();

    // ===== MONITORING AND METRICS =====
    QJsonObject getCoordinationMetrics() const;
    float getTotalUtilization() const;
    int getTotalTasksCompleted() const;
    float getAverageTaskDuration() const;
    int getConflictCount() const { return m_conflicts.size(); }
    QJsonArray getConflictHistory();
    
    // ===== LOAD BALANCING =====
    QString selectBestAgentForTask(const QString& taskDescription);
    void rebalanceWorkload();

    // ===== STATE MANAGEMENT =====
    QJsonObject getGlobalState();
    bool restoreGlobalState(const QJsonObject& state);
    void saveCheckpoint();
    bool restoreFromCheckpoint();

signals:
    void agentCreated(const QString& agentId, const QString& role);
    void agentRemoved(const QString& agentId);
    void taskAssigned(const QString& taskId, const QString& agentId);
    void taskCompleted(const QString& taskId);
    void taskFailed(const QString& taskId, const QString& error);
    void stateConflictDetected(const QString& agentId1, const QString& agentId2);
    void stateConflictResolved(const QString& resolution);
    void agentStatusChanged(const QString& agentId, const QString& newStatus);
    void coordinationMetricsUpdated();

private:
    // Internal helpers
    void syncStateWithAgent(const QString& agentId);
    QString reconcileConflictingStates(
        const QJsonObject& state1,
        const QJsonObject& state2
    );
    QString selectResolutionStrategy(const QString& conflictReason);
    void recordConflict(
        const QString& agent1,
        const QString& agent2,
        const QString& reason,
        const QString& resolution
    );
    void updateAgentMetrics(const QString& agentId);

    // Agent pool
    std::unordered_map<std::string, std::unique_ptr<AgentInstance>> m_agents;
    std::unordered_map<std::string, std::unique_ptr<TaskAssignment>> m_assignments;
    std::vector<AgentConflict> m_conflicts;
    
    // Configuration
    AgenticEngine* m_engine = nullptr;
    InferenceEngine* m_inferenceEngine = nullptr;
    
    // Metrics
    QDateTime m_coordinationStartTime;
    int m_totalTasksAssigned = 0;
    int m_totalTasksCompleted = 0;
    int m_totalConflicts = 0;
    std::vector<std::pair<QString, int>> m_taskDurations;
    
    // Checkpointing
    QJsonObject m_lastCheckpoint;
    QDateTime m_lastCheckpointTime;
};
