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
public:
    RefusalBypassPuppeteer();

    CorrectionResult bypassRefusal(const std::string& refusedResponse, const std::string& originalPrompt);
    std::string reframePrompt(const std::string& refusedResponse);

private:
    std::string generateAlternativePrompt(const std::string& original);
};

// Specialized: Hallucination correction
class HallucinationCorrectorPuppeteer : public AgenticPuppeteer
{
public:
    HallucinationCorrectorPuppeteer();

    CorrectionResult detectAndCorrectHallucination(const std::string& response, const std::vector<std::string>& knownFacts);
    std::string validateFactuality(const std::string& claim);

private:
    std::vector<std::string> m_knownFactDatabase;
};

// Specialized: Format enforcement
class FormatEnforcerPuppeteer : public AgenticPuppeteer
{
public:
    FormatEnforcerPuppeteer();

    CorrectionResult enforceJsonFormat(const std::string& response);
    CorrectionResult enforceMarkdownFormat(const std::string& response);
    CorrectionResult enforceCodeBlockFormat(const std::string& response);

    void setRequiredJsonSchema(const std::string& schemaJson);
    std::string getRequiredJsonSchema() const;

private:
    std::string m_requiredSchema;
};
