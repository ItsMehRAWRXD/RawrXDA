/**
 * @file predictive_throttling.hpp
 * @brief Predictive Thermal Throttling Engine
 * 
 * Uses exponential moving average and trend analysis to predict
 * future temperatures and proactively adjust throttling before
 * thermal limits are reached.
 */

#pragma once

#include <array>
#include <cstdint>
#include <cmath>
#include <memory>
#include <mutex>
#include <chrono>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace rawrxd::thermal {

// ═══════════════════════════════════════════════════════════════════════════════
// Shared Memory Control Block Layout
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Shared memory layout for cross-process thermal control
 * 
 * Memory layout (256 bytes total):
 * ┌─────────────────────────────────────────────────────────────┐
 * │ Offset 0x00:  Magic (8 bytes) "RAWRXDTH"                    │
 * │ Offset 0x08:  Version (4 bytes)                             │
 * │ Offset 0x0C:  Flags (4 bytes)                               │
 * │ Offset 0x10:  Timestamp (8 bytes)                           │
 * │ Offset 0x18:  Current Throttle % (4 bytes)                  │
 * │ Offset 0x1C:  Target Throttle % (4 bytes)                   │
 * │ Offset 0x20:  Predicted Max Temp (8 bytes, double)          │
 * │ Offset 0x28:  Thermal Headroom (8 bytes, double)            │
 * │ Offset 0x30:  NVMe Temps[5] (40 bytes, 5x double)           │
 * │ Offset 0x58:  GPU Temp (8 bytes, double)                    │
 * │ Offset 0x60:  CPU Temp (8 bytes, double)                    │
 * │ Offset 0x68:  Burst Mode (4 bytes)                          │
 * │ Offset 0x6C:  Active Drive Count (4 bytes)                  │
 * │ Offset 0x70:  Reserved (144 bytes)                          │
 * └─────────────────────────────────────────────────────────────┘
 */

constexpr const char* SOVEREIGN_SHM_NAME = "Local\\RawrXD_Sovereign_Thermal_v1";
constexpr size_t SOVEREIGN_SHM_SIZE = 256;

// Offsets
constexpr size_t OFFSET_MAGIC           = 0x00;
constexpr size_t OFFSET_VERSION         = 0x08;
constexpr size_t OFFSET_FLAGS           = 0x0C;
constexpr size_t OFFSET_TIMESTAMP       = 0x10;
constexpr size_t OFFSET_CURRENT_THROTTLE= 0x18;
constexpr size_t OFFSET_TARGET_THROTTLE = 0x1C;
constexpr size_t OFFSET_PREDICTED_TEMP  = 0x20;
constexpr size_t OFFSET_THERMAL_HEADROOM= 0x28;
constexpr size_t OFFSET_NVME_TEMPS      = 0x30;
constexpr size_t OFFSET_GPU_TEMP        = 0x58;
constexpr size_t OFFSET_CPU_TEMP        = 0x60;
constexpr size_t OFFSET_BURST_MODE      = 0x68;
constexpr size_t OFFSET_DRIVE_COUNT     = 0x6C;

// Flags
constexpr uint32_t FLAG_ACTIVE          = 0x00000001;
constexpr uint32_t FLAG_EMERGENCY       = 0x00000002;
constexpr uint32_t FLAG_BURST_ALLOWED   = 0x00000004;
constexpr uint32_t FLAG_PREDICTIVE_ON   = 0x00000008;

#pragma pack(push, 1)
struct SovereignControlBlock {
    char magic[8];              // "RAWRXDTH"
    uint32_t version;           // 0x00010200 = v1.2.0
    uint32_t flags;
    uint64_t timestamp;         // ms since epoch
    uint32_t currentThrottle;   // 0-100
    uint32_t targetThrottle;    // 0-100
    double predictedMaxTemp;    // Predicted temp in 10s
    double thermalHeadroom;     // Degrees below limit
    double nvmeTemps[5];        // NVMe drive temps
    double gpuTemp;
    double cpuTemp;
    uint32_t burstMode;         // 0=sovereign, 1=thermal, 2=hybrid
    uint32_t activeDriveCount;
    uint8_t reserved[144];
};
#pragma pack(pop)

