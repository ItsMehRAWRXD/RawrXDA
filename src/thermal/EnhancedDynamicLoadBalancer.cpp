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


#include <algorithm>
#include <chrono>

#ifdef _WIN32
#include <Windows.h>
#include <comdef.h>
#include <Wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")
#endif

namespace rawrxd::thermal {

// ═══════════════════════════════════════════════════════════════════════════════
// Construction
// ═══════════════════════════════════════════════════════════════════════════════

EnhancedDynamicLoadBalancer::EnhancedDynamicLoadBalancer(void* parent)
    : m_healthTimer(nullptr)
    , m_thermalTimer(nullptr)
{
    (void)parent;
// Qt connect removed
// Qt connect removed
}

EnhancedDynamicLoadBalancer::~EnhancedDynamicLoadBalancer()
{
    stopMonitoring();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Configuration
// ═══════════════════════════════════════════════════════════════════════════════

void EnhancedDynamicLoadBalancer::setConfig(const EnhancedLoadBalancerConfig& config)
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    m_config = config;
    
    // Normalize weights
    double total = m_config.thermalWeight + m_config.loadWeight + m_config.healthWeight;
    if (total > 0 && std::abs(total - 1.0) > 0.001) {
        m_config.thermalWeight /= total;
        m_config.loadWeight /= total;
        m_config.healthWeight /= total;
    }
    
    // Timer stubs: no-op for Win32 backend (was QTimer::setInterval)
    (void)m_monitoring;
    
    recalculateScores();
}

EnhancedLoadBalancerConfig EnhancedDynamicLoadBalancer::getConfig() const
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    return m_config;
}

void EnhancedDynamicLoadBalancer::setWeights(double thermalWeight, double loadWeight, double healthWeight)
{
    EnhancedLoadBalancerConfig config = getConfig();
    config.thermalWeight = thermalWeight;
    config.loadWeight = loadWeight;
    config.healthWeight = healthWeight;
    setConfig(config);
}

void EnhancedDynamicLoadBalancer::setThresholds(double minHealth, double thermalWarning, double thermalCritical)
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    m_config.minHealthScore = minHealth;
    m_config.thermalWarning = thermalWarning;
    m_config.thermalCritical = thermalCritical;
    recalculateScores();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Drive Management
// ═══════════════════════════════════════════════════════════════════════════════

void EnhancedDynamicLoadBalancer::addDrive(const std::string& drivePath, double ratedTBW)
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    
    DriveHealthProfile profile;
    profile.drivePath = drivePath;
    profile.tbw.ratedTBW = ratedTBW;
    profile.lastUpdateMs = getCurrentTimestampMs();
    
    m_drives[drivePath] = profile;
    
    lock.unlock();
    
    drivesChanged();
    
    // Immediately query health data
    if (m_config.enableHealthMonitoring) {
        queryWMIHealth(drivePath);
    }
}

void EnhancedDynamicLoadBalancer::removeDrive(const std::string& drivePath)
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    m_drives.erase(drivePath);
    m_previousScores.erase(drivePath);
    
    lock.unlock();
    drivesChanged();
}

void EnhancedDynamicLoadBalancer::clearDrives()
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    m_drives.clear();
    m_previousScores.clear();
    m_lastSelectedDrive.clear();
    
    lock.unlock();
    drivesChanged();
}

int EnhancedDynamicLoadBalancer::getDriveCount() const
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    return static_cast<int>(m_drives.size());
}

std::vector<std::string> EnhancedDynamicLoadBalancer::getDrivePaths() const
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    std::vector<std::string> paths;
    for (const auto& pair : m_drives) {
        paths.append(pair.first);
    }
    return paths;
}

bool EnhancedDynamicLoadBalancer::hasDrive(const std::string& drivePath) const
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    return m_drives.find(drivePath) != m_drives.end();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Health Updates
// ═══════════════════════════════════════════════════════════════════════════════

void EnhancedDynamicLoadBalancer::updateHealth(const std::string& drivePath, const DriveHealthProfile& profile)
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    
    auto it = m_drives.find(drivePath);
    if (it == m_drives.end()) {
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
    healthUpdated(drivePath, profile.compositeScore);
}

void EnhancedDynamicLoadBalancer::updateTemperature(const std::string& drivePath, double temperature)
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    
    auto it = m_drives.find(drivePath);
    if (it != m_drives.end()) {
        it->second.currentTemperature = temperature;
        it->second.lastUpdateMs = getCurrentTimestampMs();
        it->second.calculateComposite(m_config.thermalWeight, m_config.loadWeight, m_config.healthWeight);
        
        if (temperature >= m_config.thermalCritical) {
            it->second.isThrottled = true;
            
            lock.unlock();
            healthAlert(drivePath, 
                std::string("Temperature critical: %.1f°C"), true);
        } else if (temperature >= m_config.thermalWarning) {
            lock.unlock();
            healthAlert(drivePath,
                std::string("Temperature warning: %.1f°C"), false);
        }
    }
}

