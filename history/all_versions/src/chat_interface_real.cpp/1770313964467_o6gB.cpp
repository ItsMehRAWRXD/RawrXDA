#include "chat_interface_real.hpp"
#include "cpu_inference_engine.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

ChatSystem::ChatSystem() {
    m_inferenceEngine = new CPUInference::CPUInferenceEngine();
    m_ownsEngine = true;
}

ChatSystem::~ChatSystem() {
    m_running = false;
    if (m_autoSaveThread.joinable()) {
        m_autoSaveThread.join();
    }
    if (m_streaming) {
        cancelStreaming();
    }
    if (m_ownsEngine && m_inferenceEngine) {
        delete m_inferenceEngine;
        m_inferenceEngine = nullptr;
    }
}

bool ChatSystem::initialize(const ModelConfig& defaultModel) {
    m_currentModel = defaultModel;
    m_models[defaultModel.modelName] = defaultModel;
    
    // Start auto-save thread
    if (m_autoSave) {
        m_autoSaveThread = std::thread([this]() {
            while (m_running) {
                std::this_thread::sleep_for(std::chrono::seconds(m_autoSaveInterval));
                if (m_currentConversationId >= 0) {
                    saveConversation(m_currentConversationId);
                }
            }
        });
        m_autoSaveThread.detach();
    }

    return true;
}

bool ChatSystem::addModel(const ModelConfig& config) {
    m_models[config.modelName] = config;
    return true;
}

bool ChatSystem::switchModel(const std::string& modelName) {
    auto it = m_models.find(modelName);
    if (it == m_models.end()) {
        std::cerr << "[ChatSystem] Model not found: " << modelName << std::endl;
        return false;
    }
    m_currentModel = it->second;
    return true;
}

ChatSystem::ModelConfig ChatSystem::getCurrentModel() const {
    return m_currentModel;
}

std::vector<std::string> ChatSystem::listAvailableModels() const {
    std::vector<std::string> models;
    for (const auto& [name, _] : m_models) {
        models.push_back(name);
    }
    return models;
}

int ChatSystem::createConversation(const std::string& title) {
    Conversation conv;
    conv.id = m_nextConversationId++;
    conv.title = title.empty() ? ("Conversation " + std::to_string(conv.id)) : title;
    conv.created = std::chrono::system_clock::now();
    conv.lastModified = conv.created;
    conv.modelUsed = m_currentModel.modelName;
    conv.archived = false;
    conv.totalTokens = 0;
    conv.averageConfidence = 1.0f;

    m_conversations[conv.id] = conv;
    m_currentConversationId = conv.id;

    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalConversations++;
    }

    return conv.id;
}

bool ChatSystem::loadConversation(int conversationId) {
    auto it = m_conversations.find(conversationId);
    if (it == m_conversations.end()) {
        std::cerr << "[ChatSystem] Conversation not found: " << conversationId << std::endl;
        return false;
    }
    m_currentConversationId = conversationId;
    updateMemory();
    return true;
}

bool ChatSystem::saveConversation(int conversationId, const std::string& filePath) {
    auto it = m_conversations.find(conversationId);
    if (it == m_conversations.end()) {
        return false;
    }

    // If no path provided, use default
    std::string path = filePath;
    if (path.empty()) {
        path = "conversations/" + std::to_string(conversationId) + ".json";
    }

    return saveConversationToDisk(it->second, path);
}

ChatSystem::Conversation ChatSystem::getCurrentConversation() const {
    if (m_currentConversationId < 0) {
        return Conversation();
    }
    auto it = m_conversations.find(m_currentConversationId);
    return (it != m_conversations.end()) ? it->second : Conversation();
}

std::vector<ChatSystem::Conversation> ChatSystem::listConversations(int offset, int limit) {
    std::vector<Conversation> result;
    int count = 0;
    
    for (auto it = m_conversations.begin(); it != m_conversations.end() && count < limit; ++it) {
        if (it->first >= offset) {
            result.push_back(it->second);
            count++;
        }
    }

    return result;
}

bool ChatSystem::deleteConversation(int conversationId) {
    if (m_currentConversationId == conversationId) {
        m_currentConversationId = -1;
    }
    return m_conversations.erase(conversationId) > 0;
}

