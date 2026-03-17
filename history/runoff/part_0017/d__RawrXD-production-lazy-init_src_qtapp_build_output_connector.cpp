/**
 * @file build_output_connector.cpp
 * @brief Implementation of build output connector for Problems Panel
 * @author RawrXD Team
 * @version 1.0.0
 */

#include "build_output_connector.hpp"
#include "problems_panel.hpp"
#include <QProcess>
#include <QRegularExpression>
#include <QDateTime>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QSettings>
#include <QDebug>

/**
 * BuildOutputConnector Implementation
 */

BuildOutputConnector::BuildOutputConnector(ProblemsPanel* problemsPanel, QObject* parent)
    : QObject(parent)
    , m_problemsPanel(problemsPanel)
    , m_buildProcess(nullptr)
    , m_state(Idle)
{
    m_progressTimer = new QTimer(this);
    m_progressTimer->setInterval(500);
    connect(m_progressTimer, &QTimer::timeout, this, &BuildOutputConnector::updateProgress);
}

BuildOutputConnector::~BuildOutputConnector() {
    if (m_buildProcess) {
        if (m_buildProcess->state() == QProcess::Running) {
            m_buildProcess->kill();
            m_buildProcess->waitForFinished(3000);
        }
        delete m_buildProcess;
    }
}

bool BuildOutputConnector::startBuild(const BuildConfiguration& config) {
    if (m_state == Running) {
        qWarning() << "Build already in progress";
        return false;
    }
    
    // Validate configuration
    if (config.buildTool.isEmpty() || config.sourceFile.isEmpty()) {
        emit buildFailed("Invalid build configuration: missing build tool or source file");
        return false;
    }
    
    QFileInfo sourceInfo(config.sourceFile);
    if (!sourceInfo.exists()) {
        emit buildFailed(QString("Source file does not exist: %1").arg(config.sourceFile));
        return false;
    }
    
    // Clean up previous build process
    if (m_buildProcess) {
        delete m_buildProcess;
    }
    
    m_buildProcess = new QProcess(this);
    m_currentConfig = config;
    m_state = Running;
    m_errorCount = 0;
    m_warningCount = 0;
    m_linesProcessed = 0;
    m_fullOutput.clear();
    m_stdoutBuffer.clear();
    m_stderrBuffer.clear();
    m_buildStartTime = QDateTime::currentDateTime();
    
    // Clear problems panel
    if (m_problemsPanel) {
        // Assuming problems panel has a clear method
        // m_problemsPanel->clearProblems();
    }
    
    // Set up process signals
    connect(m_buildProcess, &QProcess::started, this, &BuildOutputConnector::onProcessStarted);
    connect(m_buildProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &BuildOutputConnector::onProcessFinished);
    connect(m_buildProcess, &QProcess::errorOccurred, this, &BuildOutputConnector::onProcessError);
    connect(m_buildProcess, &QProcess::readyReadStandardOutput, this, &BuildOutputConnector::onStdoutReadyRead);
    connect(m_buildProcess, &QProcess::readyReadStandardError, this, &BuildOutputConnector::onStderrReadyRead);
    
    // Set working directory
    if (!config.workingDirectory.isEmpty()) {
        m_buildProcess->setWorkingDirectory(config.workingDirectory);
    } else {
        m_buildProcess->setWorkingDirectory(sourceInfo.absolutePath());
    }
    
    // Construct command line
    QStringList args;
    
    // Add defines
    for (const QString& define : config.defines) {
        args << QString("/D%1").arg(define);
    }
    
    // Add include paths
    for (const QString& includePath : config.includePaths) {
        args << QString("/I\"%1\"").arg(includePath);
    }
    
    // Add library paths (for linker)
    for (const QString& libPath : config.libraryPaths) {
        args << QString("/LIBPATH:\"%1\"").arg(libPath);
    }
    
    // Add additional flags
    args << config.additionalFlags;
    
    // Add output file
    if (!config.outputFile.isEmpty()) {
        args << QString("/Fo\"%1\"").arg(config.outputFile);
    }
    
    // Add source file
    args << config.sourceFile;
    
    QString commandLine = config.buildTool + " " + args.join(" ");
    qDebug() << "Starting build:" << commandLine;
    
    emit buildStarted(config.sourceFile);
    emit buildOutputReceived(QString("=== Build started: %1 ===\n").arg(QDateTime::currentDateTime().toString()));
    emit buildOutputReceived(QString("Command: %1\n").arg(commandLine));
    emit buildOutputReceived(QString("Working directory: %1\n\n").arg(m_buildProcess->workingDirectory()));
    
    // Start the process
    m_buildProcess->start(config.buildTool, args);
    
    if (!m_buildProcess->waitForStarted(5000)) {
        m_state = Failed;
        emit buildFailed(QString("Failed to start build process: %1").arg(m_buildProcess->errorString()));
        return false;
    }
    
    m_progressTimer->start();
    return true;
}

