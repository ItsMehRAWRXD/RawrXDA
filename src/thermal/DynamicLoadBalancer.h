/**
 * @file DynamicLoadBalancer.h
 * @brief Intelligent Multi-Drive Load Balancer with Thermal Awareness
 * 
 * Implements thermal headroom-based drive selection for optimal
 * performance distribution across multiple NVMe drives.
 * 
 * @copyright RawrXD IDE 2026
 */

#pragma once

#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <functional>
#include <optional>
#include <chrono>
#include <cstdint>

namespace rawrxd::thermal {

// Forward declarations
struct ThermalSnapshot;
class PredictiveThrottling;

/**
 * @brief Drive information and status
 */
struct DriveInfo {
    int driveIndex;                 // Drive index (0-4)
    std::string deviceId;           // Device identifier
    std::string model;              // Drive model name
    uint64_t capacityBytes;         // Total capacity
    uint64_t freeBytes;             // Free space
    
    // Thermal state
    double currentTemp;             // Current temperature (°C)
    double maxAllowedTemp;          // Maximum allowed operating temp
    double thermalHeadroom;         // Distance from max temp
    
    // Load state
    double currentLoad;             // Current load percentage (0-100)
    double queueDepth;              // Current I/O queue depth
    uint64_t bytesPerSecond;        // Current throughput
    
    // Health state
    int healthPercent;              // Drive health (0-100)
    bool isAvailable;               // Drive is ready for I/O
    bool isThrottled;               // Drive is thermally throttled
    
    DriveInfo()
        : driveIndex(-1), capacityBytes(0), freeBytes(0)
        , currentTemp(0), maxAllowedTemp(70.0), thermalHeadroom(0)
        , currentLoad(0), queueDepth(0), bytesPerSecond(0)
        , healthPercent(100), isAvailable(false), isThrottled(false)
    {}
};

/**
 * @brief Load balancer configuration
 */
struct LoadBalancerConfig {
    double thermalThreshold = 60.0;     // Temperature threshold (°C)
    double loadThreshold = 90.0;        // Maximum load before avoiding drive
    double minThermalHeadroom = 5.0;    // Minimum acceptable headroom (°C)
    double headroomWeight = 0.5;        // Weight for thermal headroom in selection
    double loadWeight = 0.3;            // Weight for current load in selection
    double healthWeight = 0.2;          // Weight for health in selection
    int64_t updateIntervalMs = 1000;    // How often to refresh metrics
    bool enablePredictive = true;       // Use predictive throttling
    bool preferCoolestDrive = true;     // Prefer drive with most thermal headroom
};

/**
 * @brief Selection result with reasoning
 */
struct DriveSelectionResult {
    int selectedDrive;              // Index of selected drive (-1 if none)
    double score;                   // Selection score
    std::string reason;             // Human-readable reason for selection
    std::vector<double> allScores;  // Scores for all drives
    bool isPredictive;              // Selection used predictive data
    
    DriveSelectionResult()
        : selectedDrive(-1), score(0.0), isPredictive(false)
    {}
};

/**
 * @brief Callback for load balancer events
 */
using LoadBalancerCallback = std::function<void(const DriveSelectionResult&)>;

/**
 * @brief Dynamic Multi-Drive Load Balancer
 * 
 * Intelligently distributes I/O load across multiple NVMe drives
 * based on thermal headroom, current load, and health metrics.
 */
class DynamicLoadBalancer {
public:
    /**
     * @brief Construct with default configuration
     */
    DynamicLoadBalancer();
    
    /**
     * @brief Construct with drive temperatures and loads
     * @param driveTemps Vector of drive temperatures
     * @param driveLoads Vector of drive load percentages
     */
    DynamicLoadBalancer(const std::vector<double>& driveTemps,
                        const std::vector<double>& driveLoads);
    
    /**
     * @brief Construct with custom configuration
     * @param config Load balancer configuration
     */
    explicit DynamicLoadBalancer(const LoadBalancerConfig& config);
    
    ~DynamicLoadBalancer() = default;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Drive Management
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Initialize drives from system
     * @return Number of drives detected
     */
    int detectDrives();
    
    /**
     * @brief Add a drive manually
     * @param info Drive information
     */
    void addDrive(const DriveInfo& info);
    
    /**
     * @brief Update drive state from thermal snapshot
     * @param snapshot Thermal snapshot with temperatures
     */
    void updateFromSnapshot(const ThermalSnapshot& snapshot);
    
    /**
     * @brief Update a specific drive's metrics
     * @param driveIndex Drive index
     * @param temp Current temperature
     * @param load Current load percentage
     */
    void updateDriveMetrics(int driveIndex, double temp, double load);
    
    /**
     * @brief Mark a drive as unavailable
     * @param driveIndex Drive index
     */
    void markDriveUnavailable(int driveIndex);
    
    /**
     * @brief Get drive info by index
     * @param driveIndex Drive index
     * @return Drive info or empty optional
     */
    std::optional<DriveInfo> getDriveInfo(int driveIndex) const;
    
