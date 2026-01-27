#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <chrono>

namespace RawrXD {
namespace IDE {

// Represents a single code completion suggestion
struct CompletionSuggestion {
    std::string text;              // The completion text
    std::string label;             // Short label for display
    std::string description;       // Detailed description
    float confidence;              // 0.0-1.0 confidence score
    int priority;                  // Higher = shown first
    std::string kind;              // "function", "variable", "class", etc.
    std::string documentation;     // Extended docs
    std::vector<std::string> params; // For functions: parameter names
};

// Completion context information
struct CompletionContext {
    std::string currentLine;       // Current incomplete line
    std::string linePrefix;        // Text before cursor
    std::string lineSuffix;        // Text after cursor
    std::string filePath;          // Current file path
    std::string fileLanguage;      // Language (cpp, python, etc.)
    int lineNumber;                // Current line number
    int columnNumber;              // Cursor column
    std::string contextBefore;     // Previous 50 lines
    std::string contextAfter;      // Next 10 lines
    std::vector<std::string> visibleSymbols; // In-scope symbols
};

// Intelligent code completion engine
class IntelligentCompletionEngine {
public:
    IntelligentCompletionEngine();
    ~IntelligentCompletionEngine() = default;

    // Get completion suggestions for a context
    std::vector<CompletionSuggestion> getCompletions(
        const CompletionContext& context,
        int maxSuggestions = 10
    );

    // Get multi-line completions (full method/block)
    std::vector<std::string> getMultiLineCompletions(
        const CompletionContext& context,
        int maxLines = 20
    );

    // Validate completion against file syntax
    bool validateCompletion(
        const std::string& completion,
        const std::string& language,
        const std::string& context
    );

    // Get completion metadata
    std::string getCompletionDocumentation(const std::string& completion);
    std::vector<std::string> getCompletionParams(const std::string& functionName);

    // Configuration
    void setMinConfidenceThreshold(float threshold);
    void setMaxSuggestions(int count);
    void setLanguageModelUrl(const std::string& url);
    void setEmbeddingModelUrl(const std::string& url);

private:
    struct CachedCompletion {
        std::vector<CompletionSuggestion> suggestions;
        std::chrono::system_clock::time_point timestamp;
    };

    // Model inference
    std::vector<CompletionSuggestion> inferCompletions(const CompletionContext& context);
    
    // Scoring and ranking
    void scoreCompletions(
        std::vector<CompletionSuggestion>& suggestions,
        const CompletionContext& context
    );

    // Fuzzy matching
    float fuzzyMatchScore(
        const std::string& suggestion,
        const std::string& userInput
    );

    // Context enrichment
    std::string enrichContext(const CompletionContext& context);

    float m_confidenceThreshold;
    int m_maxSuggestions;
    std::string m_modelUrl;
    std::string m_embeddingUrl;
    std::unordered_map<std::string, CachedCompletion> m_cache;
};

} // namespace IDE
} // namespace RawrXD
