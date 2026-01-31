#ifndef AI_WORKERS_H
#define AI_WORKERS_H

#include <memory>
#include "ai_digestion_engine.hpp"
#include <limits>

class AIDigestionEngine;
class AITrainingPipeline;
class AIMetricsCollector;

// Forward declarations
struct DigestionConfig;
struct KnowledgeRepresentation;
struct AIDigestionDataset;

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
class AIDigestionWorker  {

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
        std::string currentFileName;
        double percentage = 0.0;
        int64_t elapsedTime = 0;
        int64_t estimatedTime = 0;
        std::string status;
    };

    explicit AIDigestionWorker(AIDigestionEngine* engine);
    ~AIDigestionWorker();

    // Control methods
    void startDigestion(const std::stringList& filePaths);
    void pauseDigestion();
    void resumeDigestion();
    void stopDigestion();
    
    // State queries
    State currentState() const;
    Progress currentProgress() const;
    bool isRunning() const;
    bool isPaused() const;
    \npublic:\n    void progressChanged(const Progress& progress);
    void fileStarted(const std::string& fileName);
    void fileCompleted(const std::string& fileName, bool success);
    void digestionCompleted(bool success, const std::string& message);
    void errorOccurred(const std::string& error);
    void stateChanged(State newState);
\npublic:\n    void processFiles();
\nprivate:\n    void updateProgress();

private:
    void setState(State newState);
    void processNextFile();
    void calculateTimeEstimates();
    void emitProgress();
    
    AIDigestionEngine* m_engine;
    std::stringList m_filePaths;
    std::stringList m_remainingFiles;
    State m_state;
    Progress m_progress;
    
    // Thread synchronization
    mutable std::mutex m_mutex;
    QWaitCondition m_pauseCondition;
    QAtomicInt m_shouldStop;
    
    // Timing
    std::chrono::steady_clock::time_point m_elapsedTimer;
    // Timer m_progressTimer;
    std::vector<int64_t> m_fileProcessingTimes;
    
    // Results
    std::stringList m_processedFiles;
    std::stringList m_failedFiles;
};

/**
 * Background worker for model training operations
 * Runs in separate thread to avoid blocking UI
 * Note: This is the enhanced version with progress monitoring
 */
class AITrainingWorker  {

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
        int64_t elapsedTime = 0;
        int64_t estimatedTime = 0;
        std::string status;
        std::string phase;
    };

    struct TrainingConfig {
        int epochs = 10;
        int batchSize = 32;
        double learningRate = 0.001;
        double validationSplit = 0.2;
        std::string optimizer = "adam";
        std::string lossFunction = "categorical_crossentropy";
        std::stringList metrics = {"accuracy"};
        bool useEarlyStopping = true;
        int patience = 5;
        bool saveCheckpoints = true;
        int checkpointFrequency = 1;
        
        // Agent-specific configuration
        BuildMode buildMode = BuildMode::Model;
        AgentType agentType = AgentType::None;
        std::string agentConfig;
        int agentIterations = 100;
        double agentExplorationRate = 0.1;
        bool useAgentOptimization = false;
    };

    explicit AITrainingWorker(AITrainingPipeline* pipeline);
    ~AITrainingWorker();

    // Control methods
    void startTraining(const AIDigestionDataset& dataset, 
                      const std::string& modelName,
                      const std::string& outputPath,
                      const TrainingConfig& config);
    void pauseTraining();
    void resumeTraining();
    void stopTraining();
    
    // State queries
    State currentState() const;
    Progress currentProgress() const;
    bool isRunning() const;
    bool isPaused() const;
    \npublic:\n    void progressChanged(const Progress& progress);
    void epochStarted(int epoch);
    void epochCompleted(int epoch, double loss, double accuracy);
    void batchCompleted(int batch, double batchLoss);
    void validationCompleted(double valLoss, double valAccuracy);
    void checkpointSaved(const std::string& path);
    void trainingCompleted(bool success, const std::string& modelPath);
    void errorOccurred(const std::string& error);
    void stateChanged(State newState);
\npublic:\n    void processTraining();
\nprivate:\n    void updateProgress();
    void handleEpochComplete();
    void handleBatchComplete();
    void handleValidation();

private:
    void setState(State newState);
    void calculateTimeEstimates();
    void emitProgress();
    void saveCheckpoint();
    bool shouldEarlyStop();
    void cleanupOldCheckpoints(const // & directory, int keepCount);
    
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
    std::string agentStatusMessage() const;
    
    AITrainingPipeline* m_pipeline;
    AIDigestionDataset m_dataset;
    std::string m_modelName;
    std::string m_outputPath;
    TrainingConfig m_config;
    State m_state;
    Progress m_progress;
    
    // Thread synchronization
    mutable std::mutex m_mutex;
    QWaitCondition m_pauseCondition;
    QAtomicInt m_shouldStop;
    
    // Timing
    std::chrono::steady_clock::time_point m_elapsedTimer;
    // Timer m_progressTimer;
    std::vector<int64_t> m_epochTimes;
    
    // Training state
    std::vector<double> m_trainingLosses;
    std::vector<double> m_validationLosses;
    std::vector<double> m_trainingAccuracies;
    std::vector<double> m_validationAccuracies;
    int m_bestEpoch;
    double m_bestValidationLoss;
    int m_patienceCounter;
    
    // Agent-specific state
    std::vector<double> m_agentRewards;
    std::vector<double> m_agentExplorationRates;
    int m_agentIterationCount;
    std::string m_agentState;
    
    // Results
    std::string m_finalModelPath;
    bool m_trainingSuccessful;
};

/**
 * Manager class for coordinating background workers
 * Handles thread management and worker lifecycle
 */
class AIWorkerManager  {

public:
    explicit AIWorkerManager( = nullptr);
    ~AIWorkerManager();

    // Worker creation and management
    AIDigestionWorker* createDigestionWorker(AIDigestionEngine* engine);
    AITrainingWorker* createTrainingWorker(AITrainingPipeline* pipeline);
    
    // Agent-specific worker creation
    AITrainingWorker* createAgentWorker(AITrainingPipeline* pipeline, AgentType agentType);
    AITrainingWorker* createHybridWorker(AITrainingPipeline* pipeline, AgentType agentType);
    
    // Worker control
    void startDigestionWorker(AIDigestionWorker* worker, const std::stringList& files);
    void startTrainingWorker(AITrainingWorker* worker, 
                           const AIDigestionDataset& dataset,
                           const std::string& modelName,
                           const std::string& outputPath,
                           const AITrainingWorker::TrainingConfig& config);
    
    void stopAllWorkers();
    void pauseAllWorkers();
    void resumeAllWorkers();
    
    // State queries
    bool hasActiveWorkers() const;
    std::vector<AIDigestionWorker*> activeDigestionWorkers() const;
    std::vector<AITrainingWorker*> activeTrainingWorkers() const;
\npublic:\n    void workerStarted(void* worker);
    void workerFinished(void* worker);
    void allWorkersFinished();
\nprivate:\n    void onWorkerFinished();
    void onWorkerError(const std::string& error);

private:
    void cleanupFinishedWorkers();
    void moveWorkerToThread(void* worker, std::thread* thread);
    
    std::vector<std::thread*> m_threads;
    std::vector<AIDigestionWorker*> m_digestionWorkers;
    std::vector<AITrainingWorker*> m_trainingWorkers;
    mutable std::mutex m_workersMutex;
    
    // Configuration
    int m_maxConcurrentWorkers;
    int m_threadPoolSize;
};

#endif // AI_WORKERS_H

