#include "AgentOrchestrator.h"
#include "../StreamerClient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDebug>
#include <QDebug>

AgentOrchestrator::AgentOrchestrator(QObject* parent)
    : QObject(parent) {}

void AgentOrchestrator::setStreamer(StreamerClient* s) {
    streamer_ = s;
    if (streamer_) {
        QObject::connect(streamer_, &StreamerClient::chunkReceived, this, [this](const QString& chunk){
            if (!currentTaskId_.isEmpty()) {
                const auto node = taskGraph_.value(currentTaskId_);
                emit taskChunk(currentTaskId_, chunk, node.agent);
            }
        });
        // Wire taskCompleted to automatically update orchestrator state
        QObject::connect(streamer_, &StreamerClient::taskCompleted, this, 
            [this](bool success, const QString& taskId, const QString& /*model*/, const QString& /*output*/){
                handleTaskCompletion(taskId, success);
            });
    }
}

void AgentOrchestrator::startWorkflow(const QString& architectJsonOutput) {
    const QJsonDocument doc = QJsonDocument::fromJson(architectJsonOutput.toUtf8());
    startFromArchitect(doc);
}

void AgentOrchestrator::startFromArchitect(const QJsonDocument& architectDoc) {
    taskGraph_.clear();
    const QJsonObject obj = architectDoc.object();
    currentGoal_ = obj.value("goal").toString(); // Capture initial goal
    const QJsonArray tasks = obj.value("task_graph").toArray();
    if (tasks.isEmpty()) {
        qWarning() << "AgentOrchestrator: No task_graph found";
        emit orchestrationFinished(false);
        return;
    }
    for (const auto& tVal : tasks) {
        const QJsonObject t = tVal.toObject();
        TaskNode node;
        node.id = t.value("task_id").toString();
        node.agent = t.value("agent").toString();
        node.prompt = t.value("prompt").toString();
        for (const auto& d : t.value("dependencies").toArray()) node.deps << d.toString();
        node.status = QStringLiteral("Pending");
        taskGraph_.insert(node.id, node);
    }
    executeNextTasks();
}

void AgentOrchestrator::executeNextTasks() {
    bool anyLaunched = false;
    for (auto it = taskGraph_.begin(); it != taskGraph_.end(); ++it) {
        TaskNode& node = it.value();
        if (node.status == "Blocked" || node.status == "Failed" || node.status == "Running" || node.status.startsWith("Completed")) continue;
        if (node.status != "Pending") continue;
        bool ready = true;
        for (const auto& depId : node.deps) {
            const auto dep = taskGraph_.value(depId);
            if (dep.status == "Failed") {
                node.status = QStringLiteral("Blocked");
                emit taskStatusUpdated(node.id, QStringLiteral("Blocked 🛑"), node.agent);
                ready = false;
                break;
            }
            if (dep.status != "Completed/Success") { ready = false; break; }
        }
        if (!ready) continue;
        node.status = QStringLiteral("Running");
        emit taskStarted(node.id, node.agent);
        emit taskStatusUpdated(node.id, QStringLiteral("Running ⏳"), node.agent);
        currentTaskId_ = node.id;
        QString model = QStringLiteral("quantumide-feature");
        if (node.agent.compare("security", Qt::CaseInsensitive) == 0) model = QStringLiteral("quantumide-security");
        else if (node.agent.compare("performance", Qt::CaseInsensitive) == 0) model = QStringLiteral("quantumide-performance");
        if (streamer_) {
            streamer_->startGeneration(model, node.prompt, node.id);
            anyLaunched = true;
        }
    }
    bool allDone = true;
    for (const auto& n : taskGraph_) {
        if (!n.status.startsWith("Completed")) { allDone = false; break; }
    }
    if (!anyLaunched && allDone) emit orchestrationFinished(true);
}

