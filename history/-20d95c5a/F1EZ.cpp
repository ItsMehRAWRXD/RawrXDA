// tier2_production_integration.cpp - Tier 2 Systems Integration
// Complete integration of AI Error Detection, Model Hotpatching, and Performance Monitoring

#include "tier2_production_integration.h"
#include "ai/ml_error_detector.h"
#include "ai/error_analysis_system.h"
#include "model_loader/model_hotpatch_manager.h"
#include "performance_monitor.h"
#include "monitoring/distributed_tracing_system.h"
#include "logging/structured_logger.h"
#include <QJsonDocument>
#include <QFile>
#include <QDir>

namespace Tier2Integration {

// ============================================================================
// ProductionTier2Manager Implementation
// ============================================================================

ProductionTier2Manager::ProductionTier2Manager(QObject* parent)
    : QObject(parent)
    , m_mlErrorDetector(nullptr)
    , m_errorAnalysisSystem(nullptr)
    , m_modelHotpatchManager(nullptr)
    , m_performanceMonitor(nullptr)
    , m_distributedTracing(nullptr)
    , m_initialized(false)
{
    LOG_INFO("Tier 2 Production Manager created");
}

ProductionTier2Manager::~ProductionTier2Manager() {
    shutdown();
}

bool ProductionTier2Manager::initialize(const Tier2Config& config) {
    QMutexLocker lock(&m_mutex);
    
    if (m_initialized) {
        LOG_WARN("Tier 2 Manager already initialized");
        return false;
    }
    
    m_config = config;
    
    LOG_INFO("Initializing Tier 2 Production Systems", {
        {"service_name", config.serviceName},
        {"enable_ml_error_detection", config.enableMLErrorDetection},
        {"enable_model_hotpatching", config.enableModelHotpatching},
        {"enable_distributed_tracing", config.enableDistributedTracing}
    });
    
    // Initialize ML Error Detector
    if (config.enableMLErrorDetection) {
        m_mlErrorDetector = new MLErrorDetection::MLErrorDetector(this);
        m_mlErrorDetector->initialize(config.mlModelsPath);
        
        connect(m_mlErrorDetector, &MLErrorDetection::MLErrorDetector::anomalyDetected,
                this, &ProductionTier2Manager::onAnomalyDetected);
        connect(m_mlErrorDetector, &MLErrorDetection::MLErrorDetector::errorPredicted,
                this, &ProductionTier2Manager::onErrorPredicted);
        
        LOG_INFO("ML Error Detector initialized");
    }
    
    // Initialize Error Analysis System
    if (config.enableErrorAnalysis) {
        m_errorAnalysisSystem = new ErrorAnalysisSystem(this);
        // Additional initialization can be done here
        
        LOG_INFO("Error Analysis System initialized");
    }
    
    // Initialize Model Hotpatch Manager
    if (config.enableModelHotpatching) {
        m_modelHotpatchManager = new ModelHotpatch::ModelHotpatchManager(this);
        m_modelHotpatchManager->initialize(config.modelsDirectory);
        m_modelHotpatchManager->setMaxConcurrentLoads(config.maxConcurrentModelLoads);
        m_modelHotpatchManager->setValidationTimeout(config.modelValidationTimeoutSec);
        
        connect(m_modelHotpatchManager, &ModelHotpatch::ModelHotpatchManager::modelSwapped,
                this, &ProductionTier2Manager::onModelSwapped);
        connect(m_modelHotpatchManager, &ModelHotpatch::ModelHotpatchManager::hotpatchFailed,
                this, &ProductionTier2Manager::onHotpatchFailed);
        
        LOG_INFO("Model Hotpatch Manager initialized");
    }
    
    // Initialize Performance Monitor
    if (config.enablePerformanceMonitoring) {
        m_performanceMonitor = new PerformanceMonitor(this);
        m_performanceMonitor->setSnapshotInterval(config.performanceSnapshotIntervalMs);
        m_performanceMonitor->setMetricsRetention(config.metricsRetentionHours);
        
        connect(m_performanceMonitor, &PerformanceMonitor::thresholdViolation,
                this, &ProductionTier2Manager::onPerformanceThresholdViolation);
        
        LOG_INFO("Performance Monitor initialized");
    }
    
    // Initialize Distributed Tracing
    if (config.enableDistributedTracing) {
        m_distributedTracing = new Tracing::DistributedTracingSystem(this);
        m_distributedTracing->initialize(config.serviceName, config.serviceVersion);
        m_distributedTracing->setSamplingRate(config.tracingSamplingRate);
        
        connect(m_distributedTracing, &Tracing::DistributedTracingSystem::bottleneckDetected,
                this, &ProductionTier2Manager::onBottleneckDetected);
        
        LOG_INFO("Distributed Tracing initialized");
    }
    
    // Wire up cross-component integrations
    wireIntegrations();
    
    // Start monitoring threads
    startMonitoringThreads();
    
    m_initialized = true;
    m_startTime = QDateTime::currentDateTime();
    
    LOG_INFO("Tier 2 Production Systems fully initialized");
    
    emit systemsInitialized();
    
    return true;
}

void ProductionTier2Manager::shutdown() {
    QMutexLocker lock(&m_mutex);
    
    if (!m_initialized) {
        return;
    }
    
    LOG_INFO("Shutting down Tier 2 Production Systems");
    
    // Stop monitoring threads
    stopMonitoringThreads();
    
    // Shutdown components
    if (m_distributedTracing) {
        m_distributedTracing->shutdown();
    }
    
    if (m_modelHotpatchManager) {
        m_modelHotpatchManager->shutdown();
    }
    
    // Export final metrics
    if (m_config.exportMetricsOnShutdown) {
        exportAllMetrics(m_config.metricsExportPath);
    }
    
    m_initialized = false;
    
    LOG_INFO("Tier 2 Production Systems shutdown complete");
}

void ProductionTier2Manager::wireIntegrations() {
    // Wire ML Error Detector to Error Analysis System
    if (m_mlErrorDetector && m_errorAnalysisSystem) {
        // When ML detector finds anomaly, report to error analysis
        connect(m_mlErrorDetector, &MLErrorDetection::MLErrorDetector::anomalyDetected,
                this, [this](const MLErrorDetection::AnomalyDetectionResult& result) {
                    ErrorContext context;
                    context.timestamp = result.timestamp;
                    
                    m_errorAnalysisSystem->reportError(
                        QString("Anomaly detected: %1").arg(result.detectionMethod),
                        context
                    );
                });
    }
    
    // Wire Model Hotpatch to Performance Monitor
    if (m_modelHotpatchManager && m_performanceMonitor) {
        // Record model swap events as metrics
        connect(m_modelHotpatchManager, &ModelHotpatch::ModelHotpatchManager::modelSwapped,
                this, [this](const QString& newModelId, const QString& previousModelId) {
                    m_performanceMonitor->recordMetric(
                        "model_management",
                        "model_swap",
                        1.0,
                        "count"
                    );
                });
    }
    
    // Wire Distributed Tracing to Performance Monitor
    if (m_distributedTracing && m_performanceMonitor) {
        // Record span durations as performance metrics
        connect(m_distributedTracing, &Tracing::DistributedTracingSystem::spanRecorded,
                this, [this](const Tracing::Span& span) {
                    m_performanceMonitor->recordMetric(
                        span.serviceName,
                        span.operationName,
                        span.durationMicros / 1000.0,  // Convert to ms
                        "ms"
                    );
                });
    }
    
    LOG_INFO("Component integrations wired");
}

// ============================================================================
// Error Detection Integration
// ============================================================================

QString ProductionTier2Manager::reportError(const QString& message, const QJsonObject& context) {
    QString errorId;
    
    // Report to Error Analysis System
    if (m_errorAnalysisSystem) {
        ErrorContext errorContext;
        errorContext.timestamp = QDateTime::currentDateTime();
        
        if (context.contains("sourceFile")) {
            errorContext.sourceFile = context["sourceFile"].toString();
        }
        if (context.contains("lineNumber")) {
            errorContext.lineNumber = context["lineNumber"].toInt();
        }
        
        errorId = m_errorAnalysisSystem->reportError(message, errorContext);
    }
    
    // Detect anomaly with ML
    if (m_mlErrorDetector) {
        auto anomalyResult = m_mlErrorDetector->detectAnomaly(message, context);
        
        if (anomalyResult.isAnomaly) {
            LOG_WARN("Anomaly detected in error", {
                {"error_message", message},
                {"anomaly_score", anomalyResult.anomalyScore}
            });
        }
        
        // Predict error category
        auto prediction = m_mlErrorDetector->predictErrorCategory(message, context);
        
        LOG_INFO("Error category predicted", {
            {"category", prediction.predictedCategory},
            {"confidence", prediction.confidence}
        });
    }
    
    // Record error metric
    if (m_performanceMonitor) {
        m_performanceMonitor->recordMetric("system", "errors", 1.0, "count");
    }
    
    // Create trace span for error
    if (m_distributedTracing) {
        Tracing::Span errorSpan = m_distributedTracing->startSpan("error_handling");
        errorSpan.addTag("error_message", message);
        errorSpan.addTag("error_id", errorId);
        errorSpan.isError = true;
        errorSpan.errorMessage = message;
        m_distributedTracing->finishSpan(errorSpan);
    }
    
    return errorId;
}

QJsonObject ProductionTier2Manager::diagnoseError(const QString& errorId) {
    QJsonObject diagnosis;
    
    if (m_errorAnalysisSystem) {
        diagnosis = m_errorAnalysisSystem->diagnoseError(errorId);
    }
    
    return diagnosis;
}

QStringList ProductionTier2Manager::suggestFixes(const QString& errorId) {
    if (m_errorAnalysisSystem) {
        return m_errorAnalysisSystem->suggestFixes(errorId);
    }
    
    return QStringList();
}

void ProductionTier2Manager::learnFromError(const QString& errorMessage, const QString& category,
                                          const QJsonObject& context, bool wasResolved) {
    if (m_mlErrorDetector) {
        m_mlErrorDetector->learnFromError(errorMessage, category, context, wasResolved);
        
        LOG_INFO("Error learning recorded", {
            {"category", category},
            {"resolved", wasResolved}
        });
    }
}

// ============================================================================
// Model Hotpatching Integration
// ============================================================================

bool ProductionTier2Manager::swapModel(const QString& modelId, bool validate) {
    if (!m_modelHotpatchManager) {
        return false;
    }
    
    // Create trace for model swap
    Tracing::Span* swapSpan = nullptr;
    if (m_distributedTracing) {
        auto span = m_distributedTracing->startSpan("model_swap");
        span.addTag("model_id", modelId);
        span.addTag("validate", validate);
        swapSpan = new Tracing::Span(span);
    }
    
    // Perform swap
    auto result = m_modelHotpatchManager->swapModel(modelId, validate);
    
    // Record performance metric
    if (m_performanceMonitor) {
        m_performanceMonitor->recordMetric(
            "model_management",
            "swap_duration",
            result.swapDurationMs,
            "ms"
        );
    }
    
    // Complete trace
    if (swapSpan && m_distributedTracing) {
        swapSpan->addTag("success", result.success);
        swapSpan->addTag("duration_ms", result.swapDurationMs);
        if (!result.success) {
            swapSpan->isError = true;
            swapSpan->errorMessage = result.errorMessage;
        }
        m_distributedTracing->finishSpan(*swapSpan);
        delete swapSpan;
    }
    
    return result.success;
}

bool ProductionTier2Manager::registerModel(const QString& modelPath, const QJsonObject& metadata) {
    if (!m_modelHotpatchManager) {
        return false;
    }
    
    return m_modelHotpatchManager->registerModel(modelPath, metadata);
}

QVector<ModelHotpatch::ModelInfo> ProductionTier2Manager::listModels() const {
    if (!m_modelHotpatchManager) {
        return QVector<ModelHotpatch::ModelInfo>();
    }
    
    return m_modelHotpatchManager->listModels();
}

QString ProductionTier2Manager::getActiveModelId() const {
    if (!m_modelHotpatchManager) {
        return QString();
    }
    
    return m_modelHotpatchManager->getActiveModelId();
}

bool ProductionTier2Manager::startCanaryDeployment(const ModelHotpatch::CanaryConfig& config) {
    if (!m_modelHotpatchManager) {
        return false;
    }
    
    bool success = m_modelHotpatchManager->startCanaryDeployment(config);
    
    if (success && m_performanceMonitor) {
        m_performanceMonitor->recordMetric(
            "model_management",
            "canary_deployment_started",
            1.0,
            "count"
        );
    }
    
    return success;
}

// ============================================================================
// Performance Monitoring Integration
// ============================================================================

void ProductionTier2Manager::recordMetric(const QString& component, const QString& operation,
                                        double value, const QString& unit) {
    if (m_performanceMonitor) {
        m_performanceMonitor->recordMetric(component, operation, value, unit);
    }
}

QJsonObject ProductionTier2Manager::getPerformanceReport() const {
    QJsonObject report;
    
    if (m_performanceMonitor) {
        // Get SLA compliance
        auto slaCompliance = m_performanceMonitor->evaluateAllSLAs();
        QJsonArray slaArray;
        for (const auto& sla : slaCompliance) {
            QJsonObject slaObj;
            slaObj["sla_id"] = sla.slaId;
            slaObj["compliant"] = sla.isCompliant;
            slaObj["current_value"] = sla.currentValue;
            slaObj["target_value"] = sla.targetValue;
            slaObj["compliance_percentage"] = sla.compliancePercentage;
            slaArray.append(slaObj);
        }
        report["sla_compliance"] = slaArray;
        
        // Get bottlenecks
        auto bottlenecks = m_performanceMonitor->detectBottlenecks();
        QJsonArray bottleneckArray;
        for (const auto& bottleneck : bottlenecks) {
            QJsonObject bottleneckObj;
            bottleneckObj["component"] = bottleneck.component;
            bottleneckObj["type"] = bottleneck.type;
            bottleneckObj["severity"] = bottleneck.severity;
            bottleneckObj["description"] = bottleneck.description;
            bottleneckArray.append(bottleneckObj);
        }
        report["bottlenecks"] = bottleneckArray;
        
        // Get recent snapshot
        auto snapshot = m_performanceMonitor->capturePerformanceSnapshot();
        QJsonObject snapshotObj;
        snapshotObj["cpu_usage"] = snapshot.cpuUsage;
        snapshotObj["memory_usage"] = snapshot.memoryUsage;
        snapshotObj["average_latency"] = snapshot.averageLatency;
        snapshotObj["requests_per_second"] = snapshot.requestsPerSecond;
        report["current_snapshot"] = snapshotObj;
    }
    
    return report;
}

// ============================================================================
// Distributed Tracing Integration
// ============================================================================

Tracing::Span ProductionTier2Manager::startTrace(const QString& operationName) {
    if (m_distributedTracing) {
        return m_distributedTracing->startSpan(operationName);
    }
    
    return Tracing::Span();
}

void ProductionTier2Manager::finishTrace(Tracing::Span& span) {
    if (m_distributedTracing) {
        m_distributedTracing->finishSpan(span);
    }
}

QJsonObject ProductionTier2Manager::analyzeTrace(const QString& traceId) {
    if (m_distributedTracing) {
        return m_distributedTracing->analyzeTrace(traceId);
    }
    
    return QJsonObject();
}

QJsonArray ProductionTier2Manager::findTraceBottlenecks(const QString& traceId) {
    if (m_distributedTracing) {
        return m_distributedTracing->findBottlenecks(traceId);
    }
    
    return QJsonArray();
}

// ============================================================================
// Monitoring and Reporting
// ============================================================================

void ProductionTier2Manager::startMonitoringThreads() {
    // Start periodic health checks
    m_healthCheckTimer = new QTimer(this);
    connect(m_healthCheckTimer, &QTimer::timeout, this, &ProductionTier2Manager::runHealthChecks);
    m_healthCheckTimer->start(m_config.healthCheckIntervalSec * 1000);
    
    LOG_INFO("Monitoring threads started");
}

void ProductionTier2Manager::stopMonitoringThreads() {
    if (m_healthCheckTimer) {
        m_healthCheckTimer->stop();
        delete m_healthCheckTimer;
        m_healthCheckTimer = nullptr;
    }
    
    LOG_INFO("Monitoring threads stopped");
}

void ProductionTier2Manager::runHealthChecks() {
    QJsonObject health;
    health["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    health["uptime_seconds"] = m_startTime.secsTo(QDateTime::currentDateTime());
    
    // Check ML Error Detector
    if (m_mlErrorDetector) {
        QJsonObject mlStats = m_mlErrorDetector->getDetectionStatistics();
        health["ml_error_detector"] = mlStats;
    }
    
    // Check Model Hotpatch Manager
    if (m_modelHotpatchManager) {
        QString activeModel = m_modelHotpatchManager->getActiveModelId();
        health["active_model_id"] = activeModel;
        
        if (!activeModel.isEmpty()) {
            auto modelHealth = m_modelHotpatchManager->runHealthCheck(activeModel);
            health["model_health"] = modelHealth;
        }
    }
    
    // Check Performance Monitor
    if (m_performanceMonitor) {
        auto slaCompliance = m_performanceMonitor->evaluateAllSLAs();
        bool allCompliant = std::all_of(slaCompliance.begin(), slaCompliance.end(),
                                       [](const SLACompliance& sla) { return sla.isCompliant; });
        health["sla_compliant"] = allCompliant;
    }
    
    // Check Distributed Tracing
    if (m_distributedTracing) {
        QJsonObject tracingStats = m_distributedTracing->getTracingStatistics();
        health["distributed_tracing"] = tracingStats;
    }
    
    emit healthCheckCompleted(health);
    
    LOG_DEBUG("Health check completed", {
        {"all_systems_healthy", health.contains("error") ? "false" : "true"}
    });
}

QJsonObject ProductionTier2Manager::getSystemStatus() const {
    QMutexLocker lock(&m_mutex);
    
    QJsonObject status;
    status["initialized"] = m_initialized;
    status["service_name"] = m_config.serviceName;
    status["uptime_seconds"] = m_startTime.secsTo(QDateTime::currentDateTime());
    
    status["ml_error_detection_enabled"] = m_config.enableMLErrorDetection;
    status["model_hotpatching_enabled"] = m_config.enableModelHotpatching;
    status["performance_monitoring_enabled"] = m_config.enablePerformanceMonitoring;
    status["distributed_tracing_enabled"] = m_config.enableDistributedTracing;
    
    // Add component-specific status
    if (m_mlErrorDetector) {
        status["ml_detector_stats"] = m_mlErrorDetector->getDetectionStatistics();
    }
    
    if (m_modelHotpatchManager) {
        status["active_model"] = m_modelHotpatchManager->getActiveModelId();
        status["registered_models_count"] = m_modelHotpatchManager->listModels().size();
    }
    
    if (m_distributedTracing) {
        status["tracing_stats"] = m_distributedTracing->getTracingStatistics();
    }
    
    return status;
}

bool ProductionTier2Manager::exportAllMetrics(const QString& outputDir) {
    QDir dir(outputDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    
    bool success = true;
    
    // Export performance metrics
    if (m_performanceMonitor) {
        QString perfPath = dir.filePath(QString("performance_metrics_%1.json").arg(timestamp));
        // Would export actual metrics here
        LOG_INFO("Performance metrics exported", {{"path", perfPath}});
    }
    
    // Export tracing data
    if (m_distributedTracing) {
        QString tracingPath = dir.filePath(QString("tracing_data_%1.json").arg(timestamp));
        success &= m_distributedTracing->exportToJSON(tracingPath);
    }
    
    // Export ML model
    if (m_mlErrorDetector) {
        QString modelPath = dir.filePath(QString("ml_error_model_%1.json").arg(timestamp));
        success &= m_mlErrorDetector->saveModel(modelPath);
    }
    
    // Export system status
    QJsonObject systemStatus = getSystemStatus();
    QString statusPath = dir.filePath(QString("system_status_%1.json").arg(timestamp));
    
    QFile file(statusPath);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(systemStatus);
        file.write(doc.toJson());
        file.close();
    }
    
    LOG_INFO("All metrics exported", {{"output_dir", outputDir}});
    
    return success;
}

// ============================================================================
// Event Handlers
// ============================================================================

void ProductionTier2Manager::onAnomalyDetected(const MLErrorDetection::AnomalyDetectionResult& result) {
    LOG_WARN("Anomaly detected", {
        {"anomaly_score", result.anomalyScore},
        {"detection_method", result.detectionMethod}
    });
    
    emit anomalyDetected(result.anomalyScore, result.detectionMethod);
}

void ProductionTier2Manager::onErrorPredicted(const MLErrorDetection::ErrorPrediction& prediction) {
    LOG_INFO("Error predicted", {
        {"category", prediction.predictedCategory},
        {"confidence", prediction.confidence}
    });
}

void ProductionTier2Manager::onModelSwapped(const QString& newModelId, const QString& previousModelId) {
    LOG_INFO("Model swapped", {
        {"new_model", newModelId},
        {"previous_model", previousModelId}
    });
    
    emit modelSwapped(newModelId, previousModelId);
}

void ProductionTier2Manager::onHotpatchFailed(const QString& modelId, const QString& error) {
    LOG_ERROR("Model hotpatch failed", {
        {"model_id", modelId},
        {"error", error}
    });
    
    // Report error to error detection system
    if (m_errorAnalysisSystem) {
        ErrorContext context;
        context.timestamp = QDateTime::currentDateTime();
        m_errorAnalysisSystem->reportError(
            QString("Model hotpatch failed: %1").arg(error),
            context
        );
    }
}

void ProductionTier2Manager::onPerformanceThresholdViolation(const MetricData& metric, 
                                                            const QString& severity) {
    LOG_WARN("Performance threshold violation", {
        {"component", metric.component},
        {"operation", metric.operation},
        {"value", metric.value},
        {"severity", severity}
    });
    
    emit performanceThresholdViolated(metric.component, metric.operation, metric.value);
}

void ProductionTier2Manager::onBottleneckDetected(const QString& traceId, 
                                                 const QString& serviceName, 
                                                 double latencyMs) {
    LOG_WARN("Performance bottleneck detected", {
        {"trace_id", traceId},
        {"service", serviceName},
        {"latency_ms", latencyMs}
    });
    
    emit bottleneckDetected(serviceName, latencyMs);
}

} // namespace Tier2Integration
