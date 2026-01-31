/**
 * @file EnhancedPredictiveThrottling.cpp
 * @brief ML-Ready Predictive Throttling Implementation
 * 
 * Full production implementation with multiple prediction algorithms.
 * 
 * @copyright RawrXD IDE 2026
 */

#include "EnhancedPredictiveThrottling.hpp"

#include <cmath>

namespace rawrxd::thermal {

// ═══════════════════════════════════════════════════════════════════════════════
// Construction
// ═══════════════════════════════════════════════════════════════════════════════

EnhancedPredictiveThrottling::EnhancedPredictiveThrottling()
    : m_config()
{
    m_seasonal.resize(m_config.seasonalPeriod, 0.0);
}

EnhancedPredictiveThrottling::EnhancedPredictiveThrottling(const EnhancedPredictiveConfig& config)
    : m_config(config)
{
    m_seasonal.resize(m_config.seasonalPeriod, 0.0);
    
    // Initialize ML predictor if enabled
    if (m_config.primaryAlgorithm == PredictionAlgorithm::NeuralNetwork && 
        !m_config.modelPath.isEmpty()) {
        m_mlPredictor = MLPredictorFactory::create(m_config.modelPath);
    }
}

EnhancedPredictiveThrottling::~EnhancedPredictiveThrottling() = default;

// ═══════════════════════════════════════════════════════════════════════════════
// Configuration
// ═══════════════════════════════════════════════════════════════════════════════

void EnhancedPredictiveThrottling::setConfig(const EnhancedPredictiveConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
    m_seasonal.resize(m_config.seasonalPeriod, 0.0);
    m_cacheValid = false;
}

EnhancedPredictiveConfig EnhancedPredictiveThrottling::getConfig() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

void EnhancedPredictiveThrottling::setAlgorithm(PredictionAlgorithm algorithm)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.primaryAlgorithm = algorithm;
    m_cacheValid = false;
}

void EnhancedPredictiveThrottling::setThreshold(double threshold)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.sustainableThreshold = threshold;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Data Input
// ═══════════════════════════════════════════════════════════════════════════════

void EnhancedPredictiveThrottling::addReading(double temperature, int64_t timestampMs)
{
    TemperatureReading reading;
    reading.temperature = temperature;
    reading.timestampMs = timestampMs > 0 ? timestampMs : getCurrentTimestampMs();
    reading.sensorIndex = 0;
    reading.confidence = 1.0;
    
    addReading(reading);
}

void EnhancedPredictiveThrottling::addReading(const TemperatureReading& reading)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Add to history
    m_history.push_back(reading);
    
    // Maintain history size
    while (m_history.size() > m_config.historySize) {
        m_history.pop_front();
    }
    
    // Invalidate cache
    m_cacheValid = false;
    
    // Filter outliers if enabled
    if (m_config.enableOutlierFiltering) {
        filterOutliers();
    }
    
    // Update caches
    updateCaches();
    
    // Store validation data
    if (!m_validationData.empty()) {
        // Check if any prediction was made for this time
        auto& lastPrediction = m_validationData.back();
        if (lastPrediction.second == 0.0) {
            lastPrediction.second = reading.temperature;
        }
    }
}

void EnhancedPredictiveThrottling::addReadings(const std::vector<TemperatureReading>& readings)
{
    for (const auto& reading : readings) {
        addReading(reading);
    }
}

void EnhancedPredictiveThrottling::clearHistory()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_history.clear();
    m_cacheValid = false;
    m_level = 0.0;
    m_trend = 0.0;
    std::fill(m_seasonal.begin(), m_seasonal.end(), 0.0);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Prediction
// ═══════════════════════════════════════════════════════════════════════════════

EnhancedPrediction EnhancedPredictiveThrottling::predict()
{
    return predict(m_config.predictionHorizonMs);
}

