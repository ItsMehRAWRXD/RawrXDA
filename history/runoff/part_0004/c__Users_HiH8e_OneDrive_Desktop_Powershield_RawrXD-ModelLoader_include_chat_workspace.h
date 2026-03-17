#pragma once
#include "chat_interface.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <functional>

// SCALAR-ONLY: Comprehensive chat workspace with history, custom agents, and context management

namespace RawrXD {

// Extended message metadata for workspace
struct WorkspaceChatMessage {
    std::string id;
    ChatMessage base_message;  // Use existing ChatMessage
    std::string sender;  // "User" or agent name
    std::map<std::string, std::string> metadata;  // For tool calls, etc.
    bool is_tool_use;
    bool is_tool_result;
};

// Custom agent configuration
struct CustomAgent {
    std::string name;
    std::string prompt_file_path;  // Path to .txt/.md prompt file
    std::string instructions;      // Loaded instructions
    std::map<std::string, std::string> settings;  // Temperature, max_tokens, etc.
    std::vector<std::string> toolsets;  // Enabled tools
    std::vector<std::string> mcp_servers;  // Connected MCP servers
    bool delegate_enabled;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point modified_at;
};

// Context item types
enum class ContextItemType {
    OPEN_EDITOR,
    FILE,
    FOLDER,
    INSTRUCTION,
    SCREENSHOT,
    WINDOW,
    SOURCE_CONTROL,
    PROBLEM,
    SYMBOL,
    TOOL
};

struct ContextItem {
    ContextItemType type;
    std::string name;
    std::string path;  // File path or identifier
    std::string content;  // Cached content for quick access
    size_t size_bytes;
    std::chrono::system_clock::time_point last_accessed;
    bool is_pinned;  // User can pin important context
};

// Recent items tracker (100 latest)
struct RecentItem {
    std::string name;
    std::string path;
    ContextItemType type;
    std::chrono::system_clock::time_point timestamp;
    int access_count;  // Frequency tracking
};

// Chat session (individual chat instance)
class ChatSession {
public:
    ChatSession(const std::string& id, const std::string& title);
    ~ChatSession();

    // Message management (scalar)
    void AddMessage(const WorkspaceChatMessage& msg);
    void AddUserMessage(const std::string& content);
    void AddAssistantMessage(const std::string& content);
    void AddSystemMessage(const std::string& content);
    void AddToolUse(const std::string& tool_name, const std::string& params);
    void AddToolResult(const std::string& tool_name, const std::string& result);

    const std::vector<WorkspaceChatMessage>& GetMessages() const { return messages_; }
    void ClearMessages();
    void DeleteMessage(const std::string& msg_id);

    // Agent configuration
    void SetAgent(const CustomAgent& agent);
    const CustomAgent& GetAgent() const { return agent_; }
    void UpdateAgentSettings(const std::map<std::string, std::string>& settings);
    void LoadAgentFromFile(const std::string& prompt_file);
    void SaveAgentToFile(const std::string& prompt_file);
    void GenerateChatInstructions();  // AI-assisted instruction generation

    // Context management (scalar)
    void AddContext(const ContextItem& item);
    void RemoveContext(const std::string& path);
    void ClearContext();
    void PinContext(const std::string& path, bool pin);
    const std::vector<ContextItem>& GetContext() const { return context_items_; }
    
    // Drag-and-drop file handling
    void HandleFileDrop(const std::string& file_path);
    void HandleFolderDrop(const std::string& folder_path);
    std::string CreateHotlink(const std::string& path);  // Returns "copy as path" format

    // Task control
    void StartTask(const std::string& task_description);
    void CancelTask();  // Stop ongoing task
    bool IsTaskRunning() const { return task_running_; }
    std::string GetCurrentTask() const { return current_task_; }

    // Delegation
    void DelegateToAgent(const std::string& agent_name, const std::string& task);
    
    // Session info
    std::string GetId() const { return id_; }
    std::string GetTitle() const { return title_; }
    void SetTitle(const std::string& title) { title_ = title; }
    std::chrono::system_clock::time_point GetCreatedAt() const { return created_at_; }
    std::chrono::system_clock::time_point GetLastActivityAt() const { return last_activity_; }
    void UpdateActivity();

    // Export/Import
    void ExportToFile(const std::string& file_path);
    void ImportFromFile(const std::string& file_path);

private:
    std::string id_;
    std::string title_;
    std::vector<WorkspaceChatMessage> messages_;
    CustomAgent agent_;
    std::vector<ContextItem> context_items_;
    std::chrono::system_clock::time_point created_at_;
    std::chrono::system_clock::time_point last_activity_;
    
