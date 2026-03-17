// RawrXD IDE - C++ Migration from PowerShell
#include <QApplication>
#include <QMessageBox>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QLibraryInfo>
#include <QStringList>
#include <QCommandLineParser>
#include <exception>
#include <csignal>
#include <QTimer>
#include <iostream>
#include <memory>

#if defined(_WIN32)
#include <windows.h>
#endif

#include "MainWindow.h"
#include "../ide_command_server.h"
#include "rawrxd_build_info.h"
#include "startup_readiness_checker.hpp"
#include "safe_mode_config.hpp"

namespace {
QString g_logPath;
std::unique_ptr<IDECommandServer> g_commandServer;

QString getExecutableDirPath() {
#if defined(_WIN32)
    wchar_t buf[MAX_PATH] = {0};
    const DWORD len = GetModuleFileNameW(nullptr, buf, static_cast<DWORD>(std::size(buf)));
    if (len > 0) {
        const QString exePath = QString::fromWCharArray(buf, static_cast<int>(len));
        const QFileInfo fi(exePath);
        return fi.absolutePath();
    }
#endif
    return QCoreApplication::applicationDirPath();
}

bool trySetProcessCwdToExeDir(QString* outDir) {
    const QString exeDir = getExecutableDirPath();
    if (outDir) {
        *outDir = exeDir;
    }
#if defined(_WIN32)
    const std::wstring wdir = exeDir.toStdWString();
    if (!wdir.empty() && SetCurrentDirectoryW(wdir.c_str()) != 0) {
        return true;
    }
    return false;
#else
    QDir::setCurrent(exeDir);
    return (QDir::currentPath() == exeDir);
#endif
}

QString buildRunLogPath() {
    const QString exeDir = getExecutableDirPath();
    const QString runlogsDir = QDir(exeDir).filePath(QStringLiteral("runlogs"));
    QDir().mkpath(runlogsDir);

    const qint64 pid = QCoreApplication::applicationPid();
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    return QDir(runlogsDir).filePath(QStringLiteral("RawrXD-QtShell_%1_pid%2.log").arg(stamp).arg(pid));
}

void appendLifecycleLog(const QString& line) {
    const QString path = g_logPath.isEmpty() ? QStringLiteral("terminal_diagnostics.log") : g_logPath;
    QFile f(path);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz"))
           << " [pid=" << QCoreApplication::applicationPid() << "] "
           << line << "\n";
    }
}

void fileMessageHandler(QtMsgType type, const QMessageLogContext& /*ctx*/, const QString& msg) {
    const char* typeStr = "LOG";
    switch (type) {
    case QtDebugMsg: typeStr = "DEBUG"; break;
    case QtInfoMsg: typeStr = "INFO"; break;
    case QtWarningMsg: typeStr = "WARN"; break;
    case QtCriticalMsg: typeStr = "CRIT"; break;
    case QtFatalMsg: typeStr = "FATAL"; break;
    }
    appendLifecycleLog(QString("[QtLog][%1] %2").arg(typeStr).arg(msg));
    if (type == QtFatalMsg) {
        abort();
    }
}

#if defined(_WIN32)
LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS* ep) {
    const DWORD code = ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0;
    const void* addr = ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionAddress : nullptr;
    appendLifecycleLog(QString("[APP] Unhandled SEH exception code=0x%1 addr=%2")
        .arg(QString::number(static_cast<qulonglong>(code), 16))
        .arg(reinterpret_cast<qulonglong>(addr), 0, 16));
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

void signalHandler(int sig) {
    appendLifecycleLog(QString("[APP] signal %1 received").arg(sig));
    std::abort();
}

/**
 * @brief Safe Mode CLI entry point
 * 
 * Runs diagnostic checks and outputs results to console without launching the full IDE.
 */
int runSafeModeCLI(QApplication& app, const SafeMode::Config& config) {
    std::cout << "\n=== RawrXD-QtShell Safe Mode CLI ===\n\n";
    std::cout << "Mode: " << config.appModeString().toStdString() << "\n";
    std::cout << "Verbose: " << (config.verboseDiagnostics() ? "Yes" : "No") << "\n";
    std::cout << "Recovery Mode: " << (config.enableRecoveryMode() ? "Yes" : "No") << "\n";
    std::cout << "Skip Startup Checks: " << (config.skipStartupChecks() ? "Yes" : "No") << "\n\n";
    
    if (!config.skipStartupChecks()) {
        std::cout << "Running startup checks...\n";
        StartupReadinessChecker checker;
        
        bool checkComplete = false;
        AgentReadinessReport finalReport;
        
        QObject::connect(&checker, &StartupReadinessChecker::readinessComplete,
                         &app, [&](const AgentReadinessReport& report) {
            finalReport = report;
            checkComplete = true;
            app.quit();
        });
        
        QTimer::singleShot(0, [&checker]() {
            checker.runChecks();
        });
        
        app.exec();
        
        if (checkComplete) {
            std::cout << "\nStartup Check Results:\n";
            std::cout << "  Overall Ready: " << (finalReport.overallReady ? "Yes" : "No") << "\n";
            std::cout << "  Failed Subsystems: " << finalReport.failures.size() << "\n";
            
            for (const QString& failure : finalReport.failures) {
                std::cout << "    - " << failure.toStdString() << "\n";
            }
        }
    }
    
    std::cout << "\nEnabled Features:\n";
    QStringList features = config.featureFlagsString();
    for (const QString& feature : features) {
        std::cout << "  - " << feature.toStdString() << "\n";
    }
    
    std::cout << "\nSafe mode CLI complete.\n\n";
    return 0;
}
}

