// ============================================================================
// native_inference_pipeline.hpp — Unified Native Inference Pipeline
// ============================================================================
// High-level pipeline that connects the Win32 IDE to the LocalAICore.
// Provides:
//   - One-call model loading from GGUF path
//   - Text-in / text-out inference
//   - Streaming token output via Win32 messages
//   - Background inference on worker thread
//   - Model hot-swap without IDE restart
//   - Performance monitoring integration
//   - Integration with hotpatch system for runtime model modification
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include "local_ai_core.hpp"
#include "native_speed_layer.hpp"
#include "model_memory_hotpatch.hpp"
#include <cstdint>
#include <mutex>
#include <atomic>
#include <thread>

namespace RawrXD {

// ============================================================================
// Inference Backend Selection
// ============================================================================
enum class InferenceBackend : uint32_t {
    None        = 0,
    LocalAICore = 1,
    LlamaDll    = 2,   // llama.cpp C API via dynamically loaded llama.dll
};

// ============================================================================
// Win32 message IDs for streaming tokens to the IDE
// ============================================================================
#define WM_NATIVE_AI_TOKEN      (WM_USER + 0x500)
#define WM_NATIVE_AI_COMPLETE   (WM_USER + 0x501)
#define WM_NATIVE_AI_ERROR      (WM_USER + 0x502)
#define WM_NATIVE_AI_PROGRESS   (WM_USER + 0x503)

// ============================================================================
// Pipeline State
// ============================================================================
enum class PipelineState : uint32_t {
    Idle            = 0,
    Loading         = 1,
    Ready           = 2,
    Inferring       = 3,
    Stopping        = 4,
    Error           = 5,
};

const char* PipelineStateName(PipelineState s);

// ============================================================================
// PipelineConfig — Configuration for the inference pipeline
// ============================================================================
struct PipelineConfig {
    // Model
    char        modelPath[512]      = {};
    bool        autoLoadOnInit      = false;

    // Inference defaults
    LocalAI::SamplerConfig  defaultSampler;

    // Threading
    uint32_t    inferenceThreads    = 0;  // 0 = auto-detect
    bool        backgroundInference = true;

    // KV Cache
    uint32_t    maxContextLen       = 4096;
    uint32_t    slidingWindow       = 0;
    bool        useSVDCompress      = false;

    // Win32 integration
    void*       targetHWND          = nullptr;  // HWND to receive WM_NATIVE_AI_*
    bool        postMessages        = true;

    // Performance
    bool        enableTelemetry     = true;
    float       targetTokensPerSec  = 0.0f; // 0 = no target
};

// ============================================================================
// Token stream entry — passed via WM_NATIVE_AI_TOKEN lParam
// ============================================================================
struct TokenStreamEntry {
    uint32_t    tokenId;
    char        text[64];
    uint32_t    textLen;
    float       latencyUs;
    uint32_t    seqPosition;
};

// ============================================================================
// NativeInferencePipeline — Main orchestrator for IDE integration
// ============================================================================
class NativeInferencePipeline {
public:
    NativeInferencePipeline();
    ~NativeInferencePipeline();

    NativeInferencePipeline(const NativeInferencePipeline&) = delete;
    NativeInferencePipeline& operator=(const NativeInferencePipeline&) = delete;

    /// Process-wide singleton for use from command handlers (e.g. auto_feature_registry).
    /// IDE may use its own instance via m_nativePipeline; this is for CLI/headless.
    static NativeInferencePipeline& instance() {
        static NativeInferencePipeline s;
        return s;
    }

    // ---- Lifecycle ----------------------------------------------------------

    /// Initialize the pipeline and all sub-systems
    PatchResult Init(const PipelineConfig& cfg);

    /// Shutdown everything
    PatchResult Shutdown();

    /// Current state
    PipelineState GetState() const { return m_state.load(std::memory_order_acquire); }

    /// Which inference backend is active (if any)
    InferenceBackend GetBackend() const { return m_backend.load(std::memory_order_acquire); }

    // ---- Model Management ---------------------------------------------------

    /// Load a GGUF model (blocking or async depending on config)
    PatchResult LoadModel(const char* ggufPath);

    /// Unload current model
    PatchResult UnloadModel();

    /// Hot-swap: unload current model, load new one
    PatchResult SwapModel(const char* newGgufPath);

    /// Is a model loaded and ready?
    bool IsModelReady() const;

    // ---- Inference ----------------------------------------------------------

    /// Run inference on a text prompt (returns immediately if async)
    PatchResult Infer(const char* prompt, uint32_t promptLen);

    /// Run inference with custom sampler config
    PatchResult InferWithConfig(const char* prompt, uint32_t promptLen,
                                const LocalAI::SamplerConfig& sampler);

    /// Stop current inference (if running)
    PatchResult StopInference();

    /// Wait for current inference to complete
    PatchResult WaitForCompletion(uint32_t timeoutMs = 0);

    /// Get the full generated text from last inference
    const char* GetLastOutput(uint32_t* outLen) const;

    // ---- Context Management -------------------------------------------------

    /// Clear the conversation context
    PatchResult ResetContext();

    /// Get current context length
    uint32_t ContextLength() const;

    // ---- Diagnostics --------------------------------------------------------

    /// Get the underlying LocalAICore
    /// Note: if the active backend is LlamaDll, LocalAICore may be initialized but unused.
    LocalAI::LocalAICore& GetCore() { return m_core; }

    /// Get performance diagnostics string
    void GetDiagnostics(char* buf, size_t bufLen) const;

    /// Get current tokens/sec
    float CurrentTokensPerSec() const;

    /// Get model info string
    void GetModelInfo(char* buf, size_t bufLen) const;

private:
    // ---- Internal -----------------------------------------------------------
    static bool TokenCallbackTrampoline(void* userData, uint32_t tokenId,
                                         const char* text, uint32_t textLen);
    static void LlamaTokenCallbackTrampoline(const char* piece, int len, void* userData);
    bool OnToken(uint32_t tokenId, const char* text, uint32_t textLen);
    void InferenceWorker(const char* prompt, uint32_t promptLen,
                         LocalAI::SamplerConfig sampler);
    void PostTokenMessage(const TokenStreamEntry& entry);

    // ---- State --------------------------------------------------------------
    std::atomic<PipelineState>  m_state{PipelineState::Idle};
    std::atomic<InferenceBackend> m_backend{InferenceBackend::None};
    PipelineConfig              m_config;
    LocalAI::LocalAICore        m_core;
    bool                        m_coreInitialized = false;
    void*                       m_llamaBridge = nullptr;

    // Worker thread
    std::thread                 m_workerThread;
    std::atomic<bool>           m_stopRequested{false};

    // Output accumulator
    char*                       m_outputBuf     = nullptr;
    uint32_t                    m_outputLen      = 0;
    uint32_t                    m_outputCapacity = 0;

    // Timing
    std::chrono::high_resolution_clock::time_point m_inferStart;
    std::atomic<uint32_t>       m_tokenCount{0};

    mutable std::mutex          m_mutex;
};

} // namespace RawrXD
