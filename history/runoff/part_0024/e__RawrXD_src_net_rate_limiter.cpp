#include "rate_limiter.h"
#include <QDateTime>
#include <QDebug>

RateLimiter::RateLimiter(QObject *parent)
    : QObject(parent)
{
}

RateLimiter::~RateLimiter()
{
}

void RateLimiter::setRateLimit(const QString &identifier, int requestsPerSecond)
{
    RateLimitInfo info;
    info.requestsPerSecond = requestsPerSecond;
    info.tokens = requestsPerSecond; // Start with full bucket
    info.lastRequestTime = QDateTime::currentDateTime();
    m_rateLimits[identifier] = info;
}

bool RateLimiter::isRequestAllowed(const QString &identifier)
{
    if (!m_rateLimits.contains(identifier)) {
        // No rate limit set for this identifier, allow request
        return true;
    }
    
    RateLimitInfo &info = m_rateLimits[identifier];
    refillTokens(identifier, info);
    
    if (info.tokens > 0) {
        info.tokens--;
        info.lastRequestTime = QDateTime::currentDateTime();
        return true;
    }
    
    return false;
}

void RateLimiter::refillTokens(const QString &identifier, RateLimitInfo &info)
{
    QDateTime now = QDateTime::currentDateTime();
    qint64 elapsedMs = info.lastRequestTime.msecsTo(now);
    
    // Calculate how many tokens to add based on elapsed time
    double tokensToAdd = (double)elapsedMs / 1000.0 * info.requestsPerSecond;
    
    if (tokensToAdd > 0) {
        info.tokens += (int)tokensToAdd;
        // Ensure we don't exceed the bucket size
        if (info.tokens > info.requestsPerSecond) {
            info.tokens = info.requestsPerSecond;
        }
        info.lastRequestTime = now;
        qDebug() << "Refilled tokens for" << identifier << "to" << info.tokens;
    }
}