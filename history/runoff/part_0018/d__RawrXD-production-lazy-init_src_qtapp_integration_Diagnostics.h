#pragma once
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include "InitializationTracker.h"

namespace RawrXD {
namespace Integration {

// Optional diagnostic utilities for runtime introspection
class Diagnostics {
public:
    // Generate a JSON snapshot of initialization events
    static QJsonObject initializationReport() {
        QJsonObject report;
        report["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        
        const auto &events = InitializationTracker::instance().events();
        QJsonArray eventsArray;
        
        for (const auto &evt : events) {
            QJsonObject eventObj;
            eventObj["subsystem"] = evt.subsystem;
            eventObj["event"] = evt.event;
            eventObj["latency_ms"] = static_cast<qint64>(evt.latency_ms);
            eventObj["timestamp"] = evt.timestamp.toString(Qt::ISODate);
            eventsArray.append(eventObj);
        }
        
        report["events"] = eventsArray;
        report["total_events"] = static_cast<qint64>(events.size());
        
        // Calculate total startup time
        if (!events.isEmpty()) {
            qint64 totalMs = 0;
            for (const auto &evt : events) {
                totalMs += evt.latency_ms;
            }
            report["total_startup_ms"] = totalMs;
        }
        
        return report;
    }

    // Log diagnostics to qDebug
    static void dumpInitializationReport() {
        QJsonObject report = initializationReport();
        qInfo().noquote() << "[Diagnostics] Initialization Report:"
                          << report;
    }

    // Return human-readable summary
    static QString initializationSummary() {
        const auto &events = InitializationTracker::instance().events();
        
        if (events.isEmpty()) {
            return "No initialization events recorded.";
        }

        QString summary = QString("Initialization Summary (%1 events):\n").arg(events.size());
        qint64 totalMs = 0;
        
        for (const auto &evt : events) {
            summary += QString("  [%1ms] %2::%3\n")
                .arg(evt.latency_ms, 4)
                .arg(evt.subsystem)
                .arg(evt.event);
            totalMs += evt.latency_ms;
        }
        
        summary += QString("\nTotal startup time: %1ms\n").arg(totalMs);
        return summary;
    }
};

} // namespace Integration
} // namespace RawrXD
