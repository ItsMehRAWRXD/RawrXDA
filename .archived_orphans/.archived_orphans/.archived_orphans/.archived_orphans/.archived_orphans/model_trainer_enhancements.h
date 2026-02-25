// ============================================================================
// File: model_trainer_enhancements.h
// Purpose: Enhanced ModelTrainer — C++20, no Qt. JSON via nlohmann::json.
//          Dynamic LR scheduling, checkpointing, callbacks replace signals.
// ============================================================================

#pragma once

#include "model_trainer.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <vector>
#include <string>
#include <memory>
#include <fstream>
#include <functional>

/**
 * @class EnhancedModelTrainer
 * @brief Enhanced ModelTrainer with advanced features (C++20, no Qt)
 *
 * Callbacks replace Qt signals. JSON via nlohmann::json.
 */
class EnhancedModelTrainer : public ModelTrainer
{
public:
    // ===== Enhanced Configuration =====

    struct EnhancedTrainingConfig : public TrainingConfig {
        std::string lrSchedule = "cosine";
        float warmupRatio = 0.1f;
        float minLearningRate = 1e-6f;
        int stepDecayEpochs = 10;
        float stepDecayRate = 0.5f;
        bool enableCheckpoints = true;
        int checkpointFrequency = 1;
        int keepBestCheckpoints = 3;
        bool mixedPrecision = false;
        int gradientAccumulation = 1;
        bool earlyStopping = true;
        int earlyStoppingPatience = 5;

        nlohmann::json toJson() const;
        static EnhancedTrainingConfig fromJson(const nlohmann::json& json);
    };

    enum class LearningRateSchedule { Cosine, Linear, Step, Exponential };

    // ===== Lifecycle =====

    explicit EnhancedModelTrainer();
    ~EnhancedModelTrainer() override;

    bool loadConfiguration(const std::string& configPath);
    bool saveConfiguration(const std::string& configPath) const;
    bool startTraining(const EnhancedTrainingConfig& config);

    // ===== Checkpoint Management =====

    bool saveCheckpoint(int epoch, float loss, float perplexity);
    bool loadCheckpoint(const std::string& checkpointPath);
    std::vector<std::string> getAvailableCheckpoints() const;
    void cleanupCheckpoints(int keepCount = 3);

    // ===== Callbacks (replace Qt signals) =====

    std::function<void(int epoch, int step, float learningRate)> onLearningRateChanged;
    std::function<void(const std::string& checkpointPath, int epoch, float loss)> onCheckpointSaved;
    std::function<void(const nlohmann::json& metrics)> onTrainingMetrics;
    std::function<void(int bestEpoch, float bestLoss)> onEarlyStoppingTriggered;

private:
    void runEnhancedTraining();

    float calculateLearningRate(int step, int totalSteps);
    float cosineDecay(int step, int totalSteps);
    float linearDecay(int step, int totalSteps);
    float stepDecay(int step, int totalSteps);
    float exponentialDecay(int step, int totalSteps);

    std::string getCheckpointPath(int epoch, float loss) const;
    std::pair<int, float> parseCheckpointFilename(const std::string& filename) const;

    bool executeEnhancedEpoch(int epoch);
    bool checkEarlyStopping(int epoch, float loss);
    void updateMetrics();
    void logEnhanced(const std::string& message, const std::string& level = "INFO");

    EnhancedTrainingConfig m_enhancedConfig;
    LearningRateSchedule m_lrSchedule;
    std::string m_checkpointDir;
    std::vector<std::pair<int, float>> m_checkpointHistory;
    int m_bestEpoch = 0;
    float m_bestLoss = std::numeric_limits<float>::max();
    int m_epochsWithoutImprovement = 0;
    nlohmann::json m_trainingMetrics;
    std::vector<float> m_lossHistory;
    std::vector<float> m_perplexityHistory;
    std::vector<float> m_learningRateHistory;
    std::ofstream m_logFile;
};

// ===== JSON Serialization (inline for header-only use) =====

inline nlohmann::json EnhancedModelTrainer::EnhancedTrainingConfig::toJson() const
{
    nlohmann::json j;
    j["datasetPath"] = datasetPath;
    j["outputPath"] = outputPath;
    j["epochs"] = epochs;
    j["learningRate"] = learningRate;
    j["batchSize"] = batchSize;
    j["sequenceLength"] = sequenceLength;
    j["gradientClip"] = gradientClip;
    j["validationSplit"] = validationSplit;
    j["lrSchedule"] = lrSchedule;
    j["warmupRatio"] = warmupRatio;
    j["minLearningRate"] = minLearningRate;
    j["stepDecayEpochs"] = stepDecayEpochs;
    j["stepDecayRate"] = stepDecayRate;
    j["enableCheckpoints"] = enableCheckpoints;
    j["checkpointFrequency"] = checkpointFrequency;
    j["keepBestCheckpoints"] = keepBestCheckpoints;
    j["mixedPrecision"] = mixedPrecision;
    j["gradientAccumulation"] = gradientAccumulation;
    j["earlyStopping"] = earlyStopping;
    j["earlyStoppingPatience"] = earlyStoppingPatience;
    return j;
}

inline EnhancedModelTrainer::EnhancedTrainingConfig
EnhancedModelTrainer::EnhancedTrainingConfig::fromJson(const nlohmann::json& j)
{
    EnhancedTrainingConfig c;
    c.datasetPath = j.value("datasetPath", "");
    c.outputPath = j.value("outputPath", "");
    c.epochs = j.value("epochs", 10);
    c.learningRate = j.value("learningRate", 0.001);
    c.batchSize = j.value("batchSize", 32);
    c.sequenceLength = j.value("sequenceLength", 512);
    c.gradientClip = j.value("gradientClip", 1.0);
    c.validationSplit = j.value("validationSplit", 0.1);
    c.lrSchedule = j.value("lrSchedule", "cosine");
    c.warmupRatio = j.value("warmupRatio", 0.1);
    c.minLearningRate = j.value("minLearningRate", 1e-6);
    c.stepDecayEpochs = j.value("stepDecayEpochs", 10);
    c.stepDecayRate = j.value("stepDecayRate", 0.5);
    c.enableCheckpoints = j.value("enableCheckpoints", true);
    c.checkpointFrequency = j.value("checkpointFrequency", 1);
    c.keepBestCheckpoints = j.value("keepBestCheckpoints", 3);
    c.mixedPrecision = j.value("mixedPrecision", false);
    c.gradientAccumulation = j.value("gradientAccumulation", 1);
    c.earlyStopping = j.value("earlyStopping", true);
    c.earlyStoppingPatience = j.value("earlyStoppingPatience", 5);
    return c;
}
