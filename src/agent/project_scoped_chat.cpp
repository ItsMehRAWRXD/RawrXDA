// ============================================================================
// project_scoped_chat.cpp — Real project-scoped chat history management
// ============================================================================
// Chat history and context tied to workspace/project, not global
// Persists under %APPDATA%\RawrXD\workspaces\<hash>\
// Provides per-project conversation memory for agentic workflows
// ============================================================================

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <pwd.h>
#endif

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <algorithm>

namespace fs = std::filesystem;

namespace RawrXD {
namespace Agent {

// ============================================================================
// Chat Message
// ============================================================================

enum class MessageRole {
    User,
    Assistant,
    System
};

struct ChatMessage {
    MessageRole role;
    std::string content;
    std::chrono::system_clock::time_point timestamp;
    std::string metadata; // JSON metadata (e.g., model, tokens)
};

// ============================================================================
// Project Chat Session
// ============================================================================

class ProjectChatSession {
private:
    std::string m_projectPath;
    std::string m_projectHash;
    std::string m_storagePath;
    std::vector<ChatMessage> m_messages;
    std::mutex m_mutex;
    bool m_initialized = false;
    
    // Session metadata
    struct Metadata {
        std::string projectName;
        std::chrono::system_clock::time_point created;
        std::chrono::system_clock::time_point lastAccessed;
        size_t totalMessages = 0;
    } m_metadata;
    
public:
    ProjectChatSession() = default;
    ~ProjectChatSession() {
        if (m_initialized) {
            save();
        }
    }
    
    // Initialize session for project
    bool initialize(const std::string& projectPath) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        m_projectPath = projectPath;
        m_projectHash = computeHash(projectPath);
        
        // Construct storage path
        m_storagePath = getStoragePath(m_projectHash);
        
        // Create directory if needed
        try {
            fs::create_directories(fs::path(m_storagePath).parent_path());
        } catch (const std::exception& ex) {
            fprintf(stderr, "[ProjectChatSession] Failed to create storage dir: %s\n",
                    ex.what());
            return false;
        }
        
        // Load existing history if available
        load();
        
        m_metadata.projectName = fs::path(projectPath).filename().string();
        m_metadata.lastAccessed = std::chrono::system_clock::now();
        
        m_initialized = true;
        
        fprintf(stderr, "[ProjectChatSession] Initialized for project: %s\n",
                projectPath.c_str());
        fprintf(stderr, "[ProjectChatSession] Storage: %s\n", m_storagePath.c_str());
        fprintf(stderr, "[ProjectChatSession] Loaded %zu messages\n", m_messages.size());
        
        return true;
    }
    
    // Add message to history
    void addMessage(MessageRole role, const std::string& content, 
                    const std::string& metadata = "") {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        ChatMessage msg;
        msg.role = role;
        msg.content = content;
        msg.timestamp = std::chrono::system_clock::now();
        msg.metadata = metadata;
        
        m_messages.push_back(msg);
        m_metadata.totalMessages++;
        m_metadata.lastAccessed = std::chrono::system_clock::now();
        
        // Auto-save every 10 messages
        if (m_messages.size() % 10 == 0) {
            saveUnlocked();
        }
    }
    
    // Get recent messages
    std::vector<ChatMessage> getRecentMessages(size_t count = 10) const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_mutex));
        
        if (m_messages.size() <= count) {
            return m_messages;
        }
        
        return std::vector<ChatMessage>(
            m_messages.end() - count,
            m_messages.end()
        );
    }
    
    // Get all messages
    const std::vector<ChatMessage>& getAllMessages() const {
        return m_messages;
    }
    
    // Clear history for this project
    void clearHistory() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        m_messages.clear();
        m_metadata.totalMessages = 0;
        
        saveUnlocked();
        
        fprintf(stderr, "[ProjectChatSession] Cleared history for %s\n",
                m_projectPath.c_str());
    }
    
    // Get context window (for prompt building)
    std::string getContextWindow(size_t maxTokens = 4096) const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_mutex));
        
        std::string context;
        size_t estimatedTokens = 0;
        
        // Add messages in reverse order until we hit token limit
        for (auto it = m_messages.rbegin(); it != m_messages.rend(); ++it) {
            const auto& msg = *it;
            
            // Rough token estimation (1 token ≈ 4 chars)
            size_t msgTokens = msg.content.size() / 4 + 10; // +10 for role/overhead
            
            if (estimatedTokens + msgTokens > maxTokens) {
                break;
            }
            
            std::string roleStr = getRoleString(msg.role);
            context = roleStr + ": " + msg.content + "\n\n" + context;
            estimatedTokens += msgTokens;
        }
        
        return context;
    }
    
    // Save history to disk
    bool save() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return saveUnlocked();
    }
    
