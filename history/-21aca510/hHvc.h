#pragma once

#include <string>
#include <map>
#include <vector>
#include <functional>
#include <memory>

// ============================================================================
// AGENT ORCHESTRATION - MULTI-AGENT CHAT WITH VOICE
// ============================================================================

struct AgentConfig {
    std::string id;
    std::string name;
    std::string role;
    std::string model;
};

struct ChatMessage {
    std::string agentId;
    std::string sender;     // "user" or agent id
    std::string text;
    std::string timestamp;
    bool isVoice = false;
};

class AgentOrchestra {
public:
    AgentOrchestra();
    ~AgentOrchestra();

    // Agent management
    void addAgent(const AgentConfig& config);
    void setActiveAgent(const std::string& agentId);
    std::string getActiveAgent() const { return m_activeAgent; }
    std::vector<AgentConfig> getAgents() const;

    // Chat operations
    void sendMessage(const std::string& message);
    void sendVoiceMessage(const std::string& voiceData);
    std::vector<ChatMessage> getChatHistory() const { return m_chatHistory; }
    void clearHistory();

    // Voice operations
    void startVoiceInput();
    void stopVoiceInput();
    void setVoiceAccent(const std::string& accent);
    std::string getVoiceAccent() const { return m_voiceAccent; }

    // Response generation
    std::string generateResponse(const std::string& message);

    // Callbacks
    using MessageCallback = std::function<void(const ChatMessage&)>;
    using VoiceCallback = std::function<void(const std::string&)>;
    using ErrorCallback = std::function<void(const std::string&)>;

    void setMessageCallback(MessageCallback cb) { m_messageCallback = cb; }
    void setVoiceCallback(VoiceCallback cb) { m_voiceCallback = cb; }
    void setErrorCallback(ErrorCallback cb) { m_errorCallback = cb; }

private:
    std::map<std::string, AgentConfig> m_agents;
    std::string m_activeAgent;
    std::vector<ChatMessage> m_chatHistory;
    std::string m_voiceAccent = "American";
    bool m_recordingVoice = false;

    MessageCallback m_messageCallback;
    VoiceCallback m_voiceCallback;
    ErrorCallback m_errorCallback;
};

#endif
