/**
 * @file PredictiveThrottling.h
 * @brief Predictive Thermal Throttling with EWMA Algorithm
 * 
 * Implements Exponentially Weighted Moving Average for temperature prediction
 * and proactive throttling to prevent thermal emergencies.
 * 
 * @copyright RawrXD IDE 2026
 */

#pragma once

#include <vector>
#include <deque>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <memory>
#include <functional>
#include <chrono>

namespace rawrxd::thermal {

// Forward declarations
struct ThermalSnapshot;

/**
 * @brief Shared memory control block offsets for MASM communication
 */
struct SovereignControlBlockOffsets {
    static constexpr size_t OFFSET_TEMP_THRESHOLD = 0x00;
    static constexpr size_t OFFSET_SOFT_THROTTLE = 0x08;
    static constexpr size_t OFFSET_BURST_MODE = 0x10;
    static constexpr size_t OFFSET_SESSION_KEY = 0x18;
    static constexpr size_t OFFSET_DRIVE_SELECT = 0x20;
    static constexpr size_t OFFSET_THERMAL_HEADROOM = 0x28;
    static constexpr size_t OFFSET_PREDICTION_VALID = 0x30;
    static constexpr size_t OFFSET_TIMESTAMP = 0x38;
    static constexpr size_t TOTAL_SIZE = 0x100;  // 256 bytes total block
};

/**
 * @brief Prediction result containing temperature forecast and confidence
 */
struct PredictionResult {
    double predictedTemp;           // Predicted temperature in Celsius
    double confidence;              // Confidence level 0.0-1.0
    double slope;                   // Temperature change rate (°C/sec)
    int64_t predictionHorizonMs;    // How far ahead this predicts
    bool isValid;                   // Whether prediction is reliable
    
    PredictionResult() 
        : predictedTemp(0.0), confidence(0.0), slope(0.0), 
          predictionHorizonMs(0), isValid(false) {}
};

/**
 * @brief Throttle action to be taken
 */
enum class ThrottleAction {
    NONE = 0,           // No action needed
    LIGHT,              // Light throttle (10-20%)
    MODERATE,           // Moderate throttle (20-40%)
    HEAVY,              // Heavy throttle (40-60%)
    EMERGENCY           // Emergency throttle (60%+)
};

/**
 * @brief Configuration for predictive throttling
 */
struct PredictiveConfig {
    double alpha = 0.3;                     // EWMA smoothing factor (0-1, higher = more responsive)
    size_t historySize = 20;                // Number of historical readings to maintain
    double thermalThreshold = 60.0;         // Temperature threshold for throttling (°C)
    double emergencyThreshold = 75.0;       // Emergency threshold (°C)
    int64_t predictionHorizonMs = 5000;     // Prediction horizon (milliseconds)
    double minConfidenceForAction = 0.7;    // Minimum confidence to take action
    int64_t samplingIntervalMs = 1000;      // Expected time between samples
};

/**
 * @brief Callback type for throttle actions
 */
using ThrottleCallback = std::function<void(ThrottleAction, double predictedTemp)>;

/**
 * @brief Predictive Throttling Engine with EWMA
 * 
 * Uses Exponentially Weighted Moving Average to predict future temperatures
 * and proactively adjust throttling before thermal limits are reached.
 */
class PredictiveThrottling {
public:
    /**
     * @brief Construct with default configuration
     */
    PredictiveThrottling();
    
    /**
     * @brief Construct with custom configuration
     * @param config Predictive throttling configuration
     */
    explicit PredictiveThrottling(const PredictiveConfig& config);
    
    /**
     * @brief Construct with alpha and history size (legacy interface)
     * @param alpha EWMA smoothing factor (0-1)
     * @param historySize Number of historical readings
     */
    PredictiveThrottling(double alpha, size_t historySize);
    
    ~PredictiveThrottling() = default;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Temperature Data Input
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Add a new temperature reading
     * @param temp Temperature in Celsius
     * @param timestampMs Optional timestamp (uses current time if 0)
     */
    void addTemperatureReading(double temp, int64_t timestampMs = 0);
    
    /**
     * @brief Add temperature reading from thermal snapshot
     * @param snapshot Thermal snapshot containing multiple sensors
     * @param sensorIndex Which sensor to use (0 = max, 1-5 = specific NVMe, 6 = GPU, 7 = CPU)
     */
    void addFromSnapshot(const ThermalSnapshot& snapshot, int sensorIndex = 0);
    
