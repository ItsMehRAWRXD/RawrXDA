#include "ci_cd_settings.h"
#include <QDebug>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QStandardPaths>
#include <QDir>
#include <algorithm>
#include <random>

CICDSettings::CICDSettings(QObject* parent)
    : QObject(parent)
{
    qDebug() << "[CICDSettings] Initializing CI/CD settings module";
}

CICDSettings::~CICDSettings()
{
    qDebug() << "[CICDSettings] Destroying CI/CD settings module";
}

// ===== Job Management =====

bool CICDSettings::createJob(const TrainingJob& job)
{
    qDebug() << "[CICDSettings] Creating training job:" << job.jobName;
    try {
        m_jobs[job.jobId] = job;
        return true;
    } catch (const std::exception& e) {
        qCritical() << "[CICDSettings] Error creating job:" << e.what();
        return false;
    }
}

bool CICDSettings::updateJob(const QString& jobId, const TrainingJob& job)
{
    auto it = m_jobs.find(jobId);
    if (it == m_jobs.end()) return false;
    
    m_jobs[jobId] = job;
    return true;
}

CICDSettings::TrainingJob CICDSettings::getJob(const QString& jobId) const
{
    auto it = m_jobs.find(jobId);
    if (it == m_jobs.end()) return TrainingJob();
    return it->second;
}

std::vector<CICDSettings::TrainingJob> CICDSettings::listJobs() const
{
    std::vector<TrainingJob> result;
    for (const auto& pair : m_jobs) {
        result.push_back(pair.second);
    }
    return result;
}

bool CICDSettings::deleteJob(const QString& jobId)
{
    auto it = m_jobs.find(jobId);
    if (it == m_jobs.end()) return false;
    
    m_jobs.erase(it);
    return true;
}

bool CICDSettings::setJobEnabled(const QString& jobId, bool enabled)
{
    auto it = m_jobs.find(jobId);
    if (it == m_jobs.end()) return false;
    
    it->second.enabled = enabled;
    return true;
}

// ===== Job Execution =====

QString CICDSettings::queueJob(const QString& jobId)
{
    qDebug() << "[CICDSettings] Queuing job:" << jobId;
    
    auto it = m_jobs.find(jobId);
    if (it == m_jobs.end()) return "";
    
    // Generate run ID
    QString runId = QString("run_%1_%2").arg(
        QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")
    ).arg(rand() % 10000);
    
    JobRunLog log;
    log.jobId = jobId;
    log.runId = runId;
    log.status = JobStatus::Queued;
    log.startTime = QDateTime::currentDateTime().toMSecsSinceEpoch();
    
    m_runLogs[runId] = log;
    
    emit jobQueued(jobId, runId);
    return runId;
}

bool CICDSettings::cancelJob(const QString& runId)
{
    qDebug() << "[CICDSettings] Cancelling job run:" << runId;
    
    auto it = m_runLogs.find(runId);
    if (it == m_runLogs.end()) return false;
    
    it->second.status = JobStatus::Cancelled;
    it->second.endTime = QDateTime::currentDateTime().toMSecsSinceEpoch();
    
    return true;
}

QString CICDSettings::retryJob(const QString& runId)
{
    qDebug() << "[CICDSettings] Retrying job run:" << runId;
    
    auto it = m_runLogs.find(runId);
    if (it == m_runLogs.end()) return "";
    
    // Generate new run ID
    QString newRunId = QString("run_%1_%2").arg(
        QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")
    ).arg(rand() % 10000);
    
    JobRunLog newLog = it->second;
    newLog.runId = newRunId;
    newLog.status = JobStatus::Queued;
    newLog.startTime = QDateTime::currentDateTime().toMSecsSinceEpoch();
    newLog.endTime = 0;
    newLog.logOutput = "";
    newLog.errorMessage = "";
    
    m_runLogs[newRunId] = newLog;
    
    return newRunId;
}

CICDSettings::JobRunLog CICDSettings::getJobRunLog(const QString& runId) const
{
    auto it = m_runLogs.find(runId);
    if (it == m_runLogs.end()) return JobRunLog();
    return it->second;
}

std::vector<CICDSettings::JobRunLog> CICDSettings::getJobRunHistory(const QString& jobId, int limit) const
{
    std::vector<JobRunLog> result;
    int count = 0;
    
    for (auto it = m_runLogs.rbegin(); it != m_runLogs.rend() && count < limit; ++it) {
        if (it->second.jobId == jobId) {
            result.push_back(it->second);
            count++;
        }
    }
    
    return result;
}