void BuildOutputConnector::cancelBuild() {
    if (m_state != Running || !m_buildProcess) {
        return;
    }
    
    m_state = Cancelled;
    m_buildProcess->kill();
    
    if (!m_buildProcess->waitForFinished(3000)) {
        m_buildProcess->terminate();
        m_buildProcess->waitForFinished(1000);
    }
    
    m_progressTimer->stop();
    
    emit buildOutputReceived("\n=== Build cancelled by user ===\n");
    emit buildCancelled();
    
    qint64 duration = m_buildStartTime.msecsTo(QDateTime::currentDateTime());
    recordBuildInHistory(Cancelled, duration);
}

void BuildOutputConnector::setProblemsPanel(ProblemsPanel* panel) {
    m_problemsPanel = panel;
}

QVector<BuildOutputConnector::BuildHistoryEntry> BuildOutputConnector::getBuildHistory(int maxEntries) const {
    int count = qMin(maxEntries, m_buildHistory.size());
    return m_buildHistory.mid(m_buildHistory.size() - count, count);
}

void BuildOutputConnector::clearBuildHistory() {
    m_buildHistory.clear();
}

void BuildOutputConnector::onProcessStarted() {
    qDebug() << "Build process started";
    emit buildProgress(0, "Build in progress...");
}

void BuildOutputConnector::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    m_progressTimer->stop();
    
    // Read any remaining output
    onStdoutReadyRead();
    onStderrReadyRead();
    
    qint64 duration = m_buildStartTime.msecsTo(QDateTime::currentDateTime());
    
    if (m_state == Cancelled) {
        return; // Already handled in cancelBuild()
    }
    
    bool success = (exitCode == 0 && exitStatus == QProcess::NormalExit);
    m_state = success ? Completed : Failed;
    
    QString statusMsg;
    if (success) {
        statusMsg = QString("=== Build completed successfully ===\n"
                           "Errors: %1, Warnings: %2, Duration: %3s\n")
                    .arg(m_errorCount)
                    .arg(m_warningCount)
                    .arg(duration / 1000.0, 0, 'f', 2);
    } else {
        statusMsg = QString("=== Build failed ===\n"
                           "Exit code: %1, Errors: %2, Warnings: %3, Duration: %4s\n")
                    .arg(exitCode)
                    .arg(m_errorCount)
                    .arg(m_warningCount)
                    .arg(duration / 1000.0, 0, 'f', 2);
    }
    
    emit buildOutputReceived("\n" + statusMsg);
    emit buildCompleted(success, m_errorCount, m_warningCount, duration);
    emit buildProgress(100, success ? "Build completed" : "Build failed");
    
    recordBuildInHistory(m_state, duration);
}

void BuildOutputConnector::onProcessError(QProcess::ProcessError error) {
    m_progressTimer->stop();
    m_state = Failed;
    
    QString errorMsg;
    switch (error) {
        case QProcess::FailedToStart:
            errorMsg = QString("Failed to start build tool: %1").arg(m_currentConfig.buildTool);
            break;
        case QProcess::Crashed:
            errorMsg = "Build process crashed";
            break;
        case QProcess::Timedout:
            errorMsg = "Build process timed out";
            break;
        case QProcess::WriteError:
            errorMsg = "Write error to build process";
            break;
        case QProcess::ReadError:
            errorMsg = "Read error from build process";
            break;
        default:
            errorMsg = "Unknown build process error";
            break;
    }
    
    emit buildOutputReceived(QString("\nERROR: %1\n").arg(errorMsg));
    emit buildFailed(errorMsg);
    
    qint64 duration = m_buildStartTime.msecsTo(QDateTime::currentDateTime());
    recordBuildInHistory(Failed, duration);
}

