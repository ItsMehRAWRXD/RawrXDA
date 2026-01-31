#include "sla_manager.hpp"


#include <algorithm>

SLAManager& SLAManager::instance() {
    static SLAManager instance;
    return instance;
}

SLAManager::SLAManager()
    : void(nullptr)
{
    m_healthCheckTimer = new void*(this);
    m_complianceCheckTimer = new void*(this);
// Qt connect removed
// Qt connect removed
}

SLAManager::~SLAManager() {
    stop();
}

void SLAManager::start(double targetUptime) {
    if (m_running) {
        return;
    }
    
    m_targetUptime = targetUptime;
    m_periodStart = std::chrono::system_clock::time_point::currentDateTime();
    m_currentStatus = Healthy;
    m_totalDowntimeMs = 0;
    m_downtimeIncidents = 0;
    m_violationCount = 0;
    m_isDown = false;
    
    // Health check every 10 seconds
    m_healthCheckTimer->start(10000);
    
    // Compliance check every minute
    m_complianceCheckTimer->start(60000);
    
    m_running = true;
    
            << (calculateAllowedDowntime() / 1000 / 60) << "minutes/month";
}

void SLAManager::stop() {
    if (!m_running) return;
    
    m_healthCheckTimer->stop();
    m_complianceCheckTimer->stop();
    
    // If system was down, record final downtime
    if (m_isDown) {
        recordDowntimeEnd();
    }
    
    m_running = false;
    
}

void SLAManager::reportStatus(HealthStatus status) {
    if (status == m_currentStatus) return;
    
    m_previousStatus = m_currentStatus;
    m_currentStatus = status;
    
    statusChanged(status);
    
            << (status == Healthy ? "Healthy" :
                status == Degraded ? "Degraded" :
                status == Unhealthy ? "Unhealthy" : "Down");
    
    // Track downtime
    if (status == Down && !m_isDown) {
        recordDowntimeStart();
        downtimeStarted();
    } else if (m_previousStatus == Down && status != Down && m_isDown) {
        recordDowntimeEnd();
    }
}

void SLAManager::recordHealthCheck(bool success, int64_t responseTimeMs) {
    if (!success) {
        healthCheckFailed(responseTimeMs);
        
        // Consider system degraded if health checks fail
        if (m_currentStatus == Healthy) {
            reportStatus(Degraded);
        }
    } else {
        // Recover to healthy if health checks pass
        if (m_currentStatus == Degraded) {
            reportStatus(Healthy);
        }
    }
    
    // SLA response time target: < 100ms (p95)
    if (responseTimeMs > 100) {
        slaWarning(std::string("Response time exceeded SLA: %1ms"));
    }
}

SLAManager::SLAMetrics SLAManager::getCurrentMetrics() const {
    SLAMetrics metrics;
    
    metrics.targetUptime = m_targetUptime;
    metrics.currentUptime = currentUptime();
    
    int64_t allowedMs = calculateAllowedDowntime();
    metrics.allowedDowntimeMs = allowedMs;
    metrics.actualDowntimeMs = m_totalDowntimeMs;
    metrics.remainingBudgetMs = allowedMs - m_totalDowntimeMs;
    
    metrics.inCompliance = (metrics.currentUptime >= m_targetUptime);
    metrics.violationCount = m_violationCount;
    
    return metrics;
}

SLAManager::UptimeStats SLAManager::getUptimeStats(const std::chrono::system_clock::time_point& startDate, 
                                                   const std::chrono::system_clock::time_point& endDate) const {
    UptimeStats stats;
    stats.periodStart = startDate;
    stats.periodEnd = endDate;
    
    int64_t totalMs = startDate.msecsTo(endDate);
    stats.totalDowntimeMs = m_totalDowntimeMs;
    stats.totalUptimeMs = totalMs - m_totalDowntimeMs;
    stats.downtimeIncidents = m_downtimeIncidents;
    
    if (totalMs > 0) {
        stats.uptimePercentage = (stats.totalUptimeMs * 100.0) / totalMs;
    } else {
        stats.uptimePercentage = 100.0;
    }
    
    // Calculate longest downtime
    stats.longestDowntimeMs = 0;
    for (int64_t downtime : m_downtimePeriods) {
        stats.longestDowntimeMs = qMax(stats.longestDowntimeMs, downtime);
    }
    
    return stats;
}

