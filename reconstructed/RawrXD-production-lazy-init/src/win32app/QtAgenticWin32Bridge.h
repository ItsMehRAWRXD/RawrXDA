#pragma once

/**
 * @file QtAgenticWin32Bridge.h
 * @brief Bridge between Qt-based AgenticEngine and Win32 Native API
 * 
 * This module provides integration between the cross-platform Qt-based agentic
 * engines and the Windows-specific Win32 native API, enabling full system access
 * while maintaining portability of the core agentic logic.
 */

#include "Win32NativeAgentAPI.h"
#include "../agentic_engine.h"
#include "../autonomous_feature_engine.h"
#include "../autonomous_intelligence_orchestrator.h"
#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutex>
#include <QTimer>
#include <QJsonArray>
#include <memory>
#include <functional>

namespace RawrXD {
namespace Bridge {

/**
 * @class QtAgenticWin32Bridge
 * @brief Integrates Qt agentic engines with Win32 native capabilities
 * 
 * This class serves as a bridge between the Qt-based agentic/autonomous systems
 * and the Win32 native API, providing:
 * - File system operations through Win32
 * - Process and thread management
 * - Memory and resource monitoring
 * - System-level diagnostics
 * - Registry and service operations
 */
class QtAgenticWin32Bridge : public QObject {
    Q_OBJECT

public:
    explicit QtAgenticWin32Bridge(QObject* parent = nullptr);
    ~QtAgenticWin32Bridge();

    // Initialize bridge with agentic engines
    void initialize(QObject* agenticEngine, QObject* autonomyEngine, QObject* orchestrator);
    
    // Integration with AgenticEngine
    QString win32AnalyzeCode(const QString& code, const QString& filePath);
    QString win32GenerateCode(const QString& prompt);
    QJsonObject win32DetectPatterns(const QString& code);
    
    // Integration with Autonomy systems
    QString win32ExecuteCommand(const QString& command);
    QJsonObject win32GetSystemInfo();
    QString win32ManageProcess(const QString& action, const QString& target);
    
    // Advanced Win32 operations for agents
    QString win32ReadFile(const QString& path, int startLine = -1, int endLine = -1);
    bool win32WriteFile(const QString& path, const QString& content);
    QString win32ListFiles(const QString& directory, const QString& pattern = "*");
    QString win32SearchFiles(const QString& directory, const QString& pattern, bool recursive = true);
    
    // System monitoring
    QJsonObject win32GetSystemStatus();
    QJsonObject win32GetProcessInfo(const QString& processNameOrId);
    QJsonObject win32GetMemoryInfo();
    QJsonObject win32GetNetworkInfo();
    
    // Service and registry operations
    QString win32ManageService(const QString& action, const QString& serviceName);
    QString win32ManageRegistry(const QString& action, const QString& keyPath, const QString& valueName, const QString& value);
    
    // Security and elevation
    bool win32IsElevated();
    QString win32RequestElevation();
    QJsonObject win32GetSecurityContext();

signals:
    // Signal integration with Qt systems
    void agenticWin32OperationCompleted(const QString& operation, const QString& result);
    void agenticWin32Error(const QString& operation, const QString& error);
    void agenticWin32SystemEvent(const QString& event, const QJsonObject& data);
    
    // Bridge signals
    void win32CapabilityDiscovered(const QString& capability);
    void win32ResourceUsageChanged(const QJsonObject& usage);
    void win32SecurityAlert(const QString& alert);

public slots:
    // Qt integration slots
    void onAgenticEngineRequest(const QString& operation, const QJsonObject& params);
    void onAutonomyEngineRequest(const QString& operation, const QJsonObject& params);
    void onOrchestratorRequest(const QString& operation, const QJsonObject& params);
    
    // Win32 event handling
    void onWin32Event(const QString& event, const QJsonObject& data);
    void onResourceMonitorTick();
    void onSystemMonitorTick();
    
    // Parallel operations support (not a slot, std::function doesn't work with MOC)
    QJsonArray executeParallelOperations(const QJsonArray& operations);
    
public:
    // Async operations - not a Qt slot since std::function doesn't work with Qt MOC
    void executeParallelOperationsAsync(const QJsonArray& operations, 
                                        std::function<void(const QJsonArray&)> callback);

private:
    // Bridge state
    bool m_initialized;
    QMutex m_mutex;
    
    // Win32 API access
    std::unique_ptr<Win32Agent::Win32AgentAPI> m_win32API;
    