void BuildOutputConnector::onStdoutReadyRead() {
    if (!m_buildProcess) return;
    
    QByteArray data = m_buildProcess->readAllStandardOutput();
    QString text = QString::fromLocal8Bit(data);
    
    m_stdoutBuffer += text;
    m_fullOutput += text;
    
    // Process complete lines
    QStringList lines = m_stdoutBuffer.split('\n');
    m_stdoutBuffer = lines.takeLast(); // Keep incomplete line in buffer
    
    for (const QString& line : lines) {
        if (!line.trimmed().isEmpty()) {
            processOutputLine(line, false);
        }
    }
}

void BuildOutputConnector::onStderrReadyRead() {
    if (!m_buildProcess) return;
    
    QByteArray data = m_buildProcess->readAllStandardError();
    QString text = QString::fromLocal8Bit(data);
    
    m_stderrBuffer += text;
    m_fullOutput += text;
    
    // Process complete lines
    QStringList lines = m_stderrBuffer.split('\n');
    m_stderrBuffer = lines.takeLast(); // Keep incomplete line in buffer
    
    for (const QString& line : lines) {
        if (!line.trimmed().isEmpty()) {
            processOutputLine(line, true);
        }
    }
}

void BuildOutputConnector::updateProgress() {
    if (m_state != Running) {
        return;
    }
    
    // Estimate progress based on lines processed
    int percentage = qMin(90, (m_linesProcessed * 100) / m_estimatedTotalLines);
    
    QString status;
    if (m_errorCount > 0 || m_warningCount > 0) {
        status = QString("Building... (Errors: %1, Warnings: %2)").arg(m_errorCount).arg(m_warningCount);
    } else {
        status = "Building...";
    }
    
    emit buildProgress(percentage, status);
}

void BuildOutputConnector::processOutputLine(const QString& line, bool isStderr) {
    m_linesProcessed++;
    
    if (m_realTimeUpdates) {
        emit buildOutputReceived(line + "\n");
    }
    
    // Parse for errors/warnings
    parseErrorLine(line);
}

void BuildOutputConnector::parseErrorLine(const QString& line) {
    BuildError error;
    
    // Try different parser patterns
    error = parseMASMError(line);
    if (error.file.isEmpty()) {
        error = parseCppError(line);
    }
    if (error.file.isEmpty()) {
        error = parseLinkerError(line);
    }
    if (error.file.isEmpty()) {
        error = parseGenericError(line);
    }
    
    // If we detected an error/warning, process it
    if (!error.file.isEmpty() || !error.severity.isEmpty()) {
        if (error.severity == "error") {
            m_errorCount++;
        } else if (error.severity == "warning") {
            m_warningCount++;
        }
        
        emit buildErrorDetected(error);
        sendToProblemsPanel(error);
        
        if (m_problemsPanel && !error.file.isEmpty()) {
            emit diagnosticAdded(error.file, error.line, error.severity, error.message);
        }
    }
}

BuildError BuildOutputConnector::parseMASMError(const QString& line) {
    // MASM format: filename.asm(line) : severity code: message
    // Example: test.asm(42) : error A2008: syntax error : mov
    
    static QRegularExpression masmRegex(
        R"(^(.+?)\((\d+)\)\s*:\s*(error|warning)\s+([A-Z]\d+)\s*:\s*(.+)$)",
        QRegularExpression::CaseInsensitiveOption
    );
    
    QRegularExpressionMatch match = masmRegex.match(line);
    if (match.hasMatch()) {
        BuildError error;
        error.file = match.captured(1).trimmed();
        error.line = match.captured(2).toInt();
        error.column = 0;
        error.severity = match.captured(3).toLower();
        error.code = match.captured(4);
        error.message = match.captured(5).trimmed();
        error.fullText = line;
        return error;
    }
    
    return BuildError();
}

BuildError BuildOutputConnector::parseCppError(const QString& line) {
    // MSVC C++ format: filename.cpp(line,col): severity code: message
    // Example: test.cpp(42,5): error C2065: 'undefined' : undeclared identifier
    
    static QRegularExpression cppRegex(
        R"(^(.+?)\((\d+)(?:,(\d+))?\)\s*:\s*(error|warning)\s+([A-Z]\d+)\s*:\s*(.+)$)",
        QRegularExpression::CaseInsensitiveOption
    );
    
    QRegularExpressionMatch match = cppRegex.match(line);
    if (match.hasMatch()) {
        BuildError error;
        error.file = match.captured(1).trimmed();
        error.line = match.captured(2).toInt();
        error.column = match.captured(3).isEmpty() ? 0 : match.captured(3).toInt();
        error.severity = match.captured(4).toLower();
        error.code = match.captured(5);
        error.message = match.captured(6).trimmed();
        error.fullText = line;
        return error;
    }
    
    return BuildError();
}

