/**
 * @file ai_chat_assistant.cpp
 * @brief AI chat assistant for coding help implementation
 * Batch 5 - Item 71: AI chat assistant
 */

#include "ai/ai_chat_assistant.h"
#include <sstream>
#include <algorithm>
#include <regex>

namespace RawrXD::AI {

AIChatAssistant::AIChatAssistant()
    : m_initialized(false)
    , m_temperature(0.7f)
    , m_maxTokens(2000)
    , m_streaming(false)
    , m_maxHistory(50) {
    
    // Default system prompt
    m_systemPrompt = "You are a helpful AI coding assistant. You help users with programming tasks, "
                     "explain code, debug issues, and provide code suggestions. Be concise and practical.";
}

AIChatAssistant::~AIChatAssistant() {
    shutdown();
}

bool AIChatAssistant::initialize() {
    m_initialized = true;
    return true;
}

void AIChatAssistant::shutdown() {
    stopStreaming();
    m_initialized = false;
}

void AIChatAssistant::sendMessage(const std::string& message, const ChatContext& context) {
    m_currentContext = context;
    sendMessage(message);
}

void AIChatAssistant::sendMessage(const std::string& message) {
    if (!m_initialized) {
        return;
    }
    
    // Add user message to history
    ChatMessage userMsg;
    userMsg.role = MessageRole::User;
    userMsg.content = message;
    userMsg.timestamp = std::chrono::system_clock::now();
    userMsg.isStreaming = false;
    m_history.push_back(userMsg);
    
    // Trim history if needed
    trimHistory();
    
    // Generate response
    if (m_streaming) {
        generateStreamingResponse(message);
    } else {
        generateResponse(message);
    }
}

std::future<ChatResponse> AIChatAssistant::sendMessageAsync(const std::string& message, const ChatContext& context) {
    return std::async(std::launch::async, [this, message, context]() {
        m_currentContext = context;
        
        // Add user message
        ChatMessage userMsg;
        userMsg.role = MessageRole::User;
        userMsg.content = message;
        userMsg.timestamp = std::chrono::system_clock::now();
        userMsg.isStreaming = false;
        m_history.push_back(userMsg);
        
        // Generate response
        ChatResponse response;
        response.content = generateResponseContent(message);
        response.isComplete = true;
        
        // Add assistant message to history
        ChatMessage assistantMsg;
        assistantMsg.role = MessageRole::Assistant;
        assistantMsg.content = response.content;
        assistantMsg.timestamp = std::chrono::system_clock::now();
        assistantMsg.isStreaming = false;
        
        // Extract code blocks
        assistantMsg.codeBlocks = extractCodeBlocks(response.content);
        
        m_history.push_back(assistantMsg);
        
        return response;
    });
}

void AIChatAssistant::startStreaming() {
    m_streaming = true;
}

void AIChatAssistant::stopStreaming() {
    m_streaming = false;
}

bool AIChatAssistant::isStreaming() const {
    return m_streaming;
}

std::vector<ChatMessage> AIChatAssistant::getHistory() const {
    return m_history;
}

void AIChatAssistant::clearHistory() {
    m_history.clear();
    
    // Keep system message if present
    if (!m_systemPrompt.empty()) {
        ChatMessage systemMsg;
        systemMsg.role = MessageRole::System;
        systemMsg.content = m_systemPrompt;
        systemMsg.timestamp = std::chrono::system_clock::now();
        m_history.push_back(systemMsg);
    }
}

void AIChatAssistant::setMaxHistory(int messages) {
    m_maxHistory = messages;
    trimHistory();
}

void AIChatAssistant::setSystemPrompt(const std::string& prompt) {
    m_systemPrompt = prompt;
    
    // Update or add system message
    auto it = std::find_if(m_history.begin(), m_history.end(),
        [](const ChatMessage& msg) { return msg.role == MessageRole::System; });
    
    if (it != m_history.end()) {
        it->content = prompt;
    } else {
        ChatMessage systemMsg;
        systemMsg.role = MessageRole::System;
        systemMsg.content = prompt;
        systemMsg.timestamp = std::chrono::system_clock::now();
        m_history.insert(m_history.begin(), systemMsg);
    }
}

void AIChatAssistant::updateContext(const ChatContext& context) {
    m_currentContext = context;
}

void AIChatAssistant::attachFile(const std::string& filePath) {
    if (std::find(m_attachedFiles.begin(), m_attachedFiles.end(), filePath) == m_attachedFiles.end()) {
        m_attachedFiles.push_back(filePath);
    }
}

void AIChatAssistant::detachFile(const std::string& filePath) {
    auto it = std::find(m_attachedFiles.begin(), m_attachedFiles.end(), filePath);
    if (it != m_attachedFiles.end()) {
        m_attachedFiles.erase(it);
    }
}

std::vector<std::string> AIChatAssistant::getAttachedFiles() const {
    return m_attachedFiles;
}

void AIChatAssistant::explainCode(const std::string& code) {
    std::string prompt = "Please explain this code:\n\n```\n" + code + "\n```";
    sendMessage(prompt);
}

void AIChatAssistant::fixCode(const std::string& code, const std::string& error) {
    std::string prompt = "This code has an error: " + error + "\n\n```\n" + code + 
                        "\n```\n\nPlease fix it and explain the fix.";
    sendMessage(prompt);
}

void AIChatAssistant::generateTests(const std::string& code) {
    std::string prompt = "Please generate unit tests for this code:\n\n```\n" + code + 
                        "\n```\n\nInclude edge cases and error cases.";
    sendMessage(prompt);
}

void AIChatAssistant::refactorCode(const std::string& code, const std::string& instruction) {
    std::string prompt = "Please refactor this code: " + instruction + "\n\n```\n" + code + "\n```";
    sendMessage(prompt);
}

void AIChatAssistant::documentCode(const std::string& code) {
    std::string prompt = "Please add documentation/comments to this code:\n\n```\n" + code + 
                        "\n```\n\nUse appropriate documentation style for the language.";
    sendMessage(prompt);
}

void AIChatAssistant::setModel(const std::string& model) {
    m_model = model;
}

void AIChatAssistant::setTemperature(float temperature) {
    m_temperature = std::clamp(temperature, 0.0f, 2.0f);
}

void AIChatAssistant::setMaxTokens(int maxTokens) {
    m_maxTokens = maxTokens;
}

void AIChatAssistant::onMessageReceived(MessageCallback callback) {
    m_messageCallback = callback;
}

void AIChatAssistant::onStreamChunk(StreamCallback callback) {
    m_streamCallback = callback;
}

void AIChatAssistant::generateResponse(const std::string& message) {
    // Build context
    std::string context = buildContext();
    
    // Generate response
    std::string response = generateResponseContent(message);
    
    // Create assistant message
    ChatMessage assistantMsg;
    assistantMsg.role = MessageRole::Assistant;
    assistantMsg.content = response;
    assistantMsg.timestamp = std::chrono::system_clock::now();
    assistantMsg.isStreaming = false;
    
    // Extract code blocks
    assistantMsg.codeBlocks = extractCodeBlocks(response);
    
    m_history.push_back(assistantMsg);
    
    if (m_messageCallback) {
        m_messageCallback(assistantMsg);
    }
}

void AIChatAssistant::generateStreamingResponse(const std::string& message) {
    // Build context
    std::string context = buildContext();
    
    // Generate response
    std::string response = generateResponseContent(message);
    
    // Create assistant message
    ChatMessage assistantMsg;
    assistantMsg.role = MessageRole::Assistant;
    assistantMsg.content = "";
    assistantMsg.timestamp = std::chrono::system_clock::now();
    assistantMsg.isStreaming = true;
    
    // Stream response
    std::string currentContent;
    for (size_t i = 0; i < response.length(); i++) {
        if (!m_streaming) break;
        
        currentContent += response[i];
        assistantMsg.content = currentContent;
        
        if (m_streamCallback) {
            m_streamCallback(std::string(1, response[i]));
        }
        
        // Simulate streaming delay
        if (i % 10 == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    assistantMsg.isStreaming = false;
    assistantMsg.codeBlocks = extractCodeBlocks(response);
    m_history.push_back(assistantMsg);
    
    if (m_messageCallback) {
        m_messageCallback(assistantMsg);
    }
}

std::string AIChatAssistant::buildContext() {
    std::stringstream context;
    
    // Add current file context
    if (!m_currentContext.filePath.empty()) {
        context << "Current file: " << m_currentContext.filePath << "\n";
        context << "Language: " << m_currentContext.language << "\n";
    }
    
    // Add selected text
    if (!m_currentContext.selectedText.empty()) {
        context << "Selected text:\n```\n" << m_currentContext.selectedText << "\n```\n";
    }
    
    // Add open files
    if (!m_currentContext.openFiles.empty()) {
        context << "Open files: ";
        for (size_t i = 0; i < m_currentContext.openFiles.size(); i++) {
            if (i > 0) context << ", ";
            context << m_currentContext.openFiles[i];
        }
        context << "\n";
    }
    
    // Add attached files
    if (!m_attachedFiles.empty()) {
        context << "Attached files: ";
        for (size_t i = 0; i < m_attachedFiles.size(); i++) {
            if (i > 0) context << ", ";
            context << m_attachedFiles[i];
        }
        context << "\n";
    }
    
    return context.str();
}

std::string AIChatAssistant::generateResponseContent(const std::string& message) {
    // This is a simplified implementation
    // In reality, this would call an AI model API
    
    std::stringstream response;
    
    // Simple pattern matching for demo responses
    if (message.find("explain") != std::string::npos ||
        message.find("what does") != std::string::npos) {
        response << "This code appears to be a function that processes data. ";
        response << "It takes input parameters, performs some operations, and returns a result.\n\n";
        response << "Key aspects:\n";
        response << "- Input validation\n";
        response << "- Data processing\n";
        response << "- Error handling\n";
    } else if (message.find("fix") != std::string::npos ||
               message.find("error") != std::string::npos) {
        response << "Here's the fixed code:\n\n";
        response << "```cpp\n";
        response << "// Fixed version\n";
        response << "void fixedFunction() {\n";
        response << "    // Added null check\n";
        response << "    if (ptr != nullptr) {\n";
        response << "        ptr->doSomething();\n";
        response << "    }\n";
        response << "}\n";
        response << "```\n\n";
        response << "The fix adds a null pointer check before dereferencing.";
    } else if (message.find("test") != std::string::npos) {
        response << "Here are some unit tests:\n\n";
        response << "```cpp\n";
        response << "TEST(FunctionTest, BasicCase) {\n";
        response << "    EXPECT_EQ(function(2, 3), 5);\n";
        response << "}\n\n";
        response << "TEST(FunctionTest, EdgeCase) {\n";
        response << "    EXPECT_EQ(function(0, 0), 0);\n";
        response << "}\n";
        response << "```";
    } else {
        response << "I understand you're asking about: " << message << "\n\n";
        response << "Here's what I can help with:\n";
        response <> "- Explain code functionality\n";
        response << "- Fix errors and bugs\n";
        response << "- Generate unit tests\n";
        response << "- Refactor and improve code\n";
        response << "- Add documentation\n\n";
        response << "What would you like me to do?";
    }
    
    return response.str();
}

std::vector<std::string> AIChatAssistant::extractCodeBlocks(const std::string& content) {
    std::vector<std::string> blocks;
    std::regex codeBlockRegex("```[\\w]*\\n([^`]+)```");
    std::sregex_iterator iter(content.begin(), content.end(), codeBlockRegex);
    std::sregex_iterator end;
    
    for (; iter != end; ++iter) {
        blocks.push_back((*iter)[1].str());
    }
    
    return blocks;
}

void AIChatAssistant::trimHistory() {
    // Keep system message + last N messages
    int systemMessages = 0;
    for (const auto& msg : m_history) {
        if (msg.role == MessageRole::System) systemMessages++;
    }
    
    while (static_cast<int>(m_history.size()) > m_maxHistory + systemMessages) {
        // Remove oldest non-system message
        auto it = std::find_if(m_history.begin(), m_history.end(),
            [](const ChatMessage& msg) { return msg.role != MessageRole::System; });
        if (it != m_history.end()) {
            m_history.erase(it);
        }
    }
}

} // namespace RawrXD::AI
