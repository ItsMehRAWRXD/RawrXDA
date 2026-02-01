#pragma once

#include "ai_types.hpp"
#include <string>
#include <vector>
#include <memory>

class ModelTrainer;

class AITrainingPipeline {
public:
    AITrainingPipeline();
    ~AITrainingPipeline();

    bool prepareEnvironment(const TrainingConfig& config);
    bool executeOneEpoch(int epoch, int totalEpochs, double& loss);
    bool saveCheckpoint(const std::string& path);
    bool exportModel(const std::string& format);

    std::string getLastError() const { return m_lastError; }

private:
    std::string m_lastError;
    int m_currentEpoch = 0;
    std::shared_ptr<ModelTrainer> m_trainer;
};
