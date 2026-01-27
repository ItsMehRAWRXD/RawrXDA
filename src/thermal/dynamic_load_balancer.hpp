/**
 * @file dynamic_load_balancer.hpp
 * @brief Dynamic NVMe Load Balancer with Thermal Awareness
 * 
 * Distributes I/O operations across NVMe drives based on:
 * - Thermal headroom (prioritize cooler drives)
 * - Current load (avoid overloaded drives)
 * - Drive performance characteristics
 * - JIT-LBA pattern detection
 */

#pragma once

#include "predictive_throttling.hpp"
#include <array>
#include <atomic>
#include <mutex>
#include <vector>
#include <algorithm>
#include <cstdint>

namespace rawrxd::thermal {

// ═══════════════════════════════════════════════════════════════════════════════
// Configuration
// ═══════════════════════════════════════════════════════════════════════════════

constexpr int MAX_NVME_DRIVES = 5;
constexpr double THERMAL_HEADROOM_WEIGHT = 0.4;
constexpr double LOAD_WEIGHT = 0.35;
constexpr double PERF_WEIGHT = 0.25;

// ═══════════════════════════════════════════════════════════════════════════════
// Drive Information
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Individual NVMe drive status and metrics
 */
struct NVMeDriveInfo {
    int driveIndex;             // 0-4
    double temperature;         // Current temp in °C
    double thermalHeadroom;     // Degrees below limit
    double loadPercent;         // Current I/O load 0-100
    double perfScore;           // Performance score 0-100
    uint64_t totalBytesRead;    // Lifetime bytes read
    uint64_t totalBytesWritten; // Lifetime bytes written
    uint64_t pendingOps;        // Current pending I/O ops
    bool isHealthy;             // SMART health status
    bool isThrottled;           // Currently thermal throttled
    
    // Calculated score for load balancing (higher = better choice)
    double balanceScore;
};

/**
 * @brief Load balancing decision result
 */
struct LoadBalanceResult {
    int selectedDrive;          // Index of selected drive
    double confidence;          // Confidence in selection 0-1
    const char* reason;         // Human-readable reason
    bool shouldSplit;           // True if operation should be split across drives
    int splitDrives[3];         // If splitting, which drives to use
    int splitCount;             // Number of drives to split across
};

// ═══════════════════════════════════════════════════════════════════════════════
// Dynamic Load Balancer
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Thermal-aware dynamic load balancer for NVMe RAID/stripe
 */
class DynamicLoadBalancer {
public:
    DynamicLoadBalancer()
        : m_activeDriveCount(0)
        , m_thermalLimit(65.0)
        , m_predictiveThrottling(nullptr)
    {
        for (int i = 0; i < MAX_NVME_DRIVES; ++i) {
            m_drives[i] = {};
            m_drives[i].driveIndex = i;
            m_drives[i].isHealthy = true;
            m_drives[i].perfScore = 100.0;
        }
    }
    
    /**
     * @brief Set the predictive throttling engine for shared data
     */
    void setPredictiveThrottling(PredictiveThrottling* pt) {
        m_predictiveThrottling = pt;
    }
    
    /**
     * @brief Set number of active drives
     */
    void setActiveDriveCount(int count) {
        m_activeDriveCount = std::min(count, MAX_NVME_DRIVES);
    }
    
    /**
     * @brief Update drive temperatures
     */
    void updateTemperatures(const double* temps, int count) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        for (int i = 0; i < count && i < MAX_NVME_DRIVES; ++i) {
            m_drives[i].temperature = temps[i];
            m_drives[i].thermalHeadroom = m_thermalLimit - temps[i];
            m_drives[i].isThrottled = temps[i] > (m_thermalLimit - 5);
        }
        
        recalculateScores();
    }
    
    /**
     * @brief Update drive load percentages
     */
    void updateLoads(const double* loads, int count) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        for (int i = 0; i < count && i < MAX_NVME_DRIVES; ++i) {
            m_drives[i].loadPercent = loads[i];
        }
        