    /**
     * @brief Get all drive infos
     * @return Vector of all drive infos
     */
    std::vector<DriveInfo> getAllDrives() const;
    
    /**
     * @brief Get number of available drives
     * @return Number of drives that are available for I/O
     */
    int getAvailableDriveCount() const;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Drive Selection
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Select optimal drive for next I/O operation
     * @return Index of optimal drive (-1 if none available)
     */
    int selectOptimalDrive();
    
    /**
     * @brief Select optimal drive with detailed result
     * @return Selection result with reasoning
     */
    DriveSelectionResult selectOptimalDriveDetailed();
    
    /**
     * @brief Select drive with highest thermal headroom
     * @return Drive index or -1
     */
    int selectCoolestDrive();
    
    /**
     * @brief Select drive with lowest load
     * @return Drive index or -1
     */
    int selectLeastLoadedDrive();
    
    /**
     * @brief Select drive using round-robin
     * @return Drive index or -1
     */
    int selectRoundRobin();
    
    /**
     * @brief Calculate selection score for a drive
     * @param driveIndex Drive index
     * @return Score (higher = better)
     */
    double calculateDriveScore(int driveIndex) const;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Thermal Analysis
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Get thermal headroom for a drive
     * @param driveIndex Drive index
     * @return Thermal headroom in °C
     */
    double getThermalHeadroom(int driveIndex) const;
    
    /**
     * @brief Get maximum thermal headroom across all drives
     * @return Maximum headroom
     */
    double getMaxThermalHeadroom() const;
    
    /**
     * @brief Get minimum thermal headroom across all drives
     * @return Minimum headroom
     */
    double getMinThermalHeadroom() const;
    
    /**
     * @brief Get average temperature across all drives
     * @return Average temperature
     */
    double getAverageTemperature() const;
    
    /**
     * @brief Check if any drive is thermally constrained
     * @return True if any drive is near or at thermal limit
     */
    bool hasThermallConstrainedDrives() const;
    
    /**
     * @brief Get list of thermally constrained drive indices
     * @return Vector of drive indices
     */
    std::vector<int> getThermallConstrainedDrives() const;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Load Analysis
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Get total system load across all drives
     * @return Aggregate load percentage
     */
    double getTotalSystemLoad() const;
    
    /**
     * @brief Get load distribution as vector
     * @return Vector of load percentages
     */
    std::vector<double> getLoadDistribution() const;
    
    /**
     * @brief Check if load is balanced
     * @param threshold Maximum acceptable variance
     * @return True if load is balanced
     */
    bool isLoadBalanced(double threshold = 20.0) const;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Predictive Integration
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Set predictive throttling instance for each drive
     * @param driveIndex Drive index
     * @param predictor Predictive throttling instance
     */
    void setPredictiveThrottling(int driveIndex, std::shared_ptr<PredictiveThrottling> predictor);
    
    /**
     * @brief Get predicted optimal drive using thermal predictions
     * @param horizonMs Prediction horizon in milliseconds
     * @return Drive index or -1
     */
    int selectOptimalDrivePredictive(int64_t horizonMs = 5000);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Configuration
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Get current configuration
     * @return Configuration reference
     */
    const LoadBalancerConfig& config() const { return m_config; }
    
    /**
     * @brief Set configuration
     * @param config New configuration
     */
    void setConfig(const LoadBalancerConfig& config);
    
    /**
     * @brief Set callback for selection events
     * @param callback Callback function
     */
    void setSelectionCallback(LoadBalancerCallback callback);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Statistics
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Get selection statistics
     * @return Map of drive index to selection count
     */
    std::vector<uint64_t> getSelectionStatistics() const;
    
    /**
     * @brief Reset selection statistics
     */
    void resetStatistics();

private:
    // ═══════════════════════════════════════════════════════════════════════════
    // Internal Methods
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Recalculate thermal headroom for all drives
     */
    void recalculateHeadrooms();
    
    /**
     * @brief Check if a drive meets minimum requirements
     * @param driveIndex Drive index
     * @return True if drive is eligible for selection
     */
    bool isDriveEligible(int driveIndex) const;
    
    /**
     * @brief Normalize scores to 0-1 range
     * @param scores Raw scores
     * @return Normalized scores
     */
    static std::vector<double> normalizeScores(const std::vector<double>& scores);

private:
    // ═══════════════════════════════════════════════════════════════════════════
    // Member Variables
    // ═══════════════════════════════════════════════════════════════════════════
    
    LoadBalancerConfig m_config;
    std::vector<DriveInfo> m_drives;
    std::vector<std::shared_ptr<PredictiveThrottling>> m_predictors;
    
    // Round-robin state
    int m_lastSelectedDrive;
    
    // Statistics
    std::vector<uint64_t> m_selectionCounts;
    
    // Callback
    LoadBalancerCallback m_selectionCallback;
    
    // Thread safety
    mutable std::mutex m_mutex;
};

} // namespace rawrxd::thermal
