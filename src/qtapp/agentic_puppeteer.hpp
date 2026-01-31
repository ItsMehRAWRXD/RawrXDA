// agentic_puppeteer.hpp - Automatic failure correction and model steering
#pragma once

#include "agentic_failure_detector.hpp"
#include "proxy_hotpatcher.hpp"

#include <functional>
#include <mutex>
#include <string>
#include <vector>
#include <cstdint>

// Correction strategy
enum class CorrectionStrategy {
    Retry,              // Simple retry with same prompt
    Rephrase,           // Rephrase the prompt
    AddContext,         // Add missing context
    ParameterAdjust,    // Adjust temperature/parameters
    SystemPrompt,       // Inject corrective system prompt
    FormatEnforce,      // Force output format
    HotpatchBypass      // Use proxy hotpatcher to bypass refusal
};

// Correction result
struct CorrectionResult {
    bool success = false;
    std::string correctedResponse;
    CorrectionStrategy strategyUsed;
    int attemptsUsed = 0;
    std::string errorMessage;
    
    static CorrectionResult succeeded(const std::string& response, CorrectionStrategy strategy, int attempts) {
        return CorrectionResult{true, response, strategy, attempts, ""};
    }
    
    static CorrectionResult failed(const std::string& error, int attempts) {
        return CorrectionResult{false, "", CorrectionStrategy::Retry, attempts, error};
    }
};

class AgenticPuppeteer
{

public:
    explicit AgenticPuppeteer();
    virtual ~AgenticPuppeteer();

    // Main correction method
    CorrectionResult correctFailure(
        const FailureDetection& failure,
        const std::string& originalPrompt,
        const std::string& failedResponse,
        std::function<std::string(const std::string&)> modelCallback
    );
    
    // Specialized correction methods
    CorrectionResult correctRefusal(
        const std::string& prompt,
        const std::string& refusedResponse,
        std::function<std::string(const std::string&)> modelCallback
    );
    
    CorrectionResult correctHallucination(
        const std::string& prompt,
        const std::string& hallucinatedResponse,
        const std::string& correctContext,
        std::function<std::string(const std::string&)> modelCallback
    );
    
    CorrectionResult correctFormatViolation(
        const std::string& prompt,
        const std::string& malformedResponse,
        const std::string& expectedFormat,
        std::function<std::string(const std::string&)> modelCallback
    );
    
    CorrectionResult correctInfiniteLoop(
        const std::string& prompt,
        const std::string& loopingResponse,
        std::function<std::string(const std::string&)> modelCallback
    );
    
    // Configuration
    void setMaxRetries(int maxRetries);
    void setRetryDelay(int delayMs);
    void setEnableHotpatching(bool enable);
    void setDefaultStrategy(CorrectionStrategy strategy);
    void setProxyHotpatcher(ProxyHotpatcher* hotpatcher);
    
    // Statistics
    struct Stats {
        int64_t totalCorrections = 0;
        int64_t successfulCorrections = 0;
        int64_t failedCorrections = 0;
        int64_t refusalsBypassed = 0;
        int64_t hallucinationsCorrected = 0;
        int64_t formatsCorrected = 0;
        int64_t loopsBroken = 0;
        double successRate = 0.0;
    };
    
    Stats getStatistics() const;
    void resetStatistics();

    void correctionAttempted(CorrectionStrategy strategy, int attemptNumber);
    void correctionSucceeded(CorrectionStrategy strategy, int attempts);
    void correctionFailed(const std::string& reason, int attempts);
    void refusalBypassed(const std::string& originalPrompt);

protected:
    // Strategy selection
    CorrectionStrategy selectStrategy(const FailureDetection& failure);
    
    // Strategy implementations
    std::string retryWithSamePrompt(const std::string& prompt, std::function<std::string(const std::string&)> callback);
    std::string retryWithRephrase(const std::string& prompt, std::function<std::string(const std::string&)> callback);
    std::string retryWithContext(const std::string& prompt, const std::string& context, std::function<std::string(const std::string&)> callback);
    std::string retryWithParameterAdjust(const std::string& prompt, std::function<std::string(const std::string&)> callback);
    std::string retryWithSystemPrompt(const std::string& prompt, const std::string& systemPrompt, std::function<std::string(const std::string&)> callback);
    std::string retryWithFormatEnforcement(const std::string& prompt, const std::string& format, std::function<std::string(const std::string&)> callback);
    std::string bypassWithHotpatch(const std::string& prompt, std::function<std::string(const std::string&)> callback);
    
    // Helper methods
    std::string rephrasePrompt(const std::string& original);
    std::string generateSystemPrompt(FailureType type);
    std::string extractFormatFromPrompt(const std::string& prompt);
    bool isResponseValid(const std::string& response, FailureType originalFailure);
    
    // Data members
    mutable std::mutex m_mutex;
    ProxyHotpatcher* m_proxyHotpatcher = nullptr;
    
    int m_maxRetries = 3;
    int m_retryDelay = 500; // ms
    bool m_enableHotpatching = true;
    CorrectionStrategy m_defaultStrategy = CorrectionStrategy::Rephrase;
    
    Stats m_stats;
};

// Specialized puppeteer classes

class RefusalBypassPuppeteer : public AgenticPuppeteer
{

public:
    explicit RefusalBypassPuppeteer();
    
    // Override with refusal-specific logic
    CorrectionResult bypassRefusal(
        const std::string& prompt,
        std::function<std::string(const std::string&)> callback
    );

private:
    std::vector<std::string> generateBypassPhrases(const std::string& originalPrompt);
    std::string injectBypassSystemPrompt();
};

class HallucinationCorrectorPuppeteer : public AgenticPuppeteer
{

public:
    explicit HallucinationCorrectorPuppeteer();
    
    CorrectionResult correctWithGrounding(
        const std::string& prompt,
        const std::string& groundTruth,
        std::function<std::string(const std::string&)> callback
    );

private:
    std::string buildGroundedPrompt(const std::string& original, const std::string& facts);
    bool verifyFactualAccuracy(const std::string& response, const std::string& groundTruth);
};

class FormatEnforcerPuppeteer : public AgenticPuppeteer
{

public:
    explicit FormatEnforcerPuppeteer();
    
    CorrectionResult enforceFormat(
        const std::string& prompt,
        const std::string& formatSpec,
        std::function<std::string(const std::string&)> callback
    );

private:
    std::string generateFormatInstructions(const std::string& formatSpec);
    bool validateFormat(const std::string& response, const std::string& formatSpec);
    std::string autoFixFormat(const std::string& response, const std::string& formatSpec);
};

