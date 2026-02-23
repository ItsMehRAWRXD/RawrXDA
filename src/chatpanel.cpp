// Chat Panel - AI Chat Interface Implementation
// Manages chat sessions and message history

#include "chatpanel.h"
#include "logging/logger.h"
#include <chrono>
#include <mutex>

static Logger s_chatPanelLogger("ChatPanel");

namespace ChatPanel {

class ChatPanelImpl : public IChatPanel {
public:
    ChatPanelImpl() {
        m_session.id = "default";
        m_session.title = "New Chat";
        m_session.createdAt = currentTimestamp();
    }
    
    void sendMessage(const std::string& text) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        Message userMsg;
        userMsg.role = Message::Role::User;
        userMsg.content = text;
        userMsg.timestamp = currentTimestamp();
        m_session.messages.push_back(userMsg);
        m_session.updatedAt = userMsg.timestamp;
        
        s_chatPanelLogger.info("User: {}", text.substr(0, 80));
        
        // Notify listeners
        if (m_onMessage) {
            m_onMessage(userMsg);
        }
    }
    
    void clearHistory() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_session.messages.clear();
        m_session.updatedAt = currentTimestamp();
        s_chatPanelLogger.info("History cleared");
    }
    
    std::vector<Message> getHistory() const override {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_session.messages;
    }
    
    void setSystemPrompt(const std::string& prompt) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_systemPrompt = prompt;
        
        // Add or update system message at start of history
        if (!m_session.messages.empty() && 
            m_session.messages[0].role == Message::Role::System) {
            m_session.messages[0].content = prompt;
        } else {
            Message sysMsg;
            sysMsg.role = Message::Role::System;
            sysMsg.content = prompt;
            sysMsg.timestamp = currentTimestamp();
            m_session.messages.insert(m_session.messages.begin(), sysMsg);
        }
    }
    
    void onMessageReceived(MessageCallback callback) override {
        m_onMessage = callback;
    }
    
    void onStreamChunk(StreamCallback callback) override {
        m_onStream = callback;
    }
    
private:
    static int64_t currentTimestamp() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
    
    Session m_session;
    std::string m_systemPrompt;
    MessageCallback m_onMessage;
    StreamCallback m_onStream;
    mutable std::mutex m_mutex;
};

} // namespace ChatPanel

