// ============================================================================
// dml_inference_engine.h — DirectML Inference Engine (Server-Compatible)
// ============================================================================
// Drop-in GPU inference engine that provides the same Tokenize/Generate/
// Detokenize interface as CPUInferenceEngine, but runs the forward pass
// through DirectMLCompute (DML operators on AMD GPU).
//
// This enables the CompletionServer and APIServer to use DirectML for
// inference simply by swapping the engine pointer:
//
//   CPUInferenceEngine cpuEngine;     // Old: CPU-only
//   DMLInferenceEngine dmlEngine;     // New: GPU via DirectML
//   server.Start(port, &dmlEngine);   // Same API
//
// Architecture:
//   CompletionServer / APIServer
//     └──> DMLInferenceEngine (this)
//            ├──> GGUFDMLBridge (GGUF file → GPU tensors)
//            ├──> DirectMLCompute (DML ops: GEMM, MHA, RMSNorm, etc.)
//            └──> Tokenizer (BPE from GGUF vocab)
//
// Supports:
//   - Single & dual model loading
//   - Streaming token generation (SSE-compatible)
//   - Temperature, top-k, top-p sampling
//   - KV cache for multi-turn conversation
//   - Thread-safe concurrent inference requests
//   - AIBackendManager registration as "DirectML" backend
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <memory>

namespace RawrXD {

// Forward declarations
namespace DML {
    class DirectMLCompute;
    class GGUFDMLBridge;
    struct DMLResult;
    struct DMLTensorBuffer;
}

// ============================================================================
// Sampling Parameters
// ============================================================================
struct SamplingParams {
    float       temperature     = 0.7f;
    float       topP            = 0.95f;
    uint32_t    topK            = 40;
    float       repeatPenalty   = 1.1f;
    uint32_t    repeatWindow    = 64;
    uint64_t    seed            = 0;      // 0 = random
    bool        greedy          = false;   // Override: always pick top token
};

// ============================================================================
// Generation Stats (per request)
// ============================================================================
struct GenerationStats {
    uint32_t    promptTokens    = 0;
    uint32_t    generatedTokens = 0;
    double      promptMs        = 0.0;     // Time processing prompt
    double      generationMs    = 0.0;     // Time generating tokens
    double      tokensPerSec    = 0.0;     // Generation throughput
    double      totalMs         = 0.0;
};

// ============================================================================
// DMLInferenceEngine — GPU Inference with Server-Compatible Interface
// ============================================================================
class DMLInferenceEngine {
public:
    DMLInferenceEngine();
    ~DMLInferenceEngine();

    // ===== Model Loading =====

    // Load a GGUF model for inference (initializes DML if needed)
    bool LoadModel(const std::string& modelPath);

    // Load a second model (dual-model mode, 50/50 VRAM split)
    bool LoadSecondModel(const std::string& modelPath);

    // Unload current model(s)
    void UnloadModel();

    // Check if model is loaded and ready
    bool IsModelLoaded() const { return m_modelLoaded.load(); }

    // ===== Core Inference (matches CPUInferenceEngine API) =====

    // Tokenize text to token IDs
    std::vector<int32_t> Tokenize(const std::string& text);

    // Detokenize token IDs back to text
    std::string Detokenize(const std::vector<int32_t>& tokens);

    // Generate tokens (blocking, returns all generated token IDs)
    std::vector<int32_t> Generate(const std::vector<int32_t>& inputTokens,
                                   int maxTokens = 100);

    // Generate with logit output (for advanced sampling)
    std::vector<float> Eval(const std::vector<int32_t>& inputTokens);

    // ===== Streaming Generation =====

    // Generate tokens with per-token callbacks (SSE-compatible)
    void GenerateStreaming(const std::vector<int32_t>& inputTokens,
                           int maxTokens,
                           std::function<void(const std::string&)> tokenCallback,
                           std::function<void()> completeCallback,
                           std::function<void(int32_t)> tokenIdCallback = nullptr);

    // ===== Sampling =====

    // Set sampling parameters for generation
    void SetSamplingParams(const SamplingParams& params) { m_samplingParams = params; }
    const SamplingParams& GetSamplingParams() const { return m_samplingParams; }

    // ===== Model Information =====

