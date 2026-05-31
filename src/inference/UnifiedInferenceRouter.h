// ============================================================================
// UnifiedInferenceRouter.h — Single Execution Spine for All IDE Surfaces
// ============================================================================
// Replaces fragmented inference paths with a single canonical router that
// handles all completion requests (ghost text, CLI, agent, chat) through
// a unified FIM-first interface.
//
// Architecture:
//   IDE Surface → UnifiedInferenceRouter → Backend Selector → Real Inference
// ============================================================================

#pragma once

#include "RawrXD_LlamaNative.h"
#include "MLInferenceEngine.hpp"
#include <string>
#include <memory>
#include <functional>
#include <mutex>
#include <unordered_map>

namespace RawrXD {
namespace Inference {

// ============================================================================
// Unified Completion Request — Single Interface for All IDE Surfaces
// ============================================================================

struct UnifiedCompletionRequest {
    std::string prefix;           // Text before cursor (for FIM)
    std::string suffix;           // Text after cursor (for FIM)
    std::string fullPrompt;       // Full prompt (for non-FIM)
    std::string language;         // Programming language
    std::string filePath;         // Source file path
    std::string modelPath;         // Model file path (for loading)
    int32_t cursorLine;           // Cursor line number
    int32_t cursorColumn;         // Cursor column number
    
    // Inference parameters
    int32_t maxTokens = 256;
    float temperature = 0.2f;
    float topP = 0.95f;
    int32_t topK = 48;
    
    // Context information
    std::string workspaceRoot;
    std::vector<std::string> contextFiles;
    
    // Callback for streaming results
    std::function<void(const std::string& token, bool isFinal)> streamCallback;
    
    // Request metadata
    std::string requestId;
    uint64_t timestamp;
    
    UnifiedCompletionRequest() : timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count()) {}
};

struct UnifiedCompletionResult {
    std::string completion;
    std::string error;
    bool success;
    int32_t tokensGenerated;
    int32_t promptTokens;
    double latencyMs;
    std::string backendUsed;
    
    UnifiedCompletionResult() : success(false), tokensGenerated(0), promptTokens(0), latencyMs(0.0) {}
};

// ============================================================================
// Inference Backend Types
// ============================================================================

enum class InferenceBackend {
    GGUF_Native,     // llama.cpp GGUF models
    MoE_Engine,      // Mixture of Experts engine
    Ollama_HTTP,     // Ollama HTTP API
    Titan_Kernel,    // Titan inference kernel
    CPU_Fallback     // CPU-only fallback
};

// ============================================================================
// UnifiedInferenceRouter — Single Spine for All Completion Requests
// ============================================================================

class UnifiedInferenceRouter {
public:
    static UnifiedInferenceRouter& Instance();
    
    // Initialize with available backends
    bool Initialize();
    void Shutdown();
    
    // Main completion interface — all IDE surfaces call this
    UnifiedCompletionResult Complete(const UnifiedCompletionRequest& request);
    
    // Async completion with streaming
    void CompleteAsync(const UnifiedCompletionRequest& request);
    
    // Backend management
    bool RegisterBackend(InferenceBackend backend, std::shared_ptr<void> implementation);
    bool SetDefaultBackend(InferenceBackend backend);
    
    // Model management
    bool LoadModel(const std::string& modelPath, InferenceBackend preferredBackend = InferenceBackend::GGUF_Native);
    void UnloadModel();
    
    // Statistics and monitoring
    struct RouterStats {
        uint64_t totalRequests;
        uint64_t successfulRequests;
        uint64_t failedRequests;
        std::unordered_map<std::string, uint64_t> backendUsage;
        double avgLatencyMs;
    };
    
    RouterStats GetStats() const;
    
    // Configuration
    void SetMaxConcurrentRequests(int32_t max);
    void SetTimeoutMs(int32_t timeout);
    void EnableFallback(bool enable);
    
    // Status queries
    bool IsInitialized() const { return initialized_; }
    bool IsModelLoaded() const { return modelLoaded_; }
    std::vector<InferenceBackend> GetAvailableBackends() const;
    
private:
    UnifiedInferenceRouter();
    ~UnifiedInferenceRouter();
    
    // Backend implementations
    UnifiedCompletionResult CompleteGGUF(const UnifiedCompletionRequest& request);
    UnifiedCompletionResult CompleteMoE(const UnifiedCompletionRequest& request);
    UnifiedCompletionResult CompleteOllama(const UnifiedCompletionRequest& request);
    UnifiedCompletionResult CompleteTitan(const UnifiedCompletionRequest& request);
    UnifiedCompletionResult CompleteCPU(const UnifiedCompletionRequest& request);
    
    // Backend selection logic
    InferenceBackend SelectBackend(const UnifiedCompletionRequest& request);
    
    // Request queuing and throttling
    bool CanAcceptRequest() const;
    void UpdateStats(const UnifiedCompletionResult& result, InferenceBackend backend);
    
    // Member variables
    std::unordered_map<InferenceBackend, std::shared_ptr<void>> backends_;
    InferenceBackend defaultBackend_ = InferenceBackend::GGUF_Native;
    
    std::mutex mutex_;
    bool initialized_ = false;
    bool modelLoaded_ = false;
    bool fallbackEnabled_ = true;
    
    int32_t maxConcurrentRequests_ = 4;
    int32_t currentRequests_ = 0;
    int32_t timeoutMs_ = 10000;
    
    RouterStats stats_;
    std::string loadedModelPath_;
};

// ============================================================================
// Global convenience functions
// ============================================================================

UnifiedCompletionResult CompleteFIM(
    const std::string& prefix,
    const std::string& suffix,
    const std::string& language = "",
    const std::string& filePath = ""
);

void CompleteFIMAsync(
    const std::string& prefix,
    const std::string& suffix,
    std::function<void(const UnifiedCompletionResult&)> callback,
    const std::string& language = "",
    const std::string& filePath = ""
);

} // namespace Inference
} // namespace RawrXD