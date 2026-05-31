#pragma once
#include "prometheus_config.h"
#include "prometheus_kv_cache.h"
#include "prometheus_speculative_decoder.h"
#include "prometheus_vision_encoder.h"

#include "tokenizer/bpe_tokenizer.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace Prometheus {

// Forward declarations
class MoELayer;
class AttentionLayer;
class ToolExecutor;
class CodeSandbox;

// =============================================================================
// TOKEN TYPES
// =============================================================================

struct Token {
    uint32_t id = 0;
    float logprob = 0.0f;
    std::string text;
    bool isToolCall = false;
    bool isReasoning = false;
    bool isArtifact = false;
    bool isImage = false;
    bool isSpecial = false;
};

// =============================================================================
// REQUEST TYPES
// =============================================================================

struct ToolDefinition {
    std::string name;
    std::string description;
    std::string parameterSchema;
    bool requiresConfirmation = false;
    bool parallelSafe = true;
};

struct ToolCall {
    std::string id;
    std::string name;
    std::string arguments;
    enum class State { Pending, Running, Completed, Failed, Timeout };
    State state = State::Pending;
    std::string result;
    std::string error;
    std::chrono::milliseconds duration{0};
};

struct Artifact {
    std::string type;
    std::string language;
    std::string content;
    std::string id;
    bool executable = false;
};

struct ReasoningStep {
    std::string thought;
    std::string action;
    std::string observation;
    float confidence = 0.0f;
};

struct ContentPart {
    enum class Type { Text, Image, ToolCall, ToolResult, Artifact, Reasoning };
    Type type = Type::Text;
    std::string text;
    std::string imageUrl;
    std::string imageBase64;
    ToolCall toolCall;
    std::string toolResult;
    Artifact artifact;
    std::vector<ReasoningStep> reasoning;
};

struct Message {
    std::string role;
    std::vector<ContentPart> content;
    std::string toolCallId;
    std::string name;
    std::chrono::system_clock::time_point timestamp;
    uint64_t tokenCount = 0;
};

struct GenerationConfig {
    float temperature = 0.7f;
    float topP = 0.95f;
    float topK = 40.0f;
    float minP = 0.05f;
    float repetitionPenalty = 1.0f;
    float frequencyPenalty = 0.0f;
    float presencePenalty = 0.0f;
    std::vector<std::string> stopSequences;
    uint32_t maxTokens = 4096;
    InferenceFeature features = InferenceFeature::All;
    bool enableReasoning = true;
    bool enableArtifacts = true;
    bool enableToolCalls = true;
    bool enableVision = true;
    std::string responseFormat;
    std::string jsonSchema;
    std::vector<ToolDefinition> tools;
    bool parallelToolCalls = true;
    uint32_t maxToolCalls = 10;
    std::chrono::milliseconds toolTimeout{30000};
    bool speculativeDecoding = true;
    bool streaming = true;
    bool useCache = true;
    std::string cacheKey;
    bool allowRefusals = true;
    std::vector<std::string> allowedActions;
};

struct GenerationResult {
    std::string text;
    std::vector<Token> tokens;
    std::vector<ToolCall> toolCalls;
    std::vector<Artifact> artifacts;
    std::vector<ReasoningStep> reasoning;
    bool finished = false;
    std::string finishReason;
    uint32_t totalTokens = 0;
    uint32_t promptTokens = 0;
    uint32_t completionTokens = 0;
    uint32_t reasoningTokens = 0;
    double tokensPerSecond = 0;
    std::chrono::milliseconds totalTime{0};
    std::chrono::milliseconds firstTokenLatency{0};
    uint64_t cacheHits = 0;
    uint64_t speculativeAccepts = 0;
    uint64_t speculativeRejects = 0;
    bool wasRefused = false;
    std::string refusalReason;
};

// =============================================================================
// CALLBACK TYPES
// =============================================================================

using StreamCallback = std::function<void(const Token&)>;
using ToolCallback = std::function<void(const ToolCall&)>;
using ArtifactCallback = std::function<void(const Artifact&)>;
using ReasoningCallback = std::function<void(const ReasoningStep&)>;
using ErrorCallback = std::function<void(const std::string&)>;

// =============================================================================
// MAIN ENGINE CLASS
// =============================================================================

class PrometheusEngine {
public:
    static std::unique_ptr<PrometheusEngine> create(
        const std::string& modelPath,
        ModelConfig config = {}
    );
    virtual ~PrometheusEngine() = default;

