#pragma once
#include <QString>
#include <QDateTime>
#include <QHash>
#include <vector>
#include <cstdint>

/**
 * @class SLAManager
 * @brief Service Level Agreement (SLA) enforcement and tracking
 * 
 * Features:
 * - Uptime guarantees (99.9%, 99.95%, 99.99%)
 * - Performance SLAs (latency, throughput)
 * - Availability targets
 * - SLA breach tracking and notifications
 * - Support response time SLAs
 */
class SLAManager {
    
public:
    enum SLATier {
        BASIC,      // 99.0% uptime
        STANDARD,   // 99.5% uptime
        PREMIUM,    // 99.9% uptime
        ENTERPRISE  // 99.99% uptime
    };
    
    struct SLATarget {
        QString name;
        QString description;
        double targetValue;
        QString unit;
        SLATier tier;
        QDateTime effectiveDate;
    };
    
    struct SLAMetric {
        QString metricName;
        double currentValue;
        double targetValue;
        double compliance = 1.0; // 0-1
        QDateTime measurementTime;
        bool meetsTarget = true;
    };
    
    struct SLABreach {
        QString serviceName;
        QString metric;
        double breachedValue;
        double targetValue;
        QDateTime breachTime;
        qint64 durationMs = 0;
        QString impactDescription;
        QString remediationAction;
    };
    
    struct IncidentResponse {
        QString incidentId;
        QDateTime reportedTime;
        QDateTime acknowledgedTime;
        QDateTime resolvedTime;
        QString severity; // P1, P2, P3, P4
        QString description;
        int assignedPersonnel = 0;
    };
    
    static SLAManager& instance();
    
    // Configuration
    void setSLATier(SLATier tier);
    SLATier getCurrentTier() const;
    
    /**
     * @brief Define custom SLA target
     */
    void defineSLATarget(const SLATarget& target);
    
    // Monitoring
    void recordMetric(const QString& metricName, double value);
    SLAMetric getMetricStatus(const QString& metricName) const;
    QVector<SLAMetric> getAllMetrics() const;
    
    // Uptime tracking
    /**
     * @brief Get current uptime percentage (rolling 30 days)
     */
    double getCurrentUptime() const;
    
    /**
     * @brief Get uptime for specific period
     */
    double getUptimeForPeriod(const QDateTime& start, const QDateTime& end) const;
    
    /**
     * @brief Record downtime incident
     */
    void recordDowntime(const QDateTime& startTime, const QDateTime& endTime, 
                       const QString& reason);
    
    // Breach management
    void recordBreach(const SLABreach& breach);
    QVector<SLABreach> getRecentBreaches(int count = 10) const;
    
    /**
     * @brief Calculate SLA credit for breaches
     * @return credit as percentage (0-100)
     */
    double calculateSLACredit() const;
    
    // Incident management
    /**
     * @brief Log incident with response tracking
     */
    QString createIncident(const QString& description, const QString& severity);
    
    void acknowledgeIncident(const QString& incidentId);
    void resolveIncident(const QString& incidentId);
    
    IncidentResponse getIncident(const QString& incidentId) const;
    QVector<IncidentResponse> getOpenIncidents() const;
    
    /**
     * @brief Check if mean time to acknowledge (MTTA) is met
     */
    bool isMTTAMet(const QString& incidentId) const;
    
    /**
     * @brief Check if mean time to resolution (MTTR) is met
     */
    bool isMTTRMet(const QString& incidentId) const;
    
    // Performance SLAs
    /**
     * @brief Set latency SLA target (milliseconds)
     */
    void setLatencySLA(double p99_ms, double p95_ms = -1);
    
    /**
     * @brief Set throughput SLA target (tokens/sec)
     */
    void setThroughputSLA(double min_tokens_per_sec);
    
    /**
     * @brief Set availability SLA target (%)
     */
    void setAvailabilitySLA(double percent);
    
    // Reporting
    /**
     * @brief Generate SLA compliance report
     */
    QString generateSLAReport(const QDateTime& startDate, const QDateTime& endDate) const;
    
    /**
     * @brief Get SLA compliance percentage
     */
    double getSLACompliance() const;
    
    /**
     * @brief Calculate estimated downtime allowance for period
     * @param daysInMonth Days in current/target month
     * @return maximum allowed downtime in milliseconds
     */
    qint64 calculateAllowedDowntime(int daysInMonth) const;
    
    // Alert management
    void enableSLAAlerts(bool enable);
    void setSLAAlertThreshold(double percent); // e.g., 95% triggers alert
    
    // Support response times
    void setSupportResponseSLA(const QString& severity, int responseTimeMinutes);
    int getSupportResponseSLA(const QString& severity) const;
    
private:
    SLAManager();
    ~SLAManager() = default;
    
    SLATier m_current_tier = PREMIUM;
    QHash<QString, SLATarget> m_sla_targets;
    std::vector<SLAMetric> m_metrics;
    std::vector<SLABreach> m_breaches;
    std::vector<IncidentResponse> m_incidents;
    
    struct DowntimeRecord {
        QDateTime startTime;
        QDateTime endTime;
        QString reason;
    };
    
    std::vector<DowntimeRecord> m_downtime_records;
    
    double m_latency_p99_ms = 100.0;
    double m_latency_p95_ms = 50.0;
    double m_throughput_min_tokens_sec = 20.0;
    double m_availability_target = 0.999;
    
    QHash<QString, int> m_support_response_sla; // severity -> minutes
    
    bool m_sla_alerts_enabled = true;
    double m_alert_threshold = 0.95;
    
    qint64 m_service_start_time = 0;
    
    void checkSLACompliance();
};
