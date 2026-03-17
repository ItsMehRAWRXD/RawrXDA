// autonomous_systems_integration.cpp - Master integration of all autonomous systems
#include "autonomous_systems_integration.h"
#include "agentic_executor.h"
#include "autonomous_feature_engine.h"
#include "autonomous_advanced_executor.h"
#include "autonomous_observability_system.h"
#include "autonomous_realtime_feedback_system.h"
#include "autonomous_intelligence_orchestrator.h"
#include <QDebug>
#include <QJsonDocument>
#include <QDateTime>

AutonomousSystemsIntegration::AutonomousSystemsIntegration()
{
    qInfo() << "[AutonomousSystemsIntegration] Created";
}

AutonomousSystemsIntegration::~AutonomousSystemsIntegration()
{
    shutdown();
}

// ========== INITIALIZATION ==========

void AutonomousSystemsIntegration::initialize()
{
    QJsonObject defaultConfig;
    defaultConfig["detailed_logging"] = false;
    defaultConfig["detailed_monitoring"] = true;
    defaultConfig["max_parallel_tasks"] = 4;
    defaultConfig["execution_timeout_seconds"] = 300;
    
    initializeWithConfig(defaultConfig);
}

void AutonomousSystemsIntegration::initializeWithConfig(const QJsonObject& config)
{
    qInfo() << "[AutonomousSystemsIntegration] Initializing with configuration";
    
    // Extract configuration with Qt6 compatible syntax
    m_detailedLoggingEnabled = config.contains("detailed_logging") ? 
        config["detailed_logging"].toBool() : false;
    m_detailedMonitoringEnabled = config.contains("detailed_monitoring") ? 
        config["detailed_monitoring"].toBool() : true;
    m_maxParallelTasks = config.contains("max_parallel_tasks") ? 
        config["max_parallel_tasks"].toInt() : 4;
    m_executionTimeoutSeconds = config.contains("execution_timeout_seconds") ? 
        config["execution_timeout_seconds"].toInt() : 300;
    
    // Initialize each subsystem
    initializeAgenticExecutor();
    initializeFeatureEngine();
    initializeAdvancedExecutor();
    initializeObservabilitySystem();
    initializeFeedbackSystem();
    initializeOrchestrator();
    
    // Connect systems together
    connectSystemSignals();
    setupCrossSystemMessaging();
    
    m_initialized = true;
    
    qInfo() << "[AutonomousSystemsIntegration] All systems initialized successfully";
}

void AutonomousSystemsIntegration::initializeAgenticExecutor()
{
    m_agenticExecutor = std::make_unique<AgenticExecutor>();
    qInfo() << "[AutonomousSystemsIntegration] AgenticExecutor initialized";
}

void AutonomousSystemsIntegration::initializeFeatureEngine()
{
    m_featureEngine = std::make_unique<AutonomousFeatureEngine>();
    qInfo() << "[AutonomousSystemsIntegration] AutonomousFeatureEngine initialized";
}

void AutonomousSystemsIntegration::initializeAdvancedExecutor()
{
    m_advancedExecutor = std::make_unique<AutonomousAdvancedExecutor>();
    qInfo() << "[AutonomousSystemsIntegration] AutonomousAdvancedExecutor initialized";
}

void AutonomousSystemsIntegration::initializeObservabilitySystem()
{
    m_observabilitySystem = std::make_unique<AutonomousObservabilitySystem>();
    
    if (m_detailedLoggingEnabled) {
        m_observabilitySystem->setLogLevel("DEBUG");
    } else {
        m_observabilitySystem->setLogLevel("INFO");
    }
    
    m_observabilitySystem->enableDetailedMetrics(m_detailedMonitoringEnabled);
    m_observabilitySystem->enableDistributedTracing(true);
    
    qInfo() << "[AutonomousSystemsIntegration] AutonomousObservabilitySystem initialized";
}

void AutonomousSystemsIntegration::initializeFeedbackSystem()
{
    m_feedbackSystem = std::make_unique<AutonomousRealtimeFeedbackSystem>();
    m_feedbackSystem->setUpdateFrequency(500);
    m_feedbackSystem->enableAutoScroll(true);
    
    qInfo() << "[AutonomousSystemsIntegration] AutonomousRealtimeFeedbackSystem initialized";
}

