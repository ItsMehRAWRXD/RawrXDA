// agentic_failure_detector.hpp - Detects 8 failure types in model outputs
#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>

// 8 detectable failure types
enum class AgentFailureType {
    Refusal = 0,                // Model refuses to respond
    Hallucination = 1,          // False/made-up information
    FormatViolation = 2,        // Wrong output format
    InfiniteLoop = 3,           // Repeating content
    TokenLimitExceeded = 4,     // Truncated response
    ResourceExhausted = 5,      // Out of memory/compute
    Timeout = 6,                // Inference took too long
    SafetyViolation = 7,        // Triggered safety filters
    None = 255                  // No failure
};

struct FailureInfo {
    AgentFailureType type = AgentFailureType::None;
    std::string description;
    double confidence = 0.0;
    std::string evidence;
    int64_t sequenceNumber = 0;
};

class AgenticFailureDetector 
{
public:
    AgenticFailureDetector();
    ~AgenticFailureDetector();

    AgenticFailureDetector(const AgenticFailureDetector&) = delete;
    AgenticFailureDetector& operator=(const AgenticFailureDetector&) = delete;

    // Main detection API
    FailureInfo detectFailure(const std::string& modelOutput, const std::string& context = std::string());
    std::vector<FailureInfo> detectMultipleFailures(const std::string& modelOutput);
    
    // Specific detection methods
    bool isRefusal(const std::string& output) const;
    bool isHallucination(const std::string& output) const;
    bool isFormatViolation(const std::string& output) const;
    bool isInfiniteLoop(const std::string& output) const;
    bool isTokenLimitExceeded(const std::string& output) const;
    bool isResourceExhausted(const std::string& output) const;
    bool isTimeout(const std::string& output) const;
    bool isSafetyViolation(const std::string& output) const;
    
    // Configuration
    void setRefusalThreshold(double threshold);
    void setQualityThreshold(double threshold);
    void enableToolValidation(bool enable);
    
    void addRefusalPattern(const std::string& pattern);
    void addHallucinationPattern(const std::string& pattern);
    void addLoopPattern(const std::string& pattern);
    void addSafetyPattern(const std::string& pattern);
    
    // Statistics
    struct Stats {
        int64_t totalOutputsAnalyzed = 0;
        std::map<int, int64_t> failureTypeCounts;
        double avgConfidence = 0.0;
        int64_t truePredictions = 0;
        int64_t falsePredictions = 0;
    };
    
    Stats getStatistics() const;
    void resetStatistics();
    
    // Enable/disable
    void setEnabled(bool enable);
    bool isEnabled() const;

    // Callbacks (Internal use or bridge)
    void failureDetected(AgentFailureType type, const std::string& description);
    void multipleFailuresDetected(const std::vector<FailureInfo>& failures);
    void highConfidenceDetection(AgentFailureType type, double confidence);

private:
    void initializePatterns();
    double calculateConfidence(AgentFailureType type, const std::string& output);
    
    mutable std::mutex m_mutex;
    
    // Pattern collections
    std::vector<std::string> m_refusalPatterns;
    std::vector<std::string> m_hallucinationPatterns;
    std::vector<std::string> m_loopPatterns;
    std::vector<std::string> m_safetyPatterns;
    std::vector<std::string> m_timeoutIndicators;
    std::vector<std::string> m_resourceExhaustionIndicators;
    
    // Thresholds
    double m_refusalThreshold = 0.7;
    double m_qualityThreshold = 0.6;
    bool m_enableToolValidation = true;
    
    // Statistics
    Stats m_stats;
    bool m_enabled = true;
    int64_t m_sequenceNumber = 0;
};