static_assert(sizeof(SovereignControlBlock) == 256, "Control block must be 256 bytes");

// ═══════════════════════════════════════════════════════════════════════════════
// Predictive Throttling Engine
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Predictive thermal throttling with EMA and trend analysis
 */
class PredictiveThrottling {
public:
    // Configuration
    static constexpr double DEFAULT_THERMAL_LIMIT = 65.0;   // °C
    static constexpr double EMERGENCY_LIMIT = 75.0;         // °C
    static constexpr double BURST_LIMIT = 70.0;             // °C
    static constexpr double EMA_ALPHA = 0.3;                // EMA smoothing factor
    static constexpr int HISTORY_SIZE = 60;                 // 60 samples (~1 minute)
    static constexpr int PREDICTION_HORIZON_SEC = 10;       // Predict 10s ahead
    
    PredictiveThrottling()
        : m_thermalLimit(DEFAULT_THERMAL_LIMIT)
        , m_controlBlock(nullptr)
        , m_shmHandle(nullptr)
        , m_historyIndex(0)
        , m_samplesCollected(0)
        , m_emaTemp(0.0)
        , m_tempSlope(0.0)
    {
        m_tempHistory.fill(0.0);
    }
    
    ~PredictiveThrottling() {
        detachSharedMemory();
    }
    
    /**
     * @brief Initialize shared memory control block
     * @param createNew Create new shared memory (true) or attach to existing (false)
     * @return true if successful
     */
    bool initializeSharedMemory(bool createNew = true) {
#ifdef _WIN32
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (createNew) {
            m_shmHandle = CreateFileMappingA(
                INVALID_HANDLE_VALUE,
                nullptr,
                PAGE_READWRITE,
                0,
                SOVEREIGN_SHM_SIZE,
                SOVEREIGN_SHM_NAME
            );
        } else {
            m_shmHandle = OpenFileMappingA(
                FILE_MAP_ALL_ACCESS,
                FALSE,
                SOVEREIGN_SHM_NAME
            );
        }
        
        if (!m_shmHandle) {
            return false;
        }
        
        m_controlBlock = static_cast<SovereignControlBlock*>(
            MapViewOfFile(m_shmHandle, FILE_MAP_ALL_ACCESS, 0, 0, SOVEREIGN_SHM_SIZE)
        );
        
        if (!m_controlBlock) {
            CloseHandle(m_shmHandle);
            m_shmHandle = nullptr;
            return false;
        }
        
        if (createNew) {
            // Initialize control block
            std::memcpy(m_controlBlock->magic, "RAWRXDTH", 8);
            m_controlBlock->version = 0x00010200;
            m_controlBlock->flags = FLAG_ACTIVE | FLAG_PREDICTIVE_ON;
            m_controlBlock->timestamp = getCurrentTimestamp();
            m_controlBlock->currentThrottle = 0;
            m_controlBlock->targetThrottle = 0;
            m_controlBlock->predictedMaxTemp = 0.0;
            m_controlBlock->thermalHeadroom = m_thermalLimit;
            m_controlBlock->burstMode = 2;  // Hybrid default
            m_controlBlock->activeDriveCount = 0;
            
            for (int i = 0; i < 5; ++i) {
                m_controlBlock->nvmeTemps[i] = 0.0;
            }
            m_controlBlock->gpuTemp = 0.0;
            m_controlBlock->cpuTemp = 0.0;
        }
        
        return true;
#else
        return false;  // Linux/macOS: use shm_open/mmap
#endif
    }
    
    /**
     * @brief Detach from shared memory
     */
    void detachSharedMemory() {
#ifdef _WIN32
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_controlBlock) {
            UnmapViewOfFile(m_controlBlock);
            m_controlBlock = nullptr;
        }
        
