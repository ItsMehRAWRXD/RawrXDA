#pragma once
/**
 * @file ai_completion_engine.h
 * @brief AI-powered code completion
 * Batch 5 - Item 70: AI completion engine
 */

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <future>

namespace RawrXD::AI {

struct CompletionContext {
    std::string filePath;
    std::string language;
    std::string prefix;
    std::string suffix;
    int line;
    int column;
    std::vector<std::string> recentFiles;
    std::string projectContext;
};

struct CompletionItem {
    std::string text;
    std::string displayText;
    std::string description;
    float confidence;
    std::string source;
    int insertStart;
    int insertEnd;
};

struct CompletionResult {
    std::vector<CompletionItem> items;
    bool isComplete;
    std::string error;
    std::chrono::milliseconds latency;
};

enum class CompletionTrigger {
    Manual,
    Automatic,
    Character,
    Delay
};

class AICompletionEngine {
public:
    AICompletionEngine();
    ~AICompletionEngine();

    // Initialization
    bool initialize();
    void shutdown();
    bool isInitialized() const;

    // Configuration
    void setModel(const std::string& modelPath);
    void setMaxTokens(int maxTokens);
    void setTemperature(float temperature);
    void setContextWindow(int lines);

    // Completion
    CompletionResult getCompletions(const CompletionContext& context);
    std::future<CompletionResult> getCompletionsAsync(const CompletionContext& context);
    std::optional<CompletionItem> getBestCompletion(const CompletionContext& context);

    // Streaming
    void requestStreamingCompletion(const CompletionContext& context);
    void cancelStreaming();
    bool isStreaming() const;

    // Context management
    void updateContext(const std::string& filePath, const std::string& content);
    void clearContext();
    void setProjectContext(const std::string& context);

    // Trigger settings
    void setAutoTrigger(bool enabled);
    bool isAutoTriggerEnabled() const;
    void setTriggerDelay(int milliseconds);
    void setTriggerCharacters(const std::vector<char>& chars);

    // Cache
    void clearCache();
    void setCacheSize(int size);

    // Events
    using CompletionCallback = std::function<void(const CompletionResult&)>;
    using StreamCallback = std::function<void(const std::string& token)>;
    void onCompletionReady(CompletionCallback callback);
    void onStreamToken(StreamCallback callback);

private:
    std::string m_modelPath;
    int m_maxTokens{100};
    float m_temperature{0.2f};
    int m_contextWindow{50};
    bool m_autoTrigger{true};
    int m_triggerDelay{100};
    std::vector<char> m_triggerChars;
    bool m_initialized{false};
    bool m_streaming{false};

    CompletionCallback m_completionCallback;
    StreamCallback m_streamCallback;

    std::map<std::string, std::string> m_fileContexts;
    std::string m_projectContext;

    CompletionResult generateCompletions(const CompletionContext& context);
    std::string buildPrompt(const CompletionContext& context);
    std::vector<CompletionItem> parseResponse(const std::string& response);
};

// Global instance
AICompletionEngine& getAICompletionEngine();

} // namespace RawrXD::AI
