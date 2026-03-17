#pragma once
#include <QObject>
#include <QJsonDocument>
#include <QString>
#include <QMap>

class StreamerClient;

// Consolidated AgentOrchestrator (TaskNode-based)
class AgentOrchestrator : public QObject {
    Q_OBJECT
public:
    explicit AgentOrchestrator(QObject* parent = nullptr);

    void setStreamer(StreamerClient* s);
    void startFromArchitect(const QJsonDocument& architectDoc);
    // Wrapper to support existing callers
    void startWorkflow(const QString& architectJsonOutput);

    void handleTaskCompletion(const QString& taskId, bool success);
    void setMaxRetries(int n);
    void retryBlockedTasks();

    // Persistence API for saving/loading orchestration state
    bool saveOrchestrationState(const QString& filePath);
    bool loadOrchestrationState(const QString& filePath);

signals:
    void taskStarted(const QString& id, const QString& agent);
    void taskCompleted(const QString& id, const QString& agent, const QString& outputSummary);
    void orchestrationFinished(bool success);
    void taskChunk(const QString& id, const QString& chunk, const QString& agentType);
    void taskStatusUpdated(const QString& taskId, const QString& status, const QString& agentType);

private:
    StreamerClient* streamer_ {nullptr};
    QString currentTaskId_{};
    QString currentGoal_{}; // Store initial user goal for persistence
    struct TaskNode {
        QString id;
        QString agent; // feature/security/performance
        QString prompt;
        QStringList deps;
        QString status; // Pending|Ready|Running|Completed/Success|Completed/Failure|Blocked|Failed
        int retryCount{0};
    };
    QMap<QString, TaskNode> taskGraph_;

    void executeNextTasks();
    int MAX_RETRIES = 2;
};
