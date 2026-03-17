// ============================================================================
// File: model_trainer_enhancements.h
// 
// Purpose: Enhanced ModelTrainer with dynamic learning rate scheduling,
//          JSON configuration, checkpointing, and advanced features
//
// License: Production Grade - Enterprise Ready
// ============================================================================

#pragma once

#include "model_trainer.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <math>
#include <vector>
#include <string>
#include <memory>

/**
 * @class EnhancedModelTrainer
 * @brief Enhanced ModelTrainer with advanced features
 * 
 * Additional Features:
 * - Dynamic learning rate scheduling
 * - JSON configuration support
 * - Model checkpointing
 * - Advanced error handling and logging
 * - GPU acceleration support (future)
 * - Enhanced user feedback
 */
class EnhancedModelTrainer : public ModelTrainer
{
    Q_OBJECT

public:
    // ===== Enhanced Configuration =====
    
    /**
     * @struct EnhancedTrainingConfig
     * @brief Extended training configuration with scheduling options
     */
    struct EnhancedTrainingConfig : public TrainingConfig {
        // Learning rate scheduling
        QString lrSchedule = "cosine";        ///< "cosine", "linear", "step"
        float warmupRatio = 0.1f;             ///< Warmup as ratio of total steps
        float minLearningRate = 1e-6f;        ///< Minimum learning rate
        int stepDecayEpochs = 10;             ///< Epochs between step decays
        float stepDecayRate = 0.5f;           ///< Step decay multiplier
        
        // Checkpointing
        bool enableCheckpoints = true;        ///< Save model checkpoints
        int checkpointFrequency = 1;          ///< Epochs between checkpoints
        int keepBestCheckpoints = 3;          ///< Number of best checkpoints to keep
        
        // Advanced options
        bool mixedPrecision = false;          ///< Use mixed precision training
        int gradientAccumulation = 1;         ///< Gradient accumulation steps
        bool earlyStopping = true;            ///< Enable early stopping
        int earlyStoppingPatience = 5;        ///< Epochs without improvement
        
        // JSON configuration support
        QJsonObject toJson() const;
        static EnhancedTrainingConfig fromJson(const QJsonObject& json);
    };
    
    /**
     * @enum LearningRateSchedule
     * @brief Supported learning rate schedules
     */
    enum class LearningRateSchedule {
        Cosine,     ///< Cosine decay
        Linear,     ///< Linear decay
        Step,       ///< Step decay
        Exponential ///< Exponential decay
    };

    // ===== Lifecycle =====
    
    explicit EnhancedModelTrainer(QObject* parent = nullptr);
    ~EnhancedModelTrainer() override;
    
    /**
     * @brief Load configuration from JSON file
     * @param configPath Path to JSON configuration file
     * @return true if configuration loaded successfully
     */
    bool loadConfiguration(const QString& configPath);
    
    /**
     * @brief Save configuration to JSON file
     * @param configPath Path to save configuration
     * @return true if configuration saved successfully
     */
    bool saveConfiguration(const QString& configPath) const;
    
    /**
     * @brief Start training with enhanced configuration
     * @param config Enhanced training configuration
     * @return true if training started successfully
     */
    bool startTraining(const EnhancedTrainingConfig& config);

    // ===== Checkpoint Management =====
    
    /**
     * @brief Save training checkpoint
     * @param epoch Current epoch
     * @param loss Current loss
     * @param perplexity Current perplexity
     * @return true if checkpoint saved successfully
     */
    bool saveCheckpoint(int epoch, float loss, float perplexity);
    
    /**
     * @brief Load training checkpoint
     * @param checkpointPath Path to checkpoint file
     * @return true if checkpoint loaded successfully
     */
    bool loadCheckpoint(const QString& checkpointPath);
    
    /**
     * @brief Get list of available checkpoints
     * @return List of checkpoint file paths
     */
    QStringList getAvailableCheckpoints() const;
    
    /**
     * @brief Clean up old checkpoints
     * @param keepCount Number of checkpoints to keep
     */
    void cleanupCheckpoints(int keepCount = 3);

    // ===== Enhanced Signals =====
    
signals:
    /**
     * @brief Emitted when learning rate changes
     * @param epoch Current epoch
     * @param step Current step
     * @param learningRate New learning rate
     */
    void learningRateChanged(int epoch, int step, float learningRate);
    
    /**
     * @brief Emitted when checkpoint is saved
     * @param checkpointPath Path to saved checkpoint
     * @param epoch Checkpoint epoch
     * @param loss Checkpoint loss
     */
    void checkpointSaved(const QString& checkpointPath, int epoch, float loss);
    
    /**
     * @brief Emitted for detailed training metrics
     * @param metrics JSON object with detailed metrics
     */
    void trainingMetrics(const QJsonObject& metrics);
    
    /**
     * @brief Emitted when early stopping is triggered
     * @param bestEpoch Best epoch achieved
     * @param bestLoss Best loss achieved
     */
    void earlyStoppingTriggered(int bestEpoch, float bestLoss);

private slots:
    /**
     * @brief Enhanced training loop
     */
    void runEnhancedTraining();

private:
    // ===== Learning Rate Scheduling =====
    
    /**
     * @brief Calculate learning rate based on schedule
     * @param step Current training step
     * @param totalSteps Total training steps
     * @return Current learning rate
     */
    float calculateLearningRate(int step, int totalSteps);
    
    /**
     * @brief Cosine decay learning rate schedule
     */
    float cosineDecay(int step, int totalSteps);
    
