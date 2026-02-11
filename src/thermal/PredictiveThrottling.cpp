/**
 * @file PredictiveThrottling.cpp
 * @brief Predictive Thermal Throttling Implementation
 * 
 * Full production implementation of EWMA-based temperature prediction
 * with shared memory integration for MASM kernel communication.
 * 
 * @copyright RawrXD IDE 2026
 */

#include "PredictiveThrottling.h"
#include "thermal_dashboard_plugin.hpp"
#include <algorithm>
#include <numeric>
#include <cstring>

namespace rawrxd::thermal {

// ═══════════════════════════════════════════════════════════════════════════════
// Constructors
// ═══════════════════════════════════════════════════════════════════════════════

PredictiveThrottling::PredictiveThrottling()
    : m_config()
    , m_cachedEWMA(0.0)
    , m_cachedSlope(0.0)
    , m_cacheValid(false)
    , m_controlBlock(nullptr)
    , m_throttleCallback(nullptr)
{
}

PredictiveThrottling::PredictiveThrottling(const PredictiveConfig& config)
    : m_config(config)
    , m_cachedEWMA(0.0)
    , m_cachedSlope(0.0)
    , m_cacheValid(false)
    , m_controlBlock(nullptr)
    , m_throttleCallback(nullptr)
{
}

PredictiveThrottling::PredictiveThrottling(double alpha, size_t historySize)
    : m_config()
    , m_cachedEWMA(0.0)
    , m_cachedSlope(0.0)
    , m_cacheValid(false)
    , m_controlBlock(nullptr)
    , m_throttleCallback(nullptr)
{
    m_config.alpha = alpha;
    m_config.historySize = historySize;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Temperature Data Input
// ═══════════════════════════════════════════════════════════════════════════════

void PredictiveThrottling::addTemperatureReading(double temp, int64_t timestampMs)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Use current time if not provided
    if (timestampMs == 0) {
        timestampMs = getCurrentTimestampMs();
    }
    
    // Add reading to history
    TemperatureReading reading;
    reading.temperature = temp;
    reading.timestampMs = timestampMs;
    m_history.push_back(reading);
    
    // Maintain history size limit
    while (m_history.size() > m_config.historySize) {
        m_history.pop_front();
    }
    
    // Invalidate cache
    m_cacheValid = false;
}

void PredictiveThrottling::addFromSnapshot(const ThermalSnapshot& snapshot, int sensorIndex)
{
    double temp = 0.0;
    
    if (sensorIndex == 0) {
        // Use maximum temperature across all sensors
        temp = snapshot.gpuTemp;
        temp = std::max(temp, static_cast<double>(snapshot.cpuTemp));
        for (int i = 0; i < snapshot.activeDriveCount && i < 5; ++i) {
            temp = std::max(temp, static_cast<double>(snapshot.nvmeTemps[i]));
        }
    } else if (sensorIndex >= 1 && sensorIndex <= 5) {
        // Specific NVMe drive
        int driveIndex = sensorIndex - 1;
        if (driveIndex < snapshot.activeDriveCount) {
            temp = snapshot.nvmeTemps[driveIndex];
        }
    } else if (sensorIndex == 6) {
        // GPU
        temp = snapshot.gpuTemp;
    } else if (sensorIndex == 7) {
        // CPU
        temp = snapshot.cpuTemp;
    }
    
    addTemperatureReading(temp, snapshot.timestamp);
}

void PredictiveThrottling::clearHistory()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_history.clear();
    m_cachedEWMA = 0.0;
    m_cachedSlope = 0.0;
    m_cacheValid = false;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Prediction
// ═══════════════════════════════════════════════════════════════════════════════

double PredictiveThrottling::predictNextTemperature()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_history.size() < 2) {
        return m_history.empty() ? 0.0 : m_history.back().temperature;
    }
    
    // Update cache if needed
    if (!m_cacheValid) {
        updateEWMACache();
    }
    
    // Calculate EWMA prediction
    double ewma = m_cachedEWMA;
    double slope = m_cachedSlope;
    
    // Project forward based on prediction horizon
    // slope is in °C/sec, convert horizon from ms to sec
    double horizonSec = m_config.predictionHorizonMs / 1000.0;
    double prediction = ewma + (slope * horizonSec);
    
    return prediction;
}

PredictionResult PredictiveThrottling::getPrediction(int64_t horizonMs)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    PredictionResult result;
    
    if (horizonMs == 0) {
        horizonMs = m_config.predictionHorizonMs;
    }
    result.predictionHorizonMs = horizonMs;
    
    if (m_history.size() < 2) {
        result.predictedTemp = m_history.empty() ? 0.0 : m_history.back().temperature;
        result.confidence = 0.0;
        result.slope = 0.0;
        result.isValid = false;
        return result;
    }
    
    // Update cache if needed
    if (!m_cacheValid) {
        updateEWMACache();
    }
    
    // Calculate prediction
    double horizonSec = horizonMs / 1000.0;
    result.predictedTemp = m_cachedEWMA + (m_cachedSlope * horizonSec);
    result.slope = m_cachedSlope;
    result.confidence = calculateConfidence();
    result.isValid = (result.confidence >= m_config.minConfidenceForAction);
    
    return result;
}

