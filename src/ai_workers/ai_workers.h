#pragma once

#include <vector>
#include <string>
#include <memory>
#include <functional>

// Abstract interfaces or forward declarations
class AIDigestionEngine;
class AITrainingPipeline;
class TrainingConfig;
class DigestionConfig;

class Worker {
public:
    virtual ~Worker() = default;
    virtual void cancel() = 0;
};

class DigestionWorker : public Worker {
public:
    DigestionWorker(AIDigestionEngine* engine);
    void processFiles(const std::vector<std::string>& files, const DigestionConfig& config);
    void cancel() override;
    
    std::function<void(int)> onProgress;
    std::function<void(const std::string&)> onError;
    
private:
    AIDigestionEngine* m_engine;
    bool m_cancelled = false;
};

class AITrainingWorker : public Worker {
public:
    AITrainingWorker(AITrainingPipeline* pipeline);
    void startTraining(const std::string& dataset, const std::string& modelName, const std::string& outputPath, const TrainingConfig& config);
    void cancel() override;

    std::function<void(int)> onProgress;
    std::function<void(const std::string&)> onError;
    std::function<void(const std::string&)> onComplete;

private:
    AITrainingPipeline* m_pipeline;
    bool m_cancelled = false;
};