    // Core generation
    GenerationResult generate(
        const std::vector<Message>& messages,
        GenerationConfig config = {}
    );
    void generateStream(
        const std::vector<Message>& messages,
        StreamCallback onToken,
        GenerationConfig config = {},
        std::function<void(GenerationResult)> onComplete = nullptr
    );
    GenerationResult generateWithTools(
        const std::vector<Message>& messages,
        const std::vector<ToolDefinition>& tools,
        GenerationConfig config = {}
    );
    GenerationResult generateWithReasoning(
        const std::vector<Message>& messages,
        GenerationConfig config = {}
    );
    GenerationResult generateStructured(
        const std::vector<Message>& messages,
        const std::string& jsonSchema,
        GenerationConfig config = {}
    );

    // Agentic loop
    GenerationResult runAgenticLoop(
        const std::vector<Message>& messages,
        const std::vector<ToolDefinition>& tools,
        std::function<std::string(const ToolCall&)> toolExecutor,
        GenerationConfig config = {},
        int maxIterations = 10
    );
    GenerationResult runWithCodeExecution(
        const std::vector<Message>& messages,
        GenerationConfig config = {}
    );

    // Multimodal
    std::vector<float> encodeImage(
        const std::string& imagePathOrBase64,
        const std::string& mimeType = ""
    );
    GenerationResult analyzeImage(
        const std::string& imagePathOrBase64,
        const std::string& prompt,
        GenerationConfig config = {}
    );

    // Prompt caching
    std::string cachePromptPrefix(
        const std::vector<Message>& messages,
        const std::string& cacheKey = ""
    );
    struct CacheStats {
        uint64_t totalEntries = 0;
        uint64_t totalBytes = 0;
        uint64_t hits = 0;
        uint64_t misses = 0;
        float hitRate = 0.0f;
    };
    CacheStats getCacheStats() const;
    void clearCache();

    // Configuration
    const ModelConfig& config() const { return config_; }
    ModelConfig& config() { return config_; }
    void setToolCallback(ToolCallback cb) { toolCallback_ = std::move(cb); }
    void setArtifactCallback(ArtifactCallback cb) { artifactCallback_ = std::move(cb); }
    void setReasoningCallback(ReasoningCallback cb) { reasoningCallback_ = std::move(cb); }
    void setErrorCallback(ErrorCallback cb) { errorCallback_ = std::move(cb); }

    // Batch inference
    std::vector<GenerationResult> generateBatch(
        const std::vector<std::vector<Message>>& batchMessages,
        GenerationConfig config = {}
    );
    struct BatchRequest {
        std::string id;
        std::vector<Message> messages;
        GenerationConfig config;
        StreamCallback callback;
    };
    void submitRequest(const BatchRequest& request);
    void cancelRequest(const std::string& id);
    size_t pendingRequestCount() const;

    // Inspection / debug
    std::vector<float> getLayerActivations(uint32_t layer, uint32_t position) const;
    std::vector<std::vector<float>> getAttentionWeights(uint32_t layer) const;
    std::vector<uint32_t> getExpertRouting(uint32_t layer) const;
    std::vector<uint32_t> tokenize(const std::string& text, bool addSpecial = true) const;
    std::string detokenize(const std::vector<uint32_t>& tokens, bool skipSpecial = true) const;

    // Tokenizer management
    bool loadTokenizer(const std::string& vocabPath, const std::string& mergesPath = "");
    bool loadTokenizerFromGGUF(const std::string& ggufPath);
    bool hasTokenizer() const { return tokenizer_ != nullptr; }

    // Low-level access
    std::vector<float> forward(
        const std::vector<uint32_t>& tokens,
        std::vector<std::vector<float>>* logits = nullptr
    );
    KVCache& kvCache();
    SpeculativeDecoder& speculativeDecoder();

protected:
    PrometheusEngine() = default;
    ModelConfig config_;
    std::unique_ptr<KVCache> kvCache_;
    std::unique_ptr<SpeculativeDecoder> specDecoder_;
    std::unique_ptr<VisionEncoder> visionEncoder_;
    std::unique_ptr<CodeSandbox> codeSandbox_;
    std::unique_ptr<tokenizer::BPETokenizer> tokenizer_;
    ToolCallback toolCallback_;
    ArtifactCallback artifactCallback_;
    ReasoningCallback reasoningCallback_;
    ErrorCallback errorCallback_;
    std::unordered_map<std::string, std::vector<uint32_t>> promptCache_;
};

} // namespace Prometheus
