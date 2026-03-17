// ============================================================================
// local_ai_core.hpp — Local AI Core: Self-Contained Inference Engine
// ============================================================================
// Complete local inference engine that orchestrates the NativeSpeedLayer,
// FlashAttention, KV Cache, and token sampling into a full transformer
// forward-pass pipeline. Zero external dependencies (no llama.cpp, no
// ollama, no Python runtime).
//
// Capabilities:
//   - Full transformer decoder inference (LLaMA/Mistral/Phi architecture)
//   - Memory-mapped GGUF model loading with zero-copy tensor access
//   - Speculative decoding (draft model + verify)
//   - Token sampling: top-k, top-p (nucleus), temperature, repetition penalty
//   - Mirostat v2 adaptive sampling
//   - Streaming token output via callback (function pointer, no std::function)
//   - Multi-batch inference
//   - Asynchronous layer prefetching
//   - Thread-pool based parallel layer execution
//   - Ring-buffer telemetry for tokens/sec tracking
//
// Architecture:
//   LocalAICore owns:
//     - NativeSpeedLayer (compute dispatch)
//     - FlashAttentionEngine (attention)
//     - KVCache (key-value caching with sliding window)
//     - TokenSampler (sampling strategies)
//     - ModelConfig (parsed from GGUF metadata)
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include "native_speed_layer.hpp"   // NativeSpeedLayer, KVCache, etc.
#include "flash_attention.h"        // FlashAttentionEngine, FlashAttentionConfig
#include "model_memory_hotpatch.hpp" // PatchResult
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <atomic>
#include <chrono>
#include <thread>

namespace RawrXD {
namespace LocalAI {

// ============================================================================
// Forward declarations
// ============================================================================
class LocalAICore;

// ============================================================================
// Token callback — called for each generated token
// ============================================================================
// Returns: true to continue, false to stop generation
typedef bool (*TokenCallback)(void* userData, uint32_t tokenId,
                              const char* tokenText, uint32_t tokenLen);

// ============================================================================
// Model Architecture — supported transformer variants
// ============================================================================
enum class ModelArch : uint32_t {
    Unknown     = 0,
    LLaMA       = 1,    // LLaMA / LLaMA 2 / LLaMA 3
    Mistral     = 2,    // Mistral (sliding window attention)
    Phi         = 3,    // Microsoft Phi-2/3
    Gemma       = 4,    // Google Gemma
    Qwen        = 5,    // Alibaba Qwen
    CodeLlama   = 6,    // Code LLaMA (RoPE scaling)
    DeepSeek    = 7,    // DeepSeek
    StarCoder   = 8,    // StarCoder / StarCoder2
};

const char* ModelArchName(ModelArch arch);

// ============================================================================
// ModelConfig — Parsed from GGUF metadata
// ============================================================================
struct ModelConfig {
    // Architecture
    ModelArch   arch                = ModelArch::Unknown;
    char        archName[64]        = {};
    char        modelName[128]      = {};

    // Dimensions
    uint32_t    vocabSize           = 0;
    uint32_t    hiddenDim           = 0;     // Embedding dimension
    uint32_t    ffnDim              = 0;     // FFN intermediate dimension
    uint32_t    nLayers             = 0;     // Number of transformer layers
    uint32_t    nHeads              = 0;     // Number of attention heads
    uint32_t    nKVHeads            = 0;     // Number of KV heads (GQA)
    uint32_t    headDim             = 0;     // Dimension per head
    uint32_t    maxSeqLen           = 4096;  // Maximum context length

    // Normalization
    float       rmsNormEps          = 1e-5f;
    bool        preNorm             = true;  // Pre-normalization (LLaMA-style)

    // RoPE
    float       ropeTheta           = 10000.0f;
    float       ropeFreqScale       = 1.0f;  // For extended context
    uint32_t    ropeType            = 0;     // 0=standard, 1=NTK-aware

    // Activation
    uint32_t    activationType      = 0;     // 0=SiLU, 1=GeLU, 2=ReLU

    // Quantization
    NativeSpeed::QuantType  weightQuant  = NativeSpeed::QuantType::F16;
    NativeSpeed::QuantType  embedQuant   = NativeSpeed::QuantType::F16;

    // Tokenizer
    uint32_t    bosToken            = 1;
    uint32_t    eosToken            = 2;
    uint32_t    padToken            = 0;

    // Sliding window (Mistral)
    uint32_t    slidingWindow       = 0;     // 0 = disabled

