/**
 * @file advanced_features_integration.h
 * @brief Integration of Advanced Features into MainWindow_v5
 * 
 * This file provides the complete integration of:
 * 1. Distributed Tracing with execution trace visualization
 * 2. Memory Persistence with UI-integrated snapshots
 * 3. Real-time Refactoring with automatic code improvement
 * 4. Test Generation with automated test creation
 * 5. Visual Dashboard with capability/performance monitoring
 * 
 * All components are wired to MainWindow with proper signal/slot connections.
 * 
 * @author RawrXD Team
 * @date 2026-01-10
 * @version 1.0.0
 */

#pragma once

#include <QObject>
#include <QString>
#include <QWidget>
#include <QDockWidget>
#include <memory>

// Feature widgets
#include "distributed_tracing.h"
#include "memory_snapshot_widget.h"
#include "realtime_refactoring_widget.h"
#include "test_generation_widget.h"
#include "capability_monitor_dashboard.h"
#include "execution_visualizer.h"
#include "session_persistence.h"

// Forward declarations
class QMainWindow;
class AgenticExecutor;
class AutonomousIntelligenceOrchestrator;

/**
 * @class AdvancedFeaturesIntegration
 * @brief Central integration manager for all advanced features
 * 
 * This class manages the lifecycle and integration of all advanced features,
 * connecting them to MainWindow and ensuring proper signal/slot wiring.
 */
class AdvancedFeaturesIntegration : public QObject {
    Q_OBJECT

public:
    explicit AdvancedFeaturesIntegration(QMainWindow* mainWindow, QObject* parent = nullptr);
    ~AdvancedFeaturesIntegration() override;

    // Initialize all features
    void initializeAllFeatures();
    
    // Individual feature initialization
    void initializeDistributedTracing();
    void initializeMemorySnapshots();
    void initializeRealtimeRefactoring();
    void initializeTestGeneration();
    void initializeVisualDashboard();
    
    // Connect to core systems
    void connectToAgenticExecutor(AgenticExecutor* executor);
    void connectToAutonomousOrchestrator(AutonomousIntelligenceOrchestrator* orchestrator);
    
    // Feature access
    DistributedTracing::TraceVisualizationWidget* traceWidget() const;
    MemorySnapshotWidget* snapshotWidget() const;
    RealtimeRefactoringWidget* refactoringWidget() const;
    TestGenerationWidget* testGenWidget() const;
    CapabilityMonitorDashboard* dashboardWidget() const;
    
    // Dock widget access for MainWindow integration
    QDockWidget* traceDock() const { return m_traceDock; }
    QDockWidget* snapshotDock() const { return m_snapshotDock; }
    QDockWidget* refactoringDock() const { return m_refactoringDock; }
    QDockWidget* testGenDock() const { return m_testGenDock; }
    QDockWidget* dashboardDock() const { return m_dashboardDock; }
    
    // Enable/disable features
    void enableDistributedTracing(bool enable);
    void enableMemorySnapshots(bool enable);
    void enableRealtimeRefactoring(bool enable);
    void enableTestGeneration(bool enable);
    void enableVisualDashboard(bool enable);

public slots:
    // Distributed Tracing integration
    void onAgenticTaskStarted(const QString& taskId, const QString& description);
    void onAgenticTaskCompleted(const QString& taskId, bool success);
    void onAgenticPhaseChanged(const QString& phase);
    
    // Memory Snapshot integration
    void onSessionStateChanged();
    void createAutoSnapshot();
    void loadSessionFromSnapshot(const QString& snapshotId);
    
    // Refactoring integration
    void onCodeEditorTextChanged(const QString& code, const QString& filePath);
    void onRefactoringSuggestionApplied(const RefactoringProposal& proposal);
    
    // Test Generation integration
    void onFileOpened(const QString& filePath);
    void generateTestsForCurrentFile();
    void runGeneratedTests();
    
    // Dashboard integration
    void onPerformanceMetricsUpdated(const QJsonObject& metrics);
    void onResourceAlert(const QString& alertType, const QString& message);

signals:
    // Feature status signals
    void distributedTracingReady();
    void memorySnapshotsReady();
    void realtimeRefactoringReady();
    void testGenerationReady();
    void visualDashboardReady();
    void allFeaturesReady();
    
