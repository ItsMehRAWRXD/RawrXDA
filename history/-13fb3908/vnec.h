// Model Training Pipeline - Fine-tuning with orchestration and monitoring
#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantMap>
#include <QStringList>
#include <QDateTime>
#include <QTimer>
#include <QElapsedTimer>
#include <QMutex>
#include <QReadWriteLock>
#include <QQueue>
#include <QSet>
#include <QHash>
#include <QUuid>
#include <QUrl>
#include <QThread>
#include <QThreadPool>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <memory>
#include <functional>
#include <unordered_map>
#include <vector>

// Forward declarations
class AgenticExecutor;
class AdvancedPlanningEngine;
class ToolCompositionFramework;
class ErrorAnalysisSystem;
class InferenceEngine;

/**
 * @brief Training task types
 */
enum class TrainingTaskType : int {
    FineTuning,      // Fine-tune existing model
    PreTraining,     // Pre-train from scratch
    Transfer,        // Transfer learning
    Distillation,    // Knowledge distillation
    Quantization,    // Model quantization
    Pruning,         // Model pruning
    Optimization,    // Performance optimization
    Validation,      // Model validation
    Evaluation,      // Model evaluation
    Deployment       // Deployment preparation
};

/**
 * @brief Training status enumeration
 */
enum class TrainingStatus : int {
    Pending,         // Waiting to start
    Initializing,    // Setting up training environment
    Running,         // Actively training
    Paused,          // Temporarily paused
    Completed,       // Successfully completed
    Failed,          // Training failed
    Cancelled,       // User cancelled
    Validating,      // Validation in progress
    Optimizing,      // Post-training optimization
    Deploying        // Deployment in progress
};

/**
 * @brief Model architecture specification
 */
struct ModelArchitecture {
    QString name;                   // Architecture name (GPT, BERT, etc.)
    QString version;                // Architecture version
    QJsonObject parameters;         // Model parameters
    QStringList supportedTasks;     // Supported task types
    QJsonObject hyperparameters;    // Default hyperparameters
    QJsonObject constraints;        // Training constraints
    qint64 baseModelSize = 0;       // Base model size in bytes
    int maxContextLength = 0;       // Maximum context length
    QStringList requiredDataFormats; // Required data formats
};

/**
 * @brief Training dataset specification
 */
struct TrainingDataset {
    QString datasetId;
    QString name;
    QString description;
    QUrl sourceUrl;
    QString localPath;
    QString format;                 // json, csv, parquet, etc.
    qint64 sizeBytes = 0;
    int recordCount = 0;
    QJsonObject schema;
    QStringList columns;
    QJsonObject statistics;
    QJsonObject preprocessing;
    QDateTime lastUpdated;
    QString license;
    QStringList tags;
    bool isValidated = false;
    QJsonObject validationResults;
};

/**
 * @brief Training configuration
 */
struct TrainingConfig {
    QString configId;
    QString name;
    ModelArchitecture architecture;
    TrainingDataset dataset;
    TrainingTaskType taskType = TrainingTaskType::FineTuning;
    
    // Training hyperparameters
    double learningRate = 0.001;
    int batchSize = 32;
    int epochs = 10;
    double dropoutRate = 0.1;
    QString optimizer = "adam";
    QString lossFunction = "cross_entropy";
    QJsonObject schedulerConfig;
    QJsonObject regularization;
    
    // Hardware configuration
    QStringList gpuIds;
    int maxMemoryMB = 0;
    bool mixedPrecision = false;
    bool distributedTraining = false;
    int numWorkers = 1;
    
    // Monitoring and checkpointing
    int checkpointInterval = 1000;
    QString checkpointPath;
    QStringList metrics;
    int evaluationInterval = 500;
    int earlyStoppingPatience = 5;
    double minImprovement = 0.001;
    
    // Resource limits
    qint64 maxTrainingTimeMs = 0;   // 0 = no limit
    qint64 maxMemoryUsage = 0;      // 0 = no limit
    int maxCpuCores = 0;            // 0 = no limit
    
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& json);
};

/**
 * @brief Training progress tracking
 */
struct TrainingProgress {
    QString trainingId;
    TrainingStatus status = TrainingStatus::Pending;
    QDateTime startTime;
    QDateTime lastUpdateTime;
    qint64 elapsedTimeMs = 0;
    qint64 remainingTimeMs = 0;
    
    // Progress metrics
    int currentEpoch = 0;
    int totalEpochs = 0;
    int currentBatch = 0;
    int totalBatches = 0;
    double progress = 0.0;          // 0.0 to 1.0
    
    // Training metrics
    QJsonObject currentMetrics;
    QJsonObject bestMetrics;
    QJsonObject metricHistory;
    double currentLoss = 0.0;
    double bestLoss = 1e6;
    double currentAccuracy = 0.0;
    double bestAccuracy = 0.0;
    
    // Resource usage
    qint64 memoryUsageMB = 0;
    double cpuUsagePercent = 0.0;
    double gpuUsagePercent = 0.0;
    double gpuMemoryUsageMB = 0.0;
    double throughputSamplesPerSec = 0.0;
    
