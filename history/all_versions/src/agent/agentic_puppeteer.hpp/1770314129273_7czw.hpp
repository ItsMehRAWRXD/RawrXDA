// agentic_puppeteer.hpp - Response correction via pattern matching
#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <map>
#include <memory>
#include <cstdint>

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
    std::string correctedOutput;
    FailureType detectedFailure = FailureType::None;
    std::string diagnosticMessage;
    
    static CorrectionResult ok(const std::string& output, FailureType failure) {
        return CorrectionResult{true, output, failure, "Correction applied"};
    }
    
    static CorrectionResult error(FailureType failureType, const std::string& diagnostic) {
        return CorrectionResult{false, "", failureType, diagnostic};
    }
};

// Base puppeteer for general response correction
class AgenticPuppeteer
{
public:
    AgenticPuppeteer();
    virtual ~AgenticPuppeteer();

    // Main correction API
    CorrectionResult correctResponse(const std::string& originalResponse, const std::string& userPrompt = "");
    
    // Detection and diagnosis
    FailureType detectFailure(const std::string& response);
    std::string diagnoseFailure(const std::string& response);
    
    // Pattern configuration
    void addRefusalPattern(const std::string& pattern);
    void addHallucinationPattern(const std::string& pattern);
    void addLoopPattern(const std::string& pattern);
    std::vector<std::string> getRefusalPatterns() const;
    std::vector<std::string> getHallucinationPatterns() const;
    
    // Statistics
    struct Stats {
        int64_t responsesAnalyzed = 0;
        int64_t failuresDetected = 0;
        int64_t successfulCorrections = 0;
        int64_t failedCorrections = 0;
        std::map<int, int64_t> failureTypeCount;
    };
    
    Stats getStatistics() const;
    void resetStatistics();
    
    // Enable/disable
    void setEnabled(bool enable);
    bool isEnabled() const;

protected:
    // Helper methods
    std::string applyRefusalBypass(const std::string& response);
    std::string correctHallucination(const std::string& response);
    std::string enforceFormat(const std::string& response);
    std::string handleInfiniteLoop(const std::string& response);
    
    mutable std::mutex m_mutex;
    std::vector<std::string> m_refusalPatterns;
    std::vector<std::string> m_hallucinationPatterns;
    std::vector<std::string> m_loopPatterns;
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
