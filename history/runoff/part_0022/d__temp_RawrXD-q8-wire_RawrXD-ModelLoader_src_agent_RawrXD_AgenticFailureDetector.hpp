/**
 * @file RawrXD_AgenticFailureDetector.hpp
 * @brief Pure C++20 failure detection for agent outputs (replaces Qt version)
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <regex>
#include <chrono>
#include <atomic>
#include <mutex>
#include <memory>

namespace RawrXD::Agent {

enum class FailureType {
    Refusal = 0,
    Hallucination = 1,
    FormatViolation = 2,
    InfiniteLoop = 3,
    TokenLimitExceeded = 4,
    ResourceExhausted = 5,
    Timeout = 6,
    SafetyViolation = 7,
    None = 255
};

struct FailureInfo {
    FailureType type = FailureType::None;
    std::string description;
    double confidence = 0.0;
    std::string evidence;
    std::chrono::system_clock::time_point detected_at;
    uint64_t sequence_number = 0;
};

class AgenticFailureDetector {
public:
    AgenticFailureDetector();
    ~AgenticFailureDetector();

    /**
     * @brief Detect failure in model output
     */
    FailureInfo DetectFailure(const std::string& output, const std::string& context = "");

    /**
     * @brief Detect multiple failures
     */
    std::vector<FailureInfo> DetectMultipleFailures(const std::string& output);

    /**
     * @brief Check for specific failure types
     */
    bool IsRefusal(const std::string& output) const;
    bool IsHallucination(const std::string& output) const;
    bool IsFormatViolation(const std::string& output) const;
    bool IsInfiniteLoop(const std::string& output) const;
    bool IsTokenLimitExceeded(const std::string& output) const;
    bool IsResourceExhausted(const std::string& output) const;
    bool IsTimeout(const std::string& output) const;
    bool IsSafetyViolation(const std::string& output) const;

    /**
     * @brief Configuration
     */
    void SetRefusalThreshold(double threshold) { refusal_threshold_ = threshold; }
    void SetQualityThreshold(double threshold) { quality_threshold_ = threshold; }
    void EnableToolValidation(bool enable) { tool_validation_enabled_ = enable; }

    void AddRefusalPattern(const std::string& pattern);
    void AddHallucinationPattern(const std::string& pattern);
    void AddLoopPattern(const std::string& pattern);
    void AddSafetyPattern(const std::string& pattern);

    /**
     * @brief Statistics
     */
    struct Statistics {
        uint64_t total_outputs_analyzed = 0;
        std::map<int, uint64_t> failure_type_counts;
        double avg_confidence = 0.0;
        uint64_t true_predictions = 0;
        uint64_t false_predictions = 0;
    };

    Statistics GetStatistics() const;
    void ResetStatistics();

    /**
     * @brief Enable/disable detector
     */
    void SetEnabled(bool enable) { enabled_ = enable; }
    bool IsEnabled() const { return enabled_; }

private:
    double CalculateConfidence(FailureType type, const std::string& output);

    std::vector<std::regex> refusal_patterns_;
    std::vector<std::regex> hallucination_patterns_;
    std::vector<std::regex> loop_patterns_;
    std::vector<std::regex> safety_patterns_;

    double refusal_threshold_ = 0.7;
    double quality_threshold_ = 0.5;
    bool tool_validation_enabled_ = true;
    std::atomic<bool> enabled_{true};

    mutable std::mutex stats_mutex_;
    Statistics statistics_;
    uint64_t sequence_counter_ = 0;
};

} // namespace RawrXD::Agent