BuildError BuildOutputConnector::parseLinkerError(const QString& line) {
    // Linker format: LINK : severity code: message
    // Example: LINK : fatal error LNK1104: cannot open file 'kernel32.lib'
    
    static QRegularExpression linkerRegex(
        R"(^LINK\s*:\s*(fatal error|error|warning)\s+([A-Z]+\d+)\s*:\s*(.+)$)",
        QRegularExpression::CaseInsensitiveOption
    );
    
    QRegularExpressionMatch match = linkerRegex.match(line);
    if (match.hasMatch()) {
        BuildError error;
        error.file = "linker";
        error.line = 0;
        error.column = 0;
        error.severity = match.captured(1).contains("error", Qt::CaseInsensitive) ? "error" : "warning";
        error.code = match.captured(2);
        error.message = match.captured(3).trimmed();
        error.fullText = line;
        return error;
    }
    
    return BuildError();
}

BuildError BuildOutputConnector::parseGenericError(const QString& line) {
    // Generic format: error/warning keywords
    QString lowerLine = line.toLower();
    
    if (lowerLine.contains("error") || lowerLine.contains("fatal")) {
        BuildError error;
        error.severity = "error";
        error.message = line.trimmed();
        error.fullText = line;
        return error;
    }
    
    if (lowerLine.contains("warning")) {
        BuildError error;
        error.severity = "warning";
        error.message = line.trimmed();
        error.fullText = line;
        return error;
    }
    
    return BuildError();
}

void BuildOutputConnector::sendToProblemsPanel(const BuildError& error) {
    if (!m_problemsPanel) {
        return;
    }
    
    // Assuming ProblemsPanel has an addProblem method
    // This would need to be implemented in problems_panel.cpp
    // m_problemsPanel->addProblem(error.file, error.line, error.column, 
    //                             error.severity, error.code, error.message);
}

void BuildOutputConnector::recordBuildInHistory(BuildState result, qint64 durationMs) {
    BuildHistoryEntry entry;
    entry.timestamp = m_buildStartTime;
    entry.config = m_currentConfig;
    entry.result = result;
    entry.errorCount = m_errorCount;
    entry.warningCount = m_warningCount;
    entry.durationMs = durationMs;
    entry.output = m_fullOutput;
    
    m_buildHistory.append(entry);
    
    // Trim history if too large
    while (m_buildHistory.size() > m_maxHistoryEntries) {
        m_buildHistory.removeFirst();
    }
}

/**
 * BuildManager Implementation
 */

BuildManager::BuildManager(ProblemsPanel* problemsPanel, QObject* parent)
    : QObject(parent)
    , m_currentBuildSystem(MASM)
    , m_outputDirectory("./build")
{
    m_connector = new BuildOutputConnector(problemsPanel, this);
    
    // Forward signals
    connect(m_connector, &BuildOutputConnector::buildStarted, this, &BuildManager::buildStarted);
    connect(m_connector, &BuildOutputConnector::buildCompleted, this, &BuildManager::buildCompleted);
    connect(m_connector, &BuildOutputConnector::buildProgress, this, 
            [this](int percentage, const QString&) { emit buildProgress(percentage); });
}

BuildManager::~BuildManager() {
}

bool BuildManager::buildFile(const QString& sourceFile, BuildSystem system) {
    BuildConfiguration config;
    
    switch (system) {
        case MASM:
            config = createMASMConfig(sourceFile);
            break;
        case MSVC:
            config = createMSVCConfig(sourceFile);
            break;
        default:
            qWarning() << "Unsupported build system:" << system;
            return false;
    }
    
    m_currentBuildSystem = system;
    emit buildSystemDetected(system);
    
    return m_connector->startBuild(config);
}

bool BuildManager::buildProject(const QString& projectPath, BuildSystem system) {
    BuildConfiguration config;
    
    if (system == CMake) {
        config = createCMakeConfig(projectPath);
    } else {
        qWarning() << "buildProject only supports CMake currently";
        return false;
    }
    
    return m_connector->startBuild(config);
}

