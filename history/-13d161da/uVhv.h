#pragma once
#ifndef AGENTIC_TOOLS_H
#define AGENTIC_TOOLS_H

#include "backend/ollama_client.h"

#include <nlohmann/json.hpp>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace RawrXD {
namespace Backend {

// Forward declarations
namespace Tools {
    class GitClient;
}

/**
 * @struct ToolResult
 * @brief Result of a tool execution
 */
struct ToolResult {
    bool success = false;
    std::string tool_name;
    std::string result_data;
    std::string error_message;
    int exit_code = 0;

    static ToolResult Ok(const std::string& tool, const std::string& data);
    static ToolResult Fail(const std::string& tool, const std::string& error, int code = 1);
};

enum class AgenticTool {
    UNKNOWN = 0,
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

struct ToolStats {
    uint64_t total_tool_calls = 0;
    uint64_t successful_calls = 0;
    uint64_t failed_calls = 0;
    std::unordered_map<std::string, uint64_t> tool_usage_count;
};

struct ChatConfig {
    std::string model = "";
    double temperature = 0.2;
    int max_tool_iterations = 5;

    std::function<void(const std::string&)> on_message;
    std::function<void(const ToolResult&)> on_tool_call;
};

/**
 * @class AgenticToolExecutor
 * @brief Execute tools for agentic operations
 */
class AgenticToolExecutor {
public:
    explicit AgenticToolExecutor(const std::string& workspace_root = ".");
    ~AgenticToolExecutor();

    void setWorkspaceRoot(const std::string& root);

    // Primary API used by orchestrators
    ToolResult executeTool(const std::string& tool_name, const std::unordered_map<std::string, std::string>& params);

    // JSON-string tool invocation (used by AI parsing and ToolRegistry adapter)
    ToolResult executeTool(const std::string& tool_name, const std::string& params_json);

    std::vector<ToolSchema> getToolSchemas() const;
    std::string getAvailableTools() const;

    ToolResult executeToolFromAI(const std::string& ai_tool_request);
    std::string chatWithTools(const std::string& user_message,
                              std::vector<OllamaChatMessage>& conversation_history,
                              const ChatConfig& config);

private:
    bool isPathSafe(const std::string& path) const;
    std::string normalizePath(const std::string& path) const;

    std::string toolToString(AgenticTool tool) const;
    AgenticTool stringToTool(const std::string& name) const;

    std::string paramsToJson(const std::map<std::string, std::string>& params) const;
    bool parseJson(const std::string& json_str, nlohmann::json& out, std::string& error) const;

    ToolResult executeTool(AgenticTool tool, const std::string& params_json);

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

    std::string generateToolPrompt(const std::vector<ToolSchema>& tools) const;
    bool extractToolCall(const std::string& ai_response,
                         std::string& tool_name,
                         std::string& params_json) const;

private:
    std::string m_workspace_root;
    bool m_allow_outside_workspace = false;
    std::unique_ptr<Tools::GitClient> m_git_client;
    std::unique_ptr<OllamaClient> m_ollama_client;
    ToolStats m_stats;
};

} // namespace Backend
} // namespace RawrXD

#endif
