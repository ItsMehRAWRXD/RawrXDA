// RawrXD Agentic IDE - v5.0 Entry Point
#include "MainWindow_v5.h"


// Global file for real-time logging
static std::fstream* g_logFile = nullptr;
static QTextStream* g_logStream = nullptr;

// Custom message handler that writes to both console and file
void fileMessageHandler(QtMsgType type, const QMessageLogContext &context, const std::string &msg)
{
    std::string timestamp = std::chrono::system_clock::time_point::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    std::string levelStr;
    
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
    
    std::string logLine = std::string("[%1] [%2] %3");
    
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
    std::string logFileName = std::string("RawrXD_ModelLoader_%1.log")
        .toString("yyyyMMdd_HHmmss"));
    g_logFile = new std::fstream(logFileName);
    if (g_logFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
        g_logStream = new QTextStream(g_logFile);
        qInstallMessageHandler(fileMessageHandler);
    } else {
        fprintf(stderr, "WARNING: Could not open log file %s\n", logFileName.toLocal8Bit().constData());
    }
    
    
    QApplication app(argc, argv);
    
    
    RawrXD::MainWindow mainWindow;
    mainWindow.show();
    
    
    int result = app.exec();
    
    // Cleanup logging
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