void EnhancedDynamicLoadBalancer::updateLoad(const std::string& drivePath, double loadPercent)
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    
    auto it = m_drives.find(drivePath);
    if (it != m_drives.end()) {
        it->second.currentLoad = loadPercent;
        it->second.lastUpdateMs = getCurrentTimestampMs();
        it->second.calculateComposite(m_config.thermalWeight, m_config.loadWeight, m_config.healthWeight);
    }
}

void EnhancedDynamicLoadBalancer::updateSMART(const std::string& drivePath, const SMARTData& smart)
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    
    auto it = m_drives.find(drivePath);
    if (it != m_drives.end()) {
        it->second.smart = smart;
        it->second.smart.calculateHealth();
        it->second.lastUpdateMs = getCurrentTimestampMs();
        it->second.calculateComposite(m_config.thermalWeight, m_config.loadWeight, m_config.healthWeight);
        
        DriveHealthProfile profile = it->second;
        lock.unlock();
        
        checkHealthAlerts(drivePath, profile);
        healthUpdated(drivePath, profile.smart.overallHealth / 100.0);
    }
}

void EnhancedDynamicLoadBalancer::updateTBW(const std::string& drivePath, double totalTBW)
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    
    auto it = m_drives.find(drivePath);
    if (it != m_drives.end()) {
        it->second.tbw.totalTBW = totalTBW;
        it->second.tbw.calculate();
        it->second.lastUpdateMs = getCurrentTimestampMs();
        
        // Alert if near end of life
        if (it->second.tbw.wearLevel >= 90.0) {
            lock.unlock();
            healthAlert(drivePath,
                std::string("TBW critical: %.0f%% of rated life used"),
                true);
        } else if (it->second.tbw.wearLevel >= 75.0) {
            lock.unlock();
            healthAlert(drivePath,
                std::string("TBW warning: %.0f%% of rated life used"),
                false);
        }
    }
}

DriveHealthProfile EnhancedDynamicLoadBalancer::getHealthProfile(const std::string& drivePath) const
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    
    auto it = m_drives.find(drivePath);
    if (it != m_drives.end()) {
        return it->second;
    }
    return DriveHealthProfile();
}

std::vector<DriveHealthProfile> EnhancedDynamicLoadBalancer::getAllProfiles() const
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    
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

std::string EnhancedDynamicLoadBalancer::selectOptimalDrive(OperationType opType)
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    
    if (m_drives.empty()) {
        return std::string();
    }
    
    std::string bestDrive;
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
    if (bestDrive.empty() && m_config.allowDegradedOperation) {
        for (const auto& pair : m_drives) {
            if (pair.second.isOnline && pair.second.compositeScore > bestScore) {
                bestScore = pair.second.compositeScore;
                bestDrive = pair.first;
            }
        }
        
        if (!bestDrive.empty()) {
        }
    }
    
    // Update state and signal
    if (!bestDrive.empty() && bestDrive != m_lastSelectedDrive) {
        std::string previousDrive = m_lastSelectedDrive;
        m_lastSelectedDrive = bestDrive;
        m_config.lastSelectedDrive = bestDrive;
        
        lock.unlock();
        
        // Notify of balancing event
        if (!previousDrive.empty()) {
            std::string reason = std::string("Score: %1% vs %2%")
                )
                );
            
            balancingEvent(previousDrive, bestDrive, reason);
            
            if (m_balancingEventCallback) {
                m_balancingEventCallback(previousDrive, bestDrive, reason);
            }
        }
        
        driveSelected(bestDrive);
        
        if (m_driveSelectedCallback) {
            m_driveSelectedCallback(bestDrive, m_drives[bestDrive]);
        }
    }
    
    // Store score for next comparison
    m_previousScores[bestDrive] = bestScore;
    
    return bestDrive;
}

std::vector<std::string> EnhancedDynamicLoadBalancer::getRankedDrives(OperationType opType)
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    
    std::vector<std::pair<std::string, double>> scored;
    
    for (const auto& pair : m_drives) {
        if (!pair.second.isOnline) continue;
        
        double score = adjustScoreForOperation(pair.second.compositeScore, pair.second, opType);
        scored.push_back({pair.first, score});
    }
    
    // Sort by score descending
    std::sort(scored.begin(), scored.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });
    
    std::vector<std::string> ranked;
    ranked.reserve(scored.size());
    for (const auto& pair : scored) {
        ranked.push_back(pair.first);
    }
    
    return ranked;
}