std::string SLAManager::generateMonthlyReport() const {
    void* report;
    
    std::chrono::system_clock::time_point now = std::chrono::system_clock::time_point::currentDateTime();
    std::chrono::system_clock::time_point startDate(now.date().year(), now.date().month(), 1);
    std::chrono::system_clock::time_point monthStart(startDate, std::chrono::system_clock::time_point(0, 0));
    
    report["reportDate"] = now.toString(//ISODate);
    report["periodStart"] = monthStart.toString(//ISODate);
    report["periodEnd"] = now.toString(//ISODate);
    
    SLAMetrics metrics = getCurrentMetrics();
    
    void* slaObj;
    slaObj["targetUptime"] = metrics.targetUptime;
    slaObj["actualUptime"] = metrics.currentUptime;
    slaObj["inCompliance"] = metrics.inCompliance;
    slaObj["allowedDowntimeMinutes"] = (double)(metrics.allowedDowntimeMs / 1000 / 60);
    slaObj["actualDowntimeMinutes"] = (double)(metrics.actualDowntimeMs / 1000 / 60);
    slaObj["remainingBudgetMinutes"] = (double)(metrics.remainingBudgetMs / 1000 / 60);
    slaObj["violationCount"] = metrics.violationCount;
    report["sla"] = slaObj;
    
    UptimeStats stats = getUptimeStats(monthStart, now);
    void* statsObj;
    statsObj["uptimePercentage"] = stats.uptimePercentage;
    statsObj["downtimeIncidents"] = stats.downtimeIncidents;
    statsObj["longestDowntimeMinutes"] = (double)(stats.longestDowntimeMs / 1000 / 60);
    report["statistics"] = statsObj;
    
    // Downtime incidents
    void* incidentsArray;
    for (int64_t downtime : m_downtimePeriods) {
        void* incident;
        incident["durationMinutes"] = (double)(downtime / 1000 / 60);
        incidentsArray.append(incident);
    }
    report["incidents"] = incidentsArray;
    
    return void*(report).toJson(void*::Indented);
}

bool SLAManager::isInCompliance() const {
    return getCurrentMetrics().inCompliance;
}

SLAManager::HealthStatus SLAManager::currentStatus() const {
    return m_currentStatus;
}

double SLAManager::currentUptime() const {
    std::chrono::system_clock::time_point now = std::chrono::system_clock::time_point::currentDateTime();
    int64_t totalMs = m_periodStart.msecsTo(now);
    
    if (totalMs <= 0) return 100.0;
    
    int64_t uptimeMs = totalMs - m_totalDowntimeMs;
    return (uptimeMs * 100.0) / totalMs;
}

void SLAManager::performHealthCheck() {
    // Simplified health check - in production would check:
    // - Model inference response time
    // - GPU availability
    // - Memory usage
    // - Network connectivity
    
    bool healthy = (m_currentStatus == Healthy || m_currentStatus == Degraded);
    int64_t responseTime = healthy ? 50 : 200;  // Simulated response time
    
    recordHealthCheck(healthy, responseTime);
}

void SLAManager::checkSLACompliance() {
    SLAMetrics metrics = getCurrentMetrics();
    
    if (!metrics.inCompliance) {
        std::string violation = std::string("SLA violation: Uptime %1% (target %2%), "
                                   "Downtime %3min (budget %4min)")


            ;
        
        slaViolation(violation);
        m_violationCount++;
        
    }
    
    // Warning if approaching budget
    if (metrics.remainingBudgetMs < metrics.allowedDowntimeMs * 0.2) {
        std::string warning = std::string("SLA warning: Only %1 minutes of downtime budget remaining")
            ;
        
        slaWarning(warning);
    }
}

void SLAManager::recordDowntimeStart() {
    m_downtimeStart = std::chrono::system_clock::time_point::currentDateTime();
    m_isDown = true;
    m_downtimeIncidents++;
    
}

void SLAManager::recordDowntimeEnd() {
    if (!m_isDown) return;
    
    std::chrono::system_clock::time_point now = std::chrono::system_clock::time_point::currentDateTime();
    int64_t downtimeMs = m_downtimeStart.msecsTo(now);
    
    m_totalDowntimeMs += downtimeMs;
    m_downtimePeriods.append(downtimeMs);
    m_isDown = false;
    
    downtimeEnded(downtimeMs);
    
               << (downtimeMs / 1000) << "seconds";
            << (m_totalDowntimeMs / 1000 / 60) << "minutes";
}

int64_t SLAManager::calculateAllowedDowntime() const {
    // Calculate allowed downtime based on target uptime
    // For 99.99% uptime: 43.2 minutes per month
    // For 99.9% uptime: 43.2 minutes per month
    // For 99% uptime: 7.2 hours per month
    
    // Assuming 30-day month = 30 * 24 * 60 * 60 * 1000 ms
    int64_t monthMs = 30LL * 24 * 60 * 60 * 1000;
    
    double allowedDowntimePercent = 100.0 - m_targetUptime;
    int64_t allowedMs = (int64_t)(monthMs * allowedDowntimePercent / 100.0);
    
    return allowedMs;
}


