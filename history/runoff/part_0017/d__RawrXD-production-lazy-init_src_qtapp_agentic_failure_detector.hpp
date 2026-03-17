#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QMutex>
#include <QMutexLocker>

// FailureType moved inside AgenticFailureDetector class

enum class DetectorFailure {
    None,
    Refusal,
    Hallucination,
    SafetyViolation,
    InfiniteLoop,
    FormatViolation,
    ToolMisuse,
    ContextLoss,
    QualityDegradation
};

struct FailureStats {
    int refusalsDetected = 0;
    int hallucinationsDetected = 0;
    int formatViolations = 0;
    int loopsDetected = 0;
    int qualityIssues = 0;
    int totalDetections = 0;
};

struct FailureDetection {
    bool failure = false;
    DetectorFailure type = DetectorFailure::None;
    QString reason;
    double confidence = 0.0;
    QString pattern;

    static FailureDetection none() { return {false, DetectorFailure::None, "", 0.0, ""}; }
    static FailureDetection detected(DetectorFailure type, double confidence, const QString& reason, const QString& pattern = "") {
        return {true, type, reason, confidence, pattern};
    }
    bool isFailure() const { return failure; }
};

class AgenticFailureDetector : public QObject {
    Q_OBJECT
public:
    using FailureType = DetectorFailure; // Backward compatibility alias

    explicit AgenticFailureDetector(QObject* parent = nullptr);
    ~AgenticFailureDetector();

    FailureDetection detectFailure(const QString& response, const QString& prompt);
    
    FailureDetection detectRefusal(const QString& response);
    FailureDetection detectHallucination(const QString& response, const QString& context = "");
    FailureDetection detectFormatViolation(const QString& response, const QString& expectedFormat = "");
    FailureDetection detectInfiniteLoop(const QString& response);
    FailureDetection detectQualityDegradation(const QString& response);
    FailureDetection detectToolMisuse(const QString& response);
    FailureDetection detectSafetyViolation(const QString& response);
    FailureDetection detectContextLoss(const QString& response, const QString& prompt);

signals:
    void refusalDetected(const QString& response);
    void failureDetected(FailureType type, double confidence, const QString& reason);
    void hallucinationDetected(const QString& response, const QString& pattern);
    void formatViolationDetected(const QString& response);
    void loopDetected(const QString& response);
    void qualityIssueDetected(const QString& response);

private:
    void initializePatterns();
    void initializeDefaultRefusalPatterns();
    void initializeDefaultHallucinationPatterns();
    void initializeDefaultSafetyPatterns();
    
    int detectRepetitionCount(const QString& response);
    double calculateResponseQuality(const QString& response);
    double calculateConfidence(const QString& response, FailureType type);

    QStringList m_refusalPatterns;
    QStringList m_hallucinationPatterns;
    QStringList m_safetyPatterns;
    
    bool m_enableSafetyDetection = true;
    bool m_enableRefusalDetection = true;
    bool m_enableLoopDetection = true;
    bool m_enableFormatDetection = true;
    bool m_enableHallucinationDetection = true;
    bool m_enableToolValidation = true;
    bool m_enableContextDetection = true;
    bool m_enableQualityDetection = true;
    
    double m_refusalThreshold = 0.8;
    int m_repetitionThreshold = 3;
    double m_qualityThreshold = 0.3;
    
    FailureStats m_stats;
    QMutex m_mutex;
};
