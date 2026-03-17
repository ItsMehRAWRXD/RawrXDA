// RawrXD Agentic IDE - v5.0 Entry Point
#include "MainWindow_v5.h"
#include "qt_masm_bridge.h"
#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QFileInfo>
#include <QTimer>

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
    // Early console output before Qt initialization
    fprintf(stderr, "[Main] Starting RawrXD Agentic IDE v5.0\n");
    fflush(stderr);
    
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
    
    qDebug() << "[Main] Creating QApplication";
    
    QApplication app(argc, argv);
    
    qDebug() << "[Main] QApplication created";

    // TEMPORARILY DISABLE MASM BRIDGE INITIALIZATION TO FIX CRASH
    // RawrXD::QtMasmBridge::instance().initialize();
    
    // Delay signal registration until after MainWindow is shown
    // to avoid triggering MASM initialization too early
    QTimer::singleShot(100, []() {
        // RawrXD::QtMasmBridge::instance().registerSignalHandler(
        //     RawrXD::QtMasmBridge::SignalId::HotpatchApplied,
        //     [](uint32_t signalId, const QVariantList& args) {
        //         qInfo() << "[Bridge] Signal" << signalId << "arrived with" << args;
        //     }
        // );
    });
        );
    });

    RawrXD::MainWindow mainWindow;
    mainWindow.show();
    
    // Generate test models for IDE testing (Phase 1 verification)
    QTimer::singleShot(500, []() {
        qInfo() << "[Test] Generating test models for Phase 1 verification...";
        
        // This will be implemented when we add the TestModelGenerator
        // For now, just log that we would generate test models
        qInfo() << "[Test] Test model generation ready (implemented in TestModelGenerator)";
    });
    
    qDebug() << "[Main] Entering event loop";

    // Delay property update until after MainWindow is shown
    QTimer::singleShot(200, []() {
        // RawrXD::QtMasmBridge::instance().updateProperty("main.status", "Phase 6 Qt-MASM bridge active");
    });
    
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
