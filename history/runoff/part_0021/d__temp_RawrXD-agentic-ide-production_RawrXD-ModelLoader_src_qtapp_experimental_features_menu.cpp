#include "experimental_features_menu.hpp"
#include <QMessageBox>
#include <QDebug>
#include <QTime>
#include <QProcess>
#include <QFileInfo>
#include <QDir>
#ifdef _WIN32
#include <windows.h>
#endif

// Complete class definitions required by unique_ptr destructor
class DynamicQuantizer {
public:
    bool quantize(const QString&) { return true; }
};

class SpeculativeDecoder {
public:
    void setLookahead(int) {}
};

class SmartKVCacheManager {
public:
    void optimize() {}
};

class StreamingModelLoader { 
public:
    void setMemoryBudget(size_t) {}
};

class HardwareProfiler { 
public:
    QMap<QString, double> analyzeSystem() {
        QMap<QString,double> m; m["gpu_memory_gb"]=0; m["system_ram_gb"]=8; m["cpu_cores"]=4; return m;
    }
};

class ModelPruner {
public:
    bool loadPruningStrategies(const std::string&) { return true; }
};

ExperimentalFeaturesMenu::ExperimentalFeaturesMenu(QWidget* parent)
    : QMenu("Experimental Features", parent) {
    metrics = &MetricsCollector::instance();
    performanceTimer = new QTimer(this);
    initialize();
}

ExperimentalFeaturesMenu::~ExperimentalFeaturesMenu() {}

void ExperimentalFeaturesMenu::initialize() {
    dynamicQuantAction = addAction("Dynamic Quantization (4-bit/3-bit)");
    dynamicQuantAction->setCheckable(true);
    connect(dynamicQuantAction, &QAction::toggled, this, &ExperimentalFeaturesMenu::toggleDynamicQuantization);

    speculativeDecodeAction = addAction("Speculative Decoding");
    speculativeDecodeAction->setCheckable(true);
    connect(speculativeDecodeAction, &QAction::toggled, this, &ExperimentalFeaturesMenu::toggleSpeculativeDecoding);

    kvCacheAction = addAction("KV Cache Compression (NVFP4)");
    kvCacheAction->setCheckable(true);
    connect(kvCacheAction, &QAction::toggled, this, &ExperimentalFeaturesMenu::toggleKVCacheCompression);

    streamingLoaderAction = addAction("Streaming Model Loader");
    streamingLoaderAction->setCheckable(true);
    connect(streamingLoaderAction, &QAction::toggled, this, &ExperimentalFeaturesMenu::toggleStreamingLoader);

    modelPruningAction = addAction("Adaptive Model Pruning");
    modelPruningAction->setCheckable(true);
    connect(modelPruningAction, &QAction::toggled, this, &ExperimentalFeaturesMenu::toggleModelPruning);

    addSeparator();
    memoryStatusAction = addAction("Memory: Monitoring...");
    memoryStatusAction->setEnabled(false);

    connect(performanceTimer, &QTimer::timeout, this, &ExperimentalFeaturesMenu::updateMemoryDisplay);
    performanceTimer->start(2000);

    validateHardwareCapabilities();
}

bool ExperimentalFeaturesMenu::isFeatureEnabled(const QString& feature) const {
    if (feature == "dynamic_quantization") return dynamicQuantEnabled.load();
    if (feature == "speculative_decoding") return speculativeDecodeEnabled.load();
    if (feature == "kv_cache_compression") return kvCacheCompressionEnabled.load();
    if (feature == "streaming_loader") return streamingLoaderEnabled.load();
    if (feature == "model_pruning") return modelPruningEnabled.load();
    return false;
}

void ExperimentalFeaturesMenu::setMemoryLimit(size_t max_memory_mb) {
    maxMemoryBudget = max_memory_mb;
}

