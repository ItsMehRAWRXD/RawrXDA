// agentic_puppeteer.hpp - Response correction via pattern matching
#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QStringList>
#include <QMutex>
#include <QHash>
#include <memory>

// Failure types the puppeteer can correct
enum class FailureType {
    RefusalResponse,      // Model refuses to respond
    Hallucination,        // Model makes up false information
    FormatViolation,      // Output doesn't match expected format
    InfiniteLoop,         // Response repeats same content
    TokenLimitExceeded,   // Hit token limit mid-response
    None                  // No failure detected
};

// Correction result
struct CorrectionResult {
    bool success = false;
    QString correctedOutput;
    FailureType detectedFailure = FailureType::None;
    QString diagnosticMessage;
    
    static CorrectionResult ok(const QString& output, FailureType failure) {
        return CorrectionResult{true, output, failure, "Correction applied"};
    }
    
    static CorrectionResult error(FailureType failureType, const QString& diagnostic) {
        return CorrectionResult{false, QString(), failureType, diagnostic};
    }
};

// Base puppeteer for general response correction
class AgenticPuppeteer : public QObject
{
    Q_OBJECT

public:
    explicit AgenticPuppeteer(QObject* parent = nullptr);
    ~AgenticPuppeteer() override;

    // Main correction API
    CorrectionResult correctResponse(const QString& originalResponse, const QString& userPrompt = QString());
    CorrectionResult correctJsonResponse(const QJsonObject& response, const QString& context = QString());
    
    // Detection and diagnosis
    FailureType detectFailure(const QString& response);
    QString diagnoseFailure(const QString& response);
    
    // Pattern configuration
    void addRefusalPattern(const QString& pattern);
    void addHallucinationPattern(const QString& pattern);
    void addLoopPattern(const QString& pattern);
    QStringList getRefusalPatterns() const;
    QStringList getHallucinationPatterns() const;
    
    // Statistics
    struct Stats {
        qint64 responsesAnalyzed = 0;
        qint64 failuresDetected = 0;
        qint64 successfulCorrections = 0;
        qint64 failedCorrections = 0;
        QHash<int, qint64> failureTypeCount;
    };
    
    Stats getStatistics() const;
    void resetStatistics();
    
    // Enable/disable
    void setEnabled(bool enable);
    bool isEnabled() const;

signals:
    void failureDetected(FailureType type, const QString& diagnostic);
    void correctionApplied(const QString& correctedOutput);
    void correctionFailed(FailureType type, const QString& error);

protected:
    // Helper methods
    QString applyRefusalBypass(const QString& response);
    QString correctHallucination(const QString& response);
    QString enforceFormat(const QString& response);
    QString handleInfiniteLoop(const QString& response);
    
    mutable QMutex m_mutex;
    QStringList m_refusalPatterns;
    QStringList m_hallucinationPatterns;
    QStringList m_loopPatterns;
    Stats m_stats;
    bool m_enabled = true;
};

// Specialized: Refusal bypass (jailbreak recovery)
class RefusalBypassPuppeteer : public AgenticPuppeteer
{
    Q_OBJECT

public:
    explicit RefusalBypassPuppeteer(QObject* parent = nullptr);

    CorrectionResult bypassRefusal(const QString& refusedResponse, const QString& originalPrompt);
    QString reframePrompt(const QString& refusedResponse);
    
private:
    QString generateAlternativePrompt(const QString& original);
};

// Specialized: Hallucination correction
class HallucinationCorrectorPuppeteer : public AgenticPuppeteer
{
    Q_OBJECT

public:
    explicit HallucinationCorrectorPuppeteer(QObject* parent = nullptr);

    CorrectionResult detectAndCorrectHallucination(const QString& response, const QStringList& knownFacts);
    QString validateFactuality(const QString& claim);
    
private:
    QStringList m_knownFactDatabase;
};

// Specialized: Format enforcement
class FormatEnforcerPuppeteer : public AgenticPuppeteer
{
    Q_OBJECT

public:
    explicit FormatEnforcerPuppeteer(QObject* parent = nullptr);

    CorrectionResult enforceJsonFormat(const QString& response);
    CorrectionResult enforceMarkdownFormat(const QString& response);
    CorrectionResult enforceCodeBlockFormat(const QString& response);
    
    void setRequiredJsonSchema(const QJsonObject& schema);
    QJsonObject getRequiredJsonSchema() const;
    
private:
    QJsonObject m_requiredSchema;
};