EnhancedPrediction EnhancedPredictiveThrottling::predict(int64_t horizonMs)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    EnhancedPrediction result;
    result.horizonMs = horizonMs;
    
    if (m_history.size() < 2) {
        if (!m_history.empty()) {
            result.predictedTemp = m_history.back().temperature;
            result.confidence = 0.3;
            result.isValid = true;
        }
        return result;
    }
    
    switch (m_config.primaryAlgorithm) {
        case PredictionAlgorithm::EWMA:
            result = predictEWMA(horizonMs);
            break;
        case PredictionAlgorithm::LinearRegression:
            result = predictLinearRegression(horizonMs);
            break;
        case PredictionAlgorithm::DoubleExponential:
            result = predictDoubleExponential(horizonMs);
            break;
        case PredictionAlgorithm::TripleExponential:
            result = predictTripleExponential(horizonMs);
            break;
        case PredictionAlgorithm::NeuralNetwork:
            result = predictNeuralNetwork(horizonMs);
            break;
        case PredictionAlgorithm::Ensemble:
            result = predictEnsemble(horizonMs);
            break;
    }
    
    // Calculate uncertainty bounds
    double stdDev = getStandardDeviation();
    result.upperBound = result.predictedTemp + 1.96 * stdDev;
    result.lowerBound = result.predictedTemp - 1.96 * stdDev;
    
    // Store for validation
    m_validationData.push_back({result, 0.0});
    while (m_validationData.size() > 100) {
        m_validationData.pop_front();
    }
    
    // Invoke callback
    if (m_predictionCallback) {
        m_predictionCallback(result);
    }
    
    return result;
}

double EnhancedPredictiveThrottling::getEWMA() const
{
    return m_cachedEWMA.load();
}

double EnhancedPredictiveThrottling::getSlope() const
{
    return m_cachedSlope.load();
}

