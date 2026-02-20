#ifndef RATE_LIMITER_H
#define RATE_LIMITER_H

// C++20, no Qt. Token-bucket rate limiter per identifier (IP/user).

#include <string>
#include <map>
#include <chrono>
#include <mutex>

struct RateLimitInfo {
    int requestsPerSecond = 0;
    int tokens = 0;
    std::chrono::steady_clock::time_point lastRequestTime;
};

class RateLimiter
{
public:
    RateLimiter() = default;
    ~RateLimiter() = default;

    void setRateLimit(const std::string& identifier, int requestsPerSecond);
    bool isRequestAllowed(const std::string& identifier);

private:
    void refillTokens(const std::string& identifier, RateLimitInfo& info);

    std::mutex m_mutex;
    std::map<std::string, RateLimitInfo> m_rateLimits;
};

#endif // RATE_LIMITER_H