std::tuple<int, int, int, float> CICDSettings::getJobStatistics(const QString& jobId) const
{
    int totalRuns = 0;
    int successCount = 0;
    int failureCount = 0;
    float avgDuration = 0.0f;
    
    float totalDuration = 0.0f;
    
    for (const auto& pair : m_runLogs) {
        if (pair.second.jobId == jobId) {
            totalRuns++;
            if (pair.second.status == JobStatus::Completed) {
                successCount++;
            } else if (pair.second.status == JobStatus::Failed) {
                failureCount++;
            }
            
            if (pair.second.endTime > 0) {
                float duration = (pair.second.endTime - pair.second.startTime) / 1000.0f;
                totalDuration += duration;
            }
        }
    }
    
    if (totalRuns > 0) {
        avgDuration = totalDuration / totalRuns;
    }
    
    return std::make_tuple(totalRuns, successCount, failureCount, avgDuration);
}

// ===== Pipeline Configuration =====

bool CICDSettings::definePipeline(const QString& jobId, const std::vector<PipelineStage>& stages)
{
    qDebug() << "[CICDSettings] Defining pipeline for job:" << jobId << "with" << stages.size() << "stages";
    
    m_pipelines[jobId] = stages;
    return true;
}

std::vector<CICDSettings::PipelineStage> CICDSettings::getPipeline(const QString& jobId) const
{
    auto it = m_pipelines.find(jobId);
    if (it == m_pipelines.end()) return std::vector<PipelineStage>();
    return it->second;
}

// ===== Deployment Configuration =====

bool CICDSettings::setDeploymentConfig(const QString& jobId, const DeploymentConfig& config)
{
    qDebug() << "[CICDSettings] Setting deployment config for job:" << jobId;
    
    m_deploymentConfigs[jobId] = config;
    return true;
}

CICDSettings::DeploymentConfig CICDSettings::getDeploymentConfig(const QString& jobId) const
{
    auto it = m_deploymentConfigs.find(jobId);
    if (it == m_deploymentConfigs.end()) return DeploymentConfig();
    return it->second;
}

QString CICDSettings::deployModel(const QString& jobId, const QString& runId)
{
    qDebug() << "[CICDSettings] Deploying model for job:" << jobId << "run:" << runId;
    
    // Generate deployment ID
    QString deploymentId = QString("deploy_%1_%2").arg(
        QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")
    ).arg(rand() % 10000);
    
    emit deploymentStarted(deploymentId);
    
    // Simulate deployment (in production, actually deploy)
    emit deploymentCompleted(deploymentId, true);
    
    return deploymentId;
}

bool CICDSettings::rollbackDeployment(const QString& deploymentId, const QString& targetRunId)
{
    qDebug() << "[CICDSettings] Rolling back deployment:" << deploymentId << "to run:" << targetRunId;
    
    emit deploymentRolledBack(deploymentId);
    return true;
}

// ===== Webhook Integration =====

QString CICDSettings::registerWebhook(const QString& jobId, const QString& platform,
                                     const QString& repository, const QString& branch)
{
    qDebug() << "[CICDSettings] Registering webhook for:" << repository << "on" << platform;
    
    // Generate webhook URL
    QString webhookId = QString("webhook_%1_%2").arg(
        QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")
    ).arg(rand() % 10000);
    
    QString webhookUrl = QString("https://api.example.com/webhooks/%1").arg(webhookId);
    
    emit webhookReceived(platform, "registered");
    
    return webhookUrl;
}

QString CICDSettings::handleWebhook(const QJsonObject& webhookData)
{
    qDebug() << "[CICDSettings] Handling incoming webhook";
    
    // In production, parse webhook and trigger appropriate job
    // For now, just log it
    
    return "";
}

// ===== Notifications =====

bool CICDSettings::setNotificationConfig(const NotificationConfig& config)
{
    qDebug() << "[CICDSettings] Setting notification config";
    
    m_notificationConfig = config;
    return true;
}

CICDSettings::NotificationConfig CICDSettings::getNotificationConfig() const
{
    return m_notificationConfig;
}

bool CICDSettings::sendTestNotification() const
{
    qDebug() << "[CICDSettings] Sending test notification";
    
    // In production, actually send notification
    return true;
}

// ===== Artifact Management =====

