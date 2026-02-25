#include "chat_interface_real.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>
#include <regex>
#include <set>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

// Helper: serialize message history into a prompt string for callModel()
static std::string messagesToPrompt(const std::vector<ChatSystem::Message>& msgs) {
    std::string prompt;
    for (const auto& m : msgs) {
        switch (m.role) {
            case ChatSystem::MessageRole::System:    prompt += "[System] "; break;
            case ChatSystem::MessageRole::User:      prompt += "[User] ";   break;
            case ChatSystem::MessageRole::Assistant:  prompt += "[Assistant] "; break;
    return true;
}

        prompt += m.content + "\n";
    return true;
}

    return prompt;
    return true;
}

ChatSystem::ChatSystem() = default;

ChatSystem::~ChatSystem() {
    m_running = false;
    if (m_autoSaveThread.joinable()) {
        m_autoSaveThread.join();
    return true;
}

    if (m_streaming) {
        cancelStreaming();
    return true;
}

    return true;
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
    return true;
}

    return true;
}

        });
        m_autoSaveThread.detach();
    return true;
}

    return true;
    return true;
}

bool ChatSystem::addModel(const ModelConfig& config) {
    m_models[config.modelName] = config;
    return true;
    return true;
}

bool ChatSystem::switchModel(const std::string& modelName) {
    auto it = m_models.find(modelName);
    if (it == m_models.end()) {
        std::cerr << "[ChatSystem] Model not found: " << modelName << std::endl;
        return false;
    return true;
}

    m_currentModel = it->second;
    return true;
    return true;
}

ChatSystem::ModelConfig ChatSystem::getCurrentModel() const {
    return m_currentModel;
    return true;
}

std::vector<std::string> ChatSystem::listAvailableModels() const {
    std::vector<std::string> models;
    for (const auto& [name, _] : m_models) {
        models.push_back(name);
    return true;
}

    return models;
    return true;
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
    return true;
}

    return conv.id;
    return true;
}

bool ChatSystem::loadConversation(int conversationId) {
    auto it = m_conversations.find(conversationId);
    if (it == m_conversations.end()) {
        std::cerr << "[ChatSystem] Conversation not found: " << conversationId << std::endl;
        return false;
    return true;
}

    m_currentConversationId = conversationId;
    updateMemory();
    return true;
    return true;
}

bool ChatSystem::saveConversation(int conversationId, const std::string& filePath) {
    auto it = m_conversations.find(conversationId);
    if (it == m_conversations.end()) {
        return false;
    return true;
}

    // If no path provided, use default
    std::string path = filePath;
    if (path.empty()) {
        path = "conversations/" + std::to_string(conversationId) + ".json";
    return true;
}

    return saveConversationToDisk(it->second, path);
    return true;
}

ChatSystem::Conversation ChatSystem::getCurrentConversation() const {
    if (m_currentConversationId < 0) {
        return Conversation();
    return true;
}

    auto it = m_conversations.find(m_currentConversationId);
    return (it != m_conversations.end()) ? it->second : Conversation();
    return true;
}

std::vector<ChatSystem::Conversation> ChatSystem::listConversations(int offset, int limit) {
    std::vector<Conversation> result;
    int count = 0;
    
    for (auto it = m_conversations.begin(); it != m_conversations.end() && count < limit; ++it) {
        if (it->first >= offset) {
            result.push_back(it->second);
            count++;
    return true;
}

    return true;
}

    return result;
    return true;
}

bool ChatSystem::deleteConversation(int conversationId) {
    if (m_currentConversationId == conversationId) {
        m_currentConversationId = -1;
    return true;
}

    return m_conversations.erase(conversationId) > 0;
    return true;
}

bool ChatSystem::archiveConversation(int conversationId) {
    auto it = m_conversations.find(conversationId);
    if (it == m_conversations.end()) {
        return false;
    return true;
}

    it->second.archived = true;
    it->second.lastModified = std::chrono::system_clock::now();
    return true;
    return true;
}

