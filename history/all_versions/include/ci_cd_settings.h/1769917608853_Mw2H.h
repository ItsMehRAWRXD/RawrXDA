#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <tuple>
#include <mutex>

/*!
 * @class CICDSettings
 * @brief Configure CI/CD pipelines for automated training and deployment
 * STL-only replacement for Qt-based settings.
 */
class CICDSettings
{
public:
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
        int epochs = 10;
        int batchSize = 32;
        float learningRate = 0.001f;
        std::string optimizer = "adam";
        int numGPUs = 1;
        std::string priority = "normal";
        TriggerType trigger = TriggerType::Manual;
        std::string cronSchedule;
        bool enabled = true;
        int64_t createdAt = 0;
        int64_t lastRunAt = 0;
        int successCount = 0;
        int failureCount = 0;
    };

    struct PipelineStage {
        std::string stageName;
        std::string description;
        bool enabled = true;
        int timeoutSeconds = 600;
        std::vector<std::string> commands;
        bool continueOnError = false;
    };

    struct DeploymentConfig {
        std::string modelPath;
        DeploymentStrategy strategy = DeploymentStrategy::Immediate;
        float canaryPercentage = 0.1f;
        std::string targetEnvironment = "staging";
        bool requireApproval = false;
        std::vector<std::string> approvers;
        bool rollbackOnFailure = true;
        int maxConcurrentRequests = 100;
    };

    struct JobRunLog {
        std::string jobId;
        std::string runId;
        JobStatus status;
        int64_t startTime;
        int64_t endTime;
        std::string outputLog;
        std::string errorMessage;
        float accuracy;
    };

    struct NotificationConfig {
        bool enableSlack = false;
        std::string slackWebhookUrl;
        bool enableEmail = false;
        std::string emailAddress;
        bool notifyOnSuccess = true;
        bool notifyOnFailure = true;
        bool notifyOnStart = false;
    };

    CICDSettings(void* parent = nullptr); // Parent ignored for STL compatibility
    ~CICDSettings();

    // Job Management
    bool createJob(const TrainingJob& job);
    bool updateJob(const std::string& jobId, const TrainingJob& job);
    TrainingJob getJob(const std::string& jobId) const;
    std::vector<TrainingJob> listJobs() const;
    bool deleteJob(const std::string& jobId);
    bool setJobEnabled(const std::string& jobId, bool enabled);

    // Execution
    std::string queueJob(const std::string& jobId); // Returns runId
    bool cancelJob(const std::string& runId);
    JobStatus getJobStatus(const std::string& runId) const;
    std::vector<JobRunLog> getJobHistory(const std::string& jobId, int limit = 10) const;
    std::tuple<int, int, int, float> getJobStatistics(const std::string& jobId) const; // Runs, Success, Fail, AvgDuration

    // Pipelines
    bool definePipeline(const std::string& jobId, const std::vector<PipelineStage>& stages);
    std::vector<PipelineStage> getPipeline(const std::string& jobId) const;
    std::map<std::string, void*> getPipelineTemplates() const; // Returns JSON-compatible maps

    // Deployment
    bool setDeploymentConfig(const std::string& jobId, const DeploymentConfig& config);
    DeploymentConfig getDeploymentConfig(const std::string& jobId) const;
    std::string deployModel(const std::string& jobId, const std::string& runId); // Returns deploymentId
    bool rollbackDeployment(const std::string& deploymentId, const std::string& targetRunId);
    
    // Webhooks
    std::string registerWebhook(const std::string& jobId, const std::string& platform, 
                               const std::string& repository, const std::string& branch);
    std::string handleWebhook(const void*& webhookData); // Using void* as placeholder for parsed JSON in current context

    // Notification
    bool setNotificationConfig(const NotificationConfig& config);
    NotificationConfig getNotificationConfig() const;
    bool sendTestNotification() const;

    // Artifacts
    bool storeArtifact(const std::string& artifactId, const std::string& artifactPath, const void*& metadata);
    std::string getArtifact(const std::string& artifactId) const;
    std::vector<std::string> listArtifacts(const std::string& jobId) const;
    int cleanupOldArtifacts(int olderThanDays);

    // Config Persistence
    void* exportConfiguration() const;          // Returns void* (JSON)
    bool importConfiguration(const void*& config);
    bool saveToFile(const std::string& filePath) const;
    bool loadFromFile(const std::string& filePath);

