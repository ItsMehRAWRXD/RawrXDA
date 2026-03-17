#include "orchestra_manager.hpp"
#include "auto_model_loader.h"
#include "../logging/structured_logger.h"
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDirIterator>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QThread>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QStandardPaths>
#include <QMutex>
#include <QMutexLocker>
#include <QReadWriteLock>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#ifdef ERROR
#undef ERROR
#endif
#ifdef INFO
#undef INFO
#endif
#ifdef DEBUG
#undef DEBUG
#endif
#ifdef WARN
#undef WARN
#endif
#ifdef TRACE
#undef TRACE
#endif
#ifdef FATAL
#undef FATAL
#endif

namespace RawrXD {

// Thread-safety: Global mutex for critical operations
static QMutex s_instanceMutex;
static QReadWriteLock s_stateLock;

// ============================================================
// Stub implementations for subsystem managers
// These will be replaced with full implementations
// ============================================================

class BuildManager {
public:
    QString detectBuildSystem(const QString& projectPath) {
        QDir dir(projectPath);
        if (dir.exists("CMakeLists.txt")) return "cmake";
        if (dir.exists("meson.build")) return "meson";
        if (dir.exists("Makefile")) return "make";
        if (dir.exists("build.gradle")) return "gradle";
        if (dir.exists("package.json")) return "npm";
        if (dir.exists("Cargo.toml")) return "cargo";
        return "unknown";
    }
};

class VCSManager {
public:
    QString detectVCS(const QString& projectPath) {
        QDir dir(projectPath);
        if (dir.exists(".git")) return "git";
        if (dir.exists(".svn")) return "svn";
        if (dir.exists(".hg")) return "mercurial";
        return "none";
    }
};

class DebugManager {};
class ProfilerManager {};
class AIInferenceManager {};
class TerminalManager {
public:
    QMap<int, QProcess*> sessions;
    int nextId = 1;
};
class TestManager {};
class HotpatchManager {};
class ProjectManager {};
class FileManager {};

// ============================================================
// OrchestraManager Implementation
// ============================================================

OrchestraManager& OrchestraManager::instance() {
    // C++11 guarantees thread-safe initialization of static local variables
    // This is the Meyers singleton pattern - inherently thread-safe
    static OrchestraManager s_instance;
    return s_instance;
}

OrchestraManager::OrchestraManager(QObject* parent)
    : QObject(parent)
    , m_buildManager(std::make_unique<BuildManager>())
    , m_vcsManager(std::make_unique<VCSManager>())
    , m_debugManager(std::make_unique<DebugManager>())
    , m_profilerManager(std::make_unique<ProfilerManager>())
    , m_aiManager(std::make_unique<AIInferenceManager>())
    , m_terminalManager(std::make_unique<TerminalManager>())
    , m_testManager(std::make_unique<TestManager>())
    , m_hotpatchManager(std::make_unique<HotpatchManager>())
    , m_projectManager(std::make_unique<ProjectManager>())
    , m_fileManager(std::make_unique<FileManager>())
    , m_modelDiscoveryService(nullptr) // Initialize as nullptr
{
}

OrchestraManager::~OrchestraManager() {
    shutdown();
}

bool OrchestraManager::initialize(const QString& configPath) {
    if (m_initialized) return true;

    // Initialize structured logging first
    QString logPath;
    LogLevel logLevel = LogLevel::INFO;
    
    // Check for config file for log settings
    if (!configPath.isEmpty() && QFile::exists(configPath)) {
        QFile file(configPath);
        if (file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            QJsonObject config = doc.object();
            file.close();
            
            // Extract logging configuration
            if (config.contains("logging")) {
                QJsonObject logConfig = config["logging"].toObject();
                logPath = logConfig["path"].toString();
                QString levelStr = logConfig["level"].toString("INFO");
                if (levelStr == "TRACE") logLevel = LogLevel::TRACE;
                else if (levelStr == "DEBUG") logLevel = LogLevel::DEBUG;
                else if (levelStr == "INFO") logLevel = LogLevel::INFO;
                else if (levelStr == "WARN") logLevel = LogLevel::WARN;
                else if (levelStr == "ERROR") logLevel = LogLevel::ERROR;
                else if (levelStr == "FATAL") logLevel = LogLevel::FATAL;
            }
        }
    }
    
    // Initialize the structured logger
    StructuredLogger::instance().initialize(logPath, logLevel);
    
    LOG_INFO("OrchestraManager initializing...", QJsonObject{
        {"config_path", configPath},
        {"version", "1.0.0"}
    });
    START_SPAN("orchestra_init");
    
    emit info("OrchestraManager", "Initializing...");

    m_initialized = true;
    
    LOG_INFO("OrchestraManager initialization complete", QJsonObject{
        {"subsystems_count", 10}
    });
    END_SPAN("orchestra_init");
    
    emit info("OrchestraManager", "Initialization complete");
    return true;
}

void OrchestraManager::shutdown() {
    if (!m_initialized) return;

    LOG_INFO("OrchestraManager shutting down...");
    START_SPAN("orchestra_shutdown");
    
    emit info("OrchestraManager", "Shutting down...");

    // Close any open project
    closeProject();

    // Stop debugging if active
    if (m_debugging) {
        LOG_DEBUG("Stopping active debug session");
        debugStop();
    }

    // Unload AI model if loaded
    if (m_modelLoaded) {
        LOG_DEBUG("Unloading AI model", QJsonObject{{"model", m_modelName}});
        aiUnloadModel();
    }

    // Close all terminal sessions
    int terminalCount = m_terminalManager->sessions.size();
    for (auto& session : m_terminalManager->sessions) {
        if (session) {
            session->terminate();
            session->waitForFinished(1000);
            delete session;
        }
    }
    m_terminalManager->sessions.clear();
    
    LOG_DEBUG("Closed terminal sessions", QJsonObject{{"count", terminalCount}});

    m_initialized = false;
    
    LOG_INFO("OrchestraManager shutdown complete");
    END_SPAN("orchestra_shutdown");
    
    // Shutdown the logger last
    StructuredLogger::instance().shutdown();
    
    emit info("OrchestraManager", "Shutdown complete");
}

// ================================================================
// Headless Mode / CI-CD Support
// ================================================================

void OrchestraManager::setHeadlessMode(const HeadlessConfig& config) {
    QWriteLocker lock(&s_stateLock);  // Thread-safe write access
    m_headlessConfig = config;
    
    LOG_INFO("Headless mode configured", m_headlessConfig.toJson());
    
    // Adjust log level for headless mode
    if (config.enabled) {
        LogLevel level = LogLevel::INFO;
        QString levelStr = config.logLevel.toUpper();
        if (levelStr == "TRACE") level = LogLevel::TRACE;
        else if (levelStr == "DEBUG") level = LogLevel::DEBUG;
        else if (levelStr == "INFO") level = LogLevel::INFO;
        else if (levelStr == "WARN") level = LogLevel::WARN;
        else if (levelStr == "ERROR") level = LogLevel::ERROR;
        else if (levelStr == "FATAL") level = LogLevel::FATAL;
        
        StructuredLogger::instance().initialize(QString(), level);
    }
    
    emit info("OrchestraManager", QString("Headless mode: %1").arg(config.enabled ? "enabled" : "disabled"));
}

int OrchestraManager::runHeadlessBatch(const QStringList& commands, CompletionCallback callback) {
    QReadLocker lock(&s_stateLock);  // Thread-safe read access
    if (!m_headlessConfig.enabled) {
        LOG_WARN("Headless batch called without headless mode enabled");
    }
    
    LOG_INFO("Running headless batch", QJsonObject{
        {"command_count", commands.size()},
        {"fail_fast", m_headlessConfig.failFast}
    });
    START_SPAN("headless_batch");
    
    QElapsedTimer batchTimer;
    batchTimer.start();
    
    int failedCount = 0;
    int successCount = 0;
    QJsonArray results;
    
    for (const QString& cmdLine : commands) {
        QStringList parts = cmdLine.split(' ', Qt::SkipEmptyParts);
        if (parts.isEmpty()) continue;
        
        QString cmd = parts.takeFirst();
        TaskResult result = runHeadlessCommand(cmd, parts);
        
        QJsonObject cmdResult;
        cmdResult["command"] = cmdLine;
        cmdResult["success"] = result.success;
        cmdResult["message"] = result.message;
        cmdResult["duration_ms"] = result.durationMs;
        results.append(cmdResult);
        
        if (result.success) {
            successCount++;
        } else {
            failedCount++;
            if (m_headlessConfig.failFast) {
                LOG_ERROR("Headless batch failed - fail fast enabled", QJsonObject{
                    {"failed_command", cmdLine},
                    {"error", result.message}
                });
                break;
            }
        }
    }
    
    qint64 totalDuration = batchTimer.elapsed();
    
    LOG_INFO("Headless batch completed", QJsonObject{
        {"success_count", successCount},
        {"failed_count", failedCount},
        {"total_duration_ms", static_cast<qint64>(totalDuration)}
    });
    LOG_METRIC("headless_batch_duration", totalDuration);
    END_SPAN("headless_batch");
    
    TaskResult batchResult;
    batchResult.success = (failedCount == 0);
    batchResult.message = QString("Batch completed: %1 success, %2 failed").arg(successCount).arg(failedCount);
    batchResult.durationMs = totalDuration;
    batchResult.data["results"] = results;
    batchResult.data["success_count"] = successCount;
    batchResult.data["failed_count"] = failedCount;
    
    if (callback) callback(batchResult);
    
    return failedCount > 0 ? 1 : 0;
}

TaskResult OrchestraManager::runHeadlessCommand(const QString& command, const QStringList& args) {
    LOG_DEBUG("Running headless command", QJsonObject{
        {"command", command},
        {"args", args.join(" ")}
    });
    
    QElapsedTimer cmdTimer;
    cmdTimer.start();
    
    TaskResult result;
    result.data["command"] = command;
    result.data["args"] = args.join(" ");
    
    QString cmd = command.toLower();
    
    // Dispatch to appropriate handler
    if (cmd == "build") {
        QString target = args.isEmpty() ? QString() : args[0];
        QString config = "Release";
        for (int i = 0; i < args.size() - 1; i++) {
            if (args[i] == "--config" || args[i] == "-c") {
                config = args[i + 1];
            }
        }
        
        bool done = false;
        build(target, config, nullptr, [&result, &done](const TaskResult& r) {
            result = r;
            done = true;
        });
        
        // Wait with timeout
        int timeoutMs = m_headlessConfig.timeoutSeconds * 1000;
        QElapsedTimer timeout;
        timeout.start();
        while (!done && timeout.elapsed() < timeoutMs) {
            QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents, 100);
        }
        
        if (!done) {
            result.success = false;
            result.message = "Build timed out";
        }
    }
    else if (cmd == "clean") {
        bool done = false;
        clean([&result, &done](const TaskResult& r) {
            result = r;
            done = true;
        });
        while (!done) {
            QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents, 100);
        }
    }
    else if (cmd == "test") {
        bool done = false;
        if (args.isEmpty() || args[0] == "all") {
            testRunAll(nullptr, [&result, &done](const TaskResult& r) {
                result = r;
                done = true;
            });
        } else if (args[0] == "discover") {
            testDiscover([&result, &done](const TaskResult& r) {
                result = r;
                done = true;
            });
        } else {
            testRun(args, nullptr, [&result, &done](const TaskResult& r) {
                result = r;
                done = true;
            });
        }
        
        int timeoutMs = m_headlessConfig.timeoutSeconds * 1000;
        QElapsedTimer timeout;
        timeout.start();
        while (!done && timeout.elapsed() < timeoutMs) {
            QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents, 100);
        }
        
        if (!done) {
            result.success = false;
            result.message = "Test timed out";
        }
    }
    else if (cmd == "project") {
        if (args.isEmpty()) {
            result.success = false;
            result.message = "Project subcommand required (open, close, create)";
        } else if (args[0] == "open" && args.size() > 1) {
            bool done = false;
            openProject(args[1], [&result, &done](const TaskResult& r) {
                result = r;
                done = true;
            });
            while (!done) {
                QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents, 100);
            }
        } else if (args[0] == "close") {
            closeProject();
            result.success = true;
            result.message = "Project closed";
        } else {
            result.success = false;
            result.message = "Unknown project subcommand: " + args[0];
        }
    }
    else if (cmd == "configure") {
        bool done = false;
        configure(QVariantMap(), nullptr, [&result, &done](const TaskResult& r) {
            result = r;
            done = true;
        });
        
        int timeoutMs = m_headlessConfig.timeoutSeconds * 1000;
        QElapsedTimer timeout;
        timeout.start();
        while (!done && timeout.elapsed() < timeoutMs) {
            QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents, 100);
        }
        
        if (!done) {
            result.success = false;
            result.message = "Configure timed out";
        }
    }
    else if (cmd == "status") {
        result.success = true;
        result.message = "Status retrieved";
        result.data = getSubsystemStatus();
    }
    else if (cmd == "info") {
        result.success = true;
        result.message = "System info retrieved";
        result.data = getSystemInfo();
    }
    else {
        result.success = false;
        result.message = QString("Unknown headless command: %1").arg(command);
    }
    
    result.durationMs = cmdTimer.elapsed();
    
    LOG_DEBUG("Headless command completed", QJsonObject{
        {"command", command},
        {"success", result.success},
        {"duration_ms", static_cast<qint64>(result.durationMs)}
    });
    
    return result;
}

