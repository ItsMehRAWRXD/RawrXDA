#include "ci_cd_settings.h"


#include <algorithm>

CICDSettings::CICDSettings(void* parent)
    : void(parent)
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
    std::string jobId = job.jobId.empty() ? generateJobId() : job.jobId;
    TrainingJob newJob = job;
    newJob.jobId = jobId;
    newJob.createdAt = std::chrono::system_clock::time_point::currentSecsSinceEpoch();
    newJob.lastRunAt = 0;
    newJob.successCount = 0;
    newJob.failureCount = 0;

    m_jobs[jobId] = newJob;
    return true;
}

bool CICDSettings::updateJob(const std::string& jobId, const TrainingJob& job)
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

CICDSettings::TrainingJob CICDSettings::getJob(const std::string& jobId) const
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

bool CICDSettings::deleteJob(const std::string& jobId)
{
    return m_jobs.erase(jobId) > 0;
}

bool CICDSettings::setJobEnabled(const std::string& jobId, bool enabled)
{
    auto it = m_jobs.find(jobId);
    if (it == m_jobs.end()) {
        return false;
    }

    it->second.enabled = enabled;
    return true;
}

std::string CICDSettings::queueJob(const std::string& jobId)
{
    auto it = m_jobs.find(jobId);
    if (it == m_jobs.end() || !it->second.enabled) {
        return "";
    }

    std::string runId = generateRunId();
    JobRunLog log;
    log.jobId = jobId;
    log.runId = runId;
    log.status = JobStatus::Queued;
    log.startTime = std::chrono::system_clock::time_point::currentSecsSinceEpoch();
    log.endTime = 0;

    m_runLogs[runId] = log;
    it->second.lastRunAt = log.startTime;

    jobQueued(jobId, runId);
    return runId;
}

bool CICDSettings::cancelJob(const std::string& runId)
{
    auto it = m_runLogs.find(runId);
    if (it == m_runLogs.end()) {
        return false;
    }

    if (it->second.status == JobStatus::Running || it->second.status == JobStatus::Queued) {
        it->second.status = JobStatus::Cancelled;
        it->second.endTime = std::chrono::system_clock::time_point::currentSecsSinceEpoch();
        return true;
    }

    return false;
}

std::string CICDSettings::retryJob(const std::string& runId)
{
    auto it = m_runLogs.find(runId);
    if (it == m_runLogs.end()) {
        return "";
    }

    return queueJob(it->second.jobId);
}

CICDSettings::JobRunLog CICDSettings::getJobRunLog(const std::string& runId) const
{
    auto it = m_runLogs.find(runId);
    if (it == m_runLogs.end()) {
        return JobRunLog();
    }
    return it->second;
}

std::vector<CICDSettings::JobRunLog> CICDSettings::getJobRunHistory(const std::string& jobId, int limit) const
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

std::tuple<int, int, int, float> CICDSettings::getJobStatistics(const std::string& jobId) const
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

bool CICDSettings::definePipeline(const std::string& jobId, const std::vector<PipelineStage>& stages)
{
    m_pipelines[jobId] = stages;
    return true;
}

std::vector<CICDSettings::PipelineStage> CICDSettings::getPipeline(const std::string& jobId) const
{
    auto it = m_pipelines.find(jobId);
    if (it == m_pipelines.end()) {
        return std::vector<PipelineStage>();
    }
    return it->second;
}

bool CICDSettings::setDeploymentConfig(const std::string& jobId, const DeploymentConfig& config)
{
    m_deploymentConfigs[jobId] = config;
    return true;
}

CICDSettings::DeploymentConfig CICDSettings::getDeploymentConfig(const std::string& jobId) const
{
    auto it = m_deploymentConfigs.find(jobId);
    if (it == m_deploymentConfigs.end()) {
        return DeploymentConfig();
    }
    return it->second;
}

std::string CICDSettings::deployModel(const std::string& jobId, const std::string& runId)
{
    auto runIt = m_runLogs.find(runId);
    if (runIt == m_runLogs.end() || runIt->second.jobId != jobId) {
        return "";
    }

    if (runIt->second.status != JobStatus::Completed) {
        return "";
    }

    std::string deploymentId = generateDeploymentId();
    deploymentStarted(deploymentId);

    // Simulate deployment
    deploymentCompleted(deploymentId, true);

    return deploymentId;
}

bool CICDSettings::rollbackDeployment(const std::string& deploymentId, const std::string& targetRunId)
{
    deploymentRolledBack(deploymentId);
    return true;
}

std::string CICDSettings::registerWebhook(const std::string& jobId, const std::string& platform,
                                      const std::string& repository, const std::string& branch)
{
    // Generate webhook URL
    std::string webhookUrl = std::string("https://agentic-ide.local/webhooks/%1/%2/%3/%4")
                            ;

    return webhookUrl;
}

std::string CICDSettings::handleWebhook(const void*& webhookData)
{
    std::string action = webhookData["action"].toString();
    std::string repository = webhookData["repository"].toString();

    webhookReceived("github", action);

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
    }

    if (m_notificationConfig.enableEmail) {
    }

    return true;
}

