/**
 * @file powershell_compiler_manager.cpp
 * @brief Implementation of PowerShell Compiler Manager for Agentic IDE
 */

#include "powershell_compiler_manager.h"
#include "logging/logger.h"
#include "metrics/metrics.h"
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QThread>
#include <QDebug>

// Global singleton instance
static std::unique_ptr<PowerShellCompilerManager> g_compilerManager;

PowerShellCompilerManager::PowerShellCompilerManager(QObject *parent)
    : QObject(parent)
    , m_currentProcess(nullptr)
    , m_timeoutTimer(nullptr)
    , m_totalCompilations(0)
    , m_successfulCompilations(0)
    , m_failedCompilations(0)
    , m_defaultTimeoutMs(30000) // 30 seconds default timeout
{
    initialize();
}

PowerShellCompilerManager::~PowerShellCompilerManager()
{
    cleanupProcess();
    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
        delete m_timeoutTimer;
    }
}

void PowerShellCompilerManager::initialize()
{
    // Create timeout timer
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &PowerShellCompilerManager::onTimeout);
    
    // Initialize with default compilers path
    QString defaultPath = QCoreApplication::applicationDirPath() + "/compilers";
    if (QDir(defaultPath).exists()) {
        loadCompilers(defaultPath);
    }
}

bool PowerShellCompilerManager::loadCompilers(const QString &compilersPath)
{
    QDir compilersDir(compilersPath);
    if (!compilersDir.exists()) {
        qWarning() << "Compilers directory does not exist:" << compilersPath;
        return false;
    }
    
    m_compilersBasePath = compilersPath;
    m_compilers.clear();
    
    // Look for PowerShell compiler scripts (*.ps1)
    QStringList filters;
    filters << "*.ps1";
    QFileInfoList files = compilersDir.entryInfoList(filters, QDir::Files);
    
    int loadedCount = 0;
    for (const QFileInfo &fileInfo : files) {
        QString scriptPath = fileInfo.absoluteFilePath();
        
        // Skip files that don't look like compilers
        if (fileInfo.baseName().contains("test", Qt::CaseInsensitive) ||
            fileInfo.baseName().contains("example", Qt::CaseInsensitive)) {
            continue;
        }
        
        if (validateCompilerScript(scriptPath)) {
            CompilerInfo info = parseCompilerInfo(scriptPath);
            if (!info.language.isEmpty()) {
                m_compilers[info.language] = info;
                loadedCount++;
                emit compilerLoaded(info.language, info);
                
                qInfo() << "Loaded compiler:" << info.name << "for language" << info.language;
            }
        }
    }
    
    qInfo() << "Loaded" << loadedCount << "compilers from" << compilersPath;
    return loadedCount > 0;
}

QList<CompilerInfo> PowerShellCompilerManager::getAvailableCompilers() const
{
    return m_compilers.values();
}

CompilerInfo PowerShellCompilerManager::getCompilerInfo(const QString &language) const
{
    return m_compilers.value(language, CompilerInfo());
}

bool PowerShellCompilerManager::isCompilerAvailable(const QString &language) const
{
    return m_compilers.contains(language);
}

CompilationResult PowerShellCompilerManager::compile(const QString &language, const QString &sourceCode, 
                                                     const QString &outputPath, const QJsonObject &options)
{
    if (!isCompilerAvailable(language)) {
        CompilationResult result;
        result.success = false;
        result.error = QString("Compiler not available for language: %1").arg(language);
        return result;
    }
    
    // Create temporary source file
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString tempFile = QString("%1/%2_source_%3.%4")
                          .arg(tempDir)
                          .arg(language)
                          .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz"))
                          .arg(getCompilerInfo(language).extensions.isEmpty() ? "txt" : getCompilerInfo(language).extensions.first());
    
    QFile file(tempFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        CompilationResult result;
        result.success = false;
        result.error = QString("Failed to create temporary file: %1").arg(tempFile);
        return result;
    }
    
    QTextStream out(&file);
    out << sourceCode;
    file.close();
    
    // Compile the temporary file
    CompilationResult result = compileFile(language, tempFile, outputPath, options);
    
    // Clean up temporary file
    QFile::remove(tempFile);
    
    return result;
}

