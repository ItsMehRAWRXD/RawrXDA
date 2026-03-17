#pragma once
#include "gpu_memory_manager.hpp"
#include "advanced_streaming_api.hpp"
#include "advanced_model_queue.hpp"
#include <QString>
#include <vector>
#include <memory>
#include <functional>

/**
 * @class GPUInferenceEngine
 * @brief Unified GPU inference with CPU fallback
 * 
 * Integrates:
 * - GPU backends (CUDA/HIP)
 * - Memory management (pooling, async transfers)
 * - Advanced streaming (per-tensor optimization)
 * - Model queue (hot-swapping, priority)
 */
class GPUInferenceEngine {
public:
    enum InferenceBackend { CPU, CUDA, HIP };
    
    struct InferenceConfig {
        InferenceBackend backend = CUDA;
        int gpuDevice = 0;
        float gpuMemoryGB = 24.0f;
        bool enableStreaming = true;
        bool enableOptimization = true;
        int maxConcurrentLoads = 2;
        bool fallbackToCPU = true;
    };
    
    struct OffloadStrategy {
        bool offloadEmbedding = true;
        bool offloadAttention = true;
        bool offloadFeedForward = true;
        bool offloadNorm = false; // Too small to benefit
        float computeThreshold = 1.0f; // Ops > 1 GFLOPs go to GPU
    };
    
    explicit GPUInferenceEngine(const InferenceConfig& config);
    ~GPUInferenceEngine();
    
    // Initialization
    bool initialize();
    bool selectDevice(InferenceBackend backend, int deviceId = 0);
    
    // Model management
    bool loadModel(const QString& modelPath);
    bool unloadModel(const QString& modelPath);
    bool swapModel(const QString& from, const QString& to);
    bool preloadModel(const QString& modelPath);
    
    // Inference
    std::vector<QString> inferenceStreaming(
        const QString& modelPath,
        const QString& prompt,
        int maxTokens,
        std::function<void(const QString&)> tokenCallback,
        std::function<void(float)> progressCallback
    );
    
    // Query
    bool isModelLoaded(const QString& modelPath) const;
    InferenceBackend getActiveBackend() const;
    float getGPUUtilization() const;
    uint64_t getGPUMemoryUsed() const;
    
    // Configuration
    void setOffloadStrategy(const OffloadStrategy& strategy);
    void enableAutoOptimization(bool enable);
    void setMaxBatchSize(int size);
    
    // Performance
    float benchmarkModel(const QString& modelPath);
    QString getPerformanceReport() const;
    
private:
    InferenceConfig m_config;
    OffloadStrategy m_offloadStrategy;
    
    InferenceBackend m_activeBackend = CPU;
    std::unique_ptr<GPUMemoryManager> m_memoryManager;
    std::unique_ptr<AdvancedStreamingAPI> m_streamingAPI;
    std::unique_ptr<AdvancedModelQueue> m_modelQueue;
    
    bool m_initialized = false;
    int m_maxBatchSize = 1;
    bool m_autoOptimization = true;
    
    // Helper methods
    bool initializeGPUBackend();
    bool shouldOffloadToGPU(const QString& layerName) const;
    float estimateComputeForLayer(const QString& layerName) const;
};

#endif // GPU_INFERENCE_ENGINE_HPP