bool BuildManager::rebuildAll() {
    // Implementation depends on build system
    return false;
}

bool BuildManager::cleanBuild() {
    // Clean output directory
    QDir outputDir(m_outputDirectory);
    if (outputDir.exists()) {
        outputDir.removeRecursively();
    }
    outputDir.mkpath(".");
    
    return true;
}

void BuildManager::cancelBuild() {
    m_connector->cancelBuild();
}

void BuildManager::setProblemsPanel(ProblemsPanel* panel) {
    m_connector->setProblemsPanel(panel);
}

void BuildManager::setBuildSystem(BuildSystem system) {
    m_currentBuildSystem = system;
}

BuildManager::BuildSystem BuildManager::detectBuildSystem(const QString& filePath) {
    QFileInfo info(filePath);
    QString ext = info.suffix().toLower();
    
    if (ext == "asm") return MASM;
    if (ext == "c" || ext == "cpp" || ext == "cc" || ext == "cxx") return MSVC;
    if (filePath.endsWith("CMakeLists.txt", Qt::CaseInsensitive)) return CMake;
    if (filePath.endsWith("Makefile", Qt::CaseInsensitive)) return Make;
    
    return MASM; // Default
}

BuildConfiguration BuildManager::getDefaultConfig(BuildSystem system, const QString& sourceFile) {
    switch (system) {
        case MASM: return createMASMConfig(sourceFile);
        case MSVC: return createMSVCConfig(sourceFile);
        default: return BuildConfiguration();
    }
}

void BuildManager::saveConfiguration(const QString& name, const BuildConfiguration& config) {
    m_savedConfigurations[name] = config;
    
    // Persist to QSettings
    QSettings settings("RawrXD", "BuildManager");
    settings.beginGroup("BuildConfigurations");
    settings.beginGroup(name);
    settings.setValue("buildTool", config.buildTool);
    settings.setValue("sourceFile", config.sourceFile);
    settings.setValue("outputFile", config.outputFile);
    settings.setValue("includePaths", config.includePaths);
    settings.setValue("libraryPaths", config.libraryPaths);
    settings.setValue("defines", config.defines);
    settings.setValue("additionalFlags", config.additionalFlags);
    settings.setValue("workingDirectory", config.workingDirectory);
    settings.setValue("timeoutMs", config.timeoutMs);
    settings.endGroup();
    settings.endGroup();
}

BuildConfiguration BuildManager::loadConfiguration(const QString& name) {
    if (m_savedConfigurations.contains(name)) {
        return m_savedConfigurations[name];
    }
    
    // Load from QSettings
    QSettings settings("RawrXD", "BuildManager");
    settings.beginGroup("BuildConfigurations");
    settings.beginGroup(name);
    
    BuildConfiguration config;
    config.buildTool = settings.value("buildTool").toString();
    config.sourceFile = settings.value("sourceFile").toString();
    config.outputFile = settings.value("outputFile").toString();
    config.includePaths = settings.value("includePaths").toStringList();
    config.libraryPaths = settings.value("libraryPaths").toStringList();
    config.defines = settings.value("defines").toStringList();
    config.additionalFlags = settings.value("additionalFlags").toStringList();
    config.workingDirectory = settings.value("workingDirectory").toString();
    config.timeoutMs = settings.value("timeoutMs", 120000).toInt();
    
    settings.endGroup();
    settings.endGroup();
    
    m_savedConfigurations[name] = config;
    return config;
}

QStringList BuildManager::getAvailableConfigurations() const {
    QSettings settings("RawrXD", "BuildManager");
    settings.beginGroup("BuildConfigurations");
    QStringList configs = settings.childGroups();
    settings.endGroup();
    return configs;
}

BuildConfiguration BuildManager::createMASMConfig(const QString& sourceFile) {
    BuildConfiguration config;
    config.buildTool = findBuildTool(MASM);
    config.sourceFile = sourceFile;
    
    QFileInfo info(sourceFile);
    QString baseName = info.completeBaseName();
    config.outputFile = QString("%1/%2.obj").arg(m_outputDirectory).arg(baseName);
    
    // Standard MASM flags
    config.additionalFlags << "/c";          // Compile only
    config.additionalFlags << "/nologo";     // Suppress banner
    config.additionalFlags << "/Zi";         // Debug info
    config.additionalFlags << "/W3";         // Warning level 3
    
    if (m_verboseOutput) {
        config.additionalFlags << "/Fl";     // Generate listing
    }
    
    config.includePaths = getStandardIncludePaths(MASM);
    config.workingDirectory = info.absolutePath();
    
    return config;
}

