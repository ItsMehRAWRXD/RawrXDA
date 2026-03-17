#include "ci_cd_settings.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QDebug>
#include <QDateTime>
#include <QUuid>
#include <QNetworkRequest>
#include <algorithm>

CICDSettings::CICDSettings(QObject* parent)
    : QObject(parent)
{
    // Initialize default notification config
    m_notificationConfig.enableSlack = false;
    m_notificationConfig.enableEmail = false;
    m_notificationConfig.notifyOnSuccess = true;
    m_notificationConfig.notifyOnFailure = true;
    m_notificationConfig.notifyOnStart = false;
}

CICDSettings::~CICDSettings()
{
}

bool CICDSettings::createJob(const TrainingJob& job)
{
    QString jobId = job.jobId.isEmpty() ? generateJobId() : job.jobId;
    TrainingJob newJob = job;
    newJob.jobId = jobId;
    newJob.createdAt = QDateTime::currentSecsSinceEpoch();
    newJob.lastRunAt = 0;
    newJob.successCount = 0;
    newJob.failureCount = 0;

    m_jobs[jobId] = newJob;
    qDebug() << "Created job:" << jobId;
    return true;
}

bool CICDSettings::updateJob(const QString& jobId, const TrainingJob& job)
{
    auto it = m_jobs.find(jobId);
    if (it == m_jobs.end()) {
        return false;
    }

    TrainingJob updated = job;
    updated.jobId = jobId;
    updated.createdAt = it->second.createdAt;
    updated.successCount = it->second.successCount;
    updated.failureCount = it->second.failureCount;

    m_jobs[jobId] = updated;
    return true;
}

CICDSettings::TrainingJob CICDSettings::getJob(const QString& jobId) const
{
    auto it = m_jobs.find(jobId);
    if (it == m_jobs.end()) {
        return TrainingJob();
    }
    return it->second;
}

std::vector<CICDSettings::TrainingJob> CICDSettings::listJobs() const
{
    std::vector<TrainingJob> result;
    for (const auto& [jobId, job] : m_jobs) {
        result.push_back(job);
    }
    return result;
}

bool CICDSettings::deleteJob(const QString& jobId)
{
    return m_jobs.erase(jobId) > 0;
}

bool CICDSettings::setJobEnabled(const QString& jobId, bool enabled)
{
    auto it = m_jobs.find(jobId);
    if (it == m_jobs.end()) {
        return false;
    }

    it->second.enabled = enabled;
    return true;
}

QString CICDSettings::queueJob(const QString& jobId)
{
    auto it = m_jobs.find(jobId);
    if (it == m_jobs.end() || !it->second.enabled) {
        return "";
    }

    QString runId = generateRunId();
    JobRunLog log;
    log.jobId = jobId;
    log.runId = runId;
    log.status = JobStatus::Queued;
    log.startTime = QDateTime::currentSecsSinceEpoch();
    log.endTime = 0;

    m_runLogs[runId] = log;
    it->second.lastRunAt = log.startTime;

    emit jobQueued(jobId, runId);
    qDebug() << "Queued job:" << jobId << "Run:" << runId;
    return runId;
}

bool CICDSettings::cancelJob(const QString& runId)
{
    auto it = m_runLogs.find(runId);
    if (it == m_runLogs.end()) {
        return false;
    }

    if (it->second.status == JobStatus::Running || it->second.status == JobStatus::Queued) {
        it->second.status = JobStatus::Cancelled;
        it->second.endTime = QDateTime::currentSecsSinceEpoch();
        return true;
    }

    return false;
}

QString CICDSettings::retryJob(const QString& runId)
{
    auto it = m_runLogs.find(runId);
    if (it == m_runLogs.end()) {
        return "";
    }

    return queueJob(it->second.jobId);
}

CICDSettings::JobRunLog CICDSettings::getJobRunLog(const QString& runId) const
{
    auto it = m_runLogs.find(runId);
    if (it == m_runLogs.end()) {
        return JobRunLog();
    }
    return it->second;
}

std::vector<CICDSettings::JobRunLog> CICDSettings::getJobRunHistory(const QString& jobId, int limit) const
{
    std::vector<JobRunLog> result;

    for (const auto& [runId, log] : m_runLogs) {
        if (log.jobId == jobId) {
            result.push_back(log);
        }
    }

    // Sort by start time descending
    std::sort(result.begin(), result.end(),
             [](const JobRunLog& a, const JobRunLog& b) {
                 return a.startTime > b.startTime;
             });

    if (result.size() > static_cast<size_t>(limit)) {
        result.resize(limit);
    }

    return result;
}

std::tuple<int, int, int, float> CICDSettings::getJobStatistics(const QString& jobId) const
{
    int totalRuns = 0;
    int successCount = 0;
    int failureCount = 0;
    float totalDuration = 0.0f;

    for (const auto& [runId, log] : m_runLogs) {
        if (log.jobId == jobId) {
            totalRuns++;
            if (log.status == JobStatus::Completed) {
                successCount++;
            } else if (log.status == JobStatus::Failed) {
                failureCount++;
            }
            if (log.endTime > 0) {
                totalDuration += (log.endTime - log.startTime);
            }
        }
    }

    float avgDuration = totalRuns > 0 ? totalDuration / totalRuns : 0.0f;
    return std::make_tuple(totalRuns, successCount, failureCount, avgDuration);
}

