#ifndef AI_WORKERS_H
#define AI_WORKERS_H

#include <QObject>
#include <QThread>
#include <QStringList>
#include <QMutex>
#include <QWaitCondition>
#include <QTimer>
#include <QElapsedTimer>
#include <QAtomicInt>
#include <QDir>
#include <memory>
#include "ai_digestion_engine.hpp"
#include <limits>

class AIDigestionEngine;
class AITrainingPipeline;
class AIMetricsCollector;

// Forward declarations
struct DigestionConfig;
struct KnowledgeRepresentation;
struct TrainingDataset;

// Agent type enumeration
enum class AgentType {
    None,
    CodeGenerator,
    DataAnalyst,
    ModelOptimizer,
    AutoML,
    Custom
};

// Build mode enumeration
enum class BuildMode {
    Model,          // Traditional model-based building
    Agent,          // Agent-based building
    Hybrid          // Combined model and agent approach
};

/**
 * Background worker for content digestion operations
 * Runs in separate thread to avoid blocking UI
 * Note: This is the enhanced version with progress monitoring
 */
class AIDigestionWorker : public QObject {
    Q_OBJECT

public:
    enum class State {
        Idle,
        Running,
        Paused,
        Stopping,
        Finished,
        Error
    };

    struct Progress {
        int currentFile = 0;
        int totalFiles = 0;
        QString currentFileName;
        double percentage = 0.0;
        qint64 elapsedTime = 0;
        qint64 estimatedTime = 0;
        QString status;
    };

    explicit AIDigestionWorker(AIDigestionEngine* engine, QObject* parent = nullptr);
    ~AIDigestionWorker();

    // Control methods
    void startDigestion(const QStringList& filePaths);
    void pauseDigestion();
    void resumeDigestion();
    void stopDigestion();
    
    // State queries
    State currentState() const;
    Progress currentProgress() const;
    bool isRunning() const;
    bool isPaused() const;
    
signals:
    void progressChanged(const Progress& progress);
    void fileStarted(const QString& fileName);
    void fileCompleted(const QString& fileName, bool success);
    void digestionCompleted(bool success, const QString& message);
    void errorOccurred(const QString& error);
    void stateChanged(State newState);

public slots:
    void processFiles();

private slots:
    void updateProgress();

private:
    void setState(State newState);
    void processNextFile();
    void calculateTimeEstimates();
    void emitProgress();
    
    AIDigestionEngine* m_engine;
    QStringList m_filePaths;
    QStringList m_remainingFiles;
    State m_state;
    Progress m_progress;
    
    // Thread synchronization
    mutable QMutex m_mutex;
    QWaitCondition m_pauseCondition;
    QAtomicInt m_shouldStop;
    
    // Timing
    QElapsedTimer m_elapsedTimer;
    QTimer* m_progressTimer;
    QList<qint64> m_fileProcessingTimes;
    
    // Results
    QStringList m_processedFiles;
    QStringList m_failedFiles;
};

/**
 * Background worker for model training operations
 * Runs in separate thread to avoid blocking UI
 * Note: This is the enhanced version with progress monitoring
 */
class AITrainingWorker : public QObject {
    Q_OBJECT

public:
    enum class State {
        Idle,
        Preparing,
        Training,
        Validating,
        Paused,
        Stopping,
        Finished,
        Error
    };

    struct Progress {
        int currentEpoch = 0;
        int totalEpochs = 0;
        int currentBatch = 0;
        int totalBatches = 0;
        double loss = 0.0;
        double accuracy = 0.0;
        double learningRate = 0.0;
        double percentage = 0.0;
        qint64 elapsedTime = 0;
        qint64 estimatedTime = 0;
        QString status;
        QString phase;
    };

    struct TrainingConfig {
        int epochs = 10;
        int batchSize = 32;
        double learningRate = 0.001;
        double validationSplit = 0.2;
        QString optimizer = "adam";
        QString lossFunction = "categorical_crossentropy";
        QStringList metrics = {"accuracy"};
        bool useEarlyStopping = true;
        int patience = 5;
        bool saveCheckpoints = true;
        int checkpointFrequency = 1;
        
        // Agent-specific configuration
        BuildMode buildMode = BuildMode::Model;
        AgentType agentType = AgentType::None;
        QString agentConfig;
        int agentIterations = 100;
        double agentExplorationRate = 0.1;
        bool useAgentOptimization = false;
    };

    explicit AITrainingWorker(AITrainingPipeline* pipeline, QObject* parent = nullptr);
    ~AITrainingWorker();