    // Checkpointing
    QString lastCheckpointPath;
    QDateTime lastCheckpointTime;
    int checkpointCount = 0;
    
    QString statusMessage;
    QStringList warnings;
    QStringList errors;
    
    QJsonObject toJson() const;
};

/**
 * @brief Model version information
 */
struct TrainingModelVersion {
    QString versionId;
    QString modelName;
    QString version;                // Semantic version
    QDateTime createdAt;
    QString createdBy;
    QString description;
    QStringList tags;
    
    // Model metadata
    ModelArchitecture architecture;
    TrainingConfig trainingConfig;
    QJsonObject performanceMetrics;
    QString modelPath;
    qint64 modelSizeBytes = 0;
    QString checksum;
    
    // Deployment information
    bool isDeployed = false;
    QStringList deploymentTargets;
    QDateTime lastDeployed;
    QJsonObject deploymentConfig;
    
    // Quality metrics
    double accuracyScore = 0.0;
    double f1Score = 0.0;
    double precisionScore = 0.0;
    double recallScore = 0.0;
    QJsonObject customMetrics;
    
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& json);
};

/**
 * @brief Advanced Model Training Pipeline
 */
class ModelTrainingPipeline : public QObject {
    Q_OBJECT

public:
    explicit ModelTrainingPipeline(QObject* parent = nullptr);
    ~ModelTrainingPipeline();

    // Initialization
    void initialize(AgenticExecutor* executor, AdvancedPlanningEngine* planner,
                   ToolCompositionFramework* toolFramework, ErrorAnalysisSystem* errorSystem,
                   InferenceEngine* inference);
    bool isInitialized() const { return m_initialized; }

    // Model architecture management
    bool registerArchitecture(const ModelArchitecture& architecture);
    bool removeArchitecture(const QString& architectureName);
    ModelArchitecture getArchitecture(const QString& architectureName) const;
    QStringList getAvailableArchitectures() const;
    QStringList getSupportedTaskTypes(const QString& architectureName) const;

    // Dataset management
    QString registerDataset(const TrainingDataset& dataset);
    bool removeDataset(const QString& datasetId);
    TrainingDataset getDataset(const QString& datasetId) const;
    QStringList getAvailableDatasets() const;
    QString validateDataset(const QString& datasetId);
    QJsonObject getDatasetStatistics(const QString& datasetId) const;

    // Training configuration
    QString createTrainingConfig(const TrainingConfig& config);
    bool updateTrainingConfig(const QString& configId, const TrainingConfig& config);
    bool removeTrainingConfig(const QString& configId);
    TrainingConfig getTrainingConfig(const QString& configId) const;
    QStringList getTrainingConfigs() const;
    bool validateTrainingConfig(const QString& configId);

    // Training execution
    QString startTraining(const QString& configId);
    QString startTrainingWithConfig(const TrainingConfig& config);
    bool pauseTraining(const QString& trainingId);
    bool resumeTraining(const QString& trainingId);
    bool stopTraining(const QString& trainingId);
    bool cancelTraining(const QString& trainingId);

    // Training monitoring
    TrainingProgress getTrainingProgress(const QString& trainingId) const;
    QJsonObject getTrainingMetrics(const QString& trainingId) const;
    QStringList getActiveTrainings() const;
    QStringList getCompletedTrainings() const;
    QString getTrainingLogs(const QString& trainingId) const;

    // Model versioning
    QString saveModelVersion(const QString& trainingId, const QString& version, 
                           const QString& description = QString());
    bool removeModelVersion(const QString& versionId);
    TrainingModelVersion getModelVersion(const QString& versionId) const;
    QStringList getModelVersions(const QString& modelName = QString()) const;
    QString getLatestModelVersion(const QString& modelName) const;
    QString compareModelVersions(const QString& version1Id, const QString& version2Id) const;

    // Model evaluation and validation
    QString evaluateModel(const QString& versionId, const QString& testDatasetId);
    QJsonObject getEvaluationResults(const QString& evaluationId) const;
    bool validateModel(const QString& versionId);
    QJsonObject getValidationResults(const QString& versionId) const;
    QJsonObject generateModelReport(const QString& versionId) const;

    // Model optimization
    QString optimizeModel(const QString& versionId, const QJsonObject& optimizationConfig);
    QString quantizeModel(const QString& versionId, const QString& quantizationMethod = "int8");
    QString pruneModel(const QString& versionId, double pruningRatio = 0.1);
    QString distillModel(const QString& teacherVersionId, const QString& studentConfigId);

    // Deployment preparation
    QString prepareDeployment(const QString& versionId, const QJsonObject& deploymentConfig);
    bool packageModel(const QString& versionId, const QString& outputPath);
    QJsonObject generateDeploymentManifest(const QString& versionId) const;
    bool testDeployment(const QString& versionId, const QJsonObject& testConfig);

