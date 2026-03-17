#include "RawrXD_ToolRegistry.h"
#include "win32app/IDELogger.h"

#include <chrono>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <regex>

using RawrXD::Agent::ToolRegistry;
using RawrXD::Agent::ToolResult;
using RawrXD::Agent::DangerLevel;
using json = nlohmann::json;

namespace {
constexpr DWORD kDefaultTimeoutMs = 300000;
constexpr size_t kMaxOutputBytes = 4 * 1024 * 1024;
thread_local const RawrXD::Agent::ToolDefinition* g_activeTool = nullptr;

std::wstring GetEnvVar(const std::wstring& name) {
    DWORD size = GetEnvironmentVariableW(name.c_str(), nullptr, 0);
    if (size == 0) return L"";
    std::wstring value(size, L'\0');
    GetEnvironmentVariableW(name.c_str(), value.data(), size);
    if (!value.empty() && value.back() == L'\0') value.pop_back();
    return value;
}

std::wstring ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring out(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), len);
    if (!out.empty() && out.back() == L'\0') out.pop_back();
    return out;
}

std::string ToUtf8(const std::wstring& s) {
    if (s.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, out.data(), len, nullptr, nullptr);
    if (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}

FILETIME GetLastWriteTime(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) {
        return data.ftLastWriteTime;
    }
    FILETIME zero{};
    return zero;
}

bool FileTimeEqual(const FILETIME& a, const FILETIME& b) {
    return a.dwHighDateTime == b.dwHighDateTime && a.dwLowDateTime == b.dwLowDateTime;
}

std::wstring WildcardToRegex(const std::wstring& pattern) {
    std::wstring rx;
    rx.reserve(pattern.size() * 2 + 2);
    rx += L"^";
    for (wchar_t ch : pattern) {
        switch (ch) {
            case L'*': rx += L".*"; break;
            case L'?': rx += L"."; break;
            case L'.': case L'^': case L'$': case L'+': case L'(':
            case L')': case L'[': case L']': case L'{': case L'}':
            case L'|': case L'\\':
                rx += L"\\";
                rx += ch;
                break;
            default:
                rx += ch;
                break;
        }
    }
    rx += L"$";
    return rx;
}
}

ToolRegistry& ToolRegistry::Instance() {
    static ToolRegistry instance;
    return instance;
}

ToolRegistry::ToolRegistry() {
    std::wstring cwd = std::filesystem::current_path().wstring();
    m_projectRoot = cwd;
}

ToolRegistry::~ToolRegistry() = default;

void ToolRegistry::SetProjectRoot(const std::wstring& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_projectRoot = NormalizePath(path);
}

void ToolRegistry::SetConsentCallback(ConsentCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_consentCallback = std::move(callback);
}

bool ToolRegistry::LoadFromDisk(const std::wstring& path) {
    LOG_INFO("ToolRegistry::LoadFromDisk");
    std::lock_guard<std::mutex> lock(m_mutex);

    m_registryPath = NormalizePath(path);
    std::string regPathStr = ToUtf8(m_registryPath);
    std::ifstream registryFile(regPathStr);
    if (!registryFile.is_open()) {
        LOG_ERROR("ToolRegistry: failed to open registry: " + regPathStr);
        return false;
    }

    std::stringstream buffer;
    buffer << registryFile.rdbuf();
    registryFile.close();

    json root;
    try {
        root = json::parse(buffer.str());
    } catch (const std::exception& ex) {
        LOG_ERROR(std::string("ToolRegistry: JSON parse failed: ") + ex.what());
        return false;
    }

    std::string error;
    if (!ParseRegistry(root, error)) {
        LOG_ERROR("ToolRegistry: parse error: " + error);
        return false;
    }

    m_registryJson = root;
    m_lastWriteTime = GetLastWriteTime(m_registryPath);

    LOG_INFO("ToolRegistry: loaded registry with " + std::to_string(m_tools.size()) + " tools");
    return true;
}

