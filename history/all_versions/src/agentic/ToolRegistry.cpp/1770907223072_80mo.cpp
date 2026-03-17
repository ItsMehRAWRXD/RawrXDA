// =============================================================================
// ToolRegistry.cpp — X-Macro Tool Registry Implementation
// =============================================================================
#include "ToolRegistry.h"
#include <sstream>
#include <chrono>
#include <filesystem>
#include <fstream>

#include "rawrxd_subsystem_api.hpp"
#include "../core/unified_hotpatch_manager.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <poll.h>
#include <signal.h>
#endif

// Windows <wingdi.h> defines ERROR as 0 which clashes with LogLevel::ERROR
#ifdef ERROR
#undef ERROR
#endif
#include "agentic_observability.h"
#include "DiskRecoveryAgent.h"

static AgenticObservability& GetObs() {
    static AgenticObservability instance;
    return instance;
}
static const char* kRegistryComponent = "ToolRegistry";

using RawrXD::Agent::AgentToolRegistry;
using RawrXD::Agent::ToolExecResult;
using RawrXD::Agent::ToolDescriptor;

namespace {

// -----------------------------------------------------------------------
// Default tool handlers (stubs — real implementations wire into engine)
// -----------------------------------------------------------------------

ToolExecResult HandleReadFile(const json& args) {
    std::string path = args.value("path", "");
    if (path.empty()) return ToolExecResult::error("Missing required parameter: path");

    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) return ToolExecResult::error("Failed to open file: " + path);

    std::ostringstream oss;
    oss << ifs.rdbuf();
    return ToolExecResult::ok(oss.str());
}

ToolExecResult HandleWriteFile(const json& args) {
    std::string path = args.value("path", "");
    std::string content = args.value("content", "");
    if (path.empty()) return ToolExecResult::error("Missing required parameter: path");

    // Ensure parent directories exist
    std::filesystem::path p(path);
    if (p.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
    }

    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) return ToolExecResult::error("Failed to create file: " + path);

    ofs.write(content.data(), content.size());
    ofs.close();
    return ToolExecResult::ok("File written: " + path + " (" + std::to_string(content.size()) + " bytes)");
}

ToolExecResult HandleReplaceInFile(const json& args) {
    std::string path = args.value("path", "");
    std::string oldStr = args.value("old_string", "");
    std::string newStr = args.value("new_string", "");
    if (path.empty() || oldStr.empty())
        return ToolExecResult::error("Missing required parameters: path, old_string");

    // Read
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) return ToolExecResult::error("Failed to open file: " + path);
    std::string content((std::istreambuf_iterator<char>(ifs)),
                         std::istreambuf_iterator<char>());
    ifs.close();

    // Find & replace (first occurrence)
    size_t pos = content.find(oldStr);
    if (pos == std::string::npos)
        return ToolExecResult::error("old_string not found in " + path);

    content.replace(pos, oldStr.size(), newStr);

    // Write
    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) return ToolExecResult::error("Failed to write file: " + path);
    ofs.write(content.data(), content.size());
    ofs.close();

    return ToolExecResult::ok("Replaced in " + path + " at offset " + std::to_string(pos));
}

