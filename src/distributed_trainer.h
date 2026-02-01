#pragma once

#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <functional>
#include <iostream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

struct TrainerConfig {
    float learningRate = 0.001f;
    int batchSize = 32;
    int epochs = 10;
    bool enableLoadBalancing = false;
    bool enableFaultTolerance = false;
    int parallelism = 1;

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

    bool Initialize(const TrainerConfig& config);
    void Shutdown();
    bool loadModel(const std::string& path);
    bool performTrainingStep(const void* batchData, float* lossOut);

    // Callbacks
    std::function<void(int, float)> trainingStepCompleted;
    std::function<void(float)> gradientsSynchronized;

    // Helper functions
    void logError(const std::string& msg, int code);
    void statusChanged(const std::string& status);

private:
    // Internal Methods - implemented in cpp
    bool validateConfig();
    bool initializeBackend();
    bool detectDevices();
    bool setupProcessGroup();
    void cleanupProcessGroup();
    void initializeLoadBalancer();
    void initializeFaultTolerance();
    void Checkpoint(const std::string& path);

    bool forwardPass(const void*& batchData);
    bool backwardPass();
    bool optimizerStep();

    bool synchronizeGradients();
    bool allReduceGradients();
    void compressGradients();
    void decompressGradients();

    // Additional methods referenced in cpp errors
    void cleanupBackend();
    void redistributeWork();
    void updateDeviceLoads();
    void detectCUDADevices();
    void detectRealGPUs();
    void updateMetrics(float stepTimeMs);

private:
    TrainerConfig m_config;
    bool m_initialized = false;
    int m_globalStep = 0;
    float m_currentLoss = 0.0f;
    float m_lastSyncTimeMs = 0.0f;
    std::string m_lastCheckpointPath;

    std::vector<std::vector<float>> m_gradients;
    std::vector<float> m_lastLogits;
    int32_t m_lastTargetToken = 0;
    std::vector<float> m_outputGradients;
};
