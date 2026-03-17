#pragma once

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <cstdint>
#include <nlohmann/json.hpp>

#include "backend/ollama_client.h"

namespace RawrXD {
namespace Tools { class GitClient; }
namespace Backend {

// Lightweight schema for tool discovery
struct ToolSchema {
    std::string name;
    std::string description;
    std::map<std::string, std::string> parameters;
    std::vector<std::string> required_params;
};

// Result wrapper for tool execution
struct ToolResult {
    bool success = false;
    std::string tool_name;
    std::string result_data;
    std::string error_message;
    int exit_code = 0;

    static ToolResult Ok(const std::string& tool, const std::string& data);
    static ToolResult Fail(const std::string& tool, const std::string& error, int code = -1);
};

// Execution statistics for observability
struct ToolStats {
    uint64_t total_tool_calls = 0;
    uint64_t successful_calls = 0;
    uint64_t failed_calls = 0;
    std::map<std::string, uint64_t> tool_usage_count;
};

// Agentic tool enumeration
enum class AgenticTool {
    FILE_READ,
    FILE_WRITE,
    FILE_APPEND,
    FILE_DELETE,
    FILE_RENAME,
    FILE_COPY,
    FILE_MOVE,
    FILE_LIST,
    FILE_EXISTS,
    DIR_CREATE,
    GIT_STATUS,
    GIT_ADD,
    GIT_COMMIT,
    GIT_PUSH,
    GIT_PULL,
    GIT_BRANCH,
    GIT_CHECKOUT,
    GIT_DIFF,
    GIT_STASH_SAVE,
    GIT_STASH_POP,
    GIT_FETCH,
    UNKNOWN
};

// Chat configuration for tool-augmented conversations
struct ChatConfig {
    std::string model = "llama2";
    int max_tool_iterations = 3;
    double temperature = 0.7;
    std::function<void(const std::string&)> on_message;
    std::function<void(const ToolResult&)> on_tool_call;
};

class AgenticToolExecutor {
public:
    explicit AgenticToolExecutor(const std::string& workspace_root = ".");
    ~AgenticToolExecutor();

    void setWorkspaceRoot(const std::string& root);
    void setAllowOutsideWorkspace(bool allow) { m_allow_outside_workspace = allow; }
    bool allowOutsideWorkspace() const { return m_allow_outside_workspace; }
    std::string getWorkspaceRoot() const { return m_workspace_root; }

    // Tool schema helpers
    std::vector<ToolSchema> getToolSchemas() const;
    std::string getAvailableTools() const;

    // Tool execution (string/json and map overloads)
    ToolResult executeTool(const std::string& tool_name, const std::string& params_json = "{}");
    ToolResult executeTool(AgenticTool tool, const std::string& params_json = "{}");
    ToolResult executeTool(const std::string& tool_name, const std::unordered_map<std::string, std::string>& params);

    // AI orchestration helpers
    ToolResult executeToolFromAI(const std::string& ai_tool_request);
    std::string generateToolPrompt(const std::vector<ToolSchema>& tools) const;
    bool extractToolCall(const std::string& ai_response, std::string& tool_name, std::string& params_json) const;
    std::string chatWithTools(const std::string& user_message,
                              std::vector<OllamaChatMessage>& conversation_history,
                              const ChatConfig& config);

    // Serialization helpers
    std::string paramsToJson(const std::map<std::string, std::string>& params) const;
    bool parseJson(const std::string& json_str, nlohmann::json& out, std::string& error) const;

    // Observability
    const ToolStats& stats() const { return m_stats; }

    // Tool name mapping
    std::string toolToString(AgenticTool tool) const;
    AgenticTool stringToTool(const std::string& name) const;

    // Configuration persistence
    void loadPersistentConfig();
    void savePersistentConfig() const;

private:
    // Path handling
    bool isPathSafe(const std::string& path) const;
    std::string normalizePath(const std::string& path) const;

    // File tools
    ToolResult executeFileRead(const nlohmann::json& params);
    ToolResult executeFileWrite(const nlohmann::json& params);
    ToolResult executeFileAppend(const nlohmann::json& params);
    ToolResult executeFileDelete(const nlohmann::json& params);
    ToolResult executeFileRename(const nlohmann::json& params);
    ToolResult executeFileCopy(const nlohmann::json& params);
    ToolResult executeFileMove(const nlohmann::json& params);
    ToolResult executeFileList(const nlohmann::json& params);
    ToolResult executeFileExists(const nlohmann::json& params);
    ToolResult executeDirCreate(const nlohmann::json& params);

    // Git tools
    ToolResult executeGitStatus(const nlohmann::json& params);
    ToolResult executeGitAdd(const nlohmann::json& params);
    ToolResult executeGitCommit(const nlohmann::json& params);
    ToolResult executeGitPush(const nlohmann::json& params);
    ToolResult executeGitPull(const nlohmann::json& params);
    ToolResult executeGitBranch(const nlohmann::json& params);
    ToolResult executeGitCheckout(const nlohmann::json& params);
    ToolResult executeGitDiff(const nlohmann::json& params);
    ToolResult executeGitStashSave(const nlohmann::json& params);
    ToolResult executeGitStashPop(const nlohmann::json& params);
    ToolResult executeGitFetch(const nlohmann::json& params);

private:
    std::string m_workspace_root;
    bool m_allow_outside_workspace = false;
    bool m_block_delete_commands = false;
    std::unique_ptr<Tools::GitClient> m_git_client;
    std::unique_ptr<OllamaClient> m_ollama_client;
    ToolStats m_stats;
};

} // namespace Backend
} // namespace RawrXD