    /// Compute derived values
    void ComputeDerived() {
        if (nHeads > 0 && hiddenDim > 0 && headDim == 0) {
            headDim = hiddenDim / nHeads;
        }
        if (nKVHeads == 0) nKVHeads = nHeads;  // Default: MHA
        if (ffnDim == 0) ffnDim = hiddenDim * 4; // Default 4x expansion
    }
};

// ============================================================================
// SamplerConfig — Token sampling parameters
// ============================================================================
struct SamplerConfig {
    float       temperature         = 0.8f;   // Scaling factor (0 = greedy)
    float       topP                = 0.9f;   // Nucleus sampling threshold
    uint32_t    topK                = 40;     // Top-K filtering
    float       repeatPenalty       = 1.1f;   // Repetition penalty
    uint32_t    repeatWindow        = 64;     // Window for repeat penalty
    float       presencePenalty     = 0.0f;   // Presence penalty
    float       frequencyPenalty    = 0.0f;   // Frequency penalty

    // Mirostat v2
    bool        useMirostat         = false;
    float       mirostatTau         = 5.0f;   // Target surprise
    float       mirostatEta         = 0.1f;   // Learning rate
    float       mirostatMu          = 0.0f;   // Current mu (internal)

    // Stopping
    uint32_t    maxTokens           = 2048;
    uint32_t    eosToken            = 2;
    bool        stopOnEos           = true;

    // Seed
    uint64_t    seed                = 0;     // 0 = random seed
};

// ============================================================================
// TokenSampler — Token selection from logits
// ============================================================================
class TokenSampler {
public:
    TokenSampler();
    ~TokenSampler() = default;

    /// Configure sampler parameters
    void Configure(const SamplerConfig& cfg);

    /// Sample a token from logits array of size vocabSize.
    /// Applies temperature → top-K → top-P → repetition penalty → sample.
    /// Returns the selected token ID.
    uint32_t Sample(float* logits, uint32_t vocabSize);

    /// Record a generated token (for repetition penalty tracking)
    void RecordToken(uint32_t tokenId);

    /// Reset history
    void Reset();

    /// Get current config
    const SamplerConfig& GetConfig() const { return m_config; }

private:
    SamplerConfig   m_config;
    uint32_t*       m_history           = nullptr;
    uint32_t        m_historyLen        = 0;
    uint32_t        m_historyCapacity   = 0;
    uint64_t        m_rngState[2]       = {};  // xoshiro128+ state

    // Internal
    uint64_t NextRandom();
    float    RandomFloat();  // [0, 1)
    void     ApplyTemperature(float* logits, uint32_t n);
    void     ApplyRepetitionPenalty(float* logits, uint32_t n);
    uint32_t TopKTopP(float* logits, uint32_t n);
    uint32_t MirostatV2(float* logits, uint32_t n);
};

// ============================================================================
// InferenceRequest — Parameters for a single inference call
// ============================================================================
struct InferenceRequest {
    // Input tokens
    const uint32_t* inputTokens     = nullptr;
    uint32_t        inputLen        = 0;

    // Sampling
    SamplerConfig   sampler;

    // Output callback
    TokenCallback   callback        = nullptr;
    void*           callbackData    = nullptr;

    // Batch ID (for multi-request scheduling)
    uint32_t        batchId         = 0;

    // Pre-fill only (no generation, just populate KV cache)
    bool            prefillOnly     = false;
};

// ============================================================================
// InferenceResult — Result of an inference call
// ============================================================================
struct InferenceResult {
    bool        success;
    const char* detail;
    int         errorCode;

    uint32_t*   outputTokens    = nullptr;  // Caller must free if non-null
    uint32_t    outputLen       = 0;
    uint32_t    promptTokens    = 0;        // Tokens in prompt
    float       prefillTimeMs   = 0.0f;     // Time to process prompt
    float       generateTimeMs  = 0.0f;     // Time to generate output
    float       tokensPerSec    = 0.0f;     // Generation speed

    static InferenceResult ok(const char* msg = "OK") {
        InferenceResult r;
        r.success = true;
        r.detail = msg;
        r.errorCode = 0;
        return r;
    }

    static InferenceResult error(const char* msg, int code = -1) {
        InferenceResult r;
        r.success = false;
        r.detail = msg;
        r.errorCode = code;
        return r;
    }
};

// ============================================================================
// Tokenizer — BPE tokenizer with GGUF vocabulary
// ============================================================================
class Tokenizer {
public:
    Tokenizer();
    ~Tokenizer();

    Tokenizer(const Tokenizer&) = delete;
    Tokenizer& operator=(const Tokenizer&) = delete;

    /// Load vocabulary from GGUF metadata (token strings + scores)
    PatchResult LoadFromGGUF(const void* ggufBase, uint64_t fileSize);

    /// Encode text to token IDs
    PatchResult Encode(const char* text, uint32_t textLen,
                       uint32_t* outTokens, uint32_t* outLen,
                       uint32_t maxTokens);

    /// Decode a single token ID to text
    const char* Decode(uint32_t tokenId, uint32_t* outLen) const;