CompilationResult PowerShellCompilerManager::compileFile(const QString &language, const QString &sourceFilePath, 
                                                         const QString &outputPath, const QJsonObject &options)
{
    CompilationResult result;
    result.compilerName = language;
    result.success = false;
    
    if (!isCompilerAvailable(language)) {
        result.error = QString("Compiler not available for language: %1").arg(language);
        return result;
    }
    
    if (!QFile::exists(sourceFilePath)) {
        result.error = QString("Source file does not exist: %1").arg(sourceFilePath);
        return result;
    }
    
    // Check if another compilation is in progress
    if (m_currentProcess && m_currentProcess->state() == QProcess::Running) {
        result.error = "Another compilation is already in progress";
        return result;
    }
    
    CompilerInfo compiler = getCompilerInfo(language);
    
    // Build PowerShell command
    QString command = buildPowerShellCommand(compiler, sourceFilePath, outputPath, options);
    if (command.isEmpty()) {
        result.error = "Failed to build PowerShell command";
        return result;
    }
    
    // Start compilation
    emit compilationStarted(language, sourceFilePath);
    
    m_currentProcess = new QProcess(this);
    m_currentLanguage = language;
    m_currentSource = sourceFilePath;
    m_currentOutputPath = outputPath;
    m_currentOptions = options;
    m_startTime = QDateTime::currentMSecsSinceEpoch();
    
    connect(m_currentProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &PowerShellCompilerManager::onProcessFinished);
    connect(m_currentProcess, &QProcess::errorOccurred,
            this, &PowerShellCompilerManager::onProcessError);
    
    // Set timeout
    int timeout = options.value("timeout", compiler.timeoutMs).toInt();
    if (timeout <= 0) timeout = m_defaultTimeoutMs;
    m_timeoutTimer->start(timeout);
    
    // Start the process
    m_currentProcess->start("powershell", QStringList() << "-Command" << command);
    
    // Wait for completion (with timeout)
    if (!m_currentProcess->waitForFinished(timeout)) {
        result.error = "Compilation timed out";
        cleanupProcess();
        return result;
    }
    
    // Process should have finished by now, result will be set in onProcessFinished
    // For synchronous operation, we need to wait and process the result
    QEventLoop loop;
    connect(this, &PowerShellCompilerManager::compilationFinished, &loop, &QEventLoop::quit);
    loop.exec();
    
    // Return the result that was set in onProcessFinished
    return result;
}

void PowerShellCompilerManager::setDefaultTimeout(int timeoutMs)
{
    m_defaultTimeoutMs = timeoutMs;
}

void PowerShellCompilerManager::setCompilerEnabled(const QString &language, bool enabled)
{
    if (m_compilers.contains(language)) {
        m_compilers[language].enabled = enabled;
    }
}

void PowerShellCompilerManager::setCompilerPath(const QString &language, const QString &scriptPath)
{
    if (m_compilers.contains(language) && validateCompilerScript(scriptPath)) {
        m_compilers[language].scriptPath = scriptPath;
    }
}

int PowerShellCompilerManager::getCompilerCount() const
{
    return m_compilers.size();
}

QMap<QString, int> PowerShellCompilerManager::getCompilationStats() const
{
    QMap<QString, int> stats;
    stats["total"] = m_totalCompilations;
    stats["successful"] = m_successfulCompilations;
    stats["failed"] = m_failedCompilations;
    
    for (auto it = m_compilationStats.constBegin(); it != m_compilationStats.constEnd(); ++it) {
        stats[it.key()] = it.value();
    }
    
    return stats;
}