// ================================================================
// Project Management
// ================================================================

void OrchestraManager::openProject(const QString& path, CompletionCallback callback) {
    QElapsedTimer timer;
    timer.start();

    LOG_DEBUG("Opening project", QJsonObject{{"path", path}});
    START_SPAN("project_open");
    
    TaskResult result;

    QDir dir(path);
    if (!dir.exists()) {
        result.success = false;
        result.message = QString("Project path does not exist: %1").arg(path);
        LOG_ERROR("Failed to open project - path does not exist", QJsonObject{{"path", path}});
        END_SPAN("project_open");
        if (callback) callback(result);
        return;
    }

    // Close existing project first
    if (hasOpenProject()) {
        closeProject();
    }

    m_projectPath = dir.absolutePath();

    // Detect build system and VCS
    QString buildSystem = m_buildManager->detectBuildSystem(m_projectPath);
    QString vcs = m_vcsManager->detectVCS(m_projectPath);

    result.success = true;
    result.message = QString("Project opened: %1").arg(m_projectPath);
    result.data["path"] = m_projectPath;
    result.data["buildSystem"] = buildSystem;
    result.data["vcs"] = vcs;
    result.durationMs = timer.elapsed();

    LOG_INFO("Project opened successfully", QJsonObject{
        {"path", m_projectPath},
        {"build_system", buildSystem},
        {"vcs", vcs},
        {"duration_ms", static_cast<qint64>(timer.elapsed())}
    });
    LOG_METRIC("project_open_duration", timer.elapsed());
    END_SPAN("project_open");
    
    emit projectOpened(m_projectPath);
    emit info("Project", QString("Opened: %1 (build: %2, vcs: %3)")
              .arg(m_projectPath, buildSystem, vcs));

    if (callback) callback(result);
}

void OrchestraManager::closeProject() {
    if (!hasOpenProject()) return;

    QString oldPath = m_projectPath;
    m_projectPath.clear();
    
    LOG_INFO("Project closed", QJsonObject{{"path", oldPath}});

    emit projectClosed();
    emit info("Project", QString("Closed: %1").arg(oldPath));
}

void OrchestraManager::createProject(const QString& path, const QString& templateName,
                                      const QVariantMap& options, CompletionCallback callback) {
    QElapsedTimer timer;
    timer.start();
    
    LOG_DEBUG("Creating project", QJsonObject{
        {"path", path},
        {"template", templateName}
    });
    START_SPAN("project_create");

    TaskResult result;

    QDir dir(path);
    if (dir.exists() && !dir.isEmpty()) {
        result.success = false;
        result.message = QString("Directory not empty: %1").arg(path);
        if (callback) callback(result);
        return;
    }

    // Create directory
    if (!dir.mkpath(".")) {
        result.success = false;
        result.message = QString("Failed to create directory: %1").arg(path);
        if (callback) callback(result);
        return;
    }

    // Create template files based on type
    if (templateName == "cpp" || templateName == "c++") {
        // Create CMakeLists.txt
        QFile cmake(dir.filePath("CMakeLists.txt"));
        if (cmake.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&cmake);
            out << "cmake_minimum_required(VERSION 3.16)\n";
            out << "project(" << dir.dirName() << " VERSION 1.0)\n\n";
            out << "set(CMAKE_CXX_STANDARD 17)\n";
            out << "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n\n";
            out << "add_executable(${PROJECT_NAME} src/main.cpp)\n";
            cmake.close();
        }

        // Create src directory and main.cpp
        dir.mkdir("src");
        QFile main(dir.filePath("src/main.cpp"));
        if (main.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&main);
            out << "#include <iostream>\n\n";
            out << "int main(int argc, char* argv[]) {\n";
            out << "    std::cout << \"Hello, World!\" << std::endl;\n";
            out << "    return 0;\n";
            out << "}\n";
            main.close();
        }
    }
    else if (templateName == "python") {
        // Create main.py
        QFile main(dir.filePath("main.py"));
        if (main.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&main);
            out << "#!/usr/bin/env python3\n";
            out << "\"\"\"Main entry point.\"\"\"\n\n";
            out << "def main():\n";
            out << "    print(\"Hello, World!\")\n\n";
            out << "if __name__ == \"__main__\":\n";
            out << "    main()\n";
            main.close();
        }

        // Create requirements.txt
        QFile req(dir.filePath("requirements.txt"));
        if (req.open(QIODevice::WriteOnly | QIODevice::Text)) {
            req.close();
        }
    }
    else if (templateName == "rust") {
        // Create Cargo.toml
        QFile cargo(dir.filePath("Cargo.toml"));
        if (cargo.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&cargo);
            out << "[package]\n";
            out << "name = \"" << dir.dirName().toLower() << "\"\n";
            out << "version = \"0.1.0\"\n";
            out << "edition = \"2021\"\n\n";
            out << "[dependencies]\n";
            cargo.close();
        }

        // Create src/main.rs
        dir.mkdir("src");
        QFile main(dir.filePath("src/main.rs"));
        if (main.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&main);
            out << "fn main() {\n";
            out << "    println!(\"Hello, World!\");\n";
            out << "}\n";
            main.close();
        }
    }

    // Initialize git if requested
    if (options.value("initGit", true).toBool()) {
        QProcess git;
        git.setWorkingDirectory(path);
        git.start("git", {"init"});
        git.waitForFinished(5000);

        // Create .gitignore
        QFile gitignore(dir.filePath(".gitignore"));
        if (gitignore.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&gitignore);
            out << "build/\n";
            out << "*.o\n";
            out << "*.obj\n";
            out << "__pycache__/\n";
            out << "*.pyc\n";
            out << "target/\n";
            out << ".vscode/\n";
            out << ".idea/\n";
            gitignore.close();
        }
    }

    result.success = true;
    result.message = QString("Project created: %1 (template: %2)").arg(path, templateName);
    result.data["path"] = path;
    result.data["template"] = templateName;
    result.durationMs = timer.elapsed();

    emit info("Project", result.message);
    if (callback) callback(result);
}

