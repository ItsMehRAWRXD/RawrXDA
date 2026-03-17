// autonomous_model_manager.h - Autonomous AI Model Management System
#ifndef AUTONOMOUS_MODEL_MANAGER_H
#define AUTONOMOUS_MODEL_MANAGER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QTimer>
#include <memory>

// System capability information
struct SystemCapabilities {
    qint64 totalRAM;           // Total RAM in bytes
    qint64 availableRAM;       // Available RAM in bytes
    int cpuCores;              // Number of CPU cores
    double cpuFrequency;       // CPU frequency in GHz
    bool hasGPU;               // GPU availability
    QString gpuName;           // GPU name/model
    qint64 gpuMemory;          // GPU memory in bytes
    QString platform;          // OS platform (Windows/Linux/macOS)
    bool hasAVX2;              // AVX2 instruction support
    bool hasAVX512;            // AVX512 instruction support
    QDateTime timestamp;       // When capabilities were measured
};

// AI Model metadata
struct AIModel {
    QString modelId;           // Unique identifier (e.g., "codellama-7b-q4")
    QString name;              // Display name
    QString provider;          // Provider (HuggingFace, Ollama, etc.)
    QString taskType;          // Task type (code_completion, chat, etc.)
    QStringList languages;     // Supported programming languages
    qint64 sizeBytes;          // Model size in bytes
    QString quantization;      // Quantization level (Q4, Q5, Q8, FP16)
    int contextLength;         // Context window size
    double performance;        // Benchmark score (0-1)
    qint64 minRAM;             // Minimum RAM required
    bool requiresGPU;          // GPU requirement
    QString downloadUrl;       // Download URL
    QString localPath;         // Local file path if downloaded
    bool isDownloaded;         // Download status
    QDateTime lastUsed;        // Last usage timestamp
    QJsonObject metadata;      // Additional metadata
};

// Model recommendation with confidence
struct ModelRecommendation {
    AIModel model;             // Recommended model
    double confidence;         // Confidence score (0-1)
    QString reasoning;         // Human-readable reasoning
    QJsonObject metrics;       // Detailed metrics
    QStringList alternatives;  // Alternative model IDs
};

// Download progress tracking
struct DownloadProgress {
    QString modelId;
    qint64 bytesReceived;
    qint64 bytesTotal;
    double percentage;
    double speedBytesPerSec;
    QDateTime startTime;
    QDateTime estimatedCompletion;
    QString status;            // "downloading", "extracting", "validating", "complete", "error"
    QString errorMessage;
};

// System analysis result
struct SystemAnalysis {
    SystemCapabilities capabilities;
    QVector<AIModel> compatibleModels;
    QVector<AIModel> recommendedModels;
    QStringList limitations;
    QJsonObject recommendations;
    double overallScore;       // System capability score (0-1)
};

class AutonomousModelManager : public QObject {
    Q_OBJECT

public:
    explicit AutonomousModelManager(QObject* parent = nullptr);
    ~AutonomousModelManager();

    // Autonomous model detection and recommendation
    ModelRecommendation autoDetectBestModel(const QString& taskType, 
                                           const QString& language = "cpp");
    QVector<ModelRecommendation> getTopRecommendations(const QString& taskType, 
                                                       const QString& language,
                                                       int topN = 3);
    
    // Automatic model download and setup
    bool autoDownloadAndSetup(const QString& modelId);
    bool downloadModel(const QString& modelId, const QString& destinationPath);
    DownloadProgress getDownloadProgress(const QString& modelId) const;
    void cancelDownload(const QString& modelId);
    
    // System capability analysis
    SystemAnalysis analyzeSystemCapabilities();
    SystemCapabilities getCurrentCapabilities() const;
    bool isModelCompatible(const AIModel& model) const;
    
    // Model management
    QVector<AIModel> getAvailableModels() const;
    QVector<AIModel> getInstalledModels() const;
    AIModel getModelInfo(const QString& modelId) const;
    bool loadModel(const QString& modelId);
    bool unloadCurrentModel();
    QString getCurrentModelId() const;
    
