#pragma once
#include <QDebug>
#include <QString>
#include <QDateTime>
#include "ProdIntegration.h"

namespace RawrXD {
namespace Integration {

// Unified structured logger respecting env toggles
class Logger {
public:
    enum Level { DEBUG, INFO, WARN, ERROR };

    static Logger& info() {
        static Logger inst(INFO);
        return inst;
    }

    static Logger& debug() {
        static Logger inst(DEBUG);
        return inst;
    }

    static Logger& warn() {
        static Logger inst(WARN);
        return inst;
    }

    static Logger& error() {
        static Logger inst(ERROR);
        return inst;
    }

    // Chainable API
    Logger& component(const QString &comp) {
        m_component = comp;
        return *this;
    }

    Logger& event(const QString &evt) {
        m_event = evt;
        return *this;
    }

    Logger& message(const QString &msg) {
        m_message = msg;
        return *this;
    }

    void flush() {
        if (!Config::loggingEnabled()) return;

        QString levelStr;
        switch (m_level) {
            case DEBUG: levelStr = "DEBUG"; break;
            case INFO:  levelStr = "INFO"; break;
            case WARN:  levelStr = "WARN"; break;
            case ERROR: levelStr = "ERROR"; break;
        }

        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
        qInfo().noquote() << QString("[%1] [%2] [%3] [%4] %5")
                             .arg(timestamp)
                             .arg(levelStr)
                             .arg(m_component)
                             .arg(m_event)
                             .arg(m_message);
        
        reset();
    }

    ~Logger() {
        flush();
    }

private:
    explicit Logger(Level level) : m_level(level) {}

    void reset() {
        m_component = "General";
        m_event = "event";
        m_message = "";
    }

    Level m_level;
    QString m_component = "General";
    QString m_event = "event";
    QString m_message = "";
};

} // namespace Integration
} // namespace RawrXD
