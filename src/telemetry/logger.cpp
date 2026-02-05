#include "logger.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

Logger::Logger(QObject *parent)
    : QObject(parent)
    , m_logFile(new QFile(this))
    , m_logStream(new QTextStream())
    , m_maxFileSize(10 * 1024 * 1024) // 10 MB
    , m_currentFileSize(0)
{
    // Create logs directory if it doesn't exist
    QDir logDir("logs");
    if (!logDir.exists()) {
        logDir.mkpath(".");
    }

    // Open log file
    m_logFile->setFileName("logs/app.log");
    if (!m_logFile->open(QIODevice::WriteOnly | QIODevice::Append)) {
        qWarning() << "Failed to open log file:" << m_logFile->errorString();
        return;
    }
    m_logStream->setDevice(m_logFile);
}

Logger::~Logger()
{
    if (m_logStream->device()) {
        m_logStream->flush();
    }
    delete m_logStream;
}

void Logger::log(LogLevel level, const QString &message, const QMap<QString, QVariant> &data)
{
    QMutexLocker locker(&m_mutex);

    // Rotate log file if necessary
    rotateLogFile();

    // Format log entry
    QString logEntry = formatLogEntry(level, message, data);

    // Write to stdout
    qDebug().noquote() << logEntry;

    // Write to file
    if (m_logStream->device()) {
        *m_logStream << logEntry << "\n";
        m_logStream->flush();
        m_currentFileSize += logEntry.toUtf8().size() + 1; // +1 for newline
    }
}

void Logger::rotateLogFile()
{
    if (m_currentFileSize >= m_maxFileSize) {
        m_logStream->flush();
        m_logFile->close();

        // Rename current log file
        QString currentLogPath = m_logFile->fileName();
        QString rotatedLogPath = currentLogPath + "." + QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss");
        QFile::rename(currentLogPath, rotatedLogPath);

        // Open new log file
        m_logFile->setFileName(currentLogPath);
        if (!m_logFile->open(QIODevice::WriteOnly | QIODevice::Append)) {
            qWarning() << "Failed to open new log file:" << m_logFile->errorString();
            return;
        }
        m_logStream->setDevice(m_logFile);
        m_currentFileSize = 0;
    }
}

QString Logger::formatLogEntry(LogLevel level, const QString &message, const QMap<QString, QVariant> &data)
{
    QJsonObject logObject;
    
    // Add timestamp
    logObject["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    
    // Add log level
    QString levelStr;
    switch (level) {
    case Debug: levelStr = "DEBUG"; break;
    case Info: levelStr = "INFO"; break;
    case Warning: levelStr = "WARNING"; break;
    case Error: levelStr = "ERROR"; break;
    }
    logObject["level"] = levelStr;
    
    // Add message
    logObject["message"] = message;
    
    // Add structured data
    for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
        logObject[it.key()] = QJsonValue::fromVariant(it.value());
    }
    
    // Convert to JSON string
    QJsonDocument doc(logObject);
    return doc.toJson(QJsonDocument::Compact);
}