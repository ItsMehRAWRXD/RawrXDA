#include "rate_limiter.h"
RateLimiter::RateLimiter()
    
{
}

RateLimiter::~RateLimiter()
{
}

void RateLimiter::setRateLimit(const std::string &identifier, int requestsPerSecond)
{
    RateLimitInfo info;
    info.requestsPerSecond = requestsPerSecond;
    info.tokens = requestsPerSecond; // Start with full bucket
    info.lastRequestTime = // DateTime::currentDateTime();
    m_rateLimits[identifier] = info;
}

bool RateLimiter::isRequestAllowed(const std::string &identifier)
{
    if (!m_rateLimits.contains(identifier)) {
        // No rate limit set for this identifier, allow request
        return true;
    }
    
    RateLimitInfo &info = m_rateLimits[identifier];
    refillTokens(identifier, info);
    
    if (info.tokens > 0) {
        info.tokens--;
        info.lastRequestTime = // DateTime::currentDateTime();
        return true;
    }
    
    return false;
}

void RateLimiter::refillTokens(const std::string &identifier, RateLimitInfo &info)
{
    // DateTime now = // DateTime::currentDateTime();
    int64_t elapsedMs = info.lastRequestTime.msecsTo(now);
    
    // Calculate how many tokens to add based on elapsed time
    double tokensToAdd = (double)elapsedMs / 1000.0 * info.requestsPerSecond;
    
    if (tokensToAdd > 0) {
        info.tokens += (int)tokensToAdd;
        // Ensure we don't exceed the bucket size
        if (info.tokens > info.requestsPerSecond) {
            info.tokens = info.requestsPerSecond;
        }
        info.lastRequestTime = now;
        // // qDebug:  "Refilled tokens for" << identifier << "to" << info.tokens;
    }
}