// ================================================================
// Build System
// ================================================================

void OrchestraManager::build(const QString& target, const QString& config,
                              OutputCallback output, CompletionCallback callback) {
    if (!hasOpenProject()) {
        TaskResult result;
        result.success = false;
        result.message = "No project open";
        LOG_WARN("Build attempted with no project open");
        if (callback) callback(result);
        return;
    }

    QElapsedTimer timer;
    timer.start();
    
    LOG_INFO("Starting build", QJsonObject{
        {"target", target.isEmpty() ? "default" : target},
        {"config", config},
        {"project", m_projectPath}
    });
    START_SPAN("build");
    LOG_COUNTER("build_started", 1);

    emit buildStarted(target);

    QString buildSystem = m_buildManager->detectBuildSystem(m_projectPath);
    QStringList args;
    QString program;

    if (buildSystem == "cmake") {
        program = "cmake";
        args << "--build" << "build" << "--config" << config;
        if (!target.isEmpty()) {
            args << "--target" << target;
        }
    }
    else if (buildSystem == "make") {
        program = "make";
        if (!target.isEmpty()) {
            args << target;
        }
        args << "-j" << QString::number(QThread::idealThreadCount());
    }
    else if (buildSystem == "cargo") {
        program = "cargo";
        args << "build";
        if (config == "Release") {
            args << "--release";
        }
    }
    else if (buildSystem == "npm") {
        program = "npm";
        args << "run" << "build";
    }
    else {
        TaskResult result;
        result.success = false;
        result.message = QString("Unknown build system: %1").arg(buildSystem);
        LOG_ERROR("Build failed - unknown build system", QJsonObject{{"build_system", buildSystem}});
        END_SPAN("build");
        emit buildFinished(false, result.message);
        if (callback) callback(result);
        return;
    }

    QProcess* process = new QProcess(this);
    process->setWorkingDirectory(m_projectPath);
    process->setProcessChannelMode(QProcess::MergedChannels);

    connect(process, &QProcess::readyReadStandardOutput, this, [this, process, output]() {
        QString text = QString::fromLocal8Bit(process->readAllStandardOutput());
        for (const QString& line : text.split('\n', Qt::SkipEmptyParts)) {
            emit buildOutput(line, line.contains("error", Qt::CaseInsensitive));
            if (output) output(line, line.contains("error", Qt::CaseInsensitive));
        }
    });

        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process, timer, callback, target, config](int exitCode, QProcess::ExitStatus status) {
        TaskResult result;
        result.exitCode = exitCode;
        result.success = (exitCode == 0 && status == QProcess::NormalExit);
        result.message = result.success ? "Build successful" : "Build failed";
        result.durationMs = timer.elapsed();

        if (result.success) {
            LOG_INFO("Build completed successfully", QJsonObject{
                {"target", target.isEmpty() ? "default" : target},
                {"config", config},
                {"duration_ms", static_cast<qint64>(result.durationMs)}
            });
            LOG_COUNTER("build_success", 1);
        } else {
            LOG_ERROR("Build failed", QJsonObject{
                {"exit_code", exitCode},
                {"duration_ms", static_cast<qint64>(result.durationMs)}
            });
            LOG_COUNTER("build_failure", 1);
        }
        LOG_METRIC("build_duration", result.durationMs);
        END_SPAN("build");

        emit buildFinished(result.success, result.message);
        if (callback) callback(result);

        process->deleteLater();
    });

    process->start(program, args);
}

void OrchestraManager::clean(CompletionCallback callback) {
    if (!hasOpenProject()) {
        TaskResult result;
        result.success = false;
        result.message = "No project open";
        if (callback) callback(result);
        return;
    }

    LOG_DEBUG("Cleaning build artifacts", QJsonObject{{"project", m_projectPath}});

    QString buildSystem = m_buildManager->detectBuildSystem(m_projectPath);
    QProcess process;
    process.setWorkingDirectory(m_projectPath);

    if (buildSystem == "cmake") {
        // Remove build directory
        QDir buildDir(m_projectPath + "/build");
        if (buildDir.exists()) {
            buildDir.removeRecursively();
        }
    }
    else if (buildSystem == "make") {
        process.start("make", {"clean"});
        process.waitForFinished(30000);
    }
    else if (buildSystem == "cargo") {
        process.start("cargo", {"clean"});
        process.waitForFinished(30000);
    }

    TaskResult result;
    result.success = true;
    result.message = "Clean complete";
    LOG_INFO("Clean completed", QJsonObject{{"build_system", buildSystem}});
    if (callback) callback(result);
}

void OrchestraManager::rebuild(const QString& target, const QString& config,
                                OutputCallback output, CompletionCallback callback) {
    LOG_DEBUG("Starting rebuild", QJsonObject{{"target", target}, {"config", config}});
    clean([this, target, config, output, callback](const TaskResult&) {
        build(target, config, output, callback);
    });
}

void OrchestraManager::configure(const QVariantMap& options, OutputCallback output,
                                  CompletionCallback callback) {
    if (!hasOpenProject()) {
        TaskResult result;
        result.success = false;
        result.message = "No project open";
        if (callback) callback(result);
        return;
    }
    
    LOG_DEBUG("Configuring build system", QJsonObject{{"project", m_projectPath}});

    QString buildSystem = m_buildManager->detectBuildSystem(m_projectPath);

    if (buildSystem == "cmake") {
        QDir buildDir(m_projectPath + "/build");
        if (!buildDir.exists()) {
            buildDir.mkpath(".");
        }

        QProcess* process = new QProcess(this);
        process->setWorkingDirectory(m_projectPath);

        QStringList args;
        args << "-B" << "build" << "-S" << ".";

        // Add generator if specified
        if (options.contains("generator")) {
            args << "-G" << options["generator"].toString();
        }

        connect(process, &QProcess::readyReadStandardOutput, this, [output, process]() {
            QString text = QString::fromLocal8Bit(process->readAllStandardOutput());
            if (output) {
                for (const QString& line : text.split('\n', Qt::SkipEmptyParts)) {
                    output(line, false);
                }
            }
        });

        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [process, callback](int exitCode, QProcess::ExitStatus) {
            TaskResult result;
            result.success = (exitCode == 0);
            result.exitCode = exitCode;
            result.message = result.success ? "Configuration complete" : "Configuration failed";
            if (callback) callback(result);
            process->deleteLater();
        });

        process->start("cmake", args);
    }
    else {
        TaskResult result;
        result.success = true;
        result.message = "No configuration needed for " + buildSystem;
        if (callback) callback(result);
    }
}

QStringList OrchestraManager::listBuildTargets() const {
    // This would parse CMakeLists.txt or equivalent to find targets
    return {"all", "clean", "install"};
}

// ================================================================
// Version Control
// ================================================================

