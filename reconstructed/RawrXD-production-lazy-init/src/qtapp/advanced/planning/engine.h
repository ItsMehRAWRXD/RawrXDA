#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QQueue>
#include <memory>

/**
 * @class AdvancedPlanningEngine
 * @brief Implements recursive task decomposition and intelligent planning
 * 
 * Features:
 * - Recursive task decomposition with complexity analysis
 * - Dependency resolution and parallel execution planning
 * - Resource estimation and timeline projection
 * - Adaptive planning based on execution feedback
 * - Multi-goal optimization and conflict resolution
 */
class AdvancedPlanningEngine : public QObject {
    Q_OBJECT
public:
    explicit AdvancedPlanningEngine(QObject* parent = nullptr);
    virtual ~AdvancedPlanningEngine();

    // Core planning operations
    QJsonObject createMasterPlan(const QString& goal);
    QJsonObject decomposeTaskRecursive(const QString& task, int depth = 0);
    QJsonArray generateExecutionWorkflow(const QJsonObject& plan);
    QJsonObject optimizePlan(const QJsonObject& plan);
    
    // Planning analysis
    QJsonObject analyzeComplexity(const QString& task);
    QJsonArray detectDependencies(const QJsonObject& task);
    QJsonObject estimateResources(const QJsonObject& task);
    QString estimateTimeline(const QJsonObject& plan);
    
    // Adaptive planning
    void adaptPlanFromFeedback(const QString& taskId, const QJsonObject& feedback);
    QJsonObject refinePlan(const QJsonObject& originalPlan, const QJsonObject& executionResults);
    
    // Multi-goal planning
    QJsonObject planMultiGoal(const QJsonArray& goals);
    QJsonObject resolveGoalConflicts(const QJsonArray& plans);
    QString prioritizeGoals(const QJsonArray& goals);

    // System integration
    void initialize(class AgenticExecutor* executor, class InferenceEngine* inference);
    QJsonObject getPerformanceMetrics();
    void loadConfiguration(const QJsonObject& config);
    void executionCompleted(const QJsonObject& result);
    void bottleneckDetected(const QString& taskId);

public slots:
    void processPlanningRequest(const QString& goal);
    void updateExecutionStatus(const QString& taskId, const QString& status, const QJsonObject& metrics);

signals:
    void planCreated(const QJsonObject& plan);
    void taskDecomposed(const QString& parentTask, const QJsonArray& subtasks);
    void planOptimized(const QJsonObject& optimizedPlan);
    void executionStarted(const QString& planId);
    void executionProgress(const QString& taskId, int progress, const QString& status);
    void executionCompleted(const QString& planId, bool success, const QJsonObject& results);
    void planningError(const QString& error);

private:
    // Internal planning helpers
    QJsonObject parseGoalToTasks(const QString& goal);
    QJsonObject buildTaskGraph(const QJsonArray& tasks);
    QJsonObject calculateTaskPriorities(const QJsonObject& taskGraph);
    QJsonObject generateSubtasks(const QString& task, int depth);
    QJsonObject mergeMultiGoalPlans(const QJsonArray& plans);
    QJsonObject detectConflict(const QJsonObject& plan1, const QJsonObject& plan2);
    
    // Planning state
    struct PlanningSession {
        QString sessionId;
        QString originalGoal;
        QJsonObject masterPlan;
        QJsonObject taskGraph;
        QTimer* progressTimer;
        int completedTasks = 0;
        int totalTasks = 0;
    };
    
    QHash<QString, std::shared_ptr<PlanningSession>> m_activeSessions;
    QQueue<QJsonObject> m_pendingPlans;
    QJsonObject m_planningHistory;
    
    // Planning heuristics and configuration
    QJsonObject m_complexityWeights;
    QJsonObject m_resourceLimits;
    bool m_enableAdaptivePlanning = true;
    int m_maxRecursionDepth = 5;
};
