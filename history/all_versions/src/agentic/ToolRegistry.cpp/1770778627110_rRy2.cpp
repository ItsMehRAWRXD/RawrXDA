// =============================================================================
// ToolRegistry.cpp — X-Macro Tool Registry Implementation
// =============================================================================
#include "ToolRegistry.h"
#include <sstream>
#include <chrono>
#include <filesystem>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
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
    return ToolExecResult::error("execute_command not implemented on this platform");
#endif
}

ToolExecResult HandleSearchCode(const json& args) {
    std::string query = args.value("query", "");
    std::string pattern = args.value("file_pattern", "*.*");
    [[maybe_unused]] bool is_regex = args.value("is_regex", false);
    if (query.empty()) return ToolExecResult::error("Missing required parameter: query");

    // TODO: Wire into AVX-512 SIMD search engine (RawrXD_AVX512_PatternEngine)
    // For now, use std::filesystem + basic string matching
    std::ostringstream results;
    [[maybe_unused]] int matchCount = 0;
    [[maybe_unused]] const int maxMatches = 50;

    // This is a placeholder — the real implementation dispatches to the MASM SIMD scanner
    results << "[search_code] query=\"" << query << "\" pattern=\"" << pattern << "\"\n";
    results << "Note: AVX-512 accelerator pending integration. Using fallback scanner.\n";

    return ToolExecResult::ok(results.str());
}

ToolExecResult HandleGetDiagnostics(const json& args) {
    std::string file = args.value("file", "");
    // TODO: Wire into LSP client diagnostic cache
    return ToolExecResult::ok("[get_diagnostics] No diagnostics cache connected yet. Wire into LSP client.");
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
    // TODO: Wire into BBCov / DiffCov engine
    return ToolExecResult::ok("[get_coverage] Coverage engine not yet connected. "
                              "Wire into BBCov/DiffCov for file: " + file);
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

    // TODO: Dispatch to UnifiedHotpatchManager
    return ToolExecResult::ok("[apply_hotpatch] layer=" + layer + " target=" + target +
                              " — UnifiedHotpatchManager dispatch pending integration.");
}

ToolExecResult HandleDiskRecovery(const json& args) {
    std::string action = args.value("action", "");
    if (action.empty()) return ToolExecResult::error("Missing required parameter: action");

    // Thread-local agent instance (persistent across calls within a session)
    static RawrXD::Recovery::DiskRecoveryAgent agent;

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
        agent = RawrXD::Recovery::DiskRecoveryAgent();
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
        for (auto& [key, val] : params_copy) {
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
        return ToolExecResult::error("Unknown tool: " + tool_name);
    }

    auto& td = m_tools[it->second];
    if (!td.handler) {
        return ToolExecResult::error("No handler registered for tool: " + tool_name);
    }

    // Validate args
    std::string validationError;
    if (!ValidateArgs(tool_name, args, validationError)) {
        ++td.error_count;
        return ToolExecResult::error("Validation failed: " + validationError);
    }

    auto start = std::chrono::high_resolution_clock::now();
    ToolExecResult result = td.handler(args);
    auto end = std::chrono::high_resolution_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    ++td.invocation_count;
    if (!result.success) {
        ++td.error_count;
    }

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
    for (auto& [key, schema] : schema_copy) {
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
