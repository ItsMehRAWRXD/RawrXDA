#ifndef RATE_LIMITER_H
#define RATE_LIMITER_H

#include <QObject>
#include <QMap>
#include <QDateTime>
#include <QString>

// Rate-limiter per IP / per user → token-bucket inside PerformanceOptimizer.
class RateLimiter : public QObject
{
    Q_OBJECT

public:
    explicit RateLimiter(QObject *parent = nullptr);
    ~RateLimiter();

    // Set rate limit (requests per second) for an IP or user
    void setRateLimit(const QString &identifier, int requestsPerSecond);

    // Check if a request is allowed for an IP or user
    bool isRequestAllowed(const QString &identifier);

private:
    struct RateLimitInfo {
        int requestsPerSecond;
        int tokens;
        QDateTime lastRequestTime;
    };

    QMap<QString, RateLimitInfo> m_rateLimits;
    
    // Refill tokens based on time elapsed
    void refillTokens(const QString &identifier, RateLimitInfo &info);
};

#endif // RATE_LIMITER_H