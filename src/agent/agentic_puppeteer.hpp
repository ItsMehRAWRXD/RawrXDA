#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <algorithm>
#include <nlohmann/json.hpp>

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
    FailureType originalFailure = FailureType::None;
    std::string reason;

    static CorrectionResult ok(const std::string& output, FailureType fail) {
        CorrectionResult r;
        r.success = true;
        r.correctedOutput = output;
        r.originalFailure = fail;
        return r;
    }

    static CorrectionResult error(FailureType fail, const std::string& reason) {
        CorrectionResult r;
        r.success = false;
        r.originalFailure = fail;
        r.reason = reason;
        return r;
    }
};

struct PuppeteerStats {
    uint64_t responsesAnalyzed = 0;
    uint64_t failuresDetected = 0;
    std::vector<int> failureTypeCount = std::vector<int>(6, 0);
    uint64_t successfulCorrections = 0;
    uint64_t failedCorrections = 0;
};

class AgenticPuppeteer {
public:
    AgenticPuppeteer();
    ~AgenticPuppeteer();

    CorrectionResult correctResponse(const std::string& originalResponse, const std::string& userPrompt);
    CorrectionResult correctJsonResponse(const nlohmann::json& response, const std::string& context);
    
    // Setters
    void setEnabled(bool enabled) { m_enabled = enabled; }
    
    // Explicit Logic: Allow re-prompting via callback
    void setRepromptCallback(std::function<std::string(const std::string&)> callback) {
        m_repromptCallback = callback;
    }

private:
    std::function<std::string(const std::string&)> m_repromptCallback;
    
    FailureType detectFailure(const std::string& response);
    std::string diagnoseFailure(const std::string& response);
    
    // Correction strategies
    std::string applyRefusalBypass(const std::string& response);
    std::string correctHallucination(const std::string& response);
    std::string enforceFormat(const std::string& response);
    std::string handleInfiniteLoop(const std::string& response);

    // Callbacks placeholder
    void failureDetected(FailureType type, const std::string& diagnosis);
    void correctionApplied(const std::string& corrected);
    void correctionFailed(FailureType type, const std::string& reason);

    bool m_enabled = true;
    mutable std::mutex m_mutex;
    PuppeteerStats m_stats;
    
    std::vector<std::string> m_refusalPatterns;
    std::vector<std::string> m_hallucinationPatterns;
};
