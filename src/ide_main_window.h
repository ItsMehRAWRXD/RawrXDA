// ide_main_window.h - Main IDE Window with Full Integration
#ifndef IDE_MAIN_WINDOW_H
#define IDE_MAIN_WINDOW_H


#include "autonomous_model_manager.h"
#include "intelligent_codebase_engine.h"
#include "autonomous_feature_engine.h"
#include "hybrid_cloud_manager.h"
#include "error_recovery_system.h"
#include "performance_monitor.h"
#include "model_router_adapter.h"
#include "model_router_widget.h"
#include "cloud_settings_dialog.h"
#include "metrics_dashboard.h"
#include "model_router_console.h"

class AutonomousSuggestionWidget;
class SecurityAlertWidget;
class OptimizationPanelWidget;

class IDEMainWindow : public void {

public:
    explicit IDEMainWindow(void *parent = nullptr);
    ~IDEMainWindow();

private:
    // File menu actions
    void onNewFile();
    void onOpenFile();
    void onSaveFile();
    void onSaveAs();
    void onExit();

    // Edit menu actions
    void onUndo();
    void onRedo();
    void onCut();
    void onCopy();
    void onPaste();
    void onFind();

    // View menu actions
    void onToggleSuggestions();
    void onToggleSecurity();
    void onToggleOptimizations();
    void onToggleFileExplorer();

    // Tools menu actions
    void onAnalyzeCodebase();
    void onGenerateTests();
    void onSecurityScan();
    void onOptimizeCode();
    void onSwitchModel();
    void onCloudSettings();

    // Model Router actions
    void onOpenModelRouter();
    void onShowModelDashboard();
    void onOpenModelConsole();
    void onSwitchCloudProvider();
    void onConfigureApiKeys();
    void onMonitorModelCost();

    // Help menu actions
    void onAbout();
    void onDocumentation();

    // Code editor events
    void onCodeChanged();
    void onCursorPositionChanged();

    // Autonomous system slots
    void onSuggestionGenerated(const AutonomousSuggestion& suggestion);
    void onSecurityIssueDetected(const SecurityIssue& issue);
    void onOptimizationFound(const PerformanceOptimization& optimization);
    void onTestGenerated(const GeneratedTest& test);

    // Error recovery slots
    void onErrorRecorded(const ErrorRecord& error);
    void onErrorRecovered(const std::string& errorId, bool success);
    void onSystemHealthUpdated(const SystemHealth& health);

    // Performance monitoring slots
    void onMetricRecorded(const MetricData& metric);
    void onThresholdViolation(const MetricData& metric, const std::string& severity);
    void onSnapshotCaptured(const PerformanceSnapshot& snapshot);

    // Model management slots
    void onModelDownloadProgress(const std::string& modelId, int percentage, qint64 speed, qint64 eta);
    void onModelDownloadCompleted(const std::string& modelId, bool success);
    void onModelLoaded(const std::string& modelId);

    // Cloud execution slots
    void onHealthCheckCompleted();

    // Model Router widget signals
    void onModelRouterGenerationRequested(const std::string& prompt, const std::string& model);
    void onModelRouterStatusUpdated(const std::string& status);
    void onModelRouterErrorOccurred(const std::string& error);
    void onModelRouterDashboardRequested();
    void onModelRouterConsoleRequested();
    void onModelRouterApiKeyEditRequested();

    // Real-time code analysis
    void analyzeCurrentCode();

private:
    void setupUI();
    void setupMenus();
    void setupToolbars();
    void setupDockWidgets();
    void setupStatusBar();
    void setupConnections();
    void loadSettings();
    void saveSettings();

    std::string getCurrentLanguage() const;
    std::string getCurrentFilePath() const;
    void updateStatusBar();
    void showMessage(const std::string& message, int timeout = 3000);

    // Core systems
    AutonomousModelManager* modelManager;
    IntelligentCodebaseEngine* codebaseEngine;
    AutonomousFeatureEngine* featureEngine;
    HybridCloudManager* cloudManager;
    ErrorRecoverySystem* errorRecovery;
    PerformanceMonitor* performanceMonitor;

    // Model Router systems
    ModelRouterAdapter* modelRouterAdapter;
    ModelRouterWidget* modelRouterWidget;
    QDockWidget* modelRouterDock;
    CloudSettingsDialog* cloudSettingsDialog;
    MetricsDashboard* metricsDashboard;
    QDockWidget* metricsDashboardDock;
    ModelRouterConsole* modelRouterConsole;
    QDockWidget* modelRouterConsoleDock;

    // UI components
    QPlainTextEdit* codeEditor;
    QTabWidget* editorTabs;
    
    // Dock widgets
    QDockWidget* suggestionsDock;
    QDockWidget* securityDock;
    QDockWidget* optimizationDock;
    QDockWidget* fileExplorerDock;
    QDockWidget* outputDock;
    QDockWidget* metricsDock;

    // Custom widgets
    AutonomousSuggestionWidget* suggestionsWidget;
    SecurityAlertWidget* securityWidget;
    OptimizationPanelWidget* optimizationWidget;
    QTreeWidget* fileExplorerWidget;
    QTextEdit* outputWidget;
    QListWidget* metricsWidget;

    // Status bar widgets
    QLabel* statusLabel;
    QLabel* languageLabel;
    QLabel* modelLabel;
    QLabel* healthLabel;
    QProgressBar* progressBar;

    // Menus
    QMenu* fileMenu;
    QMenu* editMenu;
    QMenu* viewMenu;
    QMenu* toolsMenu;
    QMenu* helpMenu;

    // Toolbars
    QToolBar* mainToolbar;
    QToolBar* aiToolbar;

    // State
    std::string currentFilePath;
    std::string currentLanguage;
    std::string activeModelId;
    bool isAnalyzing;
    void** analysisTimer;
};

#endif // IDE_MAIN_WINDOW_H