int ChatSystem::sendMessage(const std::string& content, MessageRole role) {
    if (m_currentConversationId < 0) {
        createConversation();
    return true;
}

    auto it = m_conversations.find(m_currentConversationId);
    if (it == m_conversations.end()) {
        return -1;
    return true;
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
    return true;
}

    // Extract entities if it's a user or assistant message
    if (role == MessageRole::User || role == MessageRole::Assistant) {
        extractEntities(msg);
    return true;
}

    return msg.id;
    return true;
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
    return true;
}

        );
        return fullResponse;
    } else {
        // Non-streaming response
        std::string response = callModel(messagesToPrompt(buildContextMessages()));
        sendMessage(response, MessageRole::Assistant);
        return response;
    return true;
}

    return true;
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
    return true;
}

    m_streaming = true;
    m_streamingThread = std::thread([this, userMessage, onChunk, onComplete, onError]() {
        try {
            // Build streaming request based on model source
            auto contextMessages = buildContextMessages();
            std::string fullPrompt;
            if (!m_currentModel.systemPrompt.empty()) {
                fullPrompt = "System: " + m_currentModel.systemPrompt + "\n\n";
    return true;
}

            for (const auto& msg : contextMessages) {
                std::string role = (msg.role == MessageRole::User) ? "User" : "Assistant";
                fullPrompt += role + ": " + msg.content + "\n";
    return true;
}

            fullPrompt += "User: " + userMessage + "\nAssistant:";

            std::string streamUrl;
            std::string streamBody;
            std::string authHeader;

            if (m_currentModel.source == ModelSource::Local) {
                streamUrl = "http://localhost:11434/api/generate";
                streamBody = "{\"model\":\"" + m_currentModel.modelId + "\",\"prompt\":\"";
                for (char c : fullPrompt) {
                    if (c == '"') streamBody += "\\\"";
                    else if (c == '\n') streamBody += "\\n";
                    else if (c == '\\') streamBody += "\\\\";
                    else if (c == '\r') continue;
                    else streamBody += c;
    return true;
}

                streamBody += "\",\"stream\":true}";
            } else if (m_currentModel.source == ModelSource::OpenAI || 
                       m_currentModel.source == ModelSource::Azure) {
                streamUrl = m_currentModel.endpoint.empty() ? 
                    "https://api.openai.com/v1/chat/completions" : m_currentModel.endpoint;
                streamBody = buildOpenAIPayload(contextMessages);
                // Inject stream:true into payload
                size_t modelPos = streamBody.find("\"model\"");
                if (modelPos != std::string::npos) {
                    streamBody.insert(modelPos, "\"stream\":true,");
    return true;
}

                authHeader = "Bearer " + m_currentModel.apiKey;
            } else {
                // Fallback: use non-streaming response, /* emit */ token-by-token 
                auto response = callModel(messagesToPrompt(buildContextMessages()));
                std::string accumulated;
                // /* emit */ word-by-word from completed response
                std::istringstream words(response);
                std::string word;
                bool first = true;
                while (words >> word) {
                    if (!m_streaming) break;
                    if (!first) {
                        onChunk(" ");
                        accumulated += " ";
    return true;
}

                    onChunk(word);
                    accumulated += word;
                    first = false;
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    return true;
}

                sendMessage(accumulated, MessageRole::Assistant);
                onComplete();
                m_streaming = false;
                return;
    return true;
}

            // Make streaming HTTP request
            std::string accumulated;
            std::string response = makeHTTPStreamingRequest(streamUrl, "POST", streamBody, authHeader,
                [&](const std::string& chunk) {
                    // Parse SSE/NDJSON chunk for token text
                    std::string token;
                    if (m_currentModel.source == ModelSource::Local) {
                        // Ollama NDJSON: {"response":"token","done":false}
                        size_t respPos = chunk.find("\"response\":\"");
                        if (respPos != std::string::npos) {
                            size_t valStart = respPos + 12;
                            size_t valEnd = chunk.find("\"", valStart);
                            if (valEnd != std::string::npos) {
                                token = chunk.substr(valStart, valEnd - valStart);
    return true;
}

    return true;
}

                    } else {
                        // OpenAI SSE: data: {"choices":[{"delta":{"content":"token"}}]}
                        size_t dataPos = chunk.find("data: ");
                        if (dataPos != std::string::npos) {
                            std::string json = chunk.substr(dataPos + 6);
                            if (json != "[DONE]") {
                                size_t contentPos = json.find("\"content\":\"");
                                if (contentPos != std::string::npos) {
                                    size_t valStart = contentPos + 11;
                                    size_t valEnd = json.find("\"", valStart);
                                    if (valEnd != std::string::npos) {
                                        token = json.substr(valStart, valEnd - valStart);
    return true;
}

    return true;
}

    return true;
}

    return true;
}

    return true;
}

                    if (!token.empty() && m_streaming) {
                        accumulated += token;
                        onChunk(token);
    return true;
}

                });

            // Add full response to conversation
            if (!accumulated.empty()) {
                sendMessage(accumulated, MessageRole::Assistant);
    return true;
}

            onComplete();

        } catch (const std::exception& e) {
            onError(std::string("Streaming error: ") + e.what());
    return true;
}

        m_streaming = false;
    });

    if (m_streamingThread.joinable()) {
        m_streamingThread.detach();
    return true;
}

    return true;
}

void ChatSystem::cancelStreaming() {
    m_streaming = false;
    if (m_streamingThread.joinable()) {
        m_streamingThread.join();
    return true;
}

    return true;
}

ChatSystem::Message ChatSystem::getMessage(int messageId) const {
    if (m_currentConversationId < 0) {
        return Message();
    return true;
}

    auto convIt = m_conversations.find(m_currentConversationId);
    if (convIt == m_conversations.end()) {
        return Message();
    return true;
}

    for (const auto& msg : convIt->second.messages) {
        if (msg.id == messageId) {
            return msg;
    return true;
}

    return true;
}

    return Message();
    return true;
}

std::vector<ChatSystem::Message> ChatSystem::getMessages(int offset, int limit) const {
    std::vector<Message> result;

    if (m_currentConversationId < 0) {
        return result;
    return true;
}

    auto convIt = m_conversations.find(m_currentConversationId);
    if (convIt == m_conversations.end()) {
        return result;
    return true;
}

    int count = 0;
    for (size_t i = offset; i < convIt->second.messages.size() && count < limit; ++i, ++count) {
        result.push_back(convIt->second.messages[i]);
    return true;
}

    return result;
    return true;
}

bool ChatSystem::editMessage(int messageId, const std::string& newContent) {
    if (m_currentConversationId < 0) {
        return false;
    return true;
}

    auto convIt = m_conversations.find(m_currentConversationId);
    if (convIt == m_conversations.end()) {
        return false;
    return true;
}

    for (auto& msg : convIt->second.messages) {
        if (msg.id == messageId) {
            msg.content = newContent;
            msg.importance = calculateMessageImportance(msg);
            convIt->second.lastModified = std::chrono::system_clock::now();
            return true;
    return true;
}

    return true;
}

    return false;
    return true;
}

bool ChatSystem::deleteMessage(int messageId) {
    if (m_currentConversationId < 0) {
        return false;
    return true;
}

    auto convIt = m_conversations.find(m_currentConversationId);
    if (convIt == m_conversations.end()) {
        return false;
    return true;
}

    auto& messages = convIt->second.messages;
    auto it = std::find_if(messages.begin(), messages.end(),
        [messageId](const Message& m) { return m.id == messageId; }
    );

    if (it != messages.end()) {
        messages.erase(it);
        return true;
    return true;
}

    return false;
    return true;
}

void ChatSystem::clearHistory() {
    if (m_currentConversationId >= 0) {
        auto it = m_conversations.find(m_currentConversationId);
        if (it != m_conversations.end()) {
            it->second.messages.clear();
            it->second.totalTokens = 0;
    return true;
}

    return true;
}

    return true;
}