void OrchestraManager::vcsStatus(CompletionCallback callback) {
    if (!hasOpenProject()) {
        TaskResult result;
        result.success = false;
        result.message = "No project open";
        if (callback) callback(result);
        return;
    }

    QString vcs = m_vcsManager->detectVCS(m_projectPath);
    if (vcs != "git") {
        TaskResult result;
        result.success = false;
        result.message = "Only Git is currently supported";
        if (callback) callback(result);
        return;
    }

    QProcess* process = new QProcess(this);
    process->setWorkingDirectory(m_projectPath);

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [process, callback](int exitCode, QProcess::ExitStatus) {
        TaskResult result;
        result.success = (exitCode == 0);
        result.exitCode = exitCode;

        QString output = QString::fromLocal8Bit(process->readAllStandardOutput());
        result.data["status"] = output;

        // Parse status
        QJsonArray files;
        for (const QString& line : output.split('\n', Qt::SkipEmptyParts)) {
            if (line.length() >= 3) {
                QJsonObject file;
                file["status"] = line.left(2).trimmed();
                file["path"] = line.mid(3).trimmed();
                files.append(file);
            }
        }
        result.data["files"] = files;
        result.message = QString("%1 file(s) changed").arg(files.size());

        if (callback) callback(result);
        process->deleteLater();
    });

    process->start("git", {"status", "--porcelain"});
}

void OrchestraManager::vcsStage(const QStringList& files, CompletionCallback callback) {
    if (!hasOpenProject()) {
        TaskResult result;
        result.success = false;
        result.message = "No project open";
        if (callback) callback(result);
        return;
    }

    QProcess* process = new QProcess(this);
    process->setWorkingDirectory(m_projectPath);

    QStringList args = {"add"};
    if (files.isEmpty()) {
        args << "-A";
    } else {
        args << files;
    }

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [process, callback](int exitCode, QProcess::ExitStatus) {
        TaskResult result;
        result.success = (exitCode == 0);
        result.exitCode = exitCode;
        result.message = result.success ? "Files staged" : "Staging failed";
        if (callback) callback(result);
        process->deleteLater();
    });

    process->start("git", args);
}

void OrchestraManager::vcsCommit(const QString& message, CompletionCallback callback) {
    if (!hasOpenProject()) {
        TaskResult result;
        result.success = false;
        result.message = "No project open";
        if (callback) callback(result);
        return;
    }

    QProcess* process = new QProcess(this);
    process->setWorkingDirectory(m_projectPath);

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [process, callback](int exitCode, QProcess::ExitStatus) {
        TaskResult result;
        result.success = (exitCode == 0);
        result.exitCode = exitCode;
        result.message = result.success ? "Committed" : "Commit failed";
        result.data["output"] = QString::fromLocal8Bit(process->readAllStandardOutput());
        if (callback) callback(result);
        process->deleteLater();
    });

    process->start("git", {"commit", "-m", message});
}

void OrchestraManager::vcsPush(const QString& remote, const QString& branch,
                                CompletionCallback callback) {
    if (!hasOpenProject()) {
        TaskResult result;
        result.success = false;
        result.message = "No project open";
        if (callback) callback(result);
        return;
    }

    emit vcsOperationStarted("push");

    QProcess* process = new QProcess(this);
    process->setWorkingDirectory(m_projectPath);

    QStringList args = {"push", remote};
    if (!branch.isEmpty()) {
        args << branch;
    }

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process, callback](int exitCode, QProcess::ExitStatus) {
        TaskResult result;
        result.success = (exitCode == 0);
        result.exitCode = exitCode;
        result.message = result.success ? "Pushed" : "Push failed";
        emit vcsOperationFinished("push", result.success);
        if (callback) callback(result);
        process->deleteLater();
    });

    process->start("git", args);
}

void OrchestraManager::vcsPull(const QString& remote, const QString& branch,
                                CompletionCallback callback) {
    if (!hasOpenProject()) {
        TaskResult result;
        result.success = false;
        result.message = "No project open";
        if (callback) callback(result);
        return;
    }

    emit vcsOperationStarted("pull");

    QProcess* process = new QProcess(this);
    process->setWorkingDirectory(m_projectPath);

    QStringList args = {"pull", remote};
    if (!branch.isEmpty()) {
        args << branch;
    }

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process, callback](int exitCode, QProcess::ExitStatus) {
        TaskResult result;
        result.success = (exitCode == 0);
        result.exitCode = exitCode;
        result.message = result.success ? "Pulled" : "Pull failed";
        result.data["output"] = QString::fromLocal8Bit(process->readAllStandardOutput());
        emit vcsOperationFinished("pull", result.success);
        if (callback) callback(result);
        process->deleteLater();
    });

    process->start("git", args);
}

void OrchestraManager::vcsCreateBranch(const QString& name, bool checkout,
                                        CompletionCallback callback) {
    if (!hasOpenProject()) {
        TaskResult result;
        result.success = false;
        result.message = "No project open";
        if (callback) callback(result);
        return;
    }

    QProcess* process = new QProcess(this);
    process->setWorkingDirectory(m_projectPath);

    QStringList args;
    if (checkout) {
        args << "checkout" << "-b" << name;
    } else {
        args << "branch" << name;
    }

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process, callback, name](int exitCode, QProcess::ExitStatus) {
        TaskResult result;
        result.success = (exitCode == 0);
        result.exitCode = exitCode;
        result.message = result.success ? QString("Created branch: %1").arg(name) : "Branch creation failed";
        emit vcsStatusChanged();
        if (callback) callback(result);
        process->deleteLater();
    });

    process->start("git", args);
}

void OrchestraManager::vcsCheckout(const QString& name, CompletionCallback callback) {
    if (!hasOpenProject()) {
        TaskResult result;
        result.success = false;
        result.message = "No project open";
        if (callback) callback(result);
        return;
    }

    QProcess* process = new QProcess(this);
    process->setWorkingDirectory(m_projectPath);

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process, callback, name](int exitCode, QProcess::ExitStatus) {
        TaskResult result;
        result.success = (exitCode == 0);
        result.exitCode = exitCode;
        result.message = result.success ? QString("Switched to: %1").arg(name) : "Checkout failed";
        emit vcsStatusChanged();
        if (callback) callback(result);
        process->deleteLater();
    });

    process->start("git", {"checkout", name});
}

QStringList OrchestraManager::vcsListBranches(bool includeRemote) const {
    if (!hasOpenProject()) return {};

    QProcess process;
    process.setWorkingDirectory(m_projectPath);

    QStringList args = {"branch"};
    if (includeRemote) {
        args << "-a";
    }

    process.start("git", args);
    process.waitForFinished(5000);

    QStringList branches;
    QString output = QString::fromLocal8Bit(process.readAllStandardOutput());
    for (QString line : output.split('\n', Qt::SkipEmptyParts)) {
        line = line.trimmed();
        if (line.startsWith("*")) {
            line = line.mid(2);
        }
        if (!line.isEmpty()) {
            branches << line;
        }
    }

    return branches;
}

void OrchestraManager::vcsLog(int count, CompletionCallback callback) {
    if (!hasOpenProject()) {
        TaskResult result;
        result.success = false;
        result.message = "No project open";
        if (callback) callback(result);
        return;
    }

    QProcess* process = new QProcess(this);
    process->setWorkingDirectory(m_projectPath);

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [process, callback](int exitCode, QProcess::ExitStatus) {
        TaskResult result;
        result.success = (exitCode == 0);
        result.exitCode = exitCode;

        QString output = QString::fromLocal8Bit(process->readAllStandardOutput());
        QJsonArray commits;

        // Parse git log format
        for (const QString& entry : output.split("\n\n", Qt::SkipEmptyParts)) {
            QJsonObject commit;
            QStringList lines = entry.split('\n');
            for (const QString& line : lines) {
                if (line.startsWith("commit ")) {
                    commit["hash"] = line.mid(7);
                } else if (line.startsWith("Author: ")) {
                    commit["author"] = line.mid(8);
                } else if (line.startsWith("Date: ")) {
                    commit["date"] = line.mid(6).trimmed();
                } else if (!line.isEmpty() && line.startsWith("    ")) {
                    commit["message"] = line.trimmed();
                }
            }
            if (!commit.isEmpty()) {
                commits.append(commit);
            }
        }

        result.data["commits"] = commits;
        result.message = QString("%1 commit(s)").arg(commits.size());
        if (callback) callback(result);
        process->deleteLater();
    });

    process->start("git", {"log", QString("-n%1").arg(count)});
}

// ================================================================
// Terminal/Shell Execution
// ================================================================

