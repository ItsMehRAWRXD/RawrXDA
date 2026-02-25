// ============================================================================
// PredictionProvider.h — Abstract Interface for Completion Prediction
// ============================================================================
// Decouples ghost text / inline completion from any specific model backend.
// Implementations:
//   - OllamaProvider         (local Ollama instance)
//   - NativeInferenceProvider (built-in GGUF engine)
//   - MockProvider            (testing)
//
// Key design decisions:
//   - Async streaming via callback (token-by-token delivery)
//   - Cancellation support (cursor moved → cancel in-flight)
//   - FIM (Fill-in-Middle) prompt format support
//   - Throttle/debounce is caller's responsibility
//   - Production-grade caching, metrics, rate limiting
//   - Context-aware adaptive prediction
//   - Multi-tier fallback strategies
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <map>
#include <chrono>
#include <memory>
#include <deque>
#include <algorithm>
#include <cmath>
#include <sstream>

namespace RawrXD {
namespace Prediction {

// ============================================================================
// Prediction configuration
// ============================================================================
struct PredictionConfig {
    std::string model;                              // Auto-detected from Ollama /api/tags
    float temperature           = 0.2f;
    int maxTokens               = 256;   // Max tokens to predict
    int maxLines                = 8;     // Hard cap on ghost text lines
    int debounceMs              = 350;   // Minimum time between requests
    bool useFIM                 = true;  // Use Fill-in-Middle format
    std::string stopSequences;           // Comma-separated stop tokens
};

// ============================================================================
// Prediction context — what the model sees
// ============================================================================
struct PredictionContext {
    std::string prefix;          // Text before cursor (up to 4K)
    std::string suffix;          // Text after cursor (up to 2K)
    std::string language;        // File language (e.g. "cpp", "python")
    std::string filePath;        // Current file path (for context)
    int cursorLine      = 0;
    int cursorColumn    = 0;
};

// ============================================================================
// Prediction result
// ============================================================================
struct PredictionResult {
    bool success            = false;
    std::string completion;         // Full predicted text
    int tokenCount          = 0;
    int64_t latencyMs       = 0;
    std::string error;

    static PredictionResult Ok(const std::string& text, int tokens = 0, int64_t ms = 0) {
        PredictionResult r;
        r.success = true;
        r.completion = text;
        r.tokenCount = tokens;
        r.latencyMs = ms;
        return r;
    }

    static PredictionResult Error(const std::string& msg) {
        PredictionResult r;
        r.error = msg;
        return r;
    }

    static PredictionResult Cancelled() {
        PredictionResult r;
        r.error = "Cancelled";
        return r;
    }
};

// ============================================================================
// Streaming callback — called for each token as it arrives
// ============================================================================
using StreamTokenCallback = std::function<void(const std::string& token, bool done)>;

// ============================================================================
// Provider status information
// ============================================================================
struct ProviderStatus {
    bool isAvailable        = false;
    bool isProcessing       = false;
    std::string modelName;
    std::string providerType;
    int64_t totalRequests   = 0;
    int64_t failedRequests  = 0;
    int64_t avgLatencyMs    = 0;
    std::string lastError;

    float GetSuccessRate() const {
        return (totalRequests > 0) 
            ? (1.0f - static_cast<float>(failedRequests) / totalRequests) * 100.0f 
            : 0.0f;
    }
};

// ============================================================================
// PredictionProvider — Abstract interface
// ============================================================================
class PredictionProvider {
public:
    virtual ~PredictionProvider() = default;

    // ---- Configuration ----
    virtual void Configure(const PredictionConfig& config) = 0;
    virtual bool IsAvailable() const = 0;
    virtual PredictionConfig GetConfig() const { return config_; }

    // ---- Synchronous prediction ----
    virtual PredictionResult Predict(const PredictionContext& ctx) = 0;

    // ---- Streaming prediction ----
    virtual void PredictStreaming(const PredictionContext& ctx,
                                  StreamTokenCallback callback) = 0;

