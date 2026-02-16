// ============================================================================
// rate_limiting_engine.hpp — Enterprise Rate Limiting (per-user/API/key)
// ============================================================================
// Feature: RateLimitingEngine (Enterprise tier)
// Token-bucket and sliding-window limits; per-user, per-API-key, global.
// Thread-safe; no exceptions.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>

namespace RawrXD {

struct RateLimitConfig {
    size_t maxRequestsPerSecond = 0;   // 0 = no limit
    size_t maxRequestsPerMinute = 0;
    size_t maxTokensPerMinute   = 0;  // Approximate token count
    size_t burstSize             = 0;  // Token bucket burst
};

struct RateLimitResult {
    bool allowed;
    int64_t retryAfterMs;   // If !allowed, suggested wait
    size_t remaining;       // Estimated remaining in window
};

class RateLimitingEngine {
public:
    RateLimitingEngine();
    ~RateLimitingEngine() = default;

    // Set limit for a key (userId, apiKeyId, or "global").
    void setLimit(const std::string& key, const RateLimitConfig& config);

    // Check whether a request is allowed. If tokenEstimate > 0, consume that many tokens.
    RateLimitResult allow(const std::string& key, size_t tokenEstimate = 1);

    // Reset counters for a key (e.g. after admin override).
    void reset(const std::string& key);

    // Get current usage for a key (for dashboards).
    void getUsage(const std::string& key, size_t* requestsLastMinute, size_t* tokensLastMinute) const;

private:
    struct Bucket {
        RateLimitConfig config;
        double tokens;           // Token bucket level
        int64_t lastRefillMs;
        size_t requestsThisMinute;
        size_t tokensThisMinute;
        int64_t minuteStartMs;
    };
    mutable std::mutex mutex_;
    std::unordered_map<std::string, Bucket> buckets_;
    int64_t nowMs() const;
};

} // namespace RawrXD