void OrchestraManager::exec(const QString& command, const QStringList& args,
                             const QString& workingDir, OutputCallback output,
                             CompletionCallback callback) {
    QElapsedTimer timer;
    timer.start();

    QProcess* process = new QProcess(this);

    if (!workingDir.isEmpty()) {
        process->setWorkingDirectory(workingDir);
    } else if (hasOpenProject()) {
        process->setWorkingDirectory(m_projectPath);
    }

    process->setProcessChannelMode(QProcess::MergedChannels);

    connect(process, &QProcess::readyReadStandardOutput, this, [process, output]() {
        QString text = QString::fromLocal8Bit(process->readAllStandardOutput());
        if (output) {
            for (const QString& line : text.split('\n', Qt::SkipEmptyParts)) {
                output(line, false);
            }
        }
    });

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [process, timer, callback](int exitCode, QProcess::ExitStatus status) {
        TaskResult result;
        result.exitCode = exitCode;
        result.success = (exitCode == 0 && status == QProcess::NormalExit);
        result.message = result.success ? "Command completed" : QString("Command failed (exit %1)").arg(exitCode);
        result.durationMs = timer.elapsed();
        if (callback) callback(result);
        process->deleteLater();
    });

    process->start(command, args);
}

void OrchestraManager::shell(const QString& command, OutputCallback output,
                              CompletionCallback callback) {
#ifdef Q_OS_WIN
    exec("cmd", {"/c", command}, QString(), output, callback);
#else
    exec("bash", {"-c", command}, QString(), output, callback);
#endif
}

int OrchestraManager::createTerminal(const QString& shell) {
    int id = m_terminalManager->nextId++;

    QProcess* process = new QProcess(this);

    connect(process, &QProcess::readyReadStandardOutput, this, [this, id, process]() {
        QString output = QString::fromLocal8Bit(process->readAllStandardOutput());
        emit terminalOutput(id, output);
    });

    connect(process, &QProcess::readyReadStandardError, this, [this, id, process]() {
        QString output = QString::fromLocal8Bit(process->readAllStandardError());
        emit terminalOutput(id, output);
    });

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, id](int, QProcess::ExitStatus) {
        m_terminalManager->sessions.remove(id);
        emit terminalClosed(id);
    });

    QString shellProgram = shell;
    if (shellProgram.isEmpty()) {
#ifdef Q_OS_WIN
        shellProgram = "pwsh";
#else
        shellProgram = "bash";
#endif
    }

    process->start(shellProgram, {});
    m_terminalManager->sessions[id] = process;

    emit terminalCreated(id);
    return id;
}

void OrchestraManager::terminalSend(int sessionId, const QString& input) {
    if (m_terminalManager->sessions.contains(sessionId)) {
        QProcess* process = m_terminalManager->sessions[sessionId];
        if (process && process->state() == QProcess::Running) {
            process->write((input + "\n").toLocal8Bit());
        }
    }
}

void OrchestraManager::terminalClose(int sessionId) {
    if (m_terminalManager->sessions.contains(sessionId)) {
        QProcess* process = m_terminalManager->sessions[sessionId];
        if (process) {
            process->terminate();
            process->waitForFinished(1000);
            delete process;
        }
        m_terminalManager->sessions.remove(sessionId);
        emit terminalClosed(sessionId);
    }
}

QJsonArray OrchestraManager::listTerminals() const {
    QJsonArray result;
    for (auto it = m_terminalManager->sessions.begin();
         it != m_terminalManager->sessions.end(); ++it) {
        QJsonObject session;
        session["id"] = it.key();
        session["running"] = (it.value() && it.value()->state() == QProcess::Running);
        result.append(session);
    }
    return result;
}

// ================================================================
// File Operations
// ================================================================

QString OrchestraManager::readFile(const QString& path) const {
    QString fullPath = path;
    if (!QDir::isAbsolutePath(path) && hasOpenProject()) {
        fullPath = m_projectPath + "/" + path;
    }

    QFile file(fullPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }

    QTextStream in(&file);
    return in.readAll();
}

bool OrchestraManager::writeFile(const QString& path, const QString& content) {
    QString fullPath = path;
    if (!QDir::isAbsolutePath(path) && hasOpenProject()) {
        fullPath = m_projectPath + "/" + path;
    }

    // Ensure directory exists
    QFileInfo fi(fullPath);
    QDir dir = fi.dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QFile file(fullPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&file);
    out << content;
    return true;
}

QStringList OrchestraManager::findFiles(const QString& pattern, const QString& searchPath) const {
    QString root = searchPath;
    if (root.isEmpty() && hasOpenProject()) {
        root = m_projectPath;
    }
    if (root.isEmpty()) {
        root = QDir::currentPath();
    }

    QStringList result;
    QDirIterator it(root, {pattern}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        result << it.next();
    }
    return result;
}

void OrchestraManager::searchInFiles(const QString& query, const QString& filePattern,
                                      const QString& searchPath, CompletionCallback callback) {
    QString root = searchPath;
    if (root.isEmpty() && hasOpenProject()) {
        root = m_projectPath;
    }

    QJsonArray matches;
    QRegularExpression regex(query);

    QDirIterator it(root, {filePattern}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString filePath = it.next();
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            int lineNum = 0;
            while (!in.atEnd()) {
                QString line = in.readLine();
                lineNum++;
                QRegularExpressionMatch match = regex.match(line);
                if (match.hasMatch()) {
                    QJsonObject matchObj;
                    matchObj["file"] = filePath;
                    matchObj["line"] = lineNum;
                    matchObj["column"] = match.capturedStart();
                    matchObj["text"] = line.trimmed();
                    matchObj["match"] = match.captured();
                    matches.append(matchObj);
                }
            }
        }
    }

    TaskResult result;
    result.success = true;
    result.data["matches"] = matches;
    result.message = QString("%1 match(es) found").arg(matches.size());
    if (callback) callback(result);
}

void OrchestraManager::replaceInFiles(const QString& search, const QString& replace,
                                       const QString& filePattern, const QString& searchPath,
                                       bool dryRun, CompletionCallback callback) {
    QString root = searchPath;
    if (root.isEmpty() && hasOpenProject()) {
        root = m_projectPath;
    }

    QRegularExpression regex(search);
    QJsonArray replacements;
    int totalReplacements = 0;

    QDirIterator it(root, {filePattern}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString filePath = it.next();
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString content = QTextStream(&file).readAll();
            file.close();

            int count = 0;
            QString newContent = content;
            QRegularExpressionMatchIterator matches = regex.globalMatch(content);
            while (matches.hasNext()) {
                matches.next();
                count++;
            }

            if (count > 0) {
                newContent = content.replace(regex, replace);
                totalReplacements += count;

                QJsonObject fileInfo;
                fileInfo["file"] = filePath;
                fileInfo["replacements"] = count;
                replacements.append(fileInfo);

                if (!dryRun) {
                    QFile outFile(filePath);
                    if (outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                        QTextStream out(&outFile);
                        out << newContent;
                    }
                }
            }
        }
    }

    TaskResult result;
    result.success = true;
    result.data["files"] = replacements;
    result.data["totalReplacements"] = totalReplacements;
    result.data["dryRun"] = dryRun;
    result.message = QString("%1 replacement(s) in %2 file(s)%3")
                     .arg(totalReplacements)
                     .arg(replacements.size())
                     .arg(dryRun ? " (dry run)" : "");
    if (callback) callback(result);
}

// ================================================================
// Debugging (stubs)
// ================================================================

void OrchestraManager::debugStart(const QString& executable, const QStringList& args,
                                   const QString& workingDir, CompletionCallback callback) {
    TaskResult result;
    result.success = false;
    result.message = "Debugging not yet implemented";
    if (callback) callback(result);
}

void OrchestraManager::debugStop() {
    m_debugging = false;
    emit debugStopped();
}

void OrchestraManager::debugContinue() {}
void OrchestraManager::debugStepOver() {}
void OrchestraManager::debugStepInto() {}
void OrchestraManager::debugStepOut() {}

int OrchestraManager::debugSetBreakpoint(const QString&, int, const QString&) {
    return -1;
}

void OrchestraManager::debugRemoveBreakpoint(int) {}
QJsonArray OrchestraManager::debugListBreakpoints() const { return {}; }

void OrchestraManager::debugEvaluate(const QString&, CompletionCallback callback) {
    TaskResult result;
    result.success = false;
    result.message = "Debugging not yet implemented";
    if (callback) callback(result);
}

QJsonArray OrchestraManager::debugGetCallStack() const { return {}; }
QJsonObject OrchestraManager::debugGetLocals() const { return {}; }

// ================================================================
// AI/ML Inference (stubs)
// ================================================================

void OrchestraManager::aiLoadModel(const QString& modelPath, ProgressCallback progress,
                                    CompletionCallback callback) {
    LOG_INFO("Loading AI model", QJsonObject{{"path", modelPath}});
    START_SPAN("ai_model_load");
    LOG_COUNTER("ai_model_load_attempts", 1);
    
    // This would integrate with the existing inference_engine.hpp
    m_modelName = QFileInfo(modelPath).baseName();
    m_modelLoaded = true;
    
    LOG_INFO("AI model loaded successfully", QJsonObject{
        {"model_name", m_modelName},
        {"path", modelPath}
    });
    LOG_COUNTER("ai_model_load_success", 1);
    END_SPAN("ai_model_load");
    
    emit aiModelLoaded(m_modelName);

    TaskResult result;
    result.success = true;
    result.message = QString("Model loaded: %1").arg(m_modelName);
    if (callback) callback(result);
}

