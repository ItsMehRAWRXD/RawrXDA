#pragma once
#include <QObject>
#include <QTimer>
#include <QThreadPool>
#include <QFuture>
#include <memory>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <chrono>
#include <queue>
#include <map>

#include "streaming_gguf_memory_manager.hpp"
#include "lazy_model_loader.hpp"
#include "large_model_optimizer.hpp"
#include "../monitoring/enterprise_metrics_collector.hpp"
#include "fault_tolerance_manager.hpp"
#include "performance_monitor.h"

// Enterprise configuration
struct EnterpriseStreamingConfig {
    size_t max_memory_per_node = 64ULL * 1024 * 1024 * 1024; // 64GB
    size_t max_concurrent_models = 8;
    size_t streaming_block_size = 128 * 1024 * 1024; // 128MB
    size_t prefetch_ahead_blocks = 8;
    double memory_pressure_threshold = 0.85;
    bool enable_distributed_cache = true;
    bool enable_fault_tolerance = true;
    bool enable_predictive_loading = true;
    size_t cache_replication_factor = 2;
    std::chrono::seconds health_check_interval{30};
    std::chrono::seconds metrics_reporting_interval{10};
    std::string log_level = "INFO";
    std::string metrics_backend = "prometheus";
};

// Production model deployment
struct ProductionModelDeployment {
    std::string model_id;
    std::string model_path;
    std::string deployment_strategy;
    std::string quantization_level;
    size_t estimated_memory_usage;
    size_t actual_memory_usage;
    double current_throughput;
    double current_latency;
    size_t request_count;
    size_t error_count;
    std::chrono::steady_clock::time_point deployment_time;
    std::string status; // "deploying", "ready", "scaling", "error", "undeploying"
    std::vector<std::string> applied_optimizations;
};

class EnterpriseStreamingController : public QObject {
    Q_OBJECT

public:
    explicit EnterpriseStreamingController(QObject* parent = nullptr);
    ~EnterpriseStreamingController();

    // Enterprise lifecycle management
    bool initialize(const EnterpriseStreamingConfig& config);
    bool shutdown();
    bool isHealthy() const { return system_healthy; }
    
    // Production model management
    QString deployModel(const QString& model_path, const QString& deployment_id = "");
    bool undeployModel(const QString& deployment_id);
    bool scaleModel(const QString& deployment_id, size_t target_instances);
    ProductionModelDeployment getDeploymentStatus(const QString& deployment_id) const;
    std::vector<ProductionModelDeployment> getAllDeployments() const;
    
    // Enterprise request handling
    struct ModelRequest {
        QString deployment_id;
        QString prompt;
        size_t max_tokens;
        double temperature;
        double timeout_seconds;
        QString request_id;
        std::chrono::steady_clock::time_point submit_time;
        std::map<QString, QVariant> metadata;
    };
    
    struct ModelResponse {
        QString request_id;
        QString deployment_id;
        QString generated_text;
        size_t tokens_generated;
        double latency_ms;
        double throughput_tokens_per_sec;
        bool success;
        QString error_message;
        std::chrono::steady_clock::time_point completion_time;
    };
    
    QFuture<ModelResponse> generateAsync(const ModelRequest& request);
    ModelResponse generateSync(const ModelRequest& request);
    
    // Enterprise monitoring and observability
    struct SystemHealth {
        bool overall_health;
        double memory_utilization;
        double cpu_utilization;
        size_t active_deployments;
        size_t total_requests;
        size_t error_rate;
        double avg_latency_ms;
        double avg_throughput;
        std::chrono::steady_clock::time_point timestamp;
        std::map<QString, QVariant> component_status;
    };
    
    SystemHealth getSystemHealth() const;
    std::map<QString, QVariant> getDetailedMetrics() const;
    std::vector<QString> getActiveAlerts() const;
    
    // Enterprise configuration management
    bool updateConfiguration(const EnterpriseStreamingConfig& config);
    EnterpriseStreamingConfig getCurrentConfiguration() const;
    
    // Disaster recovery and backup
    bool createDeploymentSnapshot(const QString& deployment_id);
    bool restoreDeploymentFromSnapshot(const QString& snapshot_id);
    std::vector<QString> listAvailableSnapshots() const;
    
