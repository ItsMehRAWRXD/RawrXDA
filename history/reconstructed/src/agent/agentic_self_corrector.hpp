// agentic_self_corrector.hpp — Automated Self-Correction Loop (Qt-free)
// Combines failure detection + puppeteer correction into an N-retry loop.
// PATTERN:   No exceptions. CorrectionResult returns.
// THREADING: Mutex-protected. Thread-safe.
// RULE:      NO SOURCE FILE IS TO BE SIMPLIFIED
#pragma once

#include "agentic_puppeteer.hpp"
#include "agentic_failure_detector.hpp"
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

struct SelfCorrectionResult {
    bool        success;
    std::string finalOutput;
    int         attemptsUsed;
    FailureType lastFailure;
    const char* detail;

    static SelfCorrectionResult ok(const std::string& output, int attempts) {
        return { true, output, attempts, FailureType::None, "Self-correction succeeded" };
    }
    static SelfCorrectionResult error(FailureType ft, const char* msg, int attempts) {
        return { false, "", attempts, ft, msg };
    }
};

class AgenticSelfCorrector {
public:
    AgenticSelfCorrector();
    ~AgenticSelfCorrector();

    // Primary API: run self-correction loop (up to maxAttempts retries)
    SelfCorrectionResult correct(const std::string& modelOutput,
                                 const std::string& userPrompt,
                                 int maxAttempts = 3);

    // Configure
    void setMaxAttempts(int n);
    int  getMaxAttempts() const;
    void setEnabled(bool enable);
    bool isEnabled() const;

    // Access underlying subsystems
    AgenticPuppeteer&        puppeteer();
    AgenticFailureDetector&  detector();

    // Statistics
    struct Stats {
        int64_t totalRequests      = 0;
        int64_t successfulFixes    = 0;
        int64_t failedAfterRetries = 0;
        int64_t totalAttempts      = 0;
    };

    Stats getStats() const;
    void  resetStats();

    // Callbacks — raw function pointers, no std::function
    void (*onCorrectionLoop)(int attempt, FailureType ft, void* ctx) = nullptr;
    void (*onCorrectionComplete)(bool success, const char* output, void* ctx) = nullptr;
    void* callbackContext = nullptr;

private:
    mutable std::mutex       m_mutex;
    AgenticPuppeteer         m_puppeteer;
    AgenticFailureDetector   m_detector;
    int                      m_maxAttempts = 3;
    bool                     m_enabled     = true;
    Stats                    m_stats;
};