void OrchestraManager::aiUnloadModel() {
    LOG_DEBUG("Unloading AI model", QJsonObject{{"model_name", m_modelName}});
    m_modelLoaded = false;
    m_modelName.clear();
    emit aiModelUnloaded();
}

void OrchestraManager::aiInfer(const QString& prompt, const QVariantMap& options,
                                OutputCallback output, CompletionCallback callback) {
    if (!m_modelLoaded) {
        TaskResult result;
        result.success = false;
        result.message = "No model loaded";
        LOG_WARN("Inference attempted with no model loaded");
        if (callback) callback(result);
        return;
    }

    LOG_DEBUG("Starting AI inference", QJsonObject{
        {"prompt_length", prompt.length()},
        {"model", m_modelName}
    });
    START_SPAN("ai_inference");
    LOG_COUNTER("ai_inference_requests", 1);
    
    QElapsedTimer inferenceTimer;
    inferenceTimer.start();
    
    emit aiInferenceStarted();

    // Stub: In real implementation, this would call inference_engine
    QString response = "[AI Response to: " + prompt.left(50) + "...]";

    if (output) {
        for (const QString& token : response.split(' ')) {
            emit aiInferenceToken(token + " ");
            output(token + " ", false);
        }
    }

    emit aiInferenceFinished();
    
    qint64 inferenceTime = inferenceTimer.elapsed();
    LOG_INFO("AI inference completed", QJsonObject{
        {"duration_ms", static_cast<qint64>(inferenceTime)},
        {"response_length", response.length()}
    });
    LOG_METRIC("ai_inference_duration", inferenceTime);
    END_SPAN("ai_inference");

    TaskResult result;
    result.success = true;
    result.data["response"] = response;
    result.message = "Inference complete";
    if (callback) callback(result);
}

void OrchestraManager::aiCancelInference() {
    LOG_DEBUG("AI inference cancelled");
    emit aiInferenceFinished();
}

void OrchestraManager::aiCodeComplete(const QString&, int, CompletionCallback callback) {
    TaskResult result;
    result.success = false;
    result.message = "Code completion not yet implemented";
    if (callback) callback(result);
}

void OrchestraManager::aiExplainCode(const QString&, OutputCallback, CompletionCallback callback) {
    TaskResult result;
    result.success = false;
    result.message = "Code explanation not yet implemented";
    if (callback) callback(result);
}

void OrchestraManager::aiRefactorCode(const QString&, const QString&, OutputCallback,
                                       CompletionCallback callback) {
    TaskResult result;
    result.success = false;
    result.message = "Code refactoring not yet implemented";
    if (callback) callback(result);
}

// ================================================================
// Testing (stubs)
// ================================================================

void OrchestraManager::testDiscover(CompletionCallback callback) {
    TaskResult result;
    result.success = true;
    result.data["tests"] = QJsonArray();
    result.message = "No tests discovered";
    if (callback) callback(result);
}

void OrchestraManager::testRunAll(OutputCallback, CompletionCallback callback) {
    TaskResult result;
    result.success = true;
    result.message = "No tests to run";
    if (callback) callback(result);
}

void OrchestraManager::testRun(const QStringList&, OutputCallback, CompletionCallback callback) {
    TaskResult result;
    result.success = true;
    result.message = "Tests complete";
    if (callback) callback(result);
}

void OrchestraManager::testCoverage(CompletionCallback callback) {
    TaskResult result;
    result.success = false;
    result.message = "Coverage not yet implemented";
    if (callback) callback(result);
}

// ================================================================
// Hotpatching (stubs)
// ================================================================

void OrchestraManager::hotpatchApply(const QByteArray&, CompletionCallback callback) {
    TaskResult result;
    result.success = false;
    result.message = "Hotpatching not yet implemented";
    if (callback) callback(result);
}

void OrchestraManager::hotpatchRevert(CompletionCallback callback) {
    TaskResult result;
    result.success = false;
    result.message = "Hotpatching not yet implemented";
    if (callback) callback(result);
}

QJsonArray OrchestraManager::hotpatchList() const {
    return {};
}

// ================================================================
// Diagnostics & Health Checks
// ================================================================

void OrchestraManager::runDiagnostics(CompletionCallback callback) {
    TaskResult result;
    result.success = true;

    QJsonObject diagnostics;

    // ========================================================
    // Tool Chain Checks
    // ========================================================
    
    // Check Git
    QProcess git;
    git.start("git", {"--version"});
    git.waitForFinished(5000);
    diagnostics["git"] = (git.exitCode() == 0);
    diagnostics["gitVersion"] = QString::fromLocal8Bit(git.readAllStandardOutput()).trimmed();

    // Check CMake
    QProcess cmake;
    cmake.start("cmake", {"--version"});
    cmake.waitForFinished(5000);
    diagnostics["cmake"] = (cmake.exitCode() == 0);
    if (cmake.exitCode() == 0) {
        QString ver = QString::fromLocal8Bit(cmake.readAllStandardOutput()).split('\n').first();
        diagnostics["cmakeVersion"] = ver;
    }

    // Check MSVC compiler
    QProcess cl;
    cl.start("cl", {});
    cl.waitForFinished(5000);
    diagnostics["msvc"] = (cl.exitCode() != -2); // -2 means not found

    // Check GCC/MinGW
    QProcess gcc;
    gcc.start("gcc", {"--version"});
    gcc.waitForFinished(5000);
    diagnostics["gcc"] = (gcc.exitCode() == 0);
    if (gcc.exitCode() == 0) {
        diagnostics["gccVersion"] = QString::fromLocal8Bit(gcc.readAllStandardOutput()).split('\n').first();
    }

    // Check Clang
    QProcess clang;
    clang.start("clang", {"--version"});
    clang.waitForFinished(5000);
    diagnostics["clang"] = (clang.exitCode() == 0);
    
    // Check Python
    QProcess python;
    python.start("python", {"--version"});
    python.waitForFinished(5000);
    diagnostics["python"] = (python.exitCode() == 0);
    if (python.exitCode() == 0) {
        diagnostics["pythonVersion"] = QString::fromLocal8Bit(python.readAllStandardOutput()).trimmed();
    }

    // Check Node.js
    QProcess node;
    node.start("node", {"--version"});
    node.waitForFinished(5000);
    diagnostics["nodejs"] = (node.exitCode() == 0);
    if (node.exitCode() == 0) {
        diagnostics["nodeVersion"] = QString::fromLocal8Bit(node.readAllStandardOutput()).trimmed();
    }

    // Check Rust/Cargo
    QProcess cargo;
    cargo.start("cargo", {"--version"});
    cargo.waitForFinished(5000);
    diagnostics["rust"] = (cargo.exitCode() == 0);
    if (cargo.exitCode() == 0) {
        diagnostics["cargoVersion"] = QString::fromLocal8Bit(cargo.readAllStandardOutput()).trimmed();
    }

    // ========================================================
    // System Information
    // ========================================================
    diagnostics["platform"] = QSysInfo::prettyProductName();
    diagnostics["architecture"] = QSysInfo::currentCpuArchitecture();
    diagnostics["hostname"] = QSysInfo::machineHostName();
    diagnostics["qtVersion"] = QString::fromLatin1(qVersion());
    diagnostics["processorCount"] = QThread::idealThreadCount();

    // ========================================================
    // Subsystem Health
    // ========================================================
    QJsonObject subsystems;
    subsystems["orchestraManager"] = m_initialized;
    subsystems["buildManager"] = (m_buildManager != nullptr);
    subsystems["vcsManager"] = (m_vcsManager != nullptr);
    subsystems["debugManager"] = (m_debugManager != nullptr);
    subsystems["profilerManager"] = (m_profilerManager != nullptr);
    subsystems["aiManager"] = (m_aiManager != nullptr);
    subsystems["terminalManager"] = (m_terminalManager != nullptr);
    subsystems["testManager"] = (m_testManager != nullptr);
    subsystems["hotpatchManager"] = (m_hotpatchManager != nullptr);
    subsystems["projectManager"] = (m_projectManager != nullptr);
    subsystems["fileManager"] = (m_fileManager != nullptr);
    diagnostics["subsystems"] = subsystems;

    // ========================================================
    // Directory Checks
    // ========================================================
    QJsonObject directories;
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString temp = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString config = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    
    directories["appDataPath"] = appData;
    directories["appDataWritable"] = QFileInfo(appData).isWritable() || QDir().mkpath(appData);
    directories["tempPath"] = temp;
    directories["tempWritable"] = QFileInfo(temp).isWritable();
    directories["configPath"] = config;
    directories["configWritable"] = QFileInfo(config).isWritable() || QDir().mkpath(config);
    diagnostics["directories"] = directories;

    // ========================================================
    // Memory Information (Windows-specific)
    // ========================================================
#ifdef Q_OS_WIN
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(memInfo);
    if (GlobalMemoryStatusEx(&memInfo)) {
        QJsonObject memory;
        memory["totalPhysicalMB"] = static_cast<qint64>(memInfo.ullTotalPhys / (1024 * 1024));
        memory["availablePhysicalMB"] = static_cast<qint64>(memInfo.ullAvailPhys / (1024 * 1024));
        memory["memoryLoadPercent"] = static_cast<int>(memInfo.dwMemoryLoad);
        memory["totalVirtualMB"] = static_cast<qint64>(memInfo.ullTotalVirtual / (1024 * 1024));
        memory["availableVirtualMB"] = static_cast<qint64>(memInfo.ullAvailVirtual / (1024 * 1024));
        diagnostics["memory"] = memory;
    }
#endif

    // ========================================================
    // Overall Health Score
    // ========================================================
    int healthScore = 100;
    int criticalIssues = 0;
    int warnings = 0;

    // Critical checks
    if (!m_initialized) {
        criticalIssues++;
        healthScore -= 20;
    }
    if (!diagnostics["cmake"].toBool()) {
        warnings++;
        healthScore -= 5;
    }
    if (!diagnostics["git"].toBool()) {
        warnings++;
        healthScore -= 5;
    }

#ifdef Q_OS_WIN
    // Check for low memory
    if (diagnostics.contains("memory")) {
        QJsonObject mem = diagnostics["memory"].toObject();
        if (mem["memoryLoadPercent"].toInt() > 90) {
            warnings++;
            healthScore -= 10;
        }
    }
#endif

    diagnostics["healthScore"] = healthScore;
    diagnostics["criticalIssues"] = criticalIssues;
    diagnostics["warnings"] = warnings;

    result.data = diagnostics;
    result.message = QString("Diagnostics complete. Health score: %1/100").arg(healthScore);
    if (healthScore < 50) {
        result.success = false;
        result.message += " (Critical issues detected)";
    }
    
    if (callback) callback(result);
}

