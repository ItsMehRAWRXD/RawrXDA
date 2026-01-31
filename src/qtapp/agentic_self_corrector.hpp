// agentic_self_corrector.hpp - Self-correcting agentic system
#pragma once


#include <functional>

// Correction result structure
struct CorrectionResult {
    bool succeeded = false;
    std::string originalError;
    std::vector<uint8_t> correctedOutput;
    std::string correctionMethod;
    int attemptsUsed = 0;
    double confidenceScore = 0.0;
    
    static CorrectionResult success(const std::vector<uint8_t>& output, const std::string& method, int attempts, double confidence) {
        return CorrectionResult{true, std::string(), output, method, attempts, confidence};
    }
    
    static CorrectionResult failure(const std::string& error) {
        return CorrectionResult{false, error, std::vector<uint8_t>(), std::string(), 0, 0.0};
    }
};

class AgenticSelfCorrector
{
public:
    explicit AgenticSelfCorrector();
    ~AgenticSelfCorrector();

    // Primary correction interface
    CorrectionResult correctAgentOutput(const std::vector<uint8_t>& output, const std::string& context = std::string());
    CorrectionResult correctWithRetry(const std::vector<uint8_t>& output, int maxRetries = 3);
    
    // Specific correction strategies
    CorrectionResult correctFormatViolation(const std::vector<uint8_t>& output);
    CorrectionResult correctRefusalResponse(const std::vector<uint8_t>& output);
    CorrectionResult correctHallucination(const std::vector<uint8_t>& output);
    CorrectionResult correctInfiniteLoop(const std::vector<uint8_t>& output);
    CorrectionResult correctTokenLimit(const std::vector<uint8_t>& output);
    
    // Configuration
    void setMaxCorrectionAttempts(int max);
    void setConfidenceThreshold(double threshold);
    void enableCorrectionMethod(const std::string& method, bool enable);
    
    // Statistics
    struct Stats {
        qint64 totalAttempts = 0;
        qint64 successfulCorrections = 0;
        qint64 failedCorrections = 0;
        double avgConfidenceScore = 0.0;
        std::unordered_map<std::string, int> methodSuccessCounts;
    };
    
    Stats getStatistics() const;
    void resetStatistics();

private:
    // Internal correction methods
    std::vector<uint8_t> performGrammarCorrection(const std::vector<uint8_t>& output);
    std::vector<uint8_t> performSemanticCorrection(const std::vector<uint8_t>& output);
    std::vector<uint8_t> performStructuralCorrection(const std::vector<uint8_t>& output);
    
    // Helper methods
    bool detectFormatViolation(const std::vector<uint8_t>& output) const;
    bool detectRefusal(const std::vector<uint8_t>& output) const;
    bool detectHallucination(const std::vector<uint8_t>& output) const;
    double calculateConfidenceScore(const std::vector<uint8_t>& output) const;
    
    mutable std::mutex m_mutex;
    Stats m_stats;
    
    int m_maxAttempts = 3;
    double m_confidenceThreshold = 0.7;
    std::unordered_map<std::string, bool> m_enabledMethods;
};

