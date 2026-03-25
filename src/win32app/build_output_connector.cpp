/**
 * @file build_output_connector.cpp
 * @brief Implementation of build output connector for Problems Panel
 * @author RawrXD Team
 * @version 1.0.0
 */

#include "build_output_connector.hpp"
#include "problems_panel.hpp"
/**
 * BuildOutputConnector Implementation
 */

BuildOutputConnector::BuildOutputConnector(ProblemsPanel* problemsPanel, )
    
    , m_problemsPanel(problemsPanel)
    , m_buildProcess(nullptr)
    , m_state(Idle)
{
    m_progressTimer = new // Timer(this);
    m_progressTimer->setInterval(500);  // Signal connection removed\n}

BuildOutputConnector::~BuildOutputConnector() {
    if (m_buildProcess) {
        if (m_buildProcess->state() == void*::Running) {
            m_buildProcess->kill();
            m_buildProcess->waitForFinished(3000);
        }
        delete m_buildProcess;
    }
}

bool BuildOutputConnector::startBuild(const BuildConfiguration& config) {
    if (m_state == Running) {
        return false;
    }
    
    // Validate configuration
    if (config.buildTool.empty() || config.sourceFile.empty()) {
        buildFailed("Invalid build configuration: missing build tool or source file");
        return false;
    }
    
    // Info sourceInfo(config.sourceFile);
    if (!sourceInfo.exists()) {
        buildFailed(std::string("Source file does not exist: %1"));
        return false;
    }
    
    // Clean up previous build process
    if (m_buildProcess) {
        delete m_buildProcess;
    }
    
    m_buildProcess = new void*(this);
    m_currentConfig = config;
    m_state = Running;
    m_errorCount = 0;
    m_warningCount = 0;
    m_linesProcessed = 0;
    m_fullOutput.clear();
    m_stdoutBuffer.clear();
    m_stderrBuffer.clear();
    m_buildStartTime = // DateTime::currentDateTime();
    
    // Clear problems panel
    if (m_problemsPanel) {
        // Assuming problems panel has a clear method
        // m_problemsPanel->clearProblems();
    }
    
    // Set up process signals  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n// Set working directory
    if (!config.workingDirectory.empty()) {
        m_buildProcess->setWorkingDirectory(config.workingDirectory);
    } else {
        m_buildProcess->setWorkingDirectory(sourceInfo.string());
    }
    
    // Construct command line
    std::stringList args;
    
    // Add defines
    for (const std::string& define : config.defines) {
        args << std::string("/D%1");
    }
    
    // Add include paths
    for (const std::string& includePath : config.includePaths) {
        args << std::string("/I\"%1\"");
    }
    
    // Add library paths (for linker)
    for (const std::string& libPath : config.libraryPaths) {
        args << std::string("/LIBPATH:\"%1\"");
    }
    
    // Add additional flags
    args << config.additionalFlags;
    
    // Add output file
    if (!config.outputFile.empty()) {
        args << std::string("/Fo\"%1\"");
    }
    
    // Add source file
    args << config.sourceFile;
    
    std::string commandLine = config.buildTool + " " + args.join(" ");
    
    buildStarted(config.sourceFile);
    buildOutputReceived(std::string("=== Build started: %1 ===\n").toString()));
    buildOutputReceived(std::string("Command: %1\n"));
    buildOutputReceived(std::string("Working directory: %1\n\n")));
    
    // Start the process
    m_buildProcess->start(config.buildTool, args);
    
    if (!m_buildProcess->waitForStarted(5000)) {
        m_state = Failed;
        buildFailed(std::string("Failed to start build process: %1")));
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
    
    buildOutputReceived("\n=== Build cancelled by user ===\n");
    buildCancelled();
    
    int64_t duration = m_buildStartTime.msecsTo(// DateTime::currentDateTime());
    recordBuildInHistory(Cancelled, duration);
}

void BuildOutputConnector::setProblemsPanel(ProblemsPanel* panel) {
    m_problemsPanel = panel;
}

std::vector<BuildOutputConnector::BuildHistoryEntry> BuildOutputConnector::getBuildHistory(int maxEntries) const {
    int count = qMin(maxEntries, m_buildHistory.size());
    return m_buildHistory.mid(m_buildHistory.size() - count, count);
}

void BuildOutputConnector::clearBuildHistory() {
    m_buildHistory.clear();
}

