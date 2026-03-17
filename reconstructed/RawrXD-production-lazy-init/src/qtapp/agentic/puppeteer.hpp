#pragma once
#include <QObject>
#include <QString>
#include <functional>
#include <QMutex>
#include <QMutexLocker>
#include "agentic_failure_detector.hpp"

enum class CorrectionStrategy {
    None,
    RetryWithPrompt,
    RetryWithExamples,
    RetryWithConstraints,
    SimplifyRequest,
    SwitchModel,
    HumanIntervention
};

struct CorrectionResult {
    bool success = false;
    QString correctedResponse;
    QString strategyUsed;
    int attempts = 0;
    double confidence = 0.0;
    QString error;

    static CorrectionResult failed(const QString& error, int attempts) {
        return {false, "", "", attempts, 0.0, error};
    }
    static CorrectionResult succeeded(const QString& response, const QString& strategy, int attempts, double confidence) {
        return {true, response, strategy, attempts, confidence, ""};
    }
};

struct PuppeteerStats {
    int totalCorrections = 0;
    int successfulCorrections = 0;
    int failedCorrections = 0;
};

class AgenticPuppeteer : public QObject {
    Q_OBJECT
public:
    explicit AgenticPuppeteer(QObject* parent = nullptr);
    ~AgenticPuppeteer();

    CorrectionResult correctFailure(
        const FailureDetection& failure,
        const QString& originalPrompt,
        const QString& failedResponse,
        std::function<QString(const QString&)> modelCallback);

signals:
    void correctionAttempted(CorrectionStrategy strategy, int attempt);
    void correctionSucceeded(const QString& strategy);
    void correctionFailed(const QString& reason);

private:
    CorrectionStrategy selectStrategy(const FailureDetection& failure);
    CorrectionResult correctRefusal(const QString& prompt, const QString& response, std::function<QString(const QString&)> callback);
    CorrectionResult correctHallucination(const QString& prompt, const QString& response, const QString& context, std::function<QString(const QString&)> callback);
    CorrectionResult correctFormatViolation(const QString& prompt, const QString& response, const QString& format, std::function<QString(const QString&)> callback);

    PuppeteerStats m_stats;
    QMutex m_mutex;
};
