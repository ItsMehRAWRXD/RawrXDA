#pragma once

#include <vector>
#include <string>
#include <chrono>
// <QString> removed (Qt-free build)

/**
 * @brief Represents a single message in a chat session
 */
struct ChatMessage {
    std::string role;           // "user", "assistant", "system"
    std::string content;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief Manages chat conversation history and prompt formatting
 * 
 * This class handles:
 * - Storing conversation history
 * - Building prompts with context
 * - Managing partial/streaming responses
 * - Session lifecycle
 */
class ChatSession {
private:
    std::vector<ChatMessage> messages;
    std::string session_id;
    std::string current_model;
    
public:
    ChatSession(const std::string& model_name);
    
    /**
     * @brief Add a message to the conversation history
     * @param role "user", "assistant", or "system"
     * @param content The message text
     */
    void addMessage(const std::string& role, const std::string& content);
    
    /**
     * @brief Build a prompt including recent conversation history
     * @param max_history Maximum number of recent messages to include
     * @return Formatted prompt string ready for model input
     */
    std::string buildPromptWithHistory(int max_history = 10);
    
    /**
     * @brief Get recent messages from history
     * @param count Number of recent messages to retrieve
     * @return Vector of recent ChatMessage objects
     */
    std::vector<ChatMessage> getRecentMessages(int count = 10);
    
    /**
     * @brief Clear all conversation history
     */
    void clearHistory();
    
    /**
     * @brief Get session ID
     */
    std::string getSessionId() const { return session_id; }
    
    /**
     * @brief Get current model name
     */
    std::string getModelName() const { return current_model; }
    
    /**
     * @brief Get total message count
     */
    size_t getMessageCount() const { return messages.size(); }
    
    // Streaming support
    std::string current_partial_response;
    
    /**
     * @brief Append token to partial response during streaming
     * @param token Token/text to append
     */
    void appendToPartial(const std::string& token);
    
    /**
     * @brief Finalize the partial response and add to history
     */
    void finalizePartial();
};