private:
    bool saveUnlocked() {
        if (!m_initialized) {
            return false;
        }
        
        try {
            std::ofstream file(m_storagePath, std::ios::binary);
            if (!file.is_open()) {
                return false;
            }
            
            // Write header
            file << "RawrXD Project Chat v1\n";
            file << "Project: " << m_projectPath << "\n";
            file << "Hash: " << m_projectHash << "\n";
            file << "Messages: " << m_messages.size() << "\n";
            file << "---\n";
            
            // Write messages
            for (const auto& msg : m_messages) {
                file << "ROLE: " << static_cast<int>(msg.role) << "\n";
                
                auto timeT = std::chrono::system_clock::to_time_t(msg.timestamp);
                file << "TIME: " << timeT << "\n";
                
                if (!msg.metadata.empty()) {
                    file << "META: " << msg.metadata << "\n";
                }
                
                file << "CONTENT_LEN: " << msg.content.size() << "\n";
                file << msg.content << "\n";
                file << "END_MSG\n";
            }
            
            file.close();
            return true;
            
        } catch (const std::exception& ex) {
            fprintf(stderr, "[ProjectChatSession] Save failed: %s\n", ex.what());
            return false;
        }
    }
    
    bool load() {
        try {
            std::ifstream file(m_storagePath, std::ios::binary);
            if (!file.is_open()) {
                // No existing history
                return true;
            }
            
            std::string line;
            
            // Read header
            std::getline(file, line); // Version
            std::getline(file, line); // Project
            std::getline(file, line); // Hash
            std::getline(file, line); // Messages
            std::getline(file, line); // ---
            
            // Read messages
            while (std::getline(file, line)) {
                if (line.find("ROLE: ") == 0) {
                    ChatMessage msg;
                    
                    int roleInt = std::stoi(line.substr(6));
                    msg.role = static_cast<MessageRole>(roleInt);
                    
                    std::getline(file, line); // TIME
                    if (line.find("TIME: ") == 0) {
                        time_t timeT = std::stoll(line.substr(6));
                        msg.timestamp = std::chrono::system_clock::from_time_t(timeT);
                    }
                    
                    std::getline(file, line); // META or CONTENT_LEN
                    if (line.find("META: ") == 0) {
                        msg.metadata = line.substr(6);
                        std::getline(file, line); // CONTENT_LEN
                    }
                    
                    if (line.find("CONTENT_LEN: ") == 0) {
                        size_t contentLen = std::stoull(line.substr(13));
                        
                        // Read content
                        msg.content.resize(contentLen);
                        file.read(&msg.content[0], contentLen);
                        
                        std::getline(file, line); // Consume newline
                        std::getline(file, line); // END_MSG
                        
                        m_messages.push_back(msg);
                    }
                }
            }
            
            file.close();
            return true;
            
        } catch (const std::exception& ex) {
            fprintf(stderr, "[ProjectChatSession] Load failed: %s\n", ex.what());
            return false;
        }
    }
    
    std::string computeHash(const std::string& path) {
        // Simple hash of path (real impl could use SHA256)
        uint32_t hash = 5381;
        for (char c : path) {
            hash = ((hash << 5) + hash) + c;
        }
        
        char buf[32];
        snprintf(buf, sizeof(buf), "%08x", hash);
        return std::string(buf);
    }
    
    std::string getStoragePath(const std::string& projectHash) {
#ifdef _WIN32
        char appData[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appData))) {
            return std::string(appData) + "\\RawrXD\\workspaces\\" + projectHash + "\\chat_history.txt";
        }
        return ".\\rawrxd_workspaces\\" + projectHash + "\\chat_history.txt";
#else
        const char* home = getenv("HOME");
        if (!home) {
            struct passwd* pw = getpwuid(getuid());
            home = pw ? pw->pw_dir : ".";
        }
        return std::string(home) + "/.rawrxd/workspaces/" + projectHash + "/chat_history.txt";
#endif
    }
    
    std::string getRoleString(MessageRole role) const {
        switch (role) {
            case MessageRole::User: return "User";
            case MessageRole::Assistant: return "Assistant";
            case MessageRole::System: return "System";
            default: return "Unknown";
        }
    }
};

// ============================================================================
// Global Session Manager
// ============================================================================

static std::unique_ptr<ProjectChatSession> g_currentSession;
static std::mutex g_sessionMutex;

} // namespace Agent
} // namespace RawrXD

// ============================================================================
// C API
// ============================================================================

extern "C" {

bool RawrXD_Agent_InitProjectChat(const char* projectPath) {
    std::lock_guard<std::mutex> lock(RawrXD::Agent::g_sessionMutex);
    
    RawrXD::Agent::g_currentSession = std::make_unique<RawrXD::Agent::ProjectChatSession>();
    return RawrXD::Agent::g_currentSession->initialize(projectPath ? projectPath : ".");
}

void RawrXD_Agent_AddChatMessage(int role, const char* content, const char* metadata) {
    std::lock_guard<std::mutex> lock(RawrXD::Agent::g_sessionMutex);
    
    if (!RawrXD::Agent::g_currentSession) {
        return;
    }
    
    RawrXD::Agent::g_currentSession->addMessage(
        static_cast<RawrXD::Agent::MessageRole>(role),
        content ? content : "",
        metadata ? metadata : ""
    );
}

void RawrXD_Agent_ClearChatHistory() {
    std::lock_guard<std::mutex> lock(RawrXD::Agent::g_sessionMutex);
    
    if (!RawrXD::Agent::g_currentSession) {
        return;
    }
    
    RawrXD::Agent::g_currentSession->clearHistory();
}

bool RawrXD_Agent_SaveChatHistory() {
    std::lock_guard<std::mutex> lock(RawrXD::Agent::g_sessionMutex);
    
    if (!RawrXD::Agent::g_currentSession) {
        return false;
    }
    
    return RawrXD::Agent::g_currentSession->save();
}

} // extern "C"