        recalculateScores();
    }
    
    /**
     * @brief Update drive performance scores
     */
    void updatePerformanceScores(const double* scores, int count) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        for (int i = 0; i < count && i < MAX_NVME_DRIVES; ++i) {
            m_drives[i].perfScore = scores[i];
        }
        
        recalculateScores();
    }
    
    /**
     * @brief Mark a drive as unhealthy (excluded from selection)
     */
    void setDriveHealth(int driveIndex, bool healthy) {
        if (driveIndex >= 0 && driveIndex < MAX_NVME_DRIVES) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_drives[driveIndex].isHealthy = healthy;
            recalculateScores();
        }
    }
    
    /**
     * @brief Select optimal drive for a read operation
     * @param sizeBytes Size of the read operation
     * @return Load balance decision
     */
    LoadBalanceResult selectDriveForRead(uint64_t sizeBytes) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return selectOptimalDrive(sizeBytes, false);
    }
    
    /**
     * @brief Select optimal drive for a write operation
     * @param sizeBytes Size of the write operation
     * @return Load balance decision
     */
    LoadBalanceResult selectDriveForWrite(uint64_t sizeBytes) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return selectOptimalDrive(sizeBytes, true);
    }
    
    /**
     * @brief Get all drive information
     */
    std::array<NVMeDriveInfo, MAX_NVME_DRIVES> getDriveInfo() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_drives;
    }
    
    /**
     * @brief Get drive with most thermal headroom
     */
    int getCoolestDrive() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        int best = -1;
        double maxHeadroom = -999.0;
        
        for (int i = 0; i < m_activeDriveCount; ++i) {
            if (m_drives[i].isHealthy && m_drives[i].thermalHeadroom > maxHeadroom) {
                maxHeadroom = m_drives[i].thermalHeadroom;
                best = i;
            }
        }
        
        return best;
    }
    
    /**
     * @brief Get drive with lowest current load
     */
    int getLeastLoadedDrive() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        int best = -1;
        double minLoad = 999.0;
        
        for (int i = 0; i < m_activeDriveCount; ++i) {
            if (m_drives[i].isHealthy && m_drives[i].loadPercent < minLoad) {
                minLoad = m_drives[i].loadPercent;
                best = i;
            }
        }
        
        return best;
    }
    
    /**
     * @brief Check if any drive is critically hot
     */
    bool hasCriticalThermal() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        for (int i = 0; i < m_activeDriveCount; ++i) {
            if (m_drives[i].temperature > 70.0) {
                return true;
            }
        }
        return false;
    }
    
    /**
     * @brief Get average thermal headroom across all drives
     */
    double getAverageThermalHeadroom() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_activeDriveCount == 0) return 0.0;
        
        double sum = 0.0;
        for (int i = 0; i < m_activeDriveCount; ++i) {
            sum += m_drives[i].thermalHeadroom;
        }
        
        return sum / m_activeDriveCount;
    }
    
    /**
     * @brief Set thermal limit for all calculations
     */
    void setThermalLimit(double limit) {
        m_thermalLimit = limit;
    }

private:
    /**
     * @brief Recalculate balance scores for all drives
     */
    void recalculateScores() {
        for (int i = 0; i < m_activeDriveCount; ++i) {
            if (!m_drives[i].isHealthy) {
                m_drives[i].balanceScore = -999.0;
                continue;
            }
            
            // Normalize metrics to 0-100 scale
            double thermalScore = std::max(0.0, m_drives[i].thermalHeadroom / m_thermalLimit * 100.0);
            double loadScore = 100.0 - m_drives[i].loadPercent;
            double perfScore = m_drives[i].perfScore;
            
            // Apply penalty for throttled drives
            if (m_drives[i].isThrottled) {
                thermalScore *= 0.5;
            }
            
            // Weighted combination
            m_drives[i].balanceScore = 
                THERMAL_HEADROOM_WEIGHT * thermalScore +
                LOAD_WEIGHT * loadScore +
                PERF_WEIGHT * perfScore;
        }
    }
    
    /**
     * @brief Select optimal drive for an operation
     */
    LoadBalanceResult selectOptimalDrive(uint64_t sizeBytes, bool isWrite) {
        LoadBalanceResult result = {};
        result.selectedDrive = -1;
        result.confidence = 0.0;
        result.reason = "No suitable drive found";
        result.shouldSplit = false;
        result.splitCount = 0;
        
        if (m_activeDriveCount == 0) {
            return result;
        }
        
        // Find best drive by score
        int bestDrive = -1;
        double bestScore = -999.0;
        
        for (int i = 0; i < m_activeDriveCount; ++i) {
            if (m_drives[i].isHealthy && m_drives[i].balanceScore > bestScore) {
                bestScore = m_drives[i].balanceScore;
                bestDrive = i;
            }
        }
        
        if (bestDrive == -1) {
            return result;
        }
        
        result.selectedDrive = bestDrive;
        
        // Calculate confidence based on score margin
        double secondBest = -999.0;
        for (int i = 0; i < m_activeDriveCount; ++i) {
            if (i != bestDrive && m_drives[i].isHealthy && m_drives[i].balanceScore > secondBest) {
                secondBest = m_drives[i].balanceScore;
            }
        }
        
        if (secondBest > -999.0) {
            result.confidence = std::min(1.0, (bestScore - secondBest) / 50.0 + 0.5);
        } else {
            result.confidence = 1.0;  // Only one drive available
        }
        
        // Determine reason
        if (m_drives[bestDrive].thermalHeadroom > 15) {
            result.reason = "Selected for thermal headroom";
        } else if (m_drives[bestDrive].loadPercent < 30) {
            result.reason = "Selected for low load";
        } else {
            result.reason = "Best overall score";
        }
        
        // Check if we should split large operations
        constexpr uint64_t SPLIT_THRESHOLD = 256 * 1024 * 1024;  // 256MB
        
        if (sizeBytes > SPLIT_THRESHOLD && m_activeDriveCount >= 2) {
            // Find top 3 drives for splitting
            std::vector<std::pair<double, int>> ranked;
            for (int i = 0; i < m_activeDriveCount; ++i) {
                if (m_drives[i].isHealthy) {
                    ranked.push_back({m_drives[i].balanceScore, i});
                }
            }
            
            std::sort(ranked.begin(), ranked.end(), std::greater<>());
            
            if (ranked.size() >= 2) {
                result.shouldSplit = true;
                result.splitCount = std::min(3, static_cast<int>(ranked.size()));
                
                for (int i = 0; i < result.splitCount; ++i) {
                    result.splitDrives[i] = ranked[i].second;
                }
                
                result.reason = "Large operation split across drives";
            }
        }
        
        return result;
    }

