// ml_error_detector.h - Machine Learning-Based Error Detection and Prediction
#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QMap>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutex>
#include <QElapsedTimer>
#include <memory>
#include <vector>
#include <unordered_map>

namespace MLErrorDetection {

/**
 * @brief Anomaly detection using statistical methods
 */
struct AnomalyDetectionResult {
    bool isAnomaly;
    double anomalyScore;        // 0.0 to 1.0
    QString detectionMethod;    // "zscore", "isolation_forest", "moving_average"
    QVector<QString> features;
    QDateTime timestamp;
    QJsonObject metadata;
};

/**
 * @brief ML feature vector for error pattern learning
 */
struct ErrorFeatureVector {
    // Lexical features
    int messageLength;
    int uniqueWordCount;
    int numberCount;
    int specialCharCount;
    
    // Contextual features
    int callStackDepth;
    int filePathDepth;
    bool hasLineNumber;
    bool hasException;
    
    // Temporal features
    qint64 timeSinceLastError;
    int errorsInLastMinute;
    int errorsInLastHour;
    
    // Semantic features (extracted via embeddings if available)
    QVector<double> semanticEmbedding;
    
    QJsonObject toJson() const;
    static ErrorFeatureVector fromJson(const QJsonObject& json);
};

/**
 * @brief Trained error classification model
 */
struct ErrorClassificationModel {
    QString modelId;
    QString modelType;          // "naive_bayes", "decision_tree", "neural_net"
    QDateTime trainedAt;
    int sampleCount;
    double accuracy;
    double precision;
    double recall;
    double f1Score;
    
    // Model parameters (stored as JSON for flexibility)
    QJsonObject parameters;
    
    // Feature importance weights
    QMap<QString, double> featureWeights;
};

/**
 * @brief Error prediction result
 */
struct ErrorPrediction {
    QString predictedCategory;
    double confidence;
    QVector<QPair<QString, double>> categoryProbabilities;
    QStringList predictedCauses;
    QStringList predictedSolutions;
    bool requiresImmediateAttention;
    double severityScore;
};

/**
 * @brief Real-time error stream analysis
 */
struct ErrorStreamMetrics {
    qint64 errorRate;               // errors per second
    double errorRateChange;         // percentage change
    QVector<QString> topErrorTypes;
    QMap<QString, int> errorDistribution;
    double systemStability;         // 0.0 to 1.0
    bool isUnderAttack;             // Potential DoS or malicious activity
    QDateTime lastUpdated;
};

/**
 * @brief Machine Learning Error Detector
 * 
 * Provides advanced error detection using:
 * - Statistical anomaly detection
 * - Pattern recognition with ML models
 * - Predictive error analysis
 * - Real-time stream processing
 */
class MLErrorDetector : public QObject {
    Q_OBJECT

public:
    explicit MLErrorDetector(QObject* parent = nullptr);
    ~MLErrorDetector();

    // Initialization
    void initialize(const QString& modelStoragePath = QString());
    bool loadModel(const QString& modelPath);
    bool saveModel(const QString& modelPath);

    // Real-time detection
    AnomalyDetectionResult detectAnomaly(const QString& errorMessage, const QJsonObject& context);
    ErrorPrediction predictErrorCategory(const QString& errorMessage, const QJsonObject& context);
    ErrorFeatureVector extractFeatures(const QString& errorMessage, const QJsonObject& context);
    
    // Pattern learning
    void learnFromError(const QString& errorMessage, const QString& category, 
                       const QJsonObject& context, bool wasResolved);
    void updateModelIncremental(const ErrorFeatureVector& features, const QString& label);
    bool trainModelBatch(const QVector<QPair<ErrorFeatureVector, QString>>& trainingData);
    
    // Anomaly detection methods
    AnomalyDetectionResult detectUsingZScore(const QVector<double>& metrics);
    AnomalyDetectionResult detectUsingMovingAverage(const QVector<double>& recentValues, double currentValue);
    AnomalyDetectionResult detectUsingIsolationForest(const ErrorFeatureVector& features);
    
    // Stream analysis
    void processErrorStream(const QString& errorMessage, const QDateTime& timestamp);
    ErrorStreamMetrics getStreamMetrics() const;
    void resetStreamMetrics();
    