signals: // Mock simple signals
    void jobStarted(const std::string& jobId, const std::string& runId);
    void jobCompleted(const std::string& jobId, const std::string& runId, bool success);
    void jobFailed(const std::string& jobId, const std::string& runId, const std::string& error);
    void deploymentStarted(const std::string& deploymentId);
    void deploymentCompleted(const std::string& deploymentId, bool success);
    void deploymentRolledBack(const std::string& deploymentId);
    void webhookReceived(const std::string& platform, const std::string& event);

private:
    std::string generateJobId();
    std::string generateRunId();
    std::string generateDeploymentId();

    mutable std::mutex m_mutex;
    std::map<std::string, TrainingJob> m_jobs;
    std::map<std::string, JobRunLog> m_runLogs;
    std::map<std::string, std::vector<PipelineStage>> m_pipelines;
    std::map<std::string, DeploymentConfig> m_deploymentConfigs;
    std::map<std::string, void*> m_artifacts; // Stores generic metadata
    
    NotificationConfig m_notificationConfig;
};

    };

    struct NotificationConfig {
        bool enableSlack;
        QString slackWebhookUrl;
        QString slackChannel;
        bool enableEmail;
        std::vector<QString> emailRecipients;
        bool notifyOnSuccess;
        bool notifyOnFailure;
        bool notifyOnStart;
    };

    struct JobRunLog {
        QString jobId;
        QString runId;
        JobStatus status;
        qint64 startTime;
        qint64 endTime;
        QString logOutput;
        float accuracy;
        float loss;
        QString artifactPath;
        QString errorMessage;
    };

    // Constructor
    explicit CICDSettings(QObject* parent = nullptr);
    ~CICDSettings() override;

    // ===== Job Management =====
    /**
     * @brief Create training job
     * @param job Job configuration
     * @return true if successful
     */
    bool createJob(const TrainingJob& job);

    /**
     * @brief Update existing job
     * @param jobId Job identifier
     * @param job Updated configuration
     * @return true if successful
     */
    bool updateJob(const QString& jobId, const TrainingJob& job);

    /**
     * @brief Get job by ID
     * @param jobId Job identifier
     * @return Job configuration or empty if not found
     */
    TrainingJob getJob(const QString& jobId) const;

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
    bool deleteJob(const QString& jobId);

    /**
     * @brief Enable/disable job
     * @param jobId Job identifier
     * @param enabled Enable state
     * @return true if successful
     */
    bool setJobEnabled(const QString& jobId, bool enabled);

    // ===== Job Execution =====
    /**
     * @brief Queue job for execution
     * @param jobId Job identifier
     * @return Run ID or empty if failed
     */
    QString queueJob(const QString& jobId);

    /**
     * @brief Cancel running job
     * @param runId Run identifier
     * @return true if successful
     */
    bool cancelJob(const QString& runId);

    /**
     * @brief Retry failed job
     * @param runId Original run ID
     * @return New run ID or empty if failed
     */
    QString retryJob(const QString& runId);

    /**
     * @brief Get job execution logs
     * @param runId Run identifier
     * @return Execution log entry
     */
    JobRunLog getJobRunLog(const QString& runId) const;

    /**
     * @brief List job run history
     * @param jobId Job identifier
     * @param limit Number of recent runs to return
     * @return List of run logs
     */
    std::vector<JobRunLog> getJobRunHistory(const QString& jobId, int limit = 20) const;

    /**
     * @brief Get job statistics
     * @param jobId Job identifier
     * @return (totalRuns, successCount, failureCount, avgDuration) tuple
     */
    std::tuple<int, int, int, float> getJobStatistics(const QString& jobId) const;

    // ===== Pipeline Configuration =====
    /**
     * @brief Define pipeline stages
     * @param jobId Job identifier
     * @param stages Pipeline stages
     * @return true if successful
     */
    bool definePipeline(const QString& jobId, const std::vector<PipelineStage>& stages);

    /**
     * @brief Get pipeline for job
     * @param jobId Job identifier
     * @return Pipeline stages
     */
    std::vector<PipelineStage> getPipeline(const QString& jobId) const;

    // ===== Deployment Configuration =====
    /**
     * @brief Set deployment configuration
     * @param jobId Job identifier
     * @param config Deployment settings
     * @return true if successful
     */
    bool setDeploymentConfig(const QString& jobId, const DeploymentConfig& config);

    /**
     * @brief Get deployment configuration
     * @param jobId Job identifier
     * @return Deployment settings
     */
    DeploymentConfig getDeploymentConfig(const QString& jobId) const;

    /**
     * @brief Deploy model
     * @param jobId Job identifier
     * @param runId Specific run to deploy
     * @return Deployment ID or empty if failed
     */
    QString deployModel(const QString& jobId, const QString& runId);

    /**
     * @brief Rollback deployment
     * @param deploymentId Deployment to rollback
     * @param targetRunId Previous run ID to restore
     * @return true if successful
     */
    bool rollbackDeployment(const QString& deploymentId, const QString& targetRunId);

    // ===== Webhook Integration =====
    /**
     * @brief Register webhook
     * @param jobId Job identifier
     * @param platform "github", "gitlab", "bitbucket"
     * @param repository Repository identifier
     * @param branch Branch to monitor
     * @return Webhook URL for registration
     */
    QString registerWebhook(const QString& jobId, const QString& platform,
                           const QString& repository, const QString& branch);

    /**
     * @brief Handle incoming webhook
     * @param webhookData JSON from webhook
     * @return Run ID if job was triggered, or empty
     */
    QString handleWebhook(const QJsonObject& webhookData);

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
    bool storeArtifact(const QString& artifactId, const QString& artifactPath,
                      const QJsonObject& metadata);

    /**
     * @brief Retrieve artifact
     * @param artifactId Artifact identifier
     * @return Local file path or empty if not found
     */
    QString getArtifact(const QString& artifactId) const;

    /**
     * @brief List artifacts for job
     * @param jobId Job identifier
     * @return Vector of artifact IDs
     */
    std::vector<QString> listArtifacts(const QString& jobId) const;

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
    QJsonObject exportConfiguration() const;

    /**
     * @brief Import CI/CD configuration
     * @param config Configuration to import
     * @return true if successful
     */
    bool importConfiguration(const QJsonObject& config);

    /**
     * @brief Save configuration to file
     * @param filePath Output file path
     * @return true if successful
     */
    bool saveToFile(const QString& filePath) const;

    /**
     * @brief Load configuration from file
     * @param filePath Input file path
     * @return true if successful
     */
    bool loadFromFile(const QString& filePath);

    /**
     * @brief Get pipeline templates
     * @return Map of template_name -> template_config
     */
    std::map<QString, QJsonObject> getPipelineTemplates() const;

