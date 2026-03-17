#pragma once

#include <QObject>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>

#include "backend/agentic_tools.h"

class MainWindow;
class QNetworkAccessManager;
class QNetworkReply;

namespace RawrXD {

struct TaskDefinition {
    QString id;
    QString description;
    QString type;
    int priority = 0;
    int estimatedTokens = 0;
    QString model;
    qint64 memoryLimit = 0;
    QString memoryStrategy;
    QHash<QString, QVariant> parameters;
};

struct OrchestrationResult {
    QString taskId;
    QString model;
    bool success = false;
    QString result;
    QString error;
    qint64 executionTime = 0;
};

class TaskOrchestrator : public QObject {
    Q_OBJECT

public:
    explicit TaskOrchestrator(::MainWindow* parent);
    ~TaskOrchestrator() override;

    RawrXD::Backend::ToolResult executeTool(const QString& toolName, const QJsonObject& params);

    void orchestrateTask(const QString& naturalLanguageDescription);
    QList<TaskDefinition> parseNaturalLanguage(const QString& description);
    QString determineTaskType(const QString& description);
    int estimateTokenCount(const QString& description);
    QString selectModelForTask(const TaskDefinition& task);
    void balanceWorkload(QList<TaskDefinition>& tasks);
    void executeTask(const TaskDefinition& task);
    void createExecutionTab(const TaskDefinition& task);
    void handleModelResponse(QNetworkReply* reply, const QString& taskId);
    QJsonObject createRollarCoasterRequest(const QString& model, const QString& prompt) const;
    QString generateTaskId() const;
    QList<QString> getAvailableModels() const;
    bool isModelAvailable(const QString& model) const;
    void setModelPreferences(const QHash<QString, int>& preferences);
    QList<TaskDefinition> getCurrentTasks() const;
    OrchestrationResult getTaskResult(const QString& taskId) const;
    void cancelTask(const QString& taskId);
    void setRollarCoasterEndpoint(const QString& endpoint);
    void setMaxParallelTasks(int maxTasks);
    void setTaskTimeout(int timeoutMs);

    void setMemoryProfile(const QString& profileName);
    void setGlobalMemoryLimit(qint64 limitBytes);
    void setTaskMemoryStrategy(const QString& strategy);
    qint64 getAvailableMemory() const;
    qint64 getTotalMemoryUsage() const;
    bool canAllocateMemory(qint64 requestedBytes) const;
    void allocateTaskMemory(const QString& taskId, qint64 bytes);
    void releaseTaskMemory(const QString& taskId);

    QList<TaskDefinition> decomposeComplexTask(const QString& description);
    QList<TaskDefinition> createMemoryAwareSubtasks(const TaskDefinition& mainTask);
    int calculateOptimalParallelism() const;
    bool shouldDecomposeFurther(const TaskDefinition& task) const;
    qint64 calculateMemoryForTask(const TaskDefinition& task) const;
    void applyMemoryConstraints(TaskDefinition& task);
    void balanceWorkloadWithMemory(QList<TaskDefinition>& tasks);
    bool canExecuteTask(const TaskDefinition& task) const;
    QStringList splitDescription(const QString& description) const;

signals:
    void taskSplitCompleted(const QList<TaskDefinition>& tasks);
    void modelSelectionCompleted(const QHash<QString, QString>& modelAssignments);
    void tabCreated(const QString& tabName, const QString& model);
    void taskStarted(const QString& taskId, const QString& model);
    void taskProgress(const QString& taskId, int progress);
    void taskCompleted(const OrchestrationResult& result);
    void orchestrationCompleted(const QList<OrchestrationResult>& results);
    void errorOccurred(const QString& errorMessage);

private:
    ::MainWindow* m_mainWindow = nullptr;
    QNetworkAccessManager* m_networkManager = nullptr;
    QString m_rollarCoasterEndpoint;
    int m_maxParallelTasks = 0;
    int m_taskTimeout = 0;

    QHash<QString, QStringList> m_modelCapabilities;
    QHash<QString, int> m_modelWorkloads;
    QHash<QString, int> m_modelPreferences;

    QHash<QString, QHash<QString, qint64>> m_memoryProfiles;
    QHash<QString, TaskDefinition> m_activeTasks;
    QHash<QString, OrchestrationResult> m_completedTasks;
    QHash<QString, qint64> m_taskMemoryUsage;

    QString m_memoryProfile;
    qint64 m_globalMemoryLimit = 0;
    QString m_defaultMemoryStrategy;
    qint64 m_totalMemoryAllocated = 0;

    RawrXD::Backend::AgenticToolExecutor m_toolExecutor;
};

} // namespace RawrXD
