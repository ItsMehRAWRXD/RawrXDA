/**
 * @file RawrXD_AgenticPuppeteer.hpp
 * @brief Pure C++20 Response correction engine (replaces Qt version)
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <regex>
#include <atomic>
#include <mutex>
#include <nlohmann/json.hpp>

namespace RawrXD::Agent {

enum class CorrectionFailureType {
    RefusalResponse = 0,
    Hallucination = 1,
    FormatViolation = 2,
    InfiniteLoop = 3,
    TokenLimitExceeded = 4,
    None = 255
};

struct CorrectionResult {
    bool success = false;
    std::string corrected_output;
    CorrectionFailureType detected_failure = CorrectionFailureType::None;
    std::string diagnostic_message;

    static CorrectionResult Ok(const std::string& output, CorrectionFailureType failure) {
        return CorrectionResult{true, output, failure, "Correction applied"};
    }

    static CorrectionResult Error(CorrectionFailureType failure_type, const std::string& diagnostic) {
        return CorrectionResult{false, "", failure_type, diagnostic};
    }
};

class AgenticPuppeteer {
public:
    AgenticPuppeteer();
    ~AgenticPuppeteer();

    /**
     * @brief Correct model response
     */
    CorrectionResult CorrectResponse(const std::string& original_response, const std::string& user_prompt = "");

    /**
     * @brief Correct JSON response
     */
    CorrectionResult CorrectJsonResponse(const nlohmann::json& response, const std::string& context = "");

    /**
     * @brief Detect failure type
     */
    CorrectionFailureType DetectFailure(const std::string& response);

    /**
     * @brief Get diagnostic message for failure
     */
    std::string DiagnoseFailure(const std::string& response);

    /**
     * @brief Pattern configuration
     */
    void AddRefusalPattern(const std::string& pattern);
    void AddHallucinationPattern(const std::string& pattern);
    void AddLoopPattern(const std::string& pattern);

    std::vector<std::string> GetRefusalPatterns() const;
    std::vector<std::string> GetHallucinationPatterns() const;

    /**
     * @brief Statistics
     */
    struct Statistics {
        uint64_t responses_analyzed = 0;
        uint64_t failures_detected = 0;
        uint64_t successful_corrections = 0;
        uint64_t failed_corrections = 0;
        std::map<int, uint64_t> failure_type_count;
    };

    Statistics GetStatistics() const;
    void ResetStatistics();

    /**
     * @brief Enable/disable
     */
    void SetEnabled(bool enable) { enabled_ = enable; }
    bool IsEnabled() const { return enabled_; }

private:
    CorrectionResult CorrectRefusal(const std::string& response);
    CorrectionResult CorrectHallucination(const std::string& response);
    CorrectionResult CorrectFormatViolation(const std::string& response);
    CorrectionResult CorrectInfiniteLoop(const std::string& response);
    CorrectionResult CorrectTokenLimit(const std::string& response);

    std::vector<std::regex> refusal_patterns_;
    std::vector<std::regex> hallucination_patterns_;
    std::vector<std::regex> loop_patterns_;

    std::atomic<bool> enabled_{true};
    mutable std::mutex stats_mutex_;
    Statistics statistics_;
};

} // namespace RawrXD::Agent
