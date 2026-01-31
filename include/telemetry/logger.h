#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QMap>
#include <QVariant>
#include <QString>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMutex>

// Structured JSON logs → stdout + rotating file
class Logger : public QObject
{
    Q_OBJECT

public:
    enum LogLevel {
        Debug = 0,
        Info = 1,
        Warning = 2,
        Error = 3
    };

    explicit Logger(QObject *parent = nullptr);
    ~Logger();

    // Log a message with structured data
    void log(LogLevel level, const QString &message, const QMap<QString, QVariant> &data = QMap<QString, QVariant>());

    // Convenience functions
    void logDebug(const QString &message, const QMap<QString, QVariant> &data = QMap<QString, QVariant>()) {
        log(Debug, message, data);
    }
    void logInfo(const QString &message, const QMap<QString, QVariant> &data = QMap<QString, QVariant>()) {
        log(Info, message, data);
    }
    void logWarning(const QString &message, const QMap<QString, QVariant> &data = QMap<QString, QVariant>()) {
        log(Warning, message, data);
    }
    void logError(const QString &message, const QMap<QString, QVariant> &data = QMap<QString, QVariant>()) {
        log(Error, message, data);
    }

private:
    QFile *m_logFile;
    QTextStream *m_logStream;
    QMutex m_mutex;
    int m_maxFileSize;
    int m_currentFileSize;

    // Rotate log file if necessary
    void rotateLogFile();

    // Format log entry as JSON
    QString formatLogEntry(LogLevel level, const QString &message, const QMap<QString, QVariant> &data);
};

#endif // LOGGER_H