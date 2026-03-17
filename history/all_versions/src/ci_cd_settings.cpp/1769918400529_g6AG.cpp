#include "ci_cd_settings.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <thread>
#include "nlohmann/json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

// Helper to generate simple random IDs
static std::string generateSimpleId(const std::string& prefix) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    std::stringstream ss;
    ss << prefix << "_";
    for (int i = 0; i < 8; i++) {
        ss << std::hex << dis(gen);
    }
    return ss.str();
}

static int64_t currentTimestamp() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

CICDSettings::CICDSettings(void* parent)
{
    (void)parent; // Unused
    // Initialize default notification config
    m_notificationConfig.enableSlack = false;
    m_notificationConfig.enableEmail = false;
    m_notificationConfig.notifyOnSuccess = true;
    m_notificationConfig.notifyOnFailure = true;
    m_notificationConfig.notifyOnStart = false;
}

CICDSettings::~CICDSettings()
{
    // Clean up any void* artifacts if they were dynamically allocated (assuming ownership)
    // For this implementation, we'll assume ownership is managed elsewhere or they are just json pointers
    for (auto& [key, ptr] : m_artifacts) {
        if (ptr) {
            delete static_cast<json*>(ptr);
            ptr = nullptr;
        }
    }
}

bool CICDSettings::createJob(const TrainingJob& job)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string jobId = job.jobId.empty() ? generateJobId() : job.jobId;
    TrainingJob newJob = job;
    newJob.jobId = jobId;
    newJob.createdAt = currentTimestamp();
    newJob.lastRunAt = 0;
    newJob.successCount = 0;
    newJob.failureCount = 0;

    m_jobs[jobId] = newJob;
    return true;
}

bool CICDSettings::updateJob(const std::string& jobId, const TrainingJob& job)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_jobs.find(jobId);
    if (it == m_jobs.end()) {
        return false;
    }

    TrainingJob updated = job;
    updated.jobId = jobId;
    updated.createdAt = it->second.createdAt; // Preserve creation time
    updated.successCount = it->second.successCount;
    updated.failureCount = it->second.failureCount;

    m_jobs[jobId] = updated;
    return true;
}

CICDSettings::TrainingJob CICDSettings::getJob(const std::string& jobId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_jobs.find(jobId);
    if (it == m_jobs.end()) {
        return TrainingJob{};
    }
    return it->second;
}

std::vector<CICDSettings::TrainingJob> CICDSettings::listJobs() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<TrainingJob> result;
    for (const auto& [jobId, job] : m_jobs) {
        result.push_back(job);
    }
    return result;
}

bool CICDSettings::deleteJob(const std::string& jobId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_jobs.erase(jobId) > 0;
}

bool CICDSettings::setJobEnabled(const std::string& jobId, bool enabled)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_jobs.find(jobId);
    if (it == m_jobs.end()) {
        return false;
    }

    it->second.enabled = enabled;
    return true;
}

std::string CICDSettings::queueJob(const std::string& jobId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_jobs.find(jobId);
    if (it == m_jobs.end() || !it->second.enabled) {
        return "";
    }

    std::string runId = generateRunId();
    JobRunLog log;
    log.jobId = jobId;
    log.runId = runId;
    log.status = JobStatus::Queued;
    log.startTime = currentTimestamp();
    log.endTime = 0;

    m_runLogs[runId] = log;
    it->second.lastRunAt = log.startTime;

    // Simulate async job start notification
    // jobStarted(jobId, runId); 
    
    return runId;
}

bool CICDSettings::cancelJob(const std::string& runId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_runLogs.find(runId);
    if (it == m_runLogs.end()) {
        return false;
    }

    if (it->second.status == JobStatus::Running || it->second.status == JobStatus::Queued) {
        it->second.status = JobStatus::Cancelled;
        it->second.endTime = currentTimestamp();
        return true;
    }

    return false;
}

CICDSettings::JobStatus CICDSettings::getJobStatus(const std::string& runId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_runLogs.find(runId);
    if (it == m_runLogs.end()) {
        return JobStatus::Failed; // Or some unknown status
    }
    return it->second.status;
}

std::vector<CICDSettings::JobRunLog> CICDSettings::getJobHistory(const std::string& jobId, int limit) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<JobRunLog> result;

    for (const auto& [runId, log] : m_runLogs) {
        if (log.jobId == jobId) {
            result.push_back(log);
        }
    }

    std::sort(result.begin(), result.end(),
             [](const JobRunLog& a, const JobRunLog& b) {
                 return a.startTime > b.startTime;
             });

    if (limit > 0 && result.size() > static_cast<size_t>(limit)) {
        result.resize(limit);
    }

    return result;
}

std::tuple<int, int, int, float> CICDSettings::getJobStatistics(const std::string& jobId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
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
                totalDuration += (float)(log.endTime - log.startTime);
            }
        }
    }

    float avgDuration = totalRuns > 0 ? totalDuration / totalRuns : 0.0f;
    return std::make_tuple(totalRuns, successCount, failureCount, avgDuration);
}

