#pragma once
#include <QObject>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <atomic>
#include <memory>
#include <vector>
#include <unordered_map>
#include <deque>
#include <functional>
#include <chrono>

#include "streaming_gguf_loader.h"
// #include "Memory/Metrics.hpp"  // File not found, commented out

// Memory block tracking
struct MemoryBlock {
    size_t offset;
    size_t size;
    bool is_loaded;
    bool is_pinned;
    std::chrono::steady_clock::time_point last_access;
    std::chrono::steady_clock::time_point load_time;
    size_t access_count;
    std::vector<std::string> tensor_names; // Tensors in this block
    float priority_score;
};

// Streaming tensor access
struct StreamingTensorAccess {
    std::string tensor_name;
    size_t tensor_offset;
    size_t access_size;
    bool is_write;
    std::chrono::steady_clock::time_point access_time;
};

// Memory pressure levels
enum class MemoryPressure {
    NORMAL = 0,
    ELEVATED = 1,
    HIGH = 2,
    CRITICAL = 3
};

// Prefetch strategies
enum class PrefetchStrategy {
    SEQUENTIAL = 0,
    LRU_BASED = 1,
    ML_PREDICTIVE = 2,
    ADAPTIVE = 3
};

class StreamingGGUFMemoryManager : public QObject {
    Q_OBJECT

public:
    explicit StreamingGGUFMemoryManager(QObject* parent = nullptr);
    ~StreamingGGUFMemoryManager();

    // Core streaming functionality
    bool initialize(size_t max_memory_bytes = 64ULL * 1024 * 1024 * 1024); // 64GB default
    void shutdown();
    
    // Model streaming
    bool streamModel(const std::string& model_path, const std::string& model_id);
    bool unloadStreamedModel(const std::string& model_id);
    bool isModelStreamed(const std::string& model_id) const;
    
    // Tensor access with automatic loading
    std::vector<float> accessTensor(const std::string& model_id, 
                                   const std::string& tensor_name,
                                   size_t offset = 0,
                                   size_t count = 0); // 0 = all
    
    // Memory management
    size_t getCurrentMemoryUsage() const;
    size_t getMaxMemoryBudget() const { return max_memory_budget; }
    MemoryPressure getMemoryPressure() const;
    double getMemoryUtilization() const;
    
    // Streaming configuration
    void setMemoryBudget(size_t max_memory_bytes);
    void setBlockSize(size_t block_size_bytes) { memory_block_size = block_size_bytes; }
    void setPrefetchStrategy(PrefetchStrategy strategy) { prefetch_strategy = strategy; }
    void setPrefetchAhead(size_t blocks_ahead) { prefetch_ahead_blocks = blocks_ahead; }
    
    // Performance monitoring
    struct StreamingMetrics {
        size_t total_tensor_accesses = 0;
        size_t cache_hits = 0;
        size_t cache_misses = 0;
        size_t blocks_loaded = 0;
        size_t blocks_evicted = 0;
        size_t prefetch_hits = 0;
        double avg_load_time_ms = 0.0;
        double memory_utilization = 0.0;
        size_t peak_memory_usage = 0;
        std::chrono::steady_clock::time_point timestamp;
    };
    
    StreamingMetrics getStreamingMetrics() const;
    std::vector<MemoryBlock> getMemoryLayout() const;

signals:
    void memoryPressureDetected(MemoryPressure level, size_t current_usage, size_t budget);
    void tensorBlockLoaded(const QString& model_id, size_t block_offset, size_t block_size);
    void tensorBlockEvicted(const QString& model_id, size_t block_offset, size_t block_size);
    void streamingMetricsUpdated(const StreamingMetrics& metrics);
    void modelStreamingProgress(const QString& model_id, double progress_percent);
    void memoryOptimizationApplied(const QString& strategy, double memory_saved);

private slots:
    void monitorMemoryPressure();
    void optimizeMemoryLayout();
    void processPrefetchQueue();
    void updateStreamingMetrics();
    void handleMemoryPressure(MemoryPressure level);

private:
    // Memory management
    std::atomic<size_t> max_memory_budget{64ULL * 1024 * 1024 * 1024}; // 64GB
    std::atomic<size_t> current_memory_usage{0};
    std::atomic<size_t> memory_block_size{64 * 1024 * 1024}; // 64MB blocks default
    std::atomic<size_t> prefetch_ahead_blocks{4};
    std::atomic<MemoryPressure> current_pressure{MemoryPressure::NORMAL};
    
