#pragma once

#include <string>
<<<<<<< HEAD

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

class CICDSettings {
public:
=======
#include <vector>
#include <map>
#include <tuple>
#include <cstdint>

/**
 * @class CICDSettings
 * @brief Configure CI/CD pipelines for automated training and deployment
 *
 * Features:
 * - Define training jobs (hyperparameters, data, schedules)
 * - Job queuing and scheduling (immediate, scheduled, recurring)
 * - Deployment pipelines (model versioning, A/B testing)
 * - Webhook integration (GitHub, GitLab, Bitbucket)
 * - Automated model evaluation and validation
 * - Performance benchmarking and regression testing
 * - Artifact management (model versioning, checkpoint storage)
 * - Notification and alerting (email, Slack, webhook)
 * - Rollback capabilities
 * - Pipeline templates and presets
 */
class CICDSettings
{
public:
    /** Show the CI/CD settings window (Win32 IDE integration). Invokes setShowCallback when set. */
    void show() { if (m_showCb) m_showCb(m_showCtx); }

    /** Register callback for show() — IDE sets this to display the CI/CD settings dialog. */
>>>>>>> origin/main
    using ShowCallback = void(*)(void* ctx);

<<<<<<< HEAD
    explicit CICDSettings(void* parent = nullptr)
        : m_parent(parent) {}

=======
    enum class JobStatus {
        Pending,
        Queued,
        Running,
        Completed,
        Failed,
        Cancelled
    };

    enum class TriggerType {
        Manual,
        Scheduled,
        Webhook,
        PullRequest,
        Push,
        Tag
    };

    enum class DeploymentStrategy {
        Immediate,          // Deploy immediately after success
        Canary,             // Deploy to small % of users first
        BlueGreen,          // Run old and new in parallel
        RollingUpdate,      // Gradual rollout
        ManualApproval      // Wait for manual approval
    };

    struct TrainingJob {
        std::string jobId;
        std::string jobName;
        std::string description;
        std::string modelName;
        std::string datasetPath;
        int epochs;
        int batchSize;
        float learningRate;
        std::string optimizer;
        int numGPUs;
        std::string priority;   // "low", "normal", "high"
        TriggerType trigger;
        std::string cronSchedule;  // For scheduled jobs (e.g., "0 2 * * *" = 2 AM daily)
        bool enabled;
        int64_t createdAt;
        int64_t lastRunAt;
        int successCount;
        int failureCount;
    };

    struct PipelineStage {
        std::string stageName;
        std::string description;
        bool enabled;
        int timeoutSeconds;
        std::vector<std::string> commands;  // Shell commands to run
        bool continueOnError;
    };

    struct DeploymentConfig {
        std::string modelPath;
        DeploymentStrategy strategy;
        float canaryPercentage;     // 0.0 to 1.0, for Canary strategy
        std::string targetEnvironment;  // "staging", "production"
        bool requireApproval;
        std::vector<std::string> approvers;
        bool rollbackOnFailure;
        int maxConcurrentRequests;
    };

    struct NotificationConfig {
        bool enableSlack;
        std::string slackWebhookUrl;
        std::string slackChannel;
        bool enableEmail;
        std::vector<std::string> emailRecipients;
        bool notifyOnSuccess;
        bool notifyOnFailure;
        bool notifyOnStart;
    };

    struct JobRunLog {
        std::string jobId;
        std::string runId;
        JobStatus status;
        int64_t startTime;
        int64_t endTime;
        std::string logOutput;
        float accuracy;
        float loss;
        std::string artifactPath;
        std::string errorMessage;
    };

    // Constructor
    CICDSettings() = default;
>>>>>>> origin/main
    ~CICDSettings() = default;

    void setShowCallback(ShowCallback cb, void* ctx) {
        m_showCb = cb;
        m_showCtx = ctx;
    }

<<<<<<< HEAD
    void show() {
        if (m_showCb) {
            m_showCb(m_showCtx ? m_showCtx : m_parent);
            return;
        }
#if defined(_WIN32)
        MessageBoxA(static_cast<HWND>(m_parent),
                    "CI/CD settings dialog callback is not wired yet.",
                    "RawrXD CI/CD Settings",
                    MB_OK | MB_ICONINFORMATION);
#endif
    }

private:
    void* m_parent = nullptr;
=======
    /**
     * @brief Update existing job
     * @param jobId Job identifier
     * @param job Updated configuration
     * @return true if successful
     */
    bool updateJob(const std::string& jobId, const TrainingJob& job);

    /**
     * @brief Get job by ID
     * @param jobId Job identifier
     * @return Job configuration or empty if not found
     */
    TrainingJob getJob(const std::string& jobId) const;

