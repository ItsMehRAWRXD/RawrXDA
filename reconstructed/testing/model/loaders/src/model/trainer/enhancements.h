// model_trainer_enhancements.h - Enhanced Training Configuration & Strategies
// Converted from Qt to pure C++17
#ifndef MODEL_TRAINER_ENHANCEMENTS_H
#define MODEL_TRAINER_ENHANCEMENTS_H

#include "model_trainer.h"
#include "common/callback_system.hpp"
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <mutex>

// LoRA configuration
struct LoRAConfig {
    int rank = 16;
    float alpha = 32.0f;
    float dropout = 0.05f;
    std::vector<std::string> targetModules = {"q_proj", "v_proj"};
    bool biasTraining = false;
    bool useRSLoRA = false;
    int taskType = 0;    // 0=CAUSAL_LM, 1=SEQ2SEQ, 2=SEQ_CLS
};

// QLoRA configuration (4-bit quantized LoRA)
struct QLoRAConfig : LoRAConfig {
    int quantBits = 4;
    std::string quantType = "nf4";         // nf4, fp4
    bool doubleQuant = true;
    std::string computeType = "float16";   // float16, bfloat16, float32
};

// Data augmentation configuration
struct DataAugmentConfig {
    bool enabled = false;
    float synonymReplacementProb = 0.1f;
    float randomInsertionProb = 0.05f;
    float randomSwapProb = 0.05f;
    float randomDeletionProb = 0.05f;
    float backTranslationProb = 0.0f;
    int maxAugmentations = 3;
    bool paraphrase = false;
    bool contextualAugment = false;
};

// Evaluation strategy
struct EvaluationConfig {
    std::string strategy = "epoch";   // epoch, steps, no
    int evalSteps = 500;
    float evalSplitRatio = 0.1f;
    std::vector<std::string> metrics = {"loss", "perplexity"};
    bool loadBestModelAtEnd = true;
    std::string metricForBestModel = "loss";
    bool greaterIsBetter = false;
    int earlyStoppingPatience = 3;
    float earlyStoppingThreshold = 0.01f;
};

// Mixed precision training config
struct MixedPrecisionConfig {
    bool enabled = false;
    std::string dtype = "float16";         // float16, bfloat16
    float lossScale = 1.0f;
    bool dynamicLossScaling = true;
    int lossScaleWindow = 1000;
    float initScale = 65536.0f;
};

// Distributed training config
struct DistributedConfig {
    bool enabled = false;
    int worldSize = 1;
    int localRank = 0;
    std::string backend = "nccl";   // nccl, gloo, mpi
    bool gradientAsyncAllReduce = false;
    int bucketCapMb = 25;
    bool findUnusedParameters = false;
};

// Curriculum learning configuration
struct CurriculumConfig {
    bool enabled = false;
    std::string strategy = "length";  // length, difficulty, random
    float startRatio = 0.3f;
    float endRatio = 1.0f;
    int warmupSteps = 100;
    std::function<float(const TrainingSample&)> difficultyFn;
};

// Hyperparameter search space
struct HyperparamSearchConfig {
    bool enabled = false;
    std::string searchMethod = "grid";  // grid, random, bayesian
    int maxTrials = 10;
    std::vector<float> learningRates = {1e-5f, 3e-5f, 5e-5f, 1e-4f};
    std::vector<int> batchSizes = {2, 4, 8};
    std::vector<float> warmupRatios = {0.05f, 0.1f, 0.15f};
    std::vector<int> loraRanks = {4, 8, 16, 32};
    std::vector<float> weightDecays = {0.0f, 0.01f, 0.1f};
};

// Training checkpoint info
struct CheckpointInfo {
    std::string path;
    int step = 0;
    int epoch = 0;
    float loss = 0.0f;
    float evalLoss = 0.0f;
    TimePoint timestamp;
    bool isBest = false;
};

// Full enhanced training configuration
struct EnhancedTrainingConfig {
    TrainingConfig base;
    LoRAConfig lora;
    QLoRAConfig qlora;
    DataAugmentConfig augmentation;
    EvaluationConfig evaluation;
    MixedPrecisionConfig mixedPrecision;
    DistributedConfig distributed;
    CurriculumConfig curriculum;
    HyperparamSearchConfig hyperparamSearch;

    bool useLoRA = false;
    bool useQLoRA = false;
    bool fullFineTune = false;
};

// Enhanced trainer with advanced features
class EnhancedModelTrainer {
public:
    EnhancedModelTrainer();
    ~EnhancedModelTrainer();

    // Training with enhanced config
    bool startTraining(const EnhancedTrainingConfig& config);
    void stopTraining();

    // LoRA operations
    bool applyLoRA(const LoRAConfig& config);
    bool mergeLoRAWeights();
    bool exportLoRAAdapter(const std::string& path);

    // Hyperparameter search
    struct TrialResult {
        std::map<std::string, float> params;
        float bestLoss = 0.0f;
        double durationSec = 0.0;
    };
    std::vector<TrialResult> runHyperparamSearch(const HyperparamSearchConfig& config);

    // Checkpointing
    std::vector<CheckpointInfo> getCheckpoints() const;
    bool loadCheckpoint(const std::string& path);
    void pruneCheckpoints(int keepBest = 3);

    // Data augmentation
    std::vector<TrainingSample> augmentSample(const TrainingSample& sample,
                                               const DataAugmentConfig& config);

    // Callbacks
    CallbackList<const TrialResult&> onTrialComplete;
    CallbackList<const CheckpointInfo&> onBestModelFound;
    CallbackList<const std::string&> onLogMessage;

private:
    std::unique_ptr<ModelTrainer> m_trainer;
    EnhancedTrainingConfig m_config;
    std::vector<CheckpointInfo> m_checkpoints;
    mutable std::mutex m_mutex;
};

#endif // MODEL_TRAINER_ENHANCEMENTS_H