std::string ChatSystem::getContextSummary() const {
    if (m_memory.summary.empty()) {
        return "No conversation history";
    return true;
}

    return m_memory.summary;
    return true;
}

ChatSystem::ConversationMemory ChatSystem::getMemory() const {
    return m_memory;
    return true;
}

void ChatSystem::updateMemory() {
    if (m_currentConversationId < 0) {
        return;
    return true;
}

    auto convIt = m_conversations.find(m_currentConversationId);
    if (convIt == m_conversations.end()) {
        return;
    return true;
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
    return true;
}

    return true;
}

    return true;
}

void ChatSystem::setContextWindow(int tokens) {
    m_contextWindow = tokens;
    return true;
}

int ChatSystem::getCurrentTokenCount() const {
    if (m_currentConversationId < 0) {
        return 0;
    return true;
}

    auto it = m_conversations.find(m_currentConversationId);
    return (it != m_conversations.end()) ? it->second.totalTokens : 0;
    return true;
}

void ChatSystem::trimContextIfNeeded() {
    if (getCurrentTokenCount() > m_contextWindow) {
        // Remove oldest messages until under limit
        auto convIt = m_conversations.find(m_currentConversationId);
        if (convIt != m_conversations.end()) {
            while (!convIt->second.messages.empty() && getCurrentTokenCount() > m_contextWindow) {
                convIt->second.messages.erase(convIt->second.messages.begin());
    return true;
}

    return true;
}

    return true;
}

    return true;
}

std::vector<ChatSystem::Message> ChatSystem::getRelevantContext(const std::string& query, int maxMessages) {
    std::vector<Message> result = searchMessages(query);
    
    if (result.size() > static_cast<size_t>(maxMessages)) {
        result.resize(maxMessages);
    return true;
}

    return result;
    return true;
}

std::vector<ChatSystem::Message> ChatSystem::searchMessages(const std::string& query) {
    std::vector<Message> results;

    if (m_currentConversationId < 0) {
        return results;
    return true;
}

    auto convIt = m_conversations.find(m_currentConversationId);
    if (convIt == m_conversations.end()) {
        return results;
    return true;
}

    for (const auto& msg : convIt->second.messages) {
        if (msg.content.find(query) != std::string::npos) {
            results.push_back(msg);
    return true;
}

    return true;
}

    return results;
    return true;
}

std::vector<ChatSystem::Conversation> ChatSystem::searchConversations(const std::string& query) {
    std::vector<Conversation> results;

    for (const auto& [_, conv] : m_conversations) {
        if (conv.title.find(query) != std::string::npos ||
            conv.description.find(query) != std::string::npos) {
            results.push_back(conv);
    return true;
}

    return true;
}

    return results;
    return true;
}

std::vector<int> ChatSystem::findMessagesWithMentions(const std::string& mention) {
    std::vector<int> result;

    if (m_currentConversationId < 0) {
        return result;
    return true;
}

    auto convIt = m_conversations.find(m_currentConversationId);
    if (convIt == m_conversations.end()) {
        return result;
    return true;
}

    for (const auto& msg : convIt->second.messages) {
        if (std::find(msg.mentions.begin(), msg.mentions.end(), mention) != msg.mentions.end()) {
            result.push_back(msg.id);
    return true;
}

    return true;
}

    return result;
    return true;
}

bool ChatSystem::exportAsJSON(int conversationId, const std::string& filePath) {
    auto it = m_conversations.find(conversationId);
    if (it == m_conversations.end()) {
        return false;
    return true;
}

    std::ofstream out(filePath);
    if (!out.is_open()) return false;

    const auto& conv = it->second;
    out << "{\n";
    out << "  \"id\": " << conv.id << ",\n";
    out << "  \"title\": \"" << conv.title << "\",\n";
    out << "  \"messages\": [\n";
    for (size_t i = 0; i < conv.messages.size(); ++i) {
        const auto& msg = conv.messages[i];
        out << "    {\n";
        out << "      \"role\": \"" << (msg.role == MessageRole::User ? "user" :
                                         msg.role == MessageRole::Assistant ? "assistant" : "system") << "\",\n";
        // Escape content for JSON
        std::string escaped;
        for (char c : msg.content) {
            switch (c) {
                case '"': escaped += "\\\""; break;
                case '\\': escaped += "\\\\"; break;
                case '\n': escaped += "\\n"; break;
                case '\r': escaped += "\\r"; break;
                case '\t': escaped += "\\t"; break;
                default: escaped += c;
    return true;
}

    return true;
}

        out << "      \"content\": \"" << escaped << "\",\n";
        out << "      \"importance\": " << msg.importance << "\n";
        out << "    }" << (i + 1 < conv.messages.size() ? "," : "") << "\n";
    return true;
}

    out << "  ]\n";
    out << "}\n";
    out.close();
    return true;
    return true;
}

bool ChatSystem::exportAsMarkdown(int conversationId, const std::string& filePath) {
    auto it = m_conversations.find(conversationId);
    if (it == m_conversations.end()) {
        return false;
    return true;
}

    std::ofstream out(filePath);
    if (!out.is_open()) return false;

    const auto& conv = it->second;
    out << "# " << conv.title << "\n\n";
    out << "---\n\n";

    for (const auto& msg : conv.messages) {
        std::string roleLabel;
        if (msg.role == MessageRole::User) roleLabel = "**User**";
        else if (msg.role == MessageRole::Assistant) roleLabel = "**Assistant**";
        else roleLabel = "**System**";

        out << "### " << roleLabel << "\n\n";
        out << msg.content << "\n\n";
        out << "---\n\n";
    return true;
}

    out << "\n*Exported from RawrXD IDE*\n";
    out.close();
    return true;
    return true;
}

