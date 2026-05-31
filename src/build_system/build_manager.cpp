#include "build_system/build_manager.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>
#include <thread>
#include <windows.h>

namespace RawrXD::BuildSystem {

BuildManager::BuildManager()
    : m_tool(BuildTool::CMake)
    , m_config(BuildConfiguration::Release)
    , m_parallelJobs(std::thread::hardware_concurrency())
{}

BuildManager::~BuildManager() {
    cancelBuild();
}

bool BuildManager::detectBuildSystem(const std::string& projectRoot) {
    namespace fs = std::filesystem;
    m_projectRoot = projectRoot;

    if (fs::exists(projectRoot + "/CMakeLists.txt")) {
        m_tool = BuildTool::CMake;
        return true;
    }
    if (fs::exists(projectRoot + "/Makefile") ||
        fs::exists(projectRoot + "/makefile")) {
        m_tool = BuildTool::Make;
        return true;
    }
    if (fs::exists(projectRoot + "/build.ninja")) {
        m_tool = BuildTool::Ninja;
        return true;
    }

    // Search for .sln files
    for (const auto& entry : fs::directory_iterator(projectRoot)) {
        if (entry.path().extension() == ".sln") {
            m_tool = BuildTool::MSBuild;
            return true;
        }
    }

    return false;
}

BuildTool BuildManager::getDetectedTool() const {
    return m_tool;
}

bool BuildManager::configure(const std::string& buildDirectory,
                            BuildConfiguration config,
                            const std::map<std::string, std::string>& options) {
    m_buildDirectory = buildDirectory;
    m_config = config;
    m_status = BuildStatus::Configuring;

    namespace fs = std::filesystem;
    fs::create_directories(buildDirectory);

    switch (m_tool) {
        case BuildTool::CMake:
            return configureCMake("Ninja", "");
        case BuildTool::MSBuild:
            return configureMSBuild("x64");
        case BuildTool::Ninja:
            return configureNinja("");
        default:
            m_status = BuildStatus::Failed;
            return false;
    }
}

bool BuildManager::configureCMake(const std::string& generator,
                                  const std::string& toolchain) {
    std::vector<std::string> args = {
        "-S", m_projectRoot,
        "-B", m_buildDirectory,
        "-G", generator,
        "-DCMAKE_BUILD_TYPE=" + buildConfigToString(m_config)
    };

    if (!toolchain.empty()) {
        args.push_back("-DCMAKE_TOOLCHAIN_FILE=" + toolchain);
    }

    return executeBuildCommand("cmake", args, m_projectRoot);
}

bool BuildManager::configureMSBuild(const std::string& platform) {
    // MSBuild doesn't require explicit configuration like CMake
    // Just verify the solution exists
    namespace fs = std::filesystem;
    for (const auto& entry : fs::directory_iterator(m_projectRoot)) {
        if (entry.path().extension() == ".sln") {
            return true;
        }
    }
    return false;
}

bool BuildManager::configureNinja(const std::string& toolchainFile) {
    // Ninja typically uses CMake to generate build files
    return configureCMake("Ninja", toolchainFile);
}

bool BuildManager::build(const std::string& target) {
    m_status = BuildStatus::Building;
    m_cancelled = false;
    m_diagnostics.clear();

    std::vector<std::string> args;
    std::string command;

    switch (m_tool) {
        case BuildTool::CMake:
            command = "cmake";
            args = {"--build", m_buildDirectory, "--config", buildConfigToString(m_config)};
            if (!target.empty()) {
                args.push_back("--target");
                args.push_back(target);
            }
            if (m_parallelJobs > 0) {
                args.push_back("--parallel");
                args.push_back(std::to_string(m_parallelJobs));
            }
            break;

        case BuildTool::MSBuild: {
            command = "msbuild";
            namespace fs = std::filesystem;
            for (const auto& entry : fs::directory_iterator(m_projectRoot)) {
                if (entry.path().extension() == ".sln") {
                    args.push_back(entry.path().string());
                    break;
                }
            }
            args.push_back("/p:Configuration=" + buildConfigToString(m_config));
            args.push_back("/m:" + std::to_string(m_parallelJobs));
            if (m_verbose) {
                args.push_back("/v:diag");
            }
            break;
        }

        case BuildTool::Ninja:
            command = "ninja";
            args.push_back("-C");
            args.push_back(m_buildDirectory);
            if (!target.empty()) {
                args.push_back(target);
            }
            if (m_parallelJobs > 0) {
                args.push_back("-j" + std::to_string(m_parallelJobs));
            }
            break;

        case BuildTool::Make:
            command = "make";
            if (!target.empty()) {
                args.push_back(target);
            }
            if (m_parallelJobs > 0) {
                args.push_back("-j" + std::to_string(m_parallelJobs));
            }
            break;

        default:
            m_status = BuildStatus::Failed;
            return false;
    }

    bool result = executeBuildCommand(command, args, m_buildDirectory);
    m_status = result ? BuildStatus::Completed : BuildStatus::Failed;
    return result;
}

bool BuildManager::clean() {
    if (m_tool == BuildTool::CMake) {
        return executeBuildCommand("cmake", {"--build", m_buildDirectory, "--target", "clean"}, m_buildDirectory);
    }
    // For other tools, just delete the build directory
    namespace fs = std::filesystem;
    return fs::remove_all(m_buildDirectory) > 0;
}

bool BuildManager::rebuild(const std::string& target) {
    clean();
    return build(target);
}

void BuildManager::cancelBuild() {
    m_cancelled = true;
}

bool BuildManager::executeBuildCommand(const std::string& command,
                                       const std::vector<std::string>& args,
                                       const std::string& workingDir) {
    std::string cmdLine = command;
    for (const auto& arg : args) {
        cmdLine += " " + arg;
    }

    SECURITY_ATTRIBUTES sa = {sizeof(sa), nullptr, TRUE};
    HANDLE hRead, hWrite;
    CreatePipe(&hRead, &hWrite, &sa, 0);
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {sizeof(si)};
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;

    PROCESS_INFORMATION pi = {};

    BOOL success = CreateProcessA(nullptr, cmdLine.data(), nullptr, nullptr, TRUE,
                                  CREATE_NO_WINDOW, nullptr, workingDir.c_str(), &si, &pi);

    CloseHandle(hWrite);

    if (!success) {
        CloseHandle(hRead);
        return false;
    }

    // Read output
    char buffer[4096];
    DWORD bytesRead;
    std::string output;

    while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        output += buffer;
        parseOutput(output);

        if (m_cancelled) {
            TerminateProcess(pi.hProcess, 1);
            break;
        }
    }

