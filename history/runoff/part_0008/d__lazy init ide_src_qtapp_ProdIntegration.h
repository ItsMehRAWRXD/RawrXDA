#pragma once
#include <QtGlobal>
#include <QDebug>
#include <QString>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDateTime>
#include <QThread>

namespace RawrXD {
namespace Integration {

struct Config {
    static bool loggingEnabled() {
        return !qEnvironmentVariableIsEmpty("RAWRXD_LOGGING_ENABLED") && qgetenv("RAWRXD_LOGGING_ENABLED") != "0";
    }
    static bool stubLoggingEnabled() {
        return !qEnvironmentVariableIsEmpty("RAWRXD_LOG_STUBS") && qgetenv("RAWRXD_LOG_STUBS") != "0";
    }
    static bool metricsEnabled() {
        return !qEnvironmentVariableIsEmpty("RAWRXD_ENABLE_METRICS") && qgetenv("RAWRXD_ENABLE_METRICS") != "0";
    }
    static bool tracingEnabled() {
        return !qEnvironmentVariableIsEmpty("RAWRXD_ENABLE_TRACING") && qgetenv("RAWRXD_ENABLE_TRACING") != "0";
    }
};

#define RAWRXD_INIT_TIMED(name) RawrXD::Integration::ScopedTimer __rawr_timer__("main", name, "init")
#define RAWRXD_TIMED_FUNC() RawrXD::Integration::ScopedTimer __rawr_timer__(__FUNCTION__, __FUNCTION__, "execution")
#define RAWRXD_TIMED_NAMED(name) RawrXD::Integration::ScopedTimer __rawr_timer__(name, name, "execution")

inline void logWithLevel(const QString &level, const QString &component, const QString &event, const QString &message, const QJsonObject &data = QJsonObject()) {
    if (!Config::loggingEnabled()) return;
    QJsonObject logEntry{
        {"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {"level", level},
        {"component", component},
        {"event", event},
        {"message", message}
    };
    if (!data.isEmpty()) {
        logEntry.insert("data", data);
    }
    qInfo().noquote() << QJsonDocument(logEntry).toJson(QJsonDocument::Compact);
}

inline void logInfo(const QString &component, const QString &event, const QString &message, const QJsonObject &data = QJsonObject()) {
    logWithLevel(QStringLiteral("INFO"), component, event, message, data);
}

inline void logDebug(const QString &component, const QString &event, const QString &message, const QJsonObject &data = QJsonObject()) {
    logWithLevel(QStringLiteral("DEBUG"), component, event, message, data);
}

inline void logWarn(const QString &component, const QString &event, const QString &message, const QJsonObject &data = QJsonObject()) {
    logWithLevel(QStringLiteral("WARN"), component, event, message, data);
}

inline void logError(const QString &component, const QString &event, const QString &message, const QJsonObject &data = QJsonObject()) {
    logWithLevel(QStringLiteral("ERROR"), component, event, message, data);
}

struct ScopedTimer {
    QString component;
    QString name;
    QString event;
    QElapsedTimer timer;
    explicit ScopedTimer(const char *componentName, const char *objectName, const char *eventName)
        : component(QString::fromUtf8(componentName)),
          name(QString::fromUtf8(objectName)),
          event(QString::fromUtf8(eventName)) {
        if (Config::loggingEnabled()) {
            timer.start();
        }
    }
    ~ScopedTimer() {
        if (Config::loggingEnabled()) {
            QJsonObject logEntry{
                {"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
                {"level", "INFO"},
                {"component", component},
                {"event", event},
                {"name", name},
                {"latency_ms", timer.elapsed()}
            };
            qInfo().noquote() << QJsonDocument(logEntry).toJson(QJsonDocument::Compact);
        }
    }
};

inline void recordMetric(const char *metricName, qint64 value = 1) {
    if (!Config::metricsEnabled()) return;
    QJsonObject metricEntry{
        {"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {"type", "metric"},
        {"name", QString::fromUtf8(metricName)},
        {"value", value}
    };
    qInfo().noquote() << QJsonDocument(metricEntry).toJson(QJsonDocument::Compact);
}

inline void traceEvent(const char *spanName, const char *eventName) {
    if (!Config::tracingEnabled()) return;
    QJsonObject traceEntry{
        {"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {"type", "trace"},
        {"span", QString::fromUtf8(spanName)},
        {"event", QString::fromUtf8(eventName)}
    };
    qInfo().noquote() << QJsonDocument(traceEntry).toJson(QJsonDocument::Compact);
}

template <typename Fn>
auto retryWithBackoff(Fn fn, int maxRetries = 3, qint64 delayMs = 100) {
    int attempt = 0;
    while (true) {
        try {
            return fn();
        } catch (const std::exception &ex) {
            ++attempt;
            if (attempt >= maxRetries) {
                logError(QStringLiteral("Retry"), QStringLiteral("exhausted"),
                         QStringLiteral("Max retries reached: %1").arg(QString::fromUtf8(ex.what())));
                throw;
            }
            logWarn(QStringLiteral("Retry"), QStringLiteral("attempt"),
                    QStringLiteral("Attempt %1 failed, retrying in %2ms").arg(attempt).arg(delayMs));
            QThread::msleep(static_cast<unsigned long>(delayMs));
            delayMs *= 2;
        }
    }
}

class CircuitBreaker {
public:
    enum class State { Closed, Open, HalfOpen };

    explicit CircuitBreaker(int failureThreshold = 5, qint64 resetTimeoutMs = 30000)
        : m_failureThreshold(failureThreshold), m_resetTimeoutMs(resetTimeoutMs) {}

    bool allowRequest() {
        if (m_state == State::Closed) return true;
        if (m_state == State::Open) {
            if (m_timer.elapsed() >= m_resetTimeoutMs) {
                m_state = State::HalfOpen;
                logInfo(QStringLiteral("CircuitBreaker"), QStringLiteral("half_open"), QStringLiteral("Testing circuit"));
                return true;
            }
            return false;
        }
        return true;
    }

    void recordSuccess() {
        if (m_state == State::HalfOpen) {
            m_state = State::Closed;
            m_failures = 0;
            logInfo(QStringLiteral("CircuitBreaker"), QStringLiteral("closed"), QStringLiteral("Circuit recovered"));
        }
    }

    void recordFailure() {
        ++m_failures;
        if (m_state == State::HalfOpen || m_failures >= m_failureThreshold) {
            m_state = State::Open;
            m_timer.start();
            logWarn(QStringLiteral("CircuitBreaker"), QStringLiteral("open"),
                    QStringLiteral("Circuit opened after %1 failures").arg(m_failures));
        }
    }

    State state() const { return m_state; }

private:
    State m_state = State::Closed;
    int m_failures = 0;
    int m_failureThreshold;
    qint64 m_resetTimeoutMs;
    QElapsedTimer m_timer;
};

} // namespace Integration
} // namespace RawrXD