    // Model registry and discovery
    void refreshModelRegistry();
    void addCustomModel(const AIModel& model);
    bool removeModel(const QString& modelId);
    
    // Auto-update and maintenance
    void enableAutoUpdate(bool enable);
    void checkForModelUpdates();
    void cleanupUnusedModels(int daysUnused = 30);
    
    // Performance monitoring
    QJsonObject getModelPerformanceStats(const QString& modelId) const;
    void recordModelUsage(const QString& modelId, double latency, bool success);
    
    // Configuration
    void setModelCacheDirectory(const QString& path);
    void setMaxCacheSize(qint64 bytes);
    void setPreferredQuantization(const QString& quantization);
    QString getModelCacheDirectory() const;

signals:
    void modelRecommendationReady(const ModelRecommendation& recommendation);
    void downloadStarted(const QString& modelId);
    void downloadProgress(const QString& modelId, double percentage);
    void downloadComplete(const QString& modelId, bool success);
    void modelLoaded(const QString& modelId);
    void modelUnloaded(const QString& modelId);
    void registryUpdated(int modelCount);
    void systemCapabilitiesChanged(const SystemCapabilities& capabilities);
    void errorOccurred(const QString& error);

private slots:
    void onNetworkReplyFinished(QNetworkReply* reply);
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onAutoUpdateTimerTimeout();

private:
    // Model selection algorithms
    double calculateModelSuitability(const AIModel& model, 
                                    const QString& taskType,
                                    const QString& language) const;
    double calculateTaskCompatibility(const AIModel& model, const QString& taskType) const;
    double calculateLanguageSupport(const AIModel& model, const QString& language) const;
    double calculatePerformanceScore(const AIModel& model) const;
    double calculateResourceEfficiency(const AIModel& model) const;
    
    // System detection
    qint64 detectTotalRAM() const;
    qint64 detectAvailableRAM() const;
    int detectCPUCores() const;
    double detectCPUFrequency() const;
    bool detectGPU(QString& gpuName, qint64& gpuMemory) const;
    bool detectAVX2Support() const;
    bool detectAVX512Support() const;
    QString detectPlatform() const;
    
    // Model registry management
    void loadModelRegistry();
    void saveModelRegistry();
    void fetchHuggingFaceModels();
    void fetchOllamaModels();
    void parseModelMetadata(const QJsonObject& json, AIModel& model);
    
    // Download management
    void startDownload(const QString& modelId, const QString& url);
    bool validateDownload(const QString& filePath, const QString& expectedHash = "");
    bool extractModel(const QString& archivePath, const QString& destinationPath);
    void optimizeModelForSystem(const QString& modelPath);
    
    // Caching and cleanup
    void updateModelCache();
    void evictLeastRecentlyUsed();
    qint64 calculateCacheSize() const;
    
    // Data members
    QVector<AIModel> availableModels;
    QVector<AIModel> installedModels;
    QHash<QString, DownloadProgress> activeDownloads;
    QHash<QString, QNetworkReply*> networkReplies;
    QHash<QString, QJsonObject> performanceStats;
    
    SystemCapabilities systemCapabilities;
    QString currentModelId;
    QString modelCacheDirectory;
    qint64 maxCacheSize;
    QString preferredQuantization;
    bool autoUpdateEnabled;
    
    QNetworkAccessManager* networkManager;
    QTimer* autoUpdateTimer;
    
    // Configuration
    static constexpr int AUTO_UPDATE_INTERVAL_HOURS = 24;
    static constexpr qint64 DEFAULT_MAX_CACHE_SIZE = 50LL * 1024 * 1024 * 1024; // 50 GB
    static constexpr double MIN_CONFIDENCE_THRESHOLD = 0.6;
};

#endif // AUTONOMOUS_MODEL_MANAGER_H