    // Connected Qt objects
    QObject* m_agenticEngine;
    QObject* m_autonomyEngine;
    QObject* m_orchestrator;
    
    // Monitoring timers
    QTimer* m_resourceMonitor;
    QTimer* m_systemMonitor;
    
    // Internal helpers
    QString executeWin32Operation(const QString& operation, const QJsonObject& params);
    QString formatWin32Result(const Win32Agent::ProcessCreateResult& result);
    QString formatWin32Result(const Win32Agent::ProcessInfo& info);
    QJsonObject formatWin32Result(const Win32Agent::SystemInfo& info);
    QJsonObject formatWin32Result(const Win32Agent::MemoryInfo& info);
    void logWin32Operation(const QString& operation, const QString& result, bool success);
    
    // Error handling
    QString handleWin32Error(const QString& operation);
    void emitWin32Error(const QString& operation, const QString& error);
    void emitWin32Event(const QString& event, const QJsonObject& data);
};

/**
 * @class AgenticEngineWin32Integration
 * @brief Provides Win32-specific extensions to the Qt AgenticEngine
 */
class AgenticEngineWin32Integration {
public:
    explicit AgenticEngineWin32Integration(QObject* agenticEngine, QtAgenticWin32Bridge* bridge);
    ~AgenticEngineWin32Integration();
    
    // Enhanced agent capabilities with Win32
    QString executeSystemCommand(const QString& command);
    QString analyzeSystemResourceUsage(const QString& processName);
    QString optimizeSystemPerformance();
    QString diagnoseSystemIssues();
    
    // Win32-specific code analysis
    QString analyzeWin32Code(const QString& code);
    QString generateWin32Code(const QString& specification);
    
    // System integration
    QString getSystemDiagnostics();
    QString performSystemMaintenance();
    
private:
    QObject* m_agenticEngine;
    QtAgenticWin32Bridge* m_bridge;
    QString m_lastDiagnostics;
    
    // Helper methods
    QString runSystemDiagnostic(const QString& category);
    QString generateOptimizationSuggestions();
    QString formatDiagnosticResults(const QJsonObject& results);
};

/**
 * @class AutonomyWin32Integration
 * @brief Provides Win32 system access to autonomous engines
 */
class AutonomyWin32Integration {
public:
    explicit AutonomyWin32Integration(QObject* autonomyEngine, QtAgenticWin32Bridge* bridge);
    ~AutonomyWin32Integration();
    
    // Autonomous system operations
    QJsonObject executeAutonomousWin32Task(const QString& task, const QJsonObject& context);
    QString planWin32Operations(const QString& goal);
    QJsonObject monitorWin32SystemState();
    
    // Resource management
    QString allocateWin32Resources(const QJsonObject& requirements);
    QString releaseWin32Resources(const QString& resourceId);
    QString optimizeWin32ResourceUsage();
    
    // System monitoring
    QJsonObject getAutonomousSystemStatus();
    QString performAutonomousMaintenance();
    
private:
    QObject* m_autonomyEngine;
    QtAgenticWin32Bridge* m_bridge;
    
    // System state tracking
    QJsonObject m_systemState;
    QMutex m_stateMutex;
    
    // Helper methods
    void updateSystemState();
    QString planResourceAllocation(const QJsonObject& requirements);
    QString executeResourceOptimization();
};

/**
 * @class Win32SystemObserver
 * @brief Observes system events and notifies agentic engines
 */
class Win32SystemObserver : public QObject {
    Q_OBJECT

public:
    explicit Win32SystemObserver(QtAgenticWin32Bridge* bridge, QObject* parent = nullptr);
    ~Win32SystemObserver();

    void startObserving();
    void stopObserving();

signals:
    void systemEventObserved(const QString& event, const QJsonObject& data);
    void performanceEventObserved(const QString& metric, double value);
    void securityEventObserved(const QString& event, const QString& details);

private slots:
    void onProcessEvent(const QString& event, const QJsonObject& data);
    void onMemoryEvent(const QString& event, const QJsonObject& data);
    void onFileSystemEvent(const QString& event, const QJsonObject& data);
    void onNetworkEvent(const QString& event, const QJsonObject& data);

private:
    QtAgenticWin32Bridge* m_bridge;
    bool m_observing;
    QTimer* m_observationTimer;
    
    // System state tracking
    QJsonObject m_lastSystemState;
    QJsonObject m_lastPerformanceMetrics;
    
    void observeSystemState();
    void detectSystemChanges();
    void emitPerformanceMetrics();
};

} // namespace Bridge
} // namespace RawrXD
