#pragma once
#include <QString>
#include <QVector>
#include <cstdint>
#include <memory>

/**
 * @class GPUBackend
 * @brief Abstract base class for GPU acceleration
 * 
 * Supports CUDA, HIP, and DirectCompute backends
 */
class GPUBackend {
    
public:
    enum BackendType {
        CPU_ONLY,
        CUDA,
        HIP,
        DIRECTCOMPUTE,
        VULKAN
    };
    
    struct DeviceInfo {
        QString name;
        uint64_t memoryAvailable = 0;
        uint64_t memoryTotal = 0;
        int computeCapability = 0;
        float maxFlopsPerSecond = 0.0f;
        bool supported = false;
    };
    
    virtual ~GPUBackend() = default;
    
    /**
     * @brief Initialize GPU backend
     * @return true if successfully initialized
     */
    virtual bool initialize() = 0;
    
    /**
     * @brief Check if backend is available
     */
    virtual bool isAvailable() const = 0;
    
    /**
     * @brief Get device information
     */
    virtual DeviceInfo getDeviceInfo() const = 0;
    
    /**
     * @brief Allocate GPU memory
     */
    virtual void* allocateMemory(uint64_t sizeBytes) = 0;
    
    /**
     * @brief Free GPU memory
     */
    virtual void freeMemory(void* ptr) = 0;
    
    /**
     * @brief Copy data to GPU
     */
    virtual bool copyToGPU(void* gpuPtr, const void* hostPtr, uint64_t sizeBytes) = 0;
    
    /**
     * @brief Copy data from GPU
     */
    virtual bool copyFromGPU(void* hostPtr, const void* gpuPtr, uint64_t sizeBytes) = 0;
    
    /**
     * @brief Dequantize tensors (Q2_K, Q3_K, etc.)
     */
    virtual bool dequantizeQ2K(const void* quantized, void* dequantized, 
                               uint64_t blocks, int qkSize) = 0;
    virtual bool dequantizeQ3K(const void* quantized, void* dequantized,
                               uint64_t blocks, int qkSize) = 0;
    virtual bool dequantizeQ5K(const void* quantized, void* dequantized,
                               uint64_t blocks, int qkSize) = 0;
    
    /**
     * @brief Matrix multiplication (main inference bottleneck)
     */
    virtual bool matmul(const void* A, const void* B, void* C,
                       int M, int N, int K, bool transposeB = false) = 0;
    
    /**
     * @brief Synchronize GPU operations
     */
    virtual void synchronize() = 0;
    
    /**
     * @brief Get estimated speedup vs CPU
     */
    virtual float getEstimatedSpeedup() const = 0;
};

/**
 * @class GPUBackendFactory
 * @brief Factory for creating GPU backends
 */
class GPUBackendFactory {
public:
    /**
     * @brief Create appropriate GPU backend based on availability
     */
    static std::unique_ptr<GPUBackend> createBestBackend();
    
    /**
     * @brief Create specific GPU backend
     */
    static std::unique_ptr<GPUBackend> createBackend(GPUBackend::BackendType type);
    
    /**
     * @brief Get available GPU backends
     */
    static QVector<GPUBackend::BackendType> getAvailableBackends();
};
