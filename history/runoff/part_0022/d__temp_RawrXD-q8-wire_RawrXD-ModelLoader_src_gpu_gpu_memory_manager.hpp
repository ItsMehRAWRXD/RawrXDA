#pragma once
#include <cstdint>
#include <vector>
#include <map>
#include <memory>
#include <queue>
#include <functional>
#include <mutex>
#include <QString>

/**
 * @class GPUMemoryManager
 * @brief Unified memory management for CUDA and HIP
 * 
 * Features:
 * - Supports both NVIDIA (CUDA) and AMD (HIP/ROCm) GPUs
 * - Memory pooling to reduce allocation overhead
 * - Automatic LRU-based eviction when memory runs low
 * - Per-tensor caching and management
 * - Async copy operations with callback support
 * - Fragmentation tracking and compaction
 */
class GPUMemoryManager {
public:
    enum GPUBackend { CUDA, HIP };
    
    struct MemoryBlock {
        uint8_t* ptr = nullptr;
        uint64_t size = 0;
        bool inUse = false;
        qint64 lastAccessTime = 0;
        QString tensorName;
        uint32_t accessCount = 0;
    };
    
    struct TensorAllocation {
        QString tensorId;
        uint64_t size = 0;
        void* gpuPtr = nullptr;
        void* cpuPtr = nullptr;
        bool isDirty = false;
        bool isLocked = false;
        qint64 createdAt = 0;
        qint64 lastAccessAt = 0;
        int deviceId = 0;
    };
    
    struct MemoryStats {
        uint64_t totalAllocated = 0;
        uint64_t totalUsed = 0;
        uint64_t totalCached = 0;
        float fragmentationRatio = 0.0f;
        int activeAllocations = 0;
        int cachedChunks = 0;
        uint64_t peakMemoryUsage = 0;
    };
    
    struct CopyOperation {
        void* source = nullptr;
        void* destination = nullptr;
        uint64_t size = 0;
        bool isHostToDevice = false;
        std::function<void(bool)> callback;
        QString operationId;
    };
    
    explicit GPUMemoryManager(GPUBackend backend);
    ~GPUMemoryManager();
    
    // Initialization
    bool initialize(int deviceId = 0);
    void shutdown();
    bool isInitialized() const;
    
    // Memory allocation
    void* allocateTensor(const QString& tensorId, uint64_t size, 
                        bool pinHostMemory = false);
    void releaseTensor(const QString& tensorId);
    void* allocateTemporary(uint64_t size);
    void releaseTemporary(void* ptr);
    
    // Tensor management
    TensorAllocation getTensorInfo(const QString& tensorId) const;
    bool tensorExists(const QString& tensorId) const;
    std::vector<TensorAllocation> getAllAllocations() const;
    
    // Memory transfer
    bool copyToDevice(const QString& tensorId, const void* cpuData, uint64_t size);
    bool copyToHost(const QString& tensorId, void* cpuData, uint64_t size);
    
    // Async operations
    QString copyToDeviceAsync(const QString& tensorId, const void* cpuData, uint64_t size,
                             std::function<void(bool)> callback = nullptr);
    QString copyToHostAsync(const QString& tensorId, void* cpuData, uint64_t size,
                           std::function<void(bool)> callback = nullptr);
    bool waitForCopy(const QString& operationId, uint32_t timeoutMs = 5000);
    
    // Memory management
    uint64_t getTotalMemory() const;
    uint64_t getAvailableMemory() const;
    uint64_t getUsedMemory() const;
    MemoryStats getMemoryStats() const;
    
    // Optimization
    bool compactMemory();
    bool evictLRU();
    void analyzeFragmentation();
    
    // Configuration
    void setMaxMemory(uint64_t maxBytes);
    void setPoolSize(uint64_t bytes);
    void setEvictionThreshold(float ratio);
    void enableAsyncTransfers(bool enable);
    
    // Debugging
    void printMemoryLayout() const;
    QString getMemoryReport() const;
    
private:
    struct PoolChunk {
        void* ptr = nullptr;
        uint64_t size = 0;
        bool inUse = false;
    };
    
    struct AsyncTransfer {
        CopyOperation op;
        bool completed = false;
        qint64 startTime = 0;
    };
    
    GPUBackend m_backend;
    int m_deviceId = 0;
    bool m_initialized = false;
    
    uint64_t m_totalMemory = 0;
    uint64_t m_maxMemory = 24ULL * 1024 * 1024 * 1024; // 24GB default
    uint64_t m_poolSize = 512 * 1024 * 1024; // 512MB pool
    float m_evictionThreshold = 0.9f; // Evict at 90% usage
    bool m_asyncEnabled = true;
    
    // Memory tracking
    std::map<QString, TensorAllocation> m_tensorAllocations;
    std::map<QString, AsyncTransfer> m_pendingTransfers;
    std::vector<PoolChunk> m_memoryPool;
    std::queue<void*> m_freePool;
    
    mutable std::mutex m_mutex;
    
    // Stats
    MemoryStats m_stats;
    uint64_t m_peakUsage = 0;
    
    // Helper methods
    void* allocateFromPool(uint64_t size);
    void returnToPool(void* ptr, uint64_t size);
    bool shouldEvict() const;
    QString findLRUTensor() const;
    float calculateFragmentation() const;
    
    // CUDA-specific
    bool initializeCUDA();
    void* allocateCUDAMemory(uint64_t size);
    void releaseCUDAMemory(void* ptr);
    bool copyCUDAToDevice(void* dest, const void* src, uint64_t size);
    bool copyCUDAToHost(void* dest, const void* src, uint64_t size);
    
    // HIP-specific
    bool initializeHIP();
    void* allocateHIPMemory(uint64_t size);
    void releaseHIPMemory(void* ptr);
    bool copyHIPToDevice(void* dest, const void* src, uint64_t size);
    bool copyHIPToHost(void* dest, const void* src, uint64_t size);
};

#endif // GPU_MEMORY_MANAGER_HPP
