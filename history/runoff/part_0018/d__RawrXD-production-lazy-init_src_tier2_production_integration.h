// tier2_production_integration.h - Complete Tier 2 Systems Integration
#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutex>
#include <QDateTime>
#include <QTimer>
#include <QVector>

// Forward declarations
namespace MLErrorDetection {
    class MLErrorDetector;
    struct AnomalyDetectionResult;
    struct ErrorPrediction;
}

class ErrorAnalysisSystem;

namespace ModelHotpatch {
    class ModelHotpatchManager;
    struct ModelInfo;
    struct CanaryConfig;
}

class PerformanceMonitor;
struct MetricData;

namespace Tracing {
    class DistributedTracingSystem;
    struct Span;
}

namespace Tier2Integration {

/**
 * @brief Configuration for Tier 2 production systems
 */
struct Tier2Config {
    // Service identification
    QString serviceName;
    QString serviceVersion;
    
    // Feature flags
    bool enableMLErrorDetection;
    bool enableErrorAnalysis;
    bool enableModelHotpatching;
    bool enablePerformanceMonitoring;
    bool enableDistributedTracing;
    
    // Paths
    QString mlModelsPath;
    QString modelsDirectory;
    QString metricsExportPath;
    
    // ML Error Detection settings
    double errorDetectionSamplingRate;
    
    // Model Hotpatching settings
    int maxConcurrentModelLoads;
    int modelValidationTimeoutSec;
    
    // Performance Monitoring settings
    int performanceSnapshotIntervalMs;
    int metricsRetentionHours;
    
    // Distributed Tracing settings
    double tracingSamplingRate;
    
    // General settings
    int healthCheckIntervalSec;
    bool exportMetricsOnShutdown;
    
    Tier2Config()
        : serviceName("RawrXD")
        , serviceVersion("1.0.0")
        , enableMLErrorDetection(true)
        , enableErrorAnalysis(true)
        , enableModelHotpatching(true)
        , enablePerformanceMonitoring(true)
        , enableDistributedTracing(true)
        , errorDetectionSamplingRate(1.0)
        , maxConcurrentModelLoads(3)
        , modelValidationTimeoutSec(60)
        , performanceSnapshotIntervalMs(60000)
        , metricsRetentionHours(24)
        , tracingSamplingRate(0.1)
        , healthCheckIntervalSec(30)
        , exportMetricsOnShutdown(true)
    {}
};

/**
 * @brief Production-Ready Tier 2 Manager
 * 
 * Integrates and coordinates:
 * - AI-powered error detection with ML
 * - Runtime model hotpatching
 * - Comprehensive performance monitoring
 * - Distributed tracing
 * 
 * All systems are production-hardened with:
 * - Full observability (structured logging + metrics + tracing)
 * - Non-intrusive error handling
 * - External configuration management
 * - Comprehensive testing coverage
 */
class ProductionTier2Manager : public QObject {
    Q_OBJECT

public:
    explicit ProductionTier2Manager(QObject* parent = nullptr);
    ~ProductionTier2Manager();

    // Lifecycle
    bool initialize(const Tier2Config& config);
    void shutdown();
    bool isInitialized() const { return m_initialized; }

    // Error Detection & Analysis
    QString reportError(const QString& message, const QJsonObject& context);
    QJsonObject diagnoseError(const QString& errorId);
    QStringList suggestFixes(const QString& errorId);
    void learnFromError(const QString& errorMessage, const QString& category,
                       const QJsonObject& context, bool wasResolved);
    QJsonObject getErrorStatistics() const;

    // Model Hotpatching
    bool swapModel(const QString& modelId, bool validate = true);
    bool registerModel(const QString& modelPath, const QJsonObject& metadata = QJsonObject());
    QVector<ModelHotpatch::ModelInfo> listModels() const;
    QString getActiveModelId() const;
    bool startCanaryDeployment(const ModelHotpatch::CanaryConfig& config);
    void stopCanaryDeployment();
    QJsonObject getCanaryMetrics() const;

    // Performance Monitoring
    void recordMetric(const QString& component, const QString& operation,
                     double value, const QString& unit = "ms");
    QJsonObject getPerformanceReport() const;
    QJsonObject getSLACompliance() const;
    QJsonArray getBottlenecks() const;

    // Distributed Tracing
    Tracing::Span startTrace(const QString& operationName);
    void finishTrace(Tracing::Span& span);
    QJsonObject analyzeTrace(const QString& traceId);
    QJsonArray findTraceBottlenecks(const QString& traceId);

    // System Status & Health
    QJsonObject getSystemStatus() const;
    void runHealthChecks();
    bool exportAllMetrics(const QString& outputDir);

signals:
    // Initialization
    void systemsInitialized();
    
    // Error Detection
    void anomalyDetected(double anomalyScore, const QString& detectionMethod);
    void criticalErrorDetected(const QString& errorId, const QString& message);
    
    // Model Hotpatching
    void modelSwapped(const QString& newModelId, const QString& previousModelId);
    void canaryPromoted(const QString& modelId);
    
    // Performance Monitoring
    void performanceThresholdViolated(const QString& component, const QString& metric, double value);
    void slaViolation(const QString& slaId, double currentValue, double targetValue);
    void bottleneckDetected(const QString& component, double latencyMs);
    
    // Health
    void healthCheckCompleted(const QJsonObject& healthStatus);

private:
    // Internal methods
    void wireIntegrations();
    void startMonitoringThreads();
    void stopMonitoringThreads();

    // Event handlers
    void onAnomalyDetected(const MLErrorDetection::AnomalyDetectionResult& result);
    void onErrorPredicted(const MLErrorDetection::ErrorPrediction& prediction);
    void onModelSwapped(const QString& newModelId, const QString& previousModelId);
    void onHotpatchFailed(const QString& modelId, const QString& error);
    void onPerformanceThresholdViolation(const MetricData& metric, const QString& severity);
    void onBottleneckDetected(const QString& traceId, const QString& serviceName, double latencyMs);

    // Components
    MLErrorDetection::MLErrorDetector* m_mlErrorDetector;
    ErrorAnalysisSystem* m_errorAnalysisSystem;
    ModelHotpatch::ModelHotpatchManager* m_modelHotpatchManager;
    PerformanceMonitor* m_performanceMonitor;
    Tracing::DistributedTracingSystem* m_distributedTracing;

    // Configuration
    Tier2Config m_config;

    // State
    mutable QMutex m_mutex;
    bool m_initialized;
    QDateTime m_startTime;
    QTimer* m_healthCheckTimer;
};

} // namespace Tier2Integration