bool ChatSystem::archiveConversation(int conversationId) {
    auto it = m_conversations.find(conversationId);
    if (it == m_conversations.end()) {
        return false;
    }
    it->second.archived = true;
    it->second.lastModified = std::chrono::system_clock::now();
    return true;
}

int ChatSystem::sendMessage(const std::string& content, MessageRole role) {
    if (m_currentConversationId < 0) {
        createConversation();
    }

    auto it = m_conversations.find(m_currentConversationId);
    if (it == m_conversations.end()) {
        return -1;
    }

    Message msg;
    msg.id = m_nextMessageId++;
    msg.role = role;
    msg.content = content;
    msg.timestamp = std::chrono::system_clock::now();
    msg.importance = calculateMessageImportance(msg);

    it->second.messages.push_back(msg);
    it->second.lastModified = msg.timestamp;
    it->second.totalTokens += tokenizeString(content);

    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalMessages++;
    }

    // Extract entities if it's a user or assistant message
    if (role == MessageRole::User || role == MessageRole::Assistant) {
        extractEntities(msg);
    }

    return msg.id;
}

std::string ChatSystem::generateResponse(const std::string& userMessage, bool streaming) {
    // Add user message
    sendMessage(userMessage, MessageRole::User);

    if (streaming) {
        std::string fullResponse;
        startStreamingResponse(
            userMessage,
            [&fullResponse](const std::string& chunk) {
                fullResponse += chunk;
            },
            []() {},
            [](const std::string& error) {
                std::cerr << "[ChatSystem] Error: " << error << std::endl;
            }
        );
        return fullResponse;
    } else {
        // Non-streaming response
        std::string response = callModel(buildContextMessages());
        sendMessage(response, MessageRole::Assistant);
        return response;
    }
}

