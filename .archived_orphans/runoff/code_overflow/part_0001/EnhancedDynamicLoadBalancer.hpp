/**
 * @file EnhancedDynamicLoadBalancer.hpp
 * @brief Health-Aware Dynamic Load Balancer with SMART Metrics
 * 
 * Enhanced load balancer that incorporates drive health metrics
 * (SMART data, TBW, bad sectors) alongside thermal and load data.
 * 
 * @copyright RawrXD IDE 2026
 */

#pragma once


#include <vector>
#include <optional>
#include <memory>
#include <atomic>
#include <functional>
#include <chrono>
#include <map>
#include <string>
#include <mutex>

namespace rawrxd::thermal {

// ═══════════════════════════════════════════════════════════════════════════════
// Health Metrics Structures
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief SMART (Self-Monitoring, Analysis and Reporting Technology) data
 */
struct SMARTData {
    // Critical SMART attributes
    int rawReadErrorRate = 0;           // ID 1
    int reallocatedSectorCount = 0;     // ID 5  - Critical
    int seekErrorRate = 0;              // ID 7
    int powerOnHours = 0;               // ID 9
    int spinRetryCount = 0;             // ID 10
    int reallocatedEventCount = 0;      // ID 196 - Critical
    int currentPendingSectorCount = 0;  // ID 197 - Critical
    int uncorrectableSectorCount = 0;   // ID 198 - Critical
    
    // NVMe specific
    int availableSpare = 100;           // Percentage
    int availableSpareThreshold = 10;
    int percentageUsed = 0;
    int64_t dataUnitsWritten = 0;       // For TBW calculation
    int64_t dataUnitsRead = 0;
    int powerCycles = 0;
    int unsafeShutdowns = 0;
    int mediaErrors = 0;                // Critical
    int numErrLogEntries = 0;
    
    // Calculated scores
    double overallHealth = 100.0;       // 0-100 score
    bool isHealthy = true;
    bool isCritical = false;
    
    std::string lastUpdated;
    
    /**
     * @brief Calculate overall health score from SMART attributes
     */
    void calculateHealth() {
        overallHealth = 100.0;
        
        // Deductions for HDD-style SMART
        if (reallocatedSectorCount > 0) {
            overallHealth -= std::min(50.0, reallocatedSectorCount * 5.0);
        }
        if (currentPendingSectorCount > 0) {
            overallHealth -= std::min(30.0, currentPendingSectorCount * 3.0);
        }
        if (uncorrectableSectorCount > 0) {
            overallHealth -= std::min(40.0, uncorrectableSectorCount * 4.0);
        }
        if (reallocatedEventCount > 10) {
            overallHealth -= std::min(20.0, (reallocatedEventCount - 10) * 2.0);
        }
        
        // NVMe specific
        overallHealth = std::min(overallHealth, static_cast<double>(availableSpare));
        if (mediaErrors > 0) {
            overallHealth -= std::min(30.0, mediaErrors * 10.0);
        }
        if (percentageUsed > 80) {
            overallHealth -= (percentageUsed - 80) * 0.5;
        }
        
        overallHealth = std::max(0.0, overallHealth);
        isHealthy = overallHealth >= 50.0;
        isCritical = overallHealth < 20.0;
    }
};

/**
 * @brief Terabytes Written (TBW) tracking
 */
struct TBWData {
    double totalTBW = 0.0;              // Total TBW since manufacture
    double ratedTBW = 0.0;              // Manufacturer rated TBW
    double remainingTBW = 0.0;          // Estimated remaining
    double dailyWriteRate = 0.0;        // Average daily write rate (GB)
    int estimatedDaysRemaining = 0;     // Estimated lifespan in days
    double wearLevel = 0.0;             // 0-100% of rated TBW used
    
    void calculate() {
        if (ratedTBW > 0) {
            wearLevel = (totalTBW / ratedTBW) * 100.0;
            remainingTBW = std::max(0.0, ratedTBW - totalTBW);
            
            if (dailyWriteRate > 0) {
                estimatedDaysRemaining = static_cast<int>((remainingTBW * 1024.0) / dailyWriteRate);
            }
        }
    }
};

/**
 * @brief Bad sector tracking
 */
struct BadSectorData {
    int64_t totalSectors = 0;
    int64_t badSectors = 0;
    int64_t pendingSectors = 0;
    int64_t reallocatedSectors = 0;
    double badSectorPercentage = 0.0;
    bool hasGrowingBadSectors = false;
    
    // Historical tracking
    int64_t badSectorsLastWeek = 0;
    int64_t badSectorGrowthRate = 0;    // Per day
    
    void calculate() {
        if (totalSectors > 0) {
            badSectorPercentage = (static_cast<double>(badSectors + pendingSectors + reallocatedSectors) 
                                   / totalSectors) * 100.0;
        }
        
        hasGrowingBadSectors = badSectorGrowthRate > 0;
    }
};

/**
 * @brief Complete drive health profile
 */
struct DriveHealthProfile {
    std::string drivePath;
    std::string driveModel;
    std::string serialNumber;
    std::string firmwareVersion;
    
