// agentic_self_corrector.hpp - Self-correcting agentic system
#pragma once

#include <QString>
#include <QByteArray>
#include <QJsonObject>
#include <QMutex>
#include <QHash>
#include <functional>

// Correction result structure
struct CorrectionResult {
    bool succeeded = false;
    QString originalError;
    QByteArray correctedOutput;
    QString correctionMethod;
    int attemptsUsed = 0;
    double confidenceScore = 0.0;
    
    static CorrectionResult success(const QByteArray& output, const QString& method, int attempts, double confidence) {
        return CorrectionResult{true, QString(), output, method, attempts, confidence};
    }
    
    static CorrectionResult failure(const QString& error) {
        return CorrectionResult{false, error, QByteArray(), QString(), 0, 0.0};
    }
};

class AgenticSelfCorrector
{
public:
    explicit AgenticSelfCorrector();
    ~AgenticSelfCorrector();

    // Primary correction interface
    CorrectionResult correctAgentOutput(const QByteArray& output, const QString& context = QString());
    CorrectionResult correctWithRetry(const QByteArray& output, int maxRetries = 3);
    
    // Specific correction strategies
    CorrectionResult correctFormatViolation(const QByteArray& output);
    CorrectionResult correctRefusalResponse(const QByteArray& output);
    CorrectionResult correctHallucination(const QByteArray& output);
    CorrectionResult correctInfiniteLoop(const QByteArray& output);
    CorrectionResult correctTokenLimit(const QByteArray& output);
    
    // Configuration
    void setMaxCorrectionAttempts(int max);
    void setConfidenceThreshold(double threshold);
    void enableCorrectionMethod(const QString& method, bool enable);
    
    // Statistics
    struct Stats {
        qint64 totalAttempts = 0;
        qint64 successfulCorrections = 0;
        qint64 failedCorrections = 0;
        double avgConfidenceScore = 0.0;
        QHash<QString, int> methodSuccessCounts;
    };
    
    Stats getStatistics() const;
    void resetStatistics();

private:
    // Internal correction methods
    QByteArray performGrammarCorrection(const QByteArray& output);
    QByteArray performSemanticCorrection(const QByteArray& output);
    QByteArray performStructuralCorrection(const QByteArray& output);
    
    // Helper methods
    bool detectFormatViolation(const QByteArray& output) const;
    bool detectRefusal(const QByteArray& output) const;
    bool detectHallucination(const QByteArray& output) const;
    double calculateConfidenceScore(const QByteArray& output) const;
    
    mutable QMutex m_mutex;
    Stats m_stats;
    
    int m_maxAttempts = 3;
    double m_confidenceThreshold = 0.7;
    QHash<QString, bool> m_enabledMethods;
};

