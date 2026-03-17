#pragma once
#include <QObject>
#include <QTimer>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <functional>

#include "streaming_gguf_memory_manager.hpp"

class LazyModelLoader : public QObject {
    Q_OBJECT

public:
    explicit LazyModelLoader(QObject* parent = nullptr);
    ~LazyModelLoader();

    // Core lazy loading functionality
    bool initialize(StreamingGGUFMemoryManager* memory_manager);
    void shutdown();
    
    // Lazy model loading
    bool registerModel(const std::string& model_path, const std::string& model_id);
    bool loadModelLazy(const std::string& model_id);
    bool unloadModelLazy(const std::string& model_id);
    bool isModelLoaded(const std::string& model_id) const;
    
    // On-demand tensor access
    std::vector<float> getTensorLazy(const std::string& model_id, 
                                    const std::string& tensor_name,
                                    size_t offset = 0,
                                    size_t count = 0);
    
    // Model size estimation for large models
    size_t estimateModelSize(const std::string& model_path);
    bool canFitInMemory(const std::string& model_path, size_t available_memory) const;
    std::string suggestOptimalQuantization(const std::string& model_path, size_t target_memory) const;
    
    // Loading strategies
    enum class LoadingStrategy {
        FULL_LAZY = 0,        // Load only when accessed
        PREFETCH_CRITICAL = 1, // Prefetch embedding/lm_head layers
        LAYER_WISE = 2,       // Load layer by layer
        ADAPTIVE = 3          // Adaptive based on access patterns
    };
    
    void setLoadingStrategy(LoadingStrategy strategy) { loading_strategy = strategy; }
    void setMaxMemoryUsage(size_t max_bytes) { max_memory_usage = max_bytes; }
    void setPrefetchCriticalLayers(bool enable) { prefetch_critical_layers = enable; }

signals:
    void modelLoadStarted(const QString& model_id);
    void modelLoadProgress(const QString& model_id, double progress);
    void modelLoadCompleted(const QString& model_id);
    void tensorLoadRequested(const QString& model_id, const QString& tensor_name);
    void memoryUsageChanged(size_t used_memory, size_t total_memory);
    void loadingStrategyApplied(const QString& strategy, double memory_saved);

private slots:
    void onMemoryPressure(int level, size_t current_usage, size_t budget);
    void optimizeLoadingStrategy();
    void prefetchCriticalLayers();

private:
    // Core components
    StreamingGGUFMemoryManager* memory_manager;
    LoadingStrategy loading_strategy{LoadingStrategy::ADAPTIVE};
    std::atomic<bool> initialized{false};
    
    // Model registry
    struct ModelInfo {
        std::string model_path;
        std::string model_id;
        size_t total_size;
        size_t loaded_size;
        bool fully_loaded;
        std::vector<std::string> critical_tensors;
        std::vector<std::string> layer_tensors;
        std::unordered_map<std::string, bool> tensor_loaded;
        std::chrono::steady_clock::time_point load_start_time;
    };
    
    std::unordered_map<std::string, std::unique_ptr<ModelInfo>> registered_models;
    std::unordered_map<std::string, std::string> model_paths;
    
    // Memory management
    std::atomic<size_t> current_memory_usage{0};
    std::atomic<size_t> max_memory_usage{64ULL * 1024 * 1024 * 1024}; // 64GB default
    std::atomic<bool> prefetch_critical_layers{true};
    
    // Access tracking
    std::unordered_map<std::string, std::vector<std::string>> access_patterns;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_access_time;
    
    // Optimization
    QTimer* optimization_timer;
    QTimer* prefetch_timer;
    
    // Loading implementation
    bool loadCriticalTensors(const std::string& model_id);
    bool loadLayerTensors(const std::string& model_id, size_t layer_index);
    bool loadTensorOnDemand(const std::string& model_id, const std::string& tensor_name);
    
    // Memory optimization
    void identifyCriticalTensors(ModelInfo& model_info);
    void identifyLayerTensors(ModelInfo& model_info);
    size_t calculateTensorSize(const std::string& model_path, const std::string& tensor_name);
    std::vector<std::string> getTensorsForLayer(const std::string& model_id, size_t layer_index);
    
    // Strategy optimization
    LoadingStrategy determineOptimalStrategy(const std::string& model_id);
    bool applyLoadingStrategy(const std::string& model_id, LoadingStrategy strategy);
    void optimizeMemoryLayout(const std::string& model_id);
    
    // Utility
    std::string makeModelKey(const std::string& model_path) const;
    bool isCriticalTensor(const std::string& tensor_name) const;
    bool isLayerTensor(const std::string& tensor_name) const;
    size_t getLayerIndex(const std::string& tensor_name) const;
};