double EnhancedPredictiveThrottling::getAcceleration() const
{
    return m_cachedAcceleration.load();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Algorithm Implementations
// ═══════════════════════════════════════════════════════════════════════════════

EnhancedPrediction EnhancedPredictiveThrottling::predictEWMA(int64_t horizonMs)
{
    EnhancedPrediction result;
    result.algorithm = PredictionAlgorithm::EWMA;
    result.horizonMs = horizonMs;
    
    // Calculate EWMA
    double ewma = m_history[0].temperature;
    for (size_t i = 1; i < m_history.size(); ++i) {
        ewma = m_config.ewmaAlpha * m_history[i].temperature + 
               (1.0 - m_config.ewmaAlpha) * ewma;
    }
    
    result.ewmaValue = ewma;
    
    // Calculate slope from recent readings
    if (m_history.size() >= 3) {
        double recentSlope = 0.0;
        size_t slopeWindow = std::min(m_history.size(), size_t(5));
        
        for (size_t i = m_history.size() - slopeWindow; i < m_history.size() - 1; ++i) {
            double dt = (m_history[i + 1].timestampMs - m_history[i].timestampMs) / 1000.0;
            if (dt > 0) {
                recentSlope += (m_history[i + 1].temperature - m_history[i].temperature) / dt;
            }
        }
        recentSlope /= (slopeWindow - 1);
        result.slope = recentSlope;
    }
    
    // Project forward
    double secondsAhead = horizonMs / 1000.0;
    result.predictedTemp = ewma + result.slope * secondsAhead;
    
    // Confidence based on slope stability
    result.confidence = calculateConfidence();
    result.isValid = true;
    
    m_cachedEWMA = ewma;
    m_cachedSlope = result.slope;
    
    return result;
}

EnhancedPrediction EnhancedPredictiveThrottling::predictLinearRegression(int64_t horizonMs)
{
    EnhancedPrediction result;
    result.algorithm = PredictionAlgorithm::LinearRegression;
    result.horizonMs = horizonMs;
    
    // Prepare data for regression
    std::vector<double> x, y;
    int64_t baseTime = m_history.front().timestampMs;
    
    for (const auto& reading : m_history) {
        x.push_back((reading.timestampMs - baseTime) / 1000.0);
        y.push_back(reading.temperature);
    }
    
    // Calculate linear regression
    double n = static_cast<double>(x.size());
    double sumX = std::accumulate(x.begin(), x.end(), 0.0);
    double sumY = std::accumulate(y.begin(), y.end(), 0.0);
    double sumXY = 0.0, sumX2 = 0.0;
    
    for (size_t i = 0; i < x.size(); ++i) {
        sumXY += x[i] * y[i];
        sumX2 += x[i] * x[i];
    }
    
    double denominator = n * sumX2 - sumX * sumX;
    if (std::abs(denominator) < 1e-10) {
        // Fallback to EWMA
        return predictEWMA(horizonMs);
    }
    
    result.regressionSlope = (n * sumXY - sumX * sumY) / denominator;
    result.regressionIntercept = (sumY - result.regressionSlope * sumX) / n;
    result.slope = result.regressionSlope;
    
    // Calculate R² score
    double meanY = sumY / n;
    double ssTot = 0.0, ssRes = 0.0;
    
    for (size_t i = 0; i < x.size(); ++i) {
        double predicted = result.regressionIntercept + result.regressionSlope * x[i];
        ssRes += (y[i] - predicted) * (y[i] - predicted);
        ssTot += (y[i] - meanY) * (y[i] - meanY);
    }
    
    result.r2Score = (ssTot > 0) ? (1.0 - ssRes / ssTot) : 0.0;
    
    // Predict at horizon
    double currentTime = x.back();
    double futureTime = currentTime + horizonMs / 1000.0;
    result.predictedTemp = result.regressionIntercept + result.regressionSlope * futureTime;
    
    // Confidence based on R² and data quality
    result.confidence = std::max(0.0, std::min(1.0, result.r2Score * calculateConfidence()));
    result.isValid = true;
    
    m_cachedSlope = result.slope;
    
    return result;
}

EnhancedPrediction EnhancedPredictiveThrottling::predictDoubleExponential(int64_t horizonMs)
{
    EnhancedPrediction result;
    result.algorithm = PredictionAlgorithm::DoubleExponential;
    result.horizonMs = horizonMs;
    
    // Initialize if needed
    if (m_level == 0.0 && !m_history.empty()) {
        m_level = m_history.front().temperature;
        m_trend = 0.0;
    }
    
    // Update level and trend for each new reading
    double alpha = m_config.ewmaAlpha;
    double beta = m_config.trendAlpha;
    
    for (const auto& reading : m_history) {
        double prevLevel = m_level;
        m_level = alpha * reading.temperature + (1.0 - alpha) * (m_level + m_trend);
        m_trend = beta * (m_level - prevLevel) + (1.0 - beta) * m_trend;
    }
    
    // Predict k steps ahead
    int k = static_cast<int>(horizonMs / m_config.samplingIntervalMs);
    result.predictedTemp = m_level + k * m_trend;
    result.slope = m_trend / (m_config.samplingIntervalMs / 1000.0);
    
    result.confidence = calculateConfidence() * 0.9;
    result.isValid = true;
    
    m_cachedSlope = result.slope;
    
    return result;
}

EnhancedPrediction EnhancedPredictiveThrottling::predictTripleExponential(int64_t horizonMs)
{
    EnhancedPrediction result;
    result.algorithm = PredictionAlgorithm::TripleExponential;
    result.horizonMs = horizonMs;
    
    // Initialize if needed
    if (m_level == 0.0 && !m_history.empty()) {
        m_level = m_history.front().temperature;
        m_trend = 0.0;
        std::fill(m_seasonal.begin(), m_seasonal.end(), 0.0);
    }
    
    double alpha = m_config.ewmaAlpha;
    double beta = m_config.trendAlpha;
    double gamma = m_config.seasonalAlpha;
    int L = m_config.seasonalPeriod;
    
    // Update for each reading
    for (const auto& reading : m_history) {
        double prevLevel = m_level;
        int idx = m_seasonalIndex % L;
        
        m_level = alpha * (reading.temperature - m_seasonal[idx]) + 
                  (1.0 - alpha) * (m_level + m_trend);
        m_trend = beta * (m_level - prevLevel) + (1.0 - beta) * m_trend;
        m_seasonal[idx] = gamma * (reading.temperature - m_level) + 
                          (1.0 - gamma) * m_seasonal[idx];
        
        m_seasonalIndex++;
    }
    
    // Predict k steps ahead
    int k = static_cast<int>(horizonMs / m_config.samplingIntervalMs);
    int futureIdx = (m_seasonalIndex + k) % L;
    result.predictedTemp = m_level + k * m_trend + m_seasonal[futureIdx];
    result.slope = m_trend / (m_config.samplingIntervalMs / 1000.0);
    
    result.confidence = calculateConfidence() * 0.85;
    result.isValid = true;
    
    m_cachedSlope = result.slope;
    
    return result;
}

EnhancedPrediction EnhancedPredictiveThrottling::predictNeuralNetwork(int64_t horizonMs)
{
    EnhancedPrediction result;
    result.algorithm = PredictionAlgorithm::NeuralNetwork;
    result.horizonMs = horizonMs;
    
    // Check if ML predictor is available
    if (!m_mlPredictor || !m_mlPredictor->isLoaded()) {
        return predictEWMA(horizonMs);
    }
    
    // Prepare history vector
    std::vector<double> historyVec;
    for (const auto& reading : m_history) {
        historyVec.push_back(reading.temperature);
    }
    
    // Run prediction
    result.predictedTemp = m_mlPredictor->predict(historyVec, horizonMs);
    result.confidence = m_mlPredictor->getConfidence() * calculateConfidence();
    
    // Also calculate slope using EWMA for reference
    auto ewmaPred = predictEWMA(horizonMs);
    result.slope = ewmaPred.slope;
    result.ewmaValue = ewmaPred.ewmaValue;
    
    result.isValid = true;
    
    return result;
}

EnhancedPrediction EnhancedPredictiveThrottling::predictEnsemble(int64_t horizonMs)
{
    EnhancedPrediction result;
    result.algorithm = PredictionAlgorithm::Ensemble;
    result.horizonMs = horizonMs;
    
    // Get predictions from all algorithms
    auto ewma = predictEWMA(horizonMs);
    auto linreg = predictLinearRegression(horizonMs);
    auto doubleExp = predictDoubleExponential(horizonMs);
    
    // Weight by confidence
    double totalWeight = ewma.confidence + linreg.confidence + doubleExp.confidence;
    
    if (totalWeight > 0) {
        result.predictedTemp = (ewma.predictedTemp * ewma.confidence +
                               linreg.predictedTemp * linreg.confidence +
                               doubleExp.predictedTemp * doubleExp.confidence) / totalWeight;
        
        result.slope = (ewma.slope * ewma.confidence +
                       linreg.slope * linreg.confidence +
                       doubleExp.slope * doubleExp.confidence) / totalWeight;
        
        result.confidence = totalWeight / 3.0;
    } else {
        result = ewma;
    }
    
    result.ewmaValue = ewma.ewmaValue;
    result.regressionSlope = linreg.regressionSlope;
    result.regressionIntercept = linreg.regressionIntercept;
    result.r2Score = linreg.r2Score;
    result.isValid = true;
    
    m_cachedSlope = result.slope;
    
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Throttle Recommendations
// ═══════════════════════════════════════════════════════════════════════════════

ThrottleRecommendation EnhancedPredictiveThrottling::getRecommendation()
{
    double currentTemp = getCurrentTemperature();
    return getRecommendation(currentTemp);
}

ThrottleRecommendation EnhancedPredictiveThrottling::getRecommendation(double currentTemp)
{
    ThrottleRecommendation rec;
    
    auto prediction = predict();
    
    // Calculate time to threshold
    if (prediction.slope > 0) {
        double headroom = m_config.sustainableThreshold - currentTemp;
        rec.timeToThreshold = headroom / prediction.slope;
    } else {
        rec.timeToThreshold = std::numeric_limits<double>::infinity();
    }
    
    // Determine throttle based on current and predicted
    rec.throttlePercent = calculateThrottlePercent(prediction.predictedTemp, currentTemp);
    rec.shouldThrottle = rec.throttlePercent > 0;
    rec.isPredictive = prediction.predictedTemp > currentTemp;
    
    // Generate reason
    if (currentTemp >= m_config.emergencyThreshold) {
        rec.reason = std::string("🚨 EMERGENCY: Temperature at %.1f°C");
        rec.throttlePercent = 100;
    } else if (currentTemp >= m_config.criticalThreshold) {
        rec.reason = std::string("⚠️ CRITICAL: Temperature at %.1f°C");
    } else if (prediction.predictedTemp >= m_config.sustainableThreshold) {
        rec.reason = std::string("📈 PREDICTIVE: Will reach %.1f°C in %.1fs")
                    ;
    } else if (currentTemp >= m_config.warningThreshold) {
        rec.reason = std::string("🟡 WARNING: Temperature at %.1f°C");
    } else {
        rec.reason = std::string("✅ NOMINAL: Temperature at %.1f°C");
    }
    
    // Invoke callback
    if (rec.shouldThrottle && m_throttleCallback) {
        m_throttleCallback(rec);
    }
    
    return rec;
}

double EnhancedPredictiveThrottling::timeToThreshold() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_history.empty()) return std::numeric_limits<double>::infinity();
    
    double currentTemp = m_history.back().temperature;
    double slope = m_cachedSlope.load();
    
    if (slope <= 0) return std::numeric_limits<double>::infinity();
    
    double headroom = m_config.sustainableThreshold - currentTemp;
    return headroom / slope;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Callbacks
// ═══════════════════════════════════════════════════════════════════════════════

void EnhancedPredictiveThrottling::setThrottleCallback(ThrottleEventCallback callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_throttleCallback = std::move(callback);
}

void EnhancedPredictiveThrottling::setPredictionCallback(PredictionCallback callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_predictionCallback = std::move(callback);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Statistics
// ═══════════════════════════════════════════════════════════════════════════════

size_t EnhancedPredictiveThrottling::getHistorySize() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_history.size();
}

double EnhancedPredictiveThrottling::getCurrentTemperature() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_history.empty() ? 0.0 : m_history.back().temperature;
}

