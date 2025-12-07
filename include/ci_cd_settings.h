#pragma once

#include <QString>
#include <QObject>
#include <QJsonObject>
#include <vector>
#include <map>

class QComboBox;
class QCheckBox;
class QSpinBox;
class QLineEdit;
class QTabWidget;

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
class CICDSettings : public QObject
{
    Q_OBJECT

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
        QString jobId;
        QString jobName;
        QString description;
        QString modelName;
        QString datasetPath;
        int epochs;
        int batchSize;
        float learningRate;
        QString optimizer;
        int numGPUs;
        QString priority;   // "low", "normal", "high"
        TriggerType trigger;
        QString cronSchedule;  // For scheduled jobs (e.g., "0 2 * * *" = 2 AM daily)
        bool enabled;
        qint64 createdAt;
        qint64 lastRunAt;
        int successCount;
        int failureCount;
    };

    struct PipelineStage {
        QString stageName;
        QString description;
        bool enabled;
        int timeoutSeconds;
        std::vector<QString> commands;  // Shell commands to run
        bool continueOnError;
    };

    struct DeploymentConfig {
        QString modelPath;
        DeploymentStrategy strategy;
        float canaryPercentage;     // 0.0 to 1.0, for Canary strategy
        QString targetEnvironment;  // "staging", "production"
        bool requireApproval;
        std::vector<QString> approvers;
        bool rollbackOnFailure;
        int maxConcurrentRequests;
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