    /// Vocabulary size
    uint32_t VocabSize() const { return m_vocabSize; }

    /// Get BOS/EOS token IDs
    uint32_t BosToken() const { return m_bosToken; }
    uint32_t EosToken() const { return m_eosToken; }

private:
    struct TokenEntry {
        const char* text;       // Points into mmap'd GGUF
        uint32_t    textLen;
        float       score;
        uint32_t    type;       // 0=normal, 1=unknown, 2=control, 3=user, 4=unused, 5=byte
    };

    TokenEntry* m_vocab         = nullptr;
    uint32_t    m_vocabSize     = 0;
    uint32_t    m_bosToken      = 1;
    uint32_t    m_eosToken      = 2;

    // BPE merge table (sorted by priority)
    struct MergeEntry {
        uint32_t    left;
        uint32_t    right;
        uint32_t    result;
        float       score;
    };

    MergeEntry* m_merges        = nullptr;
    uint32_t    m_mergeCount    = 0;
};

// ============================================================================
// LayerWeights — Pointers to a single transformer layer's tensors
// ============================================================================
// All pointers point into mmap'd GGUF — zero-copy.
struct LayerWeights {
    // Attention
    const void*     wq;         // Query projection [hiddenDim, hiddenDim]
    const void*     wk;         // Key projection   [hiddenDim, kvDim]
    const void*     wv;         // Value projection  [hiddenDim, kvDim]
    const void*     wo;         // Output projection [hiddenDim, hiddenDim]

    // FFN (gate/up/down for LLaMA-style)
    const void*     wGate;      // Gate projection   [hiddenDim, ffnDim]
    const void*     wUp;        // Up projection     [hiddenDim, ffnDim]
    const void*     wDown;      // Down projection   [ffnDim, hiddenDim]

    // Norms
    const float*    attnNorm;   // Attention layer norm weights
    const float*    ffnNorm;    // FFN layer norm weights

    // Quant types
    NativeSpeed::QuantType quantType;   // Quantization of weight matrices

    LayerWeights() { memset(this, 0, sizeof(*this)); }
};

// ============================================================================
// PerformanceCounters — Inference telemetry
// ============================================================================
struct PerformanceCounters {
    std::atomic<uint64_t>   totalPromptTokens{0};
    std::atomic<uint64_t>   totalGenTokens{0};
    std::atomic<uint64_t>   totalInferences{0};
    std::atomic<uint64_t>   totalPrefillTimeUs{0};
    std::atomic<uint64_t>   totalGenTimeUs{0};
    std::atomic<float>      peakTokensPerSec{0.0f};
    std::atomic<float>      avgTokensPerSec{0.0f};
    std::atomic<uint64_t>   cacheHits{0};
    std::atomic<uint64_t>   cacheMisses{0};

    void Reset() {
        totalPromptTokens.store(0);
        totalGenTokens.store(0);
        totalInferences.store(0);
        totalPrefillTimeUs.store(0);
        totalGenTimeUs.store(0);
        peakTokensPerSec.store(0.0f);
        avgTokensPerSec.store(0.0f);
        cacheHits.store(0);
        cacheMisses.store(0);
    }
};

// ============================================================================
// LocalAICore — Main inference engine
// ============================================================================
class LocalAICore {
public:
    LocalAICore();
    ~LocalAICore();

    // Non-copyable, non-movable
    LocalAICore(const LocalAICore&) = delete;
    LocalAICore& operator=(const LocalAICore&) = delete;

    // ---- Lifecycle ----------------------------------------------------------

    /// Initialize the engine (detect hardware, set up compute layer)
    PatchResult Init();

    /// Load a GGUF model file
    PatchResult LoadModel(const char* ggufPath);

    /// Unload the current model
    PatchResult UnloadModel();

    /// Full shutdown
    PatchResult Shutdown();

    /// Is the engine ready for inference?
    bool IsReady() const { return m_ready.load(std::memory_order_acquire); }

    /// Is a model loaded?
    bool IsModelLoaded() const { return m_modelLoaded.load(std::memory_order_acquire); }

    // ---- Inference ----------------------------------------------------------

    /// Run inference (blocking). Calls InferenceRequest::callback for each token.
    InferenceResult Infer(const InferenceRequest& req);

    /// Run inference on pre-tokenized input (lower-level)
    InferenceResult InferTokens(const uint32_t* tokens, uint32_t nTokens,
                                const SamplerConfig& sampler,
                                TokenCallback callback, void* userData);

    /// Run a text prompt through tokenization + inference
    InferenceResult InferText(const char* prompt, uint32_t promptLen,
                              const SamplerConfig& sampler,
                              TokenCallback callback, void* userData);

    // ---- Prefill / KV Cache -------------------------------------------------

