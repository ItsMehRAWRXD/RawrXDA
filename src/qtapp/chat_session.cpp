#include "chat_session.hpp"
#include <sstream>
#include <uuid/uuid.h>
#include <iostream>

ChatSession::ChatSession(const std::string& model_name) 
    : current_model(model_name)
{
    // Generate a simple session ID (in production use UUID)
    session_id = "session_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    std::cout << "[ChatSession] Created session: " << session_id << " for model: " << model_name << std::endl;
}

void ChatSession::addMessage(const std::string& role, const std::string& content) {
    ChatMessage msg;
    msg.role = role;
    msg.content = content;
    msg.timestamp = std::chrono::system_clock::now();
    messages.push_back(msg);
}

std::string ChatSession::buildPromptWithHistory(int max_history) {
    std::stringstream prompt;
    
    // Add conversation history
    if (!messages.empty()) {
        // Include recent messages (up to max_history)
        size_t start_idx = messages.size() > max_history ? messages.size() - max_history : 0;
        
        for (size_t i = start_idx; i < messages.size(); ++i) {
            const auto& msg = messages[i];
            if (msg.role == "user") {
                prompt << "User: " << msg.content << "\n";
            } else if (msg.role == "assistant") {
                prompt << "Assistant: " << msg.content << "\n";
            } else if (msg.role == "system") {
                prompt << "System: " << msg.content << "\n";
            }
        }
    }
    
    // Add prompt for next response
    prompt << "Assistant: ";
    
    return prompt.str();
}

std::vector<ChatMessage> ChatSession::getRecentMessages(int count) {
    std::vector<ChatMessage> recent;
    
    if (messages.empty()) return recent;
    
    size_t start_idx = messages.size() > count ? messages.size() - count : 0;
    for (size_t i = start_idx; i < messages.size(); ++i) {
        recent.push_back(messages[i]);
    }
    
    return recent;
}

void ChatSession::clearHistory() {
    messages.clear();
    current_partial_response.clear();
    std::cout << "[ChatSession] History cleared" << std::endl;
}

void ChatSession::appendToPartial(const std::string& token) {
    current_partial_response += token;
}

void ChatSession::finalizePartial() {
    if (!current_partial_response.empty()) {
        addMessage("assistant", current_partial_response);
        current_partial_response.clear();
    }
}
