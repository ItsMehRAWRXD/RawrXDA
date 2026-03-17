#include "chat_workspace.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <random>
#include <iomanip>

// SCALAR-ONLY: Implementation of comprehensive chat workspace

namespace RawrXD {
namespace fs = std::filesystem;

// ========== ChatSession Implementation ==========

ChatSession::ChatSession(const std::string& id, const std::string& title)
    : id_(id), title_(title), task_running_(false) {
    created_at_ = std::chrono::system_clock::now();
    last_activity_ = created_at_;
    
    // Default agent configuration
    agent_.name = "Default Assistant";
    agent_.instructions = "You are a helpful AI assistant.";
    agent_.delegate_enabled = false;
}

ChatSession::~ChatSession() {
    if (task_running_) {
        CancelTask();
    }
}

void ChatSession::AddMessage(const WorkspaceChatMessage& msg) {
    messages_.push_back(msg);
    UpdateActivity();
}

void ChatSession::AddUserMessage(const std::string& content) {
    WorkspaceChatMessage msg;
    msg.id = id_ + "_msg_" + std::to_string(messages_.size());
    msg.base_message.role = MessageRole::USER;
    msg.base_message.content = content;
    msg.base_message.timestamp = std::time(nullptr);
    msg.sender = "User";
    msg.is_tool_use = false;
    msg.is_tool_result = false;
    AddMessage(msg);
}

void ChatSession::AddAssistantMessage(const std::string& content) {
    WorkspaceChatMessage msg;
    msg.id = id_ + "_msg_" + std::to_string(messages_.size());
    msg.base_message.role = MessageRole::ASSISTANT;
    msg.base_message.content = content;
    msg.base_message.timestamp = std::time(nullptr);
    msg.sender = agent_.name;
    msg.is_tool_use = false;
    msg.is_tool_result = false;
    AddMessage(msg);
}

void ChatSession::AddSystemMessage(const std::string& content) {
    WorkspaceChatMessage msg;
    msg.id = id_ + "_msg_" + std::to_string(messages_.size());
    msg.base_message.role = MessageRole::SYSTEM;
    msg.base_message.content = content;
    msg.base_message.timestamp = std::time(nullptr);
    msg.sender = "System";
    msg.is_tool_use = false;
    msg.is_tool_result = false;
    AddMessage(msg);
}

void ChatSession::AddToolUse(const std::string& tool_name, const std::string& params) {
    WorkspaceChatMessage msg;
    msg.id = id_ + "_msg_" + std::to_string(messages_.size());
    msg.base_message.role = MessageRole::SYSTEM;
    msg.base_message.content = "Using tool: " + tool_name;
    msg.base_message.timestamp = std::time(nullptr);
    msg.sender = agent_.name;
    msg.metadata["tool_name"] = tool_name;
    msg.metadata["parameters"] = params;
    msg.is_tool_use = true;
    msg.is_tool_result = false;
    AddMessage(msg);
}

void ChatSession::AddToolResult(const std::string& tool_name, const std::string& result) {
    WorkspaceChatMessage msg;
    msg.id = id_ + "_msg_" + std::to_string(messages_.size());
    msg.base_message.role = MessageRole::SYSTEM;
    msg.base_message.content = result;
    msg.base_message.timestamp = std::time(nullptr);
    msg.sender = "Tool: " + tool_name;
    msg.metadata["tool_name"] = tool_name;
    msg.is_tool_use = false;
    msg.is_tool_result = true;
    AddMessage(msg);
}

void ChatSession::ClearMessages() {
    messages_.clear();
    UpdateActivity();
}

void ChatSession::DeleteMessage(const std::string& msg_id) {
    auto it = std::remove_if(messages_.begin(), messages_.end(),
        [&msg_id](const WorkspaceChatMessage& msg) { return msg.id == msg_id; });
    messages_.erase(it, messages_.end());
    UpdateActivity();
}

void ChatSession::SetAgent(const CustomAgent& agent) {
    agent_ = agent;
    UpdateActivity();
}

void ChatSession::UpdateAgentSettings(const std::map<std::string, std::string>& settings) {
    for (const auto& [key, value] : settings) {
        agent_.settings[key] = value;
    }
    agent_.modified_at = std::chrono::system_clock::now();
    UpdateActivity();
}

void ChatSession::LoadAgentFromFile(const std::string& prompt_file) {
    std::ifstream file(prompt_file);
    if (!file.is_open()) {
        AddSystemMessage("Error: Could not open agent file: " + prompt_file);
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    agent_.instructions = buffer.str();
    agent_.prompt_file_path = prompt_file;
    agent_.modified_at = std::chrono::system_clock::now();
    
    AddSystemMessage("Loaded agent from: " + prompt_file);
    UpdateActivity();
}

void ChatSession::SaveAgentToFile(const std::string& prompt_file) {
    std::ofstream file(prompt_file);
    if (!file.is_open()) {
        AddSystemMessage("Error: Could not save agent to: " + prompt_file);
        return;
    }

    file << agent_.instructions;
    agent_.prompt_file_path = prompt_file;
    
    AddSystemMessage("Saved agent to: " + prompt_file);
    UpdateActivity();
}

void ChatSession::GenerateChatInstructions() {
    // This would call AI to generate custom instructions based on conversation
    AddSystemMessage("Generating custom instructions based on chat history...");
    // TODO: Integrate with inference engine to analyze chat and create prompt
    UpdateActivity();
}

void ChatSession::AddContext(const ContextItem& item) {
    // Check if already exists
    auto it = std::find_if(context_items_.begin(), context_items_.end(),
        [&item](const ContextItem& ctx) { return ctx.path == item.path; });
    
    if (it != context_items_.end()) {
        *it = item;  // Update existing
    } else {
        context_items_.push_back(item);
    }
    UpdateActivity();
}

void ChatSession::RemoveContext(const std::string& path) {
    auto it = std::remove_if(context_items_.begin(), context_items_.end(),
        [&path](const ContextItem& item) { return item.path == path; });
    context_items_.erase(it, context_items_.end());
    UpdateActivity();
}

void ChatSession::ClearContext() {
    // Keep pinned items
    auto it = std::remove_if(context_items_.begin(), context_items_.end(),
        [](const ContextItem& item) { return !item.is_pinned; });
    context_items_.erase(it, context_items_.end());
    UpdateActivity();
}

void ChatSession::PinContext(const std::string& path, bool pin) {
    for (auto& item : context_items_) {
        if (item.path == path) {
            item.is_pinned = pin;
            UpdateActivity();
            return;
        }
    }
}

void ChatSession::HandleFileDrop(const std::string& file_path) {
    // Create hotlink (copy as path format)
    std::string hotlink = CreateHotlink(file_path);
    
    // Add to context
    ContextItem item;
    item.type = ContextItemType::FILE;
    item.name = fs::path(file_path).filename().string();
    item.path = file_path;
    item.last_accessed = std::chrono::system_clock::now();
    item.is_pinned = false;
    
    // Try to load content if file is small enough (<1MB)
    if (fs::exists(file_path) && fs::file_size(file_path) < 1024 * 1024) {
        std::ifstream file(file_path);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            item.content = buffer.str();
            item.size_bytes = item.content.size();
        }
    }
    
    AddContext(item);
    AddSystemMessage("Added file: " + hotlink);
}

void ChatSession::HandleFolderDrop(const std::string& folder_path) {
    std::string hotlink = CreateHotlink(folder_path);
    
    ContextItem item;
    item.type = ContextItemType::FOLDER;
    item.name = fs::path(folder_path).filename().string();
    item.path = folder_path;
    item.last_accessed = std::chrono::system_clock::now();
    item.is_pinned = false;
    
    AddContext(item);
    AddSystemMessage("Added folder: " + hotlink);
}

std::string ChatSession::CreateHotlink(const std::string& path) {
    // Windows-style path with quotes (copy as path format)
    return "\"" + path + "\"";
}

void ChatSession::StartTask(const std::string& task_description) {
    task_running_ = true;
    current_task_ = task_description;
    AddSystemMessage("Started task: " + task_description);
    UpdateActivity();
}

void ChatSession::CancelTask() {
    if (task_running_) {
        task_running_ = false;
        AddSystemMessage("Cancelled task: " + current_task_);
        current_task_.clear();
        
        if (cancel_callback_) {
            cancel_callback_();
        }
        UpdateActivity();
    }
}

void ChatSession::DelegateToAgent(const std::string& agent_name, const std::string& task) {
    if (!agent_.delegate_enabled) {
        AddSystemMessage("Error: Delegation not enabled for this agent");
        return;
    }
    
    AddSystemMessage("Delegating to agent '" + agent_name + "': " + task);
    // TODO: Create new chat session with specified agent
    UpdateActivity();
}

void ChatSession::UpdateActivity() {
    last_activity_ = std::chrono::system_clock::now();
}

void ChatSession::ExportToFile(const std::string& file_path) {
    std::ofstream file(file_path);
    if (!file.is_open()) return;

    file << "# Chat Export: " << title_ << "\n";
    file << "# Created: " << std::chrono::system_clock::to_time_t(created_at_) << "\n";
    file << "# Agent: " << agent_.name << "\n\n";

    for (const auto& msg : messages_) {
        file << "---\n";
        file << "Sender: " << msg.sender << "\n";
        file << "Role: " << static_cast<int>(msg.base_message.role) << "\n";
        file << "Time: " << msg.base_message.timestamp << "\n";
        file << "Content:\n" << msg.base_message.content << "\n";
    }
}

void ChatSession::ImportFromFile(const std::string& file_path) {
    // TODO: Parse exported chat file and reconstruct messages
    AddSystemMessage("Importing chat from: " + file_path);
}

// ========== ChatWorkspace Implementation ==========

ChatWorkspace::ChatWorkspace() 
    : active_chat_(nullptr), max_recent_items_(100), auto_save_enabled_(true) {
    history_dir_ = "C:\\Users\\HiH8e\\OneDrive\\Desktop\\Powershield\\chat_history";
    
    // Create history directory if it doesn't exist
    fs::create_directories(history_dir_);
    
    LoadHistory();
    LoadCustomAgentsFromDisk();
}

ChatWorkspace::~ChatWorkspace() {
    if (auto_save_enabled_) {
        SaveHistory();
        SaveCustomAgentsToDisk();
    }
}

ChatSession* ChatWorkspace::CreateNewChat(const std::string& title) {
    std::string id = GenerateChatId();
    auto session = std::make_unique<ChatSession>(id, title);
    ChatSession* ptr = session.get();
    
    chats_.push_back(std::move(session));
    active_chat_ = ptr;
    
    return ptr;
}

ChatSession* ChatWorkspace::CreateNewChatEditor() {
    std::string title = "Chat Editor - " + std::to_string(chats_.size() + 1);
    ChatSession* chat = CreateNewChat(title);
    chat->AddSystemMessage("Chat editor created for focused file editing tasks");
    return chat;
}

ChatSession* ChatWorkspace::CreateNewChatWindow() {
    std::string title = "Chat Window - " + std::to_string(chats_.size() + 1);
    ChatSession* chat = CreateNewChat(title);
    chat->AddSystemMessage("New chat window created for parallel conversations");
    return chat;
}

void ChatWorkspace::CloseChat(const std::string& chat_id) {
    auto it = std::find_if(chats_.begin(), chats_.end(),
        [&chat_id](const std::unique_ptr<ChatSession>& chat) {
            return chat->GetId() == chat_id;
        });
    
    if (it != chats_.end()) {
        if (active_chat_ == it->get()) {
            active_chat_ = chats_.empty() ? nullptr : chats_[0].get();
        }
        chats_.erase(it);
    }
}

void ChatWorkspace::SwitchToChat(const std::string& chat_id) {
    ChatSession* chat = GetChatById(chat_id);
    if (chat) {
        active_chat_ = chat;
    }
}

ChatSession* ChatWorkspace::GetChatById(const std::string& chat_id) {
    auto it = std::find_if(chats_.begin(), chats_.end(),
        [&chat_id](const std::unique_ptr<ChatSession>& chat) {
            return chat->GetId() == chat_id;
        });
    
    return (it != chats_.end()) ? it->get() : nullptr;
}

void ChatWorkspace::SaveHistory() {
    // Save each chat to individual file
    for (const auto& chat : chats_) {
        std::string filename = history_dir_ + "\\" + chat->GetId() + ".chat";
        chat->ExportToFile(filename);
    }
    
    // Save workspace metadata
    std::string meta_file = history_dir_ + "\\workspace.meta";
    std::ofstream file(meta_file);
    if (file.is_open()) {
        file << "chat_count:" << chats_.size() << "\n";
        if (active_chat_) {
            file << "active_chat:" << active_chat_->GetId() << "\n";
        }
    }
}

void ChatWorkspace::LoadHistory() {
    if (!fs::exists(history_dir_)) return;

    // Load all .chat files
    for (const auto& entry : fs::directory_iterator(history_dir_)) {
        if (entry.path().extension() == ".chat") {
            std::string chat_id = entry.path().stem().string();
            auto session = std::make_unique<ChatSession>(chat_id, "Restored Chat");
            session->ImportFromFile(entry.path().string());
            chats_.push_back(std::move(session));
        }
    }
}

void ChatWorkspace::ClearHistory() {
    chats_.clear();
    active_chat_ = nullptr;
    
    // Delete history files
    if (fs::exists(history_dir_)) {
        for (const auto& entry : fs::directory_iterator(history_dir_)) {
            if (entry.path().extension() == ".chat") {
                fs::remove(entry.path());
            }
        }
    }
}

void ChatWorkspace::DeleteChatHistory(const std::string& chat_id) {
    std::string filename = history_dir_ + "\\" + chat_id + ".chat";
    if (fs::exists(filename)) {
        fs::remove(filename);
    }
    CloseChat(chat_id);
}

void ChatWorkspace::ExportHistory(const std::string& directory) {
    fs::create_directories(directory);
    
    for (const auto& chat : chats_) {
        std::string filename = directory + "\\" + chat->GetTitle() + ".txt";
        chat->ExportToFile(filename);
    }
}

void ChatWorkspace::AddRecentItem(const RecentItem& item) {
    // Check if item already exists
    auto it = std::find_if(recent_items_.begin(), recent_items_.end(),
        [&item](const RecentItem& recent) { return recent.path == item.path; });
    
    if (it != recent_items_.end()) {
        // Update existing item (move to front, increment count)
        it->timestamp = std::chrono::system_clock::now();
        it->access_count++;
        
        // Move to front
        RecentItem updated = *it;
        recent_items_.erase(it);
        recent_items_.insert(recent_items_.begin(), updated);
    } else {
        // Add new item at front
        recent_items_.insert(recent_items_.begin(), item);
    }
    
    TrimRecentItems();
}

void ChatWorkspace::ClearRecentItems() {
    recent_items_.clear();
}

std::vector<RecentItem> ChatWorkspace::SearchRecentItems(const std::string& query) {
    std::vector<RecentItem> results;
    
    for (const auto& item : recent_items_) {
        if (item.name.find(query) != std::string::npos ||
            item.path.find(query) != std::string::npos) {
            results.push_back(item);
        }
    }
    
    return results;
}

std::vector<RecentItem> ChatWorkspace::GetRecentByType(ContextItemType type) {
    std::vector<RecentItem> results;
    
    for (const auto& item : recent_items_) {
        if (item.type == type) {
            results.push_back(item);
        }
    }
    
    return results;
}

void ChatWorkspace::SaveCustomAgent(const CustomAgent& agent) {
    custom_agents_[agent.name] = agent;
    SaveCustomAgentsToDisk();
}

void ChatWorkspace::LoadCustomAgent(const std::string& name) {
    auto it = custom_agents_.find(name);
    if (it != custom_agents_.end() && active_chat_) {
        active_chat_->SetAgent(it->second);
    }
}

void ChatWorkspace::DeleteCustomAgent(const std::string& name) {
    custom_agents_.erase(name);
    SaveCustomAgentsToDisk();
}

std::vector<CustomAgent> ChatWorkspace::GetAllCustomAgents() {
    std::vector<CustomAgent> agents;
    for (const auto& [name, agent] : custom_agents_) {
        agents.push_back(agent);
    }
    return agents;
}

void ChatWorkspace::ImportAgentFromFile(const std::string& file_path) {
    CustomAgent agent;
    agent.name = fs::path(file_path).stem().string();
    agent.prompt_file_path = file_path;
    
    std::ifstream file(file_path);
    if (file.is_open()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        agent.instructions = buffer.str();
        agent.created_at = std::chrono::system_clock::now();
        
        SaveCustomAgent(agent);
    }
}

void ChatWorkspace::ExportAgentToFile(const std::string& agent_name, const std::string& file_path) {
    auto it = custom_agents_.find(agent_name);
    if (it != custom_agents_.end()) {
        std::ofstream file(file_path);
        if (file.is_open()) {
            file << it->second.instructions;
        }
    }
}

void ChatWorkspace::AddMCPServer(const std::string& server_name, const std::string& endpoint) {
    mcp_servers_.push_back(server_name + ":" + endpoint);
}

void ChatWorkspace::RemoveMCPServer(const std::string& server_name) {
    auto it = std::remove_if(mcp_servers_.begin(), mcp_servers_.end(),
        [&server_name](const std::string& server) {
            return server.find(server_name) == 0;
        });
    mcp_servers_.erase(it, mcp_servers_.end());
}

void ChatWorkspace::RegisterTool(const std::string& tool_name, const std::string& description) {
    available_tools_[tool_name] = description;
}

void ChatWorkspace::UnregisterTool(const std::string& tool_name) {
    available_tools_.erase(tool_name);
}

std::vector<std::string> ChatWorkspace::GetAvailableTools() const {
    std::vector<std::string> tools;
    for (const auto& [name, desc] : available_tools_) {
        tools.push_back(name);
    }
    return tools;
}

void ChatWorkspace::UpdateOpenEditors() {
    // This would integrate with MultiTabEditor to get open files
    // TODO: Connect to tab editor component
}

void ChatWorkspace::UpdateSourceControl() {
    // This would run git commands to get status
    // TODO: Integrate with git client
}

void ChatWorkspace::UpdateProblems() {
    // This would gather compiler errors, linter warnings
    // TODO: Connect to diagnostics system
}

void ChatWorkspace::UpdateSymbols() {
    // This would parse current file for symbols
    // TODO: Connect to syntax engine
}

void ChatWorkspace::UpdateTools() {
    // Already handled by RegisterTool
}

std::vector<WorkspaceChatMessage> ChatWorkspace::SearchAllChats(const std::string& query) {
    std::vector<WorkspaceChatMessage> results;
    
    for (const auto& chat : chats_) {
        for (const auto& msg : chat->GetMessages()) {
            if (msg.base_message.content.find(query) != std::string::npos) {
                results.push_back(msg);
            }
        }
    }
    
    return results;
}

std::vector<ChatSession*> ChatWorkspace::FindChatsByTitle(const std::string& title) {
    std::vector<ChatSession*> results;
    
    for (const auto& chat : chats_) {
        if (chat->GetTitle().find(title) != std::string::npos) {
            results.push_back(chat.get());
        }
    }
    
    return results;
}

std::string ChatWorkspace::GenerateChatId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    const char* hex = "0123456789abcdef";
    std::string id = "chat_";
    
    for (int i = 0; i < 16; ++i) {
        id += hex[dis(gen)];
    }
    
    return id;
}

void ChatWorkspace::TrimRecentItems() {
    if (recent_items_.size() > static_cast<size_t>(max_recent_items_)) {
        recent_items_.resize(max_recent_items_);
    }
}

void ChatWorkspace::LoadCustomAgentsFromDisk() {
    std::string agents_dir = history_dir_ + "\\custom_agents";
    if (!fs::exists(agents_dir)) {
        fs::create_directories(agents_dir);
        return;
    }
    
    for (const auto& entry : fs::directory_iterator(agents_dir)) {
        if (entry.path().extension() == ".agent") {
            ImportAgentFromFile(entry.path().string());
        }
    }
}

void ChatWorkspace::SaveCustomAgentsToDisk() {
    std::string agents_dir = history_dir_ + "\\custom_agents";
    fs::create_directories(agents_dir);
    
    for (const auto& [name, agent] : custom_agents_) {
        std::string filename = agents_dir + "\\" + name + ".agent";
        ExportAgentToFile(name, filename);
    }
}

} // namespace RawrXD
