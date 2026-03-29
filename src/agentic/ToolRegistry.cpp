// =============================================================================
// ToolRegistry.cpp — Explicit Tool Registry Implementation (no X-Macro)
// =============================================================================
#include "ToolRegistry.h"
#include "AgentToolHandlers.h"
#include <sstream>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <cctype>

#include "../core/rawrxd_subsystem_api.hpp"
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

namespace RawrXD {
namespace Agent {

static AgenticObservability& GetObs() {
    static AgenticObservability instance;
    return instance;
}
static const char* kRegistryComponent = "ToolRegistry";

using RawrXD::Agent::AgentToolRegistry;
using RawrXD::Agent::ToolExecResult;
using RawrXD::Agent::ToolDescriptor;

namespace {

std::string NormalizeToolName(const std::string& raw) {
    if (raw.empty()) return {};

    // Normalize separators + camelCase -> snake_case
    std::string normalized;
    normalized.reserve(raw.size() + 8);
    bool lastWasUnderscore = false;
    for (char c : raw) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc)) {
            if (std::isupper(uc) && !normalized.empty() && normalized.back() != '_') {
                normalized.push_back('_');
            }
            normalized.push_back(static_cast<char>(std::tolower(uc)));
            lastWasUnderscore = false;
        } else {
            if (!normalized.empty() && !lastWasUnderscore) {
                normalized.push_back('_');
                lastWasUnderscore = true;
            }
        }
    }

    while (!normalized.empty() && normalized.front() == '_') normalized.erase(normalized.begin());
    while (!normalized.empty() && normalized.back() == '_') normalized.pop_back();

    // Legacy aliases and common model outputs
    if (normalized == "readfile") return "read_file";
    if (normalized == "writefile") return "write_file";
    if (normalized == "replacefile" || normalized == "replaceinfile") return "replace_in_file";
    if (normalized == "executecommand" || normalized == "runcommand") return "execute_command";
    if (normalized == "search" || normalized == "grep" || normalized == "searchcode") return "search_code";
    if (normalized == "diagnostics" || normalized == "getdiagnostics") return "get_diagnostics";
    if (normalized == "listdir" || normalized == "listdirectory") return "list_directory";
    if (normalized == "coverage" || normalized == "getcoverage") return "get_coverage";
    if (normalized == "build" || normalized == "runbuild") return "run_build";
    if (normalized == "hotpatch" || normalized == "applyhotpatch") return "apply_hotpatch";
    if (normalized == "diskrecovery" || normalized == "recoverdisk") return "disk_recovery";
    if (normalized == "deletefile" || normalized == "removefile") return "delete_file";
    if (normalized == "renamefile" || normalized == "movefile") return "rename_file";
    if (normalized == "copyfile") return "copy_file";
    if (normalized == "mkdir" || normalized == "makedirectory" || normalized == "createdirectory") return "make_directory";
    if (normalized == "statfile" || normalized == "getfileinfo") return "stat_file";
    if (normalized == "gitstatus") return "git_status";
    if (normalized == "runshell") return "run_shell";
    if (normalized == "semanticsearch") return "semantic_search";
    if (normalized == "mentionlookup") return "mention_lookup";
    if (normalized == "nextedithint") return "next_edit_hint";
    if (normalized == "proposemultifileedits" || normalized == "multifileedits") return "propose_multifile_edits";
    if (normalized == "loadrules") return "load_rules";
    if (normalized == "plantasks") return "plan_tasks";
    if (normalized == "setiterationstatus") return "set_iteration_status";
    if (normalized == "getiterationstatus") return "get_iteration_status";
    if (normalized == "resetiterationstatus") return "reset_iteration_status";