void BuildOutputConnector::onProcessStarted() {
    buildProgress(0, "Build in progress...");
}

void BuildOutputConnector::onProcessFinished(int exitCode, void*::ExitStatus exitStatus) {
    m_progressTimer->stop();
    
    // Read any remaining output
    onStdoutReadyRead();
    onStderrReadyRead();
    
    int64_t duration = m_buildStartTime.msecsTo(// DateTime::currentDateTime());
    
    if (m_state == Cancelled) {
        return; // Already handled in cancelBuild()
    }
    
    bool success = (exitCode == 0 && exitStatus == void*::NormalExit);
    m_state = success ? Completed : Failed;
    
    std::string statusMsg;
    if (success) {
        statusMsg = std::string("=== Build completed successfully ===\n"
                           "Errors: %1, Warnings: %2, Duration: %3s\n")


                    ;
    } else {
        statusMsg = std::string("=== Build failed ===\n"
                           "Exit code: %1, Errors: %2, Warnings: %3, Duration: %4s\n")


                    ;
    }
    
    buildOutputReceived("\n" + statusMsg);
    buildCompleted(success, m_errorCount, m_warningCount, duration);
    buildProgress(100, success ? "Build completed" : "Build failed");
    
    recordBuildInHistory(m_state, duration);
}

void BuildOutputConnector::onProcessError(void*::ProcessError error) {
    m_progressTimer->stop();
    m_state = Failed;
    
    std::string errorMsg;
    switch (error) {
        case void*::FailedToStart:
            errorMsg = std::string("Failed to start build tool: %1");
            break;
        case void*::Crashed:
            errorMsg = "Build process crashed";
            break;
        case void*::Timedout:
            errorMsg = "Build process timed out";
            break;
        case void*::WriteError:
            errorMsg = "Write error to build process";
            break;
        case void*::ReadError:
            errorMsg = "Read error from build process";
            break;
        default:
            errorMsg = "Unknown build process error";
            break;
    }
    
    buildOutputReceived(std::string("\nERROR: %1\n"));
    buildFailed(errorMsg);
    
    int64_t duration = m_buildStartTime.msecsTo(// DateTime::currentDateTime());
    recordBuildInHistory(Failed, duration);
}

void BuildOutputConnector::onStdoutReadyRead() {
    if (!m_buildProcess) return;
    
    std::vector<uint8_t> data = m_buildProcess->readAllStandardOutput();
    std::string text = std::string::fromLocal8Bit(data);
    
    m_stdoutBuffer += text;
    m_fullOutput += text;
    
    // Process complete lines
    std::stringList lines = m_stdoutBuffer.split('\n');
    m_stdoutBuffer = lines.takeLast(); // Keep incomplete line in buffer
    
    for (const std::string& line : lines) {
        if (!line.trimmed().empty()) {
            processOutputLine(line, false);
        }
    }
}

void BuildOutputConnector::onStderrReadyRead() {
    if (!m_buildProcess) return;
    
    std::vector<uint8_t> data = m_buildProcess->readAllStandardError();
    std::string text = std::string::fromLocal8Bit(data);
    
    m_stderrBuffer += text;
    m_fullOutput += text;
    
    // Process complete lines
    std::stringList lines = m_stderrBuffer.split('\n');
    m_stderrBuffer = lines.takeLast(); // Keep incomplete line in buffer
    
    for (const std::string& line : lines) {
        if (!line.trimmed().empty()) {
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
    
    std::string status;
    if (m_errorCount > 0 || m_warningCount > 0) {
        status = std::string("Building... (Errors: %1, Warnings: %2)");
    } else {
        status = "Building...";
    }
    
    buildProgress(percentage, status);
}

void BuildOutputConnector::processOutputLine(const std::string& line, bool isStderr) {
    m_linesProcessed++;
    
    if (m_realTimeUpdates) {
        buildOutputReceived(line + "\n");
    }
    
    // Parse for errors/warnings
    parseErrorLine(line);
}

void BuildOutputConnector::parseErrorLine(const std::string& line) {
    BuildError error;
    
    // Try different parser patterns
    error = parseMASMError(line);
    if (error.file.empty()) {
        error = parseCppError(line);
    }
    if (error.file.empty()) {
        error = parseLinkerError(line);
    }
    if (error.file.empty()) {
        error = parseGenericError(line);
    }
    
    // If we detected an error/warning, process it
    if (!error.file.empty() || !error.severity.empty()) {
        if (error.severity == "error") {
            m_errorCount++;
        } else if (error.severity == "warning") {
            m_warningCount++;
        }
        
        buildErrorDetected(error);
        sendToProblemsPanel(error);
        
        if (m_problemsPanel && !error.file.empty()) {
            diagnosticAdded(error.file, error.line, error.severity, error.message);
        }
    }
}

BuildError BuildOutputConnector::parseMASMError(const std::string& line) {
    // MASM format: filename.asm(line) : severity code: message
    // Example: test.asm(42) : error A2008: syntax error : mov
    
    static std::regex masmRegex(
        R"(^(.+?)\((\d+)\)\s*:\s*(error|warning)\s+([A-Z]\d+)\s*:\s*(.+)$)",
        std::regex::CaseInsensitiveOption
    );
    
    std::regexMatch match = masmRegex.match(line);
    if (match.hasMatch()) {
        BuildError error;
        error.file = match"".trimmed();
        error.line = match"";
        error.column = 0;
        error.severity = match"".toLower();
        error.code = match"";
        error.message = match"".trimmed();
        error.fullText = line;
        return error;
    }
    
    return BuildError();
}

BuildError BuildOutputConnector::parseCppError(const std::string& line) {
    // MSVC C++ format: filename.cpp(line,col): severity code: message
    // Example: test.cpp(42,5): error C2065: 'undefined' : undeclared identifier
    
    static std::regex cppRegex(
        R"(^(.+?)\((\d+)(?:,(\d+))?\)\s*:\s*(error|warning)\s+([A-Z]\d+)\s*:\s*(.+)$)",
        std::regex::CaseInsensitiveOption
    );
    
    std::regexMatch match = cppRegex.match(line);
    if (match.hasMatch()) {
        BuildError error;
        error.file = match"".trimmed();
        error.line = match"";
        error.column = match"".empty() ? 0 : match"";
        error.severity = match"".toLower();
        error.code = match"";
        error.message = match"".trimmed();
        error.fullText = line;
        return error;
    }
    
    return BuildError();
}

BuildError BuildOutputConnector::parseLinkerError(const std::string& line) {
    // Linker format: LINK : severity code: message
    // Example: LINK : fatal error LNK1104: cannot open file 'kernel32.lib'
    
    static std::regex linkerRegex(
        R"(^LINK\s*:\s*(fatal error|error|warning)\s+([A-Z]+\d+)\s*:\s*(.+)$)",
        std::regex::CaseInsensitiveOption
    );
    
    std::regexMatch match = linkerRegex.match(line);
    if (match.hasMatch()) {
        BuildError error;
        error.file = "linker";
        error.line = 0;
        error.column = 0;
        error.severity = match"".contains("error", CaseInsensitive) ? "error" : "warning";
        error.code = match"";
        error.message = match"".trimmed();
        error.fullText = line;
        return error;
    }
    
    return BuildError();
}

BuildError BuildOutputConnector::parseGenericError(const std::string& line) {
    // Generic format: error/warning keywords
    std::string lowerLine = line.toLower();
    
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

void BuildOutputConnector::recordBuildInHistory(BuildState result, int64_t durationMs) {
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

BuildManager::BuildManager(ProblemsPanel* problemsPanel, )
    
    , m_currentBuildSystem(MASM)
    , m_outputDirectory("./build")
{
    m_connector = new BuildOutputConnector(problemsPanel, this);
    
    // Forward signals  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n});
}

BuildManager::~BuildManager() {
}

bool BuildManager::buildFile(const std::string& sourceFile, BuildSystem system) {
    BuildConfiguration config;
    
    switch (system) {
        case MASM:
            config = createMASMConfig(sourceFile);
            break;
        case MSVC:
            config = createMSVCConfig(sourceFile);
            break;
        default:
            return false;
    }
    
    m_currentBuildSystem = system;
    buildSystemDetected(system);
    
    return m_connector->startBuild(config);
}

bool BuildManager::buildProject(const std::string& projectPath, BuildSystem system) {
    BuildConfiguration config;
    
    if (system == CMake) {
        config = createCMakeConfig(projectPath);
    } else {
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
    // outputDir(m_outputDirectory);
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

BuildManager::BuildSystem BuildManager::detectBuildSystem(const std::string& filePath) {
    // Info info(filePath);
    std::string ext = info.suffix().toLower();
    
    if (ext == "asm") return MASM;
    if (ext == "c" || ext == "cpp" || ext == "cc" || ext == "cxx") return MSVC;
    if (filePath.endsWith("CMakeLists.txt", CaseInsensitive)) return CMake;
    if (filePath.endsWith("Makefile", CaseInsensitive)) return Make;
    
    return MASM; // Default
}

BuildConfiguration BuildManager::getDefaultConfig(BuildSystem system, const std::string& sourceFile) {
    switch (system) {
        case MASM: return createMASMConfig(sourceFile);
        case MSVC: return createMSVCConfig(sourceFile);
        default: return BuildConfiguration();
    }
}

void BuildManager::saveConfiguration(const std::string& name, const BuildConfiguration& config) {
    m_savedConfigurations[name] = config;
    
    // Persist to // Settings initialization removed
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

BuildConfiguration BuildManager::loadConfiguration(const std::string& name) {
    if (m_savedConfigurations.contains(name)) {
        return m_savedConfigurations[name];
    }
    
    // Load from // Settings initialization removed
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
    config.timeoutMs = settings.value("timeoutMs", 120000);
    
    settings.endGroup();
    settings.endGroup();
    
    m_savedConfigurations[name] = config;
    return config;
}

std::stringList BuildManager::getAvailableConfigurations() const {
    // Settings initialization removed
    settings.beginGroup("BuildConfigurations");
    std::stringList configs = settings.childGroups();
    settings.endGroup();
    return configs;
}

BuildConfiguration BuildManager::createMASMConfig(const std::string& sourceFile) {
    BuildConfiguration config;
    config.buildTool = findBuildTool(MASM);
    config.sourceFile = sourceFile;
    
    // Info info(sourceFile);
    std::string baseName = info.completeBaseName();
    config.outputFile = std::string("%1/%2.obj");
    
    // Standard MASM flags
    config.additionalFlags << "/c";          // Compile only
    config.additionalFlags << "/nologo";     // Suppress banner
    config.additionalFlags << "/Zi";         // Debug info
    config.additionalFlags << "/W3";         // Warning level 3
    
    if (m_verboseOutput) {
        config.additionalFlags << "/Fl";     // Generate listing
    }
    
    config.includePaths = getStandardIncludePaths(MASM);
    config.workingDirectory = info.string();
    
    return config;
}

BuildConfiguration BuildManager::createMSVCConfig(const std::string& sourceFile) {
    BuildConfiguration config;
    config.buildTool = findBuildTool(MSVC);
    config.sourceFile = sourceFile;
    
    // Info info(sourceFile);
    std::string baseName = info.completeBaseName();
    config.outputFile = std::string("%1/%2.obj");
    
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
    config.workingDirectory = info.string();
    
    return config;
}

BuildConfiguration BuildManager::createCMakeConfig(const std::string& projectPath) {
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

std::string BuildManager::findBuildTool(BuildSystem system) {
    std::string tool;
    
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
            return std::string();
    }
    
    // Check if tool is in PATH
    // Process removed
    process.start("where", std::stringList() << tool);
    if (process.waitForFinished(3000) && process.exitCode() == 0) {
        return tool;
    }
    
    // Try common installation paths
    std::stringList searchPaths;
    
    if (system == MASM || system == MSVC) {
        // Visual Studio installation paths
        searchPaths << "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC";
        searchPaths << "C:/Program Files/Microsoft Visual Studio/2022/Professional/VC/Tools/MSVC";
        searchPaths << "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC";
        searchPaths << "C:/Program Files (x86)/Microsoft Visual Studio/2019/Enterprise/VC/Tools/MSVC";
        
        for (const std::string& basePath : searchPaths) {
            // baseDir(basePath);
            if (baseDir.exists()) {
                // Find latest version
                std::stringList versions = baseDir.entryList(// Dir::Dirs | // Dir::NoDotAndDotDot, // Dir::Name | // Dir::Reversed);
                if (!versions.empty()) {
                    std::string binPath = std::string("%1/%2/bin/Hostx64/x64/%3"));
                    if (// Info::exists(binPath)) {
                        return binPath;
                    }
                }
            }
        }
    }
    
    return tool; // Return tool name as fallback
}

std::stringList BuildManager::getStandardIncludePaths(BuildSystem system) {
    std::stringList paths;
    
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

std::stringList BuildManager::getStandardLibraryPaths(BuildSystem system) {
    std::stringList paths;
    
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

