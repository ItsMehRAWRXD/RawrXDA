// ml_error_detector.cpp - Complete ML Error Detection Implementation
#include "ml_error_detector.h"
#include "../logging/structured_logger.h"
#include "../qtapp/integration/ProdIntegration.h"
#include "../qtapp/integration/InitializationTracker.h"
#include <QFile>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QDir>
#include <QtMath>
#include <algorithm>
#include <numeric>
#include <random>
#include <cmath>

namespace MLErrorDetection {

// ============================================================================
// ErrorFeatureVector Implementation
// ============================================================================

QJsonObject ErrorFeatureVector::toJson() const {
    QJsonObject obj;
    obj["messageLength"] = messageLength;
    obj["uniqueWordCount"] = uniqueWordCount;
    obj["numberCount"] = numberCount;
    obj["specialCharCount"] = specialCharCount;
    obj["callStackDepth"] = callStackDepth;
    obj["filePathDepth"] = filePathDepth;
    obj["hasLineNumber"] = hasLineNumber;
    obj["hasException"] = hasException;
    obj["timeSinceLastError"] = static_cast<double>(timeSinceLastError);
    obj["errorsInLastMinute"] = errorsInLastMinute;
    obj["errorsInLastHour"] = errorsInLastHour;
    
    QJsonArray embedding;
    for (double val : semanticEmbedding) {
        embedding.append(val);
    }
    obj["semanticEmbedding"] = embedding;
    
    return obj;
}

ErrorFeatureVector ErrorFeatureVector::fromJson(const QJsonObject& json) {
    ErrorFeatureVector vec;
    vec.messageLength = json["messageLength"].toInt();
    vec.uniqueWordCount = json["uniqueWordCount"].toInt();
    vec.numberCount = json["numberCount"].toInt();
    vec.specialCharCount = json["specialCharCount"].toInt();
    vec.callStackDepth = json["callStackDepth"].toInt();
    vec.filePathDepth = json["filePathDepth"].toInt();
    vec.hasLineNumber = json["hasLineNumber"].toBool();
    vec.hasException = json["hasException"].toBool();
    vec.timeSinceLastError = static_cast<qint64>(json["timeSinceLastError"].toDouble());
    vec.errorsInLastMinute = json["errorsInLastMinute"].toInt();
    vec.errorsInLastHour = json["errorsInLastHour"].toInt();
    
    QJsonArray embedding = json["semanticEmbedding"].toArray();
    for (const QJsonValue& val : embedding) {
        vec.semanticEmbedding.append(val.toDouble());
    }
    
    return vec;
}

// ============================================================================
// MLErrorDetector Implementation
// ============================================================================

MLErrorDetector::MLErrorDetector(QObject* parent)
    : QObject(parent)
    , m_inferenceEngine(nullptr)
    , m_totalErrorsProcessed(0)
    , m_anomaliesDetected(0)
    , m_correctPredictions(0)
    , m_totalPredictions(0)
    , m_baselineMean(0.0)
    , m_baselineStdDev(1.0)
{
    RawrXD::Integration::ScopedInitTimer initTimer("MLErrorDetector");
    m_startTime = QDateTime::currentDateTime();
    m_streamTimer.start();
    
    LOG_INFO("ML Error Detector initialized");
}

MLErrorDetector::~MLErrorDetector() {
    RawrXD::Integration::ScopedTimer timer("MLErrorDetector", "destruction", "cleanup");
    LOG_INFO("ML Error Detector destroyed", {
        {"total_errors_processed", m_totalErrorsProcessed},
        {"anomalies_detected", m_anomaliesDetected},
        {"accuracy", m_totalPredictions > 0 ? (double)m_correctPredictions / m_totalPredictions : 0.0}
    });
}

void MLErrorDetector::initialize(const QString& modelStoragePath) {
    RawrXD::Integration::ScopedTimer timer("MLErrorDetector", "initialize", "init_op");
    QMutexLocker lock(&m_mutex);
    
    if (modelStoragePath.isEmpty()) {
        m_modelStoragePath = QDir::currentPath() + "/ml_models";
    } else {
        m_modelStoragePath = modelStoragePath;
    }
    
    // Create model storage directory
    QDir dir(m_modelStoragePath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    // Initialize default model
    initializeDefaultModel();
    
    // Try to load existing model
    QString defaultModelPath = m_modelStoragePath + "/default_model.json";
    if (QFile::exists(defaultModelPath)) {
        loadModel(defaultModelPath);
    }
    
    LOG_INFO("ML Error Detector initialized", {
        {"model_storage_path", m_modelStoragePath},
        {"model_loaded", QFile::exists(defaultModelPath)}
    });
}

void MLErrorDetector::initializeDefaultModel() {
    RawrXD::Integration::ScopedTimer timer("MLErrorDetector", "initializeDefaultModel", "model_setup");
    m_activeModel.modelId = "default_naive_bayes";
    m_activeModel.modelType = "naive_bayes";
    m_activeModel.trainedAt = QDateTime::currentDateTime();
    m_activeModel.sampleCount = 0;
    m_activeModel.accuracy = 0.0;
    
    // Initialize with common error categories
    QStringList categories = {"Runtime", "Syntax", "Memory", "Network", "FileSystem", "Logic"};
    for (const QString& category : categories) {
        m_classPriors[category] = 1.0 / categories.size();
    }
    
    // Initialize neural network (2-layer: 20 hidden, 6 output)
    const int inputSize = 20;
    const int hiddenSize = 20;
    const int outputSize = 6;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> dis(0.0, 0.1);
    
    m_hiddenWeights.resize(inputSize);
    for (int i = 0; i < inputSize; ++i) {
        m_hiddenWeights[i].resize(hiddenSize);
        for (int j = 0; j < hiddenSize; ++j) {
            m_hiddenWeights[i][j] = dis(gen);
        }
    }
    
    m_outputWeights.resize(hiddenSize);
    for (int i = 0; i < hiddenSize; ++i) {
        m_outputWeights[i].resize(outputSize);
        for (int j = 0; j < outputSize; ++j) {
            m_outputWeights[i][j] = dis(gen);
        }
    }
    
    m_hiddenBiases.resize(hiddenSize, 0.0);
    m_outputBiases.resize(outputSize, 0.0);
}

ErrorFeatureVector MLErrorDetector::extractFeatures(const QString& errorMessage, const QJsonObject& context) {
    ErrorFeatureVector features;
    
    // Lexical features
    features.messageLength = errorMessage.length();
    
    QStringList words = tokenizeMessage(errorMessage);
    QSet<QString> uniqueWords(words.begin(), words.end());
    features.uniqueWordCount = uniqueWords.size();
    
    QRegularExpression numberRegex("\\d+");
    features.numberCount = errorMessage.count(numberRegex);
    
    QRegularExpression specialCharRegex("[^a-zA-Z0-9\\s]");
    features.specialCharCount = errorMessage.count(specialCharRegex);
    
    // Contextual features
    if (context.contains("callStack")) {
        QJsonArray callStack = context["callStack"].toArray();
        features.callStackDepth = callStack.size();
    } else {
        features.callStackDepth = 0;
    }
    
    if (context.contains("sourceFile")) {
        QString sourceFile = context["sourceFile"].toString();
        features.filePathDepth = sourceFile.count('/') + sourceFile.count('\\');
    } else {
        features.filePathDepth = 0;
    }
    
    features.hasLineNumber = context.contains("lineNumber");
    features.hasException = errorMessage.contains("exception", Qt::CaseInsensitive) ||
                            errorMessage.contains("error", Qt::CaseInsensitive);
    
    // Temporal features
    static QDateTime lastErrorTime = QDateTime::currentDateTime();
    features.timeSinceLastError = lastErrorTime.msecsTo(QDateTime::currentDateTime());
    lastErrorTime = QDateTime::currentDateTime();
    
    // Count recent errors
    QDateTime now = QDateTime::currentDateTime();
    QDateTime oneMinuteAgo = now.addSecs(-60);
    QDateTime oneHourAgo = now.addSecs(-3600);
    
    features.errorsInLastMinute = 0;
    features.errorsInLastHour = 0;
    
    for (const auto& pair : m_errorStream) {
        if (pair.second >= oneMinuteAgo) {
            features.errorsInLastMinute++;
        }
        if (pair.second >= oneHourAgo) {
            features.errorsInLastHour++;
        }
    }
    
    // Generate semantic embedding if inference engine available
    if (m_inferenceEngine) {
        features.semanticEmbedding = generateSemanticEmbedding(errorMessage);
    } else {
        // Use simple TF-IDF-like embedding
        features.semanticEmbedding.resize(10, 0.0);
        for (int i = 0; i < qMin(10, words.size()); ++i) {
            features.semanticEmbedding[i] = words[i].length() / 100.0;
        }
    }
    
    return features;
}

AnomalyDetectionResult MLErrorDetector::detectAnomaly(const QString& errorMessage, const QJsonObject& context) {
    RawrXD::Integration::ScopedTimer timer("MLErrorDetector", "detectAnomaly", "inference");
    QMutexLocker lock(&m_mutex);
    
    ErrorFeatureVector features = extractFeatures(errorMessage, context);
    
    // Try multiple detection methods
    AnomalyDetectionResult zscoreResult = detectUsingZScore({
        static_cast<double>(features.messageLength),
        static_cast<double>(features.errorsInLastMinute),
        static_cast<double>(features.callStackDepth)
    });
    
    AnomalyDetectionResult isolationResult = detectUsingIsolationForest(features);
    
    // Combine results (use highest anomaly score)
    AnomalyDetectionResult result;
    if (zscoreResult.anomalyScore > isolationResult.anomalyScore) {
        result = zscoreResult;
    } else {
        result = isolationResult;
    }
    
    result.timestamp = QDateTime::currentDateTime();
    
    if (result.isAnomaly) {
        m_anomaliesDetected++;
        m_recentAnomalies.append(result);
        
        // Keep only last 100 anomalies
        if (m_recentAnomalies.size() > 100) {
            m_recentAnomalies.remove(0);
        }
        
        emit anomalyDetected(result);
        
        LOG_WARN("Anomaly detected", {
            {"error_message", errorMessage},
            {"anomaly_score", result.anomalyScore},
            {"detection_method", result.detectionMethod}
        });
    }
    
    m_totalErrorsProcessed++;
    
    return result;
}

AnomalyDetectionResult MLErrorDetector::detectUsingZScore(const QVector<double>& metrics) {
    AnomalyDetectionResult result;
    result.detectionMethod = "zscore";
    result.isAnomaly = false;
    result.anomalyScore = 0.0;
    
    if (metrics.isEmpty()) {
        return result;
    }
    
    double maxZScore = 0.0;
    
    for (double metric : metrics) {
        // Update baseline
        updateAnomalyBaseline(metric);
        
        // Calculate z-score
        double zscore = qAbs(calculateZScore(metric, m_baselineValues));
        
        if (zscore > maxZScore) {
            maxZScore = zscore;
        }
        
        // Threshold: 3 standard deviations
        if (zscore > 3.0) {
            result.isAnomaly = true;
        }
    }
    
    result.anomalyScore = qMin(1.0, maxZScore / 5.0);  // Normalize to 0-1
    
    return result;
}

AnomalyDetectionResult MLErrorDetector::detectUsingMovingAverage(const QVector<double>& recentValues, double currentValue) {
    AnomalyDetectionResult result;
    result.detectionMethod = "moving_average";
    result.isAnomaly = false;
    result.anomalyScore = 0.0;
    
    if (recentValues.size() < 3) {
        return result;
    }
    
    double movingAvg = calculateMovingAverage(recentValues, recentValues.size());
    double deviation = qAbs(currentValue - movingAvg);
    double avgDeviation = 0.0;
    
    for (double val : recentValues) {
        avgDeviation += qAbs(val - movingAvg);
    }
    avgDeviation /= recentValues.size();
    
    // If current value deviates more than 2x average deviation
    if (avgDeviation > 0 && deviation > 2.0 * avgDeviation) {
        result.isAnomaly = true;
        result.anomalyScore = qMin(1.0, deviation / (3.0 * avgDeviation));
    }
    
    return result;
}

AnomalyDetectionResult MLErrorDetector::detectUsingIsolationForest(const ErrorFeatureVector& features) {
    AnomalyDetectionResult result;
    result.detectionMethod = "isolation_forest";
    result.isAnomaly = false;
    result.anomalyScore = 0.0;
    
    // Simplified isolation forest: measure average depth to isolate this point
    // In a real implementation, this would use multiple random trees
    
    double isolationScore = 0.0;
    int comparisons = 0;
    
    // Compare with existing training data
    for (const auto& pair : m_trainingData) {
        const ErrorFeatureVector& trainingFeature = pair.first;
        
        // Calculate distance
        double distance = 0.0;
        distance += qAbs(features.messageLength - trainingFeature.messageLength) / 1000.0;
        distance += qAbs(features.uniqueWordCount - trainingFeature.uniqueWordCount) / 100.0;
        distance += qAbs(features.callStackDepth - trainingFeature.callStackDepth) / 50.0;
        distance += qAbs(features.errorsInLastMinute - trainingFeature.errorsInLastMinute) / 10.0;
        
        isolationScore += distance;
        comparisons++;
        
        if (comparisons >= 20) break;  // Limit comparisons
    }
    
    if (comparisons > 0) {
        isolationScore /= comparisons;
        
        // If average distance is high, it's an anomaly
        if (isolationScore > 0.7) {
            result.isAnomaly = true;
            result.anomalyScore = qMin(1.0, isolationScore);
        }
    }
    
    return result;
}

ErrorPrediction MLErrorDetector::predictErrorCategory(const QString& errorMessage, const QJsonObject& context) {
    RawrXD::Integration::ScopedTimer timer("MLErrorDetector", "predictErrorCategory", "inference");
    QMutexLocker lock(&m_mutex);
    
    ErrorFeatureVector features = extractFeatures(errorMessage, context);
    
    ErrorPrediction prediction;
    prediction.confidence = 0.0;
    
    // Use Naive Bayes classifier
    QString predictedCategory = classifyNaiveBayes(features);
    prediction.predictedCategory = predictedCategory;
    
    // Calculate probabilities for all categories
    double totalProb = 0.0;
    QMap<QString, double> probabilities;
    
    for (const QString& category : m_classPriors.keys()) {
        double prob = calculateProbability(features, category);
        probabilities[category] = prob;
        totalProb += prob;
    }
    
    // Normalize probabilities
    if (totalProb > 0) {
        for (auto it = probabilities.begin(); it != probabilities.end(); ++it) {
            it.value() /= totalProb;
            prediction.categoryProbabilities.append(qMakePair(it.key(), it.value()));
        }
        
        // Sort by probability
        std::sort(prediction.categoryProbabilities.begin(), prediction.categoryProbabilities.end(),
                 [](const QPair<QString, double>& a, const QPair<QString, double>& b) {
                     return a.second > b.second;
                 });
        
        prediction.confidence = prediction.categoryProbabilities.first().second;
    }
    
    // Add predicted solutions based on category
    if (prediction.predictedCategory == "Runtime") {
        prediction.predictedSolutions << "Check for null pointer dereferences";
        prediction.predictedSolutions << "Verify array bounds";
        prediction.predictedCauses << "Unhandled exception";
    } else if (prediction.predictedCategory == "Memory") {
        prediction.predictedSolutions << "Check for memory leaks";
        prediction.predictedSolutions << "Use smart pointers";
        prediction.predictedCauses << "Memory allocation failure";
    } else if (prediction.predictedCategory == "Network") {
        prediction.predictedSolutions << "Check network connectivity";
        prediction.predictedSolutions << "Verify firewall settings";
        prediction.predictedCauses << "Connection timeout";
    }
    
    prediction.severityScore = prediction.confidence * 
                              (features.errorsInLastMinute > 5 ? 1.5 : 1.0);
    prediction.requiresImmediateAttention = (prediction.severityScore > 0.7);
    
    m_totalPredictions++;
    
    emit errorPredicted(prediction);
    
    return prediction;
}

void MLErrorDetector::learnFromError(const QString& errorMessage, const QString& category, 
                                    const QJsonObject& context, bool wasResolved) {
    QMutexLocker lock(&m_mutex);
    
    ErrorFeatureVector features = extractFeatures(errorMessage, context);
    
    // Add to training data
    m_trainingData.append(qMakePair(features, category));
    m_categoryExamples[category].append(features);
    
    // Update model incrementally
    updateModelIncremental(features, category);
    
    // Update class priors
    int totalSamples = m_trainingData.size();
    for (const QString& cat : m_categoryExamples.keys()) {
        int categoryCount = m_categoryExamples[cat].size();
        m_classPriors[cat] = static_cast<double>(categoryCount) / totalSamples;
    }
    
    m_activeModel.sampleCount = totalSamples;
    
    if (wasResolved) {
        m_correctPredictions++;
    }
    
    LOG_DEBUG("Learned from error", {
        {"category", category},
        {"total_samples", totalSamples},
        {"wasResolved", wasResolved}
    });
}

void MLErrorDetector::updateModelIncremental(const ErrorFeatureVector& features, const QString& label) {
    // Update Naive Bayes likelihoods
    // P(feature|class) is updated incrementally
    
    QStringList words = tokenizeMessage(QString::number(features.messageLength));
    
    for (const QString& word : words) {
        if (!m_featureLikelihoods[label].contains(word)) {
            m_featureLikelihoods[label][word] = 1.0;
        } else {
            m_featureLikelihoods[label][word] += 1.0;
        }
    }
}

bool MLErrorDetector::trainModelBatch(const QVector<QPair<ErrorFeatureVector, QString>>& trainingData) {
    RawrXD::Integration::ScopedTimer timer("MLErrorDetector", "trainModelBatch", "training_op");
    QMutexLocker lock(&m_mutex);
    
    if (trainingData.isEmpty()) {
        return false;
    }
    
    LOG_INFO("Training model with batch data", {
        {"sample_count", trainingData.size()}
    });
    
    // Clear and rebuild model
    m_trainingData = trainingData;
    m_categoryExamples.clear();
    m_classPriors.clear();
    m_featureLikelihoods.clear();
    
    // Organize by category
    for (const auto& pair : trainingData) {
        m_categoryExamples[pair.second].append(pair.first);
    }
    
    // Calculate priors
    int totalSamples = trainingData.size();
    for (const QString& category : m_categoryExamples.keys()) {
        int categoryCount = m_categoryExamples[category].size();
        m_classPriors[category] = static_cast<double>(categoryCount) / totalSamples;
    }
    
    // Calculate feature likelihoods (simplified)
    for (const auto& pair : trainingData) {
        updateModelIncremental(pair.first, pair.second);
    }
    
    // Normalize likelihoods
    for (const QString& category : m_featureLikelihoods.keys()) {
        double total = 0.0;
        for (double count : m_featureLikelihoods[category].values()) {
            total += count;
        }
        if (total > 0) {
            for (auto it = m_featureLikelihoods[category].begin(); 
                 it != m_featureLikelihoods[category].end(); ++it) {
                it.value() /= total;
            }
        }
    }
    
    m_activeModel.sampleCount = totalSamples;
    m_activeModel.trainedAt = QDateTime::currentDateTime();
    
    // Evaluate accuracy on training data (ideally use separate validation set)
    int correct = 0;
    for (const auto& pair : trainingData) {
        QString predicted = classifyNaiveBayes(pair.first);
        if (predicted == pair.second) {
            correct++;
        }
    }
    
    m_activeModel.accuracy = static_cast<double>(correct) / totalSamples;
    
    emit modelTrained(m_activeModel.modelId, m_activeModel.accuracy);
    
    LOG_INFO("Model training completed", {
        {"accuracy", m_activeModel.accuracy},
        {"samples", totalSamples}
    });
    
    return true;
}

QString MLErrorDetector::classifyNaiveBayes(const ErrorFeatureVector& features) {
    QString bestCategory;
    double maxProbability = -1.0;
    
    for (const QString& category : m_classPriors.keys()) {
        double probability = calculateProbability(features, category);
        
        if (probability > maxProbability) {
            maxProbability = probability;
            bestCategory = category;
        }
    }
    
    return bestCategory;
}

double MLErrorDetector::calculateProbability(const ErrorFeatureVector& features, const QString& category) {
    if (!m_classPriors.contains(category)) {
        return 0.0;
    }
    
    double logProbability = qLn(m_classPriors[category]);
    
    // Add log probabilities for features (to avoid underflow)
    // Simplified: use message length as key feature
    QString featureKey = QString::number(features.messageLength / 100);
    
    if (m_featureLikelihoods[category].contains(featureKey)) {
        logProbability += qLn(m_featureLikelihoods[category][featureKey] + 1e-10);  // Laplace smoothing
    } else {
        logProbability += qLn(1e-10);  // Small probability for unseen features
    }
    
    return qExp(logProbability);
}

void MLErrorDetector::processErrorStream(const QString& errorMessage, const QDateTime& timestamp) {
    RawrXD::Integration::ScopedTimer timer("MLErrorDetector", "processErrorStream", "stream_processing");
    QMutexLocker lock(&m_mutex);
    
    // Add to stream
    m_errorStream.append(qMakePair(errorMessage, timestamp));
    
    // Update stream window (keep last hour)
    updateStreamWindow();
    
    // Update metrics
    QDateTime now = QDateTime::currentDateTime();
    QDateTime oneSecondAgo = now.addSecs(-1);
    QDateTime oneMinuteAgo = now.addSecs(-60);
    
    m_streamMetrics.errorRate = 0;
    for (const auto& pair : m_errorStream) {
        if (pair.second >= oneSecondAgo) {
            m_streamMetrics.errorRate++;
        }
    }
    
    // Detect stream anomalies
    m_streamMetrics.isUnderAttack = detectStreamAnomaly();
    m_streamMetrics.lastUpdated = now;
    
    // Calculate system stability (inverse of error rate, normalized)
    double normalizedErrorRate = qMin(100.0, static_cast<double>(m_streamMetrics.errorRate));
    m_streamMetrics.systemStability = 1.0 - (normalizedErrorRate / 100.0);
    
    emit streamMetricsUpdated(m_streamMetrics);
}

ErrorStreamMetrics MLErrorDetector::getStreamMetrics() const {
    QMutexLocker lock(&m_mutex);
    return m_streamMetrics;
}

void MLErrorDetector::updateStreamWindow() {
    QDateTime oneHourAgo = QDateTime::currentDateTime().addSecs(-3600);
    
    m_errorStream.erase(
        std::remove_if(m_errorStream.begin(), m_errorStream.end(),
                      [&oneHourAgo](const QPair<QString, QDateTime>& pair) {
                          return pair.second < oneHourAgo;
                      }),
        m_errorStream.end()
    );
}

bool MLErrorDetector::detectStreamAnomaly() {
    // Simple heuristic: if error rate exceeds 50 errors/second, potential attack
    if (m_streamMetrics.errorRate > 50) {
        emit highRiskPatternDetected("High error rate detected", 0.9);
        return true;
    }
    
    return false;
}

QVector<QString> MLErrorDetector::tokenizeMessage(const QString& message) {
    QVector<QString> tokens;
    QStringList words = message.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    
    for (const QString& word : words) {
        QString cleaned = word.toLower().remove(QRegularExpression("[^a-z0-9]"));
        if (!cleaned.isEmpty()) {
            tokens.append(cleaned);
        }
    }
    
    return tokens;
}

void MLErrorDetector::updateAnomalyBaseline(double value) {
    m_baselineValues.append(value);
    
    // Keep only last 1000 values
    if (m_baselineValues.size() > 1000) {
        m_baselineValues.remove(0);
    }
    
    // Recalculate mean and standard deviation
    if (m_baselineValues.size() > 1) {
        double sum = std::accumulate(m_baselineValues.begin(), m_baselineValues.end(), 0.0);
        m_baselineMean = sum / m_baselineValues.size();
        
        double variance = 0.0;
        for (double val : m_baselineValues) {
            variance += (val - m_baselineMean) * (val - m_baselineMean);
        }
        variance /= m_baselineValues.size();
        m_baselineStdDev = qSqrt(variance);
    }
}

double MLErrorDetector::calculateZScore(double value, const QVector<double>& dataset) {
    if (dataset.isEmpty() || m_baselineStdDev == 0.0) {
        return 0.0;
    }
    
    return (value - m_baselineMean) / m_baselineStdDev;
}

double MLErrorDetector::calculateMovingAverage(const QVector<double>& values, int window) {
    if (values.isEmpty()) {
        return 0.0;
    }
    
    int start = qMax(0, values.size() - window);
    double sum = 0.0;
    int count = 0;
    
    for (int i = start; i < values.size(); ++i) {
        sum += values[i];
        count++;
    }
    
    return count > 0 ? sum / count : 0.0;
}

QJsonObject MLErrorDetector::getDetectionStatistics() const {
    QMutexLocker lock(&m_mutex);
    
    QJsonObject stats;
    stats["total_errors_processed"] = static_cast<qint64>(m_totalErrorsProcessed);
    stats["anomalies_detected"] = static_cast<qint64>(m_anomaliesDetected);
    stats["correct_predictions"] = static_cast<qint64>(m_correctPredictions);
    stats["total_predictions"] = static_cast<qint64>(m_totalPredictions);
    stats["accuracy"] = m_totalPredictions > 0 ? 
                       static_cast<double>(m_correctPredictions) / m_totalPredictions : 0.0;
    stats["uptime_seconds"] = m_startTime.secsTo(QDateTime::currentDateTime());
    stats["model_id"] = m_activeModel.modelId;
    stats["model_accuracy"] = m_activeModel.accuracy;
    stats["training_samples"] = m_activeModel.sampleCount;
    
    return stats;
}

bool MLErrorDetector::saveModel(const QString& modelPath) {
    QMutexLocker lock(&m_mutex);
    
    QJsonObject modelJson;
    modelJson["model_id"] = m_activeModel.modelId;
    modelJson["model_type"] = m_activeModel.modelType;
    modelJson["trained_at"] = m_activeModel.trainedAt.toString(Qt::ISODate);
    modelJson["sample_count"] = m_activeModel.sampleCount;
    modelJson["accuracy"] = m_activeModel.accuracy;
    
    // Save class priors
    QJsonObject priors;
    for (auto it = m_classPriors.begin(); it != m_classPriors.end(); ++it) {
        priors[it.key()] = it.value();
    }
    modelJson["class_priors"] = priors;
    
    // Save feature likelihoods
    QJsonObject likelihoods;
    for (auto catIt = m_featureLikelihoods.begin(); catIt != m_featureLikelihoods.end(); ++catIt) {
        QJsonObject features;
        for (auto featIt = catIt.value().begin(); featIt != catIt.value().end(); ++featIt) {
            features[featIt.key()] = featIt.value();
        }
        likelihoods[catIt.key()] = features;
    }
    modelJson["feature_likelihoods"] = likelihoods;
    
    // Write to file
    QFile file(modelPath);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_ERROR("Failed to save model", {{"path", modelPath}});
        return false;
    }
    
    QJsonDocument doc(modelJson);
    file.write(doc.toJson());
    file.close();
    
    LOG_INFO("Model saved successfully", {{"path", modelPath}});
    
    return true;
}

bool MLErrorDetector::loadModel(const QString& modelPath) {
    QFile file(modelPath);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_ERROR("Failed to load model", {{"path", modelPath}});
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        return false;
    }
    
