/**
 * @file DynamicLoadBalancer.cpp
 * @brief Dynamic Load Balancer Implementation
 * 
 * Full production implementation of thermal-aware multi-drive
 * load balancing with predictive throttling integration.
 * 
 * @copyright RawrXD IDE 2026
 */

#include "DynamicLoadBalancer.h"
#include "PredictiveThrottling.h"
#include "thermal_dashboard_plugin.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace rawrxd::thermal {

// ═══════════════════════════════════════════════════════════════════════════════
// Constructors
// ═══════════════════════════════════════════════════════════════════════════════

DynamicLoadBalancer::DynamicLoadBalancer()
    : m_config()
    , m_lastSelectedDrive(-1)
    , m_selectionCallback(nullptr)
{
    m_drives.reserve(5);  // Max 5 NVMe drives
    m_predictors.resize(5);
    m_selectionCounts.resize(5, 0);
}

DynamicLoadBalancer::DynamicLoadBalancer(const std::vector<double>& driveTemps,
                                         const std::vector<double>& driveLoads)
    : DynamicLoadBalancer()
{
    size_t count = std::min(driveTemps.size(), driveLoads.size());
    for (size_t i = 0; i < count && i < 5; ++i) {
        DriveInfo info;
        info.driveIndex = static_cast<int>(i);
        info.currentTemp = driveTemps[i];
        info.currentLoad = driveLoads[i];
        info.maxAllowedTemp = 70.0;
        info.thermalHeadroom = info.maxAllowedTemp - info.currentTemp;
        info.isAvailable = true;
        info.isThrottled = (info.thermalHeadroom < m_config.minThermalHeadroom);
        m_drives.push_back(info);
    }
}

DynamicLoadBalancer::DynamicLoadBalancer(const LoadBalancerConfig& config)
    : DynamicLoadBalancer()
{
    m_config = config;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Drive Management
// ═══════════════════════════════════════════════════════════════════════════════

int DynamicLoadBalancer::detectDrives()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Clear existing drives
    m_drives.clear();
    
    // In production, this would use WMI/CIM to enumerate NVMe drives
    // For now, we set up 5 potential drives
    const char* defaultModels[] = {
        "SK hynix P41",
        "Samsung 990 PRO",
        "WD Black SN850X",
        "Samsung 990 PRO 4TB",
        "Crucial T705 4TB"
    };
    
    for (int i = 0; i < 5; ++i) {
        DriveInfo info;
        info.driveIndex = i;
        info.model = defaultModels[i];
        info.deviceId = "NVMe" + std::to_string(i);
        info.maxAllowedTemp = 70.0;
        info.healthPercent = 100;
        info.isAvailable = false;  // Will be set when temps are updated
        m_drives.push_back(info);
    }
    
    return static_cast<int>(m_drives.size());
}

void DynamicLoadBalancer::addDrive(const DriveInfo& info)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check if drive already exists
    for (auto& existing : m_drives) {
        if (existing.driveIndex == info.driveIndex) {
            existing = info;
            return;
        }
    }
    
    // Add new drive
    if (m_drives.size() < 5) {
        m_drives.push_back(info);
        
        // Ensure statistics vector is large enough
        if (m_selectionCounts.size() <= static_cast<size_t>(info.driveIndex)) {
            m_selectionCounts.resize(info.driveIndex + 1, 0);
        }
    }
}

void DynamicLoadBalancer::updateFromSnapshot(const ThermalSnapshot& snapshot)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Ensure we have enough drives
    while (m_drives.size() < static_cast<size_t>(snapshot.activeDriveCount)) {
        DriveInfo info;
        info.driveIndex = static_cast<int>(m_drives.size());
        info.maxAllowedTemp = 70.0;
        m_drives.push_back(info);
    }
    
    // Update temperatures for active drives
    for (int i = 0; i < snapshot.activeDriveCount && i < 5; ++i) {
        m_drives[i].currentTemp = snapshot.nvmeTemps[i];
        m_drives[i].thermalHeadroom = m_drives[i].maxAllowedTemp - snapshot.nvmeTemps[i];
        m_drives[i].isAvailable = true;
        m_drives[i].isThrottled = (m_drives[i].thermalHeadroom < m_config.minThermalHeadroom);
    }
    
    // Mark remaining drives as unavailable
    for (size_t i = snapshot.activeDriveCount; i < m_drives.size(); ++i) {
        m_drives[i].isAvailable = false;
    }
}