void PowerShellCompilerManager::resetStats()
{
    m_totalCompilations = 0;
    m_successfulCompilations = 0;
    m_failedCompilations = 0;
    m_compilationStats.clear();
}

void PowerShellCompilerManager::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_timeoutTimer->stop();
    
    CompilationResult result;
    result.compilerName = m_currentLanguage;
    result.exitCode = exitCode;
    result.durationMs = QDateTime::currentMSecsSinceEpoch() - m_startTime;
    
    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        result.success = true;
        result.output = QString::fromUtf8(m_currentProcess->readAllStandardOutput());
        
        // Try to extract output file path from output
        // This is compiler-specific and may need customization
        QString output = result.output;
        if (output.contains("Output:") || output.contains("Generated:")) {
            // Simple heuristic to extract file path
            QRegExp filePattern("[A-Za-z]:\\\\[^\n]+\.(exe|dll|so|a|obj|o)");
            if (filePattern.indexIn(output) != -1) {
                result.outputFile = filePattern.cap(0);
            }
        }
        
        if (result.outputFile.isEmpty() && !m_currentOutputPath.isEmpty()) {
            result.outputFile = m_currentOutputPath;
        }
    } else {
        result.success = false;
        result.error = QString::fromUtf8(m_currentProcess->readAllStandardError());
        if (result.error.isEmpty()) {
            result.error = QString("Compilation failed with exit code %1").arg(exitCode);
        }
    }
    
    updateStats(result.success);
    emit compilationFinished(result);
    
    if (!result.success) {
        emit compilationError(m_currentLanguage, result.error);
    }
    
    cleanupProcess();
}

void PowerShellCompilerManager::onProcessError(QProcess::ProcessError error)
{
    m_timeoutTimer->stop();
    
    CompilationResult result;
    result.compilerName = m_currentLanguage;
    result.success = false;
    result.durationMs = QDateTime::currentMSecsSinceEpoch() - m_startTime;
    
    switch (error) {
    case QProcess::FailedToStart:
        result.error = "Failed to start PowerShell process";
        break;
    case QProcess::Crashed:
        result.error = "PowerShell process crashed";
        break;
    case QProcess::Timedout:
        result.error = "PowerShell process timed out";
        break;
    case QProcess::WriteError:
        result.error = "Write error to PowerShell process";
        break;
    case QProcess::ReadError:
        result.error = "Read error from PowerShell process";
        break;
    default:
        result.error = "Unknown process error";
        break;
    }
    
    updateStats(false);
    emit compilationFinished(result);
    emit compilationError(m_currentLanguage, result.error);
    
    cleanupProcess();
}

void PowerShellCompilerManager::onTimeout()
{
    if (m_currentProcess && m_currentProcess->state() == QProcess::Running) {
        m_currentProcess->kill();
        
        CompilationResult result;
        result.compilerName = m_currentLanguage;
        result.success = false;
        result.error = "Compilation timed out";
        result.durationMs = QDateTime::currentMSecsSinceEpoch() - m_startTime;
        
        updateStats(false);
        emit compilationFinished(result);
        emit compilationError(m_currentLanguage, result.error);
        
        cleanupProcess();
    }
}

