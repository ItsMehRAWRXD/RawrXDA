#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>
#include <nlohmann/json.hpp>

namespace RawrXD {
namespace Agent {

using json = nlohmann::json;

enum class DangerLevel : int {
    Safe = 1,
    Normal = 2,
    Destructive = 3,
    Critical = 4
};

enum class ToolResult : int {
    Success = 0,
    ValidationFailed = 1,
    SandboxBlocked = 2,
    Cancelled = 3,
    ExecutionError = 4,
    Timeout = 5
};

struct ToolParam {
    std::string name;
    std::string type;
    bool required = false;
    std::string default_value;
    std::vector<std::string> enum_values;
    size_t max_length = 0;
    int min_int = 0;
    int max_int = 0;
};

struct ToolSandbox {
    std::vector<std::wstring> allowed_paths;
    std::vector<std::wstring> deny_patterns;
    size_t max_file_size = 0;
    DWORD timeout_ms = 0;
    SIZE_T memory_limit = 0;
    bool capture_output = true;
    std::vector<std::string> deny_commands;
    bool require_confirmation = false;
};

struct ToolDefinition {
    std::string name;
    std::string description;
    std::string category;
    DangerLevel danger = DangerLevel::Normal;
    std::vector<ToolParam> params;
    json params_schema = json::object();
    json sandbox_schema = json::object();
    ToolSandbox sandbox;
    std::function<ToolResult(const json&, std::string&)> handler;
};

class ToolRegistry {
public:
    using ConsentCallback = std::function<bool(const ToolDefinition&, const json&)>;

    static ToolRegistry& Instance();

    ToolRegistry();
    ~ToolRegistry();

    void SetProjectRoot(const std::wstring& path);
    void SetConsentCallback(ConsentCallback callback);

    bool LoadFromDisk(const std::wstring& path);
    bool Reload();

    std::vector<std::string> ListTools() const;
    const ToolDefinition* GetTool(const std::string& name) const;

    ToolResult Execute(const std::string& tool_name, const std::string& json_args, std::string& output);
    std::string GetSchemaForLLM() const;
    bool RollbackPending(std::string& output);

private:
    struct BackupEntry {
        std::wstring originalPath;
        std::wstring backupPath;
    };

    bool ParseRegistry(const json& root, std::string& error);
    ToolDefinition BuildDefinition(const json& toolJson);
    DangerLevel ParseDangerLevel(int value);

    std::wstring ExpandPathToken(const std::wstring& token) const;
    std::wstring NormalizePath(const std::wstring& path) const;
    bool ValidatePath(const std::wstring& path, const ToolSandbox& sandbox) const;
    bool ValidateArgs(const ToolDefinition* tool, const json& args, std::string& error) const;
    bool CheckSandbox(const ToolDefinition* tool, const json& args) const;

    bool CreateSandboxedProcess(const std::wstring& cmdline, const ToolSandbox& sandbox,
                                std::string& output, DWORD& exitCode) const;

    ToolResult HandleCodeEdit(const json& args, std::string& output);
    ToolResult HandleBuildProject(const json& args, std::string& output);
    ToolResult HandleStaticAnalysis(const json& args, std::string& output);
    ToolResult HandleGitOperation(const json& args, std::string& output);

    mutable std::mutex m_mutex;
    std::wstring m_projectRoot;
    std::wstring m_registryPath;
    json m_registryJson = json::object();
    FILETIME m_lastWriteTime{};
    std::unordered_map<std::string, ToolDefinition> m_tools;
    std::vector<BackupEntry> m_pendingBackups;
    ConsentCallback m_consentCallback;
    std::atomic<uint64_t> m_totalExecutions{0};
    std::atomic<uint64_t> m_totalErrors{0};
};

} // namespace Agent
} // namespace RawrXD