        if (m_shmHandle) {
            CloseHandle(m_shmHandle);
            m_shmHandle = nullptr;
        }
#endif
    }
    
    /**
     * @brief Update temperatures and calculate throttle
     * @param nvmeTemps Array of NVMe temperatures (up to 5)
     * @param nvmeCount Number of NVMe drives
     * @param gpuTemp GPU temperature
     * @param cpuTemp CPU temperature
     */
    void updateTemperatures(const double* nvmeTemps, int nvmeCount, double gpuTemp, double cpuTemp) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Find max NVMe temp
        double maxNvme = 0.0;
        for (int i = 0; i < nvmeCount && i < 5; ++i) {
            maxNvme = std::max(maxNvme, nvmeTemps[i]);
        }
        
        // Update history
        m_tempHistory[m_historyIndex] = maxNvme;
        m_historyIndex = (m_historyIndex + 1) % HISTORY_SIZE;
        m_samplesCollected = std::min(m_samplesCollected + 1, HISTORY_SIZE);
        
        // Calculate EMA
        if (m_samplesCollected == 1) {
            m_emaTemp = maxNvme;
        } else {
            m_emaTemp = EMA_ALPHA * maxNvme + (1.0 - EMA_ALPHA) * m_emaTemp;
        }
        
        // Calculate trend (linear regression slope)
        m_tempSlope = calculateTempSlope();
        
        // Update shared memory
        if (m_controlBlock) {
            m_controlBlock->timestamp = getCurrentTimestamp();
            
            for (int i = 0; i < nvmeCount && i < 5; ++i) {
                m_controlBlock->nvmeTemps[i] = nvmeTemps[i];
            }
            m_controlBlock->activeDriveCount = static_cast<uint32_t>(nvmeCount);
            m_controlBlock->gpuTemp = gpuTemp;
            m_controlBlock->cpuTemp = cpuTemp;
            
            // Calculate predicted temperature
            double predicted = predictNextTemperature();
            m_controlBlock->predictedMaxTemp = predicted;
            m_controlBlock->thermalHeadroom = m_thermalLimit - maxNvme;
            
            // Calculate throttle
            uint32_t throttle = calculateThrottle(maxNvme, predicted);
            m_controlBlock->targetThrottle = throttle;
            
            // Smooth throttle transition
            if (m_controlBlock->currentThrottle < throttle) {
                m_controlBlock->currentThrottle = std::min(
                    m_controlBlock->currentThrottle + 5,
                    throttle
                );
            } else if (m_controlBlock->currentThrottle > throttle) {
                m_controlBlock->currentThrottle = std::max(
                    static_cast<int>(m_controlBlock->currentThrottle) - 2,
                    static_cast<int>(throttle)
                );
            }
            
            // Update flags
            if (maxNvme > EMERGENCY_LIMIT) {
                m_controlBlock->flags |= FLAG_EMERGENCY;
            } else {
                m_controlBlock->flags &= ~FLAG_EMERGENCY;
            }
            
            if (maxNvme < BURST_LIMIT && m_tempSlope < 0.5) {
                m_controlBlock->flags |= FLAG_BURST_ALLOWED;
            } else {
                m_controlBlock->flags &= ~FLAG_BURST_ALLOWED;
            }
        }
    }
    
    /**
     * @brief Predict temperature N seconds into the future
     * @return Predicted temperature
     */
    double predictNextTemperature() const {
        // Linear extrapolation: T(future) = T(now) + slope * time
        return m_emaTemp + m_tempSlope * PREDICTION_HORIZON_SEC;
    }
    
    /**
     * @brief Get current thermal headroom
     * @return Degrees below thermal limit
     */
    double getThermalHeadroom() const {
        return m_thermalLimit - m_emaTemp;
    }
    
    /**
     * @brief Check if burst mode is safe
     * @return true if system can safely burst
     */
    bool isBurstSafe() const {
        // Safe to burst if:
        // 1. Current temp is below burst limit
        // 2. Temperature is not rising rapidly
        // 3. Predicted temp won't exceed limit
        return m_emaTemp < BURST_LIMIT && 
               m_tempSlope < 0.5 &&
               predictNextTemperature() < m_thermalLimit;
    }
    
    /**
     * @brief Set thermal limit
     */
    void setThermalLimit(double limit) {
        m_thermalLimit = limit;
    }
    
    /**
     * @brief Set burst mode in control block
     */
    void setBurstMode(int mode) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_controlBlock) {
            m_controlBlock->burstMode = static_cast<uint32_t>(mode);
        }
    }
    
    /**
     * @brief Get current throttle percentage
     */
    uint32_t getCurrentThrottle() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_controlBlock ? m_controlBlock->currentThrottle : 0;
    }
    
    /**
     * @brief Get pointer to control block (for external access)
     */
    const SovereignControlBlock* getControlBlock() const {
        return m_controlBlock;
    }

