#include "ci_cd_settings.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QDebug>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QStandardPaths>
#include <algorithm>
#include <random>

/**
 * @brief CICDSettings::CICDSettings - Constructor
 */
CICDSettings::CICDSettings(QObject* parent)
    : QObject(parent), m_selectedStrategy(DeploymentStrategy::Immediate),
      m_jobQueueSize(0), m_maxConcurrentJobs(4)
{
    qDebug() << "[CICDSettings] Initializing CI/CD settings";
    loadConfiguration();
}

/**
 * @brief CICDSettings::~CICDSettings - Destructor
 */
CICDSettings::~CICDSettings()
{
    qDebug() << "[CICDSettings] CI/CD settings destroyed";
    saveConfiguration();
}

/**
 * @brief CICDSettings::createJobConfig - Create a new training job configuration
 */
QString CICDSettings::createJobConfig(const JobConfiguration& config)
{
    qDebug() << "[CICDSettings] Creating job configuration:" << config.jobName;
    
    try {
        // Generate job ID
        QString jobId = QString("job_%1_%2")
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"))
            .arg(qrand() % 10000);
        
        // Create job object
        QJsonObject jobObj;
        jobObj["jobId"] = jobId;
        jobObj["jobName"] = config.jobName;
        jobObj["status"] = static_cast<int>(JobStatus::Pending);
        jobObj["createdAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        
        // Store training parameters
        jobObj["model"] = config.model;
        jobObj["dataset"] = config.dataset;
        jobObj["batchSize"] = config.batchSize;
        jobObj["epochs"] = config.epochs;
        jobObj["learningRate"] = config.learningRate;
        jobObj["validationSplit"] = config.validationSplit;
        
        // Store scheduling info
        jobObj["trigger"] = static_cast<int>(config.trigger);
        jobObj["schedule"] = config.schedule;  // Cron expression
        
        m_jobConfigurations[jobId] = jobObj;
        m_jobQueue.push_back(jobId);
        m_jobQueueSize = m_jobQueue.size();
        
        // Emit signal
        emit jobCreated(jobId, config.jobName);
        
        qDebug() << "[CICDSettings] Job created with ID:" << jobId;
        return jobId;
    }
    catch (const std::exception& e) {
        qCritical() << "[CICDSettings] Failed to create job:" << e.what();
        return "";
    }
}

/**
 * @brief CICDSettings::addPipelineStage - Add stage to deployment pipeline
 */
bool CICDSettings::addPipelineStage(const PipelineStage& stage)
{
    qDebug() << "[CICDSettings] Adding pipeline stage:" << stage.stageName;
    
    try {
        QJsonObject stageObj;
        stageObj["stageName"] = stage.stageName;
        stageObj["command"] = stage.command;
        stageObj["timeout"] = stage.timeoutSeconds;
        stageObj["retries"] = stage.maxRetries;
        
        m_pipelineStages.push_back(stageObj);
        
        emit pipelineStageAdded(stage.stageName);
        
        return true;
    }
    catch (const std::exception& e) {
        qCritical() << "[CICDSettings] Failed to add pipeline stage:" << e.what();
        return false;
    }
}

/**
 * @brief CICDSettings::configureDeploymentStrategy - Set deployment strategy
 */
bool CICDSettings::configureDeploymentStrategy(DeploymentStrategy strategy,
                                              const DeploymentConfig& config)
{
    qDebug() << "[CICDSettings] Configuring deployment strategy:" << static_cast<int>(strategy);
    
    try {
        m_selectedStrategy = strategy;
        
        QJsonObject strategyObj;
        strategyObj["strategy"] = static_cast<int>(strategy);
        
        switch (strategy) {
            case DeploymentStrategy::Immediate:
                strategyObj["description"] = "Deploy immediately after build";
                break;
                
            case DeploymentStrategy::Canary: {
                strategyObj["description"] = "Gradual rollout to subset of traffic";
                strategyObj["canaryPercentage"] = config.canaryTrafficPercentage;
                strategyObj["metricsWindow"] = config.metricsWindow;
                strategyObj["errorThreshold"] = config.errorThreshold;
                break;
            }
            
            case DeploymentStrategy::BlueGreen:
                strategyObj["description"] = "Switch between active and inactive environments";
                strategyObj["switchBackOnFailure"] = config.switchBackOnFailure;
                break;
                
            case DeploymentStrategy::RollingUpdate:
                strategyObj["description"] = "Gradually replace instances";
                strategyObj["maxUnavailable"] = config.maxUnavailable;
                strategyObj["maxSurge"] = config.maxSurge;
                break;
        }
        
        m_deploymentConfig = strategyObj;
        
        emit deploymentStrategyConfigured(static_cast<int>(strategy));
        
        return true;
    }
    catch (const std::exception& e) {
        qCritical() << "[CICDSettings] Failed to configure deployment:" << e.what();
        return false;
    }
}

/**
 * @brief CICDSettings::addWebhook - Add GitHub/GitLab webhook
 */
bool CICDSettings::addWebhook(const WebhookConfig& webhook)
{
    qDebug() << "[CICDSettings] Adding webhook for:" << webhook.repository;
    
    try {
        QJsonObject hookObj;
        hookObj["repository"] = webhook.repository;
        hookObj["events"] = webhook.events;
        hookObj["branch"] = webhook.branch;
        hookObj["addedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        
        m_webhooks.push_back(hookObj);
        
        emit webhookAdded(webhook.repository);
        
        qDebug() << "[CICDSettings] Webhook added";
        return true;
    }
    catch (const std::exception& e) {
        qCritical() << "[CICDSettings] Failed to add webhook:" << e.what();
        return false;
    }
}

/**
 * @brief CICDSettings::queueJob - Queue a job for execution
 */
bool CICDSettings::queueJob(const QString& jobId)
{
    qDebug() << "[CICDSettings] Queuing job:" << jobId;
    
    try {
        if (m_jobConfigurations.find(jobId) == m_jobConfigurations.end()) {
            qWarning() << "[CICDSettings] Job not found:" << jobId;
            return false;
        }
        
        // Add to queue if not already there
        auto it = std::find(m_jobQueue.begin(), m_jobQueue.end(), jobId);
        if (it == m_jobQueue.end()) {
            m_jobQueue.push_back(jobId);
        }
        
        m_jobQueueSize = m_jobQueue.size();
        
        // Update job status
        m_jobConfigurations[jobId]["status"] = static_cast<int>(JobStatus::Queued);
        
        emit jobQueued(jobId);
        
        return true;
    }
    catch (const std::exception& e) {
        qCritical() << "[CICDSettings] Failed to queue job:" << e.what();
        return false;
    }
}

/**
 * @brief CICDSettings::runPipeline - Execute deployment pipeline
 */
bool CICDSettings::runPipeline(const QString& jobId)
{
    qDebug() << "[CICDSettings] Running pipeline for job:" << jobId;
    
    try {
        if (m_jobConfigurations.find(jobId) == m_jobConfigurations.end()) {
            qWarning() << "[CICDSettings] Job not found:" << jobId;
            return false;
        }
        
        // Update job status
        m_jobConfigurations[jobId]["status"] = static_cast<int>(JobStatus::Running);
        m_jobConfigurations[jobId]["startedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        
        // Execute each stage
        for (const auto& stage : m_pipelineStages) {
            // Simulate stage execution
            qDebug() << "[CICDSettings] Executing stage:" << stage["stageName"].toString();
            
            // In production, actually execute the command
            // For now, just log it
        }
        
        // Mark as completed
        m_jobConfigurations[jobId]["status"] = static_cast<int>(JobStatus::Completed);
        m_jobConfigurations[jobId]["completedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        
        emit pipelineCompleted(jobId);
        
        qDebug() << "[CICDSettings] Pipeline completed for job:" << jobId;
        return true;
    }
    catch (const std::exception& e) {
        qCritical() << "[CICDSettings] Pipeline failed:" << e.what();
        
        // Mark as failed
        m_jobConfigurations[jobId]["status"] = static_cast<int>(JobStatus::Failed);
        m_jobConfigurations[jobId]["completedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        
        emit pipelineFailed(jobId, e.what());
        
        return false;
    }
}

/**
 * @brief CICDSettings::cancelJob - Cancel a queued or running job
 */
bool CICDSettings::cancelJob(const QString& jobId)
{
    qDebug() << "[CICDSettings] Cancelling job:" << jobId;
    
    try {
        if (m_jobConfigurations.find(jobId) == m_jobConfigurations.end()) {
            return false;
        }
        
        // Remove from queue
        auto it = std::find(m_jobQueue.begin(), m_jobQueue.end(), jobId);
        if (it != m_jobQueue.end()) {
            m_jobQueue.erase(it);
        }
        
        // Update status
        m_jobConfigurations[jobId]["status"] = static_cast<int>(JobStatus::Cancelled);
        m_jobQueueSize = m_jobQueue.size();
        
        emit jobCancelled(jobId);
        
        return true;
    }
    catch (const std::exception& e) {
        qCritical() << "[CICDSettings] Failed to cancel job:" << e.what();
        return false;
    }
}

/**
 * @brief CICDSettings::retryJob - Retry a failed job
 */
bool CICDSettings::retryJob(const QString& jobId, int maxRetries)
{
    qDebug() << "[CICDSettings] Retrying job:" << jobId;
    
    try {
        if (m_jobConfigurations.find(jobId) == m_jobConfigurations.end()) {
            return false;
        }
        
        // Update status and retry count
        int retries = m_jobConfigurations[jobId]["retries"].toInt();
        if (retries >= maxRetries) {
            qWarning() << "[CICDSettings] Max retries exceeded for job:" << jobId;
            return false;
        }
        
        m_jobConfigurations[jobId]["status"] = static_cast<int>(JobStatus::Queued);
        m_jobConfigurations[jobId]["retries"] = retries + 1;
        
        // Add back to queue
        m_jobQueue.push_back(jobId);
        m_jobQueueSize = m_jobQueue.size();
        
        emit jobRetried(jobId);
        
        return true;
    }
    catch (const std::exception& e) {
        qCritical() << "[CICDSettings] Failed to retry job:" << e.what();
        return false;
    }
}

/**
 * @brief CICDSettings::sendNotification - Send notification via Slack/Email
 */
bool CICDSettings::sendNotification(const QString& channel, const QString& message)
{
    qDebug() << "[CICDSettings] Sending notification to" << channel << ":" << message;
    
    try {
        // In production, integrate with Slack/Email API
        QJsonObject notification;
        notification["channel"] = channel;
        notification["message"] = message;
        notification["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        
        m_notifications.push_back(notification);
        
        emit notificationSent(channel, message);
        
        return true;
    }
    catch (const std::exception& e) {
        qCritical() << "[CICDSettings] Failed to send notification:" << e.what();
        return false;
    }
}

/**
 * @brief CICDSettings::getJobStatus - Get status of a job
 */
CICDSettings::JobStatus CICDSettings::getJobStatus(const QString& jobId)
{
    if (m_jobConfigurations.find(jobId) == m_jobConfigurations.end()) {
        return JobStatus::Pending;
    }
    
    int status = m_jobConfigurations[jobId]["status"].toInt();
    return static_cast<JobStatus>(status);
}

/**
 * @brief CICDSettings::getQueuedJobs - Get all queued jobs
 */
std::vector<QString> CICDSettings::getQueuedJobs()
{
    return m_jobQueue;
}

/**
 * @brief CICDSettings::getArtifactVersions - Get available artifact versions
 */
std::vector<ArtifactVersion> CICDSettings::getArtifactVersions()
{
    std::vector<ArtifactVersion> versions;
    
    for (const auto& artifact : m_artifacts) {
        ArtifactVersion ver;
        ver.version = artifact["version"].toString();
        ver.buildNumber = artifact["buildNumber"].toInt();
        ver.timestamp = artifact["timestamp"].toString();
        ver.size = artifact["size"].toInt();
        ver.checksum = artifact["checksum"].toString();
        
        versions.push_back(ver);
    }
    
    return versions;
}

/**
 * @brief CICDSettings::loadConfiguration - Load configuration from disk
 */
void CICDSettings::loadConfiguration()
{
    qDebug() << "[CICDSettings] Loading configuration";
    
    try {
        QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                           + "/ci_cd_config.json";
        
        QFile file(configPath);
        if (!file.open(QIODevice::ReadOnly)) {
            qDebug() << "[CICDSettings] No existing configuration";
            return;
        }
        
        QByteArray data = file.readAll();
        file.close();
        
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject obj = doc.object();
        
        // Load deployment strategy
        m_selectedStrategy = static_cast<DeploymentStrategy>(obj["deploymentStrategy"].toInt());
        
        // Load max concurrent jobs
        m_maxConcurrentJobs = obj["maxConcurrentJobs"].toInt(4);
        
        qDebug() << "[CICDSettings] Configuration loaded";
    }
    catch (const std::exception& e) {
        qWarning() << "[CICDSettings] Failed to load configuration:" << e.what();
    }
}

/**
 * @brief CICDSettings::saveConfiguration - Save configuration to disk
 */
void CICDSettings::saveConfiguration()
{
    qDebug() << "[CICDSettings] Saving configuration";
    
    try {
        QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                           + "/ci_cd_config.json";
        
        QJsonObject obj;
        obj["deploymentStrategy"] = static_cast<int>(m_selectedStrategy);
        obj["maxConcurrentJobs"] = m_maxConcurrentJobs;
        obj["jobCount"] = static_cast<int>(m_jobConfigurations.size());
        obj["lastSaved"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        
        QJsonDocument doc(obj);
        QFile file(configPath);
        
        if (!file.open(QIODevice::WriteOnly)) {
            qCritical() << "[CICDSettings] Failed to open config file for writing";
            return;
        }
        
        file.write(doc.toJson());
        file.close();
        
        qDebug() << "[CICDSettings] Configuration saved";
    }
    catch (const std::exception& e) {
        qWarning() << "[CICDSettings] Failed to save configuration:" << e.what();
    }
}

/**
 * @brief CICDSettings::exportJobLog - Export job execution log
 */
QJsonObject CICDSettings::exportJobLog(const QString& jobId)
{
    QJsonObject log;
    
    if (m_jobConfigurations.find(jobId) != m_jobConfigurations.end()) {
        log = m_jobConfigurations[jobId];
    }
    
    return log;
}

/**
 * @brief CICDSettings::getJobStatistics - Get job execution statistics
 */
QJsonObject CICDSettings::getJobStatistics()
{
    QJsonObject stats;
    
    stats["totalJobs"] = static_cast<int>(m_jobConfigurations.size());
    stats["queuedJobs"] = static_cast<int>(m_jobQueue.size());
    stats["maxConcurrentJobs"] = m_maxConcurrentJobs;
    stats["deploymentStrategy"] = static_cast<int>(m_selectedStrategy);
    stats["lastUpdated"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    // Count by status
    int pending = 0, completed = 0, failed = 0;
    for (const auto& job : m_jobConfigurations) {
        int status = job["status"].toInt();
        if (status == static_cast<int>(JobStatus::Completed)) completed++;
        else if (status == static_cast<int>(JobStatus::Failed)) failed++;
        else if (status == static_cast<int>(JobStatus::Pending)) pending++;
    }
    
    stats["statusPending"] = pending;
    stats["statusCompleted"] = completed;
    stats["statusFailed"] = failed;
    
    return stats;
}