    int GetVocabSize() const { return m_vocabSize; }
    int GetEmbeddingDim() const { return m_embeddingDim; }
    int GetNumLayers() const { return m_numLayers; }
    int GetNumHeads() const { return m_numHeads; }
    std::string GetArchitecture() const { return m_architecture; }
    std::string GetModelPath() const { return m_modelPath; }

    // ===== Mode Settings (compatibility with CPUInferenceEngine) =====

    void SetMaxMode(bool enabled) { m_maxMode = enabled; }
    void SetDeepThinking(bool enabled) { m_deepThinking = enabled; }
    void SetDeepResearch(bool enabled) { m_deepResearch = enabled; }
    bool IsMaxMode() const { return m_maxMode; }
    bool IsDeepThinking() const { return m_deepThinking; }
    bool IsDeepResearch() const { return m_deepResearch; }

    // ===== Performance =====

    void SetThreadCount(int threads) { m_threadCount = threads; }
    int GetThreadCount() const { return m_threadCount; }
    size_t GetMemoryUsage() const;
    void ClearCache();

    // ===== Context Management =====

    void SetContextSize(size_t size) { m_contextSize = size; }
    size_t GetContextSize() const { return m_contextSize; }

    // ===== Statistics =====

    GenerationStats GetLastStats() const { return m_lastStats; }

    // ===== Backend Registration =====

    // Register as "directml" backend in AIBackendManager
    void RegisterAsBackend(class AIBackendManager* mgr);

    // ===== Diagnostics =====

    std::string GetDiagnostics() const;

private:
    // ===== Tokenizer (BPE from GGUF) =====

    struct BPETokenizer {
        std::vector<std::string>                    vocab;          // id → token string
        std::unordered_map<std::string, int32_t>    tokenToId;      // string → id
        std::vector<float>                          scores;         // merge priority scores
        int32_t                                     bosToken = 1;   // Beginning of sequence
        int32_t                                     eosToken = 2;   // End of sequence
        int32_t                                     padToken = 0;
        int32_t                                     unkToken = 0;
        bool                                        loaded = false;
    };

    // Load tokenizer from GGUF metadata
    bool loadTokenizer(const std::string& ggufPath);

    // BPE encode
    std::vector<int32_t> bpeEncode(const std::string& text);

    // BPE decode
    std::string bpeDecode(const std::vector<int32_t>& tokens);

    // ===== Sampling =====

    // Sample a token from logits using current sampling params
    int32_t sampleToken(float* logits, int vocabSize,
                         const std::vector<int32_t>& prevTokens);

    // Top-k filtering
    void topKFilter(float* logits, int vocabSize, int k);

    // Top-p (nucleus) filtering
    void topPFilter(float* logits, int vocabSize, float p);

    // Temperature scaling
    void applyTemperature(float* logits, int vocabSize, float temp);

    // Repetition penalty
    void applyRepetitionPenalty(float* logits, int vocabSize,
                                const std::vector<int32_t>& prevTokens,
                                float penalty, uint32_t window);

    // Softmax for probabilities
    void softmax(float* logits, int vocabSize);

    // ===== Internal State =====

    // DML engine references (non-owning, from singletons)
    DML::DirectMLCompute*       m_dmlCompute    = nullptr;
    DML::GGUFDMLBridge*         m_ggufBridge    = nullptr;

    // Model info
    std::atomic<bool>           m_modelLoaded{false};
    std::string                 m_modelPath;
    std::string                 m_architecture;
    uint32_t                    m_sessionId      = 200;  // DML session ID
    int                         m_vocabSize      = 0;
    int                         m_embeddingDim   = 0;
    int                         m_numLayers      = 0;
    int                         m_numHeads       = 0;
    int                         m_numKVHeads     = 0;
    int                         m_maxSeqLen      = 4096;

    // Tokenizer
    BPETokenizer                m_tokenizer;

    // Sampling
    SamplingParams              m_samplingParams;

    // Context
    size_t                      m_contextSize    = 4096;
    int                         m_threadCount    = 8;

    // Mode flags
    bool                        m_maxMode        = false;
    bool                        m_deepThinking   = false;
    bool                        m_deepResearch   = false;

    // Stats
    GenerationStats             m_lastStats;

    // Logits buffer (reused across calls)
    std::vector<float>          m_logitsBuffer;

    // Thread safety
    mutable std::mutex          m_mutex;
};

} // namespace RawrXD