    bool task_running_;
    std::string current_task_;
    std::function<void()> cancel_callback_;
};

// Workspace chat history manager
class ChatWorkspace {
public:
    ChatWorkspace();
    ~ChatWorkspace();

    // Session management (scalar)
    ChatSession* CreateNewChat(const std::string& title = "New Chat");
    ChatSession* CreateNewChatEditor();  // Dedicated chat for file editing
    ChatSession* CreateNewChatWindow();  // Separate chat window
    void CloseChat(const std::string& chat_id);
    void SwitchToChat(const std::string& chat_id);
    ChatSession* GetActiveChat() { return active_chat_; }
    ChatSession* GetChatById(const std::string& chat_id);
    const std::vector<std::unique_ptr<ChatSession>>& GetAllChats() const { return chats_; }

    // History management (scalar)
    void SaveHistory();
    void LoadHistory();
    void ClearHistory();
    void DeleteChatHistory(const std::string& chat_id);
    void ExportHistory(const std::string& directory);

    // Recent items (100 latest, scalar)
    void AddRecentItem(const RecentItem& item);
    const std::vector<RecentItem>& GetRecentItems() const { return recent_items_; }
    void ClearRecentItems();
    std::vector<RecentItem> SearchRecentItems(const std::string& query);
    std::vector<RecentItem> GetRecentByType(ContextItemType type);

    // Custom agents library
    void SaveCustomAgent(const CustomAgent& agent);
    void LoadCustomAgent(const std::string& name);
    void DeleteCustomAgent(const std::string& name);
    std::vector<CustomAgent> GetAllCustomAgents();
    void ImportAgentFromFile(const std::string& file_path);
    void ExportAgentToFile(const std::string& agent_name, const std::string& file_path);

    // MCP servers management
    void AddMCPServer(const std::string& server_name, const std::string& endpoint);
    void RemoveMCPServer(const std::string& server_name);
    std::vector<std::string> GetAvailableMCPServers() const { return mcp_servers_; }

    // Toolsets management
    void RegisterTool(const std::string& tool_name, const std::string& description);
    void UnregisterTool(const std::string& tool_name);
    std::vector<std::string> GetAvailableTools() const;

    // Context providers (auto-update for active chat)
    void UpdateOpenEditors();  // Scan open tabs
    void UpdateSourceControl();  // Git status, changes
    void UpdateProblems();  // Compiler errors, warnings
    void UpdateSymbols();  // Code symbols in current file
    void UpdateTools();  // Available tools

    // Search across all chats
    std::vector<WorkspaceChatMessage> SearchAllChats(const std::string& query);
    std::vector<ChatSession*> FindChatsByTitle(const std::string& title);

    // Settings
    void SetHistoryDirectory(const std::string& path) { history_dir_ = path; }
    std::string GetHistoryDirectory() const { return history_dir_; }
    void SetMaxRecentItems(int max) { max_recent_items_ = max; }
    void SetAutoSaveEnabled(bool enabled) { auto_save_enabled_ = enabled; }

private:
    std::vector<std::unique_ptr<ChatSession>> chats_;
    ChatSession* active_chat_;
    std::vector<RecentItem> recent_items_;
    int max_recent_items_;  // Default 100
    std::string history_dir_;
    bool auto_save_enabled_;

    std::map<std::string, CustomAgent> custom_agents_;
    std::vector<std::string> mcp_servers_;
    std::map<std::string, std::string> available_tools_;

    // Internal helpers (scalar)
    std::string GenerateChatId();
    void TrimRecentItems();  // Keep only latest 100
    void LoadCustomAgentsFromDisk();
    void SaveCustomAgentsToDisk();
};

// Chat UI Actions
struct ChatUIAction {
    enum Type {
        NEW_CHAT,
        NEW_CHAT_EDITOR,
        NEW_CHAT_WINDOW,
        CLOSE_CHAT,
        SWITCH_CHAT,
        CANCEL_TASK,
        DELEGATE_TO_AGENT,
        ADD_CONTEXT_FILE,
        ADD_CONTEXT_FOLDER,
        ADD_CONTEXT_SCREENSHOT,
        CONFIGURE_TOOLS,
        LOAD_CUSTOM_AGENT,
        SAVE_CUSTOM_AGENT,
        GENERATE_INSTRUCTIONS,
        OPEN_CHAT_SETTINGS,
        EXPORT_CHAT,
        IMPORT_CHAT,
        SEARCH_HISTORY
    };

    Type action;
    std::string parameter;
    std::map<std::string, std::string> metadata;
};

} // namespace RawrXD