    // Advanced features
    QString scheduleTraining(const QString& configId, const QDateTime& scheduledTime);
    QString createHyperparameterSweep(const QString& baseConfigId, const QJsonObject& sweepConfig);
    QStringList getRecommendedHyperparameters(const QString& architectureName, 
                                            const QString& datasetId) const;
    QJsonObject analyzeTrainingTrends() const;

    // Resource management
    QJsonObject getResourceUsage() const;
    QStringList getAvailableGpus() const;
    bool reserveResources(const QString& trainingId, const QJsonObject& resourceSpec);
    bool releaseResources(const QString& trainingId);
    QJsonObject optimizeResourceAllocation() const;

    // Configuration and settings
    void loadConfiguration(const QJsonObject& config);
    QJsonObject saveConfiguration() const;
    void setMaxConcurrentTrainings(int maxTrainings) { m_maxConcurrentTrainings = maxTrainings; }
    void setDefaultCheckpointInterval(int interval) { m_defaultCheckpointInterval = interval; }

    // Import/Export
    QString exportTrainingHistory(const QString& format = "json") const;
    QString exportModelArchive(const QString& versionId, const QString& format = "onnx") const;
    bool importModel(const QString& filePath, const QJsonObject& metadata);
    QString generateTrainingReport() const;

public slots:
    void onTrainingProgressUpdate(const QString& trainingId, const TrainingProgress& progress);
    void onTrainingCompleted(const QString& trainingId, bool success);
    void onResourceStateChanged(const QJsonObject& resourceState);
    void performPeriodicMaintenance();

signals:
    void trainingStarted(const QString& trainingId);
    void trainingProgressChanged(const QString& trainingId, double progress);
    void trainingCompleted(const QString& trainingId, bool success);
    void trainingFailed(const QString& trainingId, const QString& error);
    void modelVersionCreated(const QString& versionId);
    void evaluationCompleted(const QString& evaluationId, const QJsonObject& results);
    void resourceLimitReached(const QString& resource, double usage);
    void checkpointCreated(const QString& trainingId, const QString& checkpointPath);
    void optimizationSuggested(const QString& suggestion);

private slots:
    void processTrainingQueue();
    void updateTrainingProgress();
    void handleTrainingEvents();
    void performResourceMonitoring();

private:
    // Core components
    AgenticExecutor* m_agenticExecutor = nullptr;
    AdvancedPlanningEngine* m_planningEngine = nullptr;
    ToolCompositionFramework* m_toolFramework = nullptr;
    ErrorAnalysisSystem* m_errorSystem = nullptr;
    InferenceEngine* m_inferenceEngine = nullptr;

    // Data storage
    std::unordered_map<QString, ModelArchitecture> m_architectures;
    std::unordered_map<QString, TrainingDataset> m_datasets;
    std::unordered_map<QString, TrainingConfig> m_configs;
    std::unordered_map<QString, TrainingProgress> m_trainingProgress;
    std::unordered_map<QString, ModelVersion> m_modelVersions;
    std::unordered_map<QString, QJsonObject> m_evaluationResults;
    mutable QReadWriteLock m_dataLock;

    // Training execution
    QQueue<QString> m_trainingQueue;
    QSet<QString> m_activeTrainings;
    std::unordered_map<QString, QFutureWatcher<void>*> m_trainingWatchers;
    mutable QMutex m_executionMutex;

    // Configuration
    bool m_initialized = false;
    int m_maxConcurrentTrainings = 2;
    int m_defaultCheckpointInterval = 1000;
    QString m_modelsDirectory;
    QString m_checkpointsDirectory;
    QString m_logsDirectory;

    // Resource monitoring
    QJsonObject m_resourceState;
    QJsonObject m_resourceLimits;
    QElapsedTimer m_uptimeTimer;

    // Timers
    QTimer* m_processingTimer;
    QTimer* m_progressTimer;
    QTimer* m_maintenanceTimer;
    QTimer* m_resourceTimer;

    // Internal methods
    void initializeComponents();
    void setupTimers();
    void setupDirectories();
    void connectSignals();
    void loadBuiltInArchitectures();
    
    // Training execution
    QString generateTrainingId();
    QString generateVersionId();
    QString generateConfigId();
    void executeTraining(const QString& trainingId);
    void finalizeTraining(const QString& trainingId, bool success);
    
    // Resource management
    bool allocateResources(const QString& trainingId, const TrainingConfig& config);
    void deallocateResources(const QString& trainingId);
    QJsonObject getCurrentResourceUsage();
    bool checkResourceAvailability(const TrainingConfig& config);
    
    // Model management
    QString saveModel(const QString& trainingId, const QString& modelPath);
    bool validateModelFile(const QString& modelPath);
    QString calculateModelChecksum(const QString& modelPath);
    
    // Utility methods
    QJsonObject createTrainingEnvironment(const TrainingConfig& config);
    bool setupTrainingDirectories(const QString& trainingId);
    void cleanupTrainingArtifacts(const QString& trainingId);
    QStringList getRecommendedGpus(const TrainingConfig& config);
};