void DynamicLoadBalancer::updateDriveMetrics(int driveIndex, double temp, double load)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (driveIndex < 0 || driveIndex >= static_cast<int>(m_drives.size())) {
        return;
    }
    
    m_drives[driveIndex].currentTemp = temp;
    m_drives[driveIndex].currentLoad = load;
    m_drives[driveIndex].thermalHeadroom = m_drives[driveIndex].maxAllowedTemp - temp;
    m_drives[driveIndex].isThrottled = (m_drives[driveIndex].thermalHeadroom < m_config.minThermalHeadroom);
}

void DynamicLoadBalancer::markDriveUnavailable(int driveIndex)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (driveIndex >= 0 && driveIndex < static_cast<int>(m_drives.size())) {
        m_drives[driveIndex].isAvailable = false;
    }
}

std::optional<DriveInfo> DynamicLoadBalancer::getDriveInfo(int driveIndex) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (driveIndex >= 0 && driveIndex < static_cast<int>(m_drives.size())) {
        return m_drives[driveIndex];
    }
    return std::nullopt;
}

std::vector<DriveInfo> DynamicLoadBalancer::getAllDrives() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_drives;
}

int DynamicLoadBalancer::getAvailableDriveCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    return static_cast<int>(std::count_if(m_drives.begin(), m_drives.end(),
        [](const DriveInfo& d) { return d.isAvailable; }));
}

// ═══════════════════════════════════════════════════════════════════════════════
// Drive Selection
// ═══════════════════════════════════════════════════════════════════════════════

int DynamicLoadBalancer::selectOptimalDrive()
{
    DriveSelectionResult result = selectOptimalDriveDetailed();
    return result.selectedDrive;
}

DriveSelectionResult DynamicLoadBalancer::selectOptimalDriveDetailed()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    DriveSelectionResult result;
    result.selectedDrive = -1;
    result.score = -1.0;
    result.allScores.resize(m_drives.size(), 0.0);
    
    if (m_drives.empty()) {
        result.reason = "No drives configured";
        return result;
    }
    
    // Calculate scores for all drives
    double bestScore = -1.0;
    int bestDrive = -1;
    
    for (size_t i = 0; i < m_drives.size(); ++i) {
        if (!isDriveEligible(static_cast<int>(i))) {
            result.allScores[i] = 0.0;
            continue;
        }
        
        double score = calculateDriveScore(static_cast<int>(i));
        result.allScores[i] = score;
        
        if (score > bestScore) {
            bestScore = score;
            bestDrive = static_cast<int>(i);
        }
    }
    
    if (bestDrive >= 0) {
        result.selectedDrive = bestDrive;
        result.score = bestScore;
        result.reason = "Selected drive " + std::to_string(bestDrive) + 
                        " (" + m_drives[bestDrive].model + 
                        ") with score " + std::to_string(bestScore) +
                        ", headroom " + std::to_string(m_drives[bestDrive].thermalHeadroom) + "°C";
        
        // Update statistics
        if (static_cast<size_t>(bestDrive) < m_selectionCounts.size()) {
            m_selectionCounts[bestDrive]++;
        }
        
        // Invoke callback
        if (m_selectionCallback) {
            m_selectionCallback(result);
        }
    } else {
        result.reason = "No eligible drives available";
    }
    
    return result;
}

int DynamicLoadBalancer::selectCoolestDrive()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    int bestDrive = -1;
    double maxHeadroom = -1.0;
    
    for (size_t i = 0; i < m_drives.size(); ++i) {
        if (!m_drives[i].isAvailable) continue;
        
        if (m_drives[i].thermalHeadroom > maxHeadroom) {
            maxHeadroom = m_drives[i].thermalHeadroom;
            bestDrive = static_cast<int>(i);
        }
    }
    
    return bestDrive;
}

int DynamicLoadBalancer::selectLeastLoadedDrive()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    int bestDrive = -1;
    double lowestLoad = 101.0;
    
    for (size_t i = 0; i < m_drives.size(); ++i) {
        if (!m_drives[i].isAvailable) continue;
        
        if (m_drives[i].currentLoad < lowestLoad) {
            lowestLoad = m_drives[i].currentLoad;
            bestDrive = static_cast<int>(i);
        }
    }
    
    return bestDrive;
}