    /**
     * @brief Clear all historical data
     */
    void clearHistory();
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Prediction
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Predict next temperature using EWMA
     * @return Predicted temperature in Celsius
     */
    double predictNextTemperature();
    
    /**
     * @brief Get detailed prediction with confidence
     * @param horizonMs Prediction horizon in milliseconds (0 = use config default)
     * @return PredictionResult with temperature, confidence, and slope
     */
    PredictionResult getPrediction(int64_t horizonMs = 0);
    
    /**
     * @brief Get predicted temperature at specific future time
     * @param futureMs Milliseconds into the future
     * @return Predicted temperature
     */
    double predictAtTime(int64_t futureMs);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Throttle Control
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Adjust throttling based on prediction
     * 
     * Writes throttle commands to shared memory control block if configured.
     * Also invokes registered callbacks.
     */
    void adjustThrottling();
    
    /**
     * @brief Determine recommended throttle action
     * @param predictedTemp Temperature to evaluate
     * @return Recommended throttle action
     */
    ThrottleAction getRecommendedAction(double predictedTemp) const;
    
    /**
     * @brief Get throttle percentage for action
     * @param action Throttle action
     * @return Throttle percentage (0-100)
     */
    static int getThrottlePercentage(ThrottleAction action);
    
    /**
     * @brief Register callback for throttle actions
     * @param callback Function to call when throttle action is taken
     */
    void setThrottleCallback(ThrottleCallback callback);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Shared Memory Integration
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Set shared memory control block pointer
     * @param controlBlock Pointer to SOVEREIGN_CONTROL_BLOCK
     */
    void setControlBlock(void* controlBlock);
    
    /**
     * @brief Write prediction to shared memory
     * @param prediction Prediction result to write
     */
    void writeToControlBlock(const PredictionResult& prediction);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Configuration
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    const PredictiveConfig& config() const { return m_config; }
    
    /**
     * @brief Update configuration
     * @param config New configuration
     */
    void setConfig(const PredictiveConfig& config);
    
    /**
     * @brief Set EWMA alpha (smoothing factor)
     * @param alpha New alpha value (0-1)
     */
    void setAlpha(double alpha);
    
    /**
     * @brief Get current EWMA value
     * @return Current EWMA temperature
     */
    double getCurrentEWMA() const;
    
    /**
     * @brief Get temperature trend (slope)
     * @return Temperature change rate in °C/sec
     */
    double getTemperatureSlope() const;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Statistics
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Get number of readings in history
     * @return Number of temperature readings
     */
    size_t getHistorySize() const;
    
    /**
     * @brief Get most recent temperature reading
     * @return Last temperature or 0 if no data
     */
    double getLastReading() const;
    
    /**
     * @brief Get minimum temperature in history
     * @return Minimum temperature
     */
    double getMinTemperature() const;
    
    /**
     * @brief Get maximum temperature in history
     * @return Maximum temperature
     */
    double getMaxTemperature() const;
    
    /**
     * @brief Get average temperature in history
     * @return Average temperature
     */
    double getAverageTemperature() const;
    
    /**
     * @brief Get standard deviation of temperatures
     * @return Standard deviation
     */
    double getStandardDeviation() const;

private:
    // ═══════════════════════════════════════════════════════════════════════════
    // Internal Methods
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Calculate EWMA from history
     * @return EWMA value
     */
    double calculateEWMA() const;
    
    /**
     * @brief Calculate temperature slope using linear regression
     * @return Slope in °C/sec
     */
    double calculateSlope() const;
    
    /**
     * @brief Calculate prediction confidence
     * @return Confidence value 0.0-1.0
     */
    double calculateConfidence() const;
    
    /**
     * @brief Update internal EWMA cache
     */
    void updateEWMACache();
    
    /**
     * @brief Get current timestamp in milliseconds
     * @return Current time in ms since epoch
     */
    static int64_t getCurrentTimestampMs();

private:
    // ═══════════════════════════════════════════════════════════════════════════
    // Member Variables
    // ═══════════════════════════════════════════════════════════════════════════
    
    PredictiveConfig m_config;
    
    // Historical temperature data with timestamps
    struct TemperatureReading {
        double temperature;
        int64_t timestampMs;
    };
    std::deque<TemperatureReading> m_history;
    
    // Cached values
    double m_cachedEWMA;
    double m_cachedSlope;
    bool m_cacheValid;
    
    // Shared memory control block
    void* m_controlBlock;
    
    // Throttle callback
    ThrottleCallback m_throttleCallback;
    
    // Thread safety
    mutable std::mutex m_mutex;
};

} // namespace rawrxd::thermal
