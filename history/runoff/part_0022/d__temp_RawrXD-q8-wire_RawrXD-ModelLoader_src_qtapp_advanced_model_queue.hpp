#pragma once
#include <QObject>
#include <QQueue>
#include <QString>
#include <QMutex>
#include <QWaitCondition>
#include <memory>
#include <functional>
#include <map>
#include <vector>

// Forward declarations
class GPUBackend;
struct GGUFParser;
struct GGUFTensorInfo;
enum class GGMLType : uint32_t;

// Forward declarations
class GPUBackend;
struct GGUFParser;
struct GGUFTensorInfo;
enum class GGMLType : uint32_t;

/**
 * @class AdvancedModelQueue
 * @brief Enterprise model management with hot-swapping
 * 
 * Features:
 * - Concurrent model loading (2+ models)
 * - Model hot-swapping without downtime
 * - Request queueing with priority
 * - Memory management and optimization
 * - Model preloading and caching
 * - Automatic model eviction
 */
class AdvancedModelQueue : public QObject {
    Q_OBJECT
    
public:
    enum Priority { Low = 0, Normal = 1, High = 2, Critical = 3 };
    
    enum ModelState {
        Unloaded,
        Loading,
        Loaded,
        Unloading,
        Error
    };
    
    struct ModelInfo {
        QString path;
        ModelState state = Unloaded;
        qint64 loadedAt = 0;
        qint64 lastAccessAt = 0;
        uint64_t memoryUsage = 0;
        int accessCount = 0;
        float averageLatency = 0.0f;
        bool isPinned = false; // Won't be evicted
    };
    
    struct InferenceRequest {
        int requestId = -1;
        QString modelPath;
        QString prompt;
        int maxTokens = 50;
        Priority priority = Normal;
        std::function<void(bool, const QString&)> callback;
        qint64 createdAt = 0;
        bool preload = false; // Preload model if needed
    };
    
    struct HotSwapTarget {
        QString sourceModel;
        QString targetModel;
        bool preserveState = true;
        float expectedLatency = 0.0f;
    };
    
    explicit AdvancedModelQueue(QObject* parent = nullptr);
    ~AdvancedModelQueue();
    
    // Model management
    int enqueueInference(const InferenceRequest& request);
    int preloadModel(const QString& path, Priority priority = Normal);
    bool hotSwapModel(const HotSwapTarget& swap);
    
    // Query methods
    ModelInfo getModelInfo(const QString& path) const;
    std::vector<ModelInfo> getAllModels() const;
    bool isModelLoaded(const QString& path) const;
    ModelState getModelState(const QString& path) const;
    
    // Configuration
    void setMaxConcurrentLoads(int count);
    void setMaxMemoryMB(uint64_t maxMemory);
    void setPinModel(const QString& path, bool pinned);
    void setPreloadThreshold(float threshold);
    
    // Queue management
    int getPendingRequestCount() const;
    int getActiveLoadCount() const;
    void clearQueue();
    void prioritizeRequest(int requestId);
    
    // Optimization
    void enableAutoOptimization(bool enable);
    void enableCaching(bool enable);
    void analyzePerformance();
    
    // Memory management
    uint64_t getTotalMemoryUsage() const;
    uint64_t getAvailableMemory() const;
    bool evictLRUModel();
    void compactMemory();
    
    // Advanced features
    void startBenchmarking();
    void stopBenchmarking();
    std::map<QString, float> getBenchmarkResults() const;
    
signals:
    // Model lifecycle
    void modelLoadStarted(const QString& path);
    void modelLoadCompleted(const QString& path, bool success);
    void modelUnloaded(const QString& path);
    void modelHotSwapped(const QString& from, const QString& to);
    
    // Request management
    void requestQueued(int requestId, const QString& model);
    void requestStarted(int requestId);
    void requestCompleted(int requestId, bool success);
    
    // Performance
    void performanceMetricsUpdated(const QString& model, float latency, float throughput);
    void memoryWarning(uint64_t used, uint64_t max);
    void optimizationApplied(const QString& description);
    
    // Status
    void queueStatusChanged(int pending, int active);
    void modelStateChanged(const QString& path, ModelState oldState, ModelState newState);
    
private slots:
    void processQueue();
    void onModelLoadFinished(const QString& path, bool success);
    void performMemoryManagement();
    
private:
    struct LoadingModel {
        QString path;
        qint64 startTime;
        int requestId;
    };
    
    struct CacheEntry {
        QString modelPath;
        std::vector<uint8_t> cachedData;
        qint64 createdAt;
        bool valid;
    };
    
    QQueue<InferenceRequest> m_requestQueue;
    std::map<QString, ModelInfo> m_loadedModels;
    std::vector<LoadingModel> m_loadingModels;
    std::map<QString, CacheEntry> m_modelCache;
    
    QMutex m_mutex;
    QWaitCondition m_queueUpdated;
    
    int m_nextRequestId = 1;
    int m_maxConcurrentLoads = 2;
    uint64_t m_maxMemoryMB = 24000; // 24GB default
    std::map<QString, std::vector<float>> m_performanceHistory;
    
    // GPU Backend
    std::unique_ptr<GPUBackend> m_gpuBackend;
    bool m_gpuInitialized;
    std::map<QString, void*> m_gpuModelBuffers;      // GPU memory pointers
    std::map<QString, uint64_t> m_gpuModelSizes;    // GPU memory sizes
    
    // Helper methods
    int findBestRequest();
    bool tryLoadModel(const InferenceRequest& request);
    bool shouldEvict() const;
    QString findLRUModel() const;
    void updateModelStats(const QString& path, float latency);
    void applyAutoOptimizations();
    ModelState updateModelState(const QString& path, ModelState newState);
    
    // GPU and model loading methods
    void initializeGPUBackend();
    bool loadGGUFModel(const QString& path);
    bool loadModelToGPU(const QString& path, const GGUFParser& parser, uint64_t totalSize);
    bool shouldDequantizeOnGPU(GGMLType type) const;
    bool dequantizeTensorOnGPU(const GGUFTensorInfo& tensor, void* gpuBuffer, uint64_t offset);
};

#endif // ADVANCED_MODEL_QUEUE_HPP
