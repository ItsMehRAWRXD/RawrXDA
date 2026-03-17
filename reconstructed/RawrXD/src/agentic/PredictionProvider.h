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

namespace RawrXD {
namespace Prediction {

// ============================================================================
// Prediction configuration
// ============================================================================
struct PredictionConfig {
    std::string model           = "qwen2.5-coder:14b";
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
// PredictionProvider — Abstract interface
// ============================================================================
class PredictionProvider {
public:
    virtual ~PredictionProvider() = default;

    // ---- Configuration ----
    virtual void Configure(const PredictionConfig& config) = 0;
    virtual bool IsAvailable() const = 0;

    // ---- Synchronous prediction ----
    virtual PredictionResult Predict(const PredictionContext& ctx) = 0;

    // ---- Streaming prediction ----
    virtual void PredictStreaming(const PredictionContext& ctx,
                                  StreamTokenCallback callback) = 0;

    // ---- Cancellation ----
    virtual void Cancel() = 0;

    // ---- FIM prompt building ----
    static std::string BuildFIMPrompt(const PredictionContext& ctx,
                                       const std::string& fimPrefix = "<|fim_prefix|>",
                                       const std::string& fimSuffix = "<|fim_suffix|>",
                                       const std::string& fimMiddle = "<|fim_middle|>");

    // ---- Completions prompt building (non-FIM fallback) ----
    static std::string BuildCompletionPrompt(const PredictionContext& ctx);
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

} // namespace Prediction
} // namespace RawrXD