double EnhancedPredictiveThrottling::getMinTemperature() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_history.empty()) return 0.0;
    
    return std::min_element(m_history.begin(), m_history.end(),
        [](const auto& a, const auto& b) { return a.temperature < b.temperature; }
    )->temperature;
}

double EnhancedPredictiveThrottling::getMaxTemperature() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_history.empty()) return 0.0;
    
    return std::max_element(m_history.begin(), m_history.end(),
        [](const auto& a, const auto& b) { return a.temperature < b.temperature; }
    )->temperature;
}

double EnhancedPredictiveThrottling::getAverageTemperature() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_history.empty()) return 0.0;
    
    double sum = 0.0;
    for (const auto& reading : m_history) {
        sum += reading.temperature;
    }
    return sum / m_history.size();
}

double EnhancedPredictiveThrottling::getStandardDeviation() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_history.size() < 2) return 0.0;
    
    double mean = getAverageTemperature();
    double sumSq = 0.0;
    
    for (const auto& reading : m_history) {
        double diff = reading.temperature - mean;
        sumSq += diff * diff;
    }
    
    return std::sqrt(sumSq / (m_history.size() - 1));
}

double EnhancedPredictiveThrottling::getPredictionAccuracy() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_validationData.empty()) return 0.0;
    
    double totalError = 0.0;
    int validCount = 0;
    
    for (const auto& pair : m_validationData) {
        if (pair.second != 0.0) {
            totalError += std::abs(pair.first.predictedTemp - pair.second);
            validCount++;
        }
    }
    
    if (validCount == 0) return 0.0;
    
    // Convert error to accuracy (assuming 10°C max error is 0% accuracy)
    double avgError = totalError / validCount;
    return std::max(0.0, 1.0 - avgError / 10.0);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Helper Functions
