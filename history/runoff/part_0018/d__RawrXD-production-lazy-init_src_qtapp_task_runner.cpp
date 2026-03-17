/**
 * @file task_runner.cpp
 * @brief Implementation of task runner abstraction
 */

#include "task_runner.hpp"
#include <QDateTime>
#include <QRegularExpression>
#include <QDebug>
#include <QJsonArray>

// ─────────────────────────────────────────────────────────────────────
// TaskRunner Implementation
// ─────────────────────────────────────────────────────────────────────

TaskRunner::TaskRunner(const QString& name, QObject* parent)
    : QObject(parent), m_name(name)
{
}

TaskRunner::~TaskRunner()
{
    if (m_process && m_process->state() == QProcess::Running) {
        m_process->terminate();
        if (!m_process->waitForFinished(3000)) {
            m_process->kill();
        }
    }
}

bool TaskRunner::start(const QString& workingDir, const QStringList& args)
{
    if (m_status != Status::Idle) {
        qWarning() << "[TaskRunner]" << m_name << "is already running";
        return false;
    }

    m_process = std::make_unique<QProcess>(this);
    m_process->setWorkingDirectory(workingDir);

    connect(m_process.get(), &QProcess::readyReadStandardOutput, this, &TaskRunner::onProcessStdout);
    connect(m_process.get(), &QProcess::readyReadStandardError, this, &TaskRunner::onProcessStderr);
    connect(m_process.get(), QOverload<int>::of(&QProcess::finished), this, &TaskRunner::onProcessFinished);
    connect(m_process.get(), QOverload<QProcess::ProcessError>::of(&QProcess::error),
            this, &TaskRunner::onProcessError);

    m_commandLine = executable() + " " + args.join(" ");
    m_output.clear();
    m_startTimeMs = QDateTime::currentMSecsSinceEpoch();

    setStatus(Status::Preparing);
    m_process->start(executable(), args);

    if (!m_process->waitForStarted()) {
        qCritical() << "[TaskRunner]" << m_name << "failed to start:" << m_process->errorString();
        setStatus(Status::Failed);
        return false;
    }

    setStatus(Status::Running);
    emit progress("Task started: " + m_commandLine);
    qInfo() << "[TaskRunner] Started:" << m_name << "cwd=" << workingDir;

    return true;
}

void TaskRunner::stop()
{
    if (!m_process || m_status == Status::Idle) {
        return;
    }

    qInfo() << "[TaskRunner]" << m_name << "stopping...";
    m_process->terminate();

    if (!m_process->waitForFinished(3000)) {
        qWarning() << "[TaskRunner]" << m_name << "force-killing...";
        m_process->kill();
    }

    setStatus(Status::Cancelled);
}

qint64 TaskRunner::elapsedMs() const
{
    if (m_startTimeMs == 0) {
        return 0;
    }
    return QDateTime::currentMSecsSinceEpoch() - m_startTimeMs;
}

QVector<TaskRunner::ParseResult> TaskRunner::parseOutput()
{
    QVector<ParseResult> results;
    for (const OutputLine& line : m_output) {
        ParseResult result = parseOutputLine(line.text);
        if (!result.code.isEmpty()) {
            results.append(result);
            emit issueFound(result);
        }
    }
    return results;
}

QJsonObject TaskRunner::toJSON() const
{
    QJsonObject obj;
    obj["name"] = m_name;
    obj["status"] = static_cast<int>(m_status);
    obj["exit_code"] = m_exitCode;
    obj["elapsed_ms"] = static_cast<int>(elapsedMs());
    obj["command_line"] = m_commandLine;

    QJsonArray outputArray;
    for (const OutputLine& line : m_output) {
        QJsonObject lineObj;
        lineObj["text"] = line.text;
        lineObj["level"] = line.level;
        lineObj["timestamp"] = static_cast<int>(line.timestamp);
        lineObj["is_stderr"] = line.isStderr;
        outputArray.append(lineObj);
    }
    obj["output"] = outputArray;

    return obj;
}

void TaskRunner::onProcessStdout()
{
    if (!m_process) return;

    QString output = QString::fromUtf8(m_process->readAllStandardOutput());
    for (const QString& line : output.split('\n')) {
        if (!line.trimmed().isEmpty()) {
            addOutput(line, "info", false);
        }
    }
}

