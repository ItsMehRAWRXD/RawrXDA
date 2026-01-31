#pragma once
#include <vector>
#include <string>
#include <mutex>
#include <chrono>

enum class AgentFailureType {
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
    AgentFailureType type = AgentFailureType::None;
    std::string description;
    double confidence = 0.0;
    std::string detectedReason;
    std::chrono::system_clock::time_point detectedAt;
    uint64_t sequenceNumber = 0;
};

struct FailureDetectorStats {
    uint64_t totalOutputsAnalyzed = 0;
    std::vector<int> failureTypeCounts = std::vector<int>(8, 0);
};

class AgenticFailureDetector {
public:
    AgenticFailureDetector();
    ~AgenticFailureDetector();

    FailureInfo detectFailure(const std::string& modelOutput, const std::string& context);
    
    // Setters
    void setEnabled(bool enabled) { m_enabled = enabled; }

    FailureDetectorStats getStats() const { return m_stats; }

private:
    void initializePatterns();
    bool isRefusal(const std::string& output);
    bool isSafetyViolation(const std::string& output);
    bool isTokenLimitExceeded(const std::string& output);
    bool isTimeout(const std::string& output);
    bool isResourceExhausted(const std::string& output);
    double calculateConfidence(AgentFailureType type, const std::string& output);

    bool m_enabled = true;
    mutable std::mutex m_mutex;
    FailureDetectorStats m_stats;
    uint64_t m_sequenceNumber = 0;

    std::vector<std::string> m_refusalPatterns;
    std::vector<std::string> m_hallucinationPatterns;
    std::vector<std::string> m_loopPatterns;
    std::vector<std::string> m_safetyPatterns;
    std::vector<std::string> m_timeoutIndicators;
    std::vector<std::string> m_resourceExhaustionIndicators;
};
