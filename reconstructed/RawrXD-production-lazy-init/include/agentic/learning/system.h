#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QList>
#include <QReadWriteLock>
#include <QDateTime>

struct PerformanceRecord {
    QString method;
    size_t inputSize;
    size_t outputSize;
    qint64 timeMs;
    double ratio;
    qint64 timestamp;
};

class AgenticLearningSystem : public QObject {
    Q_OBJECT
    
public:
    explicit AgenticLearningSystem(QObject* parent = nullptr);
    ~AgenticLearningSystem() override;
    
    // Initialize learning system
    void initialize();
    
    // Learn from compression performance
    void recordCompressionPerformance(
        const QString& method,
        size_t inputSize,
        size_t outputSize,
        qint64 timeMs
    );
    
    // Learn from inference efficiency
    void recordInferenceEfficiency(const QString& model, int tokens, qint64 timeMs, bool success);
    
    // Predict optimal compression for new data
    QString predictOptimalCompression(size_t dataSize, const QString& dataType);
    
    // Learn from user feedback
    void recordUserFeedback(const QString& operation, bool positive);
    void onSuggestionAccepted(const QString& suggestion);
    
    // Get learning statistics
    QMap<QString, double> getSuccessRates() const { return m_successRates; }
    
    // Knowledge base management
    bool loadKnowledgeBase(const QString& filePath);
    bool saveKnowledgeBase(const QString& filePath);
    
signals:
    void anomalyDetected(const QString& metric, const QString& description);
    void knowledgeUpdated();
    
private:
    // Helper method for exponential moving average
    double calculateEMA(double newValue, double currentEMA, double alpha = 0.1);
    
    QMap<QString, QList<PerformanceRecord>> m_performanceHistory;
    QMap<QString, double> m_successRates;
    QMap<QString, double> m_averageLatencies;
    mutable QReadWriteLock m_lock;
    
    static constexpr int m_maxRecordsPerMethod = 1000;
};