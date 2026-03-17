// agentic_puppeteer.hpp - Response correction via pattern matching (Qt-free)
#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <functional>
#include <nlohmann/json.hpp>
#include <cstdint>

enum class FailureType {
    RefusalResponse, Hallucination, FormatViolation, InfiniteLoop,
    TokenLimitExceeded, None
};

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

class AgenticPuppeteer {
public:
    AgenticPuppeteer();
    virtual ~AgenticPuppeteer();

    CorrectionResult correctResponse(const std::string& originalResponse, const std::string& userPrompt = "");
    CorrectionResult correctJsonResponse(const nlohmann::json& response, const std::string& context = "");
    FailureType detectFailure(const std::string& response);
    std::string diagnoseFailure(const std::string& response);
    void addRefusalPattern(const std::string& pattern);
    void addHallucinationPattern(const std::string& pattern);
    void addLoopPattern(const std::string& pattern);
    std::vector<std::string> getRefusalPatterns() const;
    std::vector<std::string> getHallucinationPatterns() const;

    struct Stats {
        int64_t responsesAnalyzed = 0;
        int64_t failuresDetected = 0;
        int64_t successfulCorrections = 0;
        int64_t failedCorrections = 0;
        std::unordered_map<int, int64_t> failureTypeCount;
    };

    Stats getStatistics() const;
    void resetStatistics();
    void setEnabled(bool enable);
    bool isEnabled() const;

    // Callbacks (replace Qt signals)
    std::function<void(FailureType, const std::string&)> onFailureDetected;
    std::function<void(const std::string&)> onCorrectionApplied;
    std::function<void(FailureType, const std::string&)> onCorrectionFailed;

protected:
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

class RefusalBypassPuppeteer : public AgenticPuppeteer {
public:
    RefusalBypassPuppeteer();
    CorrectionResult bypassRefusal(const std::string& refusedResponse, const std::string& originalPrompt);
    std::string reframePrompt(const std::string& refusedResponse);
private:
    std::string generateAlternativePrompt(const std::string& original);
};

class HallucinationCorrectorPuppeteer : public AgenticPuppeteer {
public:
    HallucinationCorrectorPuppeteer();
    CorrectionResult detectAndCorrectHallucination(const std::string& response, const std::vector<std::string>& knownFacts);
    std::string validateFactuality(const std::string& claim);
private:
    std::vector<std::string> m_knownFactDatabase;
};

class FormatEnforcerPuppeteer : public AgenticPuppeteer {
public:
    FormatEnforcerPuppeteer();
    CorrectionResult enforceJsonFormat(const std::string& response);
    CorrectionResult enforceMarkdownFormat(const std::string& response);
    CorrectionResult enforceCodeBlockFormat(const std::string& response);
    void setRequiredJsonSchema(const nlohmann::json& schema);
    nlohmann::json getRequiredJsonSchema() const;
private:
    nlohmann::json m_requiredSchema;
};