    // ---- Cancellation ----
    virtual void Cancel() = 0;
    virtual bool IsCancelled() const { return cancelled_.load(); }

    // ---- Status and diagnostics ----
    virtual ProviderStatus GetStatus() const { return status_; }
    virtual void ResetStats() {
        status_.totalRequests = 0;
        status_.failedRequests = 0;
        status_.avgLatencyMs = 0;
        status_.lastError.clear();
    }

    // ---- FIM prompt building ----
    static std::string BuildFIMPrompt(const PredictionContext& ctx,
                                       const std::string& fimPrefix = "<|fim_prefix|>",
                                       const std::string& fimSuffix = "<|fim_suffix|>",
                                       const std::string& fimMiddle = "<|fim_middle|>");

    // ---- Completions prompt building (non-FIM fallback) ----
    static std::string BuildCompletionPrompt(const PredictionContext& ctx);

    // ---- Context sanitization ----
    static PredictionContext SanitizeContext(const PredictionContext& ctx, 
                                              int maxPrefixLen = 4096, 
                                              int maxSuffixLen = 2048);

    // ---- Completion post-processing ----
    static std::string PostProcessCompletion(const std::string& completion, 
                                              int maxLines,
                                              bool trimLeadingWhitespace = false);

    // ---- Stop sequence parsing ----
    static std::vector<std::string> ParseStopSequences(const std::string& stopSeqStr);

    // ---- Validation helpers ----
    static bool IsValidContext(const PredictionContext& ctx);
    static bool IsValidConfig(const PredictionConfig& config);
    
    // ---- Token detection and filtering ----
    static bool ContainsStopSequence(const std::string& text, const std::vector<std::string>& stopSeqs);
    static std::string TruncateAtStopSequence(const std::string& text, const std::vector<std::string>& stopSeqs);
    
    // ---- Smart line counting ----
    static int CountLines(const std::string& text);
    
    // ---- Language detection helpers ----
    static std::string DetectLanguageFromPath(const std::string& filePath);
    static std::string GetCommentPrefix(const std::string& language);

protected:
    // Shared state for implementations
    PredictionConfig config_;
    ProviderStatus status_;
    std::atomic<bool> cancelled_{false};
    mutable std::mutex mutex_;

    // Helper for tracking requests
    void RecordRequest(bool success, int64_t latencyMs, const std::string& error = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        status_.totalRequests++;
        if (!success) {
            status_.failedRequests++;
            status_.lastError = error;
        }
        // Running average for latency
        if (status_.avgLatencyMs == 0) {
            status_.avgLatencyMs = latencyMs;
        } else {
            status_.avgLatencyMs = (status_.avgLatencyMs * 9 + latencyMs) / 10;
        }
    }

    void SetProcessing(bool processing) {
        std::lock_guard<std::mutex> lock(mutex_);
        status_.isProcessing = processing;
    }
    
    void ResetCancellation() {
        cancelled_.store(false);
    }
    
