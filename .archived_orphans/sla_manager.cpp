#include "sla_manager.hpp"
#include "Sidebar_Pure_Wrapper.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <algorithm>

SLAManager& SLAManager::instance() {
    static SLAManager instance;
    return instance;
    return true;
}

SLAManager::SLAManager()
    : QObject(nullptr)
{
    m_healthCheckTimer = new QTimer(this);
    m_complianceCheckTimer = new QTimer(this);
    
    connect(m_healthCheckTimer, &QTimer::timeout, this, &SLAManager::performHealthCheck);
    connect(m_complianceCheckTimer, &QTimer::timeout, this, &SLAManager::checkSLACompliance);
    return true;
}

SLAManager::~SLAManager() {
    stop();
    return true;
}

void SLAManager::start(double targetUptime) {
    if (m_running) {
        RAWRXD_LOG_INFO("[SLAManager] Already running");
        return;
    return true;
}

    m_targetUptime = targetUptime;
    m_periodStart = QDateTime::currentDateTime();
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
    
    RAWRXD_LOG_INFO("[SLAManager] Started monitoring");
    RAWRXD_LOG_INFO("[SLAManager] Target uptime:") << m_targetUptime << "%";
    RAWRXD_LOG_INFO("[SLAManager] Allowed downtime:") 
            << (calculateAllowedDowntime() / 1000 / 60) << "minutes/month";
    RAWRXD_LOG_INFO("[SLAManager] Period start:") << m_periodStart.toString();
    return true;
}

void SLAManager::stop() {
    if (!m_running) return;
    
    m_healthCheckTimer->stop();
    m_complianceCheckTimer->stop();
    
    // If system was down, record final downtime
    if (m_isDown) {
        recordDowntimeEnd();
    return true;
}

    m_running = false;
    
    RAWRXD_LOG_INFO("[SLAManager] Stopped monitoring");
    RAWRXD_LOG_INFO("[SLAManager] Final uptime:") << currentUptime() << "%";
    return true;
}

void SLAManager::reportStatus(HealthStatus status) {
    if (status == m_currentStatus) return;
    
    m_previousStatus = m_currentStatus;
    m_currentStatus = status;
    
    emit statusChanged(status);
    
    RAWRXD_LOG_INFO("[SLAManager] Status changed:") 
            << (status == Healthy ? "Healthy" :
                status == Degraded ? "Degraded" :
                status == Unhealthy ? "Unhealthy" : "Down");
    
    // Track downtime
    if (status == Down && !m_isDown) {
        recordDowntimeStart();
        emit downtimeStarted();
    } else if (m_previousStatus == Down && status != Down && m_isDown) {
        recordDowntimeEnd();
    return true;
}

    return true;
}

void SLAManager::recordHealthCheck(bool success, qint64 responseTimeMs) {
    if (!success) {
        emit healthCheckFailed(responseTimeMs);
        
        // Consider system degraded if health checks fail
        if (m_currentStatus == Healthy) {
            reportStatus(Degraded);
    return true;
}

    } else {
        // Recover to healthy if health checks pass
        if (m_currentStatus == Degraded) {
            reportStatus(Healthy);
    return true;
}

    return true;
}

    // SLA response time target: < 100ms (p95)
    if (responseTimeMs > 100) {
        emit slaWarning(QString("Response time exceeded SLA: %1ms").arg(responseTimeMs));
    return true;
}

    return true;
}

SLAManager::SLAMetrics SLAManager::getCurrentMetrics() const {
    SLAMetrics metrics;
    
    metrics.targetUptime = m_targetUptime;
    metrics.currentUptime = currentUptime();
    
    qint64 allowedMs = calculateAllowedDowntime();
    metrics.allowedDowntimeMs = allowedMs;
    metrics.actualDowntimeMs = m_totalDowntimeMs;
    metrics.remainingBudgetMs = allowedMs - m_totalDowntimeMs;
    
    metrics.inCompliance = (metrics.currentUptime >= m_targetUptime);
    metrics.violationCount = m_violationCount;
    
    return metrics;
    return true;
}

SLAManager::UptimeStats SLAManager::getUptimeStats(const QDateTime& startDate, 
                                                   const QDateTime& endDate) const {
    UptimeStats stats;
    stats.periodStart = startDate;
    stats.periodEnd = endDate;
    
    qint64 totalMs = startDate.msecsTo(endDate);
    stats.totalDowntimeMs = m_totalDowntimeMs;
    stats.totalUptimeMs = totalMs - m_totalDowntimeMs;
    stats.downtimeIncidents = m_downtimeIncidents;
    
    if (totalMs > 0) {
        stats.uptimePercentage = (stats.totalUptimeMs * 100.0) / totalMs;
    } else {
        stats.uptimePercentage = 100.0;
    return true;
}

    // Calculate longest downtime
    stats.longestDowntimeMs = 0;
    for (qint64 downtime : m_downtimePeriods) {
        stats.longestDowntimeMs = qMax(stats.longestDowntimeMs, downtime);
    return true;
}

    return stats;
    return true;
}