    // Model streaming state
    std::unordered_map<std::string, std::unique_ptr<StreamingGGUFLoader>> model_loaders;
    std::unordered_map<std::string, std::vector<MemoryBlock>> model_memory_blocks;
    std::unordered_map<std::string, std::unordered_map<std::string, size_t>> tensor_block_mapping;
    
    // Memory block management
    std::deque<MemoryBlock*> lru_queue;
    std::unordered_map<size_t, std::unique_ptr<MemoryBlock>> all_blocks;
    std::unordered_map<std::string, std::vector<StreamingTensorAccess>> access_patterns;
    
    // Prefetching
    PrefetchStrategy prefetch_strategy{PrefetchStrategy::ADAPTIVE};
    std::queue<size_t> prefetch_queue;
    std::unordered_map<std::string, std::vector<size_t>> prefetch_sequences;
    
    // Threading and synchronization
    QThread* streaming_thread;
    QMutex memory_mutex;
    QWaitCondition memory_condition;
    std::atomic<bool> streaming_active{false};
    
    // Monitoring
    QTimer* memory_monitor_timer;
    QTimer* optimization_timer;
    QTimer* metrics_timer;
    StreamingMetrics streaming_metrics;
    
    // Tensor data cache (partial loading)
    std::unordered_map<std::string, std::vector<uint8_t>> tensor_data_cache;
    std::unordered_map<std::string, std::pair<size_t, size_t>> tensor_cache_info; // offset, size
    
    // Core streaming methods
    bool analyzeModelStructure(const std::string& model_id);
    bool createMemoryBlocks(const std::string& model_id);
    MemoryBlock* loadBlock(const std::string& model_id, size_t block_index);
    bool evictBlock(MemoryBlock* block);
    bool pinBlock(MemoryBlock* block);
    bool unpinBlock(MemoryBlock* block);
    
    // Tensor access optimization
    MemoryBlock* findBlockContainingTensor(const std::string& model_id, 
                                          const std::string& tensor_name);
    std::vector<MemoryBlock*> getBlocksForTensorAccess(const std::string& model_id,
                                                       const std::string& tensor_name,
                                                       size_t offset, size_t count);
    bool ensureBlocksLoaded(const std::string& model_id, 
                           const std::vector<MemoryBlock*>& blocks);
    
    // Memory pressure handling
    size_t evictLRUBlocks(size_t target_bytes);
    size_t evictColdBlocks(size_t target_bytes);
    size_t evictLowPriorityBlocks(size_t target_bytes);
    void handleCriticalMemoryPressure();
    
    // Prefetching strategies
    std::vector<size_t> generateSequentialPrefetch(const std::string& model_id, 
                                                  size_t current_block);
    std::vector<size_t> generateLRUBasedPrefetch(const std::string& model_id);
    std::vector<size_t> generateMLPredictivePrefetch(const std::string& model_id, 
                                                    const std::string& tensor_name);
    std::vector<size_t> generateAdaptivePrefetch(const std::string& model_id, 
                                                const std::string& tensor_name);
    
    // Memory optimization
    double calculateBlockPriority(const MemoryBlock* block) const;
    void updateLRUQueue(MemoryBlock* block);
    void optimizeBlockPlacement();
    void defragmentMemory();
    
    // Access pattern analysis
    void recordTensorAccess(const std::string& model_id, 
                           const std::string& tensor_name,
                           size_t offset, size_t count);
    std::vector<std::string> predictNextTensors(const std::string& model_id, 
                                               const std::string& current_tensor);
    
    // Utility methods
    size_t estimateBlockSize(const MemoryBlock* block) const;
    bool isBlockLoaded(const MemoryBlock* block) const;
    std::chrono::milliseconds getBlockLoadTime(const MemoryBlock* block) const;
    MemoryPressure calculateMemoryPressure() const;
    
    // Metrics and monitoring
    void recordCacheHit(const std::string& tensor_name);
    void recordCacheMiss(const std::string& tensor_name);
    void recordBlockLoad(size_t block_size);
    void recordBlockEviction(size_t block_size);
    void updateMemoryUtilization();
    
    // Helper methods for existing GGUF loader integration
    size_t getTensorSize(const TensorInfo& tensor) const;
    size_t calculateOptimalBlockSize(size_t max_memory) const;
    std::string makeBlockKey(const std::string& model_id, size_t block_index) const;
    std::string makeBlockKeyFromBlock(const MemoryBlock* block) const;
    std::string extractModelIdFromBlockKey(const std::string& block_key) const;
    bool loadBlockData(StreamingGGUFLoader* loader, MemoryBlock* block, 
                      std::vector<uint8_t>& block_data);
};