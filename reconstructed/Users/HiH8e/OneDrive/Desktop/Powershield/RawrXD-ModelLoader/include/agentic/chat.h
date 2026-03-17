#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

// SCALAR-ONLY: Agentic AI chat system with multiple modes and model management

namespace RawrXD {

enum class AgentMode {
    AGENT,      // Full autonomous agent
    PLAN,       // Planning and task breakdown
    ASK,        // Q&A mode
    EDIT,       // Code editing assistant
    CONFIGURE   // Custom agent configuration
};

struct ChatMessage {
    std::string role;       // "user", "assistant", "system"
    std::string content;
    std::string timestamp;
    std::string agent_mode;
    std::string model_name;
};

struct AgentConfig {
    std::string name;
    std::string system_prompt;
    std::string model_name;
    float temperature;
    int max_tokens;
    bool auto_execute;
    std::vector<std::string> allowed_tools;
};

struct ModelInfo {
    std::string name;
    std::string path;
    std::string type;       // "gguf", "safetensors", etc.
    size_t size_bytes;
    bool is_loaded;
};

class AgenticChat {
public:
    AgenticChat(const std::string& chat_id);
    ~AgenticChat();

    // Chat operations (scalar)
    void SendMessage(const std::string& content);
    void ReceiveResponse(const std::string& response);
    void ClearHistory();
    std::vector<ChatMessage> GetHistory() const { return messages_; }
    
    // Agent mode management
    void SetMode(AgentMode mode);
    AgentMode GetMode() const { return current_mode_; }
    std::string GetModeString() const;

    // Model management
    void SetModel(const std::string& model_name);
    std::string GetCurrentModel() const { return current_model_; }
    void SetAutoModel(bool enabled) { auto_model_ = enabled; }
    bool IsAutoModel() const { return auto_model_; }

    // Agent configuration
    void SetAgentConfig(const AgentConfig& config);
    AgentConfig GetAgentConfig() const { return agent_config_; }

    // Chat identification
    std::string GetChatId() const { return chat_id_; }
    void SetTitle(const std::string& title) { title_ = title; }
    std::string GetTitle() const { return title_; }

    // Status
    bool IsBusy() const { return is_busy_; }
    void SetBusy(bool busy) { is_busy_ = busy; }

private:
    std::string chat_id_;
    std::string title_;
    AgentMode current_mode_;
    std::string current_model_;
    bool auto_model_;
    bool is_busy_;
    std::vector<ChatMessage> messages_;
    AgentConfig agent_config_;

    std::string FormatTimestamp();
};

class ChatManager {
public:
    ChatManager();
    ~ChatManager();

    // Chat management (scalar)
    std::string CreateNewChat();
    bool DeleteChat(const std::string& chat_id);
    std::shared_ptr<AgenticChat> GetChat(const std::string& chat_id);
    std::vector<std::string> GetAllChatIds();
    size_t GetChatCount() const { return chats_.size(); }

    // Active chat
    void SetActiveChat(const std::string& chat_id);
    std::string GetActiveChatId() const { return active_chat_id_; }
    std::shared_ptr<AgenticChat> GetActiveChat();

    // Model directory management
    void AddModelDirectory(const std::string& path);
    void RemoveModelDirectory(const std::string& path);
    std::vector<std::string> GetModelDirectories() const { return model_directories_; }
    void ScanModelDirectories();
    std::vector<ModelInfo> GetAvailableModels() const { return available_models_; }
    
    // Model loading (scalar)
    bool LoadModel(const std::string& model_name);
    bool UnloadModel(const std::string& model_name);
    ModelInfo* GetLoadedModel();

private:
    std::map<std::string, std::shared_ptr<AgenticChat>> chats_;
    std::string active_chat_id_;
    int next_chat_number_;
    
    std::vector<std::string> model_directories_;
    std::vector<ModelInfo> available_models_;
    std::string loaded_model_name_;

    std::string GenerateChatId();
    void ScanDirectory(const std::string& dir);
};

} // namespace RawrXD