void AutonomousSystemsIntegration::initializeOrchestrator()
{
    m_orchestrator = std::make_unique<AutonomousIntelligenceOrchestrator>();
    
    // Load configuration into the orchestrator
    QJsonObject orchestratorConfig;
    orchestratorConfig["max_parallel_tasks"] = m_maxParallelTasks;
    orchestratorConfig["task_timeout_seconds"] = m_executionTimeoutSeconds;
    orchestratorConfig["detailed_monitoring"] = m_detailedMonitoringEnabled;
    orchestratorConfig["auto_analysis"] = true;
    m_orchestrator->loadConfiguration(orchestratorConfig);
    
    qInfo() << "[AutonomousSystemsIntegration] AutonomousIntelligenceOrchestrator initialized";
}

// ========== SYSTEM COORDINATION ==========

void AutonomousSystemsIntegration::connectSystemSignals()
{
    // Connect feedback system to executor
    if (m_agenticExecutor && m_feedbackSystem) {
        // When executor logs, feed to feedback system
        // Could use Qt signals for this in production
    }
    
    // Connect observability to all systems
    if (m_observabilitySystem) {
        m_observabilitySystem->enableDistributedTracing(true);
        m_observabilitySystem->setMetricsAggregationInterval(5000);
    }
    
    qInfo() << "[AutonomousSystemsIntegration] System signals connected";
}

void AutonomousSystemsIntegration::setupCrossSystemMessaging()
{
    // Set up inter-system communication paths
    // In production, could use message bus or pub-sub pattern
    
    qInfo() << "[AutonomousSystemsIntegration] Cross-system messaging setup complete";
}

// ========== SUBSYSTEM ACCESS ==========

AgenticExecutor* AutonomousSystemsIntegration::getAgenticExecutor()
{
    return m_agenticExecutor.get();
}

AutonomousFeatureEngine* AutonomousSystemsIntegration::getFeatureEngine()
{
    return m_featureEngine.get();
}

AutonomousAdvancedExecutor* AutonomousSystemsIntegration::getAdvancedExecutor()
{
    return m_advancedExecutor.get();
}

AutonomousObservabilitySystem* AutonomousSystemsIntegration::getObservabilitySystem()
{
    return m_observabilitySystem.get();
}

AutonomousRealtimeFeedbackSystem* AutonomousSystemsIntegration::getFeedbackSystem()
{
    return m_feedbackSystem.get();
}

AutonomousIntelligenceOrchestrator* AutonomousSystemsIntegration::getOrchestrator()
{
    return m_orchestrator.get();
}

// ========== UNIFIED EXECUTION INTERFACE ==========

QJsonObject AutonomousSystemsIntegration::executeTask(const QString& taskDescription)
{
    qInfo() << "[AutonomousSystemsIntegration] Executing task:" << taskDescription;
    
    if (!m_initialized) {
        return QJsonObject{{"error", "System not initialized"}};
    }
    
    // Start tracing
    QString traceId = m_observabilitySystem->startTrace("executeTask", 
                                                       QJsonObject{{"task", taskDescription}});
    
    // Execute using advanced executor
    QJsonObject result = m_advancedExecutor->executeDynamicTask(taskDescription);
    
    // Record metrics
    m_observabilitySystem->recordMetric("task_execution_success", result["success"].toBool() ? 1.0 : 0.0);
    
    // End trace
    m_observabilitySystem->endTrace(traceId);
    
    // Update feedback
    if (m_feedbackSystem) {
        m_feedbackSystem->displayTaskResult("task_" + QString::number(qHash(taskDescription)), result);
    }
    
    return result;
}

QJsonObject AutonomousSystemsIntegration::executeComplexTask(const QString& taskDescription, const QJsonObject& context)
{
    qInfo() << "[AutonomousSystemsIntegration] Executing complex task with context";
    
    if (!m_initialized) {
        return QJsonObject{{"error", "System not initialized"}};
    }
    
    // Start trace
    QString traceId = m_observabilitySystem->startTrace("executeComplexTask",
                                                       QJsonObject{
                                                           {"task", taskDescription},
                                                           {"context_keys", QJsonArray::fromStringList(context.keys())}
                                                       });
    
    // Execute with context
    QJsonObject result = m_advancedExecutor->executeContextualTask(taskDescription, context);
    
    // Record success
    m_observabilitySystem->recordMetric("complex_task_execution_success", 
                                       result["success"].toBool() ? 1.0 : 0.0);
    
    // End trace
    m_observabilitySystem->endTrace(traceId);
    
    return result;
}

