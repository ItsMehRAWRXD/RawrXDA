#pragma once
#include <vector>
#include <string>
#include <map>
#include <chrono>

namespace RawrXD {

class CPUInferenceEngine;

namespace IDE {

struct CompletionContext {
    std::string filePath;
    std::string linePrefix;
    std::string contextBefore;
    std::string currentLine;
    std::string fileLanguage;
    std::vector<std::string> visibleSymbols;
};

struct CompletionSuggestion {
    std::string text;
    std::string label;
    std::string kind;
    float confidence;
    int priority;
};

struct CachedCompletion {
    std::vector<CompletionSuggestion> suggestions;
    std::chrono::system_clock::time_point timestamp;
};

class IntelligentCompletionEngine {
public:
    IntelligentCompletionEngine();
    
    std::vector<CompletionSuggestion> getCompletions(const CompletionContext& context, int maxSuggestions);
    std::vector<std::string> getMultiLineCompletions(const CompletionContext& context, int maxLines);
    
    bool validateCompletion(const std::string& completion, const std::string& language, const std::string& context);
    std::string getCompletionDocumentation(const std::string& completion);
    std::vector<std::string> getCompletionParams(const std::string& functionName);
    
    void setMinConfidenceThreshold(float threshold);
    void setMaxSuggestions(int count);
    void setLanguageModelUrl(const std::string& url);
    void setEmbeddingModelUrl(const std::string& url);

private:
    float m_confidenceThreshold;
    int m_maxSuggestions;
    std::string m_modelUrl;
    std::string m_embeddingUrl;
    std::map<std::string, CachedCompletion> m_cache;
    
    // Internal helpers
    std::vector<CompletionSuggestion> inferCompletions(const CompletionContext& context);
    void scoreCompletions(std::vector<CompletionSuggestion>& suggestions, const CompletionContext& context);
    float fuzzyMatchScore(const std::string& suggestion, const std::string& userInput);
    std::string enrichContext(const CompletionContext& context);
};

} // namespace IDE
} // namespace RawrXD