double PredictiveThrottling::predictAtTime(int64_t futureMs)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_history.size() < 2) {
        return m_history.empty() ? 0.0 : m_history.back().temperature;
    }
    
    if (!m_cacheValid) {
        updateEWMACache();
    }
    
    double futureSec = futureMs / 1000.0;
    return m_cachedEWMA + (m_cachedSlope * futureSec);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Throttle Control
// ═══════════════════════════════════════════════════════════════════════════════

void PredictiveThrottling::adjustThrottling()
{
    PredictionResult prediction = getPrediction();
    
    if (!prediction.isValid) {
        return;  // Not enough confidence to act
    }
    
    ThrottleAction action = getRecommendedAction(prediction.predictedTemp);
    int throttlePercent = getThrottlePercentage(action);
    
    // Write to shared memory control block if configured
    if (m_controlBlock != nullptr) {
        writeToControlBlock(prediction);
        
        // Write throttle command
        auto* block = static_cast<uint8_t*>(m_controlBlock);
        *reinterpret_cast<int*>(block + SovereignControlBlockOffsets::OFFSET_SOFT_THROTTLE) = throttlePercent;
        *reinterpret_cast<double*>(block + SovereignControlBlockOffsets::OFFSET_TEMP_THRESHOLD) = prediction.predictedTemp;
        *reinterpret_cast<int64_t*>(block + SovereignControlBlockOffsets::OFFSET_TIMESTAMP) = getCurrentTimestampMs();
    }
    
    // Invoke callback if registered
    if (m_throttleCallback) {
        m_throttleCallback(action, prediction.predictedTemp);
    }
}

ThrottleAction PredictiveThrottling::getRecommendedAction(double predictedTemp) const
{
    if (predictedTemp >= m_config.emergencyThreshold) {
        return ThrottleAction::EMERGENCY;
    }
    
    double headroom = m_config.emergencyThreshold - predictedTemp;
    double threshold = m_config.thermalThreshold;
    
    if (predictedTemp < threshold) {
        return ThrottleAction::NONE;
    }
    
    double overage = predictedTemp - threshold;
    double range = m_config.emergencyThreshold - threshold;
    double severity = overage / range;
    
    if (severity < 0.25) {
        return ThrottleAction::LIGHT;
    } else if (severity < 0.50) {
        return ThrottleAction::MODERATE;
    } else {
        return ThrottleAction::HEAVY;
    }
}

int PredictiveThrottling::getThrottlePercentage(ThrottleAction action)
{
    switch (action) {
        case ThrottleAction::NONE:      return 0;
        case ThrottleAction::LIGHT:     return 15;
        case ThrottleAction::MODERATE:  return 35;
        case ThrottleAction::HEAVY:     return 55;
        case ThrottleAction::EMERGENCY: return 75;
        default:                        return 0;
    }
}

void PredictiveThrottling::setThrottleCallback(ThrottleCallback callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_throttleCallback = std::move(callback);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Shared Memory Integration
// ═══════════════════════════════════════════════════════════════════════════════

void PredictiveThrottling::setControlBlock(void* controlBlock)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_controlBlock = controlBlock;
}

void PredictiveThrottling::writeToControlBlock(const PredictionResult& prediction)
{
    if (m_controlBlock == nullptr) {
        return;
    }
    
    auto* block = static_cast<uint8_t*>(m_controlBlock);
    
    // Write predicted temperature
    *reinterpret_cast<double*>(block + SovereignControlBlockOffsets::OFFSET_TEMP_THRESHOLD) = 
        prediction.predictedTemp;
    
    // Write thermal headroom (distance from emergency threshold)
    double headroom = m_config.emergencyThreshold - prediction.predictedTemp;
    *reinterpret_cast<double*>(block + SovereignControlBlockOffsets::OFFSET_THERMAL_HEADROOM) = headroom;
    
    // Write prediction validity flag
    *reinterpret_cast<int*>(block + SovereignControlBlockOffsets::OFFSET_PREDICTION_VALID) = 
        prediction.isValid ? 1 : 0;
    
    // Write timestamp
    *reinterpret_cast<int64_t*>(block + SovereignControlBlockOffsets::OFFSET_TIMESTAMP) = 
        getCurrentTimestampMs();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Configuration
// ═══════════════════════════════════════════════════════════════════════════════

void PredictiveThrottling::setConfig(const PredictiveConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
    m_cacheValid = false;
}

void PredictiveThrottling::setAlpha(double alpha)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.alpha = std::clamp(alpha, 0.0, 1.0);
    m_cacheValid = false;
}

double PredictiveThrottling::getCurrentEWMA() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_cacheValid) {
        const_cast<PredictiveThrottling*>(this)->updateEWMACache();
    }
    return m_cachedEWMA;
}

double PredictiveThrottling::getTemperatureSlope() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_cacheValid) {
        const_cast<PredictiveThrottling*>(this)->updateEWMACache();
    }
    return m_cachedSlope;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Statistics
// ═══════════════════════════════════════════════════════════════════════════════

