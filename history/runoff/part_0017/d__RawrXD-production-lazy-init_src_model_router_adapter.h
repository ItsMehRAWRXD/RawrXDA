#ifndef MODEL_ROUTER_ADAPTER_H
#define MODEL_ROUTER_ADAPTER_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QJsonObject>
#include <memory>
#include "model_interface.h"

/**
 * @class ModelRouterAdapter
 * @brief Bridges Universal Model Router with RawrXD IDE
 * 
 * STABLE API — frozen as of v1.0.0 (January 22, 2026)
 * Breaking changes require MAJOR version bump (v2.0.0+)
 * See FEATURE_FLAGS.md for API stability guarantees
 * 
 * Provides seamless integration between the ModelInterface (cloud + local models)
 * and RawrXD's existing model management system. Handles:
 * - Initialization and configuration loading
 * - Model selection and switching
 * - Cost and performance tracking
 * - Error recovery with fallback chains
 * - Signal/slot integration with Qt event system
 */
class ModelRouterAdapter : public QObject {
    Q_OBJECT

public:
    explicit ModelRouterAdapter(QObject *parent = nullptr);
    ~ModelRouterAdapter();

    // === Initialization & Configuration ===
    
    /**
     * Initialize the model router with configuration file
     * @param config_file_path Path to model_config.json
     * @return true if initialized successfully
     */
    bool initialize(const QString& config_file_path);

    /**
     * Load API keys from environment variables
     * Securely loads keys for all configured cloud providers
     */
    bool loadApiKeys();

    /**
     * Check if router is initialized and ready
     */
    bool isReady() const { return m_router != nullptr && m_initialized; }

    // === Model Selection ===

    /**
     * Get list of available models
     */
    QStringList getAvailableModels() const;

    /**
     * Select model for specific task
     * @param task_type "code_generation", "completion", "chat", etc.
     * @param language "python", "cpp", "javascript", etc.
     * @param prefer_local true to prefer fast local models
     */
    QString selectBestModel(const QString& task_type, const QString& language, bool prefer_local = false);

    /**
     * Select cheapest model under budget
     * @param prompt The prompt to estimate cost for
     * @param max_cost_usd Maximum cost in USD
     */
    QString selectCostOptimalModel(const QString& prompt, double max_cost_usd);

    /**
     * Select fastest available model
     */
    QString selectFastestModel();

    /**
     * Set default model for all operations
     */
    void setDefaultModel(const QString& model_name);

    /**
     * Get currently active model
     */
    QString getActiveModel() const { return m_active_model; }

    // === Generation & Inference ===

    /**
     * Synchronous text generation
     * @param prompt User prompt
     * @param model_name Model to use (uses default if empty)
     * @param max_tokens Maximum tokens in response
     * @return Generated text or empty string on error
     */
    QString generate(const QString& prompt, const QString& model_name = "", int max_tokens = 4096);

    /**
     * Asynchronous generation (non-blocking)
     * Emits generationComplete signal when done
     */
    void generateAsync(const QString& prompt, const QString& model_name = "", int max_tokens = 4096);

    /**
     * Streaming generation with chunks
     * Emits generationChunk signal for each chunk received
     */
    void generateStream(const QString& prompt, const QString& model_name = "", int max_tokens = 4096);

    // === Statistics & Monitoring ===

    /**
     * Get average latency for a model (in milliseconds)
     */
    double getAverageLatency(const QString& model_name = "") const;

    /**
     * Get success rate percentage (0-100)
     */
    int getSuccessRate(const QString& model_name = "") const;

    /**
     * Get total cost across all requests (in USD)
     */
    double getTotalCost() const;

    /**
     * Get cost breakdown by model
     */
    QMap<QString, double> getCostBreakdown() const;

    /**
     * Get usage statistics as JSON
     */
    QJsonObject getStatistics() const;

    /**
     * Export statistics to CSV file
     */
    bool exportStatisticsToCsv(const QString& file_path) const;

    // === Configuration Management ===

    /**
     * Register a new model at runtime
     */
    bool registerModel(const QString& name, const QJsonObject& config);

    /**
     * Get current configuration as JSON
     */
    QJsonObject getConfiguration() const;

    /**
     * Save configuration to file
     */
    bool saveConfiguration(const QString& file_path);

    /**
     * Set retry policy for failed requests
     * @param max_retries Number of retries
     * @param retry_delay_ms Delay between retries in milliseconds
     */
    void setRetryPolicy(int max_retries, int retry_delay_ms);

    /**
     * Set cost limit alert threshold (in USD)
     */
    void setCostAlertThreshold(double threshold_usd);

    /**
     * Set maximum request latency tolerance (in ms)
     */
    void setLatencyThreshold(int threshold_ms);

    // === Error Handling ===

    /**
     * Get last error message
     */
    QString getLastError() const { return m_last_error; }

    /**
     * Clear error state
     */
    void clearError() { m_last_error.clear(); }

    /**
     * Enable/disable fallback to local models on cloud error
     */
    void setAutoFallback(bool enabled) { m_auto_fallback = enabled; }

signals:
    // Generation signals
    void generationStarted(const QString& model_name);
    void generationProgress(int percent);
    void generationChunk(const QString& chunk);
    void generationComplete(const QString& result, int tokens_used, double latency_ms);
    void generationError(const QString& error);

    // Model signals
    void modelChanged(const QString& new_model);
    void modelListUpdated(const QStringList& models);

    // Statistics signals
    void costUpdated(double total_cost);
    void latencyUpdated(double avg_latency_ms);
    void statisticsUpdated(const QJsonObject& stats);

    // Status signals
    void statusChanged(const QString& status);
    void initialized();
    void shutting_down();

private slots:
    void onGenerationThreadFinished();
    void onStreamChunkReceived(const QString& chunk);

private:
    // Model router instance
    std::unique_ptr<ModelInterface> m_router;
    
    // State
    bool m_initialized = false;
    QString m_active_model;
    QString m_last_error;
    bool m_auto_fallback = true;
    
    // Configuration
    QJsonObject m_config;
    double m_cost_alert_threshold = 50.0;  // Default $50 alert
    int m_latency_threshold_ms = 2000;     // Default 2 second threshold
    
    // For async operations
    class GenerationThread;
    GenerationThread* m_generation_thread = nullptr;
    
    // Helper methods
    bool validateConfiguration(const QJsonObject& config);
    void logStatistic(const QString& model, const QString& metric, double value);
    QString determineFallbackModel(const QString& primary_model);
};

#endif // MODEL_ROUTER_ADAPTER_H
