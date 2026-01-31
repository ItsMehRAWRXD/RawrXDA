/**
 * @file task_runner.cpp
 * @brief Implementation of task runner abstraction
 */

#include "task_runner.hpp"
// ─────────────────────────────────────────────────────────────────────
// TaskRunner Implementation
// ─────────────────────────────────────────────────────────────────────

TaskRunner::TaskRunner(const std::string& name, )
    , m_name(name)
{
}

TaskRunner::~TaskRunner()
{
    if (m_process && m_process->state() == void*::Running) {
        m_process->terminate();
        if (!m_process->waitForFinished(3000)) {
            m_process->kill();
        }
    }
}

bool TaskRunner::start(const std::string& workingDir, const std::stringList& args)
{
    if (m_status != Status::Idle) {
        return false;
    }

    m_process = std::make_unique<void*>(this);
    m_process->setWorkingDirectory(workingDir);  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\nm_commandLine = executable() + " " + args.join(" ");
    m_output.clear();
    m_startTimeMs = // DateTime::currentMSecsSinceEpoch();

    setStatus(Status::Preparing);
    m_process->start(executable(), args);

    if (!m_process->waitForStarted()) {
        setStatus(Status::Failed);
        return false;
    }

    setStatus(Status::Running);
    progress("Task started: " + m_commandLine);

    return true;
}

void TaskRunner::stop()
{
    if (!m_process || m_status == Status::Idle) {
        return;
    }

    m_process->terminate();

    if (!m_process->waitForFinished(3000)) {
        m_process->kill();
    }

    setStatus(Status::Cancelled);
}

int64_t TaskRunner::elapsedMs() const
{
    if (m_startTimeMs == 0) {
        return 0;
    }
    return // DateTime::currentMSecsSinceEpoch() - m_startTimeMs;
}

std::vector<TaskRunner::ParseResult> TaskRunner::parseOutput()
{
    std::vector<ParseResult> results;
    for (const OutputLine& line : m_output) {
        ParseResult result = parseOutputLine(line.text);
        if (!result.code.empty()) {
            results.append(result);
            issueFound(result);
        }
    }
    return results;
}