void ChatSystem::startStreamingResponse(
    const std::string& userMessage,
    std::function<void(const std::string&)> onChunk,
    std::function<void()> onComplete,
    std::function<void(const std::string&)> onError
) {
    if (m_streaming) {
        onError("Already streaming");
        return;
    }

    m_streaming = true;
    m_streamingThread = std::thread([this, userMessage, onChunk, onComplete, onError]() {
        try {
            // Simulate streaming - in real implementation would be actual API streaming
            auto response = callModel(buildContextMessages());
            
            // Stream character by character
            for (char c : response) {
                if (!m_streaming) break;
                onChunk(std::string(1, c));
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            // Add full response to conversation
            sendMessage(response, MessageRole::Assistant);
            onComplete();

        } catch (const std::exception& e) {
            onError(std::string("Streaming error: ") + e.what());
        }
        m_streaming = false;
    });

    if (m_streamingThread.joinable()) {
        m_streamingThread.detach();
    }
}

void ChatSystem::cancelStreaming() {
    m_streaming = false;
    if (m_streamingThread.joinable()) {
        m_streamingThread.join();
    }
}

ChatSystem::Message ChatSystem::getMessage(int messageId) const {
    if (m_currentConversationId < 0) {
        return Message();
    }

    auto convIt = m_conversations.find(m_currentConversationId);
    if (convIt == m_conversations.end()) {
        return Message();
    }

    for (const auto& msg : convIt->second.messages) {
        if (msg.id == messageId) {
            return msg;
        }
    }

    return Message();
}

std::vector<ChatSystem::Message> ChatSystem::getMessages(int offset, int limit) const {
    std::vector<Message> result;

    if (m_currentConversationId < 0) {
        return result;
    }

    auto convIt = m_conversations.find(m_currentConversationId);
    if (convIt == m_conversations.end()) {
        return result;
    }

    int count = 0;
    for (size_t i = offset; i < convIt->second.messages.size() && count < limit; ++i, ++count) {
        result.push_back(convIt->second.messages[i]);
    }

    return result;
}

bool ChatSystem::editMessage(int messageId, const std::string& newContent) {
    if (m_currentConversationId < 0) {
        return false;
    }

    auto convIt = m_conversations.find(m_currentConversationId);
    if (convIt == m_conversations.end()) {
        return false;
    }

    for (auto& msg : convIt->second.messages) {
        if (msg.id == messageId) {
            msg.content = newContent;
            msg.importance = calculateMessageImportance(msg);
            convIt->second.lastModified = std::chrono::system_clock::now();
            return true;
        }
    }

    return false;
}

bool ChatSystem::deleteMessage(int messageId) {
    if (m_currentConversationId < 0) {
        return false;
    }

    auto convIt = m_conversations.find(m_currentConversationId);
    if (convIt == m_conversations.end()) {
        return false;
    }

    auto& messages = convIt->second.messages;
    auto it = std::find_if(messages.begin(), messages.end(),
        [messageId](const Message& m) { return m.id == messageId; }
    );

    if (it != messages.end()) {
        messages.erase(it);
        return true;
    }

    return false;
}

void ChatSystem::clearHistory() {
    if (m_currentConversationId >= 0) {
        auto it = m_conversations.find(m_currentConversationId);
        if (it != m_conversations.end()) {
            it->second.messages.clear();
            it->second.totalTokens = 0;
        }
    }
}

std::string ChatSystem::getContextSummary() const {
    if (m_memory.summary.empty()) {
        return "No conversation history";
    }
    return m_memory.summary;
}

ChatSystem::ConversationMemory ChatSystem::getMemory() const {
    return m_memory;
}

void ChatSystem::updateMemory() {
    if (m_currentConversationId < 0) {
        return;
    }

    auto convIt = m_conversations.find(m_currentConversationId);
    if (convIt == m_conversations.end()) {
        return;
    }

    auto& messages = convIt->second.messages;
    m_memory.totalTurns = messages.size();
    m_memory.totalTokensUsed = convIt->second.totalTokens;
    m_memory.keyTopics = extractKeyTopics(messages);
    m_memory.summary = generateSummary(messages);

    // Update important messages
    m_memory.importantMessages.clear();
    for (const auto& msg : messages) {
        if (msg.importance >= m_importance_threshold) {
            m_memory.importantMessages.emplace_back(msg.id, msg.importance);
        }
    }
}

void ChatSystem::setContextWindow(int tokens) {
    m_contextWindow = tokens;
}

int ChatSystem::getCurrentTokenCount() const {
    if (m_currentConversationId < 0) {
        return 0;
    }

    auto it = m_conversations.find(m_currentConversationId);
    return (it != m_conversations.end()) ? it->second.totalTokens : 0;
}

void ChatSystem::trimContextIfNeeded() {
    if (getCurrentTokenCount() > m_contextWindow) {
        // Remove oldest messages until under limit
        auto convIt = m_conversations.find(m_currentConversationId);
        if (convIt != m_conversations.end()) {
            while (!convIt->second.messages.empty() && getCurrentTokenCount() > m_contextWindow) {
                convIt->second.messages.erase(convIt->second.messages.begin());
            }
        }
    }
}

std::vector<ChatSystem::Message> ChatSystem::getRelevantContext(const std::string& query, int maxMessages) {
    std::vector<Message> result = searchMessages(query);
    
    if (result.size() > static_cast<size_t>(maxMessages)) {
        result.resize(maxMessages);
    }

    return result;
}

std::vector<ChatSystem::Message> ChatSystem::searchMessages(const std::string& query) {
    std::vector<Message> results;

    if (m_currentConversationId < 0) {
        return results;
    }

    auto convIt = m_conversations.find(m_currentConversationId);
    if (convIt == m_conversations.end()) {
        return results;
    }

    for (const auto& msg : convIt->second.messages) {
        if (msg.content.find(query) != std::string::npos) {
            results.push_back(msg);
        }
    }

    return results;
}

std::vector<ChatSystem::Conversation> ChatSystem::searchConversations(const std::string& query) {
    std::vector<Conversation> results;

    for (const auto& [_, conv] : m_conversations) {
        if (conv.title.find(query) != std::string::npos ||
            conv.description.find(query) != std::string::npos) {
            results.push_back(conv);
        }
    }

    return results;
}

std::vector<int> ChatSystem::findMessagesWithMentions(const std::string& mention) {
    std::vector<int> result;

    if (m_currentConversationId < 0) {
        return result;
    }

    auto convIt = m_conversations.find(m_currentConversationId);
    if (convIt == m_conversations.end()) {
        return result;
    }

    for (const auto& msg : convIt->second.messages) {
        if (std::find(msg.mentions.begin(), msg.mentions.end(), mention) != msg.mentions.end()) {
            result.push_back(msg.id);
        }
    }

    return result;
}

bool ChatSystem::exportAsJSON(int conversationId, const std::string& filePath) {
    auto it = m_conversations.find(conversationId);
    if (it == m_conversations.end()) {
        return false;
    }

    // TODO: Implement JSON export
    return true;
}

bool ChatSystem::exportAsMarkdown(int conversationId, const std::string& filePath) {
    auto it = m_conversations.find(conversationId);
    if (it == m_conversations.end()) {
        return false;
    }

    // TODO: Implement Markdown export
    return true;
}

bool ChatSystem::exportAsPDF(int conversationId, const std::string& filePath) {
    // TODO: Implement PDF export (requires PDF library)
    return false;
}

bool ChatSystem::importFromJSON(const std::string& filePath) {
    // TODO: Implement JSON import
    return false;
}

void ChatSystem::setSystemPrompt(const std::string& prompt) {
    m_currentModel.systemPrompt = prompt;
}

void ChatSystem::setTemperature(float temperature) {
    m_currentModel.temperature = std::clamp(temperature, 0.0f, 2.0f);
}

void ChatSystem::setMaxTokens(int tokens) {
    m_currentModel.maxTokens = tokens;
}

void ChatSystem::setAutoSave(bool enabled, int intervalSeconds) {
    m_autoSave = enabled;
    m_autoSaveInterval = intervalSeconds;
}

void ChatSystem::setSummarizationInterval(int messageCount) {
    m_summarizationInterval = messageCount;
}

ChatSystem::ChatStats ChatSystem::getStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

void ChatSystem::resetStats() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_stats = ChatStats();
}

