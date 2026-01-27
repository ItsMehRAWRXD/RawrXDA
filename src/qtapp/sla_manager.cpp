#include "sla_manager.hpp"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <algorithm>

SLAManager& SLAManager::instance() {
    static SLAManager instance;
    return instance;
}

SLAManager::SLAManager()
    : QObject(nullptr)
{
    m_healthCheckTimer = new QTimer(this);
    m_complianceCheckTimer = new QTimer(this);
    
    connect(m_healthCheckTimer, &QTimer::timeout, this, &SLAManager::performHealthCheck);
    connect(m_complianceCheckTimer, &QTimer::timeout, this, &SLAManager::checkSLACompliance);
}

SLAManager::~SLAManager() {
    stop();
}

void SLAManager::start(double targetUptime) {
    if (m_running) {
        qInfo() << "[SLAManager] Already running";
        return;
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
    
    qInfo() << "[SLAManager] Started monitoring";
    qInfo() << "[SLAManager] Target uptime:" << m_targetUptime << "%";
    qInfo() << "[SLAManager] Allowed downtime:" 
            << (calculateAllowedDowntime() / 1000 / 60) << "minutes/month";
    qInfo() << "[SLAManager] Period start:" << m_periodStart.toString();
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
    
    qInfo() << "[SLAManager] Stopped monitoring";
    qInfo() << "[SLAManager] Final uptime:" << currentUptime() << "%";
}

void SLAManager::reportStatus(HealthStatus status) {
    if (status == m_currentStatus) return;
    
    m_previousStatus = m_currentStatus;
    m_currentStatus = status;
    
    emit statusChanged(status);
    
    qInfo() << "[SLAManager] Status changed:" 
            << (status == Healthy ? "Healthy" :
                status == Degraded ? "Degraded" :
                status == Unhealthy ? "Unhealthy" : "Down");
    
    // Track downtime
    if (status == Down && !m_isDown) {
        recordDowntimeStart();
        emit downtimeStarted();
    } else if (m_previousStatus == Down && status != Down && m_isDown) {
        recordDowntimeEnd();
    }
}

void SLAManager::recordHealthCheck(bool success, qint64 responseTimeMs) {
    if (!success) {
        emit healthCheckFailed(responseTimeMs);
        
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
        emit slaWarning(QString("Response time exceeded SLA: %1ms").arg(responseTimeMs));
    }
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
    }
    
    // Calculate longest downtime
    stats.longestDowntimeMs = 0;
    for (qint64 downtime : m_downtimePeriods) {
        stats.longestDowntimeMs = qMax(stats.longestDowntimeMs, downtime);
    }
    
    return stats;
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
    }
    report["incidents"] = incidentsArray;
    
    return QJsonDocument(report).toJson(QJsonDocument::Indented);
}

bool SLAManager::isInCompliance() const {
    return getCurrentMetrics().inCompliance;
}

SLAManager::HealthStatus SLAManager::currentStatus() const {
    return m_currentStatus;
}

double SLAManager::currentUptime() const {
    QDateTime now = QDateTime::currentDateTime();
    qint64 totalMs = m_periodStart.msecsTo(now);
    
    if (totalMs <= 0) return 100.0;
    
    qint64 uptimeMs = totalMs - m_totalDowntimeMs;
    return (uptimeMs * 100.0) / totalMs;
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
        
        qCritical() << "[SLAManager]" << violation;
    }
    
    // Warning if approaching budget
    if (metrics.remainingBudgetMs < metrics.allowedDowntimeMs * 0.2) {
        QString warning = QString("SLA warning: Only %1 minutes of downtime budget remaining")
            .arg(metrics.remainingBudgetMs / 1000 / 60);
        
        emit slaWarning(warning);
        qWarning() << "[SLAManager]" << warning;
    }
}

void SLAManager::recordDowntimeStart() {
    m_downtimeStart = QDateTime::currentDateTime();
    m_isDown = true;
    m_downtimeIncidents++;
    
    qWarning() << "[SLAManager] Downtime started at" << m_downtimeStart.toString();
}

void SLAManager::recordDowntimeEnd() {
    if (!m_isDown) return;
    
    QDateTime now = QDateTime::currentDateTime();
    qint64 downtimeMs = m_downtimeStart.msecsTo(now);
    
    m_totalDowntimeMs += downtimeMs;
    m_downtimePeriods.append(downtimeMs);
    m_isDown = false;
    
    emit downtimeEnded(downtimeMs);
    
    qWarning() << "[SLAManager] Downtime ended. Duration:" 
               << (downtimeMs / 1000) << "seconds";
    qInfo() << "[SLAManager] Total downtime this month:" 
            << (m_totalDowntimeMs / 1000 / 60) << "minutes";
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
}