size_t PredictiveThrottling::getHistorySize() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_history.size();
}

double PredictiveThrottling::getLastReading() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_history.empty() ? 0.0 : m_history.back().temperature;
}

double PredictiveThrottling::getMinTemperature() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_history.empty()) return 0.0;
    
    auto minIt = std::min_element(m_history.begin(), m_history.end(),
        [](const TemperatureReading& a, const TemperatureReading& b) {
            return a.temperature < b.temperature;
        });
    return minIt->temperature;
}

double PredictiveThrottling::getMaxTemperature() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_history.empty()) return 0.0;
    
    auto maxIt = std::max_element(m_history.begin(), m_history.end(),
        [](const TemperatureReading& a, const TemperatureReading& b) {
            return a.temperature < b.temperature;
        });
    return maxIt->temperature;
}

double PredictiveThrottling::getAverageTemperature() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_history.empty()) return 0.0;
    
    double sum = std::accumulate(m_history.begin(), m_history.end(), 0.0,
        [](double acc, const TemperatureReading& r) {
            return acc + r.temperature;
        });
    return sum / static_cast<double>(m_history.size());
}

double PredictiveThrottling::getStandardDeviation() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_history.size() < 2) return 0.0;
    
    double mean = 0.0;
    for (const auto& r : m_history) {
        mean += r.temperature;
    }
    mean /= static_cast<double>(m_history.size());
    
    double variance = 0.0;
    for (const auto& r : m_history) {
        double diff = r.temperature - mean;
        variance += diff * diff;
    }
    variance /= static_cast<double>(m_history.size() - 1);
    
    return std::sqrt(variance);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Internal Methods
// ═══════════════════════════════════════════════════════════════════════════════

double PredictiveThrottling::calculateEWMA() const
{
    if (m_history.empty()) return 0.0;
    if (m_history.size() == 1) return m_history.front().temperature;
    
    // EWMA: S_t = α * X_t + (1 - α) * S_{t-1}
    double ewma = m_history.front().temperature;
    double alpha = m_config.alpha;
    
    for (size_t i = 1; i < m_history.size(); ++i) {
        ewma = alpha * m_history[i].temperature + (1.0 - alpha) * ewma;
    }
    
    return ewma;
}

double PredictiveThrottling::calculateSlope() const
{
    if (m_history.size() < 2) return 0.0;
    
    // Linear regression to calculate slope
    // Using timestamps for accurate time-based slope calculation
    
    size_t n = m_history.size();
    
    // Calculate means
    double meanX = 0.0;  // Time
    double meanY = 0.0;  // Temperature
    
    int64_t baseTime = m_history.front().timestampMs;
    
    for (const auto& r : m_history) {
        double x = static_cast<double>(r.timestampMs - baseTime) / 1000.0;  // Convert to seconds
        meanX += x;
        meanY += r.temperature;
    }
    meanX /= static_cast<double>(n);
    meanY /= static_cast<double>(n);
    
    // Calculate slope using least squares
    double numerator = 0.0;
    double denominator = 0.0;
    
    for (const auto& r : m_history) {
        double x = static_cast<double>(r.timestampMs - baseTime) / 1000.0;
        double xDiff = x - meanX;
        double yDiff = r.temperature - meanY;
        
        numerator += xDiff * yDiff;
        denominator += xDiff * xDiff;
    }
    
    if (std::abs(denominator) < 1e-10) {
        return 0.0;  // Avoid division by zero
    }
    
    return numerator / denominator;  // Slope in °C/sec
}

double PredictiveThrottling::calculateConfidence() const
{
    if (m_history.size() < 3) return 0.0;
    
    // Calculate confidence based on:
    // 1. Number of data points (more = higher confidence)
    // 2. Standard deviation (lower = higher confidence)
    // 3. Consistency of slope (less variation = higher confidence)
    
    // Factor 1: Data points (scale from 0 to 0.4)
    double dataPointFactor = std::min(static_cast<double>(m_history.size()) / m_config.historySize, 1.0) * 0.4;
    
    // Factor 2: Temperature stability (scale from 0 to 0.3)
    double stdDev = const_cast<PredictiveThrottling*>(this)->getStandardDeviation();
    double stabilityFactor = std::max(0.0, 1.0 - (stdDev / 10.0)) * 0.3;  // 10°C stddev = 0 confidence
    
    // Factor 3: Recency of data (scale from 0 to 0.3)
    int64_t now = getCurrentTimestampMs();
    int64_t lastReadingTime = m_history.back().timestampMs;
    double ageSec = static_cast<double>(now - lastReadingTime) / 1000.0;
    double recencyFactor = std::max(0.0, 1.0 - (ageSec / 10.0)) * 0.3;  // 10sec old = 0 confidence
    
    return std::clamp(dataPointFactor + stabilityFactor + recencyFactor, 0.0, 1.0);
}

void PredictiveThrottling::updateEWMACache()
{
    m_cachedEWMA = calculateEWMA();
    m_cachedSlope = calculateSlope();
    m_cacheValid = true;
}

int64_t PredictiveThrottling::getCurrentTimestampMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()
    ).count();
}

} // namespace rawrxd::thermal