int ChatSystem::createBranch(int fromMessageId) {
    // TODO: Implement conversation branching
    return createConversation();
}

int ChatSystem::switchBranch(int branchId) {
    return branchId;  // TODO: Implement branch switching
}

std::string ChatSystem::callModel(const std::string& prompt) {
    switch (m_currentModel.source) {
        case ModelSource::Local:
            return callLocalModel(prompt);
        case ModelSource::OpenAI:
            return callOpenAIAPI(prompt);
        case ModelSource::Azure:
            return callAzureAPI(prompt);
        case ModelSource::Anthropic:
            return callAnthropicAPI(prompt);
        default:
            return "";
    }
}

std::string ChatSystem::callLocalModel(const std::string& prompt) {
    if (!m_inferenceEngine || !m_inferenceEngine->IsModelLoaded()) {
        return "[Error: No local model loaded. Load a GGUF model first.]";
    }
    
    // Configure sampling parameters from current model config
    m_inferenceEngine->ConfigureSampling(
        m_currentModel.temperature,
        m_currentModel.topP,
        50,  // top_k default
        1.1f // repeat_penalty default
    );
    
    // Tokenize the prompt
    std::vector<int32_t> input_tokens = m_inferenceEngine->Tokenize(prompt);
    
    if (input_tokens.empty()) {
        return "[Error: Tokenization failed]";
    }
    
    // Generate response
    std::vector<int32_t> output_tokens = m_inferenceEngine->Generate(
        input_tokens, 
        m_currentModel.maxTokens
    );
    
    // Detokenize
    std::string response = m_inferenceEngine->Detokenize(output_tokens);
    
    // Update stats
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalTokensGenerated += output_tokens.size();
        m_stats.totalTokensUsed += (input_tokens.size() + output_tokens.size());
    }
    
    return response;
}

std::string ChatSystem::callOpenAIAPI(const std::string& prompt) {
    // TODO: Implement OpenAI API call
    return "OpenAI response placeholder";
}

std::string ChatSystem::callAzureAPI(const std::string& prompt) {
    // TODO: Implement Azure API call
    return "Azure response placeholder";
}

std::string ChatSystem::callAnthropicAPI(const std::string& prompt) {
    // TODO: Implement Anthropic API call
    return "Anthropic response placeholder";
}

