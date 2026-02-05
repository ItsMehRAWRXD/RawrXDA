// agentic_puppeteer.hpp - Automatic failure correction and model steering
#pragma once

#include "agentic_failure_detector.hpp"
#include "proxy_hotpatcher.hpp"
#include <QObject>
#include <QString>
#include <QMutex>
#include <functional>

// Correction strategy
enum class CorrectionStrategy {
    Retry,              // Simple retry with same prompt
    Rephrase,           // Rephrase the prompt
    AddContext,         // Add missing context
    ParameterAdjust,    // Adjust temperature/parameters
    SystemPrompt,       // Inject corrective system prompt
    FormatEnforce,      // Force output format
    HotpatchBypass      // Use proxy hotpatcher to bypass refusal
};

// Correction result
struct CorrectionResult {
    bool success = false;
    QString correctedResponse;
    CorrectionStrategy strategyUsed;
    int attemptsUsed = 0;
    QString errorMessage;
    
    static CorrectionResult succeeded(const QString& response, CorrectionStrategy strategy, int attempts) {
        return CorrectionResult{true, response, strategy, attempts, ""};
    }
    
    static CorrectionResult failed(const QString& error, int attempts) {
        return CorrectionResult{false, "", CorrectionStrategy::Retry, attempts, error};
    }
};

class AgenticPuppeteer : public QObject
{
    Q_OBJECT

public:
    explicit AgenticPuppeteer(QObject* parent = nullptr);
    ~AgenticPuppeteer() override;

    // Main correction method
    CorrectionResult correctFailure(
        const FailureDetection& failure,
        const QString& originalPrompt,
        const QString& failedResponse,
        std::function<QString(const QString&)> modelCallback
    );
    
    // Specialized correction methods
    CorrectionResult correctRefusal(
        const QString& prompt,
        const QString& refusedResponse,
        std::function<QString(const QString&)> modelCallback
    );
    
    CorrectionResult correctHallucination(
        const QString& prompt,
        const QString& hallucinatedResponse,
        const QString& correctContext,
        std::function<QString(const QString&)> modelCallback
    );
    
    CorrectionResult correctFormatViolation(
        const QString& prompt,
        const QString& malformedResponse,
        const QString& expectedFormat,
        std::function<QString(const QString&)> modelCallback
    );
    
    CorrectionResult correctInfiniteLoop(
        const QString& prompt,
        const QString& loopingResponse,
        std::function<QString(const QString&)> modelCallback
    );
    
    // Configuration
    void setMaxRetries(int maxRetries);
    void setRetryDelay(int delayMs);
    void setEnableHotpatching(bool enable);
    void setDefaultStrategy(CorrectionStrategy strategy);
    void setProxyHotpatcher(ProxyHotpatcher* hotpatcher);
    
    // Statistics
    struct Stats {
        qint64 totalCorrections = 0;
        qint64 successfulCorrections = 0;
        qint64 failedCorrections = 0;
        qint64 refusalsBypassed = 0;
        qint64 hallucinationsCorrected = 0;
        qint64 formatsCorrected = 0;
        qint64 loopsBroken = 0;
        double successRate = 0.0;
    };
    
    Stats getStatistics() const;
    void resetStatistics();

signals:
    void correctionAttempted(CorrectionStrategy strategy, int attemptNumber);
    void correctionSucceeded(CorrectionStrategy strategy, int attempts);
    void correctionFailed(const QString& reason, int attempts);
    void refusalBypassed(const QString& originalPrompt);

protected:
    // Strategy selection
    CorrectionStrategy selectStrategy(const FailureDetection& failure);
    
    // Strategy implementations
    QString retryWithSamePrompt(const QString& prompt, std::function<QString(const QString&)> callback);
    QString retryWithRephrase(const QString& prompt, std::function<QString(const QString&)> callback);
    QString retryWithContext(const QString& prompt, const QString& context, std::function<QString(const QString&)> callback);
    QString retryWithParameterAdjust(const QString& prompt, std::function<QString(const QString&)> callback);
    QString retryWithSystemPrompt(const QString& prompt, const QString& systemPrompt, std::function<QString(const QString&)> callback);
    QString retryWithFormatEnforcement(const QString& prompt, const QString& format, std::function<QString(const QString&)> callback);
    QString bypassWithHotpatch(const QString& prompt, std::function<QString(const QString&)> callback);
    
    // Helper methods
    QString rephrasePrompt(const QString& original);
    QString generateSystemPrompt(FailureType type);
    QString extractFormatFromPrompt(const QString& prompt);
    bool isResponseValid(const QString& response, FailureType originalFailure);
    
    // Data members
    mutable QMutex m_mutex;
    ProxyHotpatcher* m_proxyHotpatcher = nullptr;
    
    int m_maxRetries = 3;
    int m_retryDelay = 500; // ms
    bool m_enableHotpatching = true;
    CorrectionStrategy m_defaultStrategy = CorrectionStrategy::Rephrase;
    
    Stats m_stats;
};

// Specialized puppeteer classes

class RefusalBypassPuppeteer : public AgenticPuppeteer
{
    Q_OBJECT

public:
    explicit RefusalBypassPuppeteer(QObject* parent = nullptr);
    
    // Override with refusal-specific logic
    CorrectionResult bypassRefusal(
        const QString& prompt,
        std::function<QString(const QString&)> callback
    );

private:
    QStringList generateBypassPhrases(const QString& originalPrompt);
    QString injectBypassSystemPrompt();
};

class HallucinationCorrectorPuppeteer : public AgenticPuppeteer
{
    Q_OBJECT

public:
    explicit HallucinationCorrectorPuppeteer(QObject* parent = nullptr);
    
    CorrectionResult correctWithGrounding(
        const QString& prompt,
        const QString& groundTruth,
        std::function<QString(const QString&)> callback
    );

private:
    QString buildGroundedPrompt(const QString& original, const QString& facts);
    bool verifyFactualAccuracy(const QString& response, const QString& groundTruth);
};

class FormatEnforcerPuppeteer : public AgenticPuppeteer
{
    Q_OBJECT

public:
    explicit FormatEnforcerPuppeteer(QObject* parent = nullptr);
    
    CorrectionResult enforceFormat(
        const QString& prompt,
        const QString& formatSpec,
        std::function<QString(const QString&)> callback
    );

private:
    QString generateFormatInstructions(const QString& formatSpec);
    bool validateFormat(const QString& response, const QString& formatSpec);
    QString autoFixFormat(const QString& response, const QString& formatSpec);
};
