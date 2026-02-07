#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <queue>
#include <functional>
#include <chrono>
#include <mutex>
#include <atomic>

/**
 * @class ChatSystem
 * @brief Production-grade multi-turn conversation system with memory and context management
 * 
 * Features:
 * - Multi-turn conversation threading
 * - Context window management with token limits
 * - Conversation memory with importance scoring
 * - Model switching and fallback
 * - Export capabilities (JSON, Markdown, PDF)
 * - Streaming response support
 */
class ChatSystem {
public:
    enum class MessageRole {
        User,
        Assistant,
        System,
        Tool
    };

    enum class ModelSource {
        Local,
        OpenAI,
        Azure,
        Anthropic,
        Custom
    };

    struct Message {
        int id;
        MessageRole role;
        std::string content;
        std::chrono::system_clock::time_point timestamp;
        float importance;  // 0.0-1.0
        std::vector<std::string> mentions;  // @mentions, #tags
        std::map<std::string, std::string> metadata;
    };

    struct Conversation {
        int id;
        std::string title;
        std::string description;
        std::vector<Message> messages;
        std::chrono::system_clock::time_point created;
        std::chrono::system_clock::time_point lastModified;
        std::string modelUsed;
        float averageConfidence;
        int totalTokens;
        bool archived;
    };

    struct ModelConfig {
        ModelSource source;
        std::string modelName;
        std::string apiKey;
        std::string endpoint;
        int contextWindow;
        float temperature;
        float topP;
        int maxTokens;
        std::string systemPrompt;
    };

    struct ConversationMemory {
        std::string summary;
        std::vector<std::string> keyTopics;
        std::vector<std::pair<int, float>> importantMessages;  // (message_id, importance)
        std::map<std::string, std::string> entityMemory;  // Extracted entities and their context
        int totalTurns;
        int totalTokensUsed;
    };

    explicit ChatSystem();
    ~ChatSystem();

    // Initialization
    bool initialize(const ModelConfig& defaultModel);
    bool addModel(const ModelConfig& config);
    bool switchModel(const std::string& modelName);
    ModelConfig getCurrentModel() const;
    std::vector<std::string> listAvailableModels() const;

    // Conversation management
    int createConversation(const std::string& title = "New Conversation");
    bool loadConversation(int conversationId);
    bool saveConversation(int conversationId, const std::string& filePath = "");
    int getCurrentConversationId() const { return m_currentConversationId; }
    Conversation getCurrentConversation() const;
    std::vector<Conversation> listConversations(int offset = 0, int limit = 20);
    bool deleteConversation(int conversationId);
    bool archiveConversation(int conversationId);

    // Message operations
    int sendMessage(const std::string& content, MessageRole role = MessageRole::User);
    std::string generateResponse(const std::string& userMessage, bool streaming = false);
    
    // Streaming API
    void startStreamingResponse(
        const std::string& userMessage,
        std::function<void(const std::string&)> onChunk,
        std::function<void()> onComplete,
        std::function<void(const std::string&)> onError
    );
    void cancelStreaming();
    bool isStreaming() const { return m_streaming.load(); }

    // Message retrieval and editing
    Message getMessage(int messageId) const;
    std::vector<Message> getMessages(int offset = 0, int limit = 50) const;
    bool editMessage(int messageId, const std::string& newContent);
    bool deleteMessage(int messageId);
    void clearHistory();

    // Context and memory management
    std::string getContextSummary() const;
    ConversationMemory getMemory() const;
    void updateMemory();
    void setContextWindow(int tokens);
    int getContextWindow() const { return m_contextWindow; }
    int getCurrentTokenCount() const;
    void trimContextIfNeeded();
    std::vector<Message> getRelevantContext(const std::string& query, int maxMessages = 5);

    // Search and retrieval
    std::vector<Message> searchMessages(const std::string& query);
    std::vector<Conversation> searchConversations(const std::string& query);
    std::vector<int> findMessagesWithMentions(const std::string& mention);

    // Export functionality
    bool exportAsJSON(int conversationId, const std::string& filePath);
    bool exportAsMarkdown(int conversationId, const std::string& filePath);
    bool exportAsPDF(int conversationId, const std::string& filePath);  // Requires PDF library
    bool importFromJSON(const std::string& filePath);

    // Configuration
    void setSystemPrompt(const std::string& prompt);
    void setTemperature(float temperature);
    void setMaxTokens(int tokens);
    void setAutoSave(bool enabled, int intervalSeconds = 60);
    void setSummarizationInterval(int messageCount = 20);

    // Statistics
    struct ChatStats {
        int totalConversations = 0;
        int totalMessages = 0;
        int totalTokensUsed = 0;
        float averageResponseTime = 0.0f;
        float averageConfidence = 0.0f;
        std::map<ModelSource, int> messagesPerModel;
    };
    ChatStats getStats() const;
    void resetStats();

    // Conversation branching (experimental)
    int createBranch(int fromMessageId);
    int switchBranch(int branchId);

private:
    // Core operations
    std::string callModel(const std::string& prompt);
    std::string callLocalModel(const std::string& prompt);
    std::string callOpenAIAPI(const std::string& prompt);
    std::string callAzureAPI(const std::string& prompt);
    std::string callAnthropicAPI(const std::string& prompt);

    // Context and token management
    std::vector<Message> buildContextMessages();
    int tokenizeString(const std::string& text) const;
    std::vector<Message> selectMostRelevantMessages(int tokenBudget);
    Message summarizeMessages(const std::vector<Message>& messages);

    // Memory operations
    void extractEntities(const Message& message);
    void updateImportanceScores();
    float calculateMessageImportance(const Message& message) const;
    std::vector<std::string> extractKeyTopics(const std::vector<Message>& messages);
    std::string generateSummary(const std::vector<Message>& messages);

    // File I/O
    bool saveConversationToDisk(const Conversation& conv, const std::string& filePath);
    Conversation loadConversationFromDisk(const std::string& filePath);

    // API helpers
    std::string makeHTTPRequest(const std::string& url, const std::string& method, const std::string& body);
    std::string buildOpenAIPayload(const std::vector<Message>& context);
    std::string extractTextFromResponse(const std::string& jsonResponse);

    // State
    int m_currentConversationId = -1;
    std::map<int, Conversation> m_conversations;
    int m_nextConversationId = 1;
    int m_nextMessageId = 1;

    // Models
    std::map<std::string, ModelConfig> m_models;
    ModelConfig m_currentModel;
    ModelConfig m_fallbackModel;

    // Settings
    int m_contextWindow = 4096;
    bool m_autoSave = true;
    int m_autoSaveInterval = 60;  // seconds
    int m_summarizationInterval = 20;  // messages
    float m_importance_threshold = 0.3f;

    // Memory
    ConversationMemory m_memory;
    std::map<std::string, std::string> m_entityMemory;

    // Streaming
    std::atomic<bool> m_streaming{false};
    std::thread m_streamingThread;
    std::mutex m_streamingMutex;

    // Statistics
    mutable std::mutex m_statsMutex;
    ChatStats m_stats;

    // Auto-save thread
    std::thread m_autoSaveThread;
    std::atomic<bool> m_running{true};
};
