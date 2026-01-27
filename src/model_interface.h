// model_interface.h - Unified Interface for All Model Types (Local + Cloud)
#ifndef MODEL_INTERFACE_H
#define MODEL_INTERFACE_H

#include <QString>
#include <QObject>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>
#include <functional>
#include <vector>

class UniversalModelRouter;
class QuantizationAwareInferenceEngine;
class CloudApiClient;
struct ModelConfig;  // Defined in universal_model_router.h

// Generation options
struct GenerationOptions {
    int max_tokens = 2048;
    double temperature = 0.7;
    double top_p = 0.9;
    int top_k = 50;
    double frequency_penalty = 0.0;
    double presence_penalty = 0.0;
    bool stream = false;
    QMap<QString, QString> additional_params;
};

// Generation result
struct GenerationResult {
    QString content;
    QString model_name;
    QString backend;
    int tokens_used = 0;
    double latency_ms = 0.0;
    bool success = false;
    QString error;
    QJsonObject metadata;
};

// Unified Model Interface
class ModelInterface : public QObject {
    Q_OBJECT

public:
    explicit ModelInterface(QObject* parent = nullptr);
    ~ModelInterface();

    // Initialization
    void initialize(const QString& config_file_path);
    void initializeWithRouter(std::shared_ptr<UniversalModelRouter> router);
    bool isInitialized() const;
    
    // =========== SYNCHRONOUS INTERFACE ===========
    // Generate completion (blocking)
    GenerationResult generate(const QString& prompt,
                             const QString& model_name,
                             const GenerationOptions& options = GenerationOptions());
    
    // =========== ASYNCHRONOUS INTERFACE ===========
    // Generate completion (non-blocking)
    void generateAsync(const QString& prompt,
                      const QString& model_name,
                      std::function<void(const GenerationResult&)> callback,
                      const GenerationOptions& options = GenerationOptions());
    
    // =========== STREAMING INTERFACE ===========
    // Stream completion in real-time
    void generateStream(const QString& prompt,
                       const QString& model_name,
                       std::function<void(const QString&)> on_chunk,
                       std::function<void(const QString&)> on_error = nullptr,
                       const GenerationOptions& options = GenerationOptions());
    
    // =========== BATCH OPERATIONS ===========
    // Generate for multiple prompts
    QVector<GenerationResult> generateBatch(const QStringList& prompts,
                                           const QString& model_name,
                                           const GenerationOptions& options = GenerationOptions());
    
    void generateBatchAsync(const QStringList& prompts,
                           const QString& model_name,
                           std::function<void(const QVector<GenerationResult>&)> callback,
                           const GenerationOptions& options = GenerationOptions());
    
    // =========== MODEL MANAGEMENT ===========
    // Get available models
    QStringList getAvailableModels() const;
    QStringList getLocalModels() const;
    QStringList getCloudModels() const;
    
    // Model information
    QString getModelDescription(const QString& model_name) const;
    QJsonObject getModelInfo(const QString& model_name) const;
    bool modelExists(const QString& model_name) const;
    
    // Register custom model
    void registerModel(const QString& model_name, const ModelConfig& config);
    void unregisterModel(const QString& model_name);
    
    // =========== SMART ROUTING ===========
    // Automatic model selection based on task
    QString selectBestModel(const QString& task_type,
                           const QString& language = "cpp",
                           bool prefer_local = false);
    
    // Cost-aware routing
    QString selectCostOptimalModel(const QString& prompt,
                                  double max_cost_usd = 1.0);
    
    // Performance-aware routing
    QString selectFastestModel(const QString& model_type = "general");
    
    // =========== CONFIGURATION ===========
    // Load/save configurations
    bool loadConfig(const QString& config_file_path);
    bool saveConfig(const QString& config_file_path) const;
    
    // Set default model
    void setDefaultModel(const QString& model_name);
    QString getDefaultModel() const;
    
    // =========== STATISTICS & MONITORING ===========
    // Usage statistics
    QJsonObject getUsageStatistics() const;
    QJsonObject getModelStats(const QString& model_name) const;
    
    // Performance metrics
    double getAverageLatency(const QString& model_name = "") const;
    int getSuccessRate(const QString& model_name = "") const;
    
    // Cost tracking
    double getTotalCost() const;
    double getCostByModel(const QString& model_name) const;
    QJsonObject getCostBreakdown() const;
    
    // =========== ERROR HANDLING ===========
    // Set error callbacks
    void setErrorCallback(std::function<void(const QString&)> callback);
    
    // Retry configuration
    void setRetryPolicy(int max_retries, int retry_delay_ms);
    
    // =========== UTILITY ===========
    // Count tokens (for cost estimation)
    int estimateTokenCount(const QString& text) const;
    
    // Format model list
    QString formatModelList() const;
    QJsonArray getModelListAsJson() const;

signals:
    void initialized();
    void modelListUpdated(const QStringList& models);
    void generationCompleted(const GenerationResult& result);
    void generationFailed(const QString& error);
    void streamChunkReceived(const QString& chunk);
    void streamingCompleted();
    void streamingFailed(const QString& error);
    void batchCompleted(const QVector<GenerationResult>& results);
    void batchFailed(const QString& error);
    void statisticsUpdated();

private slots:
    void onRouterInitialized();
    void onModelRegistered(const QString& model_name);

private:
    // Internal generation methods
    GenerationResult generateInternal(const QString& prompt,
                                     const QString& model_name,
                                     const GenerationOptions& options);
    
    void generateStreamInternal(const QString& prompt,
                               const QString& model_name,
                               std::function<void(const QString&)> on_chunk,
                               std::function<void(const QString&)> on_error,
                               const GenerationOptions& options);
    
    // Helper methods
    bool isLocalModel(const QString& model_name) const;
    bool isCloudModel(const QString& model_name) const;
    ModelConfig getModelConfigOrThrow(const QString& model_name) const;
    
    // Member variables
    std::shared_ptr<UniversalModelRouter> router;
    std::shared_ptr<QuantizationAwareInferenceEngine> local_engine;
    std::shared_ptr<CloudApiClient> cloud_client;
    QString default_model;
    bool initialized_flag;
    
    // Statistics tracking
    struct ModelStats {
        int call_count = 0;
        int success_count = 0;
        int failure_count = 0;
        double total_latency_ms = 0.0;
        double total_cost = 0.0;
        int total_tokens = 0;
    };
    
    QMap<QString, ModelStats> stats_map;
    
    // Configuration
    int max_retries = 3;
    int retry_delay_ms = 1000;
    std::function<void(const QString&)> error_callback;
};

#endif // MODEL_INTERFACE_H
