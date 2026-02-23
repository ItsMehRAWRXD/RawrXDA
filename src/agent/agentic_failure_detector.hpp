// agentic_failure_detector.hpp - Detects 8 failure types with MASM acceleration (Qt-free)
#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include <cstdint>
#include <atomic>

// Forward declarations for MASM integration
struct MasmOperationResult;
struct AgentFailureEvent;

enum class AgentFailureType {
    None = 0,
    Refusal = 1,
    Hallucination = 2,
    FormatViolation = 3,
    InfiniteLoop = 4,
    TokenLimitExceeded = 5,
    ResourceExhausted = 6,
    Timeout = 7,
    SafetyViolation = 8,
    QualityDegradation = 9,
    EmptyResponse = 10,
    ToolError = 11,
    InvalidOutput = 12,
    LowConfidence = 13,
    UserAbort = 14
};

struct FailureInfo {
    AgentFailureType type = AgentFailureType::None;
    std::string description;
    double confidence = 0.0;
    std::string evidence;
    std::chrono::system_clock::time_point detectedAt;
    int64_t sequenceNumber = 0;
};

/** Lightweight action summary for pre-execution validation (no dependency on ActionExecutor) */
struct ActionSummary {
    std::string type;    ///< action type string (e.g. "file_edit", "invoke_command")
    std::string target;  ///< file path, command, or resource
    std::string params;  ///< optional params as JSON string or key=value
};

class AgenticFailureDetector {
public:
    AgenticFailureDetector();
    ~AgenticFailureDetector() = default;

    FailureInfo detectFailure(const std::string& modelOutput, const std::string& context = "");
    std::vector<FailureInfo> detectMultipleFailures(const std::string& modelOutput);

    /** Pre-execution safety: validate action before running (dangerous commands, data loss) */
    bool validateActionBeforeExecution(const ActionSummary& action);
    /** True if command string is considered dangerous (rm -rf, format, etc.) */
    bool isDangerousCommand(const std::string& commandStr) const;
    /** True if action would cause irreversible data loss without backup */
    bool wouldCauseDataLoss(const ActionSummary& action) const;
    /** Suggest a recovery step for the given failure (e.g. rollback, retry) */
    std::string suggestRecoveryAction(const FailureInfo& failure) const;

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

    // Callbacks (replace Qt signals) — raw function pointers, no std::function
    void (*onFailureDetected)(AgentFailureType type, const char* description, void* ctx) = nullptr;
    void (*onMultipleFailuresDetected)(const FailureInfo* failures, size_t count, void* ctx) = nullptr;
    void (*onHighConfidenceDetection)(AgentFailureType type, double confidence, void* ctx) = nullptr;
    void* callbackContext = nullptr;  // Opaque user context for all callbacks

private:
    void initializePatterns();
    void initializeMasmAcceleration();  // New MASM initialization
    double calculateConfidence(AgentFailureType type, const std::string& output);
    
    // MASM-accelerated detection methods
    MasmOperationResult detectFailuresMasm(const std::string& output, 
                                           AgentFailureEvent* events,
                                           size_t maxEvents, 
                                           size_t* eventCount) const;
    FailureInfo detectFailureFallback(const std::string& modelOutput, const std::string& context);  // Non-MASM fallback
    bool validateMasmIntegrity() const;  // Verify MASM bridge health
    uint64_t getMasmPerformanceStats() const;  // Get MASM cycle counts

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