    QMutexLocker lock(&m_mutex);
    
    QJsonObject modelJson = doc.object();
    m_activeModel.modelId = modelJson["model_id"].toString();
    m_activeModel.modelType = modelJson["model_type"].toString();
    m_activeModel.trainedAt = QDateTime::fromString(modelJson["trained_at"].toString(), Qt::ISODate);
    m_activeModel.sampleCount = modelJson["sample_count"].toInt();
    m_activeModel.accuracy = modelJson["accuracy"].toDouble();
    
    // Load class priors
    QJsonObject priors = modelJson["class_priors"].toObject();
    m_classPriors.clear();
    for (auto it = priors.begin(); it != priors.end(); ++it) {
        m_classPriors[it.key()] = it.value().toDouble();
    }
    
    // Load feature likelihoods
    QJsonObject likelihoods = modelJson["feature_likelihoods"].toObject();
    m_featureLikelihoods.clear();
    for (auto catIt = likelihoods.begin(); catIt != likelihoods.end(); ++catIt) {
        QJsonObject features = catIt.value().toObject();
        for (auto featIt = features.begin(); featIt != features.end(); ++featIt) {
            m_featureLikelihoods[catIt.key()][featIt.key()] = featIt.value().toDouble();
        }
    }
    
    LOG_INFO("Model loaded successfully", {
        {"path", modelPath},
        {"accuracy", m_activeModel.accuracy},
        {"samples", m_activeModel.sampleCount}
    });
    