private:
    std::array<NVMeDriveInfo, MAX_NVME_DRIVES> m_drives;
    int m_activeDriveCount;
    double m_thermalLimit;
    PredictiveThrottling* m_predictiveThrottling;
    mutable std::mutex m_mutex;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Integration Helper
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Thermal management system combining predictive throttling and load balancing
 */
class ThermalManagementSystem {
public:
    ThermalManagementSystem()
        : m_initialized(false)
    {
    }
    
    /**
     * @brief Initialize the thermal management system
     * @param createSharedMemory Create new shared memory block
     * @return true if successful
     */
    bool initialize(bool createSharedMemory = true) {
        if (m_initialized) return true;
        
        // Initialize predictive throttling with shared memory
        if (!m_predictiveThrottling.initializeSharedMemory(createSharedMemory)) {
            return false;
        }
        
        // Link load balancer to predictive throttling
        m_loadBalancer.setPredictiveThrottling(&m_predictiveThrottling);
        
        m_initialized = true;
        return true;
    }
    
    /**
     * @brief Shutdown the thermal management system
     */
    void shutdown() {
        m_predictiveThrottling.detachSharedMemory();
        m_initialized = false;
    }
    
    /**
     * @brief Update all thermal data
     */
    void update(const double* nvmeTemps, int nvmeCount, 
                double gpuTemp, double cpuTemp,
                const double* loads = nullptr) {
        // Update predictive throttling
        m_predictiveThrottling.updateTemperatures(nvmeTemps, nvmeCount, gpuTemp, cpuTemp);
        
        // Update load balancer
        m_loadBalancer.setActiveDriveCount(nvmeCount);
        m_loadBalancer.updateTemperatures(nvmeTemps, nvmeCount);
        
        if (loads) {
            m_loadBalancer.updateLoads(loads, nvmeCount);
        }
    }
    
    /**
     * @brief Get optimal drive for an operation
     */
    int selectOptimalDrive(uint64_t sizeBytes, bool isWrite) {
        auto result = isWrite 
            ? m_loadBalancer.selectDriveForWrite(sizeBytes)
            : m_loadBalancer.selectDriveForRead(sizeBytes);
        return result.selectedDrive;
    }
    
    /**
     * @brief Check if burst operations are safe
     */
    bool isBurstSafe() const {
        return m_predictiveThrottling.isBurstSafe();
    }
    
    /**
     * @brief Get current throttle percentage
     */
    uint32_t getCurrentThrottle() const {
        return m_predictiveThrottling.getCurrentThrottle();
    }
    
    /**
     * @brief Get thermal headroom
     */
    double getThermalHeadroom() const {
        return m_predictiveThrottling.getThermalHeadroom();
    }
    
    /**
     * @brief Access predictive throttling subsystem
     */
    PredictiveThrottling& predictiveThrottling() { return m_predictiveThrottling; }
    
    /**
     * @brief Access load balancer subsystem
     */
    DynamicLoadBalancer& loadBalancer() { return m_loadBalancer; }

private:
    PredictiveThrottling m_predictiveThrottling;
    DynamicLoadBalancer m_loadBalancer;
    bool m_initialized;
};

} // namespace rawrxd::thermal