int main(int argc, char* argv[])
{
    std::cout << "main() entry" << std::endl;
    try {
        g_logPath = buildRunLogPath();

        QString exeDir;
        const bool cwdOk = trySetProcessCwdToExeDir(&exeDir);
        appendLifecycleLog(QString("[APP] Startup: exeDir='%1' cwd='%2' cwdSetOk=%3")
            .arg(exeDir)
            .arg(QDir::currentPath())
            .arg(cwdOk));
        {
            QStringList args;
            args.reserve(argc);
            for (int i = 0; i < argc; ++i) {
                args.push_back(QString::fromLocal8Bit(argv[i] ? argv[i] : ""));
            }
            appendLifecycleLog(QString("[APP] Args: %1").arg(args.join(' ')));
        }

        qInstallMessageHandler(fileMessageHandler);
        std::set_terminate([]() {
            appendLifecycleLog("[APP] std::terminate invoked");
            std::abort();
        });
        std::signal(SIGABRT, signalHandler);
        std::signal(SIGTERM, signalHandler);
    #if defined(SIGSEGV)
        std::signal(SIGSEGV, signalHandler);
    #endif

#if defined(_WIN32)
        SetUnhandledExceptionFilter(unhandledExceptionFilter);
#endif

        appendLifecycleLog("[APP] main() start");

        QElapsedTimer t;
        t.start();
        QApplication app(argc, argv);
        appendLifecycleLog(QString("[APP] QApplication constructed in %1 ms").arg(t.elapsed()));

        QCoreApplication::setApplicationVersion(QStringLiteral(RAWRXD_APP_VERSION));
        QCoreApplication::setApplicationName(QStringLiteral("RawrXD Agentic IDE"));

        appendLifecycleLog(QString("[APP] Qt version=%1; appDir=%2")
            .arg(QString::fromLatin1(qVersion()))
            .arg(QCoreApplication::applicationDirPath()));
        appendLifecycleLog(QString("[APP] Qt plugin root (QLibraryInfo::PluginsPath)=%1")
            .arg(QLibraryInfo::path(QLibraryInfo::PluginsPath)));
        appendLifecycleLog(QString("[APP] QCoreApplication::libraryPaths=%1")
            .arg(QCoreApplication::libraryPaths().join(';')));

        appendLifecycleLog(QString("[APP] Env: QT_DEBUG_PLUGINS=%1")
            .arg(QString::fromLocal8Bit(qgetenv("QT_DEBUG_PLUGINS"))));
        appendLifecycleLog(QString("[APP] Env: QT_PLUGIN_PATH=%1")
            .arg(QString::fromLocal8Bit(qgetenv("QT_PLUGIN_PATH"))));
        appendLifecycleLog(QString("[APP] Env: QT_QPA_PLATFORM_PLUGIN_PATH=%1")
            .arg(QString::fromLocal8Bit(qgetenv("QT_QPA_PLATFORM_PLUGIN_PATH"))));

        // Initialize safe mode configuration from command-line arguments
        QStringList cliArgs;
        for (int i = 1; i < argc; ++i) {
            cliArgs << QString::fromLocal8Bit(argv[i]);
        }
        SafeMode::Config::initialize(cliArgs);
        
        const SafeMode::Config& safeModeConfig = SafeMode::Config::instance();
        appendLifecycleLog(QString("[APP] SafeMode: %1").arg(safeModeConfig.appModeString()));
        
        // Handle safe mode CLI mode
        if (safeModeConfig.isSafeModeCLI()) {
            appendLifecycleLog("[APP] Entering Safe Mode CLI");
            return runSafeModeCLI(app, safeModeConfig);
        }

        // Headless readiness mode: run startup checks without launching the full UI
        const bool headlessReadiness = qEnvironmentVariableIsSet("RAWRXD_HEADLESS_READINESS");
        if (headlessReadiness) {
            appendLifecycleLog("[APP] Headless readiness mode enabled (RAWRXD_HEADLESS_READINESS)");

            StartupReadinessChecker checker;

            QObject::connect(&checker, &StartupReadinessChecker::readinessComplete,
                             &app, [&](const AgentReadinessReport& report) {
                appendLifecycleLog(QString("[APP] Headless readiness complete ready=%1 failures=%2")
                    .arg(report.overallReady)
                    .arg(report.failures.size()));
                if (!report.failures.isEmpty()) {
                    appendLifecycleLog(QString("[APP] Failed subsystems: %1")
                        .arg(report.failures.join(", ")));
                }
                app.quit();
            });

            QTimer::singleShot(0, [&checker]() {
                checker.runChecks();
            });

            appendLifecycleLog("[APP] Entering event loop (headless readiness)");
            const int rc = app.exec();
            appendLifecycleLog(QString("[APP] app.exec() returned %1 (headless readiness)").arg(rc));
            return rc;
        }
        qDebug() << "Starting RawrXD-QtShell...";
        appendLifecycleLog("[APP] Starting RawrXD-QtShell...");
        appendLifecycleLog(QString("[APP] Build info: version=%1 commit=%2 config=%3 compiler=%4")
            .arg(QStringLiteral(RAWRXD_APP_VERSION))
            .arg(QStringLiteral(RAWRXD_BUILD_COMMIT))
            .arg(QStringLiteral(RAWRXD_BUILD_CONFIG_STR))
            .arg(QStringLiteral(RAWRXD_BUILD_COMPILER)));

        // Disable auto-update during initial testing
        // AutoUpdate updater;
        // updater.checkAndInstall();

        appendLifecycleLog("[APP] Creating MainWindow...");
        qDebug() << "Creating MainWindow...";

        t.restart();
        MainWindow window;
        appendLifecycleLog(QString("[APP] MainWindow constructed in %1 ms").arg(t.elapsed()));

        qDebug() << "Initializing MainWindow...";
        appendLifecycleLog("[APP] Initializing MainWindow");
        
        t.restart();
        window.initialize();
        appendLifecycleLog(QString("[APP] MainWindow initialized in %1 ms").arg(t.elapsed()));

        // Start IDE command server for CLI control
        g_commandServer = std::make_unique<IDECommandServer>(&window, &window);
        if (g_commandServer->startServer("RawrXD_IDE_Server")) {
            appendLifecycleLog("[APP] IDECommandServer started (RawrXD_IDE_Server)");
            qDebug() << "IDECommandServer started";
        } else {
            appendLifecycleLog("[APP] IDECommandServer failed to start");
            qWarning() << "IDECommandServer failed to start";
        }

        QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
            if (g_commandServer) {
                g_commandServer->stopServer();
            }
        });

        qDebug() << "Showing window...";
        appendLifecycleLog("[APP] Showing MainWindow");

        t.restart();
        window.show();
        appendLifecycleLog(QString("[APP] MainWindow show() called in %1 ms").arg(t.elapsed()));

        // Fail-safe timer to detect abrupt exit: log heartbeat every 2s
        QTimer heartbeat;
        QObject::connect(&heartbeat, &QTimer::timeout, []() {
            appendLifecycleLog("[APP] heartbeat");
        });
        heartbeat.start(2000);

        QObject::connect(&app, &QGuiApplication::lastWindowClosed, []() {
            appendLifecycleLog("[APP] lastWindowClosed");
        });
        QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
            appendLifecycleLog("[APP] aboutToQuit (main)");
        });

        appendLifecycleLog("[APP] Entering event loop");
        qDebug() << "Entering event loop...";
        const int rc = app.exec();
        appendLifecycleLog(QString("[APP] app.exec() returned %1").arg(rc));
        g_commandServer.reset();
        return rc;
    }
    catch (const std::exception& e) {
        appendLifecycleLog(QString("[APP] startup exception: %1").arg(e.what()));
        QFile errorLog("startup_crash.txt");
        if (errorLog.open(QIODevice::WriteOnly | QIODevice::Text)) {
            errorLog.write(e.what());
            errorLog.close();
        }
        return -1;
    }
}