std::string EnhancedDynamicLoadBalancer::getLastSelectedDrive() const
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    return m_lastSelectedDrive;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Monitoring
// ═══════════════════════════════════════════════════════════════════════════════

void EnhancedDynamicLoadBalancer::startMonitoring()
{
    if (m_monitoring) return;
    
    m_monitoring = true;
    
    // Timer stubs: no-op for Win32 backend (was QTimer)
    
    // Initial refresh
    refreshHealthData();
    refreshThermalData();
    
}

void EnhancedDynamicLoadBalancer::stopMonitoring()
{
    if (!m_monitoring) return;
    
    m_monitoring = false;
    // Timer stubs: no-op for Win32 backend
}

bool EnhancedDynamicLoadBalancer::isMonitoring() const
{
    return m_monitoring;
}

void EnhancedDynamicLoadBalancer::refreshHealthData()
{
    std::vector<std::string> paths = getDrivePaths();
    
    for (const std::string& path : paths) {
        queryWMIHealth(path);
    }
}

void EnhancedDynamicLoadBalancer::refreshThermalData()
{
    // Query thermal data via WMI or direct sensor access
    std::vector<std::string> paths = getDrivePaths();
    
    for (const std::string& path : paths) {
        queryNVMeHealth(path);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Callbacks
// ═══════════════════════════════════════════════════════════════════════════════

void EnhancedDynamicLoadBalancer::setDriveSelectedCallback(DriveSelectedCallback callback)
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    m_driveSelectedCallback = std::move(callback);
}

void EnhancedDynamicLoadBalancer::setHealthAlertCallback(HealthAlertCallback callback)
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    m_healthAlertCallback = std::move(callback);
}

void EnhancedDynamicLoadBalancer::setBalancingEventCallback(BalancingEventCallback callback)
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    m_balancingEventCallback = std::move(callback);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Statistics
// ═══════════════════════════════════════════════════════════════════════════════

double EnhancedDynamicLoadBalancer::getAverageHealth() const
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    if (m_drives.empty()) return 0.0;
    
    double sum = 0.0;
    for (const auto& pair : m_drives) {
        sum += pair.second.healthScore;
    }
    return sum / m_drives.size();
}

double EnhancedDynamicLoadBalancer::getAverageTemperature() const
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    if (m_drives.empty()) return 0.0;
    
    double sum = 0.0;
    for (const auto& pair : m_drives) {
        sum += pair.second.currentTemperature;
    }
    return sum / m_drives.size();
}

double EnhancedDynamicLoadBalancer::getAverageLoad() const
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    if (m_drives.empty()) return 0.0;
    
    double sum = 0.0;
    for (const auto& pair : m_drives) {
        sum += pair.second.currentLoad;
    }
    return sum / m_drives.size();
}

int EnhancedDynamicLoadBalancer::getHealthyDriveCount() const
{
    std::lock_guard<std::mutex> lock(&m_mutex);
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
    std::lock_guard<std::mutex> lock(&m_mutex);
    int count = 0;
    for (const auto& pair : m_drives) {
        if (pair.second.smart.isCritical) {
            count++;
        }
    }
    return count;
}

std::map<std::string, std::string> EnhancedDynamicLoadBalancer::getStatistics() const
{
    std::map<std::string, std::string> stats;
    
    stats["driveCount"] = std::to_string(getDriveCount());
    stats["healthyDriveCount"] = std::to_string(getHealthyDriveCount());
    stats["criticalDriveCount"] = std::to_string(getCriticalDriveCount());
    stats["averageHealth"] = std::to_string(getAverageHealth());
    stats["averageTemperature"] = std::to_string(getAverageTemperature());
    stats["averageLoad"] = std::to_string(getAverageLoad());
    stats["lastSelectedDrive"] = getLastSelectedDrive();
    stats["isMonitoring"] = isMonitoring() ? "true" : "false";
    
    return stats;
}

