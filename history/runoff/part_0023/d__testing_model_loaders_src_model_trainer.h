// model_trainer.h - GGUF Model Fine-Tuning Engine
// Converted from Qt to pure C++17 (QThread/QObject/signals removed)
#ifndef MODEL_TRAINER_H
#define MODEL_TRAINER_H

#include "common/callback_system.hpp"
#include "common/json_types.hpp"
#include "common/time_utils.hpp"
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>

// Training configuration
struct TrainingConfig {
    std::string modelPath;
    std::string datasetPath;
    std::string outputPath;
    int epochs = 3;
    int batchSize = 4;
    float learningRate = 1e-5f;
    float warmupRatio = 0.1f;
    float weightDecay = 0.01f;
    int maxSequenceLength = 512;
    int gradientAccumulationSteps = 1;
    bool useGradientCheckpointing = false;
    int loggingSteps = 10;
    int saveSteps = 100;
    std::string optimizer = "adamw";     // adamw, sgd, adafactor
    std::string scheduler = "cosine";    // cosine, linear, constant
    float maxGradNorm = 1.0f;
    bool fp16 = false;
    int seed = 42;
};

// Training metrics
struct TrainingMetrics {
    int currentEpoch = 0;
    int totalEpochs = 0;
    int currentStep = 0;
    int totalSteps = 0;
    float loss = 0.0f;
    float learningRate = 0.0f;
    float gradNorm = 0.0f;
    float tokensPerSecond = 0.0f;
    double elapsedSeconds = 0.0;
    double estimatedRemainingSeconds = 0.0;
    float bestLoss = 999.0f;
    std::vector<float> lossHistory;
};

// Dataset sample
struct TrainingSample {
    std::string input;
    std::string output;
    std::string instruction;
    int tokenCount = 0;
};

// Training state
enum class TrainingState {
    Idle,
    Loading,
    Training,
    Evaluating,
    Saving,
    Paused,
    Completed,
    Failed
};

class ModelTrainer {
public:
    ModelTrainer();
    ~ModelTrainer();

    // Training control
    bool startTraining(const TrainingConfig& config);
    void pauseTraining();
    void resumeTraining();
    void stopTraining();

    // Dataset management
    bool loadDataset(const std::string& path);
    int getDatasetSize() const;
    TrainingSample getSample(int index) const;
    bool validateDataset() const;

    // State
    TrainingState getState() const;
    TrainingMetrics getMetrics() const;
    float getProgress() const;

    // Configuration
    void setConfig(const TrainingConfig& config);
    TrainingConfig getConfig() const;

    // Export
    bool saveCheckpoint(const std::string& path);
    bool exportModel(const std::string& path, const std::string& format = "gguf");
    bool convertToGGUF(const std::string& inputPath, const std::string& outputPath,
                       const std::string& quantType = "q4_0");

    // Callbacks (replacing Qt signals)
    CallbackList<const TrainingMetrics&> onProgressUpdate;
    CallbackList<const std::string&> onTrainingComplete;
    CallbackList<const std::string&> onErrorOccurred;
    CallbackList<int, int> onEpochComplete;      // epoch, total
    CallbackList<const std::string&> onCheckpointSaved;
    CallbackList<TrainingState> onStateChanged;
    CallbackList<const std::string&> onLogMessage;

private:
    void trainingLoop();
    void trainStep(int step);
    void evaluateStep();
    float computeLoss(const TrainingSample& sample);
    void updateLearningRate(int step, int totalSteps);
    void setState(TrainingState state);

    mutable std::mutex m_mutex;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_paused{false};
    std::thread m_trainingThread;
    TrainingConfig m_config;
    TrainingMetrics m_metrics;
    TrainingState m_state = TrainingState::Idle;
    std::vector<TrainingSample> m_dataset;
};

#endif // MODEL_TRAINER_H
