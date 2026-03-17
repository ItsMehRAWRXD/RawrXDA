// RawrXD Agentic IDE - v5.0 Entry Point
#include "MainWindow_v5.h"
#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QFileInfo>

// Global file for real-time logging
static QFile* g_logFile = nullptr;
static QTextStream* g_logStream = nullptr;

// Custom message handler that writes to both console and file
void fileMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString levelStr;
    
    switch (type) {
    case QtDebugMsg:
        levelStr = "DEBUG";
        break;
    case QtInfoMsg:
        levelStr = "INFO ";
        break;
    case QtWarningMsg:
        levelStr = "WARN ";
        break;
    case QtCriticalMsg:
        levelStr = "CRIT ";
        break;
    case QtFatalMsg:
        levelStr = "FATAL";
        break;
    }
    
    QString logLine = QString("[%1] [%2] %3").arg(timestamp, levelStr, msg);
    
    // Write to console with immediate flush
    fprintf(stderr, "%s\n", logLine.toLocal8Bit().constData());
    fflush(stderr);
    
    // Write to file with immediate flush
    if (g_logStream) {
        (*g_logStream) << logLine << "\n";
        g_logStream->flush();
        
        // Also flush the underlying file descriptor for extra safety
        if (g_logFile && g_logFile->isOpen()) {
            g_logFile->flush();
        }
    }
    
    if (type == QtFatalMsg) {
        abort();
    }
}

int main(int argc, char *argv[])
{
    // Setup file logging BEFORE anything else
    QString logFileName = QString("RawrXD_ModelLoader_%1.log")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    g_logFile = new QFile(logFileName);
    if (g_logFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
        g_logStream = new QTextStream(g_logFile);
        qInstallMessageHandler(fileMessageHandler);
        qInfo() << "=== RawrXD AgenticIDE Model Loader Log Started ===";
        qInfo() << "Log file:" << QFileInfo(*g_logFile).absoluteFilePath();
    } else {
        fprintf(stderr, "WARNING: Could not open log file %s\n", logFileName.toLocal8Bit().constData());
    }
    
    qDebug() << "[Main] Starting RawrXD Agentic IDE v5.0";
    
    QApplication app(argc, argv);
    
    qDebug() << "[Main] QApplication created, showing MainWindow";
    
    RawrXD::MainWindow mainWindow;
    mainWindow.show();
    
    qDebug() << "[Main] Entering event loop";
    
    int result = app.exec();
    
    // Cleanup logging
    qInfo() << "=== RawrXD AgenticIDE Shutting Down ===";
    if (g_logStream) {
        delete g_logStream;
        g_logStream = nullptr;
    }
    if (g_logFile) {
        g_logFile->close();
        delete g_logFile;
        g_logFile = nullptr;
    }
    
    return result;
}
