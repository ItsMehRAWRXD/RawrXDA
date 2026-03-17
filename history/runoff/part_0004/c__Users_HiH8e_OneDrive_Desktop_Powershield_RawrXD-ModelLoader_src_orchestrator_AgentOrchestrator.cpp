#include "AgentOrchestrator.h"

#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include <vector>

#include "llm_adapter/GGUFRunner.h"

namespace {
constexpr auto kLogitBufferSize = 32000;
}

AgentOrchestrator::AgentOrchestrator(QObject* parent)
    : QObject(parent)
{
    llmRunner_ = new GGUFRunner(this);

    connect(llmRunner_, &GGUFRunner::tokenChunkGenerated, this, [](const QString& chunk) {
        qDebug() << "[GGUFRunner]" << chunk;
    });

    connect(llmRunner_, &GGUFRunner::inferenceComplete, this, [this](bool success) {
        if (!success) {
            emit executionError(QStringLiteral("LLM inference failed. See runner logs."));
            isExecuting_ = false;
            return;
        }

        if (!taskGraph_.isEmpty()) {
            const QString firstId = taskGraph_.firstKey();
            TaskNode& planNode = taskGraph_[firstId];
            if (planNode.status == TaskStatus::PENDING || planNode.status == TaskStatus::IN_PROGRESS) {
                planNode.status = TaskStatus::COMPLETED;
                planNode.log.append(QStringLiteral("\nPlan finalized by LLM."));
            }

            if (taskGraph_.size() == 1) {
                TaskNode implNode(QStringLiteral("T1_IMPLEMENT_AUTH: Implement the JWT token generation class."), { firstId });
                taskGraph_.insert(implNode.id, implNode);
            }
        }

        emit taskGraphUpdated();
        executeNextTask();
    });

    qDebug() << "AgentOrchestrator initialized.";
}

AgentOrchestrator::~AgentOrchestrator() = default;

// Legacy MainWindow compatibility method implementations
void AgentOrchestrator::setStreamer(StreamerClient* streamer) {
    (void)streamer;
}

void AgentOrchestrator::startWorkflow(const QString& goal) {
    setGoal(goal);
    startGoalExecution();
}

void AgentOrchestrator::handleTaskCompletion(const QString& taskId, bool success) {
    processTaskResult(taskId, success, QString());
}

void AgentOrchestrator::setMaxRetries(int retries) {
    (void)retries;
}

void AgentOrchestrator::retryBlockedTasks() {
    // Stub for now
}

bool AgentOrchestrator::saveOrchestrationState(const QString& path) {
    return saveState(path);
}

bool AgentOrchestrator::loadOrchestrationState(const QString& path) {
    return loadState(path);
}

void AgentOrchestrator::setGoal(const QString& goalDescription)
{
    currentGoal_ = goalDescription;
    taskGraph_.clear();
    setupInitialTaskGraph(goalDescription);
    emit taskGraphUpdated();
}

void AgentOrchestrator::startGoalExecution()
{
    if (isExecuting_) {
        qWarning() << "Execution already in progress.";
        return;
    }

    if (taskGraph_.isEmpty()) {
        qWarning() << "No tasks available to execute.";
        return;
    }

    isExecuting_ = true;
    qDebug() << "Starting execution for goal:" << currentGoal_;
    executeNextTask();
}

void AgentOrchestrator::processTaskResult(const QString& taskId, bool success, const QString& resultLog)
{
    if (!taskGraph_.contains(taskId)) {
        qWarning() << "Unknown task ID in processTaskResult:" << taskId;
        return;
    }

    TaskNode& node = taskGraph_[taskId];
    node.log.append(QStringLiteral("\n--- TOOL RESULT ---\n"));
    node.log.append(resultLog);

    if (success) {
        node.status = TaskStatus::COMPLETED;
    } else {
        node.status = TaskStatus::FAILED;
        emit executionError(QStringLiteral("Task %1 failed. Aborting loop.").arg(taskId));
        isExecuting_ = false;
    }

    emit taskStatusUpdate(taskId, node.statusToString());
    emit taskGraphUpdated();

    if (isExecuting_) {
        executeNextTask();
    }
}

