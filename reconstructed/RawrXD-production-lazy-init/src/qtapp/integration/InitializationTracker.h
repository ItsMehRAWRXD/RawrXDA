#pragma once
#include <QString>
#include <QMap>
#include <QElapsedTimer>
#include <QDebug>
#include <QtGlobal>

namespace RawrXD {
namespace Integration {

// Centralized tracker for subsystem initialization order and latency
class InitializationTracker {
public:
    struct Event {
        QString subsystem;
        QString event;
        qint64 latency_ms;
        QDateTime timestamp;
    };

    static InitializationTracker& instance() {
        static InitializationTracker inst;
        return inst;
    }

    void recordEvent(const QString &subsystem, const QString &event, qint64 latency_ms) {
        if (!Config::loggingEnabled()) return;
        
        Event evt{subsystem, event, latency_ms, QDateTime::currentDateTime()};
        m_events.append(evt);
        
        if (Config::loggingEnabled()) {
            qInfo().noquote() << "[InitTracker]" << subsystem << ":" << event
                              << "latency_ms=" << latency_ms;
        }
    }

    QList<Event> events() const { return m_events; }
    void reset() { m_events.clear(); }

private:
    InitializationTracker() = default;
    QList<Event> m_events;
};

// Scoped initialization timer for tracking subsystem startup
struct ScopedInitTimer {
    QString subsystem;
    QElapsedTimer timer;
    
    explicit ScopedInitTimer(const char *subsystemName)
        : subsystem(QString::fromUtf8(subsystemName)) {
        if (Config::loggingEnabled()) {
            timer.start();
        }
    }
    
    ~ScopedInitTimer() {
        if (Config::loggingEnabled()) {
            InitializationTracker::instance().recordEvent(subsystem, "init", timer.elapsed());
        }
    }
};

} // namespace Integration
} // namespace RawrXD