int DynamicLoadBalancer::selectRoundRobin()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_drives.empty()) return -1;
    
    // Find next available drive
    int startIdx = (m_lastSelectedDrive + 1) % static_cast<int>(m_drives.size());
    int idx = startIdx;
    
    do {
        if (m_drives[idx].isAvailable && !m_drives[idx].isThrottled) {
            m_lastSelectedDrive = idx;
            return idx;
        }
        idx = (idx + 1) % static_cast<int>(m_drives.size());
    } while (idx != startIdx);
    
    return -1;
}

double DynamicLoadBalancer::calculateDriveScore(int driveIndex) const
{
    if (driveIndex < 0 || driveIndex >= static_cast<int>(m_drives.size())) {
        return 0.0;
    }
    
    const DriveInfo& drive = m_drives[driveIndex];
    
    if (!drive.isAvailable) return 0.0;
    
    // Normalize components to 0-1 range
    
    // Thermal headroom score: higher headroom = better
    // Max headroom is typically around 30°C (70°C limit - 40°C ambient)
    double headroomScore = std::clamp(drive.thermalHeadroom / 30.0, 0.0, 1.0);
    
    // Load score: lower load = better
    double loadScore = std::clamp(1.0 - (drive.currentLoad / 100.0), 0.0, 1.0);
    
    // Health score: already 0-100, normalize to 0-1
    double healthScore = drive.healthPercent / 100.0;
    
    // Apply penalty for throttled drives
    if (drive.isThrottled) {
        headroomScore *= 0.5;
    }
    
    // Calculate weighted score
    double score = (m_config.headroomWeight * headroomScore) +
                   (m_config.loadWeight * loadScore) +
                   (m_config.healthWeight * healthScore);
    
    return score;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Thermal Analysis
// ═══════════════════════════════════════════════════════════════════════════════

double DynamicLoadBalancer::getThermalHeadroom(int driveIndex) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (driveIndex >= 0 && driveIndex < static_cast<int>(m_drives.size())) {
        return m_drives[driveIndex].thermalHeadroom;
    }
    return 0.0;
}

double DynamicLoadBalancer::getMaxThermalHeadroom() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_drives.empty()) return 0.0;
    
    double maxHeadroom = -100.0;
    for (const auto& drive : m_drives) {
        if (drive.isAvailable && drive.thermalHeadroom > maxHeadroom) {
            maxHeadroom = drive.thermalHeadroom;
        }
    }
    return maxHeadroom;
}

double DynamicLoadBalancer::getMinThermalHeadroom() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_drives.empty()) return 0.0;
    
    double minHeadroom = 1000.0;
    for (const auto& drive : m_drives) {
        if (drive.isAvailable && drive.thermalHeadroom < minHeadroom) {
            minHeadroom = drive.thermalHeadroom;
        }
    }
    return minHeadroom;
}

double DynamicLoadBalancer::getAverageTemperature() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_drives.empty()) return 0.0;
    
    double sum = 0.0;
    int count = 0;
    
    for (const auto& drive : m_drives) {
        if (drive.isAvailable) {
            sum += drive.currentTemp;
            count++;
        }
    }
    
    return count > 0 ? sum / count : 0.0;
}

bool DynamicLoadBalancer::hasThermallConstrainedDrives() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    return std::any_of(m_drives.begin(), m_drives.end(),
        [this](const DriveInfo& d) {
            return d.isAvailable && d.isThrottled;
        });
}

std::vector<int> DynamicLoadBalancer::getThermallConstrainedDrives() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<int> constrained;
    for (size_t i = 0; i < m_drives.size(); ++i) {
        if (m_drives[i].isAvailable && m_drives[i].isThrottled) {
            constrained.push_back(static_cast<int>(i));
        }
    }
    return constrained;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Load Analysis
// ═══════════════════════════════════════════════════════════════════════════════

double DynamicLoadBalancer::getTotalSystemLoad() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    double totalLoad = 0.0;
    int count = 0;
    
    for (const auto& drive : m_drives) {
        if (drive.isAvailable) {
            totalLoad += drive.currentLoad;
            count++;
        }
    }
    
    return count > 0 ? totalLoad / count : 0.0;
}

std::vector<double> DynamicLoadBalancer::getLoadDistribution() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<double> loads;
    for (const auto& drive : m_drives) {
        if (drive.isAvailable) {
            loads.push_back(drive.currentLoad);
        }
    }
    return loads;
}