bool ToolRegistry::Reload() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_registryPath.empty()) return false;
    FILETIME current = GetLastWriteTime(m_registryPath);
    if (FileTimeEqual(current, m_lastWriteTime)) return false;
    return LoadFromDisk(m_registryPath);
}

std::vector<std::string> ToolRegistry::ListTools() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> names;
    names.reserve(m_tools.size());
    for (const auto& kv : m_tools) {
        names.push_back(kv.first);
    }
    return names;
}

const RawrXD::Agent::ToolDefinition* ToolRegistry::GetTool(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_tools.find(name);
    if (it == m_tools.end()) return nullptr;
    return &it->second;
}

ToolResult ToolRegistry::Execute(const std::string& tool_name, const std::string& json_args, std::string& output) {
    auto start = std::chrono::steady_clock::now();
    m_totalExecutions.fetch_add(1);
    output.clear();

    try {
        const ToolDefinition* tool = GetTool(tool_name);
        if (!tool) {
            output = "Tool not found: " + tool_name;
            m_totalErrors.fetch_add(1);
            LOG_ERROR(output);
            return ToolResult::ValidationFailed;
        }

        json args = json::object_type();
        if (!json_args.empty()) {
            args = json::parse(json_args);
        }

        std::string validationError;
        if (!ValidateArgs(tool, args, validationError)) {
            output = "Validation failed: " + validationError;
            m_totalErrors.fetch_add(1);
            LOG_WARNING(output);
            return ToolResult::ValidationFailed;
        }

        if (!CheckSandbox(tool, args)) {
            output = "Sandbox blocked tool execution";
            m_totalErrors.fetch_add(1);
            LOG_WARNING(output);
            return ToolResult::SandboxBlocked;
        }

        ConsentCallback consent;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            consent = m_consentCallback;
        }

        bool requireConsent = tool->danger >= DangerLevel::Destructive || tool->sandbox.require_confirmation;
        if (requireConsent && consent) {
            if (!consent(*tool, args)) {
                output = "Execution cancelled by user";
                LOG_WARNING(output);
                return ToolResult::Cancelled;
            }
        }

        g_activeTool = tool;
        ToolResult result = tool->handler ? tool->handler(args, output) : ToolResult::ExecutionError;
        g_activeTool = nullptr;

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        LOG_INFO("Tool execution " + tool_name + " finished in " + std::to_string(elapsed) + " ms");

        if (result != ToolResult::Success) {
            m_totalErrors.fetch_add(1);
        }

        return result;
    } catch (const std::exception& ex) {
        m_totalErrors.fetch_add(1);
        output = std::string("Tool execution error: ") + ex.what();
        LOG_ERROR(output);
        return ToolResult::ExecutionError;
    } catch (...) {
        m_totalErrors.fetch_add(1);
        output = "Tool execution error: unknown exception";
        LOG_ERROR(output);
        return ToolResult::ExecutionError;
    }
}

bool ToolRegistry::ValidateArgs(const ToolDefinition* tool, const json& args, std::string& error) const {
    if (!tool) return false;
    for (const auto& param : tool->params) {
        if (param.required && !args.contains(param.name)) {
            error = "Missing required param: " + param.name;
            return false;
        }
        if (!args.contains(param.name)) continue;

        const json& value = args[param.name];
        if (param.type == "string" && !value.is_string()) {
            error = "Param " + param.name + " must be string";
            return false;
        }
        if (param.type == "integer" && !value.is_number_integer()) {
            error = "Param " + param.name + " must be integer";
            return false;
        }
        if (param.type == "boolean" && !value.is_boolean()) {
            error = "Param " + param.name + " must be boolean";
            return false;
        }
        if (param.type == "array" && !value.is_array()) {
            error = "Param " + param.name + " must be array";
            return false;
        }
        if (param.max_length > 0 && value.is_string()) {
            if (value.get<std::string>().size() > param.max_length) {
                error = "Param " + param.name + " exceeds max length";
                return false;
            }
        }
        if (!param.enum_values.empty() && value.is_string()) {
            auto str = value.get<std::string>();
            if (std::find(param.enum_values.begin(), param.enum_values.end(), str) == param.enum_values.end()) {
                error = "Param " + param.name + " not in enum";
                return false;
            }
        }
        if (param.min_int != 0 || param.max_int != 0) {
            if (value.is_number_integer()) {
                int v = value.get<int>();
                if (param.min_int != 0 && v < param.min_int) {
                    error = "Param " + param.name + " below min";
                    return false;
                }
                if (param.max_int != 0 && v > param.max_int) {
                    error = "Param " + param.name + " above max";
                    return false;
                }
            }
        }
    }
    return true;
}

