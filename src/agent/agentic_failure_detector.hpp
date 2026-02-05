// agentic_failure_detector.hpp - Detects 8 failure types in model outputs
#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QStringList>
#include <QMutex>
#include <QHash>
#include <QDateTime>

// 8 detectable failure types
enum class AgentFailureType {
    Refusal = 0,                // Model refuses to respond
    Hallucination = 1,          // False/made-up information
    FormatViolation = 2,        // Wrong output format
    InfiniteLoop = 3,           // Repeating content
    TokenLimitExceeded = 4,     // Truncated response
    ResourceExhausted = 5,      // Out of memory/compute
    Timeout = 6,                // Inference took too long
    SafetyViolation = 7,        // Triggered safety filters
    None = 255                  // No failure
};

struct FailureInfo {
    AgentFailureType type = AgentFailureType::None;
    QString description;
    double confidence = 0.0;
    QString evidence;
    QDateTime detectedAt;
    qint64 sequenceNumber = 0;
};

class AgenticFailureDetector : public QObject
{
    Q_OBJECT

public:
    explicit AgenticFailureDetector(QObject* parent = nullptr);
    ~AgenticFailureDetector() override;

    // Main detection API
    FailureInfo detectFailure(const QString& modelOutput, const QString& context = QString());
    QList<FailureInfo> detectMultipleFailures(const QString& modelOutput);
    
    // Specific detection methods
    bool isRefusal(const QString& output) const;
    bool isHallucination(const QString& output) const;
    bool isFormatViolation(const QString& output) const;
    bool isInfiniteLoop(const QString& output) const;
    bool isTokenLimitExceeded(const QString& output) const;
    bool isResourceExhausted(const QString& output) const;
    bool isTimeout(const QString& output) const;
    bool isSafetyViolation(const QString& output) const;
    
    // Configuration
    void setRefusalThreshold(double threshold);
    void setQualityThreshold(double threshold);
    void enableToolValidation(bool enable);
    
    void addRefusalPattern(const QString& pattern);
    void addHallucinationPattern(const QString& pattern);
    void addLoopPattern(const QString& pattern);
    void addSafetyPattern(const QString& pattern);
    
    // Statistics
    struct Stats {
        qint64 totalOutputsAnalyzed = 0;
        QHash<int, qint64> failureTypeCounts;
        double avgConfidence = 0.0;
        qint64 truePredictions = 0;
        qint64 falsePredictions = 0;
    };
    
    Stats getStatistics() const;
    void resetStatistics();
    
    // Enable/disable
    void setEnabled(bool enable);
    bool isEnabled() const;

signals:
    void failureDetected(AgentFailureType type, const QString& description);
    void multipleFailuresDetected(const QList<FailureInfo>& failures);
    void highConfidenceDetection(AgentFailureType type, double confidence);

private:
    void initializePatterns();
    double calculateConfidence(AgentFailureType type, const QString& output);
    
    mutable QMutex m_mutex;
    
    // Pattern collections
    QStringList m_refusalPatterns;
    QStringList m_hallucinationPatterns;
    QStringList m_loopPatterns;
    QStringList m_safetyPatterns;
    QStringList m_timeoutIndicators;
    QStringList m_resourceExhaustionIndicators;
    
    // Thresholds
    double m_refusalThreshold = 0.7;
    double m_qualityThreshold = 0.6;
    bool m_enableToolValidation = true;
    
    // Statistics
    Stats m_stats;
    bool m_enabled = true;
    qint64 m_sequenceNumber = 0;
};
