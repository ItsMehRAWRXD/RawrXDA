#pragma once

// <QObject> removed (Qt-free build)
// <QString> removed (Qt-free build)
// <QJsonObject> removed (Qt-free build)
// <QProcess> removed (Qt-free build)
// <QTimer> removed (Qt-free build)
#include <vector>
#include <map>

class CIPipelineManager  {
    /* Q_OBJECT */

public:
    enum class PipelineStatus {
        Idle,
        Running,
        Success,
        Failed,
        Cancelled
    };

    explicit CIPipelineManager(QObject* parent = nullptr);
    ~CIPipelineManager();

    // Pipeline configuration
    QString createPipeline(const QString& name, const QJsonObject& config);
    bool updatePipeline(const QString& pipelineId, const QJsonObject& config);
    bool deletePipeline(const QString& pipelineId);

    // Pipeline execution
    bool startPipeline(const QString& pipelineId);
    bool stopPipeline(const QString& pipelineId);
    PipelineStatus getPipelineStatus(const QString& pipelineId) const;

    // Integration with version control
    void setVCSIntegration(const QString& vcsType, const QJsonObject& config);
    bool triggerOnCommit(const QString& pipelineId, const QString& commitHash);

    // Notifications and reporting
    void setNotificationSettings(const QJsonObject& settings);
    QJsonObject getPipelineReport(const QString& pipelineId) const;

signals:
    void pipelineStarted(const QString& pipelineId);
    void pipelineCompleted(const QString& pipelineId, bool success);
    void pipelineStatusChanged(const QString& pipelineId, PipelineStatus status);

private:
    struct Pipeline {
        QString id;
        QString name;
        QJsonObject config;
        PipelineStatus status;
        QProcess* process;
        QDateTime startTime;
        QDateTime endTime;
    };

    std::map<QString, Pipeline> m_pipelines;
    QJsonObject m_vcsConfig;
    QJsonObject m_notificationSettings;
};