    CloseHandle(hRead);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return exitCode == 0 && !m_cancelled;
}

void BuildManager::parseOutput(const std::string& output) {
    std::istringstream stream(output);
    std::string line;

    while (std::getline(stream, line)) {
        BuildDiagnostic diag = parseDiagnostic(line);
        if (diag.severity != BuildDiagnostic::Info) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_diagnostics.push_back(diag);
            if (m_diagnosticCallback) {
                m_diagnosticCallback(diag);
            }
        }
    }
}

BuildDiagnostic BuildManager::parseDiagnostic(const std::string& line) {
    BuildDiagnostic diag;
    diag.severity = BuildDiagnostic::Info;
    diag.line = 0;
    diag.column = 0;

    // MSVC pattern: file(line): error/warning Ccode: message
    std::regex msvcPattern(R"((.+?)\((\d+)\)(?:\((\d+)\))?:\s*(error|warning)\s+(\w+\d+)?:\s*(.+))");
    std::smatch match;

    if (std::regex_match(line, match, msvcPattern)) {
        diag.filePath = match[1];
        diag.line = std::stoul(match[2]);
        if (match[3].matched) {
            diag.column = std::stoul(match[3]);
        }
        std::string severityStr = match[4];
        diag.severity = (severityStr == "error") ? BuildDiagnostic::Error : BuildDiagnostic::Warning;
        diag.code = match[5];
        diag.message = match[6];
        return diag;
    }

    // GCC/Clang pattern: file:line:column: severity: message
    std::regex gccPattern(R"((.+?):(\d+):(\d+):\s*(error|warning|note):\s*(.+))");
    if (std::regex_match(line, match, gccPattern)) {
        diag.filePath = match[1];
        diag.line = std::stoul(match[2]);
        diag.column = std::stoul(match[3]);
        std::string severityStr = match[4];
        if (severityStr == "error") diag.severity = BuildDiagnostic::Error;
        else if (severityStr == "warning") diag.severity = BuildDiagnostic::Warning;
        diag.message = match[5];
        return diag;
    }

    diag.message = line;
    return diag;
}

void BuildManager::setProgressCallback(ProgressCallback callback) {
    m_progressCallback = callback;
}

void BuildManager::setDiagnosticCallback(DiagnosticCallback callback) {
    m_diagnosticCallback = callback;
}

std::vector<BuildDiagnostic> BuildManager::getDiagnostics() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_diagnostics;
}

void BuildManager::clearDiagnostics() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_diagnostics.clear();
}

void BuildManager::setParallelJobs(uint32_t jobs) {
    m_parallelJobs = jobs;
}

void BuildManager::setVerboseOutput(bool verbose) {
    m_verbose = verbose;
}

// Global instance
BuildManager& getBuildManager() {
    static BuildManager instance;
    return instance;
}

std::string buildToolToString(BuildTool tool) {
    switch (tool) {
        case BuildTool::CMake: return "CMake";
        case BuildTool::MSBuild: return "MSBuild";
        case BuildTool::Ninja: return "Ninja";
        case BuildTool::Make: return "Make";
        case BuildTool::Custom: return "Custom";
        default: return "Unknown";
    }
}

std::string buildConfigToString(BuildConfiguration config) {
    switch (config) {
        case BuildConfiguration::Debug: return "Debug";
        case BuildConfiguration::Release: return "Release";
        case BuildConfiguration::RelWithDebInfo: return "RelWithDebInfo";
        case BuildConfiguration::MinSizeRel: return "MinSizeRel";
        default: return "Release";
    }
}

BuildConfiguration stringToBuildConfig(const std::string& str) {
    if (str == "Debug") return BuildConfiguration::Debug;
    if (str == "Release") return BuildConfiguration::Release;
    if (str == "RelWithDebInfo") return BuildConfiguration::RelWithDebInfo;
    if (str == "MinSizeRel") return BuildConfiguration::MinSizeRel;
    return BuildConfiguration::Release;
}

} // namespace RawrXD::BuildSystem