    // Control methods
    void startTraining(const TrainingDataset& dataset, 
                      const QString& modelName,
                      const QString& outputPath,
                      const TrainingConfig& config);
    void pauseTraining();
    void resumeTraining();
    void stopTraining();
    
    // State queries
    State currentState() const;
    Progress currentProgress() const;
    bool isRunning() const;
    bool isPaused() const;
    
signals:
    void progressChanged(const Progress& progress);
    void epochStarted(int epoch);
    void epochCompleted(int epoch, double loss, double accuracy);
    void batchCompleted(int batch, double batchLoss);
    void validationCompleted(double valLoss, double valAccuracy);
    void checkpointSaved(const QString& path);
    void trainingCompleted(bool success, const QString& modelPath);
    void errorOccurred(const QString& error);
    void stateChanged(State newState);

public slots:
    void processTraining();

private slots:
    void updateProgress();
    void handleEpochComplete();
    void handleBatchComplete();
    void handleValidation();

private:
    void setState(State newState);
    void calculateTimeEstimates();
    void emitProgress();
    void saveCheckpoint();
    bool shouldEarlyStop();
    void cleanupOldCheckpoints(const QDir& directory, int keepCount);
    
    // Core training stages
    bool prepareTraining();
    void performEpoch();
    void performValidation(double trainingLoss, double trainingAccuracy);
    void finishTraining();
    
    // Agent-specific methods
    void performAgentEpoch();
    void performHybridEpoch();
    bool isAgentMode() const;
    bool isHybridMode() const;
    QString agentStatusMessage() const;
    
    AITrainingPipeline* m_pipeline;
    TrainingDataset m_dataset;
    QString m_modelName;
    QString m_outputPath;
    TrainingConfig m_config;
    State m_state;
    Progress m_progress;
    
    // Thread synchronization
    mutable QMutex m_mutex;
    QWaitCondition m_pauseCondition;
    QAtomicInt m_shouldStop;
    
    // Timing
    QElapsedTimer m_elapsedTimer;
    QTimer* m_progressTimer;
    QList<qint64> m_epochTimes;
    
    // Training state
    QList<double> m_trainingLosses;
    QList<double> m_validationLosses;
    QList<double> m_trainingAccuracies;
    QList<double> m_validationAccuracies;
    int m_bestEpoch;
    double m_bestValidationLoss;
    int m_patienceCounter;
    
    // Agent-specific state
    QList<double> m_agentRewards;
    QList<double> m_agentExplorationRates;
    int m_agentIterationCount;
    QString m_agentState;
    
    // Results
    QString m_finalModelPath;
    bool m_trainingSuccessful;
};

/**
 * Manager class for coordinating background workers
 * Handles thread management and worker lifecycle
 */
class AIWorkerManager : public QObject {
    Q_OBJECT

public:
    explicit AIWorkerManager(QObject* parent = nullptr);
    ~AIWorkerManager();

    // Worker creation and management
    AIDigestionWorker* createDigestionWorker(AIDigestionEngine* engine);
    AITrainingWorker* createTrainingWorker(AITrainingPipeline* pipeline);
    
    // Agent-specific worker creation
    AITrainingWorker* createAgentWorker(AITrainingPipeline* pipeline, AgentType agentType);
    AITrainingWorker* createHybridWorker(AITrainingPipeline* pipeline, AgentType agentType);
    
    // Worker control
    void startDigestionWorker(AIDigestionWorker* worker, const QStringList& files);
    void startTrainingWorker(AITrainingWorker* worker, 
                           const TrainingDataset& dataset,
                           const QString& modelName,
                           const QString& outputPath,
                           const AITrainingWorker::TrainingConfig& config);
    
    void stopAllWorkers();
    void pauseAllWorkers();
    void resumeAllWorkers();
    
    // State queries
    bool hasActiveWorkers() const;
    QList<AIDigestionWorker*> activeDigestionWorkers() const;
    QList<AITrainingWorker*> activeTrainingWorkers() const;

signals:
    void workerStarted(QObject* worker);
    void workerFinished(QObject* worker);
    void allWorkersFinished();

private slots:
    void onWorkerFinished();
    void onWorkerError(const QString& error);

private:
    void cleanupFinishedWorkers();
    void moveWorkerToThread(QObject* worker, QThread* thread);
    
    QList<QThread*> m_threads;
    QList<AIDigestionWorker*> m_digestionWorkers;
    QList<AITrainingWorker*> m_trainingWorkers;
    mutable QMutex m_workersMutex;
    
    // Configuration
    int m_maxConcurrentWorkers;
    int m_threadPoolSize;
};

#endif // AI_WORKERS_H