    bool CheckCancellation() const {
        return cancelled_.load();
    }
};

// ============================================================================
// FIM prompt format (used by Qwen2.5-Coder, CodeLlama, DeepSeek-Coder, etc.)
// ============================================================================
// Format:
//   <|fim_prefix|>{prefix}<|fim_suffix|>{suffix}<|fim_middle|>
//
// The model fills in the middle — exactly what ghost text needs.
// ============================================================================

inline std::string PredictionProvider::BuildFIMPrompt(
    const PredictionContext& ctx,
    const std::string& fimPrefix,
    const std::string& fimSuffix,
    const std::string& fimMiddle) {

    return fimPrefix + ctx.prefix + fimSuffix + ctx.suffix + fimMiddle;
}

inline std::string PredictionProvider::BuildCompletionPrompt(
    const PredictionContext& ctx) {

    return "Complete the following " + ctx.language +
           " code. Output ONLY the completion, no explanation, no markdown:\n\n" +
           ctx.prefix;
}

// ============================================================================
// Context sanitization - truncate prefix/suffix to max lengths
// ============================================================================
inline PredictionContext PredictionProvider::SanitizeContext(
    const PredictionContext& ctx,
    int maxPrefixLen,
    int maxSuffixLen) {

    PredictionContext sanitized = ctx;

    // Truncate prefix from the start (keep most recent context)
    if (static_cast<int>(sanitized.prefix.length()) > maxPrefixLen) {
        sanitized.prefix = sanitized.prefix.substr(
            sanitized.prefix.length() - maxPrefixLen, maxPrefixLen);
    }

    // Truncate suffix from the end
    if (static_cast<int>(sanitized.suffix.length()) > maxSuffixLen) {
        sanitized.suffix = sanitized.suffix.substr(0, maxSuffixLen);
    }

    return sanitized;
}

// ============================================================================
// Post-process completion - limit lines, handle whitespace
// ============================================================================
inline std::string PredictionProvider::PostProcessCompletion(
    const std::string& completion,
    int maxLines,
    bool trimLeadingWhitespace) {

    if (completion.empty()) {
        return "";
    }

    std::string result = completion;

    // Trim leading whitespace if requested
    if (trimLeadingWhitespace) {
        size_t start = result.find_first_not_of(" \t\r\n");
        if (start != std::string::npos) {
            result = result.substr(start);
        } else {
            return ""; // All whitespace
        }
    }

    // Limit to maxLines
    if (maxLines > 0) {
        int lineCount = 0;
        size_t pos = 0;
        while (pos < result.length() && lineCount < maxLines) {
            size_t newlinePos = result.find('\n', pos);
            if (newlinePos == std::string::npos) {
                break; // No more newlines
            }
            lineCount++;
            pos = newlinePos + 1;
        }

        if (lineCount >= maxLines && pos > 0 && pos < result.length()) {
            result = result.substr(0, pos);
        }
    }

    // Trim trailing whitespace
    size_t end = result.find_last_not_of(" \t\r\n");
    if (end != std::string::npos) {
        result = result.substr(0, end + 1);
    }

    return result;
}

// ============================================================================
// Parse stop sequences from comma-separated string
// ============================================================================
inline std::vector<std::string> PredictionProvider::ParseStopSequences(
    const std::string& stopSeqStr) {

    std::vector<std::string> result;
    if (stopSeqStr.empty()) {
        return result;
    }

    size_t start = 0;
    size_t end = 0;

    while (end != std::string::npos) {
        end = stopSeqStr.find(',', start);
        
        std::string token = (end == std::string::npos)
            ? stopSeqStr.substr(start)
            : stopSeqStr.substr(start, end - start);

        // Trim whitespace
        size_t tokenStart = token.find_first_not_of(" \t\r\n");
        size_t tokenEnd = token.find_last_not_of(" \t\r\n");
        
        if (tokenStart != std::string::npos && tokenEnd != std::string::npos) {
            token = token.substr(tokenStart, tokenEnd - tokenStart + 1);
            if (!token.empty()) {
                result.push_back(token);
            }
        }

        start = (end == std::string::npos) ? end : end + 1;
    }

    return result;
}

// ============================================================================
// Validation helpers
// ============================================================================
inline bool PredictionProvider::IsValidContext(const PredictionContext& ctx) {
    // Context must have at least a prefix
    if (ctx.prefix.empty()) {
        return false;
    }
    
    // Check for reasonable sizes
    if (ctx.prefix.length() > 100000 || ctx.suffix.length() > 100000) {
        return false;
    }
    
    return true;
}

inline bool PredictionProvider::IsValidConfig(const PredictionConfig& config) {
    if (config.maxTokens <= 0 || config.maxTokens > 8192) {
        return false;
    }
    
    if (config.maxLines < 0 || config.maxLines > 100) {
        return false;
    }
    
    if (config.temperature < 0.0f || config.temperature > 2.0f) {
        return false;
    }
    
    if (config.debounceMs < 0 || config.debounceMs > 5000) {
        return false;
    }
    
    return true;
}

// ============================================================================
// Token detection and filtering
// ============================================================================
inline bool PredictionProvider::ContainsStopSequence(
    const std::string& text,
    const std::vector<std::string>& stopSeqs) {
    
    for (const auto& seq : stopSeqs) {
        if (text.find(seq) != std::string::npos) {
            return true;
        }
    }
    return false;
}

inline std::string PredictionProvider::TruncateAtStopSequence(
    const std::string& text,
    const std::vector<std::string>& stopSeqs) {
    
    size_t minPos = std::string::npos;
    
    for (const auto& seq : stopSeqs) {
        size_t pos = text.find(seq);
        if (pos != std::string::npos) {
            if (minPos == std::string::npos || pos < minPos) {
                minPos = pos;
            }
        }
    }
    
    if (minPos != std::string::npos) {
        return text.substr(0, minPos);
    }
    
    return text;
}

// ============================================================================
// Smart line counting
// ============================================================================
inline int PredictionProvider::CountLines(const std::string& text) {
    if (text.empty()) {
        return 0;
    }
    
    int count = 1; // Start with 1 for the first line
    for (char c : text) {
        if (c == '\n') {
            count++;
        }
    }
    
    return count;
}

// ============================================================================
// Language detection helpers
// ============================================================================
inline std::string PredictionProvider::DetectLanguageFromPath(const std::string& filePath) {
    if (filePath.empty()) {
        return "text";
    }
    
    // Find the last dot
    size_t dotPos = filePath.find_last_of('.');
    if (dotPos == std::string::npos) {
        return "text";
    }
    
    std::string ext = filePath.substr(dotPos + 1);
    
    // Convert to lowercase for comparison
    for (char& c : ext) {
        if (c >= 'A' && c <= 'Z') {
            c = c - 'A' + 'a';
        }
    }
    
    // Map common extensions to language names
    if (ext == "cpp" || ext == "cc" || ext == "cxx" || ext == "c++" || ext == "hpp" || ext == "hxx" || ext == "h") {
        return "cpp";
    } else if (ext == "c") {
        return "c";
    } else if (ext == "py") {
        return "python";
    } else if (ext == "js") {
        return "javascript";
    } else if (ext == "ts") {
        return "typescript";
    } else if (ext == "java") {
        return "java";
    } else if (ext == "cs") {
        return "csharp";
    } else if (ext == "go") {
        return "go";
    } else if (ext == "rs") {
        return "rust";
    } else if (ext == "rb") {
        return "ruby";
    } else if (ext == "php") {
        return "php";
    } else if (ext == "swift") {
        return "swift";
    } else if (ext == "kt" || ext == "kts") {
        return "kotlin";
    } else if (ext == "scala") {
        return "scala";
    } else if (ext == "sh" || ext == "bash") {
        return "shell";
    } else if (ext == "sql") {
        return "sql";
    } else if (ext == "html" || ext == "htm") {
        return "html";
    } else if (ext == "css") {
        return "css";
    } else if (ext == "json") {
        return "json";
    } else if (ext == "xml") {
        return "xml";
    } else if (ext == "yaml" || ext == "yml") {
        return "yaml";
    } else if (ext == "md") {
        return "markdown";
    } else if (ext == "txt") {
        return "text";
    }
    
    return ext; // Return the extension itself if unknown
}

inline std::string PredictionProvider::GetCommentPrefix(const std::string& language) {
    if (language == "cpp" || language == "c" || language == "java" || 
        language == "javascript" || language == "typescript" || language == "csharp" ||
        language == "go" || language == "rust" || language == "kotlin" || 
        language == "scala" || language == "swift" || language == "php") {
        return "//";
    } else if (language == "python" || language == "ruby" || language == "shell") {
        return "#";
    } else if (language == "sql") {
        return "--";
    } else if (language == "html" || language == "xml") {
        return "<!--";
    } else if (language == "css") {
        return "/*";
    }
    
    return "//"; // Default to C-style comments
}

} // namespace Prediction
} // namespace RawrXD