bool CICDSettings::definePipeline(const QString& jobId, const std::vector<PipelineStage>& stages)
{
    m_pipelines[jobId] = stages;
    qDebug() << "Defined pipeline for job:" << jobId << "with" << stages.size() << "stages";
    return true;
}

std::vector<CICDSettings::PipelineStage> CICDSettings::getPipeline(const QString& jobId) const
{
    auto it = m_pipelines.find(jobId);
    if (it == m_pipelines.end()) {
        return std::vector<PipelineStage>();
    }
    return it->second;
}

bool CICDSettings::setDeploymentConfig(const QString& jobId, const DeploymentConfig& config)
{
    m_deploymentConfigs[jobId] = config;
    return true;
}

CICDSettings::DeploymentConfig CICDSettings::getDeploymentConfig(const QString& jobId) const
{
    auto it = m_deploymentConfigs.find(jobId);
    if (it == m_deploymentConfigs.end()) {
        return DeploymentConfig();
    }
    return it->second;
}

QString CICDSettings::deployModel(const QString& jobId, const QString& runId)
{
    auto runIt = m_runLogs.find(runId);
    if (runIt == m_runLogs.end() || runIt->second.jobId != jobId) {
        return "";
    }

    if (runIt->second.status != JobStatus::Completed) {
        return "";
    }

    QString deploymentId = generateDeploymentId();
    emit deploymentStarted(deploymentId);

    // Simulate deployment
    emit deploymentCompleted(deploymentId, true);
    qDebug() << "Deployed model from job:" << jobId << "run:" << runId;

    return deploymentId;
}

bool CICDSettings::rollbackDeployment(const QString& deploymentId, const QString& targetRunId)
{
    emit deploymentRolledBack(deploymentId);
    qDebug() << "Rolled back deployment:" << deploymentId << "to run:" << targetRunId;
    return true;
}

QString CICDSettings::registerWebhook(const QString& jobId, const QString& platform,
                                      const QString& repository, const QString& branch)
{
    // Generate webhook URL
    QString webhookUrl = QString("https://agentic-ide.local/webhooks/%1/%2/%3/%4")
                            .arg(jobId, platform, repository, branch);

    qDebug() << "Registered webhook for job:" << jobId << "URL:" << webhookUrl;
    return webhookUrl;
}

QString CICDSettings::handleWebhook(const QJsonObject& webhookData)
{
    QString action = webhookData["action"].toString();
    QString repository = webhookData["repository"].toString();

    emit webhookReceived("github", action);
    qDebug() << "Received webhook from" << repository << "action:" << action;

    // In production: match webhook to job and queue it
    // For now: return empty
    return "";
}

bool CICDSettings::setNotificationConfig(const NotificationConfig& config)
{
    m_notificationConfig = config;
    return true;
}

CICDSettings::NotificationConfig CICDSettings::getNotificationConfig() const
{
    return m_notificationConfig;
}

bool CICDSettings::sendTestNotification() const
{
    if (m_notificationConfig.enableSlack) {
        qDebug() << "Sending test Slack notification to:" << m_notificationConfig.slackChannel;
    }

    if (m_notificationConfig.enableEmail) {
        qDebug() << "Sending test email to:" << m_notificationConfig.emailRecipients.size() << "recipients";
    }

    return true;
}

bool CICDSettings::storeArtifact(const QString& artifactId, const QString& artifactPath,
                               const QJsonObject& metadata)
{
    QJsonObject artifact = metadata;
    artifact["path"] = artifactPath;
    artifact["storedAt"] = static_cast<qint64>(QDateTime::currentSecsSinceEpoch());

    m_artifacts[artifactId] = artifact;
    qDebug() << "Stored artifact:" << artifactId << "path:" << artifactPath;
    return true;
}

QString CICDSettings::getArtifact(const QString& artifactId) const
{
    auto it = m_artifacts.find(artifactId);
    if (it == m_artifacts.end()) {
        return "";
    }
    return it->second["path"].toString();
}

std::vector<QString> CICDSettings::listArtifacts(const QString& jobId) const
{
    std::vector<QString> result;

    for (const auto& [artifactId, metadata] : m_artifacts) {
        if (metadata["jobId"].toString() == jobId) {
            result.push_back(artifactId);
        }
    }

    return result;
}

int CICDSettings::cleanupOldArtifacts(int olderThanDays)
{
    qint64 threshold = QDateTime::currentSecsSinceEpoch() - (olderThanDays * 86400);
    int deleted = 0;

    for (auto it = m_artifacts.begin(); it != m_artifacts.end(); ) {
        qint64 storedAt = it->second["storedAt"].toVariant().toLongLong();
        if (storedAt < threshold) {
            it = m_artifacts.erase(it);
            deleted++;
        } else {
            ++it;
        }
    }

    qDebug() << "Cleaned up" << deleted << "old artifacts";
    return deleted;
}