    // Health data
    SMARTData smart;
    TBWData tbw;
    BadSectorData badSectors;
    
    // Performance data (from existing system)
    double currentTemperature = 0.0;
    double currentLoad = 0.0;
    int64_t bytesReadPerSec = 0;
    int64_t bytesWrittenPerSec = 0;
    double averageLatency = 0.0;
    int queueDepth = 0;
    
    // Composite scores (0.0 - 1.0)
    double healthScore = 1.0;           // 20% weight
    double thermalScore = 1.0;          // 50% weight  
    double loadScore = 1.0;             // 30% weight
    double compositeScore = 1.0;
    
    // Status
    bool isOnline = true;
    bool isThrottled = false;
    std::string statusMessage;
    
    int64_t lastUpdateMs = 0;
    
    /**
     * @brief Calculate composite score with configurable weights
     */
    void calculateComposite(double thermalWeight = 0.50, 
                           double loadWeight = 0.30,
                           double healthWeight = 0.20) {
        // Normalize health score (SMART gives 0-100, we need 0-1)
        healthScore = smart.overallHealth / 100.0;
        
        // Thermal score (65°C is warning, 85°C is critical)
        if (currentTemperature < 45.0) {
            thermalScore = 1.0;
        } else if (currentTemperature < 65.0) {
            thermalScore = 1.0 - (currentTemperature - 45.0) / 40.0 * 0.3;
        } else if (currentTemperature < 85.0) {
            thermalScore = 0.7 - (currentTemperature - 65.0) / 20.0 * 0.5;
        } else {
            thermalScore = 0.2;
        }
        
        // Load score (inverse - lower load is better)
        loadScore = 1.0 - (currentLoad / 100.0);
        
        // Calculate composite
        compositeScore = thermalWeight * thermalScore +
                        loadWeight * loadScore +
                        healthWeight * healthScore;
        
        // Critical overrides
        if (smart.isCritical) {
            compositeScore *= 0.3;  // Heavily penalize critical drives
            statusMessage = "⚠️ CRITICAL: SMART failure imminent";
        } else if (currentTemperature >= 85.0) {
            compositeScore *= 0.5;
            statusMessage = "🔥 THERMAL: Emergency throttle active";
        } else if (!isOnline) {
            compositeScore = 0.0;
            statusMessage = "❌ OFFLINE: Drive not responding";
        } else {
            statusMessage = std::string("Score: %1%"));
        }
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Configuration
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Enhanced load balancer configuration (for health-aware balancer)
 */
struct EnhancedLoadBalancerConfig {
    // Scoring weights (must sum to 1.0)
    double thermalWeight = 0.50;
    double loadWeight = 0.30;
    double healthWeight = 0.20;
    
    // Thresholds
    double minHealthScore = 0.20;       // Below this, exclude drive
    double thermalWarning = 65.0;       // °C
    double thermalCritical = 85.0;      // °C
    double maxLoadPercent = 90.0;       // Above this, deprioritize
    
    // Behavior
    bool enableHealthMonitoring = true;
    bool enablePredictiveBalancing = true;
    bool enableAutoExclusion = true;
    int healthPollIntervalMs = 60000;   // 1 minute
    int thermalPollIntervalMs = 5000;   // 5 seconds
    
    // Redundancy
    int minHealthyDrives = 1;           // Minimum drives required
    bool allowDegradedOperation = true;
    
    // Hysteresis to prevent oscillation
    double hysteresisThreshold = 0.05;  // 5% difference required to switch
    std::string lastSelectedDrive;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Callbacks
// ═══════════════════════════════════════════════════════════════════════════════

using DriveSelectedCallback = std::function<void(const std::string& drivePath, const DriveHealthProfile& profile)>;
using HealthAlertCallback = std::function<void(const std::string& drivePath, const std::string& alert, bool isCritical)>;
using BalancingEventCallback = std::function<void(const std::string& fromDrive, const std::string& toDrive, const std::string& reason)>;

// ═══════════════════════════════════════════════════════════════════════════════
// Enhanced Dynamic Load Balancer
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @class EnhancedDynamicLoadBalancer
 * @brief Health-aware thermal load balancer for multi-drive systems
 * 
 * Extends basic thermal load balancing with comprehensive health metrics
 * including SMART data, TBW tracking, and bad sector monitoring.
 * 
 * Example usage:
 * @code
 * EnhancedDynamicLoadBalancer balancer;
 * balancer.addDrive("D:", 4000.0);  // 4TB rated TBW
 * balancer.addDrive("E:", 3000.0);
 * 
 * // Update health from WMI/SMART tools
 * DriveHealthProfile profile;
 * profile.drivePath = "D:";
 * profile.smart.overallHealth = 95.0;
 * profile.currentTemperature = 42.0;
 * balancer.updateHealth("D:", profile);
 * 
 * // Get best drive for write operation
 * auto selected = balancer.selectOptimalDrive(OperationType::Write);
 * @endcode
 */
class EnhancedDynamicLoadBalancer
{


public:
    enum class OperationType {
        Read,
        Write,
        Sequential,
        Random,
        LargeFile,
        SmallFiles
    };

