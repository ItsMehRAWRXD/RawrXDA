#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstdint>
#include <nlohmann/json.hpp>

// Enterprise-grade model recommendation structure
struct ModelRecommendation {
    std::string modelId;
    std::string name;
    double suitabilityScore;
    std::string reasoning;
    int64_t estimatedDownloadSize;
    int64_t estimatedMemoryUsage;
    std::string taskType; // "completion", "chat", "analysis"
    std::string complexityLevel; // "simple", "medium", "complex"
};

// System capability analysis structure
struct SystemAnalysis {
    int64_t availableRAM;
    int64_t availableDiskSpace;
    int cpuCores;
    bool hasGPU;
    std::string gpuType;
    int64_t gpuMemory;
};

/**
 * @brief Enterprise Autonomous Model Management System
 * 
 * Provides intelligent, autonomous model selection, download, optimization, and management.
 */
class AutonomousModelManager {
private:
    nlohmann::json availableModels;
    nlohmann::json installedModels;
    nlohmann::json recommendedModels;
    
    SystemAnalysis currentSystem;
    std::string lastRecommendedModel;
    
    // Configuration
    std::string huggingFaceApiEndpoint = "https://huggingface.co/api";
    std::string modelsRepository = "models";
    int autoUpdateInterval = 3600; // 1 hour (seconds)
    int optimizationInterval = 1800; // 30 minutes (seconds)
    double minimumSuitabilityScore = 0.75;
    int64_t maxModelSize = 10LL * 1024 * 1024 * 1024; // 10GB default
    
public:
    AutonomousModelManager();
    ~AutonomousModelManager();
    
    // Callbacks
    std::function<void(const ModelRecommendation&)> onModelRecommended;
    std::function<void(const std::string&)> onModelInstalled;
    std::function<void(const std::string&)> onModelUpdated;
    std::function<void(const SystemAnalysis&)> onSystemAnalysisComplete;
    std::function<void(const std::string&, int, int64_t, int64_t)> onDownloadProgress;
    std::function<void(const std::string&, bool)> onDownloadCompleted;
    std::function<void(const std::string&)> onError;

    // Autonomous operations
    ModelRecommendation autoDetectBestModel(const std::string& taskType, const std::string& language);
    bool autoDownloadAndSetup(const std::string& modelId);
    bool autoUpdateModels();
    bool autoOptimizeModel(const std::string& modelId);
    
    // Intelligent analysis
    SystemAnalysis analyzeSystemCapabilities();
    double calculateModelSuitability(const nlohmann::json& model, const std::string& taskType);
    nlohmann::json analyzeCodebaseRequirements(const std::string& projectPath);
    
    // Model management
    nlohmann::json getAvailableModels();
    nlohmann::json getInstalledModels();
    nlohmann::json getRecommendedModels(const std::string& taskType = "");
    bool installModel(const std::string& modelId);
    bool uninstallModel(const std::string& modelId);
    bool updateModel(const std::string& modelId);
    
    // Intelligent recommendations
    ModelRecommendation recommendModelForTask(const std::string& task, const std::string& language);
    ModelRecommendation recommendModelForCodebase(const std::string& projectPath);
    ModelRecommendation recommendModelForPerformance(int64_t targetLatency);
    
    // System integration
    bool integrateWithHuggingFace();
    bool syncWithModelRegistry();
    bool validateModelIntegrity(const std::string& modelId);

private:
    void setupNetworking();
    void startMaintenanceThreads();
    void loadAvailableModels();
    void loadInstalledModels();
};