ToolExecResult HandleExecuteCommand(const json& args) {
    std::string command = args.value("command", "");
    int timeout_ms = args.value("timeout", 30000);
    if (command.empty()) return ToolExecResult::error("Missing required parameter: command");

#ifdef _WIN32
    // CreateProcess with captured output
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hReadPipe = nullptr, hWritePipe = nullptr;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
        return ToolExecResult::error("Failed to create pipe");

    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;

    PROCESS_INFORMATION pi{};
    std::string cmdline = "cmd.exe /c " + command;

    auto start = std::chrono::high_resolution_clock::now();

    BOOL created = CreateProcessA(
        nullptr, cmdline.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);

    CloseHandle(hWritePipe);

    if (!created) {
        CloseHandle(hReadPipe);
        return ToolExecResult::error("CreateProcess failed: " + std::to_string(GetLastError()));
    }

    // Read output
    std::string output;
    char buffer[4096];
    DWORD bytesRead;
    while (ReadFile(hReadPipe, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0) {
        output.append(buffer, bytesRead);
        // Truncate if excessively large
        if (output.size() > 1024 * 1024) {
            output += "\n... [output truncated at 1MB]";
            break;
        }
    }
    CloseHandle(hReadPipe);

    DWORD waitResult = WaitForSingleObject(pi.hProcess, static_cast<DWORD>(timeout_ms));
    DWORD exitCode = 0;
    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return ToolExecResult::error("Command timed out after " + std::to_string(timeout_ms) + "ms\n" + output, 1);
    }

    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(end - start).count();

    ToolExecResult result;
    result.success = (exitCode == 0);
    result.output = output;
    result.exit_code = static_cast<int>(exitCode);
    result.elapsed_ms = elapsed;
    return result;
#else
    // POSIX: fork/exec with pipe capture
    int pipefd[2];
    if (pipe(pipefd) != 0)
        return ToolExecResult::error("Failed to create pipe");

    auto start = std::chrono::high_resolution_clock::now();

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return ToolExecResult::error("fork() failed");
    }

    if (pid == 0) {
        // Child
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);
        _exit(127);
    }

    // Parent
    close(pipefd[1]);

    std::string output;
    char buffer[4096];
    ssize_t bytesRead;

    // Non-blocking read with timeout
    struct pollfd pfd;
    pfd.fd = pipefd[0];
    pfd.events = POLLIN;

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (true) {
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()).count();
        if (remaining <= 0) {
            kill(pid, SIGKILL);
            waitpid(pid, nullptr, 0);
            close(pipefd[0]);
            return ToolExecResult::error("Command timed out after " + std::to_string(timeout_ms) + "ms\n" + output, 1);
        }

        int ready = poll(&pfd, 1, static_cast<int>(std::min(remaining, (long long)1000)));
        if (ready > 0) {
            bytesRead = read(pipefd[0], buffer, sizeof(buffer) - 1);
            if (bytesRead <= 0) break;
            buffer[bytesRead] = '\0';
            output += buffer;
            if (output.size() > 1024 * 1024) {
                output += "\n... [output truncated at 1MB]";
                break;
            }
        } else if (ready == 0) {
            // Check if child exited
            int status;
            pid_t wp = waitpid(pid, &status, WNOHANG);
            if (wp == pid) break;
        } else {
            break;
        }
    }
    close(pipefd[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : 1;

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(end - start).count();

    ToolExecResult result;
    result.success = (exitCode == 0);
    result.output = output;
    result.exit_code = exitCode;
    result.elapsed_ms = elapsed;
    return result;
#endif
}

ToolExecResult HandleSearchCode(const json& args) {
    std::string query = args.value("query", "");
    std::string pattern = args.value("file_pattern", "*.*");
    bool is_regex = args.value("is_regex", false);
    if (query.empty()) return ToolExecResult::error("Missing required parameter: query");

    std::ostringstream results;
    int matchCount = 0;
    const int maxMatches = 50;

    // Build glob extension filter from pattern
    std::vector<std::string> extensions;
    if (pattern != "*.*" && pattern != "*") {
        // Extract extension from pattern like "*.cpp" or "*.{cpp,h}"
        auto dotPos = pattern.find('.');
        if (dotPos != std::string::npos) {
            std::string ext = pattern.substr(dotPos);
            extensions.push_back(ext);
        }
    }

    auto matchesPattern = [&](const std::filesystem::path& p) -> bool {
        if (extensions.empty()) return true;
        std::string ext = p.extension().string();
        for (const auto& e : extensions) {
            if (ext == e) return true;
        }
        return false;
    };

    // Recursive search through current directory
    std::error_code ec;
    for (auto& entry : std::filesystem::recursive_directory_iterator(".", ec)) {
        if (!entry.is_regular_file()) continue;
        if (!matchesPattern(entry.path())) continue;
        if (matchCount >= maxMatches) {
            results << "\n... (truncated at " << maxMatches << " matches)\n";
            break;
        }

        // Read file and search for query
        std::ifstream ifs(entry.path(), std::ios::binary);
        if (!ifs.is_open()) continue;

        std::string line;
        int lineNum = 0;
        while (std::getline(ifs, line)) {
            lineNum++;
            size_t pos = std::string::npos;
            if (is_regex) {
                // Simple substring match as regex fallback
                pos = line.find(query);
            } else {
                pos = line.find(query);
            }

            if (pos != std::string::npos) {
                results << entry.path().string() << ":" << lineNum << ": ";
                // Truncate long lines
                if (line.size() > 200) {
                    size_t start = (pos > 80) ? pos - 80 : 0;
                    results << "..." << line.substr(start, 200) << "...";
                } else {
                    results << line;
                }
                results << "\n";
                matchCount++;
                if (matchCount >= maxMatches) break;
            }
        }
    }

    if (matchCount == 0) {
        results << "No matches found for \"" << query << "\"";
        if (!extensions.empty()) results << " in " << pattern << " files";
        results << "\n";
    } else {
        results << "\n" << matchCount << " match(es) found.\n";
    }

    return ToolExecResult::ok(results.str());
}

ToolExecResult HandleGetDiagnostics(const json& args) {
    std::string file = args.value("file", "");

    auto& registry = SubsystemRegistry::instance();

    // Attempt to retrieve diagnostics from LSP subsystem
    if (registry.isAvailable(SubsystemId::LSPDiagnostics)) {
        SubsystemParams params{};
        params.id = SubsystemId::LSPDiagnostics;
        SubsystemResult result = registry.invoke(params);
        if (result.success && result.detail) {
            return ToolExecResult::ok(result.detail);
        }
    }

    // Fallback: run compiler and parse output for diagnostics
    if (!file.empty()) {
        // Check if file is a C++ source — run a quick syntax check
        std::filesystem::path p(file);
        std::string ext = p.extension().string();
        if (ext == ".cpp" || ext == ".hpp" || ext == ".h" || ext == ".cc") {
            // Run cl.exe /Zs (syntax check only) to capture diagnostics
            json execArgs;
            execArgs["command"] = "cl.exe /Zs /EHsc /std:c++20 /W4 \"" + file + "\" 2>&1";
            execArgs["timeout"] = 15000;
            auto r = HandleExecuteCommand(execArgs);
            if (!r.output.empty()) {
                return ToolExecResult::ok("[diagnostics] " + file + ":\n" + r.output);
            }
        }
    }

    return ToolExecResult::ok("[get_diagnostics] No diagnostics available for: " +
                              (file.empty() ? "(all files)" : file));
}

ToolExecResult HandleListDirectory(const json& args) {
    std::string path = args.value("path", ".");
    bool recursive = args.value("recursive", false);
    if (path.empty()) path = ".";

    std::ostringstream oss;
    std::error_code ec;

    if (recursive) {
        for (auto& entry : std::filesystem::recursive_directory_iterator(path, ec)) {
            oss << (entry.is_directory() ? "[DIR]  " : "[FILE] ")
                << entry.path().string() << "\n";
        }
    } else {
        for (auto& entry : std::filesystem::directory_iterator(path, ec)) {
            oss << (entry.is_directory() ? "[DIR]  " : "[FILE] ")
                << entry.path().filename().string() << "\n";
        }
    }

    if (ec) return ToolExecResult::error("Directory listing failed: " + ec.message());
    return ToolExecResult::ok(oss.str());
}

ToolExecResult HandleGetCoverage(const json& args) {
    std::string file = args.value("file", "");
    std::string func = args.value("function_name", "");
    std::string mode = args.value("mode", "diffcov"); // "bbcov" or "diffcov"

    auto& registry = SubsystemRegistry::instance();

    // Dispatch to BBCov (basic-block coverage) or DiffCov (differential coverage)
    SubsystemId targetMode = (mode == "bbcov")
        ? SubsystemId::BBCov
        : SubsystemId::DiffCov;

    if (!registry.isAvailable(targetMode)) {
        return ToolExecResult::error("[get_coverage] " + mode + " subsystem not available");
    }

    SubsystemParams params{};
    params.id = targetMode;
    SubsystemResult result = registry.invoke(params);

    if (!result.success) {
        return ToolExecResult::error(
            std::string("[get_coverage] ") + mode + " failed: " + (result.detail ? result.detail : "unknown error"),
            result.errorCode);
    }

    std::ostringstream oss;
    oss << "[get_coverage] " << mode << " analysis complete";
    if (result.artifactPath) {
        oss << " — artifact: " << result.artifactPath;
    }
    oss << " (" << result.latencyMs << "ms)";
    if (!file.empty()) oss << " [filter: " << file << "]";
    if (!func.empty()) oss << " [function: " << func << "]";

    return ToolExecResult::ok(oss.str());
}

ToolExecResult HandleRunBuild(const json& args) {
    std::string target = args.value("target", "all");
    std::string config = args.value("config", "Release");

    std::string cmd = "cmake --build . --config " + config;
    if (target != "all") cmd += " --target " + target;

    json execArgs;
    execArgs["command"] = cmd;
    execArgs["timeout"] = 120000;
    return HandleExecuteCommand(execArgs);
}

ToolExecResult HandleApplyHotpatch(const json& args) {
    std::string layer = args.value("layer", "");
    std::string target = args.value("target", "");
    std::string data = args.value("data", "");
    if (layer.empty() || target.empty())
        return ToolExecResult::error("Missing required parameters: layer, target");

    auto& manager = UnifiedHotpatchManager::instance();
    UnifiedResult ur;

    if (layer == "memory") {
        // Parse target as hex address, data as hex bytes
        uintptr_t addr = 0;
        sscanf(target.c_str(), "%llx", &addr);
        if (addr == 0)
            return ToolExecResult::error("Invalid memory address: " + target);

        // Decode hex data string into byte array
        std::vector<uint8_t> bytes;
        for (size_t i = 0; i + 1 < data.size(); i += 2) {
            uint8_t byte = 0;
            sscanf(data.c_str() + i, "%2hhx", &byte);
            bytes.push_back(byte);
        }
        if (bytes.empty())
            return ToolExecResult::error("No patch data provided");

        ur = manager.apply_memory_patch(reinterpret_cast<void*>(addr),
                                         bytes.size(), bytes.data());
    }
    else if (layer == "byte") {
        // target = filename, data = hex-encoded patch
        std::vector<uint8_t> pattern, replacement;
        // data format: "PATTERN:REPLACEMENT" in hex
        auto colonPos = data.find(':');
        if (colonPos == std::string::npos)
            return ToolExecResult::error("byte layer data must be PATTERN_HEX:REPLACEMENT_HEX");

        std::string patternHex = data.substr(0, colonPos);
        std::string replaceHex = data.substr(colonPos + 1);

        for (size_t i = 0; i + 1 < patternHex.size(); i += 2) {
            uint8_t b; sscanf(patternHex.c_str() + i, "%2hhx", &b);
            pattern.push_back(b);
        }
        for (size_t i = 0; i + 1 < replaceHex.size(); i += 2) {
            uint8_t b; sscanf(replaceHex.c_str() + i, "%2hhx", &b);
            replacement.push_back(b);
        }

        ur = manager.apply_byte_search_patch(target.c_str(), pattern, replacement);
    }
    else if (layer == "server") {
        // Server patches are registered by name, not via hex data
        // target = patch name to add/remove
        if (data == "remove") {
            ur = manager.remove_server_patch(target.c_str());
        } else {
            return ToolExecResult::error(
                "Server patches must be registered programmatically. "
                "Use target=name, data=remove to remove.");
        }
    }
    else {
        return ToolExecResult::error("Unknown layer: " + layer +
                                     ". Valid: memory, byte, server");
    }

    if (!ur.result.success) {
        return ToolExecResult::error(
            std::string("[apply_hotpatch] ") + layer + " layer failed: " +
            (ur.result.detail ? ur.result.detail : "unknown error"),
            ur.result.errorCode);
    }

    std::ostringstream oss;
    oss << "[apply_hotpatch] " << layer << " layer applied successfully"
        << " | target=" << target
        << " | seq=" << ur.sequenceId;
    return ToolExecResult::ok(oss.str());
}

ToolExecResult HandleDiskRecovery(const json& args) {
    std::string action = args.value("action", "");
    if (action.empty()) return ToolExecResult::error("Missing required parameter: action");

    // Thread-local agent instance (persistent across calls within a session)
    static RawrXD::Recovery::DiskRecoveryAsmAgent agent;

    if (action == "scan" || action == "find") {
        int driveNum = agent.FindDrive();
        if (driveNum < 0)
            return ToolExecResult::error("No dying WD device found on PhysicalDrive0-15");
        return ToolExecResult::ok("Found candidate: PhysicalDrive" + std::to_string(driveNum));
    }
    else if (action == "init" || action == "initialize") {
        int driveNum = args.value("drive", -1);
        if (driveNum < 0)
            return ToolExecResult::error("Missing parameter: drive (0-15)");
        auto r = agent.Initialize(driveNum);
        if (!r.success) return ToolExecResult::error(r.detail, r.errorCode);
        return ToolExecResult::ok(r.detail);
    }
    else if (action == "extract_key" || action == "key") {
        auto r = agent.ExtractEncryptionKey();
        if (!r.success) return ToolExecResult::error(r.detail, r.errorCode);
        return ToolExecResult::ok(r.detail);
    }
    else if (action == "run" || action == "recover") {
        auto r = agent.RunRecovery();
        if (!r.success) return ToolExecResult::error(r.detail, r.errorCode);
        return ToolExecResult::ok(r.detail);
    }
    else if (action == "abort" || action == "stop") {
        agent.Abort();
        return ToolExecResult::ok("Abort signal sent to recovery worker");
    }
    else if (action == "stats" || action == "status") {
        auto stats = agent.GetStats();
        std::ostringstream oss;
        oss << "Good: " << stats.goodSectors
            << " | Bad: " << stats.badSectors
            << " | Current LBA: " << stats.currentLBA
            << " / " << stats.totalSectors
            << " (" << static_cast<int>(stats.ProgressPercent()) << "%)";
        return ToolExecResult::ok(oss.str());
    }
    else if (action == "cleanup" || action == "close") {
        // Destructor handles cleanup via RAII, but explicit reset:
        agent = RawrXD::Recovery::DiskRecoveryAsmAgent();
        return ToolExecResult::ok("Recovery context cleaned up");
    }
    else {
        return ToolExecResult::error("Unknown action: " + action +
            ". Valid: scan, init, extract_key, run, abort, stats, cleanup");
    }
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// AgentToolRegistry implementation
// ---------------------------------------------------------------------------

AgentToolRegistry& AgentToolRegistry::Instance() {
    static AgentToolRegistry instance;
    return instance;
}

AgentToolRegistry::AgentToolRegistry() {
    InitDescriptors();
}

void AgentToolRegistry::InitDescriptors() {
    // Use X-Macro to populate basic name/description
    #define INIT_DESCRIPTOR(tool_name_, tool_desc_) \
    { \
        ToolDescriptor td; \
        td.name = #tool_name_; \
        td.description = tool_desc_; \
        td.params_schema = json::object(); \
        td.handler = nullptr; \
        m_tools.push_back(std::move(td)); \
        m_nameIndex[#tool_name_] = m_tools.size() - 1; \
    }

    AGENT_TOOLS_X(INIT_DESCRIPTOR)
    #undef INIT_DESCRIPTOR

    // -----------------------------------------------------------------------
    // Wire parameter schemas programmatically (avoids preprocessor comma issue)
    // -----------------------------------------------------------------------
    auto setParam = [this](const char* tool, const char* param,
                           const char* type, const char* desc) {
        auto it = m_nameIndex.find(tool);
        if (it != m_nameIndex.end()) {
            json p;
            p["type"] = type;
            p["description"] = desc;
            m_tools[it->second].params_schema[param] = p;
        }
    };
    auto setParamWithDefault = [this](const char* tool, const char* param,
                                       const char* type, const char* desc,
                                       json defaultVal) {
        auto it = m_nameIndex.find(tool);
        if (it != m_nameIndex.end()) {
            json p;
            p["type"] = type;
            p["description"] = desc;
            p["default"] = defaultVal;
            m_tools[it->second].params_schema[param] = p;
        }
    };

    // read_file
    setParam("read_file", "path", "string", "Absolute or project-relative path to the file");

    // write_file
    setParam("write_file", "path", "string", "Target file path");
    setParam("write_file", "content", "string", "Full file content to write");

    // replace_in_file
    setParam("replace_in_file", "path", "string", "File to modify");
    setParam("replace_in_file", "old_string", "string", "Exact text to find (include context lines)");
    setParam("replace_in_file", "new_string", "string", "Replacement text");

    // execute_command
    setParam("execute_command", "command", "string", "Shell command to execute");
    setParamWithDefault("execute_command", "timeout", "number", "Timeout in milliseconds (default 30000)", 30000);

    // search_code
    setParam("search_code", "query", "string", "Search pattern (regex or literal)");
    setParamWithDefault("search_code", "file_pattern", "string", "Glob filter (default: *.*)", "*.*");
    setParamWithDefault("search_code", "is_regex", "boolean", "Treat query as regex", false);

    // get_diagnostics
    setParam("get_diagnostics", "file", "string", "File path, or empty for all diagnostics");

    // list_directory
    setParam("list_directory", "path", "string", "Directory path to list");
    setParamWithDefault("list_directory", "recursive", "boolean", "Recurse into subdirectories", false);

    // get_coverage
    setParam("get_coverage", "file", "string", "Source file to query coverage for");
    setParam("get_coverage", "function_name", "string", "Optional: specific function to check coverage");

    // run_build
    setParamWithDefault("run_build", "target", "string", "Build target (default: all)", "all");
    setParamWithDefault("run_build", "config", "string", "Build configuration (Release/Debug)", "Release");

    // apply_hotpatch
    setParam("apply_hotpatch", "layer", "string", "Patch layer: memory, byte, or server");
    setParam("apply_hotpatch", "target", "string", "Target address, file, or endpoint");
    setParam("apply_hotpatch", "data", "string", "Patch payload (hex-encoded for memory/byte)");

    // disk_recovery
    setParam("disk_recovery", "action", "string",
             "Action to perform: scan, init, extract_key, run, abort, stats, cleanup");
    setParamWithDefault("disk_recovery", "drive", "number",
                         "Physical drive number (0-15) for init action", -1);

    // Wire default handlers
    RegisterHandler("read_file",       HandleReadFile);
    RegisterHandler("write_file",      HandleWriteFile);
    RegisterHandler("replace_in_file", HandleReplaceInFile);
    RegisterHandler("execute_command", HandleExecuteCommand);
    RegisterHandler("search_code",     HandleSearchCode);
    RegisterHandler("get_diagnostics", HandleGetDiagnostics);
    RegisterHandler("list_directory",  HandleListDirectory);
    RegisterHandler("get_coverage",    HandleGetCoverage);
    RegisterHandler("run_build",       HandleRunBuild);
    RegisterHandler("apply_hotpatch",  HandleApplyHotpatch);
    RegisterHandler("disk_recovery",   HandleDiskRecovery);
}

json AgentToolRegistry::GetToolSchemas() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    json tools = json::array();

    for (const auto& td : m_tools) {
        // Build required array from params that don't have defaults
        json required_params = json::array();
        json params_copy = td.params_schema; // non-const copy for iteration
        for (auto it = params_copy.begin(); it != params_copy.end(); ++it) {
            const std::string& key = it.key();
            auto& val = it.value();
            if (!val.contains("default")) {
                required_params.push_back(key);
            }
        }

        tools.push_back({
            {"type", "function"},
            {"function", {
                {"name", td.name},
                {"description", td.description},
                {"parameters", {
                    {"type", "object"},
                    {"properties", td.params_schema},
                    {"required", required_params}
                }}
            }}
        });
    }
    return tools;
}

std::string AgentToolRegistry::GetSystemPrompt(
    const std::string& cwd,
    const std::vector<std::string>& openFiles) const
{
    std::ostringstream ss;
    ss << "You are RawrXD Agent, a local high-performance coding assistant with "
       << "compiler-aware tool access and SIMD-accelerated code search.\n\n"
       << "## Environment\n"
       << "- Working Directory: " << cwd << "\n"
       << "- Platform: Windows (MSVC 2022 / CMake 3.20+)\n"
       << "- Language: C++20 + MASM64\n\n";

    if (!openFiles.empty()) {
        ss << "## Open Files\n";
        for (const auto& f : openFiles) ss << "- " << f << "\n";
        ss << "\n";
    }

    ss << "## Available Tools\n"
       << "You have " << m_tools.size() << " tools available via function calling.\n"
       << "Use the tool schemas provided in the API request.\n\n"
       << "## Rules\n"
       << "1. Always read_file before editing to get current content.\n"
       << "2. Use replace_in_file for surgical edits; write_file for new files.\n"
       << "3. Run run_build after code changes to verify compilation.\n"
       << "4. Use get_diagnostics to check for errors after builds.\n"
       << "5. Use get_coverage to verify your changes touch the right logic paths.\n"
       << "6. Use search_code to find relevant code before making changes.\n"
       << "7. All results follow PatchResult pattern: check .success before proceeding.\n"
       << "8. No exceptions. Handle all errors via return values.\n";

    return ss.str();
}

ToolExecResult AgentToolRegistry::Dispatch(const std::string& tool_name, const json& args) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_nameIndex.find(tool_name);
    if (it == m_nameIndex.end()) {
        GetObs().logWarn(kRegistryComponent, "Dispatch: unknown tool", {{"tool", tool_name}});
        GetObs().incrementCounter("tool_registry.unknown_tool");
        return ToolExecResult::error("Unknown tool: " + tool_name);
    }

    auto& td = m_tools[it->second];
    if (!td.handler) {
        GetObs().logError(kRegistryComponent, "Dispatch: no handler", {{"tool", tool_name}});
        return ToolExecResult::error("No handler registered for tool: " + tool_name);
    }

    // Validate args
    std::string validationError;
    if (!ValidateArgs(tool_name, args, validationError)) {
        ++td.error_count;
        GetObs().logWarn(kRegistryComponent, "Dispatch: validation failed", {
            {"tool", tool_name}, {"error", validationError}
        });
        GetObs().incrementCounter("tool_registry.validation_failures");
        return ToolExecResult::error("Validation failed: " + validationError);
    }

    // Traced execution with timing
    auto spanId = GetObs().startSpan("tool_dispatch:" + tool_name);
    GetObs().logDebug(kRegistryComponent, "Dispatching tool call", {{"tool", tool_name}});

    auto start = std::chrono::high_resolution_clock::now();
    ToolExecResult result = td.handler(args);
    auto end = std::chrono::high_resolution_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    ++td.invocation_count;
    if (!result.success) {
        ++td.error_count;
        GetObs().endSpan(spanId, true, result.output);
        GetObs().logWarn(kRegistryComponent, "Tool call failed", {
            {"tool", tool_name},
            {"elapsed_ms", result.elapsed_ms},
            {"exit_code", result.exit_code}
        });
        GetObs().incrementCounter("tool_registry.tool_errors");
    } else {
        GetObs().endSpan(spanId);
        GetObs().logDebug(kRegistryComponent, "Tool call succeeded", {
            {"tool", tool_name},
            {"elapsed_ms", result.elapsed_ms}
        });
    }

    GetObs().recordHistogram("tool_registry.dispatch_ms", static_cast<float>(result.elapsed_ms));
    GetObs().incrementCounter("tool_registry.total_dispatches");

    return result;
}

void AgentToolRegistry::RegisterHandler(const std::string& tool_name, ToolHandler handler) {
    auto it = m_nameIndex.find(tool_name);
    if (it != m_nameIndex.end()) {
        m_tools[it->second].handler = handler;
    }
}

std::vector<std::string> AgentToolRegistry::ListTools() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> names;
    names.reserve(m_tools.size());
    for (const auto& td : m_tools) {
        names.emplace_back(td.name);
    }
    return names;
}