    explicit EnhancedDynamicLoadBalancer(void* parent = nullptr);
    ~EnhancedDynamicLoadBalancer() override;

    // ═══════════════════════════════════════════════════════════════════════════
    // Configuration
    // ═══════════════════════════════════════════════════════════════════════════
    
    void setConfig(const EnhancedLoadBalancerConfig& config);
    EnhancedLoadBalancerConfig getConfig() const;
    
    void setWeights(double thermalWeight, double loadWeight, double healthWeight);
    void setThresholds(double minHealth, double thermalWarning, double thermalCritical);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Drive Management
    // ═══════════════════════════════════════════════════════════════════════════
    
    void addDrive(const std::string& drivePath, double ratedTBW = 0.0);
    void removeDrive(const std::string& drivePath);
    void clearDrives();
    
    int getDriveCount() const;
    std::vector<std::string> getDrivePaths() const;
    bool hasDrive(const std::string& drivePath) const;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Health Updates
    // ═══════════════════════════════════════════════════════════════════════════
    
    void updateHealth(const std::string& drivePath, const DriveHealthProfile& profile);
    void updateTemperature(const std::string& drivePath, double temperature);
    void updateLoad(const std::string& drivePath, double loadPercent);
    void updateSMART(const std::string& drivePath, const SMARTData& smart);
    void updateTBW(const std::string& drivePath, double totalTBW);
    
    DriveHealthProfile getHealthProfile(const std::string& drivePath) const;
    std::vector<DriveHealthProfile> getAllProfiles() const;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Drive Selection
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Select optimal drive for given operation type
     * @param opType Type of operation (read, write, etc.)
     * @return Drive path or empty string if no suitable drive
     */
    std::string selectOptimalDrive(OperationType opType = OperationType::Write);
    
    /**
     * @brief Get ranked list of drives for operation
     * @param opType Type of operation
     * @return Vector of drive paths sorted by composite score (best first)
     */
    std::vector<std::string> getRankedDrives(OperationType opType = OperationType::Write);
    
    /**
     * @brief Get last selected drive path
     */
    std::string getLastSelectedDrive() const;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Monitoring
    // ═══════════════════════════════════════════════════════════════════════════
    
    void startMonitoring();
    void stopMonitoring();
    bool isMonitoring() const;
    
    void refreshHealthData();
    void refreshThermalData();
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Callbacks
    // ═══════════════════════════════════════════════════════════════════════════
    
    void setDriveSelectedCallback(DriveSelectedCallback callback);
    void setHealthAlertCallback(HealthAlertCallback callback);
    void setBalancingEventCallback(BalancingEventCallback callback);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Statistics
    // ═══════════════════════════════════════════════════════════════════════════
    
    double getAverageHealth() const;
    double getAverageTemperature() const;
    double getAverageLoad() const;
    int getHealthyDriveCount() const;
    int getCriticalDriveCount() const;
    
    std::map<std::string, std::string> getStatistics() const;
    std::map<std::string, std::string> getDriveStatistics(const std::string& drivePath) const;

    void driveSelected(const std::string& drivePath);
    void drivesChanged();
    void healthUpdated(const std::string& drivePath, double healthScore);
    void healthAlert(const std::string& drivePath, const std::string& message, bool isCritical);
    void balancingEvent(const std::string& fromDrive, const std::string& toDrive, const std::string& reason);

private:
    void onHealthPollTimer();
    void onThermalPollTimer();

private:
    // Internal helpers
    void recalculateScores();
    void checkHealthAlerts(const std::string& drivePath, const DriveHealthProfile& profile);
    double adjustScoreForOperation(double baseScore, const DriveHealthProfile& profile, OperationType opType);
    void queryWMIHealth(const std::string& drivePath);
    void queryNVMeHealth(const std::string& drivePath);
    int64_t getCurrentTimestampMs() const;
    
    // Data
    mutable std::mutex m_mutex;
    std::map<std::string, DriveHealthProfile> m_drives;
    EnhancedLoadBalancerConfig m_config;
    
    // Monitoring
    std::unique_ptr<void*> m_healthTimer;
    std::unique_ptr<void*> m_thermalTimer;
    std::atomic<bool> m_monitoring{false};
    
    // Callbacks
    DriveSelectedCallback m_driveSelectedCallback;
    HealthAlertCallback m_healthAlertCallback;
    BalancingEventCallback m_balancingEventCallback;
    
    // State
    std::string m_lastSelectedDrive;
    std::map<std::string, double> m_previousScores;  // For hysteresis
};

} // namespace rawrxd::thermal

