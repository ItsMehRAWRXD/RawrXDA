// agentic_failure_detector.hpp - Detects 8 failure types in model outputs (Qt-free)
#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include <functional>
#include <cstdint>

enum class AgentFailureType {
    Refusal = 0, Hallucination = 1, FormatViolation = 2, InfiniteLoop = 3,
    TokenLimitExceeded = 4, ResourceExhausted = 5, Timeout = 6, SafetyViolation = 7,
    None = 255
};

struct FailureInfo {
    AgentFailureType type = AgentFailureType::None;
    std::string description;
    double confidence = 0.0;
    std::string evidence;
    std::chrono::system_clock::time_point detectedAt;
    int64_t sequenceNumber = 0;
};

class AgenticFailureDetector {
public:
    AgenticFailureDetector();
    ~AgenticFailureDetector() = default;

    FailureInfo detectFailure(const std::string& modelOutput, const std::string& context = "");
    std::vector<FailureInfo> detectMultipleFailures(const std::string& modelOutput);
    bool isRefusal(const std::string& output) const;
    bool isHallucination(const std::string& output) const;
    bool isFormatViolation(const std::string& output) const;
    bool isInfiniteLoop(const std::string& output) const;
    bool isTokenLimitExceeded(const std::string& output) const;
    bool isResourceExhausted(const std::string& output) const;
    bool isTimeout(const std::string& output) const;
    bool isSafetyViolation(const std::string& output) const;

    void setRefusalThreshold(double threshold);
    void setQualityThreshold(double threshold);
    void enableToolValidation(bool enable);
    void addRefusalPattern(const std::string& pattern);
    void addHallucinationPattern(const std::string& pattern);
    void addLoopPattern(const std::string& pattern);
    void addSafetyPattern(const std::string& pattern);

    struct Stats {
        int64_t totalOutputsAnalyzed = 0;
        std::unordered_map<int, int64_t> failureTypeCounts;
        double avgConfidence = 0.0;
        int64_t truePredictions = 0;
        int64_t falsePredictions = 0;
    };

    Stats getStatistics() const;
    void resetStatistics();
    void setEnabled(bool enable);
    bool isEnabled() const;

    // Callbacks (replace Qt signals)
    std::function<void(AgentFailureType, const std::string&)> onFailureDetected;
    std::function<void(const std::vector<FailureInfo>&)> onMultipleFailuresDetected;
    std::function<void(AgentFailureType, double)> onHighConfidenceDetection;

private:
    void initializePatterns();
    double calculateConfidence(AgentFailureType type, const std::string& output);

    mutable std::mutex m_mutex;
    std::vector<std::string> m_refusalPatterns;
    std::vector<std::string> m_hallucinationPatterns;
    std::vector<std::string> m_loopPatterns;
    std::vector<std::string> m_safetyPatterns;
    std::vector<std::string> m_timeoutIndicators;
    std::vector<std::string> m_resourceExhaustionIndicators;
    double m_refusalThreshold = 0.7;
    double m_qualityThreshold = 0.6;
    bool m_enableToolValidation = true;
    Stats m_stats;
    bool m_enabled = true;
    int64_t m_sequenceNumber = 0;
};
