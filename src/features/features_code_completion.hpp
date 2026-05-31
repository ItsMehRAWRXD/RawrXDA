#pragma once
/**
 * features_code_completion.hpp - Features 1-10: Code Completion
 * 
 * 1. Real-Time Inline Autocomplete
 * 2. Multi-Line Code Completion
 * 3. Tab Autocomplete with Next-Edit Prediction
 * 4. Context-Aware Completions
 * 5. Natural Language to Code
 * 6. Snippet Generation from Description
 * 7. Auto-Import Suggestions
 * 8. Parameter Completion
 * 9. Error Correction Suggestions
 * 10. Supermaven Fast Autocomplete
 */

#include "ai_ide_features.hpp"

namespace rawrxd {

//=============================================================================
// FEATURE 1: Real-Time Inline Autocomplete
//=============================================================================

class Feature_RealTimeAutocomplete {
public:
    struct Config {
        uint32_t debounceMs = 150;
        uint32_t maxSuggestions = 3;
        uint32_t maxSuggestionLength = 100;
        bool showGhostText = true;
    };

    explicit Feature_RealTimeAutocomplete(const Config& config = {});

    void onTextChanged(const std::string& text, size_t cursorPos);
    std::vector<std::string> getSuggestions();
    void acceptSuggestion(size_t index);
    void dismissSuggestions();

    void setProvider(std::shared_ptr<AIProvider> provider) { provider_ = provider; }
    void setEditor(EditorIntegration* editor) { editor_ = editor; }

private:
    Config config_;
    std::shared_ptr<AIProvider> provider_;
    EditorIntegration* editor_ = nullptr;

    std::string currentPrefix_;
    std::vector<std::string> suggestions_;
    std::future<std::string> pendingRequest_;
    std::chrono::steady_clock::time_point lastKeystroke_;
    std::mutex mutex_;

    std::string buildPrompt(const std::string& prefix, const std::string& context);
};

inline Feature_RealTimeAutocomplete::Feature_RealTimeAutocomplete(const Config& config)
    : config_(config) {}

inline void Feature_RealTimeAutocomplete::onTextChanged(
    const std::string& text, size_t cursorPos) {

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - lastKeystroke_);

    if (elapsed.count() < config_.debounceMs) {
        return;
    }

    if (cursorPos > 0 && cursorPos <= text.length()) {
        currentPrefix_ = text.substr(0, cursorPos);
        size_t lineStart = currentPrefix_.rfind('\n');
        if (lineStart != std::string::npos) {
            currentPrefix_ = currentPrefix_.substr(lineStart + 1);
        }
    }

    lastKeystroke_ = now;
}

inline std::vector<std::string> Feature_RealTimeAutocomplete::getSuggestions() {
    return suggestions_;
}

inline void Feature_RealTimeAutocomplete::acceptSuggestion(size_t index) {
    if (index < suggestions_.size() && editor_) {
        editor_->insertAtCursor(suggestions_[index]);
        suggestions_.clear();
    }
}

inline void Feature_RealTimeAutocomplete::dismissSuggestions() {
    suggestions_.clear();
}

//=============================================================================
// FEATURE 2: Multi-Line Code Completion
//=============================================================================

class Feature_MultiLineCompletion {
public:
    struct Config {
        uint32_t maxLines = 20;
        bool includeImports = true;
        bool preserveStyle = true;
    };

    explicit Feature_MultiLineCompletion(const Config& config = {});

    std::string completeFunction(const std::string& signature, const std::string& doc);
    std::string completeClass(const std::string& signature, const std::string& members);
    std::string completeBlock(const std::string& startPattern, const std::string& context);

    void setProvider(std::shared_ptr<AIProvider> provider);

private:
    Config config_;
    std::shared_ptr<AIProvider> provider_;
};

inline Feature_MultiLineCompletion::Feature_MultiLineCompletion(const Config& config)
    : config_(config) {}