std::map<std::string, std::string> EnhancedDynamicLoadBalancer::getDriveStatistics(const std::string& drivePath) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::map<std::string, std::string> stats;
    auto it = m_drives.find(drivePath);
    
    if (it != m_drives.end()) {
        const auto& profile = it->second;
        
        stats["drivePath"] = profile.drivePath;
        stats["driveModel"] = profile.driveModel;
        stats["serialNumber"] = profile.serialNumber;
        stats["temperature"] = std::to_string(profile.currentTemperature);
        stats["load"] = std::to_string(profile.currentLoad);
        stats["healthScore"] = std::to_string(profile.healthScore * 100);
        stats["thermalScore"] = std::to_string(profile.thermalScore * 100);
        stats["loadScore"] = std::to_string(profile.loadScore * 100);
        stats["compositeScore"] = std::to_string(profile.compositeScore * 100);
        stats["isOnline"] = profile.isOnline ? "true" : "false";
        stats["isThrottled"] = profile.isThrottled ? "true" : "false";
        stats["statusMessage"] = profile.statusMessage;
        
        // SMART data
        stats["smartHealth"] = std::to_string(profile.smart.overallHealth);
        stats["smartHealthy"] = profile.smart.isHealthy ? "true" : "false";
        stats["smartCritical"] = profile.smart.isCritical ? "true" : "false";
        stats["reallocatedSectors"] = std::to_string(profile.smart.reallocatedSectorCount);
        stats["pendingSectors"] = std::to_string(profile.smart.currentPendingSectorCount);
        stats["availableSpare"] = std::to_string(profile.smart.availableSpare);
        stats["mediaErrors"] = std::to_string(profile.smart.mediaErrors);
        
        // TBW data
        stats["totalTBW"] = std::to_string(profile.tbw.totalTBW);
        stats["ratedTBW"] = std::to_string(profile.tbw.ratedTBW);
        stats["wearLevel"] = std::to_string(profile.tbw.wearLevel);
        stats["estimatedDaysRemaining"] = std::to_string(profile.tbw.estimatedDaysRemaining);
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

void EnhancedDynamicLoadBalancer::checkHealthAlerts(const std::string& drivePath, const DriveHealthProfile& profile)
{
    // SMART critical
    if (profile.smart.isCritical) {
        std::string alert = std::string("SMART critical failure predicted - Health: %1%")
            );
        
        healthAlert(drivePath, alert, true);
        
        if (m_healthAlertCallback) {
            m_healthAlertCallback(drivePath, alert, true);
        }
    }
    
    // Bad sectors growing
    if (profile.badSectors.hasGrowingBadSectors) {
        std::string alert = std::string("Bad sector count increasing: %1/day")
            ;
        
        healthAlert(drivePath, alert, true);
        
        if (m_healthAlertCallback) {
            m_healthAlertCallback(drivePath, alert, true);
        }
    }
    
    // TBW exhaustion
    if (profile.tbw.wearLevel >= 90.0) {
        std::string alert = std::string("SSD wear level critical: %1% of rated TBW used")
            );
        
        healthAlert(drivePath, alert, true);
        
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

void EnhancedDynamicLoadBalancer::queryWMIHealth(const std::string& drivePath)
{
#ifdef 
    // Use PowerShell to query disk health (async)
    void** process = new void*(this);
// Qt connect removed
                // Parse JSON output
                void* doc = void*::fromJson(output.toUtf8());
                if (!doc.isNull() && doc.isObject()) {
                    void* obj = doc.object();
                    
                    std::lock_guard<std::mutex> lock(&m_mutex);
                    auto it = m_drives.find(drivePath);
                    if (it != m_drives.end()) {
                        // Update model info
                        it->second.driveModel = obj["Model"].toString();
                        it->second.serialNumber = obj["SerialNumber"].toString();
                        
                        // Update SMART-equivalent data
                        if (obj.contains("HealthStatus")) {
                            std::string health = obj["HealthStatus"].toString();
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
    
    std::string script = std::string(
        "Get-PhysicalDisk | Where-Object { $_.DeviceId -match '%1' } | "
        "Select-Object Model, SerialNumber, HealthStatus, OperationalStatus | "
        "ConvertTo-Json"
    ));
    
    process->start("powershell", {"-Command", script});
#else
    (drivePath);
#endif
}

void EnhancedDynamicLoadBalancer::queryNVMeHealth(const std::string& drivePath)
{
#ifdef 
    // Query NVMe specific health via StorageReliabilityCounter
    void** process = new void*(this);
// Qt connect removed
                void* doc = void*::fromJson(output.toUtf8());
                if (!doc.isNull() && doc.isObject()) {
                    void* obj = doc.object();
                    
                    std::lock_guard<std::mutex> lock(&m_mutex);
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
    
    std::string script = std::string(
        "Get-PhysicalDisk | Where-Object { $_.DeviceId -match '%1' } | "
        "Get-StorageReliabilityCounter | "
        "Select-Object Temperature, Wear, ReadErrorsTotal, WriteErrorsTotal | "
        "ConvertTo-Json"
    ));
    
    process->start("powershell", {"-Command", script});
#else
    (drivePath);
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