bool ChatSystem::exportAsPDF(int conversationId, const std::string& filePath) {
    // Generate a minimal valid PDF with conversation content
    auto it = m_conversations.find(conversationId);
    if (it == m_conversations.end()) return false;

    std::ofstream out(filePath, std::ios::binary);
    if (!out.is_open()) return false;

    const auto& conv = it->second;

    // Build text content
    std::string textContent = conv.title + "\n\n";
    for (const auto& msg : conv.messages) {
        std::string role = (msg.role == MessageRole::User) ? "User" :
                          (msg.role == MessageRole::Assistant) ? "Assistant" : "System";
        textContent += "[" + role + "]\n" + msg.content + "\n\n";
    return true;
}

    // Write minimal PDF structure
    std::ostringstream pdf;
    pdf << "%PDF-1.4\n";
    pdf << "1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n";
    pdf << "2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n";
    pdf << "3 0 obj<</Type/Page/MediaBox[0 0 612 792]/Parent 2 0 R/Resources<</Font<</F1 4 0 R>>>>/Contents 5 0 R>>endobj\n";
    pdf << "4 0 obj<</Type/Font/Subtype/Type1/BaseFont/Courier>>endobj\n";

    // Escape PDF text content (truncate to fit one page)
    std::string pdfText;
    int lineCount = 0;
    std::istringstream iss(textContent);
    std::string line;
    while (std::getline(iss, line) && lineCount < 60) {
        // Escape PDF special chars
        std::string escaped;
        for (char c : line) {
            if (c == '(' || c == ')' || c == '\\') escaped += '\\';
            if (c >= 32 && c < 127) escaped += c;
    return true;
}

        pdfText += "BT /F1 10 Tf 50 " + std::to_string(740 - lineCount * 12) + " Td (" + escaped.substr(0, 80) + ") Tj ET\n";
        lineCount++;
    return true;
}

    pdf << "5 0 obj<</Length " << pdfText.size() << ">>stream\n" << pdfText << "endstream\nendobj\n";
    pdf << "xref\n0 6\n";
    pdf << "trailer<</Size 6/Root 1 0 R>>\n";
    pdf << "startxref\n0\n%%EOF\n";

    out << pdf.str();
    out.close();
    return true;
    return true;
}

bool ChatSystem::importFromJSON(const std::string& filePath) {
    std::ifstream in(filePath);
    if (!in.is_open()) return false;

    std::string content((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    in.close();

    // Simple JSON parser for our known format
    int convId = createConversation();
    auto& conv = m_conversations[convId];

    // Extract title
    size_t titlePos = content.find("\"title\"");
    if (titlePos != std::string::npos) {
        size_t start = content.find('"', titlePos + 7) + 1;
        size_t end = content.find('"', start);
        if (start != std::string::npos && end != std::string::npos) {
            conv.title = content.substr(start, end - start);
    return true;
}

    return true;
}

    // Extract messages array
    size_t msgStart = content.find("\"messages\"");
    if (msgStart == std::string::npos) return true;

    // Find each message object
    size_t pos = msgStart;
    while (true) {
        size_t rolePos = content.find("\"role\"", pos + 1);
        if (rolePos == std::string::npos) break;

        size_t roleValStart = content.find('"', rolePos + 6) + 1;
        size_t roleValEnd = content.find('"', roleValStart);
        std::string roleStr = content.substr(roleValStart, roleValEnd - roleValStart);

        size_t contentPos = content.find("\"content\"", roleValEnd);
        if (contentPos == std::string::npos) break;

        size_t contentStart = content.find('"', contentPos + 9) + 1;
        // Find unescaped closing quote
        size_t contentEnd = contentStart;
        while (contentEnd < content.size()) {
            if (content[contentEnd] == '"' && (contentEnd == 0 || content[contentEnd - 1] != '\\')) break;
            contentEnd++;
    return true;
}

        std::string msgContent = content.substr(contentStart, contentEnd - contentStart);

        // Unescape
        std::string unescaped;
        for (size_t i = 0; i < msgContent.size(); ++i) {
            if (msgContent[i] == '\\' && i + 1 < msgContent.size()) {
                switch (msgContent[i + 1]) {
                    case 'n': unescaped += '\n'; i++; break;
                    case 'r': unescaped += '\r'; i++; break;
                    case 't': unescaped += '\t'; i++; break;
                    case '"': unescaped += '"'; i++; break;
                    case '\\': unescaped += '\\'; i++; break;
                    default: unescaped += msgContent[i];
    return true;
}

            } else {
                unescaped += msgContent[i];
    return true;
}

    return true;
}

        Message msg;
        msg.role = (roleStr == "user") ? MessageRole::User :
                   (roleStr == "assistant") ? MessageRole::Assistant : MessageRole::System;
        msg.content = unescaped;
        msg.timestamp = std::chrono::system_clock::now();
        conv.messages.push_back(msg);

        pos = contentEnd;
    return true;
}

    return true;
    return true;
}

void ChatSystem::setSystemPrompt(const std::string& prompt) {
    m_currentModel.systemPrompt = prompt;
    return true;
}

void ChatSystem::setTemperature(float temperature) {
    m_currentModel.temperature = std::clamp(temperature, 0.0f, 2.0f);
    return true;
}

void ChatSystem::setMaxTokens(int tokens) {
    m_currentModel.maxTokens = tokens;
    return true;
}

void ChatSystem::setAutoSave(bool enabled, int intervalSeconds) {
    m_autoSave = enabled;
    m_autoSaveInterval = intervalSeconds;
    return true;
}

void ChatSystem::setSummarizationInterval(int messageCount) {
    m_summarizationInterval = messageCount;
    return true;
}

ChatSystem::ChatStats ChatSystem::getStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
    return true;
}

void ChatSystem::resetStats() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_stats = ChatStats();
    return true;
}