bool ToolRegistry::CheckSandbox(const ToolDefinition* tool, const json& args) const {
    if (!tool) return false;
    if (tool->name == "CodeEdit") {
        if (!args.contains("target")) return false;
        std::wstring target = NormalizePath(ToWide(args.at("target").get<std::string>()));
        return ValidatePath(target, tool->sandbox);
    }
    return true;
}

std::string ToolRegistry::GetSchemaForLLM() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_registryJson.contains("tools")) {
        return m_registryJson["tools"].dump();
    }
    return "[]";
}

bool ToolRegistry::RollbackPending(std::string& output) {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t restored = 0;
    for (const auto& entry : m_pendingBackups) {
        try {
            std::filesystem::copy_file(entry.backupPath, entry.originalPath, std::filesystem::copy_options::overwrite_existing);
            ++restored;
        } catch (const std::exception& ex) {
            output += "Rollback failed for " + ToUtf8(entry.originalPath) + ": " + ex.what() + "\n";
        }
    }
    m_pendingBackups.clear();
    output += "Restored backups: " + std::to_string(restored);
    return restored > 0;
}

bool ToolRegistry::ParseRegistry(const json& root, std::string& error) {
    if (!root.contains("tools") || !root.at("tools").is_array()) {
        error = "Registry missing tools array";
        return false;
    }

    m_tools.clear();
    for (const auto& toolJson : root.at("tools")) {
        ToolDefinition def = BuildDefinition(toolJson);
        m_tools[def.name] = std::move(def);
    }

    return true;
}

RawrXD::Agent::ToolDefinition ToolRegistry::BuildDefinition(const json& toolJson) {
    ToolDefinition def;
    def.name = toolJson.value("name", "");
    def.description = toolJson.value("description", "");
    def.category = toolJson.value("category", "");
    def.danger = ParseDangerLevel(toolJson.value("danger_level", 2));

    if (toolJson.contains("params")) {
        def.params_schema = toolJson.at("params");
        for (auto it = toolJson.at("params").begin(); it != toolJson.at("params").end(); ++it) {
            ToolParam param;
            param.name = it.key();
            const json& paramJson = it.value();
            param.type = paramJson.value("type", "string");
            param.required = paramJson.value("required", false);
            if (paramJson.contains("default")) {
                param.default_value = paramJson.at("default").dump();
            }
            if (paramJson.contains("enum") && paramJson.at("enum").is_array()) {
                for (const auto& val : paramJson.at("enum")) {
                    if (val.is_string()) param.enum_values.push_back(val.get<std::string>());
                }
            }
            param.max_length = paramJson.value("max_length", 0);
            param.min_int = paramJson.value("min", 0);
            param.max_int = paramJson.value("max", 0);
            def.params.push_back(param);
        }
    }

    if (toolJson.contains("sandbox")) {
        def.sandbox_schema = toolJson.at("sandbox");
        if (toolJson.at("sandbox").contains("allow_paths")) {
            for (const auto& p : toolJson.at("sandbox")["allow_paths"]) {
                def.sandbox.allowed_paths.push_back(ExpandPathToken(ToWide(p.get<std::string>())));
            }
        }
        if (toolJson.at("sandbox").contains("deny_patterns")) {
            for (const auto& p : toolJson.at("sandbox")["deny_patterns"]) {
                def.sandbox.deny_patterns.push_back(ToWide(p.get<std::string>()));
            }
        }
        def.sandbox.max_file_size = toolJson.at("sandbox").value("max_file_size", 0);
        def.sandbox.timeout_ms = toolJson.at("sandbox").value("timeout_ms", kDefaultTimeoutMs);
        def.sandbox.memory_limit = static_cast<SIZE_T>(toolJson.at("sandbox").value("memory_limit_mb", 0)) * 1024 * 1024;
        def.sandbox.capture_output = toolJson.at("sandbox").value("capture_output", true);
        if (toolJson.at("sandbox").contains("deny_commands")) {
            for (const auto& p : toolJson.at("sandbox")["deny_commands"]) {
                def.sandbox.deny_commands.push_back(p.get<std::string>());
            }
        }
        def.sandbox.require_confirmation = toolJson.at("sandbox").value("require_confirmation", false);
    }

    if (def.name == "CodeEdit") {
        def.handler = [this](const json& args, std::string& output) { return HandleCodeEdit(args, output); };
    } else if (def.name == "BuildProject") {
        def.handler = [this](const json& args, std::string& output) { return HandleBuildProject(args, output); };
    } else if (def.name == "StaticAnalysis") {
        def.handler = [this](const json& args, std::string& output) { return HandleStaticAnalysis(args, output); };
    } else if (def.name == "GitOperation") {
        def.handler = [this](const json& args, std::string& output) { return HandleGitOperation(args, output); };
    }

    return def;
}

