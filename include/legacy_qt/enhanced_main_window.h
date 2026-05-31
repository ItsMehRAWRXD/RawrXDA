#pragma once
#include <QMainWindow>
#include <QDockWidget>
#include <memory>

class FeaturesViewMenu;
class QAction;
class QMenu;

/**
 * @class EnhancedMainWindow
 * @brief Production-ready main window with integrated features panel and advanced menus
 * 
 * Features:
 * - Hierarchical features view with toggable sub-menus
 * - Dynamic menu system with context-aware actions
 * - Persistent window state and layout
 * - Advanced toolbar with feature shortcuts
 * - Real-time feature discovery and management
 * - Performance monitoring dashboard
 */
class EnhancedMainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit EnhancedMainWindow(QWidget *parent = nullptr);
    ~EnhancedMainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    // Features panel slots
    void onFeatureToggled(const QString &featureId, bool enabled);
    void onFeatureClicked(const QString &featureId);
    void onFeatureDoubleClicked(const QString &featureId);
    void onCategoryToggled(int category, bool visible);
    
    // Menu actions
    void onViewFeatures();
    void onViewPerformance();
    void onViewMetrics();
    void onToggleAdvancedMode();
    void onManagePlugins();
    void onOpenSettings();
    void onShowAbout();

private:
    void setupUI();
    void createMenuBar();
    void createToolBars();
    void createDockWidgets();
    void createStatusBar();
    void loadWindowState();
    void saveWindowState();
    void registerBuiltInFeatures();
    void connectSignals();

    // Dock widgets
    FeaturesViewMenu *m_featuresPanel;
    class PerformanceMonitor *m_performanceMonitor;
    class PluginRegistry *m_pluginRegistry;

    // Menus and actions
    QMenu *m_viewMenu;
    QMenu *m_toolsMenu;
    QMenu *m_advancedMenu;
    QMenu *m_helpMenu;

    // Status
    class QLabel *m_statusLabel;
    class QLabel *m_metricsLabel;
    bool m_advancedModeEnabled;
};