BuildConfiguration BuildManager::createMSVCConfig(const QString& sourceFile) {
    BuildConfiguration config;
    config.buildTool = findBuildTool(MSVC);
    config.sourceFile = sourceFile;
    
    QFileInfo info(sourceFile);
    QString baseName = info.completeBaseName();
    config.outputFile = QString("%1/%2.obj").arg(m_outputDirectory).arg(baseName);
    
    // Standard MSVC flags
    config.additionalFlags << "/c";          // Compile only
    config.additionalFlags << "/nologo";     // Suppress banner
    config.additionalFlags << "/EHsc";       // Exception handling
    config.additionalFlags << "/W4";         // Warning level 4
    config.additionalFlags << "/std:c++17";  // C++17 standard
    
    if (m_verboseOutput) {
        config.additionalFlags << "/Fa";     // Generate assembly listing
    }
    
    config.includePaths = getStandardIncludePaths(MSVC);
    config.workingDirectory = info.absolutePath();
    
    return config;
}

BuildConfiguration BuildManager::createCMakeConfig(const QString& projectPath) {
    BuildConfiguration config;
    config.buildTool = findBuildTool(CMake);
    config.sourceFile = projectPath;
    
    config.additionalFlags << "-B" << m_outputDirectory;
    config.additionalFlags << "-S" << projectPath;
    
    if (m_verboseOutput) {
        config.additionalFlags << "--verbose";
    }
    
    config.workingDirectory = projectPath;
    
    return config;
}

QString BuildManager::findBuildTool(BuildSystem system) {
    QString tool;
    
    switch (system) {
        case MASM:
            tool = "ml64.exe";
            break;
        case MSVC:
            tool = "cl.exe";
            break;
        case CMake:
            tool = "cmake.exe";
            break;
        case Make:
            tool = "nmake.exe";
            break;
        case Ninja:
            tool = "ninja.exe";
            break;
        default:
            return QString();
    }
    
    // Check if tool is in PATH
    QProcess process;
    process.start("where", QStringList() << tool);
    if (process.waitForFinished(3000) && process.exitCode() == 0) {
        return tool;
    }
    
    // Try common installation paths
    QStringList searchPaths;
    
    if (system == MASM || system == MSVC) {
        // Visual Studio installation paths
        searchPaths << "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC";
        searchPaths << "C:/Program Files/Microsoft Visual Studio/2022/Professional/VC/Tools/MSVC";
        searchPaths << "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC";
        searchPaths << "C:/Program Files (x86)/Microsoft Visual Studio/2019/Enterprise/VC/Tools/MSVC";
        
        for (const QString& basePath : searchPaths) {
            QDir baseDir(basePath);
            if (baseDir.exists()) {
                // Find latest version
                QStringList versions = baseDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
                if (!versions.isEmpty()) {
                    QString binPath = QString("%1/%2/bin/Hostx64/x64/%3").arg(basePath).arg(versions.first()).arg(tool);
                    if (QFileInfo::exists(binPath)) {
                        return binPath;
                    }
                }
            }
        }
    }
    
    return tool; // Return tool name as fallback
}

QStringList BuildManager::getStandardIncludePaths(BuildSystem system) {
    QStringList paths;
    
    if (system == MASM || system == MSVC) {
        // Add Windows SDK includes
        paths << "C:/Program Files (x86)/Windows Kits/10/Include";
        
        // Add MASM32 if available
        if (system == MASM) {
            paths << "C:/masm32/include";
            paths << "C:/masm64/include";
        }
    }
    
    return paths;
}

QStringList BuildManager::getStandardLibraryPaths(BuildSystem system) {
    QStringList paths;
    
    if (system == MASM || system == MSVC) {
        // Add Windows SDK libraries
        paths << "C:/Program Files (x86)/Windows Kits/10/Lib";
        
        // Add MASM32 if available
        if (system == MASM) {
            paths << "C:/masm32/lib";
            paths << "C:/masm64/lib";
        }
    }
    
    return paths;
}
