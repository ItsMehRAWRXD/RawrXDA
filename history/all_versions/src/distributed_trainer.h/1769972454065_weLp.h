#pragma once

#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <functional>

// Configuration structure matching usage in cpp
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

    // Matching Capitalized Initialize in cpp
    bool Initialize(const TrainerConfig& config);
    // Alias for lowercase usage if any
    void initialize(const TrainerConfig& config) { Initialize(config); }
    
    void Shutdown();
    void shutdown() { Shutdown(); }
    
    bool loadModel(const std::string& path);
    // Status update callback
    void statusChanged(const std::string& status) { 
        #ifdef WIN32
        OutputDebugStringA(("[Trainer Status] " + status + "\n").c_str());
        #endif
        std::cout << "[Trainer Status] " << status << std::endl;
    }
    void logError(const std::string& msg, int code) { 
        std::string err = "[Trainer Error] " + msg + " (Code: " + std::to_string(code) + ")\n";
        #ifdef WIN32
        OutputDebugStringA(err.c_str());
        #endif
        std::cerr << err;
    }

    // Core Training Steps
    bool performTrainingStep(const void* batchData, float* lossOut);
    
    // Callbacks
    std::function<void(int, float)> trainingStepCompleted;
    std::function<void(float)> gradientsSynchronized;

private:
    bool validateConfig() { return true; }
    bool initializeBackend() { return true; } // Real backend init would go here
    bool detectDevices() { return true; }
    bool setupProcessGroup() { return true; }
    void cleanupProcessGroup() {}
    void initializeLoadBalancer() {}
    void initializeFaultTolerance() {}
    void Checkpoint(const std::string& path) {}

    bool forwardPass(const void*& batchData);
    bool backwardPass();
    bool optimizerStep();
    
    // Distributed Sync
    bool synchronizeGradients();
    bool allReduceGradients();
    void compressGradients() {}
    void decompressGradients() {}

private:
    TrainerConfig m_config;
    bool m_initialized = false;
    int m_globalStep = 0;
    float m_currentLoss = 0.0f;
    float m_lastSyncTimeMs = 0.0f;
    std::string m_lastCheckpointPath;

    // Computed gradients for valid UpdateWeights call
    std::vector<std::vector<float>> m_gradients;
};