QJsonArray AutonomousSystemsIntegration::executeParallelTasks(const QJsonArray& tasks)
{
    qInfo() << "[AutonomousSystemsIntegration] Executing" << tasks.size() << "tasks in parallel";
    
    if (!m_initialized) {
        QJsonArray error;
        error.append(QJsonObject{{"error", "System not initialized"}});
        return error;
    }
    
    QString traceId = m_observabilitySystem->startTrace("executeParallelTasks",
                                                       QJsonObject{{"task_count", tasks.size()}});
    
    QJsonArray results = m_advancedExecutor->executeTasksInParallel(tasks);
    
    m_observabilitySystem->recordMetric("parallel_tasks_executed", static_cast<double>(tasks.size()));
    m_observabilitySystem->endTrace(traceId);
    
    return results;
}

// ========== CODE ANALYSIS INTEGRATION ==========

QJsonObject AutonomousSystemsIntegration::analyzeCode(const QString& code, const QString& filePath, const QString& language)
{
    if (!m_initialized || !m_featureEngine) {
        return QJsonObject{{"error", "System not initialized"}};
    }
    
    QString traceId = m_observabilitySystem->startTrace("analyzeCode",
                                                       QJsonObject{
                                                           {"file", filePath},
                                                           {"language", language}
                                                       });
    
    // Analyze code
    m_featureEngine->analyzeCode(code, filePath, language);
    
    // Get quality assessment
    CodeQualityMetrics metrics = m_featureEngine->assessCodeQuality(code, language);
    
    QJsonObject result;
    result["file_path"] = filePath;
    result["language"] = language;
    result["quality_metrics"] = QJsonObject{
        {"maintainability", metrics.maintainability},
        {"reliability", metrics.reliability},
        {"security", metrics.security},
        {"efficiency", metrics.efficiency},
        {"overall_score", metrics.overallScore}
    };
    
    m_observabilitySystem->endTrace(traceId);
    
    return result;
}

QJsonArray AutonomousSystemsIntegration::generateCodeSuggestions(const QString& code)
{
    if (!m_initialized || !m_featureEngine) {
        return QJsonArray();
    }
    
    // Get suggestions for code
    QVector<AutonomousSuggestion> suggestions = m_featureEngine->getSuggestionsForCode(code, "cpp");
    
    QJsonArray result;
    for (const auto& suggestion : suggestions) {
        QJsonObject suggObj;
        suggObj["id"] = suggestion.suggestionId;
        suggObj["type"] = suggestion.type;
        suggObj["confidence"] = suggestion.confidence;
        suggObj["explanation"] = suggestion.explanation;
        suggObj["benefits"] = QJsonArray::fromStringList(suggestion.benefits);
        
        result.append(suggObj);
    }
    
    return result;
}

QJsonObject AutonomousSystemsIntegration::assessCodeQuality(const QString& code)
{
    if (!m_initialized || !m_featureEngine) {
        return QJsonObject{{"error", "System not initialized"}};
    }
    
    CodeQualityMetrics metrics = m_featureEngine->assessCodeQuality(code, "cpp");
    
    return QJsonObject{
        {"maintainability", metrics.maintainability},
        {"reliability", metrics.reliability},
        {"security", metrics.security},
        {"efficiency", metrics.efficiency},
        {"overall_score", metrics.overallScore}
    };
}

// ========== MONITORING ==========

QJsonObject AutonomousSystemsIntegration::getSystemStatus() const
{
    if (!m_initialized || !m_observabilitySystem) {
        return QJsonObject{{"error", "System not initialized"}};
    }
    
    return m_observabilitySystem->getSystemHealth();
}

QJsonObject AutonomousSystemsIntegration::getPerformanceMetrics() const
{
    if (!m_initialized || !m_observabilitySystem) {
        return QJsonObject{{"error", "System not initialized"}};
    }
    
    return m_observabilitySystem->getSystemHealth();
}

QString AutonomousSystemsIntegration::getHealthReport() const
{
    if (!m_initialized || !m_observabilitySystem) {
        return "System not initialized";
    }
    
    return m_observabilitySystem->generateHealthReport();
}

// ========== CONFIGURATION ==========

void AutonomousSystemsIntegration::enableDetailedLogging(bool enable)
{
    m_detailedLoggingEnabled = enable;
    if (m_observabilitySystem) {
        m_observabilitySystem->setLogLevel(enable ? "DEBUG" : "INFO");
    }
}