    // Integration events
    void traceExported(const QString& filePath);
    void snapshotCreated(const QString& snapshotId);
    void refactoringApplied(const QString& filePath);
    void testsGenerated(int count);
    void performanceAlertTriggered(const QString& message);

private:
    void createDockWidgets();
    void setupSignalConnections();
    void wireDistributedTracing();
    void wireMemorySnapshots();
    void wireRealtimeRefactoring();
    void wireTestGeneration();
    void wireVisualDashboard();
    
    QMainWindow* m_mainWindow = nullptr;
    
    // Feature widgets
    std::unique_ptr<DistributedTracing::TraceVisualizationWidget> m_traceWidget;
    std::unique_ptr<MemorySnapshotWidget> m_snapshotWidget;
    std::unique_ptr<RealtimeRefactoringWidget> m_refactoringWidget;
    std::unique_ptr<TestGenerationWidget> m_testGenWidget;
    std::unique_ptr<CapabilityMonitorDashboard> m_dashboardWidget;
    std::unique_ptr<ExecutionVisualizer> m_executionVisualizer;
    
    // Dock widgets
    QDockWidget* m_traceDock = nullptr;
    QDockWidget* m_snapshotDock = nullptr;
    QDockWidget* m_refactoringDock = nullptr;
    QDockWidget* m_testGenDock = nullptr;
    QDockWidget* m_dashboardDock = nullptr;
    QDockWidget* m_executionVisualizerDock = nullptr;
    
    // Core system connections
    AgenticExecutor* m_agenticExecutor = nullptr;
    AutonomousIntelligenceOrchestrator* m_orchestrator = nullptr;
    
    // State
    bool m_initialized = false;
    QMap<QString, QString> m_currentTraces; // taskId -> traceId mapping
};

// ============================================================================
// MAINWINDOW INTEGRATION HELPER MACROS
// ============================================================================

/**
 * Add these declarations to MainWindow.h private section:
 * 
 * // Advanced Features Integration
 * AdvancedFeaturesIntegration* m_advancedFeatures = nullptr;
 * 
 * // Feature toggle actions
 * QAction* m_toggleDistributedTracingAction = nullptr;
 * QAction* m_toggleMemorySnapshotsAction = nullptr;
 * QAction* m_toggleRealtimeRefactoringAction = nullptr;
 * QAction* m_toggleTestGenerationAction = nullptr;
 * QAction* m_toggleVisualDashboardAction = nullptr;
 */

/**
 * Add these method declarations to MainWindow.h private slots:
 * 
 * void toggleDistributedTracing(bool visible);
 * void toggleMemorySnapshots(bool visible);
 * void toggleRealtimeRefactoring(bool visible);
 * void toggleTestGeneration(bool visible);
 * void toggleVisualDashboard(bool visible);
 * void onAdvancedFeaturesReady();
 */

/**
 * Add this to MainWindow constructor (after setupMenuBar, setupDockWidgets):
 * 
 * // Initialize advanced features
 * m_advancedFeatures = new AdvancedFeaturesIntegration(this, this);
 * connect(m_advancedFeatures, &AdvancedFeaturesIntegration::allFeaturesReady,
 *         this, &MainWindow::onAdvancedFeaturesReady);
 * 
 * // Initialize with agent systems
 * if (m_agenticExecutor) {
 *     m_advancedFeatures->connectToAgenticExecutor(m_agenticExecutor);
 * }
 * if (m_autonomousSystemsIntegration && m_autonomousSystemsIntegration->getOrchestrator()) {
 *     m_advancedFeatures->connectToAutonomousOrchestrator(
 *         m_autonomousSystemsIntegration->getOrchestrator());
 * }
 * 
 * // Initialize all features
 * QTimer::singleShot(2000, m_advancedFeatures, &AdvancedFeaturesIntegration::initializeAllFeatures);
 */