DangerLevel ToolRegistry::ParseDangerLevel(int value) {
    switch (value) {
        case 1: return DangerLevel::Safe;
        case 2: return DangerLevel::Normal;
        case 3: return DangerLevel::Destructive;
        case 4: return DangerLevel::Critical;
        default: return DangerLevel::Normal;
    }
}

std::wstring ToolRegistry::ExpandPathToken(const std::wstring& token) const {
    if (token == L"${PROJECT_ROOT}") return m_projectRoot;
    if (token == L"${TEMP}") {
        auto temp = GetEnvVar(L"TEMP");
        if (!temp.empty()) return temp;
        return GetEnvVar(L"TMP");
    }
    return token;
}

std::wstring ToolRegistry::NormalizePath(const std::wstring& path) const {
    try {
        return std::filesystem::weakly_canonical(path).wstring();
    } catch (...) {
        return path;
    }
}

bool ToolRegistry::ValidatePath(const std::wstring& path, const ToolSandbox& sandbox) const {
    std::wstring normalized = NormalizePath(path);

    bool allowed = false;
    for (const auto& allow : sandbox.allowed_paths) {
        if (allow.empty()) continue;
        std::wstring allowNorm = NormalizePath(allow);
        if (normalized.find(allowNorm) == 0) {
            allowed = true;
            break;
        }
    }
    if (!allowed) {
        LOG_WARNING("Sandbox deny: path not in allow list: " + ToUtf8(normalized));
        return false;
    }

    for (const auto& denyPattern : sandbox.deny_patterns) {
        std::wstring rxPattern = WildcardToRegex(denyPattern);
        std::wregex rx(rxPattern, std::regex_constants::icase);
        if (std::regex_match(normalized, rx)) {
            LOG_WARNING("Sandbox deny: pattern matched: " + ToUtf8(normalized));
            return false;
        }
    }

    if (sandbox.max_file_size > 0) {
        try {
            auto size = std::filesystem::file_size(normalized);
            if (size > sandbox.max_file_size) {
                LOG_WARNING("Sandbox deny: file too large");
                return false;
            }
        } catch (...) {}
    }

    return true;
}