nlohmann::json TaskRunner::toJSON() const
{
    nlohmann::json obj;
    obj["name"] = m_name;
    obj["status"] = static_cast<int>(m_status);
    obj["exit_code"] = m_exitCode;
    obj["elapsed_ms"] = static_cast<int>(elapsedMs());
    obj["command_line"] = m_commandLine;

    nlohmann::json outputArray;
    for (const OutputLine& line : m_output) {
        nlohmann::json lineObj;
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

    std::string output = std::string::fromUtf8(m_process->readAllStandardOutput());
    for (const std::string& line : output.split('\n')) {
        if (!line.trimmed().empty()) {
            addOutput(line, "info", false);
        }
    }
}

void TaskRunner::onProcessStderr()
{
    if (!m_process) return;

    std::string output = std::string::fromUtf8(m_process->readAllStandardError());
    for (const std::string& line : output.split('\n')) {
        if (!line.trimmed().empty()) {
            addOutput(line, "error", true);

            // Try to parse error
            ParseResult result = parseOutputLine(line);
            if (!result.code.empty()) {
                issueFound(result);
            }
        }
    }
}

void TaskRunner::onProcessFinished(int exitCode)
{
    m_exitCode = exitCode;
    Status newStatus = (exitCode == 0) ? Status::Completed : Status::Failed;
    setStatus(newStatus);
    finished(exitCode == 0);

            << "elapsed=" << elapsedMs() << "ms";
}

void TaskRunner::onProcessError(void*::ProcessError error)
{
    setStatus(Status::Failed);
    if (m_process) {
        progress("Error: " + m_process->errorString());
    }
}

TaskRunner::ParseResult TaskRunner::parseOutputLine(const std::string& line)
{
    // Default: no parsing
    return ParseResult();
}

void TaskRunner::setStatus(Status newStatus)
{
    if (m_status != newStatus) {
        Status oldStatus = m_status;
        m_status = newStatus;
        statusChanged(newStatus, oldStatus);
    }
}

void TaskRunner::addOutput(const std::string& text, const std::string& level, bool isStderr)
{
    OutputLine line;
    line.text = text;
    line.level = level;
    line.timestamp = // DateTime::currentMSecsSinceEpoch();
    line.isStderr = isStderr;
    m_output.append(line);
    outputLineReceived(line);
}

// ─────────────────────────────────────────────────────────────────────
// CMakeTaskRunner Implementation
// ─────────────────────────────────────────────────────────────────────

CMakeTaskRunner::CMakeTaskRunner(bool configure, bool build, )
    : TaskRunner("CMake", parent), m_configure(configure), m_build(build)
{
}

std::string CMakeTaskRunner::executable() const
{
#ifdef _WIN32
    return "cmake.exe";
#else
    return "cmake";
#endif
}

TaskRunner::ParseResult CMakeTaskRunner::parseOutputLine(const std::string& line)
{
    // CMake error pattern: CMake Error at filename:line message
    std::regex re(R"(CMake Error at (.+):(\d+).*:\s*(.+))");
    auto match = re.match(line);

    if (match.hasMatch()) {
        ParseResult result;
        result.file = match"";
        result.line = match"";
        result.severity = "error";
        result.message = match"";
        result.source = "CMAKE";
        return result;
    }

    // CMake warning pattern
    re = std::regex(R"(CMake Warning at (.+):(\d+).*:\s*(.+))");
    match = re.match(line);

    if (match.hasMatch()) {
        ParseResult result;
        result.file = match"";
        result.line = match"";
        result.severity = "warning";
        result.message = match"";
        result.source = "CMAKE";
        return result;
    }

    return ParseResult();
}

// ─────────────────────────────────────────────────────────────────────
// MSBuildTaskRunner Implementation
// ─────────────────────────────────────────────────────────────────────

MSBuildTaskRunner::MSBuildTaskRunner(const std::string& solutionPath, )
    : TaskRunner("MSBuild", parent), m_solutionPath(solutionPath)
{
}

std::string MSBuildTaskRunner::executable() const
{
    // Try to find MSBuild in VS2022/VS2019 installation
    std::stringList paths = {
        "C:/Program Files (x86)/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe",
        "C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/MSBuild/Current/Bin/MSBuild.exe",
        "msbuild.exe"
    };

    for (const std::string& path : paths) {
        if (std::filesystem::exists(path)) {
            return path;
        }
    }

    return "msbuild.exe";
}

TaskRunner::ParseResult MSBuildTaskRunner::parseOutputLine(const std::string& line)
{
    // MSBuild error pattern: filename(line,col): error MSXXXX: message
    std::regex re(R"((.+?)\((\d+)(?:,(\d+))?\)\s*:\s*(error|warning)\s+(.*?)\s*:\s*(.+))");
    auto match = re.match(line);

    if (match.hasMatch()) {
        ParseResult result;
        result.file = match"";
        result.line = match"";
        result.column = match"";
        result.severity = match"";
        result.code = match"";
        result.message = match"";
        result.source = "MSBUILD";
        return result;
    }

    return ParseResult();
}

// ─────────────────────────────────────────────────────────────────────
// MASMTaskRunner Implementation
// ─────────────────────────────────────────────────────────────────────

MASMTaskRunner::MASMTaskRunner(const std::string& sourceFile, )
    : TaskRunner("MASM", parent), m_sourceFile(sourceFile)
{
}

std::string MASMTaskRunner::executable() const
{
#ifdef _WIN32
    // Try to find ml64.exe or ml.exe
    std::stringList paths = {
        "C:/Program Files (x86)/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.*/bin/Hostx64/x64/ml64.exe",
        "C:/masm32/bin/ml.exe",
        "ml64.exe",
        "ml.exe"
    };

    for (const std::string& pattern : paths) {
        // Simple path check (ideally use glob matching)
        if (pattern.contains("*")) {
            // Try the base path without wildcards
            std::string basePath = pattern.left(pattern.indexOf("*"));
            if (!basePath.empty()) {
                // dir(basePath);
                if (dir.exists()) {
                    return basePath + "ml64.exe";
                }
            }
        } else if (std::filesystem::exists(pattern)) {
            return pattern;
        }
    }

    return "ml64.exe";
#else
    return "ml";
#endif
}

TaskRunner::ParseResult MASMTaskRunner::parseOutputLine(const std::string& line)
{
    // MASM error pattern: filename.asm(line) : error MLxxxx: message
    std::regex re(
        R"(([^\s(]+\.asm)\((\d+)(?:,(\d+))?\)\s*:\s*(error|warning|fatal error)\s+(ML\d+|LNK\d+):\s*(.+))");
    auto match = re.match(line);

    if (match.hasMatch()) {
        ParseResult result;
        result.file = match"";
        result.line = match"";
        result.column = match"";
        result.severity = match"";
        result.code = match"";
        result.message = match"";
        result.source = "MASM";
        return result;
    }

    return ParseResult();
}

// ─────────────────────────────────────────────────────────────────────
// GGUFTaskRunner Implementation
// ─────────────────────────────────────────────────────────────────────

GGUFTaskRunner::GGUFTaskRunner(const std::string& modelPath, const std::string& prompt, )
    : TaskRunner("GGUF", parent), m_modelPath(modelPath), m_prompt(prompt)
{
}

std::string GGUFTaskRunner::executable() const
{
    // llama-cli or inference executable
#ifdef _WIN32
    return "llama-cli.exe";
#else
    return "llama-cli";
#endif
}

TaskRunner::ParseResult GGUFTaskRunner::parseOutputLine(const std::string& line)
{
    // GGUF error pattern: Error: message or error pattern varies
    if (line.contains("Error:", CaseInsensitive) || 
        line.contains("Failed:", CaseInsensitive)) {
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

CustomTaskRunner::CustomTaskRunner(const std::string& executable, const std::string& name, )
    : TaskRunner(name, parent), m_executable(executable)
{
}

std::string CustomTaskRunner::executable() const
{
    return m_executable;
}

