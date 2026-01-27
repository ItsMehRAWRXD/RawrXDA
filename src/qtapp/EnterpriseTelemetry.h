#pragma once

#include <chrono>
#include <mutex>
#include <QString>
#include <QLoggingCategory>
#include <QElapsedTimer>

namespace RawrXD {

class EnterpriseTelemetry {
public:
    struct ScopedTimer {
        QString name;
        qint64 startNs{0};
        QElapsedTimer timer;
        ScopedTimer(const QString &label) : name(label) {
            timer.start();
            startNs = std::chrono::steady_clock::now().time_since_epoch().count();
        }
        qint64 elapsedMs() const { return timer.elapsed(); }
    };

    static EnterpriseTelemetry &instance() {
        static EnterpriseTelemetry inst;
        return inst;
    }

    void recordEvent(const QString &category, const QString &name, const QString &details = QString()) {
        logEvent(category, name, details, 0);
    }

    void recordTiming(const QString &category, const QString &name, qint64 durationMs, const QString &details = QString()) {
        logEvent(category, name, details, durationMs);
    }

    ScopedTimer startTimer(const QString &name) { return ScopedTimer(name); }

private:
    EnterpriseTelemetry() = default;

    void logEvent(const QString &category, const QString &name, const QString &details, qint64 durationMs) {
        QLoggingCategory cat("RawrXD.Telemetry");
        const QString message = durationMs > 0
            ? QStringLiteral("[%1] %2 duration=%3ms %4").arg(category, name).arg(durationMs).arg(details)
            : QStringLiteral("[%1] %2 %3").arg(category, name, details);
        qInfo(cat) << message;
    }
};

} // namespace RawrXD