bool ToolRegistry::CreateSandboxedProcess(const std::wstring& cmdline, const ToolSandbox& sandbox, std::string& output, DWORD& exitCode) const {
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hRead = nullptr;
    HANDLE hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        output = "Failed to create output pipe";
        return false;
    }
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi{};
    std::wstring cmd = cmdline;

    BOOL created = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(hWrite);

    if (!created) {
        CloseHandle(hRead);
        output = "Failed to spawn process";
        return false;
    }

    std::string buffer;
    buffer.reserve(4096);
    DWORD start = GetTickCount();
    DWORD timeout = sandbox.timeout_ms ? sandbox.timeout_ms : kDefaultTimeoutMs;

    while (true) {
        DWORD available = 0;
        if (PeekNamedPipe(hRead, nullptr, 0, nullptr, &available, nullptr) && available > 0) {
            char temp[4096];
            DWORD bytesRead = 0;
            if (ReadFile(hRead, temp, sizeof(temp) - 1, &bytesRead, nullptr) && bytesRead > 0) {
                temp[bytesRead] = '\0';
                buffer.append(temp, bytesRead);
                if (buffer.size() > kMaxOutputBytes) {
                    buffer.resize(kMaxOutputBytes);
                    break;
                }
            }
        }

        DWORD waitResult = WaitForSingleObject(pi.hProcess, 100);
        if (waitResult == WAIT_OBJECT_0) break;

        if (GetTickCount() - start > timeout) {
            TerminateProcess(pi.hProcess, 1);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            CloseHandle(hRead);
            output = buffer + "\nTimeout waiting for process";
            exitCode = WAIT_TIMEOUT;
            return false;
        }
    }

    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hRead);

    output = buffer;
    return true;
}

