/**
 * @file EnhancedDynamicLoadBalancer.cpp
 * @brief Health-Aware Dynamic Load Balancer Implementation
 * 
 * Full production implementation with SMART monitoring, TBW tracking,
 * and intelligent drive selection based on composite health scores.
 * 
 * @copyright RawrXD IDE 2026
 */

#include "EnhancedDynamicLoadBalancer.hpp"
#include <QDebug>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <algorithm>
#include <chrono>

#ifdef Q_OS_WIN
#include <Windows.h>
#include <comdef.h>
#include <Wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")
#endif

namespace rawrxd::thermal {

// ═══════════════════════════════════════════════════════════════════════════════
// Construction
// ═══════════════════════════════════════════════════════════════════════════════

EnhancedDynamicLoadBalancer::EnhancedDynamicLoadBalancer(QObject* parent)
    : QObject(parent)
    , m_healthTimer(std::make_unique<QTimer>(this))
    , m_thermalTimer(std::make_unique<QTimer>(this))
{
    connect(m_healthTimer.get(), &QTimer::timeout, this, &EnhancedDynamicLoadBalancer::onHealthPollTimer);
    connect(m_thermalTimer.get(), &QTimer::timeout, this, &EnhancedDynamicLoadBalancer::onThermalPollTimer);
}

EnhancedDynamicLoadBalancer::~EnhancedDynamicLoadBalancer()
{
    stopMonitoring();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Configuration
// ═══════════════════════════════════════════════════════════════════════════════

void EnhancedDynamicLoadBalancer::setConfig(const LoadBalancerConfig& config)
{
    QMutexLocker lock(&m_mutex);
    m_config = config;
    
    // Normalize weights
    double total = m_config.thermalWeight + m_config.loadWeight + m_config.healthWeight;
    if (total > 0 && std::abs(total - 1.0) > 0.001) {
        m_config.thermalWeight /= total;
        m_config.loadWeight /= total;
        m_config.healthWeight /= total;
    }
    
    // Update timer intervals
    if (m_monitoring) {
        m_healthTimer->setInterval(m_config.healthPollIntervalMs);
        m_thermalTimer->setInterval(m_config.thermalPollIntervalMs);
    }
    
    recalculateScores();
}

LoadBalancerConfig EnhancedDynamicLoadBalancer::getConfig() const
{
    QMutexLocker lock(&m_mutex);
    return m_config;
}

void EnhancedDynamicLoadBalancer::setWeights(double thermalWeight, double loadWeight, double healthWeight)
{
    LoadBalancerConfig config = getConfig();
    config.thermalWeight = thermalWeight;
    config.loadWeight = loadWeight;
    config.healthWeight = healthWeight;
    setConfig(config);
}

void EnhancedDynamicLoadBalancer::setThresholds(double minHealth, double thermalWarning, double thermalCritical)
{
    QMutexLocker lock(&m_mutex);
    m_config.minHealthScore = minHealth;
    m_config.thermalWarning = thermalWarning;
    m_config.thermalCritical = thermalCritical;
    recalculateScores();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Drive Management
// ═══════════════════════════════════════════════════════════════════════════════

void EnhancedDynamicLoadBalancer::addDrive(const QString& drivePath, double ratedTBW)
{
    QMutexLocker lock(&m_mutex);
    
    DriveHealthProfile profile;
    profile.drivePath = drivePath;
    profile.tbw.ratedTBW = ratedTBW;
    profile.lastUpdateMs = getCurrentTimestampMs();
    
    m_drives[drivePath] = profile;
    
    lock.unlock();
    
    emit drivesChanged();
    
    // Immediately query health data
    if (m_config.enableHealthMonitoring) {
        queryWMIHealth(drivePath);
    }
}

void EnhancedDynamicLoadBalancer::removeDrive(const QString& drivePath)
{
    QMutexLocker lock(&m_mutex);
    m_drives.erase(drivePath);
    m_previousScores.erase(drivePath);
    
    lock.unlock();
    emit drivesChanged();
}

void EnhancedDynamicLoadBalancer::clearDrives()
{
    QMutexLocker lock(&m_mutex);
    m_drives.clear();
    m_previousScores.clear();
    m_lastSelectedDrive.clear();
    
    lock.unlock();
    emit drivesChanged();
}

int EnhancedDynamicLoadBalancer::getDriveCount() const
{
    QMutexLocker lock(&m_mutex);
    return static_cast<int>(m_drives.size());
}

QStringList EnhancedDynamicLoadBalancer::getDrivePaths() const
{
    QMutexLocker lock(&m_mutex);
    QStringList paths;
    for (const auto& pair : m_drives) {
        paths.append(pair.first);
    }
    return paths;
}

bool EnhancedDynamicLoadBalancer::hasDrive(const QString& drivePath) const
{
    QMutexLocker lock(&m_mutex);
    return m_drives.find(drivePath) != m_drives.end();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Health Updates
// ═══════════════════════════════════════════════════════════════════════════════

void EnhancedDynamicLoadBalancer::updateHealth(const QString& drivePath, const DriveHealthProfile& profile)
{
    QMutexLocker lock(&m_mutex);
    
    auto it = m_drives.find(drivePath);
    if (it == m_drives.end()) {
        qWarning() << "Drive not found:" << drivePath;
        return;
    }
    
    // Preserve rated TBW if not in new profile
    double ratedTBW = it->second.tbw.ratedTBW;
    it->second = profile;
    if (it->second.tbw.ratedTBW == 0.0) {
        it->second.tbw.ratedTBW = ratedTBW;
    }
    
    it->second.lastUpdateMs = getCurrentTimestampMs();
    it->second.calculateComposite(m_config.thermalWeight, m_config.loadWeight, m_config.healthWeight);
    
    lock.unlock();
    
    checkHealthAlerts(drivePath, profile);
    emit healthUpdated(drivePath, profile.compositeScore);
}

void EnhancedDynamicLoadBalancer::updateTemperature(const QString& drivePath, double temperature)
{
    QMutexLocker lock(&m_mutex);
    
    auto it = m_drives.find(drivePath);
    if (it != m_drives.end()) {
        it->second.currentTemperature = temperature;
        it->second.lastUpdateMs = getCurrentTimestampMs();
        it->second.calculateComposite(m_config.thermalWeight, m_config.loadWeight, m_config.healthWeight);
        
        if (temperature >= m_config.thermalCritical) {
            it->second.isThrottled = true;
            
            lock.unlock();
            emit healthAlert(drivePath, 
                QString("Temperature critical: %.1f°C").arg(temperature), true);
        } else if (temperature >= m_config.thermalWarning) {
            lock.unlock();
            emit healthAlert(drivePath,
                QString("Temperature warning: %.1f°C").arg(temperature), false);
        }
    }
}

void EnhancedDynamicLoadBalancer::updateLoad(const QString& drivePath, double loadPercent)
{
    QMutexLocker lock(&m_mutex);
    
    auto it = m_drives.find(drivePath);
    if (it != m_drives.end()) {
        it->second.currentLoad = loadPercent;
        it->second.lastUpdateMs = getCurrentTimestampMs();
        it->second.calculateComposite(m_config.thermalWeight, m_config.loadWeight, m_config.healthWeight);
    }
}

void EnhancedDynamicLoadBalancer::updateSMART(const QString& drivePath, const SMARTData& smart)
{
    QMutexLocker lock(&m_mutex);
    
    auto it = m_drives.find(drivePath);
    if (it != m_drives.end()) {
        it->second.smart = smart;
        it->second.smart.calculateHealth();
        it->second.lastUpdateMs = getCurrentTimestampMs();
        it->second.calculateComposite(m_config.thermalWeight, m_config.loadWeight, m_config.healthWeight);
        
        DriveHealthProfile profile = it->second;
        lock.unlock();
        
        checkHealthAlerts(drivePath, profile);
        emit healthUpdated(drivePath, profile.smart.overallHealth / 100.0);
    }
}

void EnhancedDynamicLoadBalancer::updateTBW(const QString& drivePath, double totalTBW)
{
    QMutexLocker lock(&m_mutex);
    
    auto it = m_drives.find(drivePath);
    if (it != m_drives.end()) {
        it->second.tbw.totalTBW = totalTBW;
        it->second.tbw.calculate();
        it->second.lastUpdateMs = getCurrentTimestampMs();
        
        // Alert if near end of life
        if (it->second.tbw.wearLevel >= 90.0) {
            lock.unlock();
            emit healthAlert(drivePath,
                QString("TBW critical: %.0f%% of rated life used").arg(it->second.tbw.wearLevel),
                true);
        } else if (it->second.tbw.wearLevel >= 75.0) {
            lock.unlock();
            emit healthAlert(drivePath,
                QString("TBW warning: %.0f%% of rated life used").arg(it->second.tbw.wearLevel),
                false);
        }
    }
}

DriveHealthProfile EnhancedDynamicLoadBalancer::getHealthProfile(const QString& drivePath) const
{
    QMutexLocker lock(&m_mutex);
    
    auto it = m_drives.find(drivePath);
    if (it != m_drives.end()) {
        return it->second;
    }
    return DriveHealthProfile();
}

std::vector<DriveHealthProfile> EnhancedDynamicLoadBalancer::getAllProfiles() const
{
    QMutexLocker lock(&m_mutex);
    
    std::vector<DriveHealthProfile> profiles;
    profiles.reserve(m_drives.size());
    
    for (const auto& pair : m_drives) {
        profiles.push_back(pair.second);
    }
    
    return profiles;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Drive Selection
// ═══════════════════════════════════════════════════════════════════════════════

QString EnhancedDynamicLoadBalancer::selectOptimalDrive(OperationType opType)
{
    QMutexLocker lock(&m_mutex);
    
    if (m_drives.empty()) {
        qWarning() << "No drives available for selection";
        return QString();
    }
    
    QString bestDrive;
    double bestScore = -1.0;
    
    for (const auto& pair : m_drives) {
        const auto& profile = pair.second;
        
        // Skip offline drives
        if (!profile.isOnline) {
            continue;
        }
        
        // Skip critically unhealthy drives (unless no choice)
        if (profile.smart.isCritical && m_config.enableAutoExclusion) {
            continue;
        }
        
        // Skip drives below minimum health
        if (profile.healthScore < m_config.minHealthScore && m_config.enableAutoExclusion) {
            continue;
        }
        
        // Calculate adjusted score for operation type
        double score = adjustScoreForOperation(profile.compositeScore, profile, opType);
        
        // Apply hysteresis if this was the previous selection
        if (pair.first == m_config.lastSelectedDrive) {
            score += m_config.hysteresisThreshold;
        }
        
        if (score > bestScore) {
            bestScore = score;
            bestDrive = pair.first;
        }
    }
    
    // If no drive found, try without exclusions
    if (bestDrive.isEmpty() && m_config.allowDegradedOperation) {
        for (const auto& pair : m_drives) {
            if (pair.second.isOnline && pair.second.compositeScore > bestScore) {
                bestScore = pair.second.compositeScore;
                bestDrive = pair.first;
            }
        }
        
        if (!bestDrive.isEmpty()) {
            qWarning() << "Operating in degraded mode with drive:" << bestDrive;
        }
    }
    
    // Update state and emit signal
    if (!bestDrive.isEmpty() && bestDrive != m_lastSelectedDrive) {
        QString previousDrive = m_lastSelectedDrive;
        m_lastSelectedDrive = bestDrive;
        m_config.lastSelectedDrive = bestDrive;
        
        lock.unlock();
        
        // Notify of balancing event
        if (!previousDrive.isEmpty()) {
            QString reason = QString("Score: %1% vs %2%")
                .arg(static_cast<int>(bestScore * 100))
                .arg(static_cast<int>(m_previousScores[previousDrive] * 100));
            
            emit balancingEvent(previousDrive, bestDrive, reason);
            
            if (m_balancingEventCallback) {
                m_balancingEventCallback(previousDrive, bestDrive, reason);
            }
        }
        
        emit driveSelected(bestDrive);
        
        if (m_driveSelectedCallback) {
            m_driveSelectedCallback(bestDrive, m_drives[bestDrive]);
        }
    }
    
    // Store score for next comparison
    m_previousScores[bestDrive] = bestScore;
    
    return bestDrive;
}

std::vector<QString> EnhancedDynamicLoadBalancer::getRankedDrives(OperationType opType)
{
    QMutexLocker lock(&m_mutex);
    
    std::vector<std::pair<QString, double>> scored;
    
    for (const auto& pair : m_drives) {
        if (!pair.second.isOnline) continue;
        
        double score = adjustScoreForOperation(pair.second.compositeScore, pair.second, opType);
        scored.push_back({pair.first, score});
    }
    
    // Sort by score descending
    std::sort(scored.begin(), scored.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });
    
    std::vector<QString> ranked;
    ranked.reserve(scored.size());
    for (const auto& pair : scored) {
        ranked.push_back(pair.first);
    }
    
    return ranked;
}

QString EnhancedDynamicLoadBalancer::getLastSelectedDrive() const
{
    QMutexLocker lock(&m_mutex);
    return m_lastSelectedDrive;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Monitoring
// ═══════════════════════════════════════════════════════════════════════════════

void EnhancedDynamicLoadBalancer::startMonitoring()
{
    if (m_monitoring) return;
    
    m_monitoring = true;
    
    m_healthTimer->setInterval(m_config.healthPollIntervalMs);
    m_thermalTimer->setInterval(m_config.thermalPollIntervalMs);
    
    m_healthTimer->start();
    m_thermalTimer->start();
    
    // Initial refresh
    refreshHealthData();
    refreshThermalData();
    
    qInfo() << "Load balancer monitoring started";
}

void EnhancedDynamicLoadBalancer::stopMonitoring()
{
    if (!m_monitoring) return;
    
    m_monitoring = false;
    m_healthTimer->stop();
    m_thermalTimer->stop();
    
    qInfo() << "Load balancer monitoring stopped";
}

bool EnhancedDynamicLoadBalancer::isMonitoring() const
{
    return m_monitoring;
}

void EnhancedDynamicLoadBalancer::refreshHealthData()
{
    QStringList paths = getDrivePaths();
    
    for (const QString& path : paths) {
        queryWMIHealth(path);
    }
}

void EnhancedDynamicLoadBalancer::refreshThermalData()
{
    // Query thermal data via WMI or direct sensor access
    QStringList paths = getDrivePaths();
    
    for (const QString& path : paths) {
        queryNVMeHealth(path);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Callbacks
// ═══════════════════════════════════════════════════════════════════════════════

void EnhancedDynamicLoadBalancer::setDriveSelectedCallback(DriveSelectedCallback callback)
{
    QMutexLocker lock(&m_mutex);
    m_driveSelectedCallback = std::move(callback);
}

void EnhancedDynamicLoadBalancer::setHealthAlertCallback(HealthAlertCallback callback)
{
    QMutexLocker lock(&m_mutex);
    m_healthAlertCallback = std::move(callback);
}

void EnhancedDynamicLoadBalancer::setBalancingEventCallback(BalancingEventCallback callback)
{
    QMutexLocker lock(&m_mutex);
    m_balancingEventCallback = std::move(callback);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Statistics
// ═══════════════════════════════════════════════════════════════════════════════

double EnhancedDynamicLoadBalancer::getAverageHealth() const
{
    QMutexLocker lock(&m_mutex);
    if (m_drives.empty()) return 0.0;
    
    double sum = 0.0;
    for (const auto& pair : m_drives) {
        sum += pair.second.healthScore;
    }
    return sum / m_drives.size();
}

double EnhancedDynamicLoadBalancer::getAverageTemperature() const
{
    QMutexLocker lock(&m_mutex);
    if (m_drives.empty()) return 0.0;
    
    double sum = 0.0;
    for (const auto& pair : m_drives) {
        sum += pair.second.currentTemperature;
    }
    return sum / m_drives.size();
}

double EnhancedDynamicLoadBalancer::getAverageLoad() const
{
    QMutexLocker lock(&m_mutex);
    if (m_drives.empty()) return 0.0;
    
    double sum = 0.0;
    for (const auto& pair : m_drives) {
        sum += pair.second.currentLoad;
    }
    return sum / m_drives.size();
}

int EnhancedDynamicLoadBalancer::getHealthyDriveCount() const
{
    QMutexLocker lock(&m_mutex);
    int count = 0;
    for (const auto& pair : m_drives) {
        if (pair.second.smart.isHealthy && pair.second.isOnline) {
            count++;
        }
    }
    return count;
}

int EnhancedDynamicLoadBalancer::getCriticalDriveCount() const
{
    QMutexLocker lock(&m_mutex);
    int count = 0;
    for (const auto& pair : m_drives) {
        if (pair.second.smart.isCritical) {
            count++;
        }
    }
    return count;
}

QVariantMap EnhancedDynamicLoadBalancer::getStatistics() const
{
    QVariantMap stats;
    
    stats["driveCount"] = getDriveCount();
    stats["healthyDriveCount"] = getHealthyDriveCount();
    stats["criticalDriveCount"] = getCriticalDriveCount();
    stats["averageHealth"] = getAverageHealth();
    stats["averageTemperature"] = getAverageTemperature();
    stats["averageLoad"] = getAverageLoad();
    stats["lastSelectedDrive"] = getLastSelectedDrive();
    stats["isMonitoring"] = isMonitoring();
    
    return stats;
}

QVariantMap EnhancedDynamicLoadBalancer::getDriveStatistics(const QString& drivePath) const
{
    QMutexLocker lock(&m_mutex);
    
    QVariantMap stats;
    auto it = m_drives.find(drivePath);
    
    if (it != m_drives.end()) {
        const auto& profile = it->second;
        
        stats["drivePath"] = profile.drivePath;
        stats["driveModel"] = profile.driveModel;
        stats["serialNumber"] = profile.serialNumber;
        stats["temperature"] = profile.currentTemperature;
        stats["load"] = profile.currentLoad;
        stats["healthScore"] = profile.healthScore * 100;
        stats["thermalScore"] = profile.thermalScore * 100;
        stats["loadScore"] = profile.loadScore * 100;
        stats["compositeScore"] = profile.compositeScore * 100;
        stats["isOnline"] = profile.isOnline;
        stats["isThrottled"] = profile.isThrottled;
        stats["statusMessage"] = profile.statusMessage;
        
        // SMART data
        stats["smartHealth"] = profile.smart.overallHealth;
        stats["smartHealthy"] = profile.smart.isHealthy;
        stats["smartCritical"] = profile.smart.isCritical;
        stats["reallocatedSectors"] = profile.smart.reallocatedSectorCount;
        stats["pendingSectors"] = profile.smart.currentPendingSectorCount;
        stats["availableSpare"] = profile.smart.availableSpare;
        stats["mediaErrors"] = profile.smart.mediaErrors;
        
        // TBW data
        stats["totalTBW"] = profile.tbw.totalTBW;
        stats["ratedTBW"] = profile.tbw.ratedTBW;
        stats["wearLevel"] = profile.tbw.wearLevel;
        stats["estimatedDaysRemaining"] = profile.tbw.estimatedDaysRemaining;
    }
    
    return stats;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Private Slots
// ═══════════════════════════════════════════════════════════════════════════════

void EnhancedDynamicLoadBalancer::onHealthPollTimer()
{
    refreshHealthData();
}

void EnhancedDynamicLoadBalancer::onThermalPollTimer()
{
    refreshThermalData();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Private Helpers
// ═══════════════════════════════════════════════════════════════════════════════

void EnhancedDynamicLoadBalancer::recalculateScores()
{
    for (auto& pair : m_drives) {
        pair.second.calculateComposite(m_config.thermalWeight, m_config.loadWeight, m_config.healthWeight);
    }
}

void EnhancedDynamicLoadBalancer::checkHealthAlerts(const QString& drivePath, const DriveHealthProfile& profile)
{
    // SMART critical
    if (profile.smart.isCritical) {
        QString alert = QString("SMART critical failure predicted - Health: %1%")
            .arg(static_cast<int>(profile.smart.overallHealth));
        
        emit healthAlert(drivePath, alert, true);
        
        if (m_healthAlertCallback) {
            m_healthAlertCallback(drivePath, alert, true);
        }
    }
    
    // Bad sectors growing
    if (profile.badSectors.hasGrowingBadSectors) {
        QString alert = QString("Bad sector count increasing: %1/day")
            .arg(profile.badSectors.badSectorGrowthRate);
        
        emit healthAlert(drivePath, alert, true);
        
        if (m_healthAlertCallback) {
            m_healthAlertCallback(drivePath, alert, true);
        }
    }
    
    // TBW exhaustion
    if (profile.tbw.wearLevel >= 90.0) {
        QString alert = QString("SSD wear level critical: %1% of rated TBW used")
            .arg(static_cast<int>(profile.tbw.wearLevel));
        
        emit healthAlert(drivePath, alert, true);
        
        if (m_healthAlertCallback) {
            m_healthAlertCallback(drivePath, alert, true);
        }
    }
}

double EnhancedDynamicLoadBalancer::adjustScoreForOperation(double baseScore, 
    const DriveHealthProfile& profile, OperationType opType)
{
    double adjustedScore = baseScore;
    
    switch (opType) {
        case OperationType::Write:
            // Prefer drives with more TBW remaining
            if (profile.tbw.ratedTBW > 0) {
                double tbwBonus = (1.0 - profile.tbw.wearLevel / 100.0) * 0.1;
                adjustedScore += tbwBonus;
            }
            // Penalize high-temperature drives more for writes
            if (profile.currentTemperature > 60.0) {
                adjustedScore *= 0.9;
            }
            break;
            
        case OperationType::Read:
            // Reads are less sensitive to health
            adjustedScore = baseScore * 0.8 + profile.thermalScore * 0.2;
            break;
            
        case OperationType::Sequential:
            // Prefer lower load drives for sequential
            adjustedScore += profile.loadScore * 0.1;
            break;
            
        case OperationType::Random:
            // Random I/O benefits from cooler drives
            adjustedScore += profile.thermalScore * 0.15;
            break;
            
        case OperationType::LargeFile:
            // Large files benefit from thermal headroom
            adjustedScore += profile.thermalScore * 0.2;
            // And lower queue depth
            if (profile.queueDepth < 16) {
                adjustedScore += 0.05;
            }
            break;
            
        case OperationType::SmallFiles:
            // Small files benefit from healthy drives
            adjustedScore += profile.healthScore * 0.1;
            break;
    }
    
    return std::max(0.0, std::min(1.0, adjustedScore));
}

void EnhancedDynamicLoadBalancer::queryWMIHealth(const QString& drivePath)
{
#ifdef Q_OS_WIN
    // Use PowerShell to query disk health (async)
    QProcess* process = new QProcess(this);
    
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        [this, drivePath, process](int exitCode, QProcess::ExitStatus) {
            if (exitCode == 0) {
                QString output = process->readAllStandardOutput();
                
                // Parse JSON output
                QJsonDocument doc = QJsonDocument::fromJson(output.toUtf8());
                if (!doc.isNull() && doc.isObject()) {
                    QJsonObject obj = doc.object();
                    
                    QMutexLocker lock(&m_mutex);
                    auto it = m_drives.find(drivePath);
                    if (it != m_drives.end()) {
                        // Update model info
                        it->second.driveModel = obj["Model"].toString();
                        it->second.serialNumber = obj["SerialNumber"].toString();
                        
                        // Update SMART-equivalent data
                        if (obj.contains("HealthStatus")) {
                            QString health = obj["HealthStatus"].toString();
                            if (health == "Healthy") {
                                it->second.smart.overallHealth = 95.0;
                                it->second.smart.isHealthy = true;
                            } else if (health == "Warning") {
                                it->second.smart.overallHealth = 50.0;
                                it->second.smart.isHealthy = false;
                            } else {
                                it->second.smart.overallHealth = 20.0;
                                it->second.smart.isHealthy = false;
                                it->second.smart.isCritical = true;
                            }
                        }
                        
                        it->second.calculateComposite(m_config.thermalWeight, 
                            m_config.loadWeight, m_config.healthWeight);
                    }
                }
            }
            process->deleteLater();
        });
    
    QString script = QString(
        "Get-PhysicalDisk | Where-Object { $_.DeviceId -match '%1' } | "
        "Select-Object Model, SerialNumber, HealthStatus, OperationalStatus | "
        "ConvertTo-Json"
    ).arg(drivePath.left(1));
    
    process->start("powershell", {"-Command", script});
#else
    Q_UNUSED(drivePath);
#endif
}

void EnhancedDynamicLoadBalancer::queryNVMeHealth(const QString& drivePath)
{
#ifdef Q_OS_WIN
    // Query NVMe specific health via StorageReliabilityCounter
    QProcess* process = new QProcess(this);
    
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        [this, drivePath, process](int exitCode, QProcess::ExitStatus) {
            if (exitCode == 0) {
                QString output = process->readAllStandardOutput();
                
                QJsonDocument doc = QJsonDocument::fromJson(output.toUtf8());
                if (!doc.isNull() && doc.isObject()) {
                    QJsonObject obj = doc.object();
                    
                    QMutexLocker lock(&m_mutex);
                    auto it = m_drives.find(drivePath);
                    if (it != m_drives.end()) {
                        // NVMe temperature
                        if (obj.contains("Temperature")) {
                            it->second.currentTemperature = obj["Temperature"].toDouble();
                        }
                        
                        // NVMe wear
                        if (obj.contains("Wear")) {
                            it->second.smart.percentageUsed = obj["Wear"].toInt();
                        }
                        
                        // Read/Write counts for TBW
                        if (obj.contains("ReadErrorsTotal")) {
                            it->second.smart.rawReadErrorRate = obj["ReadErrorsTotal"].toInt();
                        }
                        
                        it->second.calculateComposite(m_config.thermalWeight,
                            m_config.loadWeight, m_config.healthWeight);
                    }
                }
            }
            process->deleteLater();
        });
    
    QString script = QString(
        "Get-PhysicalDisk | Where-Object { $_.DeviceId -match '%1' } | "
        "Get-StorageReliabilityCounter | "
        "Select-Object Temperature, Wear, ReadErrorsTotal, WriteErrorsTotal | "
        "ConvertTo-Json"
    ).arg(drivePath.left(1));
    
    process->start("powershell", {"-Command", script});
#else
    Q_UNUSED(drivePath);
#endif
}

int64_t EnhancedDynamicLoadBalancer::getCurrentTimestampMs() const
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()
    ).count();
}

} // namespace rawrxd::thermal
