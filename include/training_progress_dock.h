#pragma once

// C++20 / Win32. Training progress dock; no Qt. Callbacks for trainer events.

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

class ModelTrainer;

class TrainingProgressDock
{
public:
    using StopRequestedFn = std::function<void()>;

    explicit TrainingProgressDock(ModelTrainer* trainer);
    void setOnStopRequested(StopRequestedFn f) { m_onStopRequested = std::move(f); }
    void initialize();

    void onTrainingStarted();
    void onEpochStarted(int currentEpoch, int totalEpochs);
    void onBatchProcessed(int currentBatch, int totalBatches, float loss);
    void onEpochCompleted(int epoch, float avgLoss, float perplexity);
    void onTrainingCompleted(const std::string& modelPath, float finalLoss);
    void onTrainingStopped();
    void onTrainingError(const std::string& error);
    void onLogMessage(const std::string& message);
    void onValidationResults(float perplexity, const std::string& details);

    void* getWidgetHandle() const { return m_handle; }

private:
    void setupUI();
    void resetMetrics();
    void updateTimeEstimate();
    std::string formatDuration(int64_t seconds) const;

    void* m_handle = nullptr;
    ModelTrainer* m_trainer = nullptr;
    int m_currentEpoch = 0;
    int m_totalEpochs = 0;
    int m_currentBatch = 0;
    int m_totalBatches = 0;
    float m_currentLoss = 0.f;
    float m_bestLoss = 0.f;
    int64_t m_trainingStartTime = 0;
    int64_t m_lastBatchTime = 0;
    int m_totalBatchesProcessed = 0;
    std::vector<float> m_lossHistory;
    std::vector<float> m_perplexityHistory;
    StopRequestedFn m_onStopRequested;
};
