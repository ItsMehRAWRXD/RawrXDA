#pragma once

#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <functional>

struct TrainingConfig {
    float learningRate = 0.001f;
    int batchSize = 32;
    int epochs = 10;
    
    struct PGConfig {
        int worldSize = 1;
        int rank = 0;
    } pgConfig;
    
    enum class GradientCompression {
        None,
        FP16,
        Quantized
    } compression = GradientCompression::None;
};

class DistributedTrainer {
public:
    DistributedTrainer(void* parent = nullptr);
    ~DistributedTrainer();

    void initialize(const TrainingConfig& config) { m_config = config; m_initialized = true; }
    void shutdown() { m_initialized = false; }
    
    bool loadModel(const std::string& path);
    
    // Core Training Steps
    bool performTrainingStep(const void* batchData, float* lossOut);
    
    // Callbacks
    std::function<void(int, float)> trainingStepCompleted;
    std::function<void(float)> gradientsSynchronized;

private:
    bool forwardPass(const void*& batchData);
    bool backwardPass();
    bool optimizerStep();
    
    // Distributed Sync
    bool synchronizeGradients();
    bool allReduceGradients();
    void compressGradients() {}
    void decompressGradients() {}

private:
    TrainingConfig m_config;
    bool m_initialized = false;
    int m_globalStep = 0;
    float m_currentLoss = 0.0f;
    float m_lastSyncTimeMs = 0.0f;
    
    // If we want real logic, we need the engine
    // We can use a pointer to avoid including the heavy header here if strictly needed,
    // but including it is safer for completeness.
};