void TaskRunner::onProcessStderr()
{
    if (!m_process) return;

    QString output = QString::fromUtf8(m_process->readAllStandardError());
    for (const QString& line : output.split('\n')) {
        if (!line.trimmed().isEmpty()) {
            addOutput(line, "error", true);

            // Try to parse error
            ParseResult result = parseOutputLine(line);
            if (!result.code.isEmpty()) {
                emit issueFound(result);
            }
        }
    }
}

void TaskRunner::onProcessFinished(int exitCode)
{
    m_exitCode = exitCode;
    Status newStatus = (exitCode == 0) ? Status::Completed : Status::Failed;
    setStatus(newStatus);
    emit finished(exitCode == 0);

    qInfo() << "[TaskRunner]" << m_name << "finished with exit code" << exitCode
            << "elapsed=" << elapsedMs() << "ms";
}

void TaskRunner::onProcessError(QProcess::ProcessError error)
{
    qCritical() << "[TaskRunner]" << m_name << "process error:" << error;
    setStatus(Status::Failed);
    if (m_process) {
        emit progress("Error: " + m_process->errorString());
    }
}

TaskRunner::ParseResult TaskRunner::parseOutputLine(const QString& line)
{
    // Default: no parsing
    return ParseResult();
}

void TaskRunner::setStatus(Status newStatus)
{
    if (m_status != newStatus) {
        Status oldStatus = m_status;
        m_status = newStatus;
        emit statusChanged(newStatus, oldStatus);
    }
}

void TaskRunner::addOutput(const QString& text, const QString& level, bool isStderr)
{
    OutputLine line;
    line.text = text;
    line.level = level;
    line.timestamp = QDateTime::currentMSecsSinceEpoch();
    line.isStderr = isStderr;
    m_output.append(line);
    emit outputLineReceived(line);
}

// ─────────────────────────────────────────────────────────────────────
// CMakeTaskRunner Implementation
// ─────────────────────────────────────────────────────────────────────

CMakeTaskRunner::CMakeTaskRunner(bool configure, bool build, QObject* parent)
    : TaskRunner("CMake", parent), m_configure(configure), m_build(build)
{
}

QString CMakeTaskRunner::executable() const
{
#ifdef Q_OS_WIN
    return "cmake.exe";
#else
    return "cmake";
#endif
}

TaskRunner::ParseResult CMakeTaskRunner::parseOutputLine(const QString& line)
{
    // CMake error pattern: CMake Error at filename:line message
    QRegularExpression re(R"(CMake Error at (.+):(\d+).*:\s*(.+))");
    auto match = re.match(line);

    if (match.hasMatch()) {
        ParseResult result;
        result.file = match.captured(1);
        result.line = match.captured(2).toInt();
        result.severity = "error";
        result.message = match.captured(3);
        result.source = "CMAKE";
        return result;
    }

    // CMake warning pattern
    re = QRegularExpression(R"(CMake Warning at (.+):(\d+).*:\s*(.+))");
    match = re.match(line);

    if (match.hasMatch()) {
        ParseResult result;
        result.file = match.captured(1);
        result.line = match.captured(2).toInt();
        result.severity = "warning";
        result.message = match.captured(3);
        result.source = "CMAKE";
        return result;
    }

    return ParseResult();
}

// ─────────────────────────────────────────────────────────────────────
// MSBuildTaskRunner Implementation
// ─────────────────────────────────────────────────────────────────────

MSBuildTaskRunner::MSBuildTaskRunner(const QString& solutionPath, QObject* parent)
    : TaskRunner("MSBuild", parent), m_solutionPath(solutionPath)
{
}

QString MSBuildTaskRunner::executable() const
{
    // Try to find MSBuild in VS2022/VS2019 installation
    QStringList paths = {
        "C:/Program Files (x86)/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe",
        "C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/MSBuild/Current/Bin/MSBuild.exe",
        "msbuild.exe"
    };

    for (const QString& path : paths) {
        if (QFile::exists(path)) {
            return path;
        }
    }

    return "msbuild.exe";
}

