#include "chat_interface.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>

// SCALAR-ONLY: Two-way chat with no threading

namespace RawrXD {

ChatInterface::ChatInterface() : is_streaming_(false) {
}

ChatInterface::~ChatInterface() {
}

void ChatInterface::SendUserMessage(const std::string& message) {
    ChatMessage msg = CreateMessage(MessageRole::USER, message);
    messages_.push_back(msg);
    
    std::cout << "[USER] " << message << std::endl;
    
    if (on_user_message_) {
        on_user_message_(message);
    }
}

void ChatInterface::AddAssistantMessage(const std::string& message) {
    ChatMessage msg = CreateMessage(MessageRole::ASSISTANT, message);
    messages_.push_back(msg);
    
    std::cout << "[ASSISTANT] " << message << std::endl;
    
    if (on_assistant_reply_) {
        on_assistant_reply_(message);
    }
}

void ChatInterface::AddSystemMessage(const std::string& message) {
    ChatMessage msg = CreateMessage(MessageRole::SYSTEM, message);
    messages_.push_back(msg);
    
    std::cout << "[SYSTEM] " << message << std::endl;
}

void ChatInterface::ClearHistory() {
    messages_.clear();
    std::cout << "Chat history cleared" << std::endl;
}

std::string ChatInterface::GetConversationContext() const {
    std::ostringstream context;
    
    // Scalar: build context from message history
    for (const auto& msg : messages_) {
        switch (msg.role) {
            case MessageRole::USER:
                context << "User: ";
                break;
            case MessageRole::ASSISTANT:
                context << "Assistant: ";
                break;
            case MessageRole::SYSTEM:
                context << "System: ";
                break;
        }
        context << msg.content << "\n";
    }
    
    return context.str();
}

void ChatInterface::StartStreaming() {
    is_streaming_ = true;
    current_streaming_message_.clear();
    std::cout << "[ASSISTANT] [Streaming started]" << std::endl;
}

void ChatInterface::AppendStreamingToken(const std::string& token) {
    if (!is_streaming_) return;
    
    current_streaming_message_ += token;
    std::cout << token << std::flush;
    
    if (on_streaming_token_) {
        on_streaming_token_(token);
    }
}

void ChatInterface::EndStreaming() {
    if (!is_streaming_) return;
    
    is_streaming_ = false;
    std::cout << std::endl;
    
    // Add complete message to history
    if (!current_streaming_message_.empty()) {
        ChatMessage msg = CreateMessage(MessageRole::ASSISTANT, current_streaming_message_);
        messages_.push_back(msg);
        current_streaming_message_.clear();
    }
}

ChatMessage ChatInterface::CreateMessage(MessageRole role, const std::string& content) {
    ChatMessage msg;
    msg.role = role;
    msg.content = content;
    msg.timestamp = std::time(nullptr);
    msg.id = GenerateMessageId();
    return msg;
}

std::string ChatInterface::GenerateMessageId() {
    // Scalar: simple ID generation using timestamp and counter
    static int counter = 0;
    std::ostringstream id;
    id << "msg_" << std::time(nullptr) << "_" << (counter++);
    return id.str();
}

void ChatInterface::SetOnUserMessage(std::function<void(const std::string&)> callback) {
    on_user_message_ = callback;
}

void ChatInterface::SetOnAssistantReply(std::function<void(const std::string&)> callback) {
    on_assistant_reply_ = callback;
}

void ChatInterface::SetOnStreamingToken(std::function<void(const std::string&)> callback) {
    on_streaming_token_ = callback;
}

} // namespace RawrXD
