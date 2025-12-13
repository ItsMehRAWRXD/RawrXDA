#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QString>
#include <QVector>
#include <memory>

// Enterprise-grade model recommendation structure
struct ModelRecommendation {
    QString modelId;
    QString name;
    double suitabilityScore;
    QString reasoning;
    qint64 estimatedDownloadSize;
    qint64 estimatedMemoryUsage;
    QString taskType; // "completion", "chat", "analysis"
    QString complexityLevel; // "simple", "medium", "complex"
};

// System capability analysis structure
struct SystemAnalysis {
    qint64 availableRAM;
    qint64 availableDiskSpace;
    int cpuCores;
    bool hasGPU;
    QString gpuType;
    qint64 gpuMemory;
};

/**
 * @brief Enterprise Autonomous Model Management System
 * 
 * Provides intelligent, autonomous model selection, download, optimization, and management.
 * Integrates with HuggingFace for model discovery and implements enterprise-grade features.
 */
class AutonomousModelManager : public QObject {
    Q_OBJECT
    
private:
    QNetworkAccessManager* networkManager;
    QTimer* autoUpdateTimer;
    QTimer* optimizationTimer;
    
    QJsonArray availableModels;
    QJsonArray installedModels;
    QJsonArray recommendedModels;
    
    SystemAnalysis currentSystem;
    QString lastRecommendedModel;
    
    // Configuration
    QString huggingFaceApiEndpoint = "https://huggingface.co/api";
    QString modelsRepository = "models";
    int autoUpdateInterval = 3600000; // 1 hour
    int optimizationInterval = 1800000; // 30 minutes
    double minimumSuitabilityScore = 0.75;
    qint64 maxModelSize = 10LL * 1024 * 1024 * 1024; // 10GB default
    
public:
    explicit AutonomousModelManager(QObject* parent = nullptr);
    ~AutonomousModelManager();
    
    // Autonomous operations
    ModelRecommendation autoDetectBestModel(const QString& taskType, const QString& language);
    bool autoDownloadAndSetup(const QString& modelId);
    bool autoUpdateModels();
    bool autoOptimizeModel(const QString& modelId);
    
    // Intelligent analysis
    SystemAnalysis analyzeSystemCapabilities();
    double calculateModelSuitability(const QJsonObject& model, const QString& taskType);
    QJsonArray analyzeCodebaseRequirements(const QString& projectPath);
    
    // Model management
    QJsonArray getAvailableModels();
    QJsonArray getInstalledModels();
    QJsonArray getRecommendedModels(const QString& taskType = "");
    bool installModel(const QString& modelId);
    bool uninstallModel(const QString& modelId);
    bool updateModel(const QString& modelId);
    
    // Intelligent recommendations
    ModelRecommendation recommendModelForTask(const QString& task, const QString& language);
    ModelRecommendation recommendModelForCodebase(const QString& projectPath);
    ModelRecommendation recommendModelForPerformance(qint64 targetLatency);
    
    // System integration
    bool integrateWithHuggingFace();
    bool syncWithModelRegistry();
    bool validateModelIntegrity(const QString& modelId);
    
signals:
    void modelRecommended(const ModelRecommendation& recommendation);
    void modelInstalled(const QString& modelId);
    void modelUpdated(const QString& modelId);
    void systemAnalysisComplete(const SystemAnalysis& analysis);
    void recommendationReady(const ModelRecommendation& recommendation);
    void autoUpdateCompleted();
    void errorOccurred(const QString& error);

private:
    void setupNetworkManager();
    void setupTimers();
    void loadAvailableModels();
    void loadInstalledModels();
    void saveInstalledModels();
    QJsonObject fetchModelInfo(const QString& modelId);
    bool downloadModelFile(const QString& url, const QString& destination);
    bool validateDownloadedModel(const QString& modelPath);
    bool optimizeModelForSystem(const QString& modelId);
    SystemAnalysis getCurrentSystemAnalysis();
    qint64 getAvailableRAM();
    qint64 getAvailableDiskSpace();
    bool detectGPU();
    QString generateRecommendationReasoning(const QJsonObject& model, const SystemAnalysis& system, const QString& taskType);
    qint64 estimateMemoryUsage(const QJsonObject& model);
    QString determineComplexityLevel(const QJsonObject& model);
    double estimateLatency(const QJsonObject& model);
};