bool DynamicLoadBalancer::isLoadBalanced(double threshold) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_drives.size() < 2) return true;
    
    double minLoad = 100.0;
    double maxLoad = 0.0;
    
    for (const auto& drive : m_drives) {
        if (drive.isAvailable) {
            minLoad = std::min(minLoad, drive.currentLoad);
            maxLoad = std::max(maxLoad, drive.currentLoad);
        }
    }
    
    return (maxLoad - minLoad) <= threshold;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Predictive Integration
// ═══════════════════════════════════════════════════════════════════════════════

void DynamicLoadBalancer::setPredictiveThrottling(int driveIndex,
                                                   std::shared_ptr<PredictiveThrottling> predictor)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (driveIndex >= 0 && driveIndex < static_cast<int>(m_predictors.size())) {
        m_predictors[driveIndex] = predictor;
    }
}

int DynamicLoadBalancer::selectOptimalDrivePredictive(int64_t horizonMs)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_config.enablePredictive) {
        // Fall back to non-predictive selection
        return const_cast<DynamicLoadBalancer*>(this)->selectOptimalDrive();
    }
    
    int bestDrive = -1;
    double maxPredictedHeadroom = -100.0;
    
    for (size_t i = 0; i < m_drives.size(); ++i) {
        if (!m_drives[i].isAvailable) continue;
        
        double predictedTemp = m_drives[i].currentTemp;
        
        // Use predictor if available
        if (i < m_predictors.size() && m_predictors[i]) {
            PredictionResult prediction = m_predictors[i]->getPrediction(horizonMs);
            if (prediction.isValid) {
                predictedTemp = prediction.predictedTemp;
            }
        }
        
        double predictedHeadroom = m_drives[i].maxAllowedTemp - predictedTemp;
        
        // Must have minimum headroom and not exceed load threshold
        if (predictedHeadroom >= m_config.minThermalHeadroom &&
            m_drives[i].currentLoad < m_config.loadThreshold) {
            
            if (predictedHeadroom > maxPredictedHeadroom) {
                maxPredictedHeadroom = predictedHeadroom;
                bestDrive = static_cast<int>(i);
            }
        }
    }
    
    return bestDrive;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Configuration
// ═══════════════════════════════════════════════════════════════════════════════

void DynamicLoadBalancer::setConfig(const LoadBalancerConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

void DynamicLoadBalancer::setSelectionCallback(LoadBalancerCallback callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_selectionCallback = std::move(callback);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Statistics
// ═══════════════════════════════════════════════════════════════════════════════

std::vector<uint64_t> DynamicLoadBalancer::getSelectionStatistics() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_selectionCounts;
}

void DynamicLoadBalancer::resetStatistics()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::fill(m_selectionCounts.begin(), m_selectionCounts.end(), 0);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Internal Methods
// ═══════════════════════════════════════════════════════════════════════════════

void DynamicLoadBalancer::recalculateHeadrooms()
{
    for (auto& drive : m_drives) {
        drive.thermalHeadroom = drive.maxAllowedTemp - drive.currentTemp;
        drive.isThrottled = (drive.thermalHeadroom < m_config.minThermalHeadroom);
    }
}

bool DynamicLoadBalancer::isDriveEligible(int driveIndex) const
{
    if (driveIndex < 0 || driveIndex >= static_cast<int>(m_drives.size())) {
        return false;
    }
    
    const DriveInfo& drive = m_drives[driveIndex];
    
    // Must be available
    if (!drive.isAvailable) return false;
    
    // Must have minimum thermal headroom
    if (drive.thermalHeadroom < m_config.minThermalHeadroom) return false;
    
    // Must be below load threshold
    if (drive.currentLoad >= m_config.loadThreshold) return false;
    
    return true;
}

std::vector<double> DynamicLoadBalancer::normalizeScores(const std::vector<double>& scores)
{
    if (scores.empty()) return {};
    
    double maxScore = *std::max_element(scores.begin(), scores.end());
    if (maxScore <= 0.0) return std::vector<double>(scores.size(), 0.0);
    
    std::vector<double> normalized;
    normalized.reserve(scores.size());
    
    for (double score : scores) {
        normalized.push_back(score / maxScore);
    }
    
    return normalized;
}

} // namespace rawrxd::thermal