bool CICDSettings::storeArtifact(const QString& artifactId, const QString& artifactPath,
                                const QJsonObject& metadata)
{
    qDebug() << "[CICDSettings] Storing artifact:" << artifactId;
    
    QJsonObject artifact = metadata;
    artifact["artifactId"] = artifactId;
    artifact["path"] = artifactPath;
    artifact["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    m_artifacts[artifactId] = artifact;
    return true;
}

QString CICDSettings::getArtifact(const QString& artifactId) const
{
    auto it = m_artifacts.find(artifactId);
    if (it == m_artifacts.end()) return "";
    
    return it->second["path"].toString();
}

std::vector<QString> CICDSettings::listArtifacts(const QString& jobId) const
{
    std::vector<QString> result;
    
    for (const auto& pair : m_artifacts) {
        if (pair.second["jobId"].toString() == jobId) {
            result.push_back(pair.first);
        }
    }
    
    return result;
}

int CICDSettings::cleanupOldArtifacts(int olderThanDays)
{
    qDebug() << "[CICDSettings] Cleaning up artifacts older than" << olderThanDays << "days";
    
    int removed = 0;
    QDateTime cutoff = QDateTime::currentDateTime().addDays(-olderThanDays);
    
    auto it = m_artifacts.begin();
    while (it != m_artifacts.end()) {
        QString timestampStr = it->second["timestamp"].toString();
        QDateTime timestamp = QDateTime::fromString(timestampStr, Qt::ISODate);
        
        if (timestamp < cutoff) {
            it = m_artifacts.erase(it);
            removed++;
        } else {
            ++it;
        }
    }
    
    return removed;
}

// ===== Configuration Export/Import =====

QJsonObject CICDSettings::exportConfiguration() const
{
    qDebug() << "[CICDSettings] Exporting configuration";
    
    QJsonObject config;
    
    // Export jobs
    QJsonArray jobsArray;
    for (const auto& pair : m_jobs) {
        QJsonObject jobObj;
        jobObj["jobId"] = pair.second.jobId;
        jobObj["jobName"] = pair.second.jobName;
        jobObj["modelName"] = pair.second.modelName;
        jobObj["enabled"] = pair.second.enabled;
        jobsArray.append(jobObj);
    }
    config["jobs"] = jobsArray;
    
    // Export deployment configs
    QJsonObject deploymentObj;
    for (const auto& pair : m_deploymentConfigs) {
        QJsonObject configObj;
        configObj["strategy"] = static_cast<int>(pair.second.strategy);
        configObj["targetEnvironment"] = pair.second.targetEnvironment;
        deploymentObj[pair.first] = configObj;
    }
    config["deploymentConfigs"] = deploymentObj;
    
    // Export notification config
    QJsonObject notifObj;
    notifObj["enableSlack"] = m_notificationConfig.enableSlack;
    notifObj["enableEmail"] = m_notificationConfig.enableEmail;
    config["notifications"] = notifObj;
    
    return config;
}

bool CICDSettings::importConfiguration(const QJsonObject& config)
{
    qDebug() << "[CICDSettings] Importing configuration";
    
    try {
        // Import jobs
        if (config.contains("jobs")) {
            QJsonArray jobsArray = config["jobs"].toArray();
            for (const auto& jobValue : jobsArray) {
                QJsonObject jobObj = jobValue.toObject();
                TrainingJob job;
                job.jobId = jobObj["jobId"].toString();
                job.jobName = jobObj["jobName"].toString();
                job.modelName = jobObj["modelName"].toString();
                job.enabled = jobObj["enabled"].toBool();
                m_jobs[job.jobId] = job;
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        qCritical() << "[CICDSettings] Error importing configuration:" << e.what();
        return false;
    }
}

bool CICDSettings::saveToFile(const QString& filePath) const
{
    qDebug() << "[CICDSettings] Saving configuration to file:" << filePath;
    
    try {
        QJsonObject config = exportConfiguration();
        QJsonDocument doc(config);
        
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            qCritical() << "[CICDSettings] Failed to open file for writing";
            return false;
        }
        
        file.write(doc.toJson());
        file.close();
        
        return true;
    } catch (const std::exception& e) {
        qCritical() << "[CICDSettings] Error saving to file:" << e.what();
        return false;
    }
}

bool CICDSettings::loadFromFile(const QString& filePath)
{
    qDebug() << "[CICDSettings] Loading configuration from file:" << filePath;
    
    try {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "[CICDSettings] File not found:" << filePath;
            return false;
        }
        
        QByteArray data = file.readAll();
        file.close();
        
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            qCritical() << "[CICDSettings] Invalid JSON format";
            return false;
        }
        
        return importConfiguration(doc.object());
    } catch (const std::exception& e) {
        qCritical() << "[CICDSettings] Error loading from file:" << e.what();
        return false;
    }
}

std::map<QString, QJsonObject> CICDSettings::getPipelineTemplates() const
{
    qDebug() << "[CICDSettings] Getting pipeline templates";
    
    std::map<QString, QJsonObject> templates;
    
    // Define some standard templates
    QJsonObject basicTemplate;
    basicTemplate["name"] = "Basic Training";
    basicTemplate["stages"] = QJsonArray();
    templates["basic"] = basicTemplate;
    
    QJsonObject advancedTemplate;
    advancedTemplate["name"] = "Advanced With Testing";
    advancedTemplate["stages"] = QJsonArray();
    templates["advanced"] = advancedTemplate;
    
    return templates;
}