uint64_t AgentToolRegistry::GetTotalInvocations() const {
    uint64_t total = 0;
    for (const auto& td : m_tools) {
        total += td.invocation_count;
    }
    return total;
}

uint64_t AgentToolRegistry::GetTotalErrors() const {
    uint64_t total = 0;
    for (const auto& td : m_tools) {
        total += td.error_count;
    }
    return total;
}

bool AgentToolRegistry::ValidateArgs(
    const std::string& tool_name,
    const json& args,
    std::string& error) const
{
    auto it = m_nameIndex.find(tool_name);
    if (it == m_nameIndex.end()) {
        error = "Unknown tool: " + tool_name;
        return false;
    }

    const auto& td = m_tools[it->second];

    // Check required parameters (those without defaults)
    json schema_copy = td.params_schema; // non-const copy for iteration
    for (auto& item : schema_copy.items()) {
        const auto& key = item.key();
        auto& schema = item.value();
        if (!schema.contains("default") && !args.contains(key)) {
            error = "Missing required parameter: " + key;
            return false;
        }
        // Type checking
        if (args.contains(key)) {
            std::string expected_type = schema.value("type", "string");
            const auto& val = args[key];
            if (expected_type == "string" && !val.is_string()) {
                error = "Parameter '" + key + "' must be a string";
                return false;
            }
            if (expected_type == "number" && !val.is_number()) {
                error = "Parameter '" + key + "' must be a number";
                return false;
            }
            if (expected_type == "boolean" && !val.is_boolean()) {
                error = "Parameter '" + key + "' must be a boolean";
                return false;
            }
        }
    }
    return true;
}