bool PowerShellCompilerManager::validateCompilerScript(const QString &scriptPath)
{
    if (!QFile::exists(scriptPath)) {
        return false;
    }
    
    QFile file(scriptPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    
    // Basic validation: check if it contains PowerShell content
    QTextStream in(&file);
    QString content = in.readAll();
    file.close();
    
    return content.contains("param(") || content.contains("function") || 
           content.contains("Write-Output") || content.contains("Write-Error");
}

CompilerInfo PowerShellCompilerManager::parseCompilerInfo(const QString &scriptPath)
{
    CompilerInfo info;
    info.scriptPath = scriptPath;
    info.enabled = true;
    info.timeoutMs = m_defaultTimeoutMs;
    
    // Extract language from filename
    QFileInfo fileInfo(scriptPath);
    QString baseName = fileInfo.baseName().toLower();
    
    // Map filename to language
    if (baseName.contains("cpp") || baseName.contains("c++")) {
        info.language = "cpp";
        info.name = "C++ Compiler";
        info.extensions = QStringList() << ".cpp" << ".cxx" << ".cc" << ".c";
    } else if (baseName.contains("java")) {
        info.language = "java";
        info.name = "Java Compiler";
        info.extensions = QStringList() << ".java";
    } else if (baseName.contains("python")) {
        info.language = "python";
        info.name = "Python Compiler";
        info.extensions = QStringList() << ".py";
    } else if (baseName.contains("rust")) {
        info.language = "rust";
        info.name = "Rust Compiler";
        info.extensions = QStringList() << ".rs";
    } else if (baseName.contains("go")) {
        info.language = "go";
        info.name = "Go Compiler";
        info.extensions = QStringList() << ".go";
    } else if (baseName.contains("javascript") || baseName.contains("js")) {
        info.language = "javascript";
        info.name = "JavaScript Compiler";
        info.extensions = QStringList() << ".js" << ".mjs";
    } else if (baseName.contains("typescript") || baseName.contains("ts")) {
        info.language = "typescript";
        info.name = "TypeScript Compiler";
        info.extensions = QStringList() << ".ts" << ".tsx";
    } else {
        // Generic compiler
        info.language = baseName;
        info.name = baseName + " Compiler";
        info.extensions = QStringList() << "." + baseName;
    }
    
    info.description = QString("PowerShell-based compiler for %1").arg(info.name);
    info.version = "1.0.0";
    info.outputType = "executable";
    
    return info;
}

QString PowerShellCompilerManager::buildPowerShellCommand(const CompilerInfo &compiler, const QString &source, 
                                                          const QString &outputPath, const QJsonObject &options)
{
    QString command;
    
    // Basic command structure
    command += QString("& '%1'")
                  .arg(compiler.scriptPath.replace("'", "''"));
    
    // Add source file parameter
    command += QString(" -SourceFile '%1'").arg(source.replace("'", "''"));
    
    // Add output path if specified
    if (!outputPath.isEmpty()) {
        command += QString(" -OutputPath '%1'").arg(outputPath.replace("'", "''"));
    }
    
    // Add options from JSON object
    for (auto it = options.constBegin(); it != options.constEnd(); ++it) {
        QString key = it.key();
        QJsonValue value = it.value();
        
        if (value.isString()) {
            command += QString(" -%1 '%2'").arg(key).arg(value.toString().replace("'", "''"));
        } else if (value.isDouble()) {
            command += QString(" -%1 %2").arg(key).arg(value.toDouble());
        } else if (value.isBool()) {
            command += QString(" -%1").arg(key);
        }
    }
    
    return command;
}

void PowerShellCompilerManager::cleanupProcess()
{
    if (m_currentProcess) {
        if (m_currentProcess->state() == QProcess::Running) {
            m_currentProcess->kill();
            m_currentProcess->waitForFinished(1000);
        }
        m_currentProcess->disconnect();
        m_currentProcess->deleteLater();
        m_currentProcess = nullptr;
    }
    
    m_currentLanguage.clear();
    m_currentSource.clear();
    m_currentOutputPath.clear();
    m_currentOptions = QJsonObject();
    m_timeoutTimer->stop();
}

void PowerShellCompilerManager::updateStats(bool success)
{
    m_totalCompilations++;
    
    if (success) {
        m_successfulCompilations++;
    } else {
        m_failedCompilations++;
    }
    
    m_compilationStats[m_currentLanguage]++;
}

PowerShellCompilerManager* getPowerShellCompilerManager()
{
    if (!g_compilerManager) {
        g_compilerManager = std::make_unique<PowerShellCompilerManager>();
    }
    return g_compilerManager.get();
}