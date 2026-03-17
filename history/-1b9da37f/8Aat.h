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
    void orchestrationFinished(bool success);
    void taskChunk(const QString& taskId, const QString& chunk, const QString& role);
    void taskStatusUpdated(const QString& taskId, const QString& status, const QString& detail);

public:
    // Legacy MainWindow compatibility methods
    void setStreamer(class StreamerClient* streamer) { (void)streamer; }
    void startWorkflow(const QString& goal) { setGoal(goal); startGoalExecution(); }
    void handleTaskCompletion(const QString& taskId, bool success) { processTaskResult(taskId, success, QString()); }
    void setMaxRetries(int retries) { (void)retries; }
    void retryBlockedTasks() {}
    bool saveOrchestrationState(const QString& path) { return saveState(path); }
    bool loadOrchestrationState(const QString& path) { return loadState(path); }

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