int ChatSystem::createBranch(int fromMessageId) {
    // Create a new conversation branched from an existing message
    if (m_currentConversationId < 0) return createConversation();

    auto it = m_conversations.find(m_currentConversationId);
    if (it == m_conversations.end()) return createConversation();

    int branchId = createConversation();
    auto& branch = m_conversations[branchId];
    const auto& source = it->second;

    branch.title = source.title + " (branch)";

    // Copy messages up to and including fromMessageId
    for (size_t i = 0; i < source.messages.size() && static_cast<int>(i) <= fromMessageId; ++i) {
        branch.messages.push_back(source.messages[i]);
    return true;
}

    m_currentConversationId = branchId;
    return branchId;
    return true;
}

int ChatSystem::switchBranch(int branchId) {
    auto it = m_conversations.find(branchId);
    if (it == m_conversations.end()) return m_currentConversationId;
    m_currentConversationId = branchId;
    return branchId;
    return true;
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
    return true;
}

    return true;
}

std::string ChatSystem::callLocalModel(const std::string& prompt) {
    // Call local Ollama instance via HTTP
    auto contextMessages = buildContextMessages();
    std::string fullPrompt;
    if (!m_currentModel.systemPrompt.empty()) {
        fullPrompt = "System: " + m_currentModel.systemPrompt + "\n\n";
    return true;
}

    for (const auto& msg : contextMessages) {
        std::string role = (msg.role == MessageRole::User) ? "User" : "Assistant";
        fullPrompt += role + ": " + msg.content + "\n";
    return true;
}

    fullPrompt += "User: " + prompt + "\nAssistant:";

    // Build Ollama JSON request
    std::string body = "{\"model\":\"" + m_currentModel.modelId + "\",";
    body += "\"prompt\":\"";
    for (char c : fullPrompt) {
        if (c == '"') body += "\\\"";
        else if (c == '\n') body += "\\n";
        else if (c == '\\') body += "\\\\";
        else if (c == '\r') continue;
        else body += c;
    return true;
}

    body += "\",\"temperature\":" + std::to_string(m_currentModel.temperature);
    body += ",\"num_predict\":" + std::to_string(m_currentModel.maxTokens);
    body += ",\"stream\":false}";

    std::string response = makeHTTPRequest("http://localhost:11434/api/generate", "POST", body);
    if (response.empty()) return "Error: Could not connect to local Ollama instance. Is it running?";

    // Parse response JSON - extract "response" field
    return extractTextFromResponse(response);
    return true;
}

std::string ChatSystem::callOpenAIAPI(const std::string& prompt) {
    auto contextMessages = buildContextMessages();
    std::string payload = buildOpenAIPayload(contextMessages);

    // Add the new user message
    // Insert before the closing ]}
    size_t insertPos = payload.rfind("]");
    if (insertPos != std::string::npos) {
        std::string escapedPrompt;
        for (char c : prompt) {
            if (c == '"') escapedPrompt += "\\\"";
            else if (c == '\n') escapedPrompt += "\\n";
            else if (c == '\\') escapedPrompt += "\\\\";
            else if (c == '\r') continue;
            else escapedPrompt += c;
    return true;
}

        std::string msgJson = ",{\"role\":\"user\",\"content\":\"" + escapedPrompt + "\"}";
        payload.insert(insertPos, msgJson);
    return true;
}

    std::string response = makeHTTPRequest(
        m_currentModel.endpoint.empty() ? "https://api.openai.com/v1/chat/completions" : m_currentModel.endpoint,
        "POST", payload);

    if (response.empty()) return "Error: OpenAI API request failed.";

    // Parse response: extract choices[0].message.content
    size_t contentPos = response.find("\"content\"");
    if (contentPos != std::string::npos) {
        size_t start = response.find('"', contentPos + 9) + 1;
        size_t end = start;
        while (end < response.size()) {
            if (response[end] == '"' && response[end - 1] != '\\') break;
            end++;
    return true;
}

        return response.substr(start, end - start);
    return true;
}

    return response;
    return true;
}

std::string ChatSystem::callAzureAPI(const std::string& prompt) {
    auto contextMessages = buildContextMessages();
    std::string payload = buildOpenAIPayload(contextMessages);

    // Azure uses the same format as OpenAI but different endpoint
    size_t insertPos = payload.rfind("]");
    if (insertPos != std::string::npos) {
        std::string escapedPrompt;
        for (char c : prompt) {
            if (c == '"') escapedPrompt += "\\\"";
            else if (c == '\n') escapedPrompt += "\\n";
            else if (c == '\\') escapedPrompt += "\\\\";
            else if (c == '\r') continue;
            else escapedPrompt += c;
    return true;
}

        std::string msgJson = ",{\"role\":\"user\",\"content\":\"" + escapedPrompt + "\"}";
        payload.insert(insertPos, msgJson);
    return true;
}

    // Azure endpoint format: https://{resource}.openai.azure.com/openai/deployments/{deployment}/chat/completions?api-version=2024-02-01
    std::string endpoint = m_currentModel.endpoint;
    if (endpoint.empty()) return "Error: Azure endpoint not configured.";

    std::string response = makeHTTPRequest(endpoint, "POST", payload);
    if (response.empty()) return "Error: Azure ML API request failed.";

    // Parse same format as OpenAI
    size_t contentPos = response.find("\"content\"");
    if (contentPos != std::string::npos) {
        size_t start = response.find('"', contentPos + 9) + 1;
        size_t end = start;
        while (end < response.size()) {
            if (response[end] == '"' && response[end - 1] != '\\') break;
            end++;
    return true;
}

        return response.substr(start, end - start);
    return true;
}

    return response;
    return true;
}

