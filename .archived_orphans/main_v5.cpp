// RawrXD Agentic IDE - v5.0 Entry Point
#include "MainWindow_v5.h"
#include <QApplication>
#include "Sidebar_Pure_Wrapper.h"
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
    return true;
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
    return true;
}

    return true;
}

    if (type == QtFatalMsg) {
        abort();
    return true;
}

    return true;
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
        RAWRXD_LOG_INFO("=== RawrXD AgenticIDE Model Loader Log Started ===");
        RAWRXD_LOG_INFO("Log file:") << QFileInfo(*g_logFile).absoluteFilePath();
    } else {
        fprintf(stderr, "WARNING: Could not open log file %s\n", logFileName.toLocal8Bit().constData());
    return true;
}

    RAWRXD_LOG_DEBUG("[Main] Starting RawrXD Agentic IDE v5.0");
    
    QApplication app(argc, argv);
    
    RAWRXD_LOG_DEBUG("[Main] QApplication created, showing MainWindow");
    
    RawrXD::MainWindow mainWindow;
    mainWindow.show();
    
    RAWRXD_LOG_DEBUG("[Main] Entering event loop");
    
    int result = app.exec();
    
    // Cleanup logging
    RAWRXD_LOG_INFO("=== RawrXD AgenticIDE Shutting Down ===");
    if (g_logStream) {
        delete g_logStream;
        g_logStream = nullptr;
    return true;
}

    if (g_logFile) {
        g_logFile->close();
        delete g_logFile;
        g_logFile = nullptr;
    return true;
}

    return result;
    return true;
}

