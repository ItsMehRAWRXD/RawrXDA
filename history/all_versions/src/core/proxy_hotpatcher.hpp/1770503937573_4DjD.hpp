// proxy_hotpatcher.hpp — Proxy Hotpatcher (Byte-Level Output Rewriting)
// Token bias injection, stream termination logic, custom validators.
//
// Rule: void* customValidator — function pointer, not std::function
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
#pragma once

#include "model_memory_hotpatch.hpp"
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <atomic>
#include <vector>

// ---------------------------------------------------------------------------
// Token bias entry — adjusts logits before sampling
// ---------------------------------------------------------------------------
struct TokenBias {
    uint32_t    tokenId;
    float       biasValue;      // Positive = boost, negative = suppress
    bool        permanent;      // If false, removed after one application
};

// ---------------------------------------------------------------------------
// Stream termination rule
// ---------------------------------------------------------------------------
struct StreamTerminationRule {
    const char* name;
    const char* stopSequence;       // Null-terminated string to match
    size_t      maxTokens;          // Hard cutoff (0 = unlimited)
    bool        enabled;
};

// ---------------------------------------------------------------------------
// Output rewrite rule — pattern-based text replacement in output stream
// ---------------------------------------------------------------------------
struct OutputRewriteRule {
    const char* name;
    const char* pattern;            // Source pattern (plain text)
    const char* replacement;        // Replacement text
    uint64_t    hitCount;
    bool        enabled;
};

// ---------------------------------------------------------------------------
// Custom validator — function pointer (NOT std::function)
// ---------------------------------------------------------------------------
typedef bool (*ProxyValidatorFn)(const char* output, size_t outputLen, void* userData);

struct ProxyValidator {
    const char*         name;
    ProxyValidatorFn    validate;       // Function pointer
    void*               userData;       // Opaque user data
    bool                enabled;
};

// ---------------------------------------------------------------------------
// ProxyHotpatchStats
// ---------------------------------------------------------------------------
struct ProxyHotpatchStats {
    std::atomic<uint64_t> tokensProcessed{0};
    std::atomic<uint64_t> biasesApplied{0};
    std::atomic<uint64_t> streamsTerminated{0};
    std::atomic<uint64_t> rewritesApplied{0};
    std::atomic<uint64_t> validationsPassed{0};
    std::atomic<uint64_t> validationsFailed{0};
};

// ---------------------------------------------------------------------------
// ProxyHotpatcher — Main class
// ---------------------------------------------------------------------------
class ProxyHotpatcher {
public:
    static ProxyHotpatcher& instance();

    // ---- Token Bias Injection ----
    PatchResult add_token_bias(const TokenBias& bias);
    PatchResult remove_token_bias(uint32_t tokenId);
    PatchResult clear_token_biases();
    // Apply all biases to a logits array (in-place). Returns count of biases applied.
    size_t apply_token_biases(float* logits, size_t vocabSize);

    // ---- Stream Termination ----
    PatchResult add_termination_rule(const StreamTerminationRule& rule);
    PatchResult remove_termination_rule(const char* name);
    // Check if output should be terminated. Returns true if a rule triggered.
    bool check_termination(const char* output, size_t outputLen, size_t tokenCount);

    // ---- Output Rewriting ----
    PatchResult add_rewrite_rule(const OutputRewriteRule& rule);
    PatchResult remove_rewrite_rule(const char* name);
    // Rewrite output in-place (buffer must have room for expansion).
    // Returns the new length after rewrites.
    size_t apply_rewrites(char* output, size_t outputLen, size_t bufferCapacity);

    // ---- Custom Validators ----
    PatchResult add_validator(const ProxyValidator& validator);
    PatchResult remove_validator(const char* name);
    // Run all validators on the output. Returns true if ALL pass.
    bool run_validators(const char* output, size_t outputLen);

    // ---- Statistics ----
    const ProxyHotpatchStats& getStats() const;
    void resetStats();

private:
    ProxyHotpatcher();
    ~ProxyHotpatcher();
    ProxyHotpatcher(const ProxyHotpatcher&) = delete;
    ProxyHotpatcher& operator=(const ProxyHotpatcher&) = delete;

    std::mutex                          m_mutex;
    ProxyHotpatchStats                  m_stats;

    std::vector<TokenBias>              m_biases;
    std::vector<StreamTerminationRule>  m_terminationRules;
    std::vector<OutputRewriteRule>      m_rewriteRules;
    std::vector<ProxyValidator>         m_validators;
};
