#pragma once
#include <string>
#include <vector>
#include <functional>
#include <ctime>

// SCALAR-ONLY: Two-way chat interface between AI and user

namespace RawrXD {

enum class MessageRole {
    USER,
    ASSISTANT,
    SYSTEM
};

struct ChatMessage {
    MessageRole role;
    std::string content;
    std::time_t timestamp;
    std::string id;
};

class ChatInterface {
public:
    ChatInterface();
    ~ChatInterface();

    // Message management (scalar)
    void SendUserMessage(const std::string& message);
    void AddAssistantMessage(const std::string& message);
    void AddSystemMessage(const std::string& message);
    void ClearHistory();
    
    // History access
    const std::vector<ChatMessage>& GetMessages() const { return messages_; }
    std::string GetConversationContext() const;
    
    // Callbacks
    void SetOnUserMessage(std::function<void(const std::string&)> callback);
    void SetOnAssistantReply(std::function<void(const std::string&)> callback);
    void SetOnStreamingToken(std::function<void(const std::string&)> callback);
    
    // Streaming support (scalar)
    void StartStreaming();
    void AppendStreamingToken(const std::string& token);
    void EndStreaming();
    bool IsStreaming() const { return is_streaming_; }

private:
    std::vector<ChatMessage> messages_;
    bool is_streaming_;
    std::string current_streaming_message_;
    
    std::function<void(const std::string&)> on_user_message_;
    std::function<void(const std::string&)> on_assistant_reply_;
    std::function<void(const std::string&)> on_streaming_token_;
    
    std::string GenerateMessageId();
    ChatMessage CreateMessage(MessageRole role, const std::string& content);
};

} // namespace RawrXD