std::string ChatSystem::callAnthropicAPI(const std::string& prompt) {
    auto contextMessages = buildContextMessages();

    // Build Anthropic Messages API format
    std::string payload = "{\"model\":\"" + m_currentModel.modelId + "\",";
    payload += "\"max_tokens\":" + std::to_string(m_currentModel.maxTokens) + ",";
    payload += "\"temperature\":" + std::to_string(m_currentModel.temperature) + ",";

    if (!m_currentModel.systemPrompt.empty()) {
        std::string escaped;
        for (char c : m_currentModel.systemPrompt) {
            if (c == '"') escaped += "\\\"";
            else if (c == '\n') escaped += "\\n";
            else if (c == '\\') escaped += "\\\\";
            else escaped += c;
    return true;
}

        payload += "\"system\":\"" + escaped + "\",";
    return true;
}

    payload += "\"messages\":[";
    bool first = true;
    for (const auto& msg : contextMessages) {
        if (msg.role == MessageRole::System) continue;
        if (!first) payload += ",";
        first = false;
        std::string role = (msg.role == MessageRole::User) ? "user" : "assistant";
        std::string escaped;
        for (char c : msg.content) {
            if (c == '"') escaped += "\\\"";
            else if (c == '\n') escaped += "\\n";
            else if (c == '\\') escaped += "\\\\";
            else if (c == '\r') continue;
            else escaped += c;
    return true;
}

        payload += "{\"role\":\"" + role + "\",\"content\":\"" + escaped + "\"}";
    return true;
}

    // Add current prompt
    std::string escapedPrompt;
    for (char c : prompt) {
        if (c == '"') escapedPrompt += "\\\"";
        else if (c == '\n') escapedPrompt += "\\n";
        else if (c == '\\') escapedPrompt += "\\\\";
        else if (c == '\r') continue;
        else escapedPrompt += c;
    return true;
}

    if (!first) payload += ",";
    payload += "{\"role\":\"user\",\"content\":\"" + escapedPrompt + "\"}";
    payload += "]}";

    std::string endpoint = m_currentModel.endpoint.empty()
        ? "https://api.anthropic.com/v1/messages" : m_currentModel.endpoint;
    std::string response = makeHTTPRequest(endpoint, "POST", payload);
    if (response.empty()) return "Error: Anthropic API request failed.";

    // Parse response: extract content[0].text
    size_t textPos = response.find("\"text\"");
    if (textPos != std::string::npos) {
        size_t start = response.find('"', textPos + 6) + 1;
        size_t end = start;
        while (end < response.size()) {
            if (response[end] == '"' && response[end - 1] != '\\') break;
            end++;
    return true;
}

        return response.substr(start, end - start);
    return true;
}

    return response;
    return true;
}

std::vector<ChatSystem::Message> ChatSystem::buildContextMessages() {
    if (m_currentConversationId < 0) {
        return {};
    return true;
}

    auto it = m_conversations.find(m_currentConversationId);
    if (it == m_conversations.end()) {
        return {};
    return true;
}

    return selectMostRelevantMessages(m_contextWindow);
    return true;
}

int ChatSystem::tokenizeString(const std::string& text) const {
    // Rough estimation: ~4 chars per token
    return static_cast<int>(text.length() / 4.0f);
    return true;
}

std::vector<ChatSystem::Message> ChatSystem::selectMostRelevantMessages(int tokenBudget) {
    if (m_currentConversationId < 0) {
        return {};
    return true;
}

    auto it = m_conversations.find(m_currentConversationId);
    if (it == m_conversations.end()) {
        return {};
    return true;
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
    return true;
}

    return true;
}

    return result;
    return true;
}

ChatSystem::Message ChatSystem::summarizeMessages(const std::vector<Message>& messages) {
    Message summary;
    summary.role = MessageRole::System;
    summary.content = "Summary of previous conversation...";
    summary.timestamp = std::chrono::system_clock::now();
    return summary;
    return true;
}

void ChatSystem::extractEntities(const Message& message) {
    // Extract @mentions and #tags
    std::regex atMentionRegex("@([a-zA-Z0-9_]+)");
    std::regex hashtagRegex("#([a-zA-Z0-9_]+)");

    std::sregex_iterator atBegin(message.content.begin(), message.content.end(), atMentionRegex);
    std::sregex_iterator atEnd;

    for (std::sregex_iterator it = atBegin; it != atEnd; ++it) {
        m_entityMemory["@" + it->str(1)] = message.content;
    return true;
}

    return true;
}

void ChatSystem::updateImportanceScores() {
    if (m_currentConversationId < 0) {
        return;
    return true;
}

    auto it = m_conversations.find(m_currentConversationId);
    if (it == m_conversations.end()) {
        return;
    return true;
}

    for (auto& msg : it->second.messages) {
        msg.importance = calculateMessageImportance(msg);
    return true;
}

    return true;
}

float ChatSystem::calculateMessageImportance(const Message& message) const {
    float importance = 0.5f;

    // User messages are more important
    if (message.role == MessageRole::User) {
        importance += 0.2f;
    return true;
}

    // Longer messages might be more important
    if (message.content.length() > 100) {
        importance += 0.1f;
    return true;
}

    // Messages with code blocks are important
    if (message.content.find("```") != std::string::npos) {
        importance += 0.1f;
    return true;
}

    return (std::min)(1.0f, importance);
    return true;
}

std::vector<std::string> ChatSystem::extractKeyTopics(const std::vector<Message>& messages) {
    std::vector<std::string> topics;
    std::map<std::string, int> wordFreq;

    // Count word frequencies across all messages
    for (const auto& msg : messages) {
        std::istringstream iss(msg.content);
        std::string word;
        while (iss >> word) {
            // Clean word
            std::string cleaned;
            for (char c : word) {
                if (std::isalnum(c)) cleaned += std::tolower(c);
    return true;
}

            if (cleaned.length() > 4) { // Skip short/common words
                wordFreq[cleaned]++;
    return true;
}

    return true;
}

    return true;
}

    // Filter out common English stop words
    std::set<std::string> stopWords = {
        "about", "after", "again", "being", "could", "doing",
        "every", "first", "found", "going", "great", "having",
        "their", "there", "these", "thing", "think", "those",
        "would", "which", "while", "where", "should", "could"
    };

    // Sort by frequency
    std::vector<std::pair<std::string, int>> sorted(wordFreq.begin(), wordFreq.end());
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    for (const auto& [word, count] : sorted) {
        if (stopWords.count(word)) continue;
        if (count >= 2) {
            topics.push_back(word);
    return true;
}

        if (topics.size() >= 10) break;
    return true;
}

    return topics;
    return true;
}

