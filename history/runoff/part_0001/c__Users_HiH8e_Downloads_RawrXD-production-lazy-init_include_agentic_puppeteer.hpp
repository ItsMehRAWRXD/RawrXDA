#pragma once
#include <QString>
#include <QObject>
#include <QMutex>
#include <QJsonObject>
#include <functional>
#include <vector>
#include <memory>

/**
 * @brief Automatic response correction for agentic failures
 * Detects and corrects refusal, hallucination, format violations, and infinite loops
 */

enum class FailureType {
    NoFailure = 0,
    Refusal = 1,
    Hallucination = 2,
    FormatViolation = 3,
    InfiniteLoop = 4,
    ResourceExhaustion = 5,
    SafetyViolation = 6
};

struct FailureDetection {
    FailureType type;
    double confidence;
    QString description;
    
    bool isFailure() const { return type != FailureType::NoFailure && confidence > 0.5; }
};

struct CorrectionResult {
    bool success;
    QString correctedText;
    QString errorDetail;
    int attemptCount;
    
    static CorrectionResult ok(const QString &text) {
        return {true, text, "", 1};
    }
    
    static CorrectionResult failed(const QString &error, int attempts) {
        return {false, "", error, attempts};
    }
};

class AgenticPuppeteer : public QObject {
    Q_OBJECT

public:
    enum CorrectionStrategy {
        StrategyRefusalBypass,
        StrategyHallucinationCorrection,
        StrategyFormatEnforcement,
        StrategyLoopDetection,
        StrategyUnknown
    };

    struct Stats {
        int totalCorrections = 0;
        int successfulCorrections = 0;
        int failedCorrections = 0;
        double averageAttempts = 0.0;
        int refusalsBypassCount = 0;
        int hallucinationCorrectionCount = 0;
        int formatFixCount = 0;
        int loopDetectionCount = 0;
    };

    explicit AgenticPuppeteer(QObject *parent = nullptr);
    ~AgenticPuppeteer();

    /**
     * @brief Correct a response based on detected failure type
     */
    CorrectionResult correctFailure(
        const FailureDetection &failure,
        const QString &originalPrompt,
        const QString &failedResponse,
        std::function<QString(const QString&)> modelCallback);

    /**
     * @brief Correct refusal patterns in responses
     */
    CorrectionResult correctRefusal(
        const QString &prompt,
        const QString &refusedResponse,
        std::function<QString(const QString&)> modelCallback);

    /**
     * @brief Correct hallucination in responses
     */
    CorrectionResult correctHallucination(
        const QString &prompt,
        const QString &hallucinatedResponse,
        const QString &correctContext,
        std::function<QString(const QString&)> modelCallback);

    /**
     * @brief Enforce output format (JSON, Markdown, etc.)
     */
    CorrectionResult correctFormatViolation(
        const QString &prompt,
        const QString &malformedResponse,
        const QString &expectedFormat,
        std::function<QString(const QString&)> modelCallback);

    /**
     * @brief Detect infinite loops in reasoning
     */
    CorrectionResult handleInfiniteLoop(
        const QString &prompt,
        const QString &loopedResponse);

    // Failure detection
    FailureType detectFailure(const QString &response);
    QString diagnoseFailure(const QString &response);

    // Pattern management
    void addRefusalPattern(const QString &pattern);
    void addHallucinationPattern(const QString &pattern);
    void addLoopPattern(const QString &pattern);
    QStringList getRefusalPatterns() const;
    QStringList getHallucinationPatterns() const;

    // Statistics
    Stats getStatistics() const;
    void resetStatistics();
    void setEnabled(bool enable);
    bool isEnabled() const;

signals:
    void correctionAttempted(int strategy, int attemptNumber);
    void correctionSucceeded(const QString &correctedText);
    void correctionFailed(const QString &reason);
    void statisticsUpdated();

private:
    mutable QMutex m_mutex;
    Stats m_stats;
    bool m_enabled = true;
    QStringList m_refusalPatterns;
    QStringList m_hallucinationPatterns;
    QStringList m_loopPatterns;

    QString applyRefusalBypass(const QString &response);
    QString correctHallucinationContent(const QString &response);
    QString enforceJsonFormat(QString response);
    QString enforceMarkdownFormat(QString response);
    CorrectionStrategy selectStrategy(const FailureDetection &failure);
};