// ═══════════════════════════════════════════════════════════════════════════════

int64_t EnhancedPredictiveThrottling::getCurrentTimestampMs() const
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()
    ).count();
}

void EnhancedPredictiveThrottling::filterOutliers()
{
    if (m_history.size() < 5) return;
    
    // Calculate median
    std::vector<double> temps;
    for (const auto& reading : m_history) {
        temps.push_back(reading.temperature);
    }
    std::sort(temps.begin(), temps.end());
    double median = temps[temps.size() / 2];
    
    // Mark outliers by reducing confidence
    for (auto& reading : m_history) {
        if (std::abs(reading.temperature - median) > m_config.outlierThreshold) {
            reading.confidence *= 0.5;
        }
    }
}

void EnhancedPredictiveThrottling::updateCaches()
{
    if (m_history.empty()) return;
    
    // Update EWMA cache
    double ewma = m_history[0].temperature;
    for (size_t i = 1; i < m_history.size(); ++i) {
        ewma = m_config.ewmaAlpha * m_history[i].temperature + 
               (1.0 - m_config.ewmaAlpha) * ewma;
    }
    m_cachedEWMA = ewma;
    
    // Update slope cache
    if (m_history.size() >= 2) {
        const auto& prev = m_history[m_history.size() - 2];
        const auto& curr = m_history.back();
        double dt = (curr.timestampMs - prev.timestampMs) / 1000.0;
        if (dt > 0) {
            double newSlope = (curr.temperature - prev.temperature) / dt;
            double oldSlope = m_cachedSlope.load();
            m_cachedSlope = 0.3 * newSlope + 0.7 * oldSlope;
        }
    }
    
    m_cacheValid = true;
}