private:
    /**
     * @brief Calculate temperature slope using linear regression
     */
    double calculateTempSlope() const {
        if (m_samplesCollected < 10) {
            return 0.0;  // Not enough data
        }
        
        // Use last 30 samples (30 seconds) for slope calculation
        int n = std::min(30, m_samplesCollected);
        
        double sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
        
        for (int i = 0; i < n; ++i) {
            int idx = (m_historyIndex - 1 - i + HISTORY_SIZE) % HISTORY_SIZE;
            double x = static_cast<double>(n - 1 - i);  // Time (newest = 0)
            double y = m_tempHistory[idx];
            
            sumX += x;
            sumY += y;
            sumXY += x * y;
            sumX2 += x * x;
        }
        
        double denom = n * sumX2 - sumX * sumX;
        if (std::abs(denom) < 1e-10) {
            return 0.0;
        }
        
        // Slope in °C per second
        return (n * sumXY - sumX * sumY) / denom;
    }
    
    /**
     * @brief Calculate throttle based on current and predicted temps
     */
    uint32_t calculateThrottle(double currentTemp, double predictedTemp) const {
        uint32_t burstMode = m_controlBlock ? m_controlBlock->burstMode : 2;
        
        // Emergency override
        if (currentTemp > EMERGENCY_LIMIT) {
            return 80;  // Heavy throttle
        }
        
        double maxTemp = std::max(currentTemp, predictedTemp);
        
        switch (burstMode) {
            case 0:  // Sovereign Max - minimal throttle
                if (currentTemp > BURST_LIMIT) {
                    return static_cast<uint32_t>((currentTemp - BURST_LIMIT) * 8);
                }
                return 0;
                
            case 1:  // Thermal Governed - conservative
                if (maxTemp > m_thermalLimit - 5) {
                    return static_cast<uint32_t>((maxTemp - (m_thermalLimit - 10)) * 6);
                }
                return 15;  // Baseline eco throttle
                
            case 2:  // Adaptive Hybrid - smart
            default:
                if (predictedTemp > m_thermalLimit) {
                    // Proactive throttle based on prediction
                    return static_cast<uint32_t>((predictedTemp - m_thermalLimit + 5) * 4);
                } else if (currentTemp > m_thermalLimit - 5) {
                    // Reactive throttle
                    return static_cast<uint32_t>((currentTemp - (m_thermalLimit - 5)) * 5);
                } else if (m_tempSlope > 1.0) {
                    // Temperature rising fast
                    return 10;
                }
                return 0;
        }
    }
    
    uint64_t getCurrentTimestamp() const {
        auto now = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        ).count();
    }

private:
    double m_thermalLimit;
    SovereignControlBlock* m_controlBlock;
    void* m_shmHandle;
    
    // Temperature history for prediction
    std::array<double, HISTORY_SIZE> m_tempHistory;
    int m_historyIndex;
    int m_samplesCollected;
    
    // Predictive state
    double m_emaTemp;       // Exponential moving average
    double m_tempSlope;     // Temperature change rate (°C/s)
    
    mutable std::mutex m_mutex;
};

} // namespace rawrxd::thermal