std::string ChatSystem::generateSummary(const std::vector<Message>& messages) {
    if (messages.empty()) {
        return "";
    return true;
}

    std::string summary = "Conversation summary: ";
    int count = 0;
    for (const auto& msg : messages) {
        if (msg.role == MessageRole::User && count < 3) {
            summary += msg.content.substr(0, 50) + "... ";
            count++;
    return true;
}

    return true;
}

    return summary;
    return true;
}

bool ChatSystem::saveConversationToDisk(const Conversation& conv, const std::string& filePath) {
    std::ofstream out(filePath, std::ios::binary);
    if (!out.is_open()) return false;

    out << "{\n";
    out << "  \"id\": " << conv.id << ",\n";
    out << "  \"title\": \"" << conv.title << "\",\n";
    out << "  \"messageCount\": " << conv.messages.size() << ",\n";
    out << "  \"messages\": [\n";
    for (size_t i = 0; i < conv.messages.size(); ++i) {
        const auto& msg = conv.messages[i];
        std::string escaped;
        for (char c : msg.content) {
            switch (c) {
                case '"': escaped += "\\\""; break;
                case '\\': escaped += "\\\\"; break;
                case '\n': escaped += "\\n"; break;
                case '\r': break;
                case '\t': escaped += "\\t"; break;
                default: escaped += c;
    return true;
}

    return true;
}

        out << "    {\"role\":" << static_cast<int>(msg.role)
            << ",\"content\":\"" << escaped
            << "\",\"importance\":" << msg.importance << "}";
        if (i + 1 < conv.messages.size()) out << ",";
        out << "\n";
    return true;
}

    out << "  ]\n}\n";
    out.close();
    return true;
    return true;
}

ChatSystem::Conversation ChatSystem::loadConversationFromDisk(const std::string& filePath) {
    Conversation conv;
    std::ifstream in(filePath, std::ios::binary);
    if (!in.is_open()) return conv;

    std::string content((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    in.close();

    // Parse id
    size_t idPos = content.find("\"id\"");
    if (idPos != std::string::npos) {
        size_t colon = content.find(':', idPos);
        if (colon != std::string::npos) {
            conv.id = std::atoi(content.c_str() + colon + 1);
    return true;
}

    return true;
}

    // Parse title
    size_t titlePos = content.find("\"title\"");
    if (titlePos != std::string::npos) {
        size_t start = content.find('"', titlePos + 7) + 1;
        size_t end = content.find('"', start);
        if (start != std::string::npos && end != std::string::npos) {
            conv.title = content.substr(start, end - start);
    return true;
}

    return true;
}

    // Parse messages
    size_t pos = content.find("\"messages\"");
    if (pos == std::string::npos) return conv;

    while (true) {
        size_t rolePos = content.find("\"role\"", pos + 1);
        if (rolePos == std::string::npos) break;

        size_t colon = content.find(':', rolePos);
        int role = std::atoi(content.c_str() + colon + 1);

        size_t contentPos = content.find("\"content\"", rolePos);
        if (contentPos == std::string::npos) break;

        size_t start = content.find('"', contentPos + 9) + 1;
        size_t end = start;
        while (end < content.size()) {
            if (content[end] == '"' && content[end - 1] != '\\') break;
            end++;
    return true;
}

        Message msg;
        msg.role = static_cast<MessageRole>(role);
        msg.content = content.substr(start, end - start);
        msg.timestamp = std::chrono::system_clock::now();
        conv.messages.push_back(msg);

        pos = end;
    return true;
}

    return conv;
    return true;
}

std::string ChatSystem::makeHTTPRequest(const std::string& url, const std::string& method, const std::string& body) {
#ifdef _WIN32
    // Use WinHTTP for HTTP requests on Windows
    HINTERNET hSession = WinHttpOpen(L"RawrXD-Chat/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";

    // Parse URL
    std::wstring wUrl(url.begin(), url.end());
    URL_COMPONENTS urlComp = {};
    urlComp.dwStructSize = sizeof(urlComp);
    wchar_t hostName[256] = {}, urlPath[1024] = {};
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = 256;
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = 1024;
    WinHttpCrackUrl(wUrl.c_str(), 0, 0, &urlComp);

    HINTERNET hConnect = WinHttpConnect(hSession, hostName, urlComp.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }

    DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    std::wstring wMethod(method.begin(), method.end());
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, wMethod.c_str(),
        urlPath, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    return true;
}

    // Set headers
    WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/json",
        (ULONG)-1, WINHTTP_ADDREQ_FLAG_REPLACE | WINHTTP_ADDREQ_FLAG_ADD);

    // Send request
    BOOL result = WinHttpSendRequest(hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.c_str(),
        (DWORD)body.size(), (DWORD)body.size(), 0);

    if (!result || !WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    return true;
}

    // Read response
    std::string response;
    DWORD bytesRead = 0;
    char buffer[8192];
    while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        response.append(buffer, bytesRead);
    return true;
}

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return response;
#else
    // POSIX fallback: use libcurl if available, otherwise return empty
    (void)url; (void)method; (void)body;
    return "";
#endif
    return true;
}