    /**
     * @brief List all jobs
     * @return Vector of all job configurations
     */
    std::vector<TrainingJob> listJobs() const;

    /**
     * @brief Delete job
     * @param jobId Job identifier
     * @return true if successful
     */
    bool deleteJob(const std::string& jobId);

    /**
     * @brief Enable/disable job
     * @param jobId Job identifier
     * @param enabled Enable state
     * @return true if successful
     */
    bool setJobEnabled(const std::string& jobId, bool enabled);

    // ===== Job Execution =====
    /**
     * @brief Queue job for execution
     * @param jobId Job identifier
     * @return Run ID or empty if failed
     */
    std::string queueJob(const std::string& jobId);

    /**
     * @brief Cancel running job
     * @param runId Run identifier
     * @return true if successful
     */
    bool cancelJob(const std::string& runId);

    /**
     * @brief Retry failed job
     * @param runId Original run ID
     * @return New run ID or empty if failed
     */
    std::string retryJob(const std::string& runId);

    /**
     * @brief Get job execution logs
     * @param runId Run identifier
     * @return Execution log entry
     */
    JobRunLog getJobRunLog(const std::string& runId) const;

    /**
     * @brief List job run history
     * @param jobId Job identifier
     * @param limit Number of recent runs to return
     * @return List of run logs
     */
    std::vector<JobRunLog> getJobRunHistory(const std::string& jobId, int limit = 20) const;

    /**
     * @brief Get job statistics
     * @param jobId Job identifier
     * @return (totalRuns, successCount, failureCount, avgDuration) tuple
     */
    std::tuple<int, int, int, float> getJobStatistics(const std::string& jobId) const;

    // ===== Pipeline Configuration =====
    /**
     * @brief Define pipeline stages
     * @param jobId Job identifier
     * @param stages Pipeline stages
     * @return true if successful
     */
    bool definePipeline(const std::string& jobId, const std::vector<PipelineStage>& stages);

    /**
     * @brief Get pipeline for job
     * @param jobId Job identifier
     * @return Pipeline stages
     */
    std::vector<PipelineStage> getPipeline(const std::string& jobId) const;

    // ===== Deployment Configuration =====
    /**
     * @brief Set deployment configuration
     * @param jobId Job identifier
     * @param config Deployment settings
     * @return true if successful
     */
    bool setDeploymentConfig(const std::string& jobId, const DeploymentConfig& config);

    /**
     * @brief Get deployment configuration
     * @param jobId Job identifier
     * @return Deployment settings
     */
    DeploymentConfig getDeploymentConfig(const std::string& jobId) const;

    /**
     * @brief Deploy model
     * @param jobId Job identifier
     * @param runId Specific run to deploy
     * @return Deployment ID or empty if failed
     */
    std::string deployModel(const std::string& jobId, const std::string& runId);

    /**
     * @brief Rollback deployment
     * @param deploymentId Deployment to rollback
     * @param targetRunId Previous run ID to restore
     * @return true if successful
     */
    bool rollbackDeployment(const std::string& deploymentId, const std::string& targetRunId);

    // ===== Webhook Integration =====
    /**
     * @brief Register webhook
     * @param jobId Job identifier
     * @param platform "github", "gitlab", "bitbucket"
     * @param repository Repository identifier
     * @param branch Branch to monitor
     * @return Webhook URL for registration
     */
    std::string registerWebhook(const std::string& jobId, const std::string& platform,
                           const std::string& repository, const std::string& branch);

    /**
     * @brief Handle incoming webhook
     * @param webhookData JSON from webhook
     * @return Run ID if job was triggered, or empty
     */
    std::string handleWebhook(const std::string& webhookData);

    // ===== Notifications =====
    /**
     * @brief Set notification configuration
     * @param config Notification settings
     * @return true if successful
     */
    bool setNotificationConfig(const NotificationConfig& config);

    /**
     * @brief Get notification configuration
     * @return Notification settings
     */
    NotificationConfig getNotificationConfig() const;

    /**
     * @brief Send test notification
     * @return true if successfully sent
     */
    bool sendTestNotification() const;

    // ===== Artifact Management =====
    /**
     * @brief Store training artifact (model, checkpoint, etc.)
     * @param artifactId Artifact identifier
     * @param artifactPath Local file path
     * @param metadata JSON metadata
     * @return true if successful
     */
    bool storeArtifact(const std::string& artifactId, const std::string& artifactPath,
                      const std::string& metadata);

    /**
     * @brief Retrieve artifact
     * @param artifactId Artifact identifier
     * @return Local file path or empty if not found
     */
    std::string getArtifact(const std::string& artifactId) const;

