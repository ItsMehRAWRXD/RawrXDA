/**
 * @file EnhancedPredictiveThrottling.hpp
 * @brief ML-Ready Predictive Thermal Throttling System
 * 
 * Implements multi-algorithm temperature prediction with EWMA baseline
 * and optional ONNX/TensorFlow model support for advanced prediction.
 * 
 * @copyright RawrXD IDE 2026
 */

#pragma once

#include <vector>
#include <deque>
#include <memory>
#include <functional>
#include <mutex>
#include <atomic>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <numeric>


namespace rawrxd::thermal {

// Forward declarations
class MLPredictor;

/**
 * @brief Prediction algorithm selection
 */
enum class PredictionAlgorithm {
    EWMA,                   // Exponentially Weighted Moving Average (default)
    LinearRegression,       // Simple linear regression
    DoubleExponential,      // Holt's double exponential smoothing
    TripleExponential,      // Holt-Winters triple exponential
    NeuralNetwork,          // ONNX/TensorFlow neural network
    Ensemble                // Weighted ensemble of multiple algorithms
};

/**
 * @brief Temperature reading with metadata
 */
struct TemperatureReading {
    double temperature;             // Temperature in Celsius
    int64_t timestampMs;           // Unix timestamp in milliseconds
    int sensorIndex;               // Which sensor (0=aggregate, 1-5=NVMe, 6=GPU, 7=CPU)
    double confidence;             // Reading confidence (0-1)
    
    TemperatureReading()
        : temperature(0), timestampMs(0), sensorIndex(0), confidence(1.0) {}
    
    TemperatureReading(double temp, int64_t ts = 0, int sensor = 0)
        : temperature(temp), timestampMs(ts), sensorIndex(sensor), confidence(1.0) {}
};

/**
 * @brief Prediction result with detailed metrics
 */
struct EnhancedPrediction {
    double predictedTemp;           // Predicted temperature
    double confidence;              // Overall prediction confidence
    double slope;                   // Temperature change rate (°C/sec)
    double acceleration;            // Rate of change of slope
    int64_t horizonMs;              // Prediction horizon
    PredictionAlgorithm algorithm;  // Algorithm used
    bool isValid;                   // Prediction validity
    
    // Uncertainty bounds
    double upperBound;              // 95% confidence upper bound
    double lowerBound;              // 95% confidence lower bound
    
    // Algorithm-specific metrics
    double ewmaValue;
    double regressionSlope;
    double regressionIntercept;
    double r2Score;                 // Regression fit quality
    
    EnhancedPrediction()
        : predictedTemp(0), confidence(0), slope(0), acceleration(0)
        , horizonMs(0), algorithm(PredictionAlgorithm::EWMA), isValid(false)
        , upperBound(0), lowerBound(0), ewmaValue(0)
        , regressionSlope(0), regressionIntercept(0), r2Score(0) {}
};

/**
 * @brief Throttle recommendation based on prediction
 */
struct ThrottleRecommendation {
    int throttlePercent;            // Recommended throttle (0-100)
    bool shouldThrottle;            // Whether throttling is needed
    bool isPredictive;              // Based on prediction vs current temp
    double timeToThreshold;         // Seconds until threshold reached
    std::string reason;                 // Human-readable explanation
    
    ThrottleRecommendation()
        : throttlePercent(0), shouldThrottle(false), isPredictive(false)
        , timeToThreshold(0) {}
};

/**
 * @brief Enhanced configuration for predictive throttling
 */
struct EnhancedPredictiveConfig {
    // Algorithm selection
    PredictionAlgorithm primaryAlgorithm = PredictionAlgorithm::EWMA;
    bool enableEnsemble = false;
    
    // EWMA parameters
    double ewmaAlpha = 0.3;                 // Smoothing factor (0-1)
    
    // Double/Triple exponential parameters
    double trendAlpha = 0.1;                // Trend smoothing
    double seasonalAlpha = 0.05;            // Seasonal smoothing
    int seasonalPeriod = 60;                // Seasonal period (samples)
    
