#pragma once

#include <QObject>
#include <QMap>
#include <QString>

#include "TaskNode.h"

using beacon::TaskNode;
using beacon::TaskStatus;

class GGUFRunner;

/**
 * @brief Central controller for autonomous task orchestration.
 */
class AgentOrchestrator : public QObject
{
    Q_OBJECT

public:
    explicit AgentOrchestrator(QObject* parent = nullptr);
    ~AgentOrchestrator() override;

    void setGoal(const QString& goalDescription);
    void startGoalExecution();
    void processTaskResult(const QString& taskId, bool success, const QString& resultLog);

    bool saveState(const QString& filePath = QStringLiteral("orchestrator_state.json")) const;
    bool loadState(const QString& filePath = QStringLiteral("orchestrator_state.json"));

    const QMap<QString, TaskNode>& getTaskGraph() const { return taskGraph_; }

signals:
    void taskGraphUpdated();
    void taskStatusUpdate(const QString& taskId, const QString& status);
    void executionError(const QString& message);
    void nextTaskProposed(const QString& taskDescription);

private:
    QString currentGoal_;
    QMap<QString, TaskNode> taskGraph_;
    bool isExecuting_{false};
    GGUFRunner* llmRunner_{nullptr};

    void executeNextTask();
    void generateNextPlan();
    QString findReadyTask() const;
    void setupInitialTaskGraph(const QString& goal);
};