    /**
     * @brief Linear decay learning rate schedule
     */
    float linearDecay(int step, int totalSteps);
    
    /**
     * @brief Step decay learning rate schedule
     */
    float stepDecay(int step, int totalSteps);
    
    /**
     * @brief Exponential decay learning rate schedule
     */
    float exponentialDecay(int step, int totalSteps);
    
    // ===== Checkpoint Management =====
    
    /**
     * @brief Get checkpoint file path
     * @param epoch Epoch number
     * @param loss Training loss
     * @return Checkpoint file path
     */
    QString getCheckpointPath(int epoch, float loss) const;
    
    /**
     * @brief Parse checkpoint filename
     * @param filename Checkpoint filename
     * @return Epoch and loss extracted from filename
     */
    std::pair<int, float> parseCheckpointFilename(const QString& filename) const;
    
    // ===== Enhanced Training =====
    
    /**
     * @brief Execute enhanced training epoch
     * @param epoch Current epoch
     * @return true if epoch completed successfully
     */
    bool executeEnhancedEpoch(int epoch);
    
    /**
     * @brief Check for early stopping conditions
     * @param epoch Current epoch
     * @param loss Current loss
     * @return true if early stopping should be triggered
     */
    bool checkEarlyStopping(int epoch, float loss);
    
    /**
     * @brief Update training metrics
     */
    void updateMetrics();
    
    /**
     * @brief Log enhanced training information
     * @param message Log message
     * @param level Log level (INFO, WARNING, ERROR)
     */
    void logEnhanced(const QString& message, const QString& level = "INFO");

    // ===== Enhanced State =====
    
    EnhancedTrainingConfig m_enhancedConfig;
    LearningRateSchedule m_lrSchedule;
    
    // Checkpoint management
    QString m_checkpointDir;
    std::vector<std::pair<int, float>> m_checkpointHistory;
    
    // Early stopping
    int m_bestEpoch = 0;
    float m_bestLoss = std::numeric_limits<float>::max();
    int m_epochsWithoutImprovement = 0;
    
    // Metrics tracking
    QJsonObject m_trainingMetrics;
    std::vector<float> m_lossHistory;
    std::vector<float> m_perplexityHistory;
    std::vector<float> m_learningRateHistory;
    
    // Enhanced logging
    QFile* m_logFile = nullptr;
};

// ===== JSON Serialization Helpers =====

/**
 * @brief Serialize EnhancedTrainingConfig to JSON
 */
QJsonObject EnhancedModelTrainer::EnhancedTrainingConfig::toJson() const
{
    QJsonObject json;
    
    // Base configuration
    json["datasetPath"] = datasetPath;
    json["outputPath"] = outputPath;
    json["epochs"] = epochs;
    json["learningRate"] = learningRate;
    json["batchSize"] = batchSize;
    json["sequenceLength"] = sequenceLength;
    json["gradientClip"] = gradientClip;
    json["validationSplit"] = validationSplit;
    
    // Enhanced configuration
    json["lrSchedule"] = lrSchedule;
    json["warmupRatio"] = warmupRatio;
    json["minLearningRate"] = minLearningRate;
    json["stepDecayEpochs"] = stepDecayEpochs;
    json["stepDecayRate"] = stepDecayRate;
    json["enableCheckpoints"] = enableCheckpoints;
    json["checkpointFrequency"] = checkpointFrequency;
    json["keepBestCheckpoints"] = keepBestCheckpoints;
    json["mixedPrecision"] = mixedPrecision;
    json["gradientAccumulation"] = gradientAccumulation;
    json["earlyStopping"] = earlyStopping;
    json["earlyStoppingPatience"] = earlyStoppingPatience;
    
    return json;
}

/**
 * @brief Deserialize EnhancedTrainingConfig from JSON
 */
EnhancedModelTrainer::EnhancedTrainingConfig 
EnhancedModelTrainer::EnhancedTrainingConfig::fromJson(const QJsonObject& json)
{
    EnhancedTrainingConfig config;
    
    // Base configuration
    config.datasetPath = json["datasetPath"].toString();
    config.outputPath = json["outputPath"].toString();
    config.epochs = json["epochs"].toInt();
    config.learningRate = json["learningRate"].toDouble();
    config.batchSize = json["batchSize"].toInt();
    config.sequenceLength = json["sequenceLength"].toInt();
    config.gradientClip = json["gradientClip"].toDouble();
    config.validationSplit = json["validationSplit"].toDouble();
    
    // Enhanced configuration
    config.lrSchedule = json["lrSchedule"].toString();
    config.warmupRatio = json["warmupRatio"].toDouble();
    config.minLearningRate = json["minLearningRate"].toDouble();
    config.stepDecayEpochs = json["stepDecayEpochs"].toInt();
    config.stepDecayRate = json["stepDecayRate"].toDouble();
    config.enableCheckpoints = json["enableCheckpoints"].toBool();
    config.checkpointFrequency = json["checkpointFrequency"].toInt();
    config.keepBestCheckpoints = json["keepBestCheckpoints"].toInt();
    config.mixedPrecision = json["mixedPrecision"].toBool();
    config.gradientAccumulation = json["gradientAccumulation"].toInt();
    config.earlyStopping = json["earlyStopping"].toBool();
    config.earlyStoppingPatience = json["earlyStoppingPatience"].toInt();
    
    return config;
}

#endif // ENHANCED_MODEL_TRAINER_H