    // History and prediction
    size_t historySize = 30;                // Number of readings to maintain
    int64_t predictionHorizonMs = 5000;     // How far ahead to predict
    int64_t samplingIntervalMs = 1000;      // Expected sample interval
    
    // Thermal thresholds
    double sustainableThreshold = 59.5;     // Sustainable mode ceiling
    double warningThreshold = 55.0;         // Warning level
    double criticalThreshold = 65.0;        // Critical level
    double emergencyThreshold = 75.0;       // Emergency shutdown level
    
    // Confidence and filtering
    double minConfidenceForAction = 0.6;    // Min confidence to act
    double outlierThreshold = 10.0;         // °C deviation to flag outlier
    bool enableOutlierFiltering = true;
    
    // Neural network settings (if enabled)
    std::string modelPath;                      // Path to ONNX model
    bool enableGPUInference = false;
};

/**
 * @brief Callback type for throttle events
 */
using ThrottleEventCallback = std::function<void(const ThrottleRecommendation&)>;
using PredictionCallback = std::function<void(const EnhancedPrediction&)>;

/**
 * @brief Enhanced Predictive Throttling Engine
 * 
 * Multi-algorithm temperature prediction system with ML support.
 */
class EnhancedPredictiveThrottling {
public:
    // ═══════════════════════════════════════════════════════════════════════
    // Construction
    // ═══════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Default constructor
     */
    EnhancedPredictiveThrottling();
    
    /**
     * @brief Construct with configuration
     */
    explicit EnhancedPredictiveThrottling(const EnhancedPredictiveConfig& config);
    
    /**
     * @brief Destructor
     */
    ~EnhancedPredictiveThrottling();
    
    // ═══════════════════════════════════════════════════════════════════════
    // Configuration
    // ═══════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Update configuration
     */
    void setConfig(const EnhancedPredictiveConfig& config);
    
    /**
     * @brief Get current configuration
     */
    EnhancedPredictiveConfig getConfig() const;
    
    /**
     * @brief Set prediction algorithm
     */
    void setAlgorithm(PredictionAlgorithm algorithm);
    
    /**
     * @brief Set thermal threshold
     */
    void setThreshold(double threshold);
    
    // ═══════════════════════════════════════════════════════════════════════
    // Data Input
    // ═══════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Add temperature reading
     */
    void addReading(double temperature, int64_t timestampMs = 0);
    
    /**
     * @brief Add reading with sensor index
     */
    void addReading(const TemperatureReading& reading);
    
    /**
     * @brief Add multiple readings (batch update)
     */
    void addReadings(const std::vector<TemperatureReading>& readings);
    
    /**
     * @brief Clear all history
     */
    void clearHistory();
    
    // ═══════════════════════════════════════════════════════════════════════
    // Prediction
    // ═══════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Predict temperature at horizon
     */
    EnhancedPrediction predict();
    
    /**
     * @brief Predict at specific horizon
     */
    EnhancedPrediction predict(int64_t horizonMs);
    
    /**
     * @brief Get current EWMA value
     */
    double getEWMA() const;
    
    /**
     * @brief Get temperature slope
     */
    double getSlope() const;
    
    /**
     * @brief Get acceleration (slope of slope)
     */
    double getAcceleration() const;
    
    // ═══════════════════════════════════════════════════════════════════════
    // Throttle Recommendations
    // ═══════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Get throttle recommendation
     */
    ThrottleRecommendation getRecommendation();
    
    /**
     * @brief Get recommendation with current temperature
     */
    ThrottleRecommendation getRecommendation(double currentTemp);
    
    /**
     * @brief Calculate time until threshold
     */
    double timeToThreshold() const;
    
    // ═══════════════════════════════════════════════════════════════════════
    // Callbacks
    // ═══════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Set throttle event callback
     */
    void setThrottleCallback(ThrottleEventCallback callback);
    
    /**
     * @brief Set prediction callback
     */
    void setPredictionCallback(PredictionCallback callback);
    
    // ═══════════════════════════════════════════════════════════════════════
    // Statistics
    // ═══════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Get history size
     */
    size_t getHistorySize() const;
    
    /**
     * @brief Get current temperature (latest reading)
     */
    double getCurrentTemperature() const;
    
