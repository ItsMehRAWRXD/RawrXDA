#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <filesystem>
#include "../nlohmann/json.hpp"

// Use nlohmann::json directly
using json = nlohmann::json;

namespace RawrXD {
namespace Tools {
    class FileOps;
    class GitClient;
}

namespace Backend {
    class OllamaClient;
    struct OllamaChatMessage;
    struct OllamaResponse;
}

namespace Backend {

// Tool enumeration
enum class AgenticTool {
    // File operations
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
    
    // Git operations
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
    
    // Special
    UNKNOWN
};

// Tool result structure
struct ToolResult {
    bool success;
    std::string tool_name;
    std::string result_data;      // Main result as JSON string or plain text
    std::string error_message;
    int exit_code = 0;
    
    // Convenience constructors
    static ToolResult Ok(const std::string& tool, const std::string& data);
    static ToolResult Fail(const std::string& tool, const std::string& error, int code = -1);
};

// Tool schema for AI discovery
struct ToolSchema {
    std::string name;
    std::string description;
    std::map<std::string, std::string> parameters;  // param_name -> type/description
    std::vector<std::string> required_params;
};

// Main agentic tool executor
class AgenticToolExecutor {
public:
    AgenticToolExecutor(const std::string& workspace_root);
    ~AgenticToolExecutor();
    
    // Workspace management
    void setWorkspaceRoot(const std::string& root);
    std::string getWorkspaceRoot() const { return m_workspace_root; }
    
    // Tool discovery - returns JSON array of available tools
    std::string getAvailableTools() const;
    std::vector<ToolSchema> getToolSchemas() const;
    
    // Tool execution with JSON parameters
    ToolResult executeTool(const std::string& tool_name, const std::string& params_json);
    ToolResult executeTool(AgenticTool tool, const std::string& params_json);
    
    // AI integration - chat with tool calling capability
    struct ChatConfig {
        std::string model = "qwen2.5-coder:32b";
        bool enable_tools = true;
        int max_tool_iterations = 10;
        double temperature = 0.7;
        std::function<void(const std::string&)> on_message = nullptr;
        std::function<void(const ToolResult&)> on_tool_call = nullptr;
    };
    
    // Run chat with agentic tool calling loop
    std::string chatWithTools(const std::string& user_message, 
                             std::vector<OllamaChatMessage>& conversation_history,
                             const ChatConfig& config = ChatConfig());
    
    // Single tool call from AI
    ToolResult executeToolFromAI(const std::string& ai_tool_request);
    
    // Security settings
    void setAllowOutsideWorkspace(bool allow) { m_allow_outside_workspace = allow; }
    bool getAllowOutsideWorkspace() const { return m_allow_outside_workspace; }
    
    // Statistics
    struct Stats {
        int total_tool_calls = 0;
        int successful_calls = 0;
        int failed_calls = 0;
        std::map<std::string, int> tool_usage_count;
    };
    Stats getStats() const { return m_stats; }
    void resetStats() { m_stats = Stats(); }

private:
    std::string m_workspace_root;
    bool m_allow_outside_workspace = false;
    Stats m_stats;
    std::unique_ptr<Tools::GitClient> m_git_client;
    std::unique_ptr<OllamaClient> m_ollama_client;
    
    // Path validation
    bool isPathSafe(const std::string& path) const;
    std::string normalizePath(const std::string& path) const;
    
    // Tool name mapping
    AgenticTool stringToTool(const std::string& name) const;
    std::string toolToString(AgenticTool tool) const;
    
    // Individual tool implementations
    ToolResult executeFileRead(const json& params);
    ToolResult executeFileWrite(const json& params);
    ToolResult executeFileAppend(const json& params);
    ToolResult executeFileDelete(const json& params);
    ToolResult executeFileRename(const json& params);
    ToolResult executeFileCopy(const json& params);
    ToolResult executeFileMove(const json& params);
    ToolResult executeFileList(const json& params);
    ToolResult executeFileExists(const json& params);
    ToolResult executeDirCreate(const json& params);
    
    ToolResult executeGitStatus(const json& params);
    ToolResult executeGitAdd(const json& params);
    ToolResult executeGitCommit(const json& params);
    ToolResult executeGitPush(const json& params);
    ToolResult executeGitPull(const json& params);
    ToolResult executeGitBranch(const json& params);
    ToolResult executeGitCheckout(const json& params);
    ToolResult executeGitDiff(const json& params);
    ToolResult executeGitStashSave(const json& params);
    ToolResult executeGitStashPop(const json& params);
    ToolResult executeGitFetch(const json& params);
    
    // JSON helpers
    std::string paramsToJson(const std::map<std::string, std::string>& params) const;
    bool parseJson(const std::string& json_str, json& out, std::string& error) const;
    
    // AI prompt generation
    std::string generateToolPrompt(const std::vector<ToolSchema>& tools) const;
    bool extractToolCall(const std::string& ai_response, std::string& tool_name, std::string& params_json) const;
};

} // namespace Backend
} // namespace RawrXD
