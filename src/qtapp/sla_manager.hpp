#pragma once


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
class SLAManager : public void {

public:
    enum HealthStatus {
        Healthy,
        Degraded,
        Unhealthy,
        Down
    };

    struct UptimeStats {
        std::chrono::system_clock::time_point periodStart;
        std::chrono::system_clock::time_point periodEnd;
        int64_t totalUptimeMs;
        int64_t totalDowntimeMs;
        double uptimePercentage;
        int downtimeIncidents;
        int64_t longestDowntimeMs;
    };

    struct SLAMetrics {
        double currentUptime;        // Current uptime %
        double targetUptime;         // Target (99.99%)
        int64_t allowedDowntimeMs;    // Monthly budget (43 min)
        int64_t actualDowntimeMs;     // Actual downtime
        int64_t remainingBudgetMs;    // Remaining budget
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
    void recordHealthCheck(bool success, int64_t responseTimeMs);

    /**
     * @brief Get current SLA metrics
     */
    SLAMetrics getCurrentMetrics() const;

    /**
     * @brief Get uptime statistics for period
     */
    UptimeStats getUptimeStats(const std::chrono::system_clock::time_point& startDate, const std::chrono::system_clock::time_point& endDate) const;

    /**
     * @brief Generate monthly SLA report
     */
    std::string generateMonthlyReport() const;

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

    void statusChanged(HealthStatus status);
    void slaViolation(const std::string& details);
    void slaWarning(const std::string& message);
    void healthCheckFailed(int64_t responseTimeMs);
    void downtimeStarted();
    void downtimeEnded(int64_t durationMs);

private:
    void performHealthCheck();
    void checkSLACompliance();

private:
    SLAManager();  // Singleton
    SLAManager(const SLAManager&) = delete;
    SLAManager& operator=(const SLAManager&) = delete;

    void recordDowntimeStart();
    void recordDowntimeEnd();
    int64_t calculateAllowedDowntime() const;

    HealthStatus m_currentStatus = Healthy;
    HealthStatus m_previousStatus = Healthy;
    
    std::chrono::system_clock::time_point m_periodStart;
    std::chrono::system_clock::time_point m_downtimeStart;
    int64_t m_totalDowntimeMs = 0;
    int m_downtimeIncidents = 0;
    int m_violationCount = 0;
    
    double m_targetUptime = 99.99;
    
    void** m_healthCheckTimer = nullptr;
    void** m_complianceCheckTimer = nullptr;
    
    bool m_running = false;
    bool m_isDown = false;
    
    std::vector<int64_t> m_downtimePeriods;
};


