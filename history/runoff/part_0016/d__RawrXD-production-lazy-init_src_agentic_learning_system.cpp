#include "agentic_learning_system.h"
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <algorithm>
#include <cmath>

AgenticLearningSystem::AgenticLearningSystem(QObject* parent)
    : QObject(parent) {
}

AgenticLearningSystem::~AgenticLearningSystem() = default;

void AgenticLearningSystem::initialize() {
    qInfo() << "[AgenticLearning] Initializing cognitive engine...";
    // In production, load from default path
    loadKnowledgeBase("learning_kb.json");
}

void AgenticLearningSystem::recordCompressionPerformance(
    const QString& method,
    size_t inputSize,
    size_t outputSize,
    qint64 timeMs) {
    
    QWriteLocker locker(&m_lock);
    PerformanceRecord record;
    record.method = method;
    record.inputSize = inputSize;
    record.outputSize = outputSize;
    record.timeMs = timeMs;
    record.ratio = inputSize > 0 ? (double)outputSize / inputSize : 1.0;
    record.timestamp = QDateTime::currentMSecsSinceEpoch();
    
    m_performanceHistory[method].append(record);
    
    // Update EMA latency
    if (m_averageLatencies.contains(method)) {
        m_averageLatencies[method] = calculateEMA((double)timeMs, m_averageLatencies[method]);
    } else {
        m_averageLatencies[method] = (double)timeMs;
    }
    
    // Check for anomalies
    if (timeMs > m_averageLatencies[method] * 3.0 && m_performanceHistory[method].size() > 10) {
        emit anomalyDetected(method, "Latency spike detected (3x above EMA)");
    }
    
    if (m_performanceHistory[method].size() > m_maxRecordsPerMethod) {
        m_performanceHistory[method].removeFirst();
    }
    
    qDebug() << "[AgenticLearning] Recorded performance:" << method 
             << "ratio:" << record.ratio << "time:" << timeMs << "ms";
}

void AgenticLearningSystem::recordInferenceEfficiency(const QString& model, int tokens, qint64 timeMs, bool success) {
    QWriteLocker locker(&m_lock);
    QString key = "inference_" + model;
    
    if (!m_successRates.contains(key)) {
        m_successRates[key] = success ? 1.0 : 0.0;
    } else {
        m_successRates[key] = calculateEMA(success ? 1.0 : 0.0, m_successRates[key], 0.05);
    }
    
    double tps = tokens > 0 ? (tokens * 1000.0) / (timeMs + 1) : 0.0;
    qDebug() << "[AgenticLearning] Inference efficiency for" << model << ":" << tps << "tokens/sec";
}

QString AgenticLearningSystem::predictOptimalCompression(size_t dataSize, const QString& dataType) {
    QReadLocker locker(&m_lock);
    
    QString bestMethod = "brutal_gzip"; 
    double bestScore = -1.0;
    
    for (auto it = m_performanceHistory.begin(); it != m_performanceHistory.end(); ++it) {
        const QString& method = it.key();
        const QList<PerformanceRecord>& records = it.value();
        
        if (records.isEmpty()) continue;
        
        double avgRatio = 0.0;
        double avgTime = 0.0;
        int count = 0;
        
        // Window-based filtering for similar sizes
        for (const auto& record : records) {
            if (record.inputSize > dataSize * 0.7 && record.inputSize < dataSize * 1.3) {
                avgRatio += record.ratio;
                avgTime += record.timeMs;
                count++;
            }
        }
        
        if (count > 0) {
            avgRatio /= count;
            avgTime /= count;
            
            // Heuristic score: (Compression Gain)^2 / Log(Time + 1)
            double score = std::pow(1.0 - avgRatio, 2.0) / (std::log10(avgTime + 2.0));
            
            if (score > bestScore) {
                bestScore = score;
                bestMethod = method;
            }
        }
    }
    
    return bestMethod;
}

void AgenticLearningSystem::recordUserFeedback(const QString& operation, bool positive) {
    QWriteLocker locker(&m_lock);
    m_successRates[operation] = calculateEMA(positive ? 1.0 : 0.0, m_successRates.value(operation, 0.5));
    emit knowledgeUpdated();
}

void AgenticLearningSystem::onSuggestionAccepted(const QString& suggestionId) {
    recordUserFeedback("suggestion_" + suggestionId, true);
}

double AgenticLearningSystem::calculateEMA(double current, double previous, double alpha) {
    return (alpha * current) + ((1.0 - alpha) * previous);
}

bool AgenticLearningSystem::saveKnowledgeBase(const QString& filePath) {
    QJsonObject root;
    QJsonObject successObj;
    for (auto it = m_successRates.begin(); it != m_successRates.end(); ++it) {
        successObj[it.key()] = it.value();
    }
    root["success_rates"] = successObj;
    
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson());
        return true;
    }
    return false;
}

bool AgenticLearningSystem::loadKnowledgeBase(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject root = doc.object();
    QJsonObject successObj = root["success_rates"].toObject();
    
    QWriteLocker locker(&m_lock);
    for (auto it = successObj.begin(); it != successObj.end(); ++it) {
        m_successRates[it.key()] = it.value().toDouble();
    }
    return true;
}
