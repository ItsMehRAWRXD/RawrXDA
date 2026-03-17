#pragma once
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QTimer>
#include <atomic>
#include <mutex>
#include <memory>
#include "gguf_loader.h"
#include "inference_engine.h"
#include "metrics_collector.hpp"

// Forward declarations for unique_ptr members
class DynamicQuantizer;
class SpeculativeDecoder;
class SmartKVCacheManager;
class StreamingModelLoader;
class ModelPruner;
class HardwareProfiler;

class ExperimentalFeaturesMenu : public QMenu {
    Q_OBJECT

public:
    explicit ExperimentalFeaturesMenu(QWidget* parent = nullptr);
    ~ExperimentalFeaturesMenu();

    void initialize();
    bool isFeatureEnabled(const QString& feature) const;
    void setMemoryLimit(size_t max_memory_mb);

private slots:
    void toggleDynamicQuantization(bool enabled);
    void toggleSpeculativeDecoding(bool enabled);
    void toggleKVCacheCompression(bool enabled);
    void toggleStreamingLoader(bool enabled);
    void toggleModelPruning(bool enabled);
    void updateMemoryDisplay();
    void validateHardwareCapabilities();

private:
    // Menu Actions
    QAction* dynamicQuantAction;
    QAction* speculativeDecodeAction;
    QAction* kvCacheAction;
    QAction* streamingLoaderAction;
    QAction* modelPruningAction;
    QAction* memoryStatusAction;
    
    // Feature States
    std::atomic<bool> dynamicQuantEnabled{false};
    std::atomic<bool> speculativeDecodeEnabled{false};
    std::atomic<bool> kvCacheCompressionEnabled{false};
    std::atomic<bool> streamingLoaderEnabled{false};
    std::atomic<bool> modelPruningEnabled{false};
    
    // Production Components
    std::unique_ptr<class DynamicQuantizer> quantizer;
    std::unique_ptr<class SpeculativeDecoder> speculativeDecoder;
    std::unique_ptr<class SmartKVCacheManager> kvCacheManager;
    std::unique_ptr<class StreamingModelLoader> streamingLoader;
    std::unique_ptr<class ModelPruner> modelPruner;
    std::unique_ptr<class HardwareProfiler> hardwareProfiler;
    
    // Thread Safety
    mutable std::mutex featureMutex;
    std::atomic<size_t> currentMemoryUsage{0};
    std::atomic<size_t> maxMemoryBudget{4096}; // 4GB default
    
    // Performance Monitoring
    MetricsCollector* metrics;
    QTimer* performanceTimer;
    
    // Real Implementation Methods
    bool initializeDynamicQuantizer();
    bool initializeSpeculativeDecoder();
    bool initializeKVCacheManager();
    bool initializeStreamingLoader();
    bool initializeModelPruner();
    
    void validateFeatureCompatibility();
    void updateMenuStates();
    void logFeatureUsage(const QString& feature, bool enabled);
    
signals:
    void featureToggled(const QString& feature, bool enabled);
    void memoryUsageChanged(size_t usage_mb);
    void performanceMetricsUpdated(const QMap<QString, double>& metrics);
};