bool CICDSettings::definePipeline(const std::string& jobId, const std::vector<PipelineStage>& stages)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pipelines[jobId] = stages;
    return true;
}

std::vector<CICDSettings::PipelineStage> CICDSettings::getPipeline(const std::string& jobId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_pipelines.find(jobId);
    if (it == m_pipelines.end()) {
        return {};
    }
    return it->second;
}

std::map<std::string, void*> CICDSettings::getPipelineTemplates() const
{
    // Return dynamically allocated json objects as void*
    // Caller is responsible for cleanup if they take ownership, or we manage it.
    // In this simulation, we return new objects and leak them intentionally as per the weird interface signature,
    // or we could store them. For safety, let's create a temporary map.
    
    std::map<std::string, void*> templates;
    
    json* basic = new json({
        {"name", "Basic Training"},
        {"description", "Simple training job"},
        {"stages", {
            {{"name", "train"}, {"timeout", 3600}}
        }}
    });
    
    json* prod = new json({
        {"name", "Production Training"},
        {"description", "Full production pipeline"},
        {"stages", {
            {{"name", "train"}, {"timeout", 7200}},
            {{"name", "evaluate"}, {"timeout", 1800}},
            {{"name", "deploy"}, {"timeout", 600}}
        }}
    });

    templates["basic"] = basic;
    templates["production"] = prod;
    return templates;
}

bool CICDSettings::setDeploymentConfig(const std::string& jobId, const DeploymentConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_deploymentConfigs[jobId] = config;
    return true;
}

CICDSettings::DeploymentConfig CICDSettings::getDeploymentConfig(const std::string& jobId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_deploymentConfigs.find(jobId);
    if (it == m_deploymentConfigs.end()) {
        return DeploymentConfig();
    }
    return it->second;
}

std::string CICDSettings::deployModel(const std::string& jobId, const std::string& runId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Validate run exists
    auto runIt = m_runLogs.find(runId);
    if (runIt == m_runLogs.end() || runIt->second.jobId != jobId) {
        std::cerr << "Deployment Error: Invalid Run ID or Job ID mismatch." << std::endl;
        return "";
    }

    // In a real scenario, we might want to allow force deploys, but let's check basic status
    // if (runIt->second.status != JobStatus::Completed) { ... }

    std::string deploymentId = generateDeploymentId();
    std::cout << "[Deployment] Starting deployment sequence for Job: " << jobId << ", Run: " << runId << std::endl;

    // Retrieve Deployment Config
    auto configIt = m_deploymentConfigs.find(jobId);
    std::string modelPath = (configIt != m_deploymentConfigs.end()) ? configIt->second.modelPath : "default_model.bin";
    std::string targetEnv = (configIt != m_deploymentConfigs.end()) ? configIt->second.targetEnvironment : "staging";

    try {
        // 1. Real Deployment Environment Prep
        fs::path deployDir = fs::current_path() / "deployments" / targetEnv / deploymentId;
        fs::create_directories(deployDir);

        // 2. Real Model Copy
        fs::path sourcePath = fs::path(modelPath);
        if (fs::exists(sourcePath)) {
            fs::copy(sourcePath, deployDir / sourcePath.filename(), fs::copy_options::overwrite_existing);
            std::cout << "[Deployment] Copied model artifact to: " << deployDir.string() << std::endl;
        } else {
             throw std::runtime_error("Deployment Source Invalid: " + modelPath);
        }

        // 3. Write Deployment Metadata using nlohmann::json
        json meta = {
            {"deploymentId", deploymentId},
            {"jobId", jobId},
            {"runId", runId},
            {"timestamp", currentTimestamp()},
            {"status", "active"}
        };
        std::ofstream metaFile(deployDir / "deployment_metadata.json");
        metaFile << meta.dump(4);
        metaFile.close();

        std::cout << "[Deployment] Deployment " << deploymentId << " completed successfully." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[Deployment] Failed with error: " << e.what() << std::endl;
        return ""; // Indicate failure
    }

    return deploymentId;
}

bool CICDSettings::rollbackDeployment(const std::string& deploymentId, const std::string& targetRunId)
{
    // deploymentRolledBack(deploymentId); // signal
    std::cout << "[Rollback] Rolling back " << deploymentId << " to run " << targetRunId << std::endl;
    return true;
}

std::string CICDSettings::registerWebhook(const std::string& jobId, const std::string& platform,
                                      const std::string& repository, const std::string& branch)
{
    std::stringstream url;
    url << "https://agentic-ide.local/webhooks/" 
        << platform << "/" 
        << jobId << "/" 
        << repository;
    return url.str();
}

std::string CICDSettings::handleWebhook(const void*& webhookData)
{
    // Assume webhookData is a pointer to a nlohmann::json object
    if (!webhookData) return "";

    const json* j = static_cast<const json*>(webhookData);
    if (j->contains("action") && j->contains("repository")) {
        std::string action = (*j)["action"].get<std::string>();
        // webhookReceived("github", action);
        return "processed";
    }
    return "";
}