bool AgentOrchestrator::saveState(const QString& filePath) const
{
    QJsonObject root;
    root[QStringLiteral("version")] = QStringLiteral("1.0");
    root[QStringLiteral("goal")] = currentGoal_;
    root[QStringLiteral("is_executing")] = isExecuting_;

    QJsonArray tasks;
    for (const TaskNode& node : taskGraph_) {
        tasks.append(node.toJson());
    }
    root[QStringLiteral("task_graph")] = tasks;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qCritical() << "Unable to open state file for writing:" << file.errorString();
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    qDebug() << "Saved orchestrator state to" << filePath;
    return true;
}

bool AgentOrchestrator::loadState(const QString& filePath)
{
    QFile file(filePath);
    if (!file.exists()) {
        return false;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qCritical() << "Unable to open state file for reading:" << file.errorString();
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        qCritical() << "State file is not a JSON object.";
        return false;
    }

    const QJsonObject root = doc.object();
    taskGraph_.clear();
    currentGoal_ = root.value(QStringLiteral("goal")).toString();
    isExecuting_ = root.value(QStringLiteral("is_executing")).toBool();

    const QJsonArray tasks = root.value(QStringLiteral("task_graph")).toArray();
    for (const QJsonValue& value : tasks) {
        TaskNode node = TaskNode::fromJson(value.toObject());
        taskGraph_.insert(node.id, node);
    }

    qDebug() << "Loaded orchestrator state from" << filePath << "Task count:" << taskGraph_.size();
    emit taskGraphUpdated();
    return true;
}

void AgentOrchestrator::executeNextTask()
{
    if (!isExecuting_) {
        return;
    }

    const QString nextTaskId = findReadyTask();
    if (!nextTaskId.isEmpty()) {
        TaskNode& node = taskGraph_[nextTaskId];
        node.status = TaskStatus::IN_PROGRESS;
        emit taskStatusUpdate(nextTaskId, node.statusToString());

        qDebug() << "Executing task:" << node.description;
        emit nextTaskProposed(node.description);
        return;
    }

    bool allCompleted = true;
    for (const TaskNode& node : taskGraph_) {
        if (node.status != TaskStatus::COMPLETED && node.status != TaskStatus::SKIPPED) {
            allCompleted = false;
            break;
        }
    }

    if (allCompleted && !taskGraph_.isEmpty()) {
        qDebug() << "All tasks completed. Goal achieved.";
        isExecuting_ = false;
        return;
    }

    qDebug() << "No ready tasks; requesting plan refinement.";
    generateNextPlan();
}

void AgentOrchestrator::generateNextPlan()
{
    QJsonObject root;
    root[QStringLiteral("goal")] = currentGoal_;

    QJsonArray tasks;
    for (const TaskNode& node : taskGraph_) {
        tasks.append(node.toJson());
    }
    root[QStringLiteral("graph")] = QString::fromUtf8(QJsonDocument(tasks).toJson(QJsonDocument::Compact));

    const QString prompt = QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));

    std::vector<float> logits(static_cast<size_t>(kLogitBufferSize));
    const bool ok = llmRunner_->runInference(prompt, logits.data());
    if (!ok) {
        emit executionError(QStringLiteral("GGUF runner rejected prompt."));
        isExecuting_ = false;
    }
}

QString AgentOrchestrator::findReadyTask() const
{
    for (auto it = taskGraph_.cbegin(); it != taskGraph_.cend(); ++it) {
        const TaskNode& node = it.value();
        if (node.status != TaskStatus::PENDING) {
            continue;
        }
        bool ready = true;
        for (const QString& dependency : node.dependencies) {
            const auto depIt = taskGraph_.constFind(dependency);
            if (depIt == taskGraph_.cend() || depIt.value().status != TaskStatus::COMPLETED) {
                ready = false;
                break;
            }
        }
        if (ready) {
            return it.key();
        }
    }
    return QString();
}

void AgentOrchestrator::setupInitialTaskGraph(const QString& goal)
{
    TaskNode planNode(QStringLiteral("T0_PLAN: Devise execution steps for goal '%1'.").arg(goal), {});
    taskGraph_.insert(planNode.id, planNode);
    qDebug() << "Initial planning task inserted with id" << planNode.id;
}
