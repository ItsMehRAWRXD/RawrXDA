#pragma once
/**
 * @file ai_chat_assistant.h
 * @brief AI chat assistant for coding help
 * Batch 5 - Item 71: AI chat assistant
 */

#include <string>
#include <vector>
#include <functional>
#include <future>

namespace RawrXD::AI {

enum class MessageRole {
    System,
    User,
    Assistant,
    Tool
};

struct ChatMessage {
    MessageRole role;
    std::string content;
    std::chrono::system_clock::time_point timestamp;
    std::vector<std::string> codeBlocks;
    bool isStreaming;
};

struct ChatContext {
    std::string filePath;
    std::string selectedText;
    std::string language;
    std::vector<std::string> openFiles;
    std::string projectContext;
};

struct ChatResponse {
    std::string content;
    bool isComplete;
    std::string error;
};

class AIChatAssistant {
public:
    AIChatAssistant();
    ~AIChatAssistant();

    // Initialization
    bool initialize();
    void shutdown();

    // Chat
    void sendMessage(const std::string& message, const ChatContext& context);
    void sendMessage(const std::string& message);
    std::future<ChatResponse> sendMessageAsync(const std::string& message, const ChatContext& context);

    // Streaming
    void startStreaming();
    void stopStreaming();
    bool isStreaming() const;

    // History
    std::vector<ChatMessage> getHistory() const;
    void clearHistory();
    void setMaxHistory(int messages);

    // Context
    void setSystemPrompt(const std::string& prompt);
    void updateContext(const ChatContext& context);
    void attachFile(const std::string& filePath);
    void detachFile(const std::string& filePath);
    std::vector<std::string> getAttachedFiles() const;

    // Quick actions
    void explainCode(const std::string& code);
    void fixCode(const std::string& code, const std::string& error);
    void generateTests(const std::string& code);
    void refactorCode(const std::string& code, const std::string& instruction);
    void documentCode(const std::string& code);

    // Configuration
    void setModel(const std::string& model);
    void setTemperature(float temperature);
    void setMaxTokens(int maxTokens);

    // Events
    using MessageCallback = std::function<void(const ChatMessage&)>;
    using StreamCallback = std::function<void(const std::string& chunk)>;
    void onMessageReceived(MessageCallback callback);
    void onStreamChunk(StreamCallback callback);

private:
    std::vector<ChatMessage> m_history;
    std::string m_systemPrompt;
    ChatContext m_currentContext;
    std::vector<std::string> m_attachedFiles;
    std::string m_model;
    float m_temperature{0.7f};
    int m_maxTokens{2000};
    int m_maxHistory{20};
    bool m_streaming{false};

    MessageCallback m_messageCallback;
    StreamCallback m_streamCallback;

    std::string buildPrompt(const std::string& message);
    ChatMessage createMessage(MessageRole role, const std::string& content);
    void trimHistory();
    void notifyMessageReceived(const ChatMessage& message);
};

// Global instance
AIChatAssistant& getAIChatAssistant();

} // namespace RawrXD::AI