    /**
     * @brief List artifacts for job
     * @param jobId Job identifier
     * @return Vector of artifact IDs
     */
    std::vector<std::string> listArtifacts(const std::string& jobId) const;

    /**
     * @brief Cleanup old artifacts
     * @param olderThanDays Delete artifacts older than N days
     * @return Number of artifacts deleted
     */
    int cleanupOldArtifacts(int olderThanDays);

    // ===== Configuration Export/Import =====
    /**
     * @brief Export all CI/CD configuration
     * @return Configuration as JSON
     */
    std::string exportConfiguration() const;

    /**
     * @brief Import CI/CD configuration
     * @param config Configuration to import
     * @return true if successful
     */
    bool importConfiguration(const std::string& config);

    /**
     * @brief Save configuration to file
     * @param filePath Output file path
     * @return true if successful
     */
    bool saveToFile(const std::string& filePath) const;

    /**
     * @brief Load configuration from file
     * @param filePath Input file path
     * @return true if successful
     */
    bool loadFromFile(const std::string& filePath);

    /**
     * @brief Get pipeline templates
     * @return Map of template_name -> template_config
     */
    std::map<std::string, std::string> getPipelineTemplates() const;

    // --- Callbacks (replaces Qt signals) ---
    using JobCb = void(*)(void* ctx, const char* jobId, const char* runId);
    using JobCompletedCb = void(*)(void* ctx, const char* runId, bool success);
    using JobFailedCb = void(*)(void* ctx, const char* runId, const char* error);
    using DeployCb = void(*)(void* ctx, const char* deploymentId);
    using DeployCompletedCb = void(*)(void* ctx, const char* deploymentId, bool success);
    using StageCb = void(*)(void* ctx, const char* runId, const char* stageName);
    using WebhookCb = void(*)(void* ctx, const char* platform, const char* action);

    void setJobQueuedCb(JobCb cb, void* ctx) { m_queuedCb = cb; m_queuedCtx = ctx; }
    void setJobCompletedCb(JobCompletedCb cb, void* ctx) { m_completedCb = cb; m_completedCtx = ctx; }
    void setJobFailedCb(JobFailedCb cb, void* ctx) { m_failedCb = cb; m_failedCtx = ctx; }
    void setDeployStartedCb(DeployCb cb, void* ctx) { m_deployStartCb = cb; m_deployStartCtx = ctx; }
    void setDeployCompletedCb(DeployCompletedCb cb, void* ctx) { m_deployCompCb = cb; m_deployCompCtx = ctx; }
    void setDeployRollbackCb(DeployCb cb, void* ctx) { m_rollbackCb = cb; m_rollbackCtx = ctx; }
    void setPipelineStageCb(StageCb cb, void* ctx) { m_stageCb = cb; m_stageCtx = ctx; }
    void setPipelineErrorCb(StageCb cb, void* ctx) { m_pipeErrCb = cb; m_pipeErrCtx = ctx; }
    void setWebhookCb(WebhookCb cb, void* ctx) { m_webhookCb = cb; m_webhookCtx = ctx; }

private:
    std::map<std::string, TrainingJob> m_jobs;              // jobId -> job config
    std::map<std::string, JobRunLog> m_runLogs;             // runId -> run log
    std::map<std::string, std::vector<PipelineStage>> m_pipelines;  // jobId -> stages
    std::map<std::string, DeploymentConfig> m_deploymentConfigs;  // jobId -> config
    NotificationConfig m_notificationConfig;
    std::map<std::string, std::string> m_artifacts;         // artifactId -> metadata

    std::string generateJobId();
    std::string generateRunId();
    std::string generateDeploymentId();

    // Callback pointers
    JobCb m_queuedCb = nullptr;
    void* m_queuedCtx = nullptr;
    JobCompletedCb m_completedCb = nullptr;
    void* m_completedCtx = nullptr;
    JobFailedCb m_failedCb = nullptr;
    void* m_failedCtx = nullptr;
    DeployCb m_deployStartCb = nullptr;
    void* m_deployStartCtx = nullptr;
    DeployCompletedCb m_deployCompCb = nullptr;
    void* m_deployCompCtx = nullptr;
    DeployCb m_rollbackCb = nullptr;
    void* m_rollbackCtx = nullptr;
    StageCb m_stageCb = nullptr;
    void* m_stageCtx = nullptr;
    StageCb m_pipeErrCb = nullptr;
    void* m_pipeErrCtx = nullptr;
    WebhookCb m_webhookCb = nullptr;
    void* m_webhookCtx = nullptr;

>>>>>>> origin/main
    ShowCallback m_showCb = nullptr;
    void* m_showCtx = nullptr;
};