inline std::string Feature_MultiLineCompletion::completeFunction(
    const std::string& signature, const std::string& doc) {

    if (!provider_) return "";

    std::string prompt = "Complete this function implementation:\n\nFunction signature: "
        + signature + "\n\nDocumentation: " + doc
        + "\n\nOutput only the function body, properly indented. No explanations.";

    auto future = provider_->complete(prompt);
    return future.get();
}

inline void Feature_MultiLineCompletion::setProvider(std::shared_ptr<AIProvider> p) {
    provider_ = p;
}

//=============================================================================
// FEATURE 3: Tab Autocomplete with Next-Edit Prediction
//=============================================================================

class Feature_TabAutocomplete {
public:
    struct Prediction {
        std::string text;
        size_t startPosition = 0;
        float confidence = 0.0f;
        std::vector<std::pair<size_t, size_t>> affectedRanges;
    };

    void analyzeDocument(const std::string& content, size_t cursorPos);
    Prediction getNextPrediction();
    std::vector<Prediction> getAllPredictions();
    void recordAcceptance(const Prediction& pred);

    void setProvider(std::shared_ptr<AIProvider> provider);

private:
    std::shared_ptr<AIProvider> provider_;
    std::vector<Prediction> predictions_;
    std::vector<std::string> recentEdits_;
};

//=============================================================================
// FEATURE 4: Context-Aware Completions
//=============================================================================

class Feature_ContextAwareCompletion {
public:
    void updateContext(const CodeContext& context);
    std::vector<std::string> getCompletions(const std::string& prefix);

    void addOpenFile(const std::string& path, const std::string& content);
    void addImport(const std::string& module, const std::string& symbol);
    void addVariable(const std::string& name, const std::string& type, const std::string& value);

    void setProvider(std::shared_ptr<AIProvider> provider);

private:
    std::shared_ptr<AIProvider> provider_;
    std::string context_;
    std::unordered_map<std::string, std::string> openFiles_;
    std::unordered_map<std::string, std::string> imports_;
    std::unordered_map<std::string, std::string> variables_;
};

//=============================================================================
// FEATURE 5: Natural Language to Code
//=============================================================================

class Feature_NaturalLanguageToCode {
public:
    std::string generateFromDescription(
        const std::string& description,
        const std::string& language);

    std::string generateFunction(const std::string& description);
    std::string generateClass(const std::string& description);
    std::string generateTests(const std::string& functionCode);

    void setProvider(std::shared_ptr<AIProvider> provider);

private:
    std::shared_ptr<AIProvider> provider_;
};

inline std::string Feature_NaturalLanguageToCode::generateFromDescription(
    const std::string& description, const std::string& language) {

    if (!provider_) return "";

    std::string prompt = "Generate " + language + " code for:\n\n" + description
        + "\n\nOutput only the code, no explanations. Include necessary imports.";

    return provider_->complete(prompt).get();
}

inline void Feature_NaturalLanguageToCode::setProvider(std::shared_ptr<AIProvider> p) {
    provider_ = p;
}

//=============================================================================
// FEATURE 6: Snippet Generation from Description
//=============================================================================

class Feature_SnippetGeneration {
public:
    struct Snippet {
        std::string code;
        std::string name;
        std::string description;
        std::vector<std::string> placeholders;
    };

    Snippet generateSnippet(const std::string& pattern, const std::string& language);
    std::vector<Snippet> getCommonPatterns(const std::string& language);

    void setProvider(std::shared_ptr<AIProvider> provider);

private:
    std::shared_ptr<AIProvider> provider_;

    std::unordered_map<std::string, std::vector<Snippet>> builtInPatterns_ = {
        {"cpp", {
            {"for (int i = 0; i < $1; ++i) { $2 }", "for loop", "Basic for loop", {"$1", "$2"}},
            {"class $1 {\npublic:\n    $2\n};", "class", "Basic class", {"$1", "$2"}},
        }},
        {"python", {
            {"for i in range($1):\n    $2", "for loop", "Python for loop", {"$1", "$2"}},
            {"def $1($2):\n    $3", "function", "Python function", {"$1", "$2", "$3"}},
        }}
    };
};