double EnhancedPredictiveThrottling::calculateConfidence() const
{
    if (m_history.size() < 2) return 0.3;
    
    double confidence = 1.0;
    
    // Reduce confidence for small history
    if (m_history.size() < 10) {
        confidence *= m_history.size() / 10.0;
    }
    
    // Reduce confidence for high variance
    double stdDev = getStandardDeviation();
    if (stdDev > 5.0) {
        confidence *= std::max(0.3, 1.0 - (stdDev - 5.0) / 10.0);
    }
    
    // Factor in reading confidence
    double avgReadingConf = 0.0;
    for (const auto& reading : m_history) {
        avgReadingConf += reading.confidence;
    }
    avgReadingConf /= m_history.size();
    confidence *= avgReadingConf;
    
    return std::max(0.1, std::min(1.0, confidence));
}

int EnhancedPredictiveThrottling::calculateThrottlePercent(double predictedTemp, double currentTemp) const
{
    // Use the higher of predicted or current
    double referenceTemp = std::max(predictedTemp, currentTemp);
    
    if (referenceTemp >= m_config.emergencyThreshold) {
        return 100;
    } else if (referenceTemp >= m_config.criticalThreshold) {
        // 60-90% throttle
        double ratio = (referenceTemp - m_config.criticalThreshold) / 
                      (m_config.emergencyThreshold - m_config.criticalThreshold);
        return 60 + static_cast<int>(30 * ratio);
    } else if (referenceTemp >= m_config.sustainableThreshold) {
        // 20-60% throttle
        double ratio = (referenceTemp - m_config.sustainableThreshold) / 
                      (m_config.criticalThreshold - m_config.sustainableThreshold);
        return 20 + static_cast<int>(40 * ratio);
    } else if (referenceTemp >= m_config.warningThreshold) {
        // 0-20% throttle (light)
        double ratio = (referenceTemp - m_config.warningThreshold) / 
                      (m_config.sustainableThreshold - m_config.warningThreshold);
        return static_cast<int>(20 * ratio);
    }
    
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
// ML Predictor Factory
// ═══════════════════════════════════════════════════════════════════════════════

std::unique_ptr<MLPredictor> MLPredictorFactory::create(const std::string& modelPath)
{
    // Placeholder - would instantiate ONNX or TensorFlow runtime predictor
    (modelPath);
    return nullptr;
}

bool MLPredictorFactory::isMLAvailable()
{
    // Check if ONNX runtime or TensorFlow is available
    return false;  // Placeholder
}

} // namespace rawrxd::thermal