    return true;
}

QVector<double> MLErrorDetector::generateSemanticEmbedding(const QString& text) {
    // Placeholder: In production, use a real embedding model
    // This would call the inference engine to generate embeddings
    QVector<double> embedding(128, 0.0);
    
    // Simple hash-based pseudo-embedding
    QByteArray hash = QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256);
    for (int i = 0; i < qMin(128, hash.size()); ++i) {
        embedding[i] = static_cast<double>(static_cast<unsigned char>(hash[i])) / 255.0;
    }
    
    return embedding;
}

void MLErrorDetector::setInferenceEngine(QObject* engine) {
    m_inferenceEngine = engine;
    LOG_INFO("Inference engine set for ML error detector");
}

// ============================================================================
// EnsembleErrorDetector Implementation
// ============================================================================

EnsembleErrorDetector::EnsembleErrorDetector(QObject* parent)
    : QObject(parent)
{
    RawrXD::Integration::ScopedInitTimer initTimer("EnsembleErrorDetector");
}

void EnsembleErrorDetector::addDetector(MLErrorDetector* detector, double weight) {
    m_detectors.append(qMakePair(detector, weight));
}

ErrorPrediction EnsembleErrorDetector::predictWithEnsemble(const QString& errorMessage, const QJsonObject& context) {
    RawrXD::Integration::ScopedTimer timer("EnsembleErrorDetector", "predictWithEnsemble", "ensemble_prediction");
    QMap<QString, double> voteCounts;
    QMap<QString, QStringList> allSolutions;
    
    double totalWeight = 0.0;
    
    for (const auto& pair : m_detectors) {
        MLErrorDetector* detector = pair.first;
        double weight = pair.second;
        
        ErrorPrediction prediction = detector->predictErrorCategory(errorMessage, context);
        
        // Weighted voting
        voteCounts[prediction.predictedCategory] += weight * prediction.confidence;
        allSolutions[prediction.predictedCategory].append(prediction.predictedSolutions);
        
        totalWeight += weight;
    }
    
    // Find category with highest weighted vote
    QString bestCategory;
    double maxVote = -1.0;
    
    for (auto it = voteCounts.begin(); it != voteCounts.end(); ++it) {
        if (it.value() > maxVote) {
            maxVote = it.value();
            bestCategory = it.key();
        }
    }
    
    ErrorPrediction ensemblePrediction;
    ensemblePrediction.predictedCategory = bestCategory;
    ensemblePrediction.confidence = maxVote / totalWeight;
    ensemblePrediction.predictedSolutions = allSolutions[bestCategory];
    
    return ensemblePrediction;
}

} // namespace MLErrorDetection