//=============================================================================
// FEATURE 7: Auto-Import Suggestions
//=============================================================================

class Feature_AutoImport {
public:
    struct ImportSuggestion {
        std::string module;
        std::string symbol;
        std::string importStatement;
        float confidence = 0.0f;
    };

    void analyzeUsage(const std::string& symbol, const std::string& context);
    std::vector<ImportSuggestion> suggestImports();
    void applyImport(const ImportSuggestion& suggestion);

    void setEditor(EditorIntegration* editor) { editor_ = editor; }
    void setProvider(std::shared_ptr<AIProvider> provider) { provider_ = provider; }

private:
    std::shared_ptr<AIProvider> provider_;
    EditorIntegration* editor_ = nullptr;
    std::vector<ImportSuggestion> suggestions_;

    std::unordered_map<std::string, std::vector<ImportSuggestion>> commonImports_;
};

//=============================================================================
// FEATURE 8: Parameter Completion
//=============================================================================

class Feature_ParameterCompletion {
public:
    struct Parameter {
        std::string name;
        std::string type;
        std::string defaultValue;
        std::string description;
    };

    std::vector<Parameter> getParameters(const std::string& functionName, const std::string& context);
    std::string completeParameter(const Parameter& param, const std::string& context);

    void setProvider(std::shared_ptr<AIProvider> provider);

private:
    std::shared_ptr<AIProvider> provider_;
};

//=============================================================================
// FEATURE 9: Error Correction Suggestions
//=============================================================================

class Feature_ErrorCorrection {
public:
    struct ErrorSuggestion {
        std::string error;
        std::string fix;
        std::string explanation;
        std::vector<TextEdit> edits;
        float confidence = 0.0f;
    };

    std::vector<ErrorSuggestion> analyzeError(
        const std::string& errorMessage,
        const std::string& code);
    void applyFix(const ErrorSuggestion& suggestion);

    void setProvider(std::shared_ptr<AIProvider> provider);
    void setEditor(EditorIntegration* editor);

private:
    std::shared_ptr<AIProvider> provider_;
    EditorIntegration* editor_ = nullptr;
};

//=============================================================================
// FEATURE 10: Supermaven Fast Autocomplete
//=============================================================================

class Feature_FastAutocomplete {
public:
    struct Config {
        uint32_t targetLatencyMs = 50;
        bool speculativeDecoding = true;
        uint32_t cacheSize = 10000;
    };

    explicit Feature_FastAutocomplete(const Config& config);

    std::string complete(const std::string& prefix);
    void precompute(const std::string& context);

    void setProvider(std::shared_ptr<AIProvider> provider);

private:
    Config config_;
    std::shared_ptr<AIProvider> provider_;
    std::unordered_map<std::string, std::string> cache_;
    std::mutex cacheMutex_;

    std::string fastPath(const std::string& prefix);
    std::string slowPath(const std::string& prefix);
};

inline Feature_FastAutocomplete::Feature_FastAutocomplete(const Config& config)
    : config_(config) {}

inline std::string Feature_FastAutocomplete::complete(const std::string& prefix) {
    std::string result = fastPath(prefix);
    if (!result.empty()) return result;
    return slowPath(prefix);
}

inline std::string Feature_FastAutocomplete::fastPath(const std::string& prefix) {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    auto it = cache_.find(prefix);
    if (it != cache_.end()) return it->second;
    return "";
}

inline std::string Feature_FastAutocomplete::slowPath(const std::string& prefix) {
    if (!provider_) return "";
    auto result = provider_->complete(prefix).get();
    std::lock_guard<std::mutex> lock(cacheMutex_);
    if (cache_.size() >= config_.cacheSize) cache_.clear();
    cache_[prefix] = result;
    return result;
}

inline void Feature_FastAutocomplete::setProvider(std::shared_ptr<AIProvider> p) {
    provider_ = p;
}

} // namespace rawrxd
