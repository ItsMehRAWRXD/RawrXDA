#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QWidget>
#include <QTimer>
#include <QMap>
#include <QVector>

/**
 * @class AutonomousRealtimeFeedbackSystem
 * @brief Real-time feedback and visualization for autonomous agents
 * 
 * Provides UI components and visualization:
 * - Real-time execution progress updates
 * - Live metrics visualization
 * - Status indicators and health monitors
 * - Interactive agent controls
 * - Results display and analytics
 */
class AutonomousRealtimeFeedbackSystem : public QObject {
    Q_OBJECT

public:
    explicit AutonomousRealtimeFeedbackSystem(QObject* parent = nullptr);
    ~AutonomousRealtimeFeedbackSystem();

    // Real-time updates
    void updateExecutionProgress(const QString& taskId, int current, int total);
    void updateAgentStatus(const QString& agentId, const QString& status, const QJsonObject& details);
    void updateMetrics(const QString& metricName, double value);
    void displayTaskResult(const QString& taskId, const QJsonObject& result);
    
    // Live visualization
    QJsonObject getExecutionDashboard() const;
    QJsonObject getPerformanceDashboard() const;
    QJsonObject getHealthDashboard() const;
    QJsonObject getAgentStatus(const QString& agentId) const;
    
    // Streaming updates
    void startLiveUpdates(int intervalMs = 500);
    void stopLiveUpdates();
    bool isLiveUpdating() const;
    
    // Alert display
    void displayAlert(const QString& severity, const QString& message, const QJsonObject& details);
    void displayNotification(const QString& message, const QString& type = "info");
    
    // Logging view
    void addLogEntry(const QString& level, const QString& component, const QString& message);
    QJsonArray getRecentLogs(int count = 50) const;
    
    // Analytics and reporting
    QJsonObject generateExecutionSummary() const;
    QJsonArray getTaskHistory() const;
    QString generateVisualizationData() const;
    
    // Configuration
    void setUpdateFrequency(int frequencyMs);
    void enableAutoScroll(bool enable);
    void setMaxDisplayItems(int max);
    void setTheme(const QString& theme);  // dark, light

signals:
    void progressUpdated(const QString& taskId, int current, int total);
    void statusChanged(const QString& agentId, const QString& newStatus);
    void metricsUpdated(const QString& metricName, double value);
    void resultDisplayed(const QString& taskId, const QJsonObject& result);
    void alertDisplayed(const QString& severity, const QString& message);
    void notificationDisplayed(const QString& message);
    void dashboardRefreshed(const QString& dashboardType);
    void liveUpdatesStarted();
    void liveUpdatesStopped();

private:
    // Update structures
    struct TaskProgress {
        QString taskId;
        int currentStep = 0;
        int totalSteps = 0;
        QString currentAction;
        QDateTime startTime;
        QDateTime lastUpdateTime;
        QString status;
    };
    
    struct AgentStatusInfo {
        QString agentId;
        QString currentStatus;
        QJsonObject details;
        QDateTime lastUpdated;
        double cpuUsage = 0.0;
        double memoryUsage = 0.0;
    };
    
    struct LogEntry {
        QDateTime timestamp;
        QString level;
        QString component;
        QString message;
        QString displayColor;
    };
    
    // Internal update methods
    void onLiveUpdateTimer();
    void updateDashboard();
    void refreshMetricsDisplay();
    void refreshAgentStatusDisplay();
    
    // Visualization helpers
    QString formatMetricValue(const QString& metricName, double value);
    QString getStatusColor(const QString& status);
    QJsonObject buildExecutionGraph() const;
    QJsonObject buildPerformanceMetrics() const;
    
    // Members
    QMap<QString, TaskProgress> m_taskProgress;
    QMap<QString, AgentStatusInfo> m_agentStatus;
    QVector<LogEntry> m_logHistory;
    
    QTimer m_updateTimer;
    int m_updateFrequencyMs = 500;
    bool m_autoScroll = true;
    int m_maxDisplayItems = 1000;
    QString m_currentTheme = "dark";
    
    // Cached data
    QJsonObject m_cachedDashboard;
    QDateTime m_lastDashboardUpdate;
    
    QMap<QString, QVector<double>> m_metricHistory;
};