void ExperimentalFeaturesMenu::toggleDynamicQuantization(bool enabled) {
    std::lock_guard<std::mutex> g(featureMutex);
    dynamicQuantEnabled = enabled;
    logFeatureUsage("dynamic_quantization", enabled);
    emit featureToggled("dynamic_quantization", enabled);
}

void ExperimentalFeaturesMenu::toggleSpeculativeDecoding(bool enabled) {
    std::lock_guard<std::mutex> g(featureMutex);
    speculativeDecodeEnabled = enabled;
    logFeatureUsage("speculative_decoding", enabled);
    emit featureToggled("speculative_decoding", enabled);
}

void ExperimentalFeaturesMenu::toggleKVCacheCompression(bool enabled) {
    std::lock_guard<std::mutex> g(featureMutex);
    kvCacheCompressionEnabled = enabled;
    logFeatureUsage("kv_cache_compression", enabled);
    emit featureToggled("kv_cache_compression", enabled);
}

void ExperimentalFeaturesMenu::toggleStreamingLoader(bool enabled) {
    std::lock_guard<std::mutex> g(featureMutex);
    streamingLoaderEnabled = enabled;
    logFeatureUsage("streaming_loader", enabled);
    emit featureToggled("streaming_loader", enabled);
}

void ExperimentalFeaturesMenu::toggleModelPruning(bool enabled) {
    std::lock_guard<std::mutex> g(featureMutex);
    modelPruningEnabled = enabled;
    logFeatureUsage("model_pruning", enabled);
    emit featureToggled("model_pruning", enabled);
}

void ExperimentalFeaturesMenu::updateMemoryDisplay() {
#ifdef _WIN32
    MEMORYSTATUSEX status; status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    const size_t total = static_cast<size_t>(status.ullTotalPhys);
    const size_t avail = static_cast<size_t>(status.ullAvailPhys);
#else
    const size_t total = 0, avail = 0;
#endif
    currentMemoryUsage = total - avail;
    const double used_gb = (total - avail) / (1024.0*1024.0*1024.0);
    const double total_gb = total / (1024.0*1024.0*1024.0);
    const double avail_gb = avail / (1024.0*1024.0*1024.0);
    memoryStatusAction->setText(QString("Memory: %1/%2 GB Available: %3 GB")
        .arg(used_gb,0,'f',1).arg(total_gb,0,'f',1).arg(avail_gb,0,'f',1));
    emit memoryUsageChanged(static_cast<size_t>((total-avail)/(1024*1024)));
}

void ExperimentalFeaturesMenu::validateHardwareCapabilities() {
    auto caps = hardwareProfiler ? hardwareProfiler->analyzeSystem() : QMap<QString,double>{};
    Q_UNUSED(caps);
    // All actions enabled; finer gating can be added later
    for (auto* act : {dynamicQuantAction, speculativeDecodeAction, kvCacheAction, streamingLoaderAction, modelPruningAction}) {
        if (act) act->setEnabled(true);
    }
}

bool ExperimentalFeaturesMenu::initializeDynamicQuantizer() { return true; }
bool ExperimentalFeaturesMenu::initializeSpeculativeDecoder() { return true; }
bool ExperimentalFeaturesMenu::initializeKVCacheManager() { return true; }
bool ExperimentalFeaturesMenu::initializeStreamingLoader() { return true; }
bool ExperimentalFeaturesMenu::initializeModelPruner() { return true; }

void ExperimentalFeaturesMenu::validateFeatureCompatibility() {}
void ExperimentalFeaturesMenu::updateMenuStates() {}

void ExperimentalFeaturesMenu::logFeatureUsage(const QString& feature, bool enabled) {
    QMap<QString, QVariant> props; props["feature"]=feature; props["enabled"]=enabled; props["memory_mb"]=static_cast<qulonglong>(currentMemoryUsage/(1024*1024));
    if (metrics) metrics->recordEvent("experimental_feature_toggle", props);
    qInfo() << "EXPERIMENTAL FEATURE TOGGLE:" << feature << (enabled?"ENABLED":"DISABLED");
}
