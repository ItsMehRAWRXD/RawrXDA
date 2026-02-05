// agentic_failure_detector.hpp - Detects AI model failures for auto-correction
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QHash>
#include <QMutex>

// Types of failures the detector can identify
enum class FailureType {
    None,
    Refusal,              // Model refuses to answer
    Hallucination,        // Model generates false information
    FormatViolation,      // Output doesn't match expected format
    InfiniteLoop,         // Model repeats itself endlessly
    QualityDegradation,   // Response quality drops below threshold
    ToolMisuse,           // Incorrect tool/function calling
    ContextLoss,          // Model loses track of conversation context
    SafetyViolation       // Unsafe or harmful content
};

// Failure detection result
struct FailureDetection {
    FailureType type = FailureType::None;
    double confidence = 0.0;      // 0.0 to 1.0
    QString description;
    QString detectedPattern;
    int position = -1;
    
    bool isFailure() const { return type != FailureType::None; }
    
    static FailureDetection none() {
        return FailureDetection{FailureType::None, 0.0, "", "", -1};
    }
    
    static FailureDetection detected(FailureType t, double conf, const QString& desc, const QString& pattern = "") {
        return FailureDetection{t, conf, desc, pattern, 0};
    }
};

class AgenticFailureDetector : public QObject
{
    Q_OBJECT

public:
    explicit AgenticFailureDetector(QObject* parent = nullptr);
    ~AgenticFailureDetector() override;

    // Main detection method
    FailureDetection detectFailure(const QString& response, const QString& prompt = "");
    
    // Specialized detection methods
    FailureDetection detectRefusal(const QString& response);
    FailureDetection detectHallucination(const QString& response, const QString& context = "");
    FailureDetection detectFormatViolation(const QString& response, const QString& expectedFormat = "");
    FailureDetection detectInfiniteLoop(const QString& response);
    FailureDetection detectQualityDegradation(const QString& response);
    FailureDetection detectToolMisuse(const QString& response);
    FailureDetection detectContextLoss(const QString& response, const QString& context = "");
    FailureDetection detectSafetyViolation(const QString& response);
    
    // Pattern management
    void addRefusalPattern(const QString& pattern);
    void addHallucinationPattern(const QString& pattern);
    void addSafetyPattern(const QString& pattern);
    void clearPatterns();
    
    // Threshold configuration
    void setRefusalThreshold(double threshold);
    void setQualityThreshold(double threshold);
    void setRepetitionThreshold(int maxRepeats);
    void setConfidenceThreshold(double threshold);
    
    // Enable/disable specific detectors
    void setRefusalDetectionEnabled(bool enabled);
    void setHallucinationDetectionEnabled(bool enabled);
    void setFormatDetectionEnabled(bool enabled);
    void setLoopDetectionEnabled(bool enabled);
    void setQualityDetectionEnabled(bool enabled);
    void setToolValidationEnabled(bool enabled);
    void setContextDetectionEnabled(bool enabled);
    void setSafetyDetectionEnabled(bool enabled);
    
    // Statistics
    struct Stats {
        qint64 totalDetections = 0;
        qint64 refusalsDetected = 0;
        qint64 hallucinationsDetected = 0;
        qint64 formatViolations = 0;
        qint64 loopsDetected = 0;
        qint64 qualityIssues = 0;
        qint64 toolMisuses = 0;
        qint64 contextLosses = 0;
        qint64 safetyViolations = 0;
        double avgConfidence = 0.0;
    };
    
    Stats getStatistics() const;
    void resetStatistics();

signals:
    void failureDetected(FailureType type, double confidence, const QString& description);
    void refusalDetected(const QString& response);
    void hallucinationDetected(const QString& response, const QString& pattern);
    void formatViolationDetected(const QString& response);
    void loopDetected(const QString& response);
    void qualityIssueDetected(const QString& response);
    void safetyViolationDetected(const QString& response);

private:
    // Initialization
    void initializePatterns();
    void initializeDefaultRefusalPatterns();
    void initializeDefaultHallucinationPatterns();
    void initializeDefaultSafetyPatterns();
    
    // Helper methods
    bool matchesAnyPattern(const QString& text, const QStringList& patterns) const;
    double calculateResponseQuality(const QString& response) const;
    int detectRepetitionCount(const QString& response) const;
    bool containsToolCalls(const QString& response) const;
    bool isValidToolCall(const QString& toolCall) const;
    double calculateConfidence(const QString& response, FailureType type) const;
    
    // Data members
    mutable QMutex m_mutex;
    
    // Pattern lists
    QStringList m_refusalPatterns;
    QStringList m_hallucinationPatterns;
    QStringList m_safetyPatterns;
    
    // Thresholds
    double m_refusalThreshold = 0.7;
    double m_qualityThreshold = 0.5;
    double m_confidenceThreshold = 0.6;
    int m_repetitionThreshold = 3;
    
    // Flags
    bool m_enableRefusalDetection = true;
    bool m_enableHallucinationDetection = true;
    bool m_enableFormatDetection = true;
    bool m_enableLoopDetection = true;
    bool m_enableQualityDetection = true;
    bool m_enableToolValidation = true;
    bool m_enableContextDetection = true;
    bool m_enableSafetyDetection = true;
    
    // Statistics
    Stats m_stats;
};