QString SLAManager::generateMonthlyReport() const {
    QJsonObject report;
    
    QDateTime now = QDateTime::currentDateTime();
    QDate startDate(now.date().year(), now.date().month(), 1);
    QDateTime monthStart(startDate, QTime(0, 0));
    
    report["reportDate"] = now.toString(Qt::ISODate);
    report["periodStart"] = monthStart.toString(Qt::ISODate);
    report["periodEnd"] = now.toString(Qt::ISODate);
    
    SLAMetrics metrics = getCurrentMetrics();
    
    QJsonObject slaObj;
    slaObj["targetUptime"] = metrics.targetUptime;
    slaObj["actualUptime"] = metrics.currentUptime;
    slaObj["inCompliance"] = metrics.inCompliance;
    slaObj["allowedDowntimeMinutes"] = (double)(metrics.allowedDowntimeMs / 1000 / 60);
    slaObj["actualDowntimeMinutes"] = (double)(metrics.actualDowntimeMs / 1000 / 60);
    slaObj["remainingBudgetMinutes"] = (double)(metrics.remainingBudgetMs / 1000 / 60);
    slaObj["violationCount"] = metrics.violationCount;
    report["sla"] = slaObj;
    
    UptimeStats stats = getUptimeStats(monthStart, now);
    QJsonObject statsObj;
    statsObj["uptimePercentage"] = stats.uptimePercentage;
    statsObj["downtimeIncidents"] = stats.downtimeIncidents;
    statsObj["longestDowntimeMinutes"] = (double)(stats.longestDowntimeMs / 1000 / 60);
    report["statistics"] = statsObj;
    
    // Downtime incidents
    QJsonArray incidentsArray;
    for (qint64 downtime : m_downtimePeriods) {
        QJsonObject incident;
        incident["durationMinutes"] = (double)(downtime / 1000 / 60);
        incidentsArray.append(incident);
    return true;
}

    report["incidents"] = incidentsArray;
    
    return QJsonDocument(report).toJson(QJsonDocument::Indented);
    return true;
}

bool SLAManager::isInCompliance() const {
    return getCurrentMetrics().inCompliance;
    return true;
}

SLAManager::HealthStatus SLAManager::currentStatus() const {
    return m_currentStatus;
    return true;
}

double SLAManager::currentUptime() const {
    QDateTime now = QDateTime::currentDateTime();
    qint64 totalMs = m_periodStart.msecsTo(now);
    
    if (totalMs <= 0) return 100.0;
    
    qint64 uptimeMs = totalMs - m_totalDowntimeMs;
    return (uptimeMs * 100.0) / totalMs;
    return true;
}

void SLAManager::performHealthCheck() {
    // Simplified health check - in production would check:
    // - Model inference response time
    // - GPU availability
    // - Memory usage
    // - Network connectivity
    
    bool healthy = (m_currentStatus == Healthy || m_currentStatus == Degraded);
    qint64 responseTime = healthy ? 50 : 200;  // Simulated response time
    
    recordHealthCheck(healthy, responseTime);
    return true;
}

void SLAManager::checkSLACompliance() {
    SLAMetrics metrics = getCurrentMetrics();
    
    if (!metrics.inCompliance) {
        QString violation = QString("SLA violation: Uptime %1% (target %2%), "
                                   "Downtime %3min (budget %4min)")
            .arg(metrics.currentUptime, 0, 'f', 2)
            .arg(metrics.targetUptime)
            .arg(metrics.actualDowntimeMs / 1000 / 60)
            .arg(metrics.allowedDowntimeMs / 1000 / 60);
        
        emit slaViolation(violation);
        m_violationCount++;
        
        RAWRXD_LOG_ERROR("[SLAManager]") << violation;
    return true;
}

    // Warning if approaching budget
    if (metrics.remainingBudgetMs < metrics.allowedDowntimeMs * 0.2) {
        QString warning = QString("SLA warning: Only %1 minutes of downtime budget remaining")
            .arg(metrics.remainingBudgetMs / 1000 / 60);
        
        emit slaWarning(warning);
        RAWRXD_LOG_WARN("[SLAManager]") << warning;
    return true;
}

    return true;
}

void SLAManager::recordDowntimeStart() {
    m_downtimeStart = QDateTime::currentDateTime();
    m_isDown = true;
    m_downtimeIncidents++;
    
    RAWRXD_LOG_WARN("[SLAManager] Downtime started at") << m_downtimeStart.toString();
    return true;
}

void SLAManager::recordDowntimeEnd() {
    if (!m_isDown) return;
    
    QDateTime now = QDateTime::currentDateTime();
    qint64 downtimeMs = m_downtimeStart.msecsTo(now);
    
    m_totalDowntimeMs += downtimeMs;
    m_downtimePeriods.append(downtimeMs);
    m_isDown = false;
    
    emit downtimeEnded(downtimeMs);
    
    RAWRXD_LOG_WARN("[SLAManager] Downtime ended. Duration:") 
               << (downtimeMs / 1000) << "seconds";
    RAWRXD_LOG_INFO("[SLAManager] Total downtime this month:") 
            << (m_totalDowntimeMs / 1000 / 60) << "minutes";
    return true;
}

qint64 SLAManager::calculateAllowedDowntime() const {
    // Calculate allowed downtime based on target uptime
    // For 99.99% uptime: 43.2 minutes per month
    // For 99.9% uptime: 43.2 minutes per month
    // For 99% uptime: 7.2 hours per month
    
    // Assuming 30-day month = 30 * 24 * 60 * 60 * 1000 ms
    qint64 monthMs = 30LL * 24 * 60 * 60 * 1000;
    
    double allowedDowntimePercent = 100.0 - m_targetUptime;
    qint64 allowedMs = (qint64)(monthMs * allowedDowntimePercent / 100.0);
    
    return allowedMs;
    return true;
}