    /// Prefill the KV cache with a token sequence (no generation)
    PatchResult Prefill(const uint32_t* tokens, uint32_t nTokens);

    /// Clear the KV cache and reset sequence position
    PatchResult ResetContext();

    /// Get current context length (tokens in KV cache)
    uint32_t ContextLength() const { return m_seqPos; }

    // ---- Configuration ------------------------------------------------------

    /// Get the parsed model configuration
    const ModelConfig& GetModelConfig() const { return m_modelConfig; }

    /// Set number of CPU threads for compute
    void SetThreadCount(uint32_t nThreads);

    /// Enable/disable speculative decoding
    void SetSpeculativeDecoding(bool enable, uint32_t draftAhead = 4);

    /// Set KV cache configuration
    PatchResult ConfigureKVCache(const NativeSpeed::KVCacheConfig& cfg);

    // ---- Diagnostics --------------------------------------------------------

    /// Get performance counters
    const PerformanceCounters& GetPerformance() const { return m_perf; }

    /// Get the native speed layer (for advanced use)
    NativeSpeed::NativeSpeedLayer& GetSpeedLayer() { return m_speed; }

    /// Get the tokenizer
    Tokenizer& GetTokenizer() { return m_tokenizer; }

    /// Human-readable status string
    void GetStatusString(char* buf, size_t bufLen) const;

private:
    // ---- Internal: Transformer Forward Pass ---------------------------------

    /// Run one transformer layer forward pass
    PatchResult ForwardLayer(uint32_t layerIdx, float* hidden, uint32_t seqLen,
                             uint32_t startPos, uint32_t deviceId = 0);

    /// Attention sub-layer
    PatchResult ForwardAttention(uint32_t layerIdx, float* hidden,
                                 uint32_t seqLen, uint32_t startPos);

    /// FFN sub-layer
    PatchResult ForwardFFN(uint32_t layerIdx, float* hidden,
                           uint32_t seqLen);

    /// Compute logits from final hidden state
    PatchResult ComputeLogits(const float* hidden, float* logits);

    // ---- Internal: Model Setup----------------------------------------------

    /// Parse model config from GGUF metadata
    PatchResult ParseModelConfig();

    /// Resolve tensor pointers for all layers
    PatchResult ResolveLayerWeights();

    /// Allocate scratch buffers for forward pass
    PatchResult AllocateScratchBuffers();

    /// Free scratch buffers
    void FreeScratchBuffers();

    // ---- Internal: Speculative Decoding ------------------------------------

    /// Generate draft tokens speculatively
    uint32_t GenerateDraft(float* hiddenState, uint32_t nDraft);

    /// Verify draft tokens against full model
    uint32_t VerifyDraft(const uint32_t* draft, uint32_t nDraft,
                         float* logits);

    // ---- State --------------------------------------------------------------

    std::atomic<bool>       m_ready{false};
    std::atomic<bool>       m_modelLoaded{false};

    // Sub-systems (owned)
    NativeSpeed::NativeSpeedLayer   m_speed;
    FlashAttentionEngine            m_flashAttn;
    NativeSpeed::KVCache            m_kvCache;
    TokenSampler                    m_sampler;
    Tokenizer                       m_tokenizer;

    // Model configuration
    ModelConfig             m_modelConfig;

    // Layer weights (resolved from GGUF tensors)
    LayerWeights*           m_layers        = nullptr;
    uint32_t                m_nLayers       = 0;

    // Embedding table pointer (into mmap)
    const void*             m_embedWeights  = nullptr;
    const float*            m_normWeights   = nullptr;  // Final RMS norm
    const void*             m_lmHead        = nullptr;   // Output projection

    // Scratch buffers
    float*                  m_hidden        = nullptr;  // [maxSeqLen × hiddenDim]
    float*                  m_hidden2       = nullptr;  // Second hidden buffer
    float*                  m_attnOut       = nullptr;  // Attention output
    float*                  m_ffnOut        = nullptr;  // FFN output
    float*                  m_logits        = nullptr;  // [vocabSize]
    float*                  m_qBuf          = nullptr;  // Query projection buffer
    float*                  m_kBuf          = nullptr;  // Key projection buffer
    float*                  m_vBuf          = nullptr;  // Value projection buffer

    // Sequence position
    uint32_t                m_seqPos        = 0;

    // Threading
    uint32_t                m_nThreads      = 4;

    // Speculative decoding
    bool                    m_specEnabled   = false;
    uint32_t                m_specDraftN    = 4;
    uint32_t*               m_draftTokens   = nullptr;

    // Performance
    PerformanceCounters     m_perf;

    // Thread safety
    mutable std::mutex      m_mutex;
};

} // namespace LocalAI
} // namespace RawrXD