TaskRunner::ParseResult MSBuildTaskRunner::parseOutputLine(const QString& line)
{
    // MSBuild error pattern: filename(line,col): error MSXXXX: message
    QRegularExpression re(R"((.+?)\((\d+)(?:,(\d+))?\)\s*:\s*(error|warning)\s+(.*?)\s*:\s*(.+))");
    auto match = re.match(line);

    if (match.hasMatch()) {
        ParseResult result;
        result.file = match.captured(1);
        result.line = match.captured(2).toInt();
        result.column = match.captured(3).toInt();
        result.severity = match.captured(4);
        result.code = match.captured(5);
        result.message = match.captured(6);
        result.source = "MSBUILD";
        return result;
    }

    return ParseResult();
}

// ─────────────────────────────────────────────────────────────────────
// MASMTaskRunner Implementation
// ─────────────────────────────────────────────────────────────────────

MASMTaskRunner::MASMTaskRunner(const QString& sourceFile, QObject* parent)
    : TaskRunner("MASM", parent), m_sourceFile(sourceFile)
{
}

QString MASMTaskRunner::executable() const
{
#ifdef Q_OS_WIN
    // Try to find ml64.exe or ml.exe
    QStringList paths = {
        "C:/Program Files (x86)/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.*/bin/Hostx64/x64/ml64.exe",
        "C:/masm32/bin/ml.exe",
        "ml64.exe",
        "ml.exe"
    };

    for (const QString& pattern : paths) {
        // Simple path check (ideally use glob matching)
        if (pattern.contains("*")) {
            // Try the base path without wildcards
            QString basePath = pattern.left(pattern.indexOf("*"));
            if (!basePath.isEmpty()) {
                QDir dir(basePath);
                if (dir.exists()) {
                    return basePath + "ml64.exe";
                }
            }
        } else if (QFile::exists(pattern)) {
            return pattern;
        }
    }

    return "ml64.exe";
#else
    return "ml";
#endif
}

TaskRunner::ParseResult MASMTaskRunner::parseOutputLine(const QString& line)
{
    // MASM error pattern: filename.asm(line) : error MLxxxx: message
    QRegularExpression re(
        R"(([^\s(]+\.asm)\((\d+)(?:,(\d+))?\)\s*:\s*(error|warning|fatal error)\s+(ML\d+|LNK\d+):\s*(.+))");
    auto match = re.match(line);

    if (match.hasMatch()) {
        ParseResult result;
        result.file = match.captured(1);
        result.line = match.captured(2).toInt();
        result.column = match.captured(3).toInt();
        result.severity = match.captured(4);
        result.code = match.captured(5);
        result.message = match.captured(6);
        result.source = "MASM";
        return result;
    }

    return ParseResult();
}

// ─────────────────────────────────────────────────────────────────────
// GGUFTaskRunner Implementation
// ─────────────────────────────────────────────────────────────────────

GGUFTaskRunner::GGUFTaskRunner(const QString& modelPath, const QString& prompt, QObject* parent)
    : TaskRunner("GGUF", parent), m_modelPath(modelPath), m_prompt(prompt)
{
}

QString GGUFTaskRunner::executable() const
{
    // llama-cli or inference executable
#ifdef Q_OS_WIN
    return "llama-cli.exe";
#else
    return "llama-cli";
#endif
}

TaskRunner::ParseResult GGUFTaskRunner::parseOutputLine(const QString& line)
{
    // GGUF error pattern: Error: message or error pattern varies
    if (line.contains("Error:", Qt::CaseInsensitive) || 
        line.contains("Failed:", Qt::CaseInsensitive)) {
        ParseResult result;
        result.severity = "error";
        result.code = "GGUF_ERROR";
        result.message = line;
        result.source = "GGUF";
        return result;
    }

    return ParseResult();
}

// ─────────────────────────────────────────────────────────────────────
// CustomTaskRunner Implementation
// ─────────────────────────────────────────────────────────────────────

CustomTaskRunner::CustomTaskRunner(const QString& executable, const QString& name, QObject* parent)
    : TaskRunner(name, parent), m_executable(executable)
{
}

QString CustomTaskRunner::executable() const
{
    return m_executable;
}