bool CICDSettings::storeArtifact(const std::string& artifactId, const std::string& artifactPath,
                               const void*& metadata)
{
    void* artifact = metadata;
    artifact["path"] = artifactPath;
    artifact["storedAt"] = static_cast<int64_t>(std::chrono::system_clock::time_point::currentSecsSinceEpoch());

    m_artifacts[artifactId] = artifact;
    return true;
}

std::string CICDSettings::getArtifact(const std::string& artifactId) const
{
    auto it = m_artifacts.find(artifactId);
    if (it == m_artifacts.end()) {
        return "";
    }
    return it->second["path"].toString();
}

std::vector<std::string> CICDSettings::listArtifacts(const std::string& jobId) const
{
    std::vector<std::string> result;

    for (const auto& [artifactId, metadata] : m_artifacts) {
        if (metadata["jobId"].toString() == jobId) {
            result.push_back(artifactId);
        }
    }

    return result;
}

int CICDSettings::cleanupOldArtifacts(int olderThanDays)
{
    int64_t threshold = std::chrono::system_clock::time_point::currentSecsSinceEpoch() - (olderThanDays * 86400);
    int deleted = 0;

    for (auto it = m_artifacts.begin(); it != m_artifacts.end(); ) {
        int64_t storedAt = it->second["storedAt"].toVariant().toLongLong();
        if (storedAt < threshold) {
            it = m_artifacts.erase(it);
            deleted++;
        } else {
            ++it;
        }
    }

    return deleted;
}

void* CICDSettings::exportConfiguration() const
{
    void* config;

    // Export jobs
    void* jobsArray;
    for (const auto& [jobId, job] : m_jobs) {
        void* jobObj;
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
    void* deploymentArray;
    for (const auto& [jobId, deployConfig] : m_deploymentConfigs) {
        void* deployObj;
        deployObj["jobId"] = jobId;
        deployObj["targetEnvironment"] = deployConfig.targetEnvironment;
        deployObj["strategy"] = static_cast<int>(deployConfig.strategy);
        deployObj["requireApproval"] = deployConfig.requireApproval;
        deploymentArray.append(deployObj);
    }
    config["deployments"] = deploymentArray;

    // Export notification config
    void* notifObj;
    notifObj["enableSlack"] = m_notificationConfig.enableSlack;
    notifObj["enableEmail"] = m_notificationConfig.enableEmail;
    notifObj["notifyOnSuccess"] = m_notificationConfig.notifyOnSuccess;
    notifObj["notifyOnFailure"] = m_notificationConfig.notifyOnFailure;
    config["notifications"] = notifObj;

    return config;
}

bool CICDSettings::importConfiguration(const void*& config)
{
    // Import jobs
    void* jobsArray = config["jobs"].toArray();
    for (const void*& val : jobsArray) {
        void* jobObj = val.toObject();
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
    void* deploymentArray = config["deployments"].toArray();
    for (const void*& val : deploymentArray) {
        void* deployObj = val.toObject();
        DeploymentConfig deployConfig;
        deployConfig.targetEnvironment = deployObj["targetEnvironment"].toString();
        deployConfig.strategy = static_cast<DeploymentStrategy>(deployObj["strategy"].toInt());
        deployConfig.requireApproval = deployObj["requireApproval"].toBool();
        setDeploymentConfig(deployObj["jobId"].toString(), deployConfig);
    }

    return true;
}

bool CICDSettings::saveToFile(const std::string& filePath) const
{
    std::fstream file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    void* config = exportConfiguration();
    file.write(void*(config).toJson(void*::Indented));
    file.close();

    return true;
}

bool CICDSettings::loadFromFile(const std::string& filePath)
{
    std::fstream file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    void* doc = void*::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) {
        return false;
    }

    return importConfiguration(doc.object());
}

std::map<std::string, void*> CICDSettings::getPipelineTemplates() const
{
    std::map<std::string, void*> templates;

    // Basic training template
    void* basicTemplate;
    basicTemplate["name"] = "Basic Training";
    basicTemplate["description"] = "Simple training job";
    void* stagesArray;
    void* stage1;
    stage1["name"] = "train";
    stage1["timeout"] = 3600;
    stagesArray.append(stage1);
    basicTemplate["stages"] = stagesArray;
    templates["basic"] = basicTemplate;

    // Production template
    void* prodTemplate;
    prodTemplate["name"] = "Production Training";
    prodTemplate["description"] = "Full production pipeline with validation and deployment";
    void* prodStages;
    
    void* trainStage;
    trainStage["name"] = "train";
    trainStage["timeout"] = 7200;
    prodStages.append(trainStage);
    
    void* evalStage;
    evalStage["name"] = "evaluate";
    evalStage["timeout"] = 1800;
    prodStages.append(evalStage);
    
    void* deployStage;
    deployStage["name"] = "deploy";
    deployStage["timeout"] = 600;
    prodStages.append(deployStage);
    
    prodTemplate["stages"] = prodStages;
    templates["production"] = prodTemplate;

    return templates;
}

std::string CICDSettings::generateJobId()
{
    return "job_" + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
}

std::string CICDSettings::generateRunId()
{
    return "run_" + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
}

std::string CICDSettings::generateDeploymentId()
{
    return "deploy_" + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
}


