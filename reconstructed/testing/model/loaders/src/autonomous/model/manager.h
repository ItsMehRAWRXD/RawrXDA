// autonomous_model_manager.h - Autonomous AI Model Management System
// Converted from Qt to pure C++17
#ifndef AUTONOMOUS_MODEL_MANAGER_H
#define AUTONOMOUS_MODEL_MANAGER_H

#include "common/json_types.hpp"
#include "common/callback_system.hpp"
#include "common/time_utils.hpp"
#include "common/string_utils.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <cstdint>

// System capability information
struct SystemCapabilities {
    int64_t totalRAM = 0;
    int64_t availableRAM = 0;
    int cpuCores = 0;
    double cpuFrequency = 0.0;
    bool hasGPU = false;
    std::string gpuName;
    int64_t gpuMemory = 0;
    std::string platform;
    bool hasAVX2 = false;
    bool hasAVX512 = false;
    TimePoint timestamp;
};

// AI Model metadata
struct AIModel {
    std::string modelId;
    std::string name;
    std::string provider;
    std::string taskType;
    std::vector<std::string> languages;
    int64_t sizeBytes = 0;
    std::string quantization;
    int contextLength = 0;
    double performance = 0.0;
    int64_t minRAM = 0;
    bool requiresGPU = false;
    std::string downloadUrl;
    std::string localPath;
    bool isDownloaded = false;
    TimePoint lastUsed;
    JsonObject metadata;
};

// Model recommendation with confidence
struct ModelRecommendation {
    AIModel model;
    double confidence = 0.0;
    std::string reasoning;
    JsonObject metrics;
    std::vector<std::string> alternatives;
};

// Download progress tracking
struct DownloadProgress {
    std::string modelId;
    int64_t bytesReceived = 0;
    int64_t bytesTotal = 0;
    double percentage = 0.0;
    double speedBytesPerSec = 0.0;
    TimePoint startTime;
    TimePoint estimatedCompletion;
    std::string status;
    std::string errorMessage;
};

// System analysis result
struct SystemAnalysis {
    SystemCapabilities capabilities;
    std::vector<AIModel> compatibleModels;
    std::vector<AIModel> recommendedModels;
    std::vector<std::string> limitations;
    JsonObject recommendations;
    double overallScore = 0.0;
};

class AutonomousModelManager {
public:
    AutonomousModelManager();
    ~AutonomousModelManager();

    ModelRecommendation autoDetectBestModel(const std::string& taskType,
                                            const std::string& language = "cpp");
    std::vector<ModelRecommendation> getTopRecommendations(const std::string& taskType,
                                                            const std::string& language,
                                                            int topN = 3);
    bool autoDownloadAndSetup(const std::string& modelId);
    bool downloadModel(const std::string& modelId, const std::string& destinationPath);
    DownloadProgress getDownloadProgress(const std::string& modelId) const;
    void cancelDownload(const std::string& modelId);

    SystemAnalysis analyzeSystemCapabilities();
    SystemCapabilities getCurrentCapabilities() const;
    bool isModelCompatible(const AIModel& model) const;

    std::vector<AIModel> getAvailableModels() const;
    std::vector<AIModel> getInstalledModels() const;
    AIModel getModelInfo(const std::string& modelId) const;
    bool loadModel(const std::string& modelId);
    bool unloadCurrentModel();
    std::string getCurrentModelId() const;

    void refreshModelRegistry();
    void addCustomModel(const AIModel& model);
    bool removeModel(const std::string& modelId);

    void enableAutoUpdate(bool enable);
    void checkForModelUpdates();
    void cleanupUnusedModels(int daysUnused = 30);

    JsonObject getModelPerformanceStats(const std::string& modelId) const;
    void recordModelUsage(const std::string& modelId, double latency, bool success);

    void setModelCacheDirectory(const std::string& path);
    void setMaxCacheSize(int64_t bytes);
    void setPreferredQuantization(const std::string& quantization);
    std::string getModelCacheDirectory() const;

    // Callbacks (replacing Qt signals)
    CallbackList<const ModelRecommendation&> onModelRecommendationReady;
    CallbackList<const std::string&> onDownloadStarted;
    CallbackList<const std::string&, double> onDownloadProgress;
    CallbackList<const std::string&, bool> onDownloadComplete;
    CallbackList<const std::string&> onModelLoaded;
    CallbackList<const std::string&> onModelUnloaded;
    CallbackList<int> onRegistryUpdated;
    CallbackList<const SystemCapabilities&> onSystemCapabilitiesChanged;
    CallbackList<const std::string&> onErrorOccurred;

private:
    double calculateModelSuitability(const AIModel& model, const std::string& taskType,
                                      const std::string& language) const;
    double calculateTaskCompatibility(const AIModel& model, const std::string& taskType) const;
    double calculateLanguageSupport(const AIModel& model, const std::string& language) const;
    double calculatePerformanceScore(const AIModel& model) const;
    double calculateResourceEfficiency(const AIModel& model) const;

    int64_t detectTotalRAM() const;
    int64_t detectAvailableRAM() const;
    int detectCPUCores() const;
    double detectCPUFrequency() const;
    bool detectGPU(std::string& gpuName, int64_t& gpuMemory) const;
    bool detectAVX2Support() const;
    bool detectAVX512Support() const;
    std::string detectPlatform() const;

    void loadModelRegistry();
    void saveModelRegistry();
    void parseModelMetadata(const JsonObject& json, AIModel& model);
    bool validateDownload(const std::string& filePath, const std::string& expectedHash = "");
    void optimizeModelForSystem(const std::string& modelPath);
    int64_t calculateCacheSize() const;

    std::vector<AIModel> availableModels;
    std::vector<AIModel> installedModels;
    std::unordered_map<std::string, DownloadProgress> activeDownloads;
    std::unordered_map<std::string, JsonObject> performanceStats;

    SystemCapabilities systemCapabilities;
    std::string currentModelId;
    std::string modelCacheDirectory;
    int64_t maxCacheSize;
    std::string preferredQuantization;
    bool autoUpdateEnabled = false;
    std::unique_ptr<std::thread> autoUpdateThread;
    std::atomic<bool> autoUpdateRunning{false};

    static constexpr int AUTO_UPDATE_INTERVAL_HOURS = 24;
    static constexpr int64_t DEFAULT_MAX_CACHE_SIZE = 50LL * 1024 * 1024 * 1024;
    static constexpr double MIN_CONFIDENCE_THRESHOLD = 0.6;
};

#endif // AUTONOMOUS_MODEL_MANAGER_H
