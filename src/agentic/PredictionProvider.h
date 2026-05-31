// ============================================================================
// PredictionProvider.h — Prediction interface for completions/ghost text
// ============================================================================
// Single source of truth for prediction types used by:
//   - Win32IDE ghost text (inline completions)
//   - Agentic streaming providers (Ollama, Titan, etc.)
//
// NOTE: Keep this header production-only. Any self-tests must live in a
// dedicated test translation unit under src/tests or tests/.
// ============================================================================

#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace RawrXD
{
namespace Prediction
{

struct PredictionConfig
{
    std::string model;
    float temperature = 0.7f;
    int maxTokens = 2048;
    int maxLines = 5;
    bool useFIM = true;
    std::string stopSequences;
};

struct PredictionContext
{
    std::string prefix;
    std::string suffix;
    std::string filePath;
    std::string language;
    int cursorLine = 0;
    int cursorColumn = 0;
};

struct PredictionResult
{
    std::string text;
    std::string completion;
    bool success = false;
    int tokens = 0;
    int64_t elapsedMs = 0;
    std::string error;

    PredictionResult() = default;
    explicit PredictionResult(const std::string& t) : text(t), completion(t), success(true) {}

    static PredictionResult Ok(const std::string& comp, int tok = 0, int64_t elapsed = 0)
    {
        PredictionResult r;
        r.completion = comp;
        r.text = comp;
        r.tokens = tok;
        r.elapsedMs = elapsed;
        r.success = true;
        return r;
    }

    static PredictionResult Error(const std::string& msg)
    {
        PredictionResult r;
        r.error = msg;
        r.success = false;
        return r;
    }

    static PredictionResult Cancelled()
    {
        PredictionResult r;
        r.error = "Cancelled";
        r.success = false;
        return r;
    }
};

// StreamTokenCallback: (token, is_final) -> should_continue
using StreamTokenCallback = std::function<bool(const std::string& token, bool isFinal)>;

class PredictionProvider
{
  public:
    virtual ~PredictionProvider() = default;
    virtual void Configure(const PredictionConfig& config) = 0;
    virtual bool IsAvailable() const = 0;
    virtual PredictionResult Predict(const PredictionContext& ctx) = 0;
    virtual void PredictStreaming(const PredictionContext& ctx, StreamTokenCallback callback) = 0;
    virtual void Cancel() = 0;
};

}  // namespace Prediction
}  // namespace RawrXD
