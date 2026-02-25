#ifndef ENTERPRISE_WORKFLOW_ENGINE_HPP
#define ENTERPRISE_WORKFLOW_ENGINE_HPP

#include <QObject>
#include <QString>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QTimer>
#include <QQueue>
#include <QMap>

struct WorkflowStep {
    QString id;
    QString name;
    QVariantList tools;
    bool parallelizable;
    int timeoutMs;
    QString status; // pending, running, completed, failed
    QJsonObject results;
    QDateTime startedAt;
    QDateTime completedAt;
    QJsonObject metadata;
};

struct Workflow {
    QString id;
    QString name;
    QVector<WorkflowStep> steps;
    QString status; // pending, running, completed, failed
    QJsonObject parameters;
    QJsonObject results;
    QJsonObject metadata;
    QDateTime createdAt;
    QDateTime completedAt;
};

struct ToolResult {
    bool success;
    QString output;
    QString errorMessage;
    int exitCode;
    qint64 executionTimeMs;
    QJsonObject metadata;
};

class EnterpriseWorkflowEngine : public QObject {
    Q_OBJECT

public:
    explicit EnterpriseWorkflowEngine(QObject* parent = nullptr);
    ~EnterpriseWorkflowEngine();

    // Real autonomous workflow execution
    QString submitWorkflow(const QString& workflowDefinitionId, const QJsonObject& parameters);
    
    // Schedule workflow for later execution
    QString scheduleWorkflow(const QString& workflowDefinitionId, 
                           const QJsonObject& parameters,
                           const QDateTime& scheduledTime,
                           const QString& recurrence = "once");
    
    // Get workflow status
    Workflow getWorkflowStatus(const QString& workflowId);
    QJsonObject getWorkflowResults(const QString& workflowId);
    
    // List available workflow templates
    QJsonArray getAvailableWorkflowTemplates();
    
    // Advanced capabilities
    Workflow applyRealAIOptimization(const Workflow& originalWorkflow);
    
signals:
    void workflowStarted(const QString& workflowId);
    void workflowProgress(const QString& workflowId, int progressPercent, const QString& status);
    void workflowCompleted(const QString& workflowId, const QJsonObject& results);
    void workflowFailed(const QString& workflowId, const QString& error);
    void stepCompleted(const QString& workflowId, const QString& stepId, const QJsonObject& results);
    void optimizationApplied(const QString& workflowId, const QJsonObject& optimizations);

private:
    class Private;
    QScopedPointer<Private> d_ptr;

    // Real workflow execution methods
    QVector<Workflow> executeRealWorkflow(const QString& workflowId);
    QVector<Workflow> executeRealWorkflowSteps(const Workflow& workflow);
    Workflow executeRealWorkflowStep(const WorkflowStep& step, const QJsonObject& context);
    QJsonObject executeToolsWithAIDecision(const WorkflowStep& step, 
                                          const QJsonObject& aiDecision, 
                                          const QJsonObject& context);
    
    // Optimization methods
    Workflow applyRealParallelizationOptimization(const Workflow& workflow);
    Workflow applyRealResourceOptimization(const Workflow& workflow);
    Workflow applyRealAIStepReordering(const Workflow& workflow);
    Workflow applyRealQuantumSafeOptimization(const Workflow& workflow);
    
    // Helper methods
    QVector<QVector<int>> groupStepsForRealExecution(const Workflow& workflow);
    QVector<Workflow> aggregateRealResults(const QVector<Workflow>& stepResults);
    void recordRealWorkflowMetrics(const Workflow& workflow);
    QJsonObject toolResultToJson(const ToolResult& result);
    QJsonObject decisionToJson(const QJsonObject& decision);
    QMap<int, QSet<int>> buildRealDependencyGraph(const Workflow& workflow);
    QVector<QVector<int>> findRealParallelGroups(const QMap<int, QSet<int>>& graph);
    QVector<WorkflowStep> createCustomWorkflowSteps(const QJsonObject& parameters);
    QVariantList optimizationSuggestionsToVariantList();
};

#endif // ENTERPRISE_WORKFLOW_ENGINE_HPP
