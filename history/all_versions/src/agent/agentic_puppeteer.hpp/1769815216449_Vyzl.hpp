// agentic_puppeteer.hpp - Response correction via pattern matching
#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

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
        return CorrectionResult{false, std::string(), failureType, diagnostic};
    }
};

// Base puppeteer for general response correction
class AgenticPuppeteer 
{
public:
    AgenticPuppeteer();
    virtual ~AgenticPuppeteer();

    AgenticPuppeteer(const AgenticPuppeteer&) = delete;
    AgenticPuppeteer& operator=(const AgenticPuppeteer&) = delete;

    // Main correction API
    CorrectionResult correctResponse(const std::string& originalResponse, const std::string& userPrompt = std::string());
    CorrectionResult correctJsonResponse(const json& response, const std::string& context = std::string());
    
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

    // Callbacks
    void failureDetected(FailureType type, const std::string& diagnostic);
    void correctionApplied(const std::string& correctedOutput);
    void correctionFailed(FailureType type, const std::string& error);

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




