// ============================================================================
// rate_limiting_engine.cpp — Enterprise Rate Limiting Implementation
// ============================================================================

#include "rate_limiting_engine.hpp"
#include <algorithm>
#include <cmath>

namespace RawrXD {

RateLimitingEngine::RateLimitingEngine() = default;

int64_t RateLimitingEngine::nowMs() const {
    return (int64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

void RateLimitingEngine::setLimit(const std::string& key, const RateLimitConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    Bucket& b = buckets_[key];
    b.config = config;
    b.tokens = config.burstSize ? (double)config.burstSize : 1000.0;
    b.lastRefillMs = nowMs();
    b.requestsThisMinute = 0;
    b.tokensThisMinute = 0;
    b.minuteStartMs = b.lastRefillMs;
}

void RateLimitingEngine::reset(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = buckets_.find(key);
    if (it != buckets_.end()) {
        it->second.tokens = it->second.config.burstSize ? (double)it->second.config.burstSize : 1000.0;
        it->second.lastRefillMs = nowMs();
        it->second.requestsThisMinute = 0;
        it->second.tokensThisMinute = 0;
        it->second.minuteStartMs = it->second.lastRefillMs;
    }
}

RateLimitResult RateLimitingEngine::allow(const std::string& key, size_t tokenEstimate) {
    std::lock_guard<std::mutex> lock(mutex_);
    RateLimitResult r{ true, 0, 0 };
    auto it = buckets_.find(key);
    if (it == buckets_.end()) {
        r.remaining = 1000;
        return r;
    }
    Bucket& b = it->second;
    int64_t t = nowMs();

    // Minute window
    if (t - b.minuteStartMs >= 60000) {
        b.minuteStartMs = t;
        b.requestsThisMinute = 0;
        b.tokensThisMinute = 0;
    }
    if (b.config.maxRequestsPerMinute && b.requestsThisMinute >= b.config.maxRequestsPerMinute) {
        r.allowed = false;
        r.retryAfterMs = 60000 - (t - b.minuteStartMs);
        r.remaining = 0;
        return r;
    }
    if (b.config.maxTokensPerMinute && (b.tokensThisMinute + tokenEstimate) > b.config.maxTokensPerMinute) {
        r.allowed = false;
        r.retryAfterMs = 60000 - (t - b.minuteStartMs);
        r.remaining = 0;
        return r;
    }

    // Token bucket (refill by time)
    double refillRate = b.config.maxRequestsPerSecond ? (double)b.config.maxRequestsPerSecond : 1000.0;
    if (b.config.burstSize) {
        double elapsed = (t - b.lastRefillMs) / 1000.0;
        b.tokens = std::min((double)b.config.burstSize, b.tokens + elapsed * refillRate);
        b.lastRefillMs = t;
        if (b.tokens < (double)tokenEstimate) {
            r.allowed = false;
            r.retryAfterMs = (int64_t)((tokenEstimate - b.tokens) / refillRate * 1000.0);
            r.remaining = (size_t)std::max(0.0, b.tokens);
            return r;
        }
        b.tokens -= (double)tokenEstimate;
    }

    b.requestsThisMinute++;
    b.tokensThisMinute += tokenEstimate;
    r.remaining = b.config.maxRequestsPerMinute
        ? (b.config.maxRequestsPerMinute - b.requestsThisMinute)
        : 999;
    return r;
}

void RateLimitingEngine::getUsage(const std::string& key, size_t* requestsLastMinute, size_t* tokensLastMinute) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = buckets_.find(key);
    if (it == buckets_.end()) {
        if (requestsLastMinute) *requestsLastMinute = 0;
        if (tokensLastMinute) *tokensLastMinute = 0;
        return;
    }
    const Bucket& b = it->second;
    int64_t t = nowMs();
    if (t - b.minuteStartMs >= 60000) {
        if (requestsLastMinute) *requestsLastMinute = 0;
        if (tokensLastMinute) *tokensLastMinute = 0;
        return;
    }
    if (requestsLastMinute) *requestsLastMinute = b.requestsThisMinute;
    if (tokensLastMinute) *tokensLastMinute = b.tokensThisMinute;
}

} // namespace RawrXD