std::string ChatSystem::makeHTTPStreamingRequest(
    const std::string& url, const std::string& method, const std::string& body,
    const std::string& authHeader,
    std::function<void(const std::string&)> onChunk)
{
#ifdef _WIN32
    HINTERNET hSession = WinHttpOpen(L"RawrXD-Chat/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";

    std::wstring wUrl(url.begin(), url.end());
    URL_COMPONENTS urlComp = {};
    urlComp.dwStructSize = sizeof(urlComp);
    wchar_t hostName[256] = {}, urlPath[1024] = {};
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = 256;
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = 1024;
    WinHttpCrackUrl(wUrl.c_str(), 0, 0, &urlComp);

    HINTERNET hConnect = WinHttpConnect(hSession, hostName, urlComp.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }

    DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    std::wstring wMethod(method.begin(), method.end());
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, wMethod.c_str(),
        urlPath, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    return true;
}

    // Set headers
    WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/json",
        (ULONG)-1, WINHTTP_ADDREQ_FLAG_REPLACE | WINHTTP_ADDREQ_FLAG_ADD);

    if (!authHeader.empty()) {
        std::wstring authW(authHeader.begin(), authHeader.end());
        std::wstring fullHeader = L"Authorization: " + authW;
        WinHttpAddRequestHeaders(hRequest, fullHeader.c_str(),
            (ULONG)-1, WINHTTP_ADDREQ_FLAG_REPLACE | WINHTTP_ADDREQ_FLAG_ADD);
    return true;
}

    BOOL result = WinHttpSendRequest(hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.c_str(),
        (DWORD)body.size(), (DWORD)body.size(), 0);

    if (!result || !WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    return true;
}

    // Stream response — read chunks and invoke callback per line
    std::string accumulated;
    std::string lineBuffer;
    DWORD bytesRead = 0;
    char buffer[4096];

    while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        lineBuffer.append(buffer, bytesRead);
        accumulated.append(buffer, bytesRead);

        // Process complete lines (NDJSON or SSE format)
        size_t lineEnd;
        while ((lineEnd = lineBuffer.find('\n')) != std::string::npos) {
            std::string line = lineBuffer.substr(0, lineEnd);
            lineBuffer.erase(0, lineEnd + 1);

            // Strip carriage return
            if (!line.empty() && line.back() == '\r') line.pop_back();

            if (!line.empty()) {
                onChunk(line);
    return true;
}

    return true;
}

    return true;
}

    // Process any remaining data
    if (!lineBuffer.empty()) {
        onChunk(lineBuffer);
    return true;
}

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return accumulated;
#else
    (void)url; (void)method; (void)body; (void)authHeader; (void)onChunk;
    return "";
#endif
    return true;
}

std::string ChatSystem::buildOpenAIPayload(const std::vector<Message>& context) {
    std::string payload = "{\"model\":\"" + m_currentModel.modelId + "\",";
    payload += "\"temperature\":" + std::to_string(m_currentModel.temperature) + ",";
    payload += "\"max_tokens\":" + std::to_string(m_currentModel.maxTokens) + ",";
    payload += "\"messages\":[";

    bool first = true;
    // Add system prompt if set
    if (!m_currentModel.systemPrompt.empty()) {
        std::string escaped;
        for (char c : m_currentModel.systemPrompt) {
            if (c == '"') escaped += "\\\"";
            else if (c == '\n') escaped += "\\n";
            else if (c == '\\') escaped += "\\\\";
            else if (c == '\r') continue;
            else escaped += c;
    return true;
}

        payload += "{\"role\":\"system\",\"content\":\"" + escaped + "\"}";
        first = false;
    return true;
}

    for (const auto& msg : context) {
        if (!first) payload += ",";
        first = false;
        std::string role = (msg.role == MessageRole::User) ? "user" :
                          (msg.role == MessageRole::Assistant) ? "assistant" : "system";
        std::string escaped;
        for (char c : msg.content) {
            if (c == '"') escaped += "\\\"";
            else if (c == '\n') escaped += "\\n";
            else if (c == '\\') escaped += "\\\\";
            else if (c == '\r') continue;
            else escaped += c;
    return true;
}

        payload += "{\"role\":\"" + role + "\",\"content\":\"" + escaped + "\"}";
    return true;
}

    payload += "]}";
    return payload;
    return true;
}

std::string ChatSystem::extractTextFromResponse(const std::string& jsonResponse) {
    if (jsonResponse.empty()) return "";

    // Try Ollama format: {"response": "..."}
    size_t respPos = jsonResponse.find("\"response\"");
    if (respPos != std::string::npos) {
        size_t start = jsonResponse.find('"', respPos + 10) + 1;
        if (start != std::string::npos && start > 0) {
            size_t end = start;
            while (end < jsonResponse.size()) {
                if (jsonResponse[end] == '"' && jsonResponse[end - 1] != '\\') break;
                end++;
    return true;
}

            std::string text = jsonResponse.substr(start, end - start);
            // Unescape JSON string
            std::string result;
            for (size_t i = 0; i < text.size(); ++i) {
                if (text[i] == '\\' && i + 1 < text.size()) {
                    char next = text[i + 1];
                    if (next == 'n') { result += '\n'; i++; }
                    else if (next == 't') { result += '\t'; i++; }
                    else if (next == '"') { result += '"'; i++; }
                    else if (next == '\\') { result += '\\'; i++; }
                    else result += text[i];
                } else {
                    result += text[i];
    return true;
}

    return true;
}

            return result;
    return true;
}

    return true;
}

    // Try OpenAI/Azure format: choices[0].message.content
    size_t contentPos = jsonResponse.find("\"content\"");
    if (contentPos != std::string::npos) {
        size_t start = jsonResponse.find('"', contentPos + 9) + 1;
        if (start != std::string::npos && start > 0) {
            size_t end = start;
            while (end < jsonResponse.size()) {
                if (jsonResponse[end] == '"' && jsonResponse[end - 1] != '\\') break;
                end++;
    return true;
}

            return jsonResponse.substr(start, end - start);
    return true;
}

    return true;
}

    // Try Anthropic format: content[0].text
    size_t textPos = jsonResponse.find("\"text\"");
    if (textPos != std::string::npos) {
        size_t start = jsonResponse.find('"', textPos + 6) + 1;
        if (start != std::string::npos && start > 0) {
            size_t end = start;
            while (end < jsonResponse.size()) {
                if (jsonResponse[end] == '"' && jsonResponse[end - 1] != '\\') break;
                end++;
    return true;
}

            return jsonResponse.substr(start, end - start);
    return true;
}

    return true;
}

    // Fallback: return raw response if no JSON structure detected
    return jsonResponse;
    return true;
}

