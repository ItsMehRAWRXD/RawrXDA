#pragma once
#include <cstdint>
#include <string>

namespace Prometheus {

// =============================================================================
// MODEL CONFIGURATION (120B MoE)
// =============================================================================

struct ModelConfig {
    // ========= ARCHITECTURE =========
    uint32_t vocabSize = 256000;      // 256K vocab (multilingual + code)
    uint32_t hiddenDim = 12288;       // 12k hidden
    uint32_t numLayers = 14;           // 14 transformer layers (~120B total)
    uint32_t numHeads = 96;            // 96 attention heads
    uint32_t numKVHeads = 12;          // 12 KV heads (GQA 8:1)
    uint32_t headDim = 128;            // 128 per head
    uint32_t intermediateDim = 32768; // 2.67x hidden (MoE FFN)
    uint32_t maxPosition = 262144;     // 256K context (extended RoPE)

    // ========= MOE =========
    uint32_t numExperts = 16;          // 16 experts
    uint32_t expertsPerToken = 8;      // 8 active per token
    uint32_t sharedExperts = 2;        // 2 always-active shared experts
    float    expertDropout = 0.0f;     // No dropout at inference
    float    loadBalanceFactor = 0.01f;  // Aux loss coefficient

    // ========= QUANTIZATION =========
    uint32_t weightBits = 4;          // 4-bit weights (Q4_K_M)
    uint32_t kvCacheBits = 4;         // 4-bit KV cache
    bool     useBlockwiseQuant = true;
    uint32_t quantBlockSize = 256;

    // ========= ATTENTION =========
    uint32_t slidingWindow = 4096;    // Local attention window
    uint32_t globalStride = 8;        // Global attention every 8th token
    bool     useRingAttention = true; // For 1M+ context
    bool     useFlashAttention = true;
    float    attentionDropout = 0.0f;
    float    ropeTheta = 10000.0f;   // RoPE base frequency
    float    ropeScaleFactor = 1.0f; // RoPE scaling factor

    // ========= SPECIAL TOKENS =========
    uint32_t padToken = 0;
    uint32_t eosToken = 1;
    uint32_t bosToken = 2;
    uint32_t unkToken = 3;
    uint32_t maskToken = 4;
    uint32_t reasoningStart = 256003;   // <reasoning>
    uint32_t reasoningEnd = 256004;     // </reasoning>
    uint32_t artifactStart = 256005;    // <artifact>
    uint32_t artifactEnd = 256006;      // </artifact>
    uint32_t thoughtStart = 256007;     // <thought>
    uint32_t thoughtEnd = 256008;       // </thought>
    uint32_t imageStart = 256009;       // <image>
    uint32_t imageEnd = 256010;         // </image>

    // ========= INFERENCE =========
    bool enableSpeculativeDecoding = true;
    uint32_t speculativeTokens = 8;
    uint32_t draftModelDim = 2048;      // Small draft model
    bool enableContinuousBatching = true;
    uint32_t maxBatchSize = 256;

    // ========= MULTIMODAL =========
    bool enableVision = true;
    uint32_t visionPatchSize = 14;
    uint32_t visionDim = 1024;
    uint32_t visionLayers = 24;
    std::string visionEncoder = "siglip";  // SigLIP or CLIP variant

    // ========= CAPABILITIES =========
    bool enableToolCalling = true;
    bool enableCodeExecution = true;
    bool enableStructuredOutput = true;
    bool enableArtifacts = true;

    // ========= SAFETY =========
    bool enableRefusalTokens = true;
    bool enableConstitutionalAI = true;

    // Derived constants
    uint64_t totalParams() const {
        uint64_t embed = (uint64_t)vocabSize * hiddenDim;
        uint64_t attnQKV = (uint64_t)hiddenDim * (numHeads * headDim * 3);
        uint64_t attnOut = (uint64_t)numHeads * headDim * hiddenDim;
        uint64_t attn = attnQKV + attnOut;
        uint64_t gate = (uint64_t)hiddenDim * numExperts;
        uint64_t expertUp = (uint64_t)hiddenDim * intermediateDim * numExperts;
        uint64_t expertDown = (uint64_t)intermediateDim * hiddenDim * numExperts;
        uint64_t sharedFFN = 2ULL * hiddenDim * intermediateDim * sharedExperts;
        uint64_t ffn = gate + expertUp + expertDown + sharedFFN;
        uint64_t norms = (uint64_t)hiddenDim * 2 * numLayers;
        uint64_t layer = attn + ffn + norms;
        return embed + (layer * numLayers) + ((uint64_t)hiddenDim * vocabSize);
    }