    return normalized;
}

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
        const char* detail = ur.result.detail.empty() ? "unknown error" : ur.result.detail.c_str();
        return ToolExecResult::error(
            std::string("[apply_hotpatch] ") + layer + " layer failed: " +
            detail,
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

// ---------------------------------------------------------------------------
// Agent operation handler proxies — delegate to AgentToolHandlers
// Converts ToolCallResult → ToolExecResult for ToolRegistry dispatch
// ---------------------------------------------------------------------------
namespace {

ToolExecResult toExecResult(const RawrXD::Agent::ToolCallResult& r) {
    if (r.isSuccess()) {
        return ToolExecResult::ok(r.output, static_cast<double>(r.durationMs));
    }
    return ToolExecResult::error(r.error.empty() ? r.output : r.error);
}

ToolExecResult HandleCompactConversation(const json& args) {
    return toExecResult(AgentToolHandlers::CompactConversation(args));
}

ToolExecResult HandleOptimizeToolSelection(const json& args) {
    return toExecResult(AgentToolHandlers::OptimizeToolSelection(args));
}

ToolExecResult HandleResolveSymbol(const json& args) {
    return toExecResult(AgentToolHandlers::ResolveSymbol(args));
}

ToolExecResult HandleReadLines(const json& args) {
    return toExecResult(AgentToolHandlers::ReadLines(args));
}

ToolExecResult HandlePlanCodeExploration(const json& args) {
    return toExecResult(AgentToolHandlers::PlanCodeExploration(args));
}

ToolExecResult HandleSearchFiles(const json& args) {
    return toExecResult(AgentToolHandlers::SearchFiles(args));
}

ToolExecResult HandleRestoreCheckpoint(const json& args) {
    return toExecResult(AgentToolHandlers::RestoreCheckpoint(args));
}

ToolExecResult HandleEvaluateIntegration(const json& args) {
    return toExecResult(AgentToolHandlers::EvaluateIntegrationAuditFeasibility(args));
}

// Extended file/system tool proxies
ToolExecResult HandleDeleteFile(const json& args) {
    return toExecResult(AgentToolHandlers::DeleteFile(args));
}
ToolExecResult HandleRenameFile(const json& args) {
    return toExecResult(AgentToolHandlers::RenameFile(args));
}
ToolExecResult HandleCopyFile(const json& args) {
    return toExecResult(AgentToolHandlers::CopyFile(args));
}
ToolExecResult HandleMakeDirectory(const json& args) {
    return toExecResult(AgentToolHandlers::MakeDirectory(args));
}
ToolExecResult HandleStatFile(const json& args) {
    return toExecResult(AgentToolHandlers::StatFile(args));
}
ToolExecResult HandleGitStatus(const json& args) {
    return toExecResult(AgentToolHandlers::GitStatus(args));
}
ToolExecResult HandleRunShell(const json& args) {
    return toExecResult(AgentToolHandlers::RunShell(args));
}

// AI-specific tool proxies
ToolExecResult HandleSemanticSearch(const json& args) {
    return toExecResult(AgentToolHandlers::SemanticSearch(args));
}
ToolExecResult HandleMentionLookup(const json& args) {
    return toExecResult(AgentToolHandlers::MentionLookup(args));
}
ToolExecResult HandleNextEditHint(const json& args) {
    return toExecResult(AgentToolHandlers::NextEditHint(args));
}
ToolExecResult HandleProposeMultiFileEdits(const json& args) {
    return toExecResult(AgentToolHandlers::ProposeMultiFileEdits(args));
}
ToolExecResult HandleLoadRules(const json& args) {
    return toExecResult(AgentToolHandlers::LoadRules(args));
}
ToolExecResult HandlePlanTasks(const json& args) {
    return toExecResult(AgentToolHandlers::PlanTasks(args));
}

ToolExecResult HandleSetIterationStatus(const json& args) {
    return toExecResult(AgentToolHandlers::SetIterationStatus(args));
}

ToolExecResult HandleGetIterationStatus(const json& args) {
    return toExecResult(AgentToolHandlers::GetIterationStatus(args));
}

ToolExecResult HandleResetIterationStatus(const json& args) {
    return toExecResult(AgentToolHandlers::ResetIterationStatus(args));
}

} // proxy namespace

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
    // Explicit tool registration — no X-Macro
    auto addTool = [this](const char* name, const char* description) {
        ToolDescriptor td;
        td.name = name;
        td.description = description;
        td.params_schema = json::object();
        td.handler = nullptr;
        m_tools.push_back(std::move(td));
        m_nameIndex[name] = m_tools.size() - 1;
    };

    // Core file/build tools
    addTool("read_file",        "Read the content of a file at a specific path. Returns UTF-8 text.");
    addTool("write_file",       "Create a new file or overwrite an existing one with the provided content.");
    addTool("replace_in_file",  "Search and replace a block of text within a file. Uses exact string matching.");
    addTool("execute_command",  "Run a shell command in the terminal. Returns stdout, stderr, and exit code.");
    addTool("search_code",      "Fast regex/literal search across the codebase. Uses AVX-512 SIMD accelerator when available.");
    addTool("get_diagnostics",  "Retrieve current compiler/LSP errors and warnings for a specific file or all files.");
    addTool("list_directory",   "List files and subdirectories at a given path.");
    addTool("get_coverage",     "Retrieve BBCov/DiffCov coverage data for a file or function to verify logic path changes.");
    addTool("run_build",        "Trigger a CMake build with specified target and configuration.");
    addTool("apply_hotpatch",   "Apply a runtime hotpatch through the unified hotpatch manager (memory, byte-level, or server layer).");
    addTool("disk_recovery",    "Control the hardware disk recovery agent for dying WD My Book USB bridges (scan, init, extract key, run, abort, stats).");

    // Extended file/system tools (CLI/GUI parity)
    addTool("delete_file",       "Delete a regular file at the given path. Only supports files (not directories).");
    addTool("rename_file",       "Rename or move a file from source to destination path.");
    addTool("copy_file",         "Copy a file to a destination path, optionally overwriting.");
    addTool("make_directory",    "Create a directory (and parent directories) at the given path.");
    addTool("stat_file",         "Return file metadata: existence, type, size.");
    addTool("git_status",        "Run git status in the workspace root and return short branch output.");
    addTool("run_shell",         "Run a shell command with sandbox policy enforcement. Delegates to execute_command after validation.");

    // AI-specific tools
    addTool("semantic_search",        "Search workspace files using TF-IDF cosine similarity. Returns top-K matching file snippets.");
    addTool("mention_lookup",         "Find references to a symbol across the workspace. Wraps semantic search with mention-focused defaults.");
    addTool("next_edit_hint",         "Heuristically suggest the next code edit based on provided context.");
    addTool("propose_multifile_edits","Generate a structured plan for edits across multiple files.");
    addTool("load_rules",             "Parse a .rawrrules file to seed system instructions and constraints.");
    addTool("plan_tasks",             "Generate a lightweight deterministic task plan from a goal description.");
    addTool("set_iteration_status",   "Set long-running model/agent iteration status (busy/current/total/phase/message).");
    addTool("get_iteration_status",   "Get current long-running model/agent iteration status.");
    addTool("reset_iteration_status", "Reset long-running model/agent iteration status to idle.");

    // Agent operation tools (accessible outside hotpatch panel via /api/tool)
    addTool("compact_conversation",     "Compact a conversation to reduce token count while preserving meaning. Returns compacted text.");
    addTool("optimize_tool_selection",  "Analyze task intent and rank available tools by relevance. Returns prioritized tool list.");
    addTool("resolve_symbol",           "Resolve a symbol name to its definition location across the workspace. Returns file paths and line numbers.");
    addTool("read_lines",              "Read specific line ranges from a file. Returns the requested lines with line numbers.");
    addTool("plan_code_exploration",    "Plan a structured code exploration strategy for a codebase. Returns exploration plan with entry points.");
    addTool("search_files",            "Search for files matching a glob pattern across the workspace. Returns matching file paths.");
    addTool("restore_checkpoint",      "Restore a previously saved checkpoint (conversation state, workspace, or both).");
    addTool("evaluate_integration_audit_feasibility", "Evaluate integration audit feasibility for the current workspace. Returns readiness matrix.");

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

    // delete_file
    setParam("delete_file", "path", "string", "Absolute or project-relative path to the file to delete");

    // rename_file
    setParam("rename_file", "source", "string", "Source file path (or 'path')");
    setParam("rename_file", "destination", "string", "Destination file path");

    // copy_file
    setParam("copy_file", "source", "string", "Source file path (or 'path')");
    setParam("copy_file", "destination", "string", "Destination file path");
    setParamWithDefault("copy_file", "overwrite", "boolean", "Overwrite if destination exists (default true)", true);

    // make_directory
    setParam("make_directory", "path", "string", "Directory path to create (parents auto-created)");

    // stat_file
    setParam("stat_file", "path", "string", "File or directory path to stat");

    // git_status
    setParamWithDefault("git_status", "root", "string", "Git repository root (defaults to workspace)", "");
    setParamWithDefault("git_status", "timeout", "number", "Timeout in milliseconds", 10000);

    // run_shell
    setParam("run_shell", "command", "string", "Shell command to execute (subject to sandbox policy)");
    setParamWithDefault("run_shell", "timeout", "number", "Timeout in milliseconds (default 30000)", 30000);

    // semantic_search
    setParam("semantic_search", "query", "string", "Natural language search query");
    setParamWithDefault("semantic_search", "root", "string", "Root directory to search (defaults to workspace)", ".");
    setParamWithDefault("semantic_search", "top_k", "number", "Number of top results to return", 5);

    // mention_lookup
    setParam("mention_lookup", "symbol", "string", "Symbol name to look up mentions for");
    setParamWithDefault("mention_lookup", "top_k", "number", "Number of results (clamped 1-10)", 3);
    setParamWithDefault("mention_lookup", "include_non_code", "boolean", "Include non-code files in search", false);

    // next_edit_hint
    setParam("next_edit_hint", "context", "string", "Code context to analyze for next edit suggestions");

    // propose_multifile_edits
    setParam("propose_multifile_edits", "files", "array", "Array of file paths to plan edits for");
    setParamWithDefault("propose_multifile_edits", "instruction", "string",
                         "Edit instruction to apply across files", "Apply requested change across files.");

    // load_rules
    setParamWithDefault("load_rules", "path", "string", "Path to .rawrrules file", "");
    setParamWithDefault("load_rules", "content", "string", "Inline rules content (overrides path)", "");

    // plan_tasks
    setParam("plan_tasks", "goal", "string", "High-level goal to generate a task plan for");
    setParamWithDefault("plan_tasks", "max_steps", "number", "Maximum plan steps (3-10)", 6);
    setParamWithDefault("plan_tasks", "deadline", "string", "Optional deadline string", "");
    setParamWithDefault("plan_tasks", "owner", "string", "Optional task owner", "");

    // set_iteration_status
    setParamWithDefault("set_iteration_status", "busy", "boolean", "Whether the model/agent is currently busy", false);
    setParamWithDefault("set_iteration_status", "current", "number", "Current iteration index", 0);
    setParamWithDefault("set_iteration_status", "total", "number", "Total iterations expected", 0);
    setParamWithDefault("set_iteration_status", "phase", "string", "Current phase label", "idle");
    setParamWithDefault("set_iteration_status", "message", "string", "Optional human-readable status", "");

    // compact_conversation
    setParamWithDefault("compact_conversation", "keep_last", "number",
                         "Number of recent entries to keep (default 200)", 200);
    setParamWithDefault("compact_conversation", "retention_days", "number",
                         "Purge entries older than N days (-1 = disabled)", -1);
    setParamWithDefault("compact_conversation", "flush", "boolean",
                         "Flush to disk after compaction", true);

    // optimize_tool_selection
    setParam("optimize_tool_selection", "task", "string",
             "Description of the task to optimize tool selection for");
    setParamWithDefault("optimize_tool_selection", "max_tools", "number",
                         "Maximum number of tools to recommend (1-10)", 6);

    // resolve_symbol
    setParam("resolve_symbol", "symbol", "string",
             "Symbol name to resolve across the workspace");
    setParamWithDefault("resolve_symbol", "file_pattern", "string",
                         "Glob filter for files to search (default: *.*)", "*.*");

    // read_lines
    setParam("read_lines", "path", "string",
             "Absolute or project-relative path to the file");
    setParamWithDefault("read_lines", "start_line", "number",
                         "First line to read (1-based)", 1);
    setParamWithDefault("read_lines", "end_line", "number",
                         "Last line to read (1-based)", 1);

    // plan_code_exploration
    setParam("plan_code_exploration", "goal", "string",
             "High-level exploration goal (e.g., 'Map all entry points')");
    setParamWithDefault("plan_code_exploration", "root", "string",
                         "Root directory to explore (default: cwd)", ".");

    // search_files
    setParam("search_files", "pattern", "string",
             "Glob pattern to match files (e.g., '*.cpp', 'src/**/*.h')");
    setParamWithDefault("search_files", "max_results", "number",
                         "Maximum number of matching files to return", 200);

    // restore_checkpoint
    setParamWithDefault("restore_checkpoint", "checkpoint_path", "string",
                         "Path to checkpoint file or 'latest'", "latest");

    // evaluate_integration_audit_feasibility
    setParamWithDefault("evaluate_integration_audit_feasibility", "workspace", "string",
                         "Workspace root to evaluate", ".");

    // Wire default handlers — core tools
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

    // Wire agent operation handlers — proxy to AgentToolHandlers
    RegisterHandler("compact_conversation",     HandleCompactConversation);
    RegisterHandler("optimize_tool_selection",  HandleOptimizeToolSelection);
    RegisterHandler("resolve_symbol",           HandleResolveSymbol);
    RegisterHandler("read_lines",              HandleReadLines);
    RegisterHandler("plan_code_exploration",    HandlePlanCodeExploration);
    RegisterHandler("search_files",            HandleSearchFiles);
    RegisterHandler("restore_checkpoint",      HandleRestoreCheckpoint);
    RegisterHandler("evaluate_integration_audit_feasibility", HandleEvaluateIntegration);

    // Wire extended file/system handlers — proxy to AgentToolHandlers
    RegisterHandler("delete_file",           HandleDeleteFile);
    RegisterHandler("rename_file",           HandleRenameFile);
    RegisterHandler("copy_file",             HandleCopyFile);
    RegisterHandler("make_directory",        HandleMakeDirectory);
    RegisterHandler("stat_file",             HandleStatFile);
    RegisterHandler("git_status",            HandleGitStatus);
    RegisterHandler("run_shell",             HandleRunShell);

    // Wire AI-specific handlers — proxy to AgentToolHandlers
    RegisterHandler("semantic_search",        HandleSemanticSearch);
    RegisterHandler("mention_lookup",         HandleMentionLookup);
    RegisterHandler("next_edit_hint",         HandleNextEditHint);
    RegisterHandler("propose_multifile_edits",HandleProposeMultiFileEdits);
    RegisterHandler("load_rules",             HandleLoadRules);
    RegisterHandler("plan_tasks",             HandlePlanTasks);
    RegisterHandler("set_iteration_status",   HandleSetIterationStatus);
    RegisterHandler("get_iteration_status",   HandleGetIterationStatus);
    RegisterHandler("reset_iteration_status", HandleResetIterationStatus);
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

        tools.push_back(nlohmann::json::object({
            {"type", "function"},
            {"function", nlohmann::json::object({
                {"name", td.name},
                {"description", td.description},
                {"parameters", nlohmann::json::object({
                    {"type", "object"},
                    {"properties", td.params_schema},
                    {"required", required_params}
                })}
            })}
        }));
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

    const std::string normalizedTool = NormalizeToolName(tool_name);
    if (normalizedTool.empty()) {
        return ToolExecResult::error("Unknown tool: " + tool_name);
    }

    auto it = m_nameIndex.find(normalizedTool);
    if (it == m_nameIndex.end()) {
        GetObs().logWarn(kRegistryComponent, "Dispatch: unknown tool", nlohmann::json::object({
            {"tool", tool_name},
            {"normalized", normalizedTool}
        }));
        GetObs().incrementCounter("tool_registry.unknown_tool");
        return ToolExecResult::error("Unknown tool: " + tool_name);
    }

    auto& td = m_tools[it->second];
    if (!td.handler) {
        GetObs().logError(kRegistryComponent, "Dispatch: no handler", nlohmann::json::object({
            {"tool", tool_name},
            {"normalized", normalizedTool}
        }));
        return ToolExecResult::error("No handler registered for tool: " + tool_name);
    }

    // Validate args
    std::string validationError;
    if (!ValidateArgs(normalizedTool, args, validationError)) {
        ++td.error_count;
        GetObs().logWarn(kRegistryComponent, "Dispatch: validation failed", nlohmann::json::object({
            {"tool", tool_name}, {"normalized", normalizedTool}, {"error", validationError}
        }));
        GetObs().incrementCounter("tool_registry.validation_failures");
        return ToolExecResult::error("Validation failed: " + validationError);
    }

    // Traced execution with timing
    auto spanId = GetObs().startSpan("tool_dispatch:" + normalizedTool);
    GetObs().logDebug(kRegistryComponent, "Dispatching tool call", nlohmann::json::object({
        {"tool", tool_name},
        {"normalized", normalizedTool}
    }));

    auto start = std::chrono::high_resolution_clock::now();
    ToolExecResult result = td.handler(args);
    auto end = std::chrono::high_resolution_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    ++td.invocation_count;
    if (!result.success) {
        ++td.error_count;
        GetObs().endSpan(spanId, true, result.output);
        GetObs().logWarn(kRegistryComponent, "Tool call failed", nlohmann::json::object({
            {"tool", normalizedTool},
            {"elapsed_ms", result.elapsed_ms},
            {"exit_code", result.exit_code}
        }));
        GetObs().incrementCounter("tool_registry.tool_errors");
    } else {
        GetObs().endSpan(spanId);
        GetObs().logDebug(kRegistryComponent, "Tool call succeeded", nlohmann::json::object({
            {"tool", normalizedTool},
            {"elapsed_ms", result.elapsed_ms}
        }));
    }

    GetObs().recordHistogram("tool_registry.dispatch_ms", static_cast<float>(result.elapsed_ms));
    GetObs().incrementCounter("tool_registry.total_dispatches");

    return result;
}

void AgentToolRegistry::RegisterHandler(const std::string& tool_name, ToolHandler handler) {
    const std::string normalizedTool = NormalizeToolName(tool_name);
    auto it = m_nameIndex.find(normalizedTool);
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
    const std::string normalizedTool = NormalizeToolName(tool_name);
    auto it = m_nameIndex.find(normalizedTool);
    if (it == m_nameIndex.end()) {
        error = "Unknown tool: " + tool_name;
        return false;
    }

    const auto& td = m_tools[it->second];

    // Check required parameters (those without defaults)
    json schema_copy = td.params_schema; // non-const copy for iteration
    for (auto it2 = schema_copy.begin(); it2 != schema_copy.end(); ++it2) {
        const std::string key = it2.key();
        auto& schema = it2.value();
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

} // namespace Agent
} // namespace RawrXD