void AgentOrchestrator::handleTaskCompletion(const QString& taskId, bool success) {
    if (!taskGraph_.contains(taskId)) return;
    TaskNode& node = taskGraph_[taskId];
    if (success) {
        node.status = QStringLiteral("Completed/Success");
        node.retryCount = 0;
        emit taskStatusUpdated(node.id, QStringLiteral("Completed ✅"), node.agent);
    } else {
        node.retryCount += 1;
        if (node.retryCount <= MAX_RETRIES) {
            node.status = QStringLiteral("Pending");
            emit taskStatusUpdated(node.id, QStringLiteral("Retrying (%1/%2)…").arg(node.retryCount).arg(MAX_RETRIES), node.agent);
        } else {
            node.status = QStringLiteral("Failed");
            emit taskStatusUpdated(node.id, QStringLiteral("Failed ❌"), node.agent);
        }
    }
    currentTaskId_.clear();
    const QString summary = QString("Agent %1 ran with prompt: %2").arg(node.agent, node.prompt);
    emit taskCompleted(node.id, node.agent, summary);
    executeNextTasks();
}

void AgentOrchestrator::setMaxRetries(int n) {
    if (n < 0) n = 0;
    MAX_RETRIES = n;
}

void AgentOrchestrator::retryBlockedTasks() {
    for (auto it = taskGraph_.begin(); it != taskGraph_.end(); ++it) {
        TaskNode& node = it.value();
        if (node.status == "Blocked") {
            node.status = QStringLiteral("Pending");
            emit taskStatusUpdated(node.id, QStringLiteral("Re-evaluating…"), node.agent);
        }
    }
    executeNextTasks();
}

bool AgentOrchestrator::saveOrchestrationState(const QString& filePath) {
    QJsonArray tasksArray;
    
    for (auto it = taskGraph_.constBegin(); it != taskGraph_.constEnd(); ++it) {
        const TaskNode& node = it.value();
        QJsonObject taskObj;
        taskObj["task_id"] = node.id;
        taskObj["agent"] = node.agent;
        taskObj["prompt"] = node.prompt;
        taskObj["status"] = node.status;
        taskObj["retry_count"] = node.retryCount;
        
        QJsonArray depsArray;
        for (const QString& dep : node.deps) {
            depsArray.append(dep);
        }
        taskObj["dependencies"] = depsArray;
        
        tasksArray.append(taskObj);
    }
    
    QJsonObject root;
    root["version"] = "1.0";
    root["goal"] = currentGoal_;
    root["current_task"] = currentTaskId_;
    root["max_retries"] = MAX_RETRIES;
    root["task_graph"] = tasksArray;
    
    QJsonDocument doc(root);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for writing:" << filePath;
        return false;
    }
    
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

bool AgentOrchestrator::loadOrchestrationState(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for reading:" << filePath;
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "JSON parse error:" << parseError.errorString();
        return false;
    }
    
    if (!doc.isObject()) {
        qWarning() << "Invalid JSON format: root is not an object";
        return false;
    }
    
    QJsonObject root = doc.object();
    QString version = root.value("version").toString();
    if (version != "1.0") {
        qWarning() << "Unsupported orchestration state version:" << version;
        return false;
    }
    
    taskGraph_.clear();
    currentGoal_ = root.value("goal").toString();
    currentTaskId_ = root.value("current_task").toString();
    MAX_RETRIES = root.value("max_retries").toInt(2); // Default to 2 if not present
    
    QJsonArray tasksArray = root.value("task_graph").toArray();
    for (const QJsonValue& taskVal : tasksArray) {
        QJsonObject taskObj = taskVal.toObject();
        TaskNode node;
        node.id = taskObj.value("task_id").toString();
        node.agent = taskObj.value("agent").toString();
        node.prompt = taskObj.value("prompt").toString();
        node.status = taskObj.value("status").toString();
        node.retryCount = taskObj.value("retry_count").toInt();
        
        QJsonArray depsArray = taskObj.value("dependencies").toArray();
        for (const QJsonValue& depVal : depsArray) {
            node.deps.append(depVal.toString());
        }
        
        taskGraph_.insert(node.id, node);
    }
    
    // Resume execution if there are pending/running tasks
    bool hasActiveTasks = false;
    for (const auto& node : taskGraph_) {
        if (node.status == "Pending" || node.status == "Running") {
            hasActiveTasks = true;
            break;
        }
    }
    
    if (hasActiveTasks) {
        executeNextTasks();
    }
    
    return true;
}
