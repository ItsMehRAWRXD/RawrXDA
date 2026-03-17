#include "structured_logger.h"
#include <QDir>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QThread>
#include <QDateTime>
#include <QUuid>

namespace RawrXD {

StructuredLogger& StructuredLogger::instance() {
    static StructuredLogger instance;
    return instance;
}

void StructuredLogger::initialize(const QString& logFilePath, LogLevel level) {
    QMutexLocker lock(&mutex_);
    
    if (initialized_) {
        return;
    }
    
    currentLevel_ = level;
    
    QString actualPath = logFilePath;
    if (actualPath.isEmpty()) {
        QDir appDir(QCoreApplication::applicationDirPath());
        QDir logDir(appDir.filePath("logs"));
        if (!logDir.exists()) {
            logDir.mkpath(".");
        }
        actualPath = logDir.filePath("rawrxd-production.log");
    }
    
    logFile_.setFileName(actualPath);
    if (!logFile_.open(QIODevice::WriteOnly | QIODevice::Append)) {
        // Fallback to stderr
        qWarning() << "Failed to open log file:" << actualPath;
        return;
    }
    
    initialized_ = true;
    
    QJsonObject initLog;
    initLog["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    initLog["level"] = "INFO";
    initLog["message"] = "Structured logger initialized";
    initLog["logger"] = "StructuredLogger";
    initLog["file"] = actualPath;
    
    writeLogEntry(initLog);
}

void StructuredLogger::shutdown() {
    QMutexLocker lock(&mutex_);
    
    if (initialized_) {
        QJsonObject shutdownLog;
        shutdownLog["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        shutdownLog["level"] = "INFO";
        shutdownLog["message"] = "Structured logger shutting down";
        shutdownLog["logger"] = "StructuredLogger";
        
        writeLogEntry(shutdownLog);
        
        logFile_.close();
        initialized_ = false;
    }
}

void StructuredLogger::log(LogLevel level, const QString& message, const QJsonObject& context) {
    if (level < currentLevel_ || !initialized_) {
        return;
    }
    
    QJsonObject logEntry;
    logEntry["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    logEntry["level"] = levelToString(level);
    logEntry["message"] = message;
    logEntry["thread_id"] = QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId()));
    
    if (!context.isEmpty()) {
        logEntry["context"] = context;
    }
    
    if (!currentSpanId_.isEmpty()) {
        logEntry["span_id"] = currentSpanId_;
    }
    
    writeLogEntry(logEntry);
}

void StructuredLogger::logWithMetrics(LogLevel level, const QString& message, const QString& operation, 
                                     qint64 durationMs, const QJsonObject& context) {
    QJsonObject enhancedContext = context;
    enhancedContext["operation"] = operation;
    enhancedContext["duration_ms"] = durationMs;
    
    log(level, message, enhancedContext);
    
    // Record metric
    if (durationMs > 0) {
        recordMetric("operation_duration", durationMs, {{ "operation", operation }});
    }
}

void StructuredLogger::trace(const QString& message, const QJsonObject& context) {
    log(LogLevel::TRACE, message, context);
}

void StructuredLogger::debug(const QString& message, const QJsonObject& context) {
    log(LogLevel::DEBUG, message, context);
}

void StructuredLogger::info(const QString& message, const QJsonObject& context) {
    log(LogLevel::INFO, message, context);
}

void StructuredLogger::warn(const QString& message, const QJsonObject& context) {
    log(LogLevel::WARN, message, context);
}

void StructuredLogger::error(const QString& message, const QJsonObject& context) {
    log(LogLevel::ERROR, message, context);
}

void StructuredLogger::fatal(const QString& message, const QJsonObject& context) {
    log(LogLevel::FATAL, message, context);
}

void StructuredLogger::recordMetric(const QString& name, double value, const QJsonObject& tags) {
    QMutexLocker lock(&metricsMutex_);
    
    QJsonObject metricEntry;
    metricEntry["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    metricEntry["type"] = "metric";
    metricEntry["name"] = name;
    metricEntry["value"] = value;
    
    if (!tags.isEmpty()) {
        metricEntry["tags"] = tags;
    }
    
    writeLogEntry(metricEntry);
    
    // Update internal metrics storage
    if (!histograms_.contains(name)) {
        histograms_[name] = QList<double>();
    }
    histograms_[name].append(value);
}

void StructuredLogger::incrementCounter(const QString& name, int value, const QJsonObject& tags) {
    QMutexLocker lock(&metricsMutex_);
    
    counters_[name] += value;
    
    QJsonObject counterEntry;
    counterEntry["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    counterEntry["type"] = "counter";
    counterEntry["name"] = name;
    counterEntry["value"] = value;
    counterEntry["total"] = counters_[name];
    
    if (!tags.isEmpty()) {
        counterEntry["tags"] = tags;
    }
    
    writeLogEntry(counterEntry);
}

void StructuredLogger::startSpan(const QString& operationName, const QString& spanId) {
    QString actualSpanId = spanId.isEmpty() ? QUuid::createUuid().toString() : spanId;
    
    QJsonObject spanStart;
    spanStart["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    spanStart["type"] = "span_start";
    spanStart["span_id"] = actualSpanId;
    spanStart["operation"] = operationName;
    
    writeLogEntry(spanStart);
    
    activeSpans_[actualSpanId] = QDateTime::currentDateTimeUtc();
    currentSpanId_ = actualSpanId;
}

void StructuredLogger::endSpan(const QString& spanId, const QJsonObject& tags) {
    if (!activeSpans_.contains(spanId)) {
        return;
    }
    
    QDateTime startTime = activeSpans_[spanId];
    qint64 durationMs = startTime.msecsTo(QDateTime::currentDateTimeUtc());
    
    QJsonObject spanEnd;
    spanEnd["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    spanEnd["type"] = "span_end";
    spanEnd["span_id"] = spanId;
    spanEnd["duration_ms"] = durationMs;
    
    if (!tags.isEmpty()) {
        spanEnd["tags"] = tags;
    }
    
    writeLogEntry(spanEnd);
    
    activeSpans_.remove(spanId);
    if (currentSpanId_ == spanId) {
        currentSpanId_.clear();
    }
}

void StructuredLogger::writeLogEntry(const QJsonObject& logEntry) {
    if (!initialized_) {
        return;
    }
    
    QMutexLocker lock(&mutex_);
    
    rotateLogIfNeeded();
    
    QJsonDocument doc(logEntry);
    QByteArray data = doc.toJson(QJsonDocument::Compact) + "\n";
    
    logFile_.write(data);
    logFile_.flush();
}

void StructuredLogger::rotateLogIfNeeded() {
    if (logFile_.size() > maxFileSize_) {
        logFile_.close();
        
        QString oldPath = logFile_.fileName();
        QString newPath = oldPath + "." + QDateTime::currentDateTimeUtc().toString("yyyyMMdd_hhmmss");
        
        QFile::rename(oldPath, newPath);
        
        if (!logFile_.open(QIODevice::WriteOnly | QIODevice::Append)) {
            qWarning() << "Failed to reopen log file after rotation";
            initialized_ = false;
        }
    }
}

QString StructuredLogger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARN: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

StructuredLogger::~StructuredLogger() {
    shutdown();
}

} // namespace RawrXD