/**
 * Add this to setupMenuBar() - create "Advanced" menu:
 * 
 * QMenu* advancedMenu = menuBar()->addMenu(tr("&Advanced"));
 * 
 * m_toggleDistributedTracingAction = advancedMenu->addAction(tr("Distributed &Tracing"));
 * m_toggleDistributedTracingAction->setCheckable(true);
 * connect(m_toggleDistributedTracingAction, &QAction::toggled,
 *         this, &MainWindow::toggleDistributedTracing);
 * 
 * m_toggleMemorySnapshotsAction = advancedMenu->addAction(tr("Memory &Snapshots"));
 * m_toggleMemorySnapshotsAction->setCheckable(true);
 * connect(m_toggleMemorySnapshotsAction, &QAction::toggled,
 *         this, &MainWindow::toggleMemorySnapshots);
 * 
 * m_toggleRealtimeRefactoringAction = advancedMenu->addAction(tr("Real-time &Refactoring"));
 * m_toggleRealtimeRefactoringAction->setCheckable(true);
 * connect(m_toggleRealtimeRefactoringAction, &QAction::toggled,
 *         this, &MainWindow::toggleRealtimeRefactoring);
 * 
 * m_toggleTestGenerationAction = advancedMenu->addAction(tr("Test &Generation"));
 * m_toggleTestGenerationAction->setCheckable(true);
 * connect(m_toggleTestGenerationAction, &QAction::toggled,
 *         this, &MainWindow::toggleTestGeneration);
 * 
 * m_toggleVisualDashboardAction = advancedMenu->addAction(tr("Visual &Dashboard"));
 * m_toggleVisualDashboardAction->setCheckable(true);
 * connect(m_toggleVisualDashboardAction, &QAction::toggled,
 *         this, &MainWindow::toggleVisualDashboard);
 * 
 * advancedMenu->addSeparator();
 * advancedMenu->addAction(tr("Export All Traces..."), m_advancedFeatures, 
 *                         [this]() { m_advancedFeatures->traceWidget()->exportCurrentTrace(); });
 * advancedMenu->addAction(tr("Create Memory Snapshot"), m_advancedFeatures,
 *                         &AdvancedFeaturesIntegration::createAutoSnapshot);
 */

/**
 * Add these slot implementations to MainWindow.cpp:
 * 
 * void MainWindow::toggleDistributedTracing(bool visible) {
 *     if (m_advancedFeatures && m_advancedFeatures->traceDock()) {
 *         m_advancedFeatures->traceDock()->setVisible(visible);
 *         m_advancedFeatures->enableDistributedTracing(visible);
 *     }
 * }
 * 
 * void MainWindow::toggleMemorySnapshots(bool visible) {
 *     if (m_advancedFeatures && m_advancedFeatures->snapshotDock()) {
 *         m_advancedFeatures->snapshotDock()->setVisible(visible);
 *         m_advancedFeatures->enableMemorySnapshots(visible);
 *     }
 * }
 * 
 * void MainWindow::toggleRealtimeRefactoring(bool visible) {
 *     if (m_advancedFeatures && m_advancedFeatures->refactoringDock()) {
 *         m_advancedFeatures->refactoringDock()->setVisible(visible);
 *         m_advancedFeatures->enableRealtimeRefactoring(visible);
 *     }
 * }
 * 
 * void MainWindow::toggleTestGeneration(bool visible) {
 *     if (m_advancedFeatures && m_advancedFeatures->testGenDock()) {
 *         m_advancedFeatures->testGenDock()->setVisible(visible);
 *         m_advancedFeatures->enableTestGeneration(visible);
 *     }
 * }
 * 
 * void MainWindow::toggleVisualDashboard(bool visible) {
 *     if (m_advancedFeatures && m_advancedFeatures->dashboardDock()) {
 *         m_advancedFeatures->dashboardDock()->setVisible(visible);
 *         m_advancedFeatures->enableVisualDashboard(visible);
 *     }
 * }
 * 
 * void MainWindow::onAdvancedFeaturesReady() {
 *     statusBar()->showMessage("Advanced features ready: Distributed Tracing, "
 *                              "Memory Snapshots, Refactoring, Test Generation, Dashboard", 5000);
 *     qInfo() << "[MainWindow] All advanced features initialized successfully";
 * }
 */

#endif // ADVANCED_FEATURES_INTEGRATION_H