std::vector<ChatSystem::Message> ChatSystem::buildContextMessages() {
    if (m_currentConversationId < 0) {
        return {};
    }

    auto it = m_conversations.find(m_currentConversationId);
    if (it == m_conversations.end()) {
        return {};
    }

    return selectMostRelevantMessages(m_contextWindow);
}

int ChatSystem::tokenizeString(const std::string& text) const {
    // Rough estimation: ~4 chars per token
    return static_cast<int>(text.length() / 4.0f);
}

std::vector<ChatSystem::Message> ChatSystem::selectMostRelevantMessages(int tokenBudget) {
    if (m_currentConversationId < 0) {
        return {};
    }

    auto it = m_conversations.find(m_currentConversationId);
    if (it == m_conversations.end()) {
        return {};
    }

    // For now, just return last N messages that fit in token budget
    std::vector<Message> result;
    int tokenCount = 0;

    auto& messages = it->second.messages;
    for (auto revIt = messages.rbegin(); revIt != messages.rend(); ++revIt) {
        int msgTokens = tokenizeString(revIt->content);
        if (tokenCount + msgTokens <= tokenBudget) {
            result.insert(result.begin(), *revIt);
            tokenCount += msgTokens;
        } else {
            break;
        }
    }

    return result;
}

ChatSystem::Message ChatSystem::summarizeMessages(const std::vector<Message>& messages) {
    Message summary;
    summary.role = MessageRole::System;
    summary.content = "Summary of previous conversation...";
    summary.timestamp = std::chrono::system_clock::now();
    return summary;
}

void ChatSystem::extractEntities(const Message& message) {
    // Extract @mentions and #tags
    std::regex atMentionRegex("@([a-zA-Z0-9_]+)");
    std::regex hashtagRegex("#([a-zA-Z0-9_]+)");

    std::sregex_iterator atBegin(message.content.begin(), message.content.end(), atMentionRegex);
    std::sregex_iterator atEnd;

    for (std::sregex_iterator it = atBegin; it != atEnd; ++it) {
        m_entityMemory["@" + it->str(1)] = message.content;
    }
}

void ChatSystem::updateImportanceScores() {
    if (m_currentConversationId < 0) {
        return;
    }

    auto it = m_conversations.find(m_currentConversationId);
    if (it == m_conversations.end()) {
        return;
    }

    for (auto& msg : it->second.messages) {
        msg.importance = calculateMessageImportance(msg);
    }
}

float ChatSystem::calculateMessageImportance(const Message& message) const {
    float importance = 0.5f;

    // User messages are more important
    if (message.role == MessageRole::User) {
        importance += 0.2f;
    }

    // Longer messages might be more important
    if (message.content.length() > 100) {
        importance += 0.1f;
    }

    // Messages with code blocks are important
    if (message.content.find("```") != std::string::npos) {
        importance += 0.1f;
    }

    return std::min(1.0f, importance);
}

std::vector<std::string> ChatSystem::extractKeyTopics(const std::vector<Message>& messages) {
    std::vector<std::string> topics;
    // TODO: Implement topic extraction
    return topics;
}

std::string ChatSystem::generateSummary(const std::vector<Message>& messages) {
    if (messages.empty()) {
        return "";
    }

    std::string summary = "Conversation summary: ";
    int count = 0;
    for (const auto& msg : messages) {
        if (msg.role == MessageRole::User && count < 3) {
            summary += msg.content.substr(0, 50) + "... ";
            count++;
        }
    }

    return summary;
}

bool ChatSystem::saveConversationToDisk(const Conversation& conv, const std::string& filePath) {
    // TODO: Implement disk save
    return true;
}

ChatSystem::Conversation ChatSystem::loadConversationFromDisk(const std::string& filePath) {
    // TODO: Implement disk load
    return Conversation();
}

std::string ChatSystem::makeHTTPRequest(const std::string& url, const std::string& method, const std::string& body) {
    // TODO: Implement HTTP request (WinHTTP or similar)
    return "";
}

std::string ChatSystem::buildOpenAIPayload(const std::vector<Message>& context) {
    // TODO: Build OpenAI JSON payload
    return "";
}

std::string ChatSystem::extractTextFromResponse(const std::string& jsonResponse) {
    // TODO: Parse JSON response and extract text
    return "";
}