signals:
    /// Emitted when job is queued
    void jobQueued(const QString& jobId, const QString& runId);

    /// Emitted when job run completes
    void jobCompleted(const QString& runId, bool success);

    /// Emitted when job fails
    void jobFailed(const QString& runId, const QString& error);

    /// Emitted when deployment starts
    void deploymentStarted(const QString& deploymentId);

    /// Emitted when deployment completes
    void deploymentCompleted(const QString& deploymentId, bool success);

    /// Emitted when rollback occurs
    void deploymentRolledBack(const QString& deploymentId);

    /// Emitted when pipeline stage completes
    void pipelineStageCompleted(const QString& runId, const QString& stageName);

    /// Emitted on pipeline error
    void pipelineError(const QString& runId, const QString& error);

    /// Emitted when webhook is received
    void webhookReceived(const QString& platform, const QString& action);

private:
    std::map<QString, TrainingJob> m_jobs;              // jobId -> job config
    std::map<QString, JobRunLog> m_runLogs;             // runId -> run log
    std::map<QString, std::vector<PipelineStage>> m_pipelines;  // jobId -> stages
    std::map<QString, DeploymentConfig> m_deploymentConfigs;  // jobId -> config
    NotificationConfig m_notificationConfig;
    std::map<QString, QJsonObject> m_artifacts;         // artifactId -> metadata

    QString generateJobId();
    QString generateRunId();
    QString generateDeploymentId();
};
