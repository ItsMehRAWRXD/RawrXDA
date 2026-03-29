#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "backend/ollama_client.h"

namespace RawrXD {
namespace Tools {
class GitClient;
}

namespace Backend {

enum class AgenticTool {
    UNKNOWN,
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
};

struct ToolSchema {
    std::string name;
    std::string description;
    std::map<std::string, std::string> parameters;
    std::vector<std::string> required_params;
};

struct ToolResult {
    bool success = false;
    std::string tool_name;
    std::string result_data;
    std::string error_message;
    int exit_code = 0;

    static ToolResult Ok(const std::string& tool, const std::string& data);
    static ToolResult Fail(const std::string& tool, const std::string& error, int code = -1);
};

struct ChatConfig {
    std::string model = "llama3";
    float temperature = 0.2f;
    int max_tool_iterations = 3;
    std::function<void(const std::string&)> on_message;
    std::function<void(const ToolResult&)> on_tool_call;
};

class AgenticToolExecutor {
public:
    explicit AgenticToolExecutor(const std::string& workspace_root = std::string());
    ~AgenticToolExecutor();

    void setWorkspaceRoot(const std::string& root);
    bool isPathSafe(const std::string& path) const;
    std::string normalizePath(const std::string& path) const;

    std::string toolToString(AgenticTool tool) const;
    AgenticTool stringToTool(const std::string& name) const;

    std::vector<ToolSchema> getToolSchemas() const;
    std::string getAvailableTools() const;
    std::string paramsToJson(const std::map<std::string, std::string>& params) const;

    ToolResult executeTool(const std::string& tool_name, const std::string& params_json);
    ToolResult executeTool(AgenticTool tool, const std::string& params_json);
    ToolResult executeToolFromAI(const std::string& ai_tool_request);

    std::string chatWithTools(const std::string& user_message,
                              std::vector<OllamaChatMessage>& conversation_history,
                              const ChatConfig& config);

private:
    bool parseJson(const std::string& json_str, nlohmann::json& out, std::string& error) const;
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

    void loadPersistentConfig();
    void savePersistentConfig() const;

    std::string generateToolPrompt(const std::vector<ToolSchema>& tools) const;
    bool extractToolCall(const std::string& ai_response, std::string& tool_name, std::string& params_json) const;

    std::string m_workspace_root;
    bool m_allow_outside_workspace = false;
    bool m_block_delete_commands = false;

    // Tracks long-running agent/model iteration progress.
    bool m_iteration_busy = false;
    int m_iteration_current = 0;
    int m_iteration_total = 0;
    std::string m_iteration_phase = "idle";
    std::string m_iteration_message;

    std::unique_ptr<Tools::GitClient> m_git_client;
    std::unique_ptr<OllamaClient> m_ollama_client;

    // Tool execution statistics
    struct ToolStats {
        uint64_t total_tool_calls = 0;
        uint64_t successful_calls = 0;
        uint64_t failed_calls = 0;
        std::map<std::string, uint64_t> tool_usage_count;
    } m_stats;
};

} // namespace Backend
} // namespace RawrXD