    // Model management
    QVector<ErrorClassificationModel> getAvailableModels() const;
    ErrorClassificationModel getCurrentModel() const;
    bool setActiveModel(const QString& modelId);
    double evaluateModelAccuracy(const QVector<QPair<ErrorFeatureVector, QString>>& testData);
    
    // Semantic analysis (if inference engine available)
    void setInferenceEngine(QObject* engine);
    QVector<double> generateSemanticEmbedding(const QString& text);
    double calculateSemanticSimilarity(const QString& error1, const QString& error2);
    
    // Statistics and reporting
    QJsonObject getDetectionStatistics() const;
    QJsonArray getRecentAnomalies(int count = 10) const;
    void exportTrainingData(const QString& outputPath);

signals:
    void anomalyDetected(const AnomalyDetectionResult& result);
    void errorPredicted(const ErrorPrediction& prediction);
    void modelTrained(const QString& modelId, double accuracy);
    void streamMetricsUpdated(const ErrorStreamMetrics& metrics);
    void highRiskPatternDetected(const QString& pattern, double risk);

private:
    // Internal methods
    void initializeDefaultModel();
    void updateAnomalyBaseline(double value);
    double calculateZScore(double value, const QVector<double>& dataset);
    double calculateMovingAverage(const QVector<double>& values, int window);
    QVector<QString> tokenizeMessage(const QString& message);
    QMap<QString, double> calculateFeatureImportance();
    
    // Naive Bayes classifier
    QString classifyNaiveBayes(const ErrorFeatureVector& features);
    double calculateProbability(const ErrorFeatureVector& features, const QString& category);
    
    // Decision tree classifier (simple implementation)
    QString classifyDecisionTree(const ErrorFeatureVector& features);
    
    // Neural network (simple feedforward)
    QVector<double> forwardPass(const QVector<double>& inputs);
    void backpropagate(const QVector<double>& expected, double learningRate);
    
    // Stream processing
    void updateStreamWindow();
    bool detectStreamAnomaly();
    
    // Data structures
    mutable QMutex m_mutex;
    QString m_modelStoragePath;
    
    // Current active model
    ErrorClassificationModel m_activeModel;
    QVector<ErrorClassificationModel> m_availableModels;
    
    // Training data storage
    QVector<QPair<ErrorFeatureVector, QString>> m_trainingData;
    QMap<QString, QVector<ErrorFeatureVector>> m_categoryExamples;
    
    // Anomaly detection state
    QVector<double> m_baselineValues;
    QVector<AnomalyDetectionResult> m_recentAnomalies;
    double m_baselineMean;
    double m_baselineStdDev;
    
    // Stream processing
    ErrorStreamMetrics m_streamMetrics;
    QVector<QPair<QString, QDateTime>> m_errorStream;
    QElapsedTimer m_streamTimer;
    
    // Naive Bayes parameters
    QMap<QString, double> m_classPriors;  // P(class)
    QMap<QString, QMap<QString, double>> m_featureLikelihoods;  // P(feature|class)
    
    // Neural network weights (simple 2-layer network)
    std::vector<std::vector<double>> m_hiddenWeights;
    std::vector<std::vector<double>> m_outputWeights;
    std::vector<double> m_hiddenBiases;
    std::vector<double> m_outputBiases;
    
    // Inference engine (optional)
    QObject* m_inferenceEngine;
    
    // Statistics
    qint64 m_totalErrorsProcessed;
    qint64 m_anomaliesDetected;
    qint64 m_correctPredictions;
    qint64 m_totalPredictions;
    QDateTime m_startTime;
};

/**
 * @brief Ensemble error detector combining multiple models
 */
class EnsembleErrorDetector : public QObject {
    Q_OBJECT

public:
    explicit EnsembleErrorDetector(QObject* parent = nullptr);
    
    void addDetector(MLErrorDetector* detector, double weight = 1.0);
    ErrorPrediction predictWithEnsemble(const QString& errorMessage, const QJsonObject& context);
    AnomalyDetectionResult detectWithEnsemble(const QString& errorMessage, const QJsonObject& context);
    
    QVector<QPair<MLErrorDetector*, double>> getDetectors() const { return m_detectors; }

private:
    QVector<QPair<MLErrorDetector*, double>> m_detectors;  // detector and weight
};

} // namespace MLErrorDetection