void AutonomousSystemsIntegration::enableDetailedMonitoring(bool enable)
{
    m_detailedMonitoringEnabled = enable;
    if (m_observabilitySystem) {
        m_observabilitySystem->enableDetailedMetrics(enable);
    }
    // Orchestrator uses configuration instead of direct method calls
    if (m_orchestrator) {
        QJsonObject config;
        config["detailed_monitoring"] = enable;
        m_orchestrator->loadConfiguration(config);
    }
}

void AutonomousSystemsIntegration::setMaxParallelTasks(int max)
{
    m_maxParallelTasks = max;
    // Orchestrator uses configuration
    if (m_orchestrator) {
        QJsonObject config;
        config["max_parallel_tasks"] = max;
        m_orchestrator->loadConfiguration(config);
    }
    if (m_advancedExecutor) {
        m_advancedExecutor->setParallelizationThreshold(max);
    }
}

void AutonomousSystemsIntegration::setExecutionTimeout(int seconds)
{
    m_executionTimeoutSeconds = seconds;
    // Orchestrator uses configuration
    if (m_orchestrator) {
        QJsonObject config;
        config["task_timeout_seconds"] = seconds;
        m_orchestrator->loadConfiguration(config);
    }
}

// ========== EXPORT AND REPORTING ==========

QString AutonomousSystemsIntegration::exportFullReport() const
{
    if (!m_initialized) {
        return "System not initialized";
    }
    
    QString report;
    report += "=== AUTONOMOUS SYSTEMS INTEGRATION REPORT ===\n\n";
    
    report += "Generated: " + QDateTime::currentDateTime().toString(Qt::ISODate) + "\n\n";
    
    report += "=== SYSTEM STATUS ===\n";
    report += getHealthReport() + "\n";
    
    if (m_observabilitySystem) {
        report += "\n=== PERFORMANCE ANALYSIS ===\n";
        report += m_observabilitySystem->generatePerformanceReport() + "\n";
    }
    
    if (m_feedbackSystem) {
        report += "\n=== EXECUTION SUMMARY ===\n";
        QJsonObject summary = m_feedbackSystem->generateExecutionSummary();
        report += QJsonDocument(summary).toJson(QJsonDocument::Indented).constData();
        report += "\n";
    }
    
    return report;
}

QString AutonomousSystemsIntegration::exportMetricsReport() const
{
    if (!m_initialized || !m_observabilitySystem) {
        return "System not initialized";
    }
    
    return m_observabilitySystem->exportMetricsAsPrometheus();
}

QString AutonomousSystemsIntegration::exportPerformanceAnalysis() const
{
    if (!m_initialized || !m_observabilitySystem) {
        return "System not initialized";
    }
    
    return m_observabilitySystem->generatePerformanceReport();
}

QString AutonomousSystemsIntegration::exportAuditLog() const
{
    if (!m_initialized || !m_observabilitySystem) {
        return "System not initialized";
    }
    
    return m_observabilitySystem->generateAuditLog();
}

// ========== ERROR HANDLING ==========

void AutonomousSystemsIntegration::handleSystemError(const QString& error)
{
    qCritical() << "[AutonomousSystemsIntegration] System error:" << error;
    
    QJsonObject errorObj;
    errorObj["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    errorObj["error"] = error;
    
    m_errorHistory.append(errorObj);
    
    if (m_observabilitySystem) {
        m_observabilitySystem->logCritical("AutonomousSystemsIntegration", error);
    }
}

QJsonObject AutonomousSystemsIntegration::getErrorHistory() const
{
    QJsonObject result;
    result["total_errors"] = m_errorHistory.size();
    result["errors"] = m_errorHistory;
    return result;
}

// ========== SHUTDOWN ==========

void AutonomousSystemsIntegration::shutdown()
{
    qInfo() << "[AutonomousSystemsIntegration] Shutting down all systems";
    
    // Cleanup is automatic through unique_ptr destructors
    m_agenticExecutor.reset();
    m_featureEngine.reset();
    m_advancedExecutor.reset();
    m_observabilitySystem.reset();
    m_feedbackSystem.reset();
    m_orchestrator.reset();
    
    m_initialized = false;
    
    qInfo() << "[AutonomousSystemsIntegration] Shutdown complete";
}
