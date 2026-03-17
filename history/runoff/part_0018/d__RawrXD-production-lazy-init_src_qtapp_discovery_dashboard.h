#pragma once

#include <QDockWidget>
#include <QScrollArea>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QTabWidget>
#include <QListWidget>
#include <QListWidgetItem>
#include <QGroupBox>
#include <QFrame>

/**
 * @class DiscoveryDashboard
 * @brief Dockable dashboard for autonomous capabilities and performance monitoring
 * 
 * Features:
 * - Real-time capability display with status indicators
 * - Performance metrics visualization
 * - Execution progress tracking
 * - System health monitoring
 * - Adaptive learning metrics
 * - Agent activity visualization
 * 
 * This dashboard integrates as a dockable pane in the IDE.
 */
class DiscoveryDashboard : public QDockWidget {
    Q_OBJECT
public:
    explicit DiscoveryDashboard(QWidget* parent = nullptr);
    virtual ~DiscoveryDashboard();

    // Dashboard control
    void initialize(); // Added to fix link errors in IDEMainWindow
    void refresh();    // Added to fix link errors in IDEMainWindow
    void refreshAll();
    void updateCapabilityStatus(const QString& capability, bool active, const QString& status = QString());
    void updatePerformanceMetrics(const QJsonObject& metrics);
    void addExecutionEvent(const QString& eventType, const QString& description);
    
    // Dashboard configuration
    void setRefreshInterval(int milliseconds);
    void enableAutoRefresh(bool enable);
    void setCapabilitiesToMonitor(const QStringList& capabilities);

public slots:
    void onCapabilityClicked(const QString& capability);
    void onRefreshRequested();
    void onAutoRefresh();
    void onExportMetrics();

signals:
    void capabilitySelected(const QString& capability);
    void systemHealthChanged(double score); // Added to fix link error
    void errorOccurred(const QString& error); // Added to fix link error
    void dashboardRefreshed();
    void metricsExported(const QString& filePath);

private:
    void setupUI();
    void setupCapabilityGrid();
    void setupMetricsPanel();
    void setupActivityFeed();
    void setupSystemHealth();
    void updateAllDisplays();
    void updateCapabilityCards();
    void updateMetricsDisplay();
    void updateActivityDisplay();
    void updateSystemHealthDisplay();
    
    // UI Components
    QTabWidget* m_mainTabs;
    QWidget* m_capabilitiesTab;
    QWidget* m_metricsTab;
    QWidget* m_activityTab;
    QWidget* m_systemTab;
    
    QGridLayout* m_capabilityGrid;
    QMap<QString, QFrame*> m_capabilityCards;
    
    QVBoxLayout* m_metricsLayout;
    QProgressBar* m_overallHealthBar;
    QLabel* m_activeTasksLabel;
    QLabel* m_completedTasksLabel;
    QLabel* m_successRateLabel;
    
    QListWidget* m_activityList;
    QTimer* m_refreshTimer;
    QPushButton* m_refreshButton;
    QPushButton* m_exportButton;
    
    // Data
    QMap<QString, bool> m_capabilityStatus;
    QJsonObject m_performanceMetrics;
    QJsonArray m_activityEvents;
    QStringList m_monitoredCapabilities;
    
    // Styles
    QString m_cardStyle;
    QString m_activeStyle;
    QString m_inactiveStyle;
    QString m_warningStyle;
    
    // Helper methods
    QFrame* createCapabilityCard(const QString& name, const QString& description);
    void updateCapabilityCard(const QString& name, bool active, const QString& status);
    void addActivityItem(const QString& type, const QString& message, const QString& timestamp);
};
