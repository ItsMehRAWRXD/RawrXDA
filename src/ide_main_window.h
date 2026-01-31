// ide_main_window.h - Main IDE Window (Pure C++ / Qt-Free)
#ifndef IDE_MAIN_WINDOW_H
#define IDE_MAIN_WINDOW_H

#include <string>
#include <memory>
#include <vector>

class AutonomousModelManager;
class IntelligentCodebaseEngine;
class AutonomousFeatureEngine;
class HybridCloudManager;
class ErrorRecoverySystem;
class PerformanceMonitor;
class ModelRouterAdapter;

class IDEMainWindow {
public:
    explicit IDEMainWindow();
    ~IDEMainWindow();

    void setupUI();

private:
    // Core systems
    std::unique_ptr<AutonomousModelManager> modelManager;
    std::unique_ptr<IntelligentCodebaseEngine> codebaseEngine;
    std::unique_ptr<HybridCloudManager> cloudManager;
    std::unique_ptr<AutonomousFeatureEngine> featureEngine;
    std::unique_ptr<ErrorRecoverySystem> errorRecovery;
    std::unique_ptr<PerformanceMonitor> performanceMonitor;
    std::unique_ptr<ModelRouterAdapter> modelRouterAdapter;

    std::string currentLanguage;
    bool isAnalyzing;
};
#endif
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
    void onModelDownloadProgress(const std::string& modelId, int percentage, int64_t speed, int64_t eta);
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
    void* modelRouterDock;
    CloudSettingsDialog* cloudSettingsDialog;
    MetricsDashboard* metricsDashboard;
    void* metricsDashboardDock;
    ModelRouterConsole* modelRouterConsole;
    void* modelRouterConsoleDock;

    // UI components
    QPlainTextEdit* codeEditor;
    void* editorTabs;
    
    // Dock widgets
    void* suggestionsDock;
    void* securityDock;
    void* optimizationDock;
    void* fileExplorerDock;
    void* outputDock;
    void* metricsDock;

    // Custom widgets
    AutonomousSuggestionWidget* suggestionsWidget;
    SecurityAlertWidget* securityWidget;
    OptimizationPanelWidget* optimizationWidget;
    QTreeWidget* fileExplorerWidget;
    void* outputWidget;
    QListWidget* metricsWidget;

    // Status bar widgets
    void* statusLabel;
    void* languageLabel;
    void* modelLabel;
    void* healthLabel;
    void* progressBar;

    // Menus
    void* fileMenu;
    void* editMenu;
    void* viewMenu;
    void* toolsMenu;
    void* helpMenu;

    // Toolbars
    void* mainToolbar;
    void* aiToolbar;

    // State
    std::string currentFilePath;
    std::string currentLanguage;
    std::string activeModelId;
    bool isAnalyzing;
    void** analysisTimer;
};

#endif // IDE_MAIN_WINDOW_H


