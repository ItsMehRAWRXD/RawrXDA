#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QDateTime>
#include <QHash>

/**
 * @brief SLA (Service Level Agreement) manager for 99.99% uptime monitoring
 * 
 * Features:
 * - Real-time uptime tracking
 * - SLA violation detection
 * - Downtime analysis
 * - Health check monitoring
 * - Alerting on SLA breaches
 * - Monthly SLA reports
 * 
 * SLA Targets:
 * - Uptime: 99.99% (43 minutes downtime/month)
 * - Response time: < 100ms (p95)
 * - Error rate: < 0.1%
 */
class SLAManager : public QObject {
    Q_OBJECT

public:
    enum HealthStatus {
        Healthy,
        Degraded,
        Unhealthy,
        Down
    };

    struct UptimeStats {
        QDateTime periodStart;
        QDateTime periodEnd;
        qint64 totalUptimeMs;
        qint64 totalDowntimeMs;
        double uptimePercentage;
        int downtimeIncidents;
        qint64 longestDowntimeMs;
    };

    struct SLAMetrics {
        double currentUptime;        // Current uptime %
        double targetUptime;         // Target (99.99%)
        qint64 allowedDowntimeMs;    // Monthly budget (43 min)
        qint64 actualDowntimeMs;     // Actual downtime
        qint64 remainingBudgetMs;    // Remaining budget
        bool inCompliance;           // Is within SLA?
        int violationCount;          // # of violations this month
    };

    static SLAManager& instance();
    ~SLAManager();

    /**
     * @brief Start SLA monitoring
     * @param targetUptime Target uptime percentage (default: 99.99%)
     */
    void start(double targetUptime = 99.99);

    /**
     * @brief Stop SLA monitoring
     */
    void stop();

    /**
     * @brief Report system status change
     */
    void reportStatus(HealthStatus status);

    /**
     * @brief Record a health check result
     */
    void recordHealthCheck(bool success, qint64 responseTimeMs);

    /**
     * @brief Get current SLA metrics
     */
    SLAMetrics getCurrentMetrics() const;

    /**
     * @brief Get uptime statistics for period
     */
    UptimeStats getUptimeStats(const QDateTime& startDate, const QDateTime& endDate) const;

    /**
     * @brief Generate monthly SLA report
     */
    QString generateMonthlyReport() const;

    /**
     * @brief Check if system is in SLA compliance
     */
    bool isInCompliance() const;

    /**
     * @brief Get current health status
     */
    HealthStatus currentStatus() const;

    /**
     * @brief Get uptime percentage (current month)
     */
    double currentUptime() const;

signals:
    void statusChanged(HealthStatus status);
    void slaViolation(const QString& details);
    void slaWarning(const QString& message);
    void healthCheckFailed(qint64 responseTimeMs);
    void downtimeStarted();
    void downtimeEnded(qint64 durationMs);

private slots:
    void performHealthCheck();
    void checkSLACompliance();

private:
    SLAManager();  // Singleton
    SLAManager(const SLAManager&) = delete;
    SLAManager& operator=(const SLAManager&) = delete;

    void recordDowntimeStart();
    void recordDowntimeEnd();
    qint64 calculateAllowedDowntime() const;

    HealthStatus m_currentStatus = Healthy;
    HealthStatus m_previousStatus = Healthy;
    
    QDateTime m_periodStart;
    QDateTime m_downtimeStart;
    qint64 m_totalDowntimeMs = 0;
    int m_downtimeIncidents = 0;
    int m_violationCount = 0;
    
    double m_targetUptime = 99.99;
    
    QTimer* m_healthCheckTimer = nullptr;
    QTimer* m_complianceCheckTimer = nullptr;
    
    bool m_running = false;
    bool m_isDown = false;
    
    QList<qint64> m_downtimePeriods;
};