QJsonObject CICDSettings::exportConfiguration() const
{
    QJsonObject config;

    // Export jobs
    QJsonArray jobsArray;
    for (const auto& [jobId, job] : m_jobs) {
        QJsonObject jobObj;
        jobObj["jobId"] = job.jobId;
        jobObj["jobName"] = job.jobName;
        jobObj["modelName"] = job.modelName;
        jobObj["epochs"] = job.epochs;
        jobObj["batchSize"] = job.batchSize;
        jobObj["learningRate"] = job.learningRate;
        jobObj["enabled"] = job.enabled;
        jobsArray.append(jobObj);
    }
    config["jobs"] = jobsArray;

    // Export deployment configs
    QJsonArray deploymentArray;
    for (const auto& [jobId, deployConfig] : m_deploymentConfigs) {
        QJsonObject deployObj;
        deployObj["jobId"] = jobId;
        deployObj["targetEnvironment"] = deployConfig.targetEnvironment;
        deployObj["strategy"] = static_cast<int>(deployConfig.strategy);
        deployObj["requireApproval"] = deployConfig.requireApproval;
        deploymentArray.append(deployObj);
    }
    config["deployments"] = deploymentArray;

    // Export notification config
    QJsonObject notifObj;
    notifObj["enableSlack"] = m_notificationConfig.enableSlack;
    notifObj["enableEmail"] = m_notificationConfig.enableEmail;
    notifObj["notifyOnSuccess"] = m_notificationConfig.notifyOnSuccess;
    notifObj["notifyOnFailure"] = m_notificationConfig.notifyOnFailure;
    config["notifications"] = notifObj;

    return config;
}

bool CICDSettings::importConfiguration(const QJsonObject& config)
{
    // Import jobs
    QJsonArray jobsArray = config["jobs"].toArray();
    for (const QJsonValue& val : jobsArray) {
        QJsonObject jobObj = val.toObject();
        TrainingJob job;
        job.jobId = jobObj["jobId"].toString();
        job.jobName = jobObj["jobName"].toString();
        job.modelName = jobObj["modelName"].toString();
        job.epochs = jobObj["epochs"].toInt();
        job.batchSize = jobObj["batchSize"].toInt();
        job.learningRate = jobObj["learningRate"].toDouble();
        job.enabled = jobObj["enabled"].toBool();
        createJob(job);
    }

    // Import deployment configs
    QJsonArray deploymentArray = config["deployments"].toArray();
    for (const QJsonValue& val : deploymentArray) {
        QJsonObject deployObj = val.toObject();
        DeploymentConfig deployConfig;
        deployConfig.targetEnvironment = deployObj["targetEnvironment"].toString();
        deployConfig.strategy = static_cast<DeploymentStrategy>(deployObj["strategy"].toInt());
        deployConfig.requireApproval = deployObj["requireApproval"].toBool();
        setDeploymentConfig(deployObj["jobId"].toString(), deployConfig);
    }

    return true;
}

bool CICDSettings::saveToFile(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open file:" << filePath;
        return false;
    }

    QJsonObject config = exportConfiguration();
    file.write(QJsonDocument(config).toJson(QJsonDocument::Indented));
    file.close();

    qDebug() << "Saved CI/CD configuration to:" << filePath;
    return true;
}

bool CICDSettings::loadFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open file:" << filePath;
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) {
        return false;
    }

    return importConfiguration(doc.object());
}

std::map<QString, QJsonObject> CICDSettings::getPipelineTemplates() const
{
    std::map<QString, QJsonObject> templates;

    // Basic training template
    QJsonObject basicTemplate;
    basicTemplate["name"] = "Basic Training";
    basicTemplate["description"] = "Simple training job";
    QJsonArray stagesArray;
    QJsonObject stage1;
    stage1["name"] = "train";
    stage1["timeout"] = 3600;
    stagesArray.append(stage1);
    basicTemplate["stages"] = stagesArray;
    templates["basic"] = basicTemplate;

    // Production template
    QJsonObject prodTemplate;
    prodTemplate["name"] = "Production Training";
    prodTemplate["description"] = "Full production pipeline with validation and deployment";
    QJsonArray prodStages;
    
    QJsonObject trainStage;
    trainStage["name"] = "train";
    trainStage["timeout"] = 7200;
    prodStages.append(trainStage);
    
    QJsonObject evalStage;
    evalStage["name"] = "evaluate";
    evalStage["timeout"] = 1800;
    prodStages.append(evalStage);
    
    QJsonObject deployStage;
    deployStage["name"] = "deploy";
    deployStage["timeout"] = 600;
    prodStages.append(deployStage);
    
    prodTemplate["stages"] = prodStages;
    templates["production"] = prodTemplate;

    return templates;
}

QString CICDSettings::generateJobId()
{
    return "job_" + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
}

QString CICDSettings::generateRunId()
{
    return "run_" + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
}

QString CICDSettings::generateDeploymentId()
{
    return "deploy_" + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
}
