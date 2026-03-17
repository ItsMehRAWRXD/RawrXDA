#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>

// Forward declarations
class AgenticExecutor;
class AutonomousFeatureEngine;
class AutonomousAdvancedExecutor;
class AutonomousObservabilitySystem;
class AutonomousRealtimeFeedbackSystem;
class AutonomousIntelligenceOrchestrator;

/**
 * @class AutonomousSystemsIntegration
 * @brief Complete integration of all autonomous systems
 * 
 * This is the master control center that coordinates all autonomous
 * components into a unified, production-ready system.
 * 
 * Features:
 * - Unified interface to all autonomous systems
 * - Cross-system coordination and messaging
 * - Integrated error handling and recovery
 * - Comprehensive monitoring and analytics
 * - Real-time feedback and visualization
 * - Production deployment ready
 */
class AutonomousSystemsIntegration {
public:
    explicit AutonomousSystemsIntegration();
    ~AutonomousSystemsIntegration();
    
    // Initialize all systems
    void initialize();
    void initializeWithConfig(const QJsonObject& config);
    
    // Access subsystems
    AgenticExecutor* getAgenticExecutor();
    AutonomousFeatureEngine* getFeatureEngine();
    AutonomousAdvancedExecutor* getAdvancedExecutor();
    AutonomousObservabilitySystem* getObservabilitySystem();
    AutonomousRealtimeFeedbackSystem* getFeedbackSystem();
    AutonomousIntelligenceOrchestrator* getOrchestrator();
    
    // Unified execution interface
    QJsonObject executeTask(const QString& taskDescription);
    QJsonObject executeComplexTask(const QString& taskDescription, const QJsonObject& context);
    QJsonArray executeParallelTasks(const QJsonArray& tasks);
    
    // Code analysis integration
    QJsonObject analyzeCode(const QString& code, const QString& filePath, const QString& language);
    QJsonArray generateCodeSuggestions(const QString& code);
    QJsonObject assessCodeQuality(const QString& code);
    
    // Real-time monitoring
    QJsonObject getSystemStatus() const;
    QJsonObject getPerformanceMetrics() const;
    QString getHealthReport() const;
    
    // Configuration management
    void enableDetailedLogging(bool enable);
    void enableDetailedMonitoring(bool enable);
    void setMaxParallelTasks(int max);
    void setExecutionTimeout(int seconds);
    
    // Export and reporting
    QString exportFullReport() const;
    QString exportMetricsReport() const;
    QString exportPerformanceAnalysis() const;
    QString exportAuditLog() const;
    
    // Error handling
    void handleSystemError(const QString& error);
    QJsonObject getErrorHistory() const;
    
    // Shutdown
    void shutdown();

private:
    // Initialize individual systems
    void initializeAgenticExecutor();
    void initializeFeatureEngine();
    void initializeAdvancedExecutor();
    void initializeObservabilitySystem();
    void initializeFeedbackSystem();
    void initializeOrchestrator();
    
    // Inter-system communication
    void connectSystemSignals();
    void setupCrossSystemMessaging();
    
    // Member systems (unique_ptrs manage lifetime)
    std::unique_ptr<AgenticExecutor> m_agenticExecutor;
    std::unique_ptr<AutonomousFeatureEngine> m_featureEngine;
    std::unique_ptr<AutonomousAdvancedExecutor> m_advancedExecutor;
    std::unique_ptr<AutonomousObservabilitySystem> m_observabilitySystem;
    std::unique_ptr<AutonomousRealtimeFeedbackSystem> m_feedbackSystem;
    std::unique_ptr<AutonomousIntelligenceOrchestrator> m_orchestrator;
    
    // Configuration
    bool m_detailedLoggingEnabled = false;
    bool m_detailedMonitoringEnabled = false;
    int m_maxParallelTasks = 4;
    int m_executionTimeoutSeconds = 300;
    
    // State
    bool m_initialized = false;
    QJsonArray m_errorHistory;
};