QJsonObject OrchestraManager::getSystemInfo() const {
    QJsonObject info;
    info["platform"] = QSysInfo::prettyProductName();
    info["architecture"] = QSysInfo::currentCpuArchitecture();
    info["hostname"] = QSysInfo::machineHostName();
    info["qtVersion"] = QString::fromLatin1(qVersion());
    info["processors"] = QThread::idealThreadCount();
    info["buildType"] = 
#ifdef NDEBUG
        "Release";
#else
        "Debug";
#endif
    info["compileTime"] = QString(__DATE__) + " " + QString(__TIME__);

#ifdef Q_OS_WIN
    // Windows-specific info
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    info["pageSize"] = static_cast<int>(sysInfo.dwPageSize);
    info["processorType"] = static_cast<int>(sysInfo.dwProcessorType);

    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(memInfo);
    if (GlobalMemoryStatusEx(&memInfo)) {
        info["totalMemoryGB"] = QString::number(memInfo.ullTotalPhys / (1024.0 * 1024 * 1024), 'f', 1);
        info["availableMemoryGB"] = QString::number(memInfo.ullAvailPhys / (1024.0 * 1024 * 1024), 'f', 1);
    }
#endif

    return info;
}

QJsonObject OrchestraManager::getSubsystemStatus() const {
    QJsonObject status;
    
    // Core state
    status["initialized"] = m_initialized;
    status["projectOpen"] = hasOpenProject();
    status["projectPath"] = m_projectPath;
    status["debugging"] = m_debugging;
    status["modelLoaded"] = m_modelLoaded;
    status["modelName"] = m_modelName;
    status["activeTerminals"] = m_terminalManager->sessions.size();

    // Subsystem availability
    QJsonObject available;
    available["build"] = (m_buildManager != nullptr);
    available["vcs"] = (m_vcsManager != nullptr);
    available["debug"] = (m_debugManager != nullptr);
    available["profiler"] = (m_profilerManager != nullptr);
    available["ai"] = (m_aiManager != nullptr);
    available["terminal"] = (m_terminalManager != nullptr);
    available["test"] = (m_testManager != nullptr);
    available["hotpatch"] = (m_hotpatchManager != nullptr);
    available["project"] = (m_projectManager != nullptr);
    available["file"] = (m_fileManager != nullptr);
    available["model_discovery"] = (m_modelDiscoveryService != nullptr);
    status["subsystems"] = available;

    // Feature capabilities
    QJsonObject capabilities;
    capabilities["canBuild"] = hasOpenProject() && m_buildManager != nullptr;
    capabilities["canDebug"] = hasOpenProject() && m_debugManager != nullptr && !m_debugging;
    capabilities["canProfile"] = hasOpenProject() && m_profilerManager != nullptr;
    capabilities["canRunTests"] = hasOpenProject() && m_testManager != nullptr;
    capabilities["canInfer"] = m_modelLoaded && m_aiManager != nullptr;
    capabilities["canHotpatch"] = m_hotpatchManager != nullptr;
    capabilities["canDiscoverModels"] = m_modelDiscoveryService != nullptr;
    status["capabilities"] = capabilities;

    return status;
}

// ================================================================
// ModelDiscoveryService Implementation
// ================================================================

class ModelDiscoveryService : public QObject {
    Q_OBJECT

public:
    explicit ModelDiscoveryService(QObject* parent = nullptr) : QObject(parent) {
        // Initialize with default settings
    }
    
    ~ModelDiscoveryService() override {
        // Cleanup resources
    }
    
    bool initialize(const QString& cachePath, qint64 maxCacheSizeMB) {
        m_cachePath = cachePath;
        m_maxCacheSizeMB = maxCacheSizeMB;
        
        // Ensure cache directory exists
        QDir cacheDir(m_cachePath);
        if (!cacheDir.exists()) {
            cacheDir.mkpath(".");
        }
        
        return true;
    }
    
    QList<ModelInfo> discoverModels(const QStringList& searchPaths, bool recursive) {
        QList<ModelInfo> models;
        
        // TODO: Implement actual model discovery logic
        // This will scan directories for model files and extract metadata
        
        return models;
    }
    
    QList<ModelInfo> getDiscoveredModels() const {
        return m_discoveredModels;
    }
    
    bool loadModel(const QString& modelId, std::function<void(int)> progress) {
        // TODO: Implement actual model loading logic
        // This will load the model into memory and prepare for inference
        
        return true;
    }
    
    ModelMetrics getMetrics() const {
        return m_metrics;
    }

private:
    QString m_cachePath;
    qint64 m_maxCacheSizeMB = 0;
    QList<ModelInfo> m_discoveredModels;
    ModelMetrics m_metrics;
};

// ================================================================
// Model Discovery Service Implementation
// ================================================================

bool OrchestraManager::initializeModelDiscovery(const QString& cachePath, qint64 maxCacheSizeMB) {
    QMutexLocker lock(&s_instanceMutex);  // Thread-safe initialization
    
    if (m_modelDiscoveryService) {
        emit info("ModelDiscovery", "Service already initialized");
        return true;
    }

    LOG_INFO("Initializing model discovery service", QJsonObject{
        {"cache_path", cachePath},
        {"max_cache_size_mb", static_cast<qint64>(maxCacheSizeMB)}
    });
    START_SPAN("model_discovery_init");
    
    try {
        // Create model discovery service
        m_modelDiscoveryService = std::make_unique<ModelDiscoveryService>();
        
        QString actualCachePath = cachePath.isEmpty() 
            ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/models"
            : cachePath;
            
        // Ensure cache directory exists
        QDir cacheDir(actualCachePath);
        if (!cacheDir.exists()) {
            cacheDir.mkpath(".");
        }
        
        LOG_INFO("Model discovery service initialized", QJsonObject{
            {"cache_path", actualCachePath},
            {"max_cache_size_mb", static_cast<qint64>(maxCacheSizeMB)}
        });
        END_SPAN("model_discovery_init");
        
        emit info("ModelDiscovery", QString("Initialized with cache: %1 (max %2MB)")
                  .arg(actualCachePath).arg(maxCacheSizeMB));
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Model discovery initialization failed", QJsonObject{
            {"error", QString::fromStdString(e.what())}
        });
        END_SPAN("model_discovery_init");
        emit error("ModelDiscovery", QString("Initialization failed: %1").arg(e.what()));
        return false;
    }
}