    // Multi-node support (for future scaling)
    bool registerNode(const QString& node_id, const QString& node_address);
    bool unregisterNode(const QString& node_id);
    std::vector<QString> getActiveNodes() const;

signals:
    void deploymentCreated(const QString& deployment_id, const QString& model_path);
    void deploymentStatusChanged(const QString& deployment_id, const QString& status);
    void systemHealthChanged(bool healthy, const QString& reason);
    void metricsUpdated(const std::map<QString, QVariant>& metrics);
    void alertTriggered(const QString& alert_type, const QString& message);
    void requestCompleted(const ModelResponse& response);

private slots:
    void performHealthCheck();
    void collectSystemMetrics();
    void handleMemoryPressure(int level,
                             size_t current_usage, size_t budget);
    void processDeploymentQueue();
    void validateSystemState();
    void reportMetricsToBackend();

private:
    // Core enterprise components
    std::unique_ptr<StreamingGGUFMemoryManager> memory_manager;
    std::unique_ptr<LazyModelLoader> lazy_loader;
    std::unique_ptr<LargeModelOptimizer> model_optimizer;
    std::unique_ptr<PerformanceMonitor> performance_engine;
    std::unique_ptr<EnterpriseMetricsCollector> metrics_collector;
    std::unique_ptr<FaultToleranceManager> fault_tolerance;
    
    // Enterprise configuration
    EnterpriseStreamingConfig enterprise_config;
    std::atomic<bool> system_healthy{false};
    std::atomic<bool> system_initialized{false};
    
    // Deployment management
    std::unordered_map<QString, ProductionModelDeployment> deployments;
    std::queue<std::pair<QString, QString>> deployment_queue;
    std::unordered_map<QString, QString> deployment_snapshots;
    QMutex deployment_mutex;
    
    // Request processing
    std::atomic<size_t> total_requests{0};
    std::atomic<size_t> failed_requests{0};
    std::unordered_map<QString, ModelRequest> active_requests;
    std::unordered_map<QString, ModelResponse> completed_requests;
    
    // System monitoring
    QTimer* health_check_timer;
    QTimer* metrics_timer;
    QTimer* deployment_timer;
    std::chrono::steady_clock::time_point system_start_time;
    
    // Thread pool for async operations
    QThreadPool* request_thread_pool;
    QThreadPool* deployment_thread_pool;
    
    // Enterprise monitoring
    mutable std::mutex metrics_mutex;
    std::map<QString, QVariant> current_metrics;
    std::vector<QString> active_alerts;
    
    // Core enterprise methods
    bool validateDeploymentRequest(const QString& model_path, QString& error_message);
    bool prepareModelForDeployment(const QString& model_path, const QString& deployment_id);
    bool performPreDeploymentChecks(const QString& model_path, const QString& deployment_id);
    void executeDeployment(const QString& model_path, const QString& deployment_id);
    void rollbackDeployment(const QString& deployment_id, const QString& reason);
    
    // Request processing pipeline
    ModelResponse processRequest(const ModelRequest& request);
    bool validateRequest(const ModelRequest& request, QString& error_message);
    bool routeRequestToDeployment(const ModelRequest& request, QString& deployment_id);
    ModelResponse generateFromDeployment(const ModelRequest& request, const QString& deployment_id);
    
    // Enterprise monitoring
    void updateSystemMetrics();
    void checkSystemHealth();
    void generateAlerts();
    void clearAlert(const QString& alert_type);
    bool shouldTriggerAlert(const QString& alert_type, const QVariant& value);
    
    // Fault tolerance
    bool handleDeploymentFailure(const QString& deployment_id, const QString& error);
    bool handleRequestFailure(const QString& request_id, const QString& error);
    bool recoverFromError(const QString& component, const QString& error);
    
    // Performance optimization
    void optimizeSystemPerformance();
    void balanceMemoryUsage();
    void adjustDeploymentScaling();
    double calculateOptimalBlockSize(const QString& model_path);
    
    // Security and compliance
    bool validateModelIntegrity(const QString& model_path);
    bool checkSecurityPolicies(const QString& model_path);
    void auditDeployment(const QString& deployment_id);
    
    // Utility methods
    QString generateDeploymentId();
    QString generateRequestId();
    std::chrono::milliseconds getCurrentLatency();
    double getCurrentThroughput();
    size_t estimateModelMemoryUsage(const QString& model_path);
    bool isModelCompatible(const QString& model_path);
};