    uint64_t activeParamsPerToken() const {
        uint64_t embed = (uint64_t)vocabSize * hiddenDim;
        uint64_t attnQKV = (uint64_t)hiddenDim * (numHeads * headDim * 3);
        uint64_t attnOut = (uint64_t)numHeads * headDim * hiddenDim;
        uint64_t activeExpertFFN = (uint64_t)hiddenDim * intermediateDim * 2 * (expertsPerToken + sharedExperts);
        uint64_t layer = attnQKV + attnOut + activeExpertFFN + (uint64_t)hiddenDim * 2;
        return embed + (layer * numLayers);
    }

    uint64_t estimateVRAM(bool withKVCache = true) const {
        uint64_t weights = totalParams() / 2;  // 4-bit = 0.5 bytes
        uint64_t kvCache = 0;
        if (withKVCache) {
            kvCache = 2ULL * numLayers * numKVHeads * headDim * maxPosition * (kvCacheBits / 8);
        }
        uint64_t activations = (uint64_t)hiddenDim * numLayers * 4;
        return weights + kvCache + activations;
    }
};

// =============================================================================
// SPECIAL TOKEN DEFINITIONS
// =============================================================================

namespace SpecialTokens {
constexpr const char* TOOL_CALL_START = "<|tool_call|>";
constexpr const char* TOOL_CALL_END = "</|tool_call|>";
constexpr const char* TOOL_RESULT_START = "<|tool_result|>";
constexpr const char* TOOL_RESULT_END = "</|tool_result|>";
constexpr const char* REASONING_START = "<|reasoning|>";
constexpr const char* REASONING_END = "</|reasoning|>";
constexpr const char* THOUGHT_START = "<|thought|>";
constexpr const char* THOUGHT_END = "</|thought|>";
constexpr const char* ARTIFACT_START = "<|artifact|>";
constexpr const char* ARTIFACT_END = "</|artifact|>";
constexpr const char* CODE_START = "<|code|>";
constexpr const char* CODE_END = "</|code|>";
constexpr const char* JSON_START = "<|json|>";
constexpr const char* JSON_END = "</|json|>";
constexpr const char* IMAGE_START = "<|image|>";
constexpr const char* IMAGE_END = "</|image|>";
constexpr const char* AUDIO_START = "<|audio|>";
constexpr const char* AUDIO_END = "</|audio|>";
constexpr const char* REFUSAL_START = "<|refusal|>";
constexpr const char* REFUSAL_END = "</|refusal|>";
constexpr const char* EOT = "<|eot|>";
constexpr const char* END_TURN = "<|end_turn|>";
} // namespace SpecialTokens

// =============================================================================
// INFERENCE FEATURES
// =============================================================================

enum class InferenceFeature : uint32_t {
    None = 0,
    ToolCalling = 1 << 0,
    ParallelTools = 1 << 1,
    StructuredOutput = 1 << 2,
    JSONMode = 1 << 3,
    CodeExecution = 1 << 4,
    Artifacts = 1 << 5,
    ExtendedThinking = 1 << 6,
    VisionInput = 1 << 7,
    AudioInput = 1 << 8,
    VideoInput = 1 << 9,
    Caching = 1 << 10,
    Streaming = 1 << 11,
    FunctionCalling = 1 << 12,
    All = 0xFFFFFFFF
};

inline InferenceFeature operator|(InferenceFeature a, InferenceFeature b) {
    return static_cast<InferenceFeature>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool hasFeature(InferenceFeature features, InferenceFeature feature) {
    return (static_cast<uint32_t>(features) & static_cast<uint32_t>(feature)) != 0;
}

} // namespace Prometheus
