// rawr_inference_pipeline.h — Unified local-inference pipeline
// Both CLI (RawrXD-Serve) and the Win32IDE use this path for local GGUF models.
// The implementation delegates to the InferencePlugin DLL bridge, which is the
// same executable-adjacent DLL that `rawrxd serve` loads.
#pragma once

#include <functional>
#include <string>
#include <vector>

namespace RawrXD {

// ---------------------------------------------------------------------------
// InferenceMessage — one turn in a multi-turn conversation
// ---------------------------------------------------------------------------
struct InferenceMessage {
    std::string role;     // "system", "user", "assistant"
    std::string content;
};

// ---------------------------------------------------------------------------
// PipelineRequest — unified request for both CLI and Win32IDE paths
// ---------------------------------------------------------------------------
struct PipelineRequest {
    std::string model;                          // model name or full path
    std::string prompt;                         // single-turn prompt (used when messages is empty)
    std::vector<InferenceMessage> messages;     // multi-turn conversation history
    float       temperature  = 0.7f;
    int         numPredict   = 512;
    bool        stream       = true;
};

// ---------------------------------------------------------------------------
// InferenceCallbacks — streaming callbacks matching CLI StreamTokenFn contract
// ---------------------------------------------------------------------------
struct InferenceCallbacks {
    // Called for each partial token.  `done` is true on the final call.
    std::function<void(const std::string& token, bool done)> onToken;
    // Called once after the last token with the full accumulated text.
    std::function<void(const std::string& accumulated)>      onComplete;
    // Called on hard error (DLL missing, model load failure, generation error).
    std::function<void(const std::string& errorMsg)>         onError;
};

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------

/// Returns true if the InferencePlugin DLL is already loaded and a model is
/// ready.  Does NOT attempt to load the DLL or the model.
bool isInferencePipelineReady();

/// Returns true if RAWRXD_PIPELINE_STRICT=1 is set in the environment.
/// When strict mode is on, callers MUST treat a `false` return from
/// runLocalInferencePipeline as fatal — no fallback to agentic/Ollama is
/// permitted.  Used for CLI/UI parity testing.
bool isPipelineStrictMode();

/// Attempt to load the InferencePlugin DLL (if not already loaded) and then
/// load the specified model weights.
/// Returns an empty string on success; a human-readable error on failure.
std::string initInferencePipeline(const std::string& modelPath);

/// Run streaming inference through the CLI-identical InferencePlugin path.
/// Returns true if generation completed without error.
/// Returns false if the plugin DLL is not available — caller should fall back
/// to the agentic bridge or Ollama path.
bool runLocalInferencePipeline(const PipelineRequest& req, const InferenceCallbacks& cbs);

} // namespace RawrXD
