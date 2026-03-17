// ide_main_window.h - Main IDE Window with Full Integration
#ifndef IDE_MAIN_WINDOW_H
#define IDE_MAIN_WINDOW_H

#include <QMainWindow>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QDockWidget>
#include <QListWidget>
#include <QTreeWidget>
#include <QStatusBar>
#include <QMenuBar>
#include <QToolBar>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QComboBox>
#include <QSplitter>
#include <QTabWidget>
#include <QJsonArray>
#include <QJsonObject>
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
#include "compiler_panel.h"

class AutonomousSuggestionWidget;
class SecurityAlertWidget;
class OptimizationPanelWidget;

class IDEMainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit IDEMainWindow(QWidget *parent = nullptr);
    ~IDEMainWindow();

private slots:
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
    void onToggleCompilerPanel();

    // Tools menu actions
    void onAnalyzeCodebase();
    void onGenerateTests();
    void onSecurityScan();
    void onOptimizeCode();
    void onSwitchModel();
    void onCloudSettings();
    void onCompileCurrentFile();
    void onCompileSelectedCode();

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
    void onErrorRecovered(const QString& errorId, bool success);
    void onSystemHealthUpdated(const SystemHealth& health);

    // Performance monitoring slots
    void onMetricRecorded(const MetricData& metric);
    void onThresholdViolation(const MetricData& metric, const QString& severity);
    void onSnapshotCaptured(const PerformanceSnapshot& snapshot);

    // Model management slots
    void onModelDownloadProgress(double progress, const QString& message);
    void onModelDownloadCompleted(const QString& modelId, bool success);
    void onModelLoaded(const QString& modelId);

    // Cloud execution slots
    void onHealthCheckCompleted();

    // Model Router widget signals
    void onModelRouterGenerationRequested(const QString& prompt, const QString& model);
    void onModelRouterStatusUpdated(const QString& status);
    void onModelRouterErrorOccurred(const QString& error);
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

    QString getCurrentLanguage() const;
    QString getCurrentFilePath() const;
    void updateStatusBar();
    void showMessage(const QString& message, int timeout = 3000);

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
    QDockWidget* compilerDock;

    // Custom widgets
    AutonomousSuggestionWidget* suggestionsWidget;
    SecurityAlertWidget* securityWidget;
    OptimizationPanelWidget* optimizationWidget;
    QTreeWidget* fileExplorerWidget;
    QTextEdit* outputWidget;
    QListWidget* metricsWidget;
    CompilerPanel* compilerPanel;

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
    QString currentFilePath;
    QString currentLanguage;
    QString activeModelId;
    bool isAnalyzing;
    QTimer* analysisTimer;
};

#endif // IDE_MAIN_WINDOW_H