    /**
     * @brief Get min/max/avg over history
     */
    double getMinTemperature() const;
    double getMaxTemperature() const;
    double getAverageTemperature() const;
    double getStandardDeviation() const;
    
    /**
     * @brief Get prediction accuracy (if validation data available)
     */
    double getPredictionAccuracy() const;

private:
    // ═══════════════════════════════════════════════════════════════════════
    // Algorithm Implementations
    // ═══════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Calculate EWMA prediction
     */
    EnhancedPrediction predictEWMA(int64_t horizonMs);
    
    /**
     * @brief Calculate linear regression prediction
     */
    EnhancedPrediction predictLinearRegression(int64_t horizonMs);
    
    /**
     * @brief Calculate double exponential (Holt's) prediction
     */
    EnhancedPrediction predictDoubleExponential(int64_t horizonMs);
    
    /**
     * @brief Calculate triple exponential (Holt-Winters) prediction
     */
    EnhancedPrediction predictTripleExponential(int64_t horizonMs);
    
    /**
     * @brief Calculate neural network prediction
     */
    EnhancedPrediction predictNeuralNetwork(int64_t horizonMs);
    
    /**
     * @brief Calculate ensemble prediction
     */
    EnhancedPrediction predictEnsemble(int64_t horizonMs);
    
    // ═══════════════════════════════════════════════════════════════════════
    // Helper Functions
    // ═══════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Get current timestamp
     */
    int64_t getCurrentTimestampMs() const;
    
    /**
     * @brief Filter outliers from readings
     */
    void filterOutliers();
    
    /**
     * @brief Update internal caches
     */
    void updateCaches();
    
    /**
     * @brief Calculate confidence based on data quality
     */
    double calculateConfidence() const;
    
    /**
     * @brief Calculate throttle percent from prediction
     */
    int calculateThrottlePercent(double predictedTemp, double currentTemp) const;
    
    // ═══════════════════════════════════════════════════════════════════════
    // Member Variables
    // ═══════════════════════════════════════════════════════════════════════
    
    EnhancedPredictiveConfig m_config;
    std::deque<TemperatureReading> m_history;
    mutable std::mutex m_mutex;
    
    // Cached calculations
    std::atomic<double> m_cachedEWMA{0.0};
    std::atomic<double> m_cachedSlope{0.0};
    std::atomic<double> m_cachedAcceleration{0.0};
    std::atomic<bool> m_cacheValid{false};
    
    // Double exponential state
    double m_level = 0.0;
    double m_trend = 0.0;
    
    // Triple exponential state
    std::vector<double> m_seasonal;
    int m_seasonalIndex = 0;
    
    // Neural network predictor (lazy initialized)
    std::unique_ptr<MLPredictor> m_mlPredictor;
    
    // Callbacks
    ThrottleEventCallback m_throttleCallback;
    PredictionCallback m_predictionCallback;
    
    // Validation data for accuracy tracking
    std::deque<std::pair<EnhancedPrediction, double>> m_validationData;
};

// ═══════════════════════════════════════════════════════════════════════════════
// ML Predictor Interface (for ONNX/TensorFlow)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Abstract ML predictor interface
 */
class MLPredictor {
public:
    virtual ~MLPredictor() = default;
    
    /**
     * @brief Load model from file
     */
    virtual bool loadModel(const std::string& modelPath) = 0;
    
    /**
     * @brief Check if model is loaded
     */
    virtual bool isLoaded() const = 0;
    
    /**
     * @brief Run prediction
     */
    virtual double predict(const std::vector<double>& history, int64_t horizonMs) = 0;
    
    /**
     * @brief Get model confidence
     */
    virtual double getConfidence() const = 0;
};

/**
 * @brief Factory for creating ML predictors
 */
class MLPredictorFactory {
public:
    /**
     * @brief Create appropriate predictor for model file
     */
    static std::unique_ptr<MLPredictor> create(const std::string& modelPath);
    
    /**
     * @brief Check if ML support is available
     */
    static bool isMLAvailable();
};

} // namespace rawrxd::thermal