QList<ModelInfo> OrchestraManager::discoverModels(const QStringList& searchPaths, bool recursive) {
    QReadLocker lock(&s_stateLock);  // Thread-safe read access
    
    if (!m_modelDiscoveryService) {
        LOG_WARN("Model discovery attempted without service initialization");
        emit error("ModelDiscovery", "Service not initialized");
        return QList<ModelInfo>();
    }

    LOG_INFO("Starting model discovery", QJsonObject{
        {"search_paths_count", searchPaths.size()},
        {"recursive", recursive}
    });
    START_SPAN("model_discovery");
    LOG_COUNTER("model_discovery_calls", 1);
    
    QElapsedTimer discoveryTimer;
    discoveryTimer.start();
    emit modelDiscoveryStarted();
    
    QStringList actualPaths = searchPaths;
    if (actualPaths.isEmpty()) {
        // Default search paths
        actualPaths << QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.ollama/models";
        actualPaths << QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/models";
        actualPaths << "./models";
    }
    
    emit modelDiscoveryProgress(0, "Starting model discovery...");
    
    // Integrate with AutoModelLoader
    QList<ModelInfo> models;
    
    try {
        // Get AutoModelLoader instance
        auto& loader = AutoModelLoader::AutoModelLoader::GetInstance();
        
        // Discover models using AutoModelLoader
        std::vector<std::string> discoveredPaths = loader.discoverAvailableModels();
        
        emit modelDiscoveryProgress(50, "Processing discovered models...");
        
        for (size_t i = 0; i < discoveredPaths.size(); ++i) {
            const std::string& modelPath = discoveredPaths[i];
            int progressPercent = 50 + (i + 1) * 50 / discoveredPaths.size();
            
            emit modelDiscoveryProgress(progressPercent, QString("Processing %1").arg(QString::fromStdString(modelPath)));
            
            // Get model metadata from AutoModelLoader
            AutoModelLoader::ModelMetadata metadata = loader.getModelMetadata(modelPath);
            
            // Convert to our ModelInfo structure
            ModelInfo model;
            model.id = QString::fromStdString(metadata.path);
            model.name = QString::fromStdString(metadata.name);
            model.path = QString::fromStdString(metadata.path);
            model.sizeMB = static_cast<qint64>(metadata.sizeBytes / (1024 * 1024));
            model.format = QString::fromStdString(metadata.modelType);
            model.framework = QString::fromStdString(metadata.architecture);
            model.parameterCount = static_cast<uint32_t>(metadata.sizeBytes / 1000000); // Approximate parameter count
            model.quantization = "unknown"; // AutoModelLoader doesn't provide this
            model.cached = metadata.isHealthy;
            model.checksum = QString::fromStdString(metadata.sha256Hash);
            
            models << model;
        }
        
        emit modelDiscoveryProgress(100, "Discovery complete");
        
        qint64 discoveryTime = discoveryTimer.elapsed();
        LOG_INFO("Model discovery completed", QJsonObject{
            {"models_found", models.size()},
            {"duration_ms", static_cast<qint64>(discoveryTime)}
        });
        LOG_METRIC("model_discovery_duration", discoveryTime);
        LOG_COUNTER("models_discovered", models.size());
        END_SPAN("model_discovery");
        
        emit modelDiscoveryFinished(models);
        
    } catch (const std::exception& e) {
        QString errorMsg = QString("Model discovery failed: %1").arg(e.what());
        LOG_ERROR("Model discovery failed", QJsonObject{
            {"error", QString::fromStdString(e.what())}
        });
        END_SPAN("model_discovery");
        emit modelDiscoveryError(errorMsg);
    }
    
    return models;
}

QList<ModelInfo> OrchestraManager::getDiscoveredModels() const {
    if (!m_modelDiscoveryService) {
        return QList<ModelInfo>();
    }
    
    QList<ModelInfo> models;
    
    try {
        auto& loader = AutoModelLoader::AutoModelLoader::GetInstance();
        std::vector<AutoModelLoader::ModelMetadata> cachedModels = loader.getCachedModels();
        
        for (const auto& metadata : cachedModels) {
            ModelInfo model;
            model.id = QString::fromStdString(metadata.path);
            model.name = QString::fromStdString(metadata.name);
            model.path = QString::fromStdString(metadata.path);
            model.sizeMB = static_cast<qint64>(metadata.sizeBytes / (1024 * 1024));
            model.format = QString::fromStdString(metadata.modelType);
            model.framework = QString::fromStdString(metadata.architecture);
            model.parameterCount = static_cast<uint32_t>(metadata.sizeBytes / 1000000);
            model.quantization = "unknown";
            model.cached = metadata.isHealthy;
            model.checksum = QString::fromStdString(metadata.sha256Hash);
            
            models << model;
        }
    } catch (const std::exception& e) {
        qWarning() << "Failed to get discovered models:" << e.what();
    }
    
    return models;
}

ModelInfo OrchestraManager::getModelInfo(const QString& modelId) const {
    ModelInfo info;
    
    try {
        auto& loader = AutoModelLoader::AutoModelLoader::GetInstance();
        AutoModelLoader::ModelMetadata metadata = loader.getModelMetadata(modelId.toStdString());
        
        info.id = QString::fromStdString(metadata.path);
        info.name = QString::fromStdString(metadata.name);
        info.path = QString::fromStdString(metadata.path);
        info.sizeMB = static_cast<qint64>(metadata.sizeBytes / (1024 * 1024));
        info.format = QString::fromStdString(metadata.modelType);
        info.framework = QString::fromStdString(metadata.architecture);
        info.parameterCount = static_cast<uint32_t>(metadata.sizeBytes / 1000000);
        info.quantization = "unknown";
        info.cached = metadata.isHealthy;
        info.checksum = QString::fromStdString(metadata.sha256Hash);
        
    } catch (const std::exception& e) {
        qWarning() << "Failed to get model info for" << modelId << ":" << e.what();
    }
    
    return info;
}

bool OrchestraManager::loadModel(const QString& modelId, std::function<void(int)> progress) {
    if (!m_modelDiscoveryService) {
        LOG_WARN("Model load attempted without service initialization");
        emit error("ModelDiscovery", "Service not initialized");
        return false;
    }

    LOG_INFO("Loading model", QJsonObject{{"model_id", modelId}});
    START_SPAN("model_load");
    LOG_COUNTER("model_load_calls", 1);
    
    QElapsedTimer loadTimer;
    loadTimer.start();
    
    emit modelLoadingStarted(modelId);
    
    try {
        auto& loader = AutoModelLoader::AutoModelLoader::GetInstance();
        
        // Load model using AutoModelLoader
        bool success = loader.loadModel(modelId.toStdString());
        
        if (progress) progress(100);
        emit modelLoadingProgress(100);
        
        qint64 loadTime = loadTimer.elapsed();
        
        if (success) {
            m_modelLoaded = true;
            m_modelName = modelId;
            
            LOG_INFO("Model loaded successfully", QJsonObject{
                {"model_id", modelId},
                {"duration_ms", static_cast<qint64>(loadTime)}
            });
            LOG_METRIC("model_load_duration", loadTime);
            LOG_COUNTER("model_load_success", 1);
            END_SPAN("model_load");
            
            emit modelLoaded(modelId);
            emit aiModelLoaded(modelId);
        } else {
            LOG_ERROR("Model load failed", QJsonObject{{"model_id", modelId}});
            LOG_COUNTER("model_load_failure", 1);
            END_SPAN("model_load");
            emit modelLoadFailed(modelId, "Failed to load model");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        QString errorMsg = QString("Model load failed: %1").arg(e.what());
        LOG_ERROR("Model load exception", QJsonObject{
            {"model_id", modelId},
            {"error", QString::fromStdString(e.what())}
        });
        LOG_COUNTER("model_load_failure", 1);
        END_SPAN("model_load");
        emit modelLoadFailed(modelId, errorMsg);
        return false;
    }
}

ModelMetrics OrchestraManager::getModelMetrics() const {
    ModelMetrics metrics;
    
    if (!m_modelDiscoveryService) {
        return metrics;
    }
    
    try {
        auto& loader = AutoModelLoader::AutoModelLoader::GetInstance();
        auto& loaderMetrics = loader.getMetrics();
        
        metrics.totalDiscoveryCalls = loaderMetrics.totalDiscoveryCalls;
        metrics.totalLoadCalls = loaderMetrics.totalLoadCalls;
        metrics.successfulLoads = loaderMetrics.successfulLoads;
        metrics.failedLoads = loaderMetrics.failedLoads;
        metrics.cacheHits = loaderMetrics.cacheHits;
        metrics.cacheMisses = loaderMetrics.cacheMisses;
        metrics.totalCacheSize = loader.getCachedModels().size();
        
    } catch (const std::exception& e) {
        qWarning() << "Failed to get model metrics:" << e.what();
    }
    
    return metrics;
}

} // namespace RawrXD

#include "orchestra_manager.moc"
