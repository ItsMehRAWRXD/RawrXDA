#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <functional>

namespace RawrXD { namespace Backend {

// Forward declarations
namespace Tools {
    class GitClient;
}
class OllamaClient;

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

/**
 * @class AgenticToolExecutor
 * @brief Execute tools for agentic operations
 */
class AgenticToolExecutor {
public:
    explicit AgenticToolExecutor(const std::string& workspace_root = ".");
    ~AgenticToolExecutor();
    
    void setWorkspaceRoot(const std::string& root);
    ToolResult executeTool(const std::string& tool_name, const std::unordered_map<std::string, std::string>& params);
    // Safety toggle for destructive operations
    void toggleBlockDelete(bool enabled);
    bool isBlockDeleteEnabled() const;
    
private:
    std::string m_workspace_root;
    bool m_allow_outside_workspace;
    std::unique_ptr<Tools::GitClient> m_git_client;
    std::unique_ptr<OllamaClient> m_ollama_client;
    bool m_block_delete_commands = true; // default: block destructive deletions on startup
    void loadPersistentConfig();
    void savePersistentConfig() const;
};

}} // namespace RawrXD::Backend

#endif // AGENTIC_TOOLS_H
