// agentic_failure_detector.hpp - Detects AI model failures for auto-correction
#pragma once


// Types of failures the detector can identify
enum class FailureType {
    None,
    Refusal,              // Model refuses to answer
    Hallucination,        // Model generates false information
    FormatViolation,      // Output doesn't match expected format
    InfiniteLoop,         // Model repeats itself endlessly
    QualityDegradation,   // Response quality drops below threshold
    ToolMisuse,           // Incorrect tool/function calling
    ContextLoss,          // Model loses track of conversation context
    SafetyViolation       // Unsafe or harmful content
};

// Failure detection result
struct FailureDetection {
    FailureType type = FailureType::None;
    double confidence = 0.0;      // 0.0 to 1.0
    std::string description;
    std::string detectedPattern;
    int position = -1;
    
    bool isFailure() const { return type != FailureType::None; }
    
    static FailureDetection none() {
        return FailureDetection{FailureType::None, 0.0, "", "", -1};
    }
    
    static FailureDetection detected(FailureType t, double conf, const std::string& desc, const std::string& pattern = "") {
        return FailureDetection{t, conf, desc, pattern, 0};
    }
};

class AgenticFailureDetector : public void
{

public:
    explicit AgenticFailureDetector(void* parent = nullptr);
    ~AgenticFailureDetector() override;

    // Main detection method
    FailureDetection detectFailure(const std::string& response, const std::string& prompt = "");
    
    // Specialized detection methods
    FailureDetection detectRefusal(const std::string& response);
    FailureDetection detectHallucination(const std::string& response, const std::string& context = "");
    FailureDetection detectFormatViolation(const std::string& response, const std::string& expectedFormat = "");
    FailureDetection detectInfiniteLoop(const std::string& response);
    FailureDetection detectQualityDegradation(const std::string& response);
    FailureDetection detectToolMisuse(const std::string& response);
    FailureDetection detectContextLoss(const std::string& response, const std::string& context = "");
    FailureDetection detectSafetyViolation(const std::string& response);
    
    // Pattern management
    void addRefusalPattern(const std::string& pattern);
    void addHallucinationPattern(const std::string& pattern);
    void addSafetyPattern(const std::string& pattern);
    void clearPatterns();
    
    // Threshold configuration
    void setRefusalThreshold(double threshold);
    void setQualityThreshold(double threshold);
    void setRepetitionThreshold(int maxRepeats);
    void setConfidenceThreshold(double threshold);
    
    // Enable/disable specific detectors
    void setRefusalDetectionEnabled(bool enabled);
    void setHallucinationDetectionEnabled(bool enabled);
    void setFormatDetectionEnabled(bool enabled);
    void setLoopDetectionEnabled(bool enabled);
    void setQualityDetectionEnabled(bool enabled);
    void setToolValidationEnabled(bool enabled);
    void setContextDetectionEnabled(bool enabled);
    void setSafetyDetectionEnabled(bool enabled);
    
    // Statistics
    struct Stats {
        qint64 totalDetections = 0;
        qint64 refusalsDetected = 0;
        qint64 hallucinationsDetected = 0;
        qint64 formatViolations = 0;
        qint64 loopsDetected = 0;
        qint64 qualityIssues = 0;
        qint64 toolMisuses = 0;
        qint64 contextLosses = 0;
        qint64 safetyViolations = 0;
        double avgConfidence = 0.0;
    };
    
    Stats getStatistics() const;
    void resetStatistics();

    void failureDetected(FailureType type, double confidence, const std::string& description);
    void refusalDetected(const std::string& response);
    void hallucinationDetected(const std::string& response, const std::string& pattern);
    void formatViolationDetected(const std::string& response);
    void loopDetected(const std::string& response);
    void qualityIssueDetected(const std::string& response);
    void safetyViolationDetected(const std::string& response);

private:
    // Initialization
    void initializePatterns();
    void initializeDefaultRefusalPatterns();
    void initializeDefaultHallucinationPatterns();
    void initializeDefaultSafetyPatterns();
    
    // Helper methods
    bool matchesAnyPattern(const std::string& text, const std::vector<std::string>& patterns) const;
    double calculateResponseQuality(const std::string& response) const;
    int detectRepetitionCount(const std::string& response) const;
    bool containsToolCalls(const std::string& response) const;
    bool isValidToolCall(const std::string& toolCall) const;
    double calculateConfidence(const std::string& response, FailureType type) const;
    
    // Data members
    mutable std::mutex m_mutex;
    
    // Pattern lists
    std::vector<std::string> m_refusalPatterns;
    std::vector<std::string> m_hallucinationPatterns;
    std::vector<std::string> m_safetyPatterns;
    
    // Thresholds
    double m_refusalThreshold = 0.7;
    double m_qualityThreshold = 0.5;
    double m_confidenceThreshold = 0.6;
    int m_repetitionThreshold = 3;
    
    // Flags
    bool m_enableRefusalDetection = true;
    bool m_enableHallucinationDetection = true;
    bool m_enableFormatDetection = true;
    bool m_enableLoopDetection = true;
    bool m_enableQualityDetection = true;
    bool m_enableToolValidation = true;
    bool m_enableContextDetection = true;
    bool m_enableSafetyDetection = true;
    
    // Statistics
    Stats m_stats;
};