ToolResult ToolRegistry::HandleCodeEdit(const json& args, std::string& output) {
    if (!args.contains("target") || !args.at("target").is_string()) {
        output = "CodeEdit missing target";
        return ToolResult::ValidationFailed;
    }

    std::wstring target = NormalizePath(ToWide(args.at("target").get<std::string>()));
    bool createBackup = args.value("create_backup", true);

    std::ifstream file(target);
    if (!file.is_open()) {
        output = "Failed to open target file";
        return ToolResult::ExecutionError;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();

    int lineStart = args.value("line_start", 0);
    int lineEnd = args.value("line_end", 0);
    std::string replacement = args.value("replacement", "");

    if (lineStart <= 0) {
        lines.clear();
        std::stringstream ss(replacement);
        while (std::getline(ss, line)) {
            lines.push_back(line);
        }
    } else {
        if (lineEnd <= 0) lineEnd = lineStart;
        if (lineStart > static_cast<int>(lines.size()) || lineEnd > static_cast<int>(lines.size())) {
            output = "Line range out of bounds";
            return ToolResult::ExecutionError;
        }
        lineStart -= 1;
        lineEnd -= 1;
        std::vector<std::string> newLines;
        std::stringstream ss(replacement);
        while (std::getline(ss, line)) newLines.push_back(line);
        lines.erase(lines.begin() + lineStart, lines.begin() + lineEnd + 1);
        lines.insert(lines.begin() + lineStart, newLines.begin(), newLines.end());
    }

    if (createBackup) {
        std::wstring backupPath = target + L".bak";
        try {
            std::filesystem::copy_file(target, backupPath, std::filesystem::copy_options::overwrite_existing);
            std::lock_guard<std::mutex> lock(m_mutex);
            m_pendingBackups.push_back({target, backupPath});
        } catch (...) {
            output = "Failed to create backup";
            return ToolResult::ExecutionError;
        }
    }

    std::ofstream out(target, std::ios::trunc);
    if (!out.is_open()) {
        output = "Failed to open file for writing";
        return ToolResult::ExecutionError;
    }
    for (size_t i = 0; i < lines.size(); ++i) {
        out << lines[i];
        if (i + 1 < lines.size()) out << "\n";
    }
    out.close();

    output = "CodeEdit applied successfully";
    return ToolResult::Success;
}

ToolResult ToolRegistry::HandleBuildProject(const json& args, std::string& output) {
    std::string config = args.value("config", "Release");
    std::string target = args.value("target", "all");

    std::filesystem::path root = m_projectRoot.empty() ? std::filesystem::current_path() : std::filesystem::path(m_projectRoot);
    std::filesystem::path buildDir = root / "build";
    std::wstring cmdLine;

    if (std::filesystem::exists(buildDir / "CMakeCache.txt")) {
        cmdLine = L"cmake --build \"" + buildDir.wstring() + L"\" --config " + ToWide(config) + L" --target " + ToWide(target);
    } else if (std::filesystem::exists(root / "Build_Release.ps1")) {
        cmdLine = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"" + (root / "Build_Release.ps1").wstring() + L"\"";
    } else {
        output = "No build configuration found";
        return ToolResult::ExecutionError;
    }

    DWORD exitCode = 0;
    std::string procOutput;
    const ToolSandbox* sandbox = g_activeTool ? &g_activeTool->sandbox : nullptr;
    ToolSandbox effective = sandbox ? *sandbox : ToolSandbox{};
    if (!CreateSandboxedProcess(cmdLine, effective, procOutput, exitCode)) {
        output = procOutput;
        return exitCode == WAIT_TIMEOUT ? ToolResult::Timeout : ToolResult::ExecutionError;
    }

    output = procOutput.empty() ? "Build completed" : procOutput;
    return exitCode == 0 ? ToolResult::Success : ToolResult::ExecutionError;
}

ToolResult ToolRegistry::HandleStaticAnalysis(const json& args, std::string& output) {
    if (!args.contains("files") || !args.at("files").is_array()) {
        output = "StaticAnalysis requires files array";
        return ToolResult::ValidationFailed;
    }
    std::vector<std::string> files;
    for (const auto& f : args.at("files")) {
        if (f.is_string()) files.push_back(f.get<std::string>());
    }
    if (files.empty()) {
        output = "StaticAnalysis requires at least one file";
        return ToolResult::ValidationFailed;
    }

    std::string checks = "";
    if (args.contains("checks")) {
        for (const auto& c : args.at("checks")) {
            if (!checks.empty()) checks += ",";
            checks += c.get<std::string>();
        }
    }
    if (checks.empty()) checks = "cppcoreguidelines-*,modernize-*";

    std::wstring cmdLine = L"clang-tidy -checks=" + ToWide(checks);
    for (const auto& f : files) {
        cmdLine += L" \"" + ToWide(f) + L"\"";
    }

    DWORD exitCode = 0;
    std::string procOutput;
    const ToolSandbox* sandbox = g_activeTool ? &g_activeTool->sandbox : nullptr;
    ToolSandbox effective = sandbox ? *sandbox : ToolSandbox{};
    if (!CreateSandboxedProcess(cmdLine, effective, procOutput, exitCode)) {
        output = procOutput;
        return exitCode == WAIT_TIMEOUT ? ToolResult::Timeout : ToolResult::ExecutionError;
    }

    output = procOutput;
    return exitCode == 0 ? ToolResult::Success : ToolResult::ExecutionError;
}

ToolResult ToolRegistry::HandleGitOperation(const json& args, std::string& output) {
    std::string command = args.value("command", "status");
    static const std::vector<std::string> allowed = {"status", "diff", "commit", "branch", "checkout", "pull"};
    if (std::find(allowed.begin(), allowed.end(), command) == allowed.end()) {
        output = "Git command not allowed";
        return ToolResult::ValidationFailed;
    }

    std::wstring cmdLine = L"git " + ToWide(command);
    if (args.contains("args") && args.at("args").is_array()) {
        for (const auto& a : args.at("args")) {
            cmdLine += L" \"" + ToWide(a.get<std::string>()) + L"\"";
        }
    }

    DWORD exitCode = 0;
    std::string procOutput;
    const ToolSandbox* sandbox = g_activeTool ? &g_activeTool->sandbox : nullptr;
    ToolSandbox effective = sandbox ? *sandbox : ToolSandbox{};
    if (!CreateSandboxedProcess(cmdLine, effective, procOutput, exitCode)) {
        output = procOutput;
        return exitCode == WAIT_TIMEOUT ? ToolResult::Timeout : ToolResult::ExecutionError;
    }

    output = procOutput;
    return exitCode == 0 ? ToolResult::Success : ToolResult::ExecutionError;
}
