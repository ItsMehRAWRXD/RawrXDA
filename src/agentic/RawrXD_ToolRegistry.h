#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <atomic>
#include <optional>
#include <nlohmann/json.hpp>

namespace RawrXD {
namespace Agent {

enum class DangerLevel { Safe = 1, Normal = 2, Destructive = 3, Critical = 4 };
enum class ToolResult { Success, ValidationFailed, SandboxBlocked, ExecutionError, Timeout, Cancelled };

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
    ToolSandbox sandbox;
    nlohmann::json params_schema;
    nlohmann::json sandbox_schema;
    std::function<ToolResult(const nlohmann::json&, std::string&)> handler;
};

class ToolRegistry {
public:
    static ToolRegistry& Instance();

    bool LoadFromDisk(const std::wstring& path);
    bool Reload();
    void SetProjectRoot(const std::wstring& path);

    std::vector<std::string> ListTools() const;
    const ToolDefinition* GetTool(const std::string& name) const;

    ToolResult Execute(const std::string& tool_name, const std::string& json_args, std::string& output);

    using ConsentCallback = std::function<bool(const ToolDefinition&, const nlohmann::json&)>;
    void SetConsentCallback(ConsentCallback callback);

    bool ValidateArgs(const ToolDefinition* tool, const nlohmann::json& args, std::string& error) const;
    bool CheckSandbox(const ToolDefinition* tool, const nlohmann::json& args) const;

    std::string GetSchemaForLLM() const;
    bool RollbackPending(std::string& output);

    uint64_t GetTotalExecutions() const { return m_totalExecutions.load(); }
    uint64_t GetTotalErrors() const { return m_totalErrors.load(); }

private:
    ToolRegistry();
    ~ToolRegistry();

    bool ParseRegistry(const nlohmann::json& root, std::string& error);
    ToolDefinition BuildDefinition(const nlohmann::json& toolJson);
    static DangerLevel ParseDangerLevel(int value);

    std::wstring ExpandPathToken(const std::wstring& token) const;
    std::wstring NormalizePath(const std::wstring& path) const;
    bool ValidatePath(const std::wstring& path, const ToolSandbox& sandbox) const;
    bool CreateSandboxedProcess(const std::wstring& cmdline, const ToolSandbox& sandbox, std::string& output, DWORD& exitCode) const;

    ToolResult HandleCodeEdit(const nlohmann::json& args, std::string& output);
    ToolResult HandleBuildProject(const nlohmann::json& args, std::string& output);
    ToolResult HandleStaticAnalysis(const nlohmann::json& args, std::string& output);
    ToolResult HandleGitOperation(const nlohmann::json& args, std::string& output);

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, ToolDefinition> m_tools;
    ConsentCallback m_consentCallback;

    std::wstring m_registryPath;
    std::wstring m_projectRoot;
    FILETIME m_lastWriteTime{};
    nlohmann::json m_registryJson;

    struct BackupEntry {
        std::wstring originalPath;
        std::wstring backupPath;
    };

    std::vector<BackupEntry> m_pendingBackups;

    std::atomic<uint64_t> m_totalExecutions{0};
    std::atomic<uint64_t> m_totalErrors{0};
};

} // namespace Agent
} // namespace RawrXD