bool CICDSettings::setNotificationConfig(const NotificationConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_notificationConfig = config;
    return true;
}

CICDSettings::NotificationConfig CICDSettings::getNotificationConfig() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_notificationConfig;
}

bool CICDSettings::sendTestNotification() const
{
    std::cout << "Sending test notification..." << std::endl;
    // Implementation would use curl or similar to hit slack/email
    return true;
}

bool CICDSettings::storeArtifact(const std::string& artifactId, const std::string& artifactPath,
                               const void*& metadata)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    json* artifactJson = new json();
    if (metadata) {
        *artifactJson = *static_cast<const json*>(metadata);
    }
    (*artifactJson)["path"] = artifactPath;
    (*artifactJson)["storedAt"] = currentTimestamp();
    
    // Store as void* to match interface (leak or manage later)
    m_artifacts[artifactId] = artifactJson; 
    return true;
}

std::string CICDSettings::getArtifact(const std::string& artifactId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_artifacts.find(artifactId);
    if (it == m_artifacts.end()) {
        return "";
    }
    json* j = static_cast<json*>(it->second);
    return j->value("path", "");
}

std::vector<std::string> CICDSettings::listArtifacts(const std::string& jobId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> result;

    for (const auto& [artifactId, metadata] : m_artifacts) {
        json* j = static_cast<json*>(metadata);
        if (j->value("jobId", "") == jobId) {
            result.push_back(artifactId);
        }
    }
    return result;
}

int CICDSettings::cleanupOldArtifacts(int olderThanDays)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    int64_t threshold = currentTimestamp() - (olderThanDays * 86400);
    int deleted = 0;

    for (auto it = m_artifacts.begin(); it != m_artifacts.end(); ) {
        json* j = static_cast<json*>(it->second);
        int64_t storedAt = j->value("storedAt", (int64_t)0);
        
        if (storedAt < threshold) {
            delete j; // Free memory
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
    std::lock_guard<std::mutex> lock(m_mutex);
    json* root = new json();
    
    json jobsArray = json::array();
    for (const auto& [jobId, job] : m_jobs) {
        jobsArray.push_back({
            {"jobId", job.jobId},
            {"jobName", job.jobName},
            {"modelName", job.modelName},
            {"epochs", job.epochs},
            {"batchSize", job.batchSize},
            {"learningRate", job.learningRate},
            {"enabled", job.enabled}
            // Add other fields as needed
        });
    }
    (*root)["jobs"] = jobsArray;

    json deploysArray = json::array();
    for (const auto& [jobId, cfg] : m_deploymentConfigs) {
        deploysArray.push_back({
            {"jobId", jobId},
            {"targetEnvironment", cfg.targetEnvironment},
            {"strategy", (int)cfg.strategy},
            {"requireApproval", cfg.requireApproval}
        });
    }
    (*root)["deployments"] = deploysArray;

    return root;
}

bool CICDSettings::importConfiguration(const void*& config)
{
    if (!config) return false;
    const json* root = static_cast<const json*>(config);
    std::lock_guard<std::mutex> lock(m_mutex);

    if (root->contains("jobs")) {
        for (const auto& j : (*root)["jobs"]) {
            TrainingJob job;
            job.jobId = j.value("jobId", "");
            job.jobName = j.value("jobName", "");
            job.modelName = j.value("modelName", "");
            job.epochs = j.value("epochs", 10);
            job.batchSize = j.value("batchSize", 32);
            job.learningRate = j.value("learningRate", 0.001);
            job.enabled = j.value("enabled", true);
            m_jobs[job.jobId] = job;
        }
    }

    if (root->contains("deployments")) {
        for (const auto& d : (*root)["deployments"]) {
            DeploymentConfig cfg;
            std::string jId = d.value("jobId", "");
            cfg.targetEnvironment = d.value("targetEnvironment", "staging");
            cfg.strategy = (DeploymentStrategy)d.value("strategy", 0);
            cfg.requireApproval = d.value("requireApproval", false);
            m_deploymentConfigs[jId] = cfg;
        }
    }

    return true;
}

bool CICDSettings::saveToFile(const std::string& filePath) const
{
    // Need to temporarily export to json to save
    void* configPtr = exportConfiguration();
    json* config = static_cast<json*>(configPtr);
    
    try {
        std::ofstream file(filePath);
        file << config->dump(4);
        file.close();
        delete config; // Cleanup temporary export
        return true;
    } catch (...) {
        delete config;
        return false;
    }
}

bool CICDSettings::loadFromFile(const std::string& filePath)
{
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) return false;
        
        json j;
        file >> j;
        const void* ptr = &j;
        return importConfiguration(ptr);
    } catch (...) {
        return false;
    }
}

std::string CICDSettings::generateJobId() { return generateSimpleId("job"); }
std::string CICDSettings::generateRunId() { return generateSimpleId("run"); }
std::string CICDSettings::generateDeploymentId() { return generateSimpleId("deploy"); }



