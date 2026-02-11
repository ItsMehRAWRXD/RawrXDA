// ============================================================================
// model_cascade.h — Model Router Intelligence
// ============================================================================
// Multi-armed bandit model selection with Thompson sampling,
// circuit breakers, token estimation, and cascading fallback.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <functional>
#include <chrono>
#include <random>

namespace RawrXD {
namespace Routing {

// ============================================================================
// Quantization Format
// ============================================================================

enum class QuantizationFormat : uint8_t {
    NONE = 0,
    Q4_0, Q4_1, Q4_K_S, Q4_K_M,
    Q5_0, Q5_1, Q5_K_S, Q5_K_M,
    Q6_K, Q8_0,
    F16, F32,
    IQ2_XXS, IQ2_XS, IQ3_XXS, IQ3_S,
};

const char* quantFormatName(QuantizationFormat fmt);

// ============================================================================
// Task Type
// ============================================================================

enum class TaskType : uint8_t {
    CODE_COMPLETION = 0,
    CHAT,
    REFACTOR,
    EXPLAIN,
    COMMIT_MSG,
    CODE_REVIEW,
    BUG_FIX,
    DOCUMENTATION,
    TEST_GENERATION,
    TRANSLATION,
};

const char* taskTypeName(TaskType type);

// ============================================================================
// Model Capability — describes what a model can do
// ============================================================================

struct ModelCapability {
    std::string modelId;            // e.g., "phi-3-mini", "llama-3-8b"
    std::string endpoint;           // e.g., "http://localhost:11434"
    std::string provider;           // "ollama", "anthropic", "openai", "local"

    // Performance metrics (updated from telemetry)
    float latencyP50 = 0.0f;       // Milliseconds
    float latencyP95 = 0.0f;
    float latencyP99 = 0.0f;
    float tokensPerSecond = 0.0f;

    // Context window
    size_t contextWindow = 4096;
    size_t maxOutputTokens = 2048;

    // Format
    QuantizationFormat format = QuantizationFormat::NONE;
    bool supportsTools = false;
    bool supportsFIM = false;
    bool supportsStreaming = true;

    // Resource requirements
    size_t vramMB = 0;
    size_t ramMB = 0;

    // Quality scoring (from acceptance tracking)
    float qualityScore = 0.5f;     // 0.0 - 1.0
    float acceptanceRate = 0.5f;   // Tab acceptance rate

    // Task affinities (per-task quality scores)
    std::unordered_map<uint8_t, float> taskAffinities;

    float getTaskAffinity(TaskType type) const {
        auto it = taskAffinities.find(static_cast<uint8_t>(type));
        return (it != taskAffinities.end()) ? it->second : qualityScore;
    }
};

// ============================================================================
// Routing Decision — the router's output
// ============================================================================

struct RoutingDecision {
    std::string modelId;
    float confidence;              // 0.0 - 1.0
    std::string fallbackChain;     // "local-phi3 -> ollama-llama3 -> anthropic-claude"
    TaskType taskType;
    int estimatedTokens;           // Prompt token estimate
    const char* reason;            // Why this model was selected
};

struct RoutingResult {
    bool success;
    const char* detail;
    int errorCode;
    RoutingDecision decision;

    static RoutingResult ok(RoutingDecision dec) {
        return {true, "Routed", 0, std::move(dec)};
    }
    static RoutingResult error(const char* msg, int code = -1) {
        return {false, msg, code, {}};
    }
};

// ============================================================================
// Circuit Breaker — disable model after consecutive failures
// ============================================================================

class CircuitBreaker {
public:
    explicit CircuitBreaker(int failureThreshold = 3,
                             int recoveryTimeoutMs = 30000);

    void recordSuccess();
    void recordFailure();
    bool isOpen() const;    // true = model disabled
    void reset();

    int consecutiveFailures() const { return m_failures; }

private:
    int m_failureThreshold;
    int m_recoveryTimeoutMs;
    std::atomic<int> m_failures{0};
    std::chrono::steady_clock::time_point m_lastFailure;
    mutable std::mutex m_mutex;
};

// ============================================================================
// Latency Histogram — per-model P50/P95/P99 tracking
// ============================================================================

class LatencyHistogram {
public:
    explicit LatencyHistogram(size_t maxSamples = 1000);

    void recordLatency(float ms);
    float percentile(float p) const;  // p in [0, 1]
    float p50() const { return percentile(0.50f); }
    float p95() const { return percentile(0.95f); }
    float p99() const { return percentile(0.99f); }
    float mean() const;
    size_t sampleCount() const;
    void clear();

private:
    std::vector<float> m_samples;
    size_t m_maxSamples;
    mutable std::mutex m_mutex;
    mutable bool m_sorted = false;
    mutable std::vector<float> m_sortedCache;
};

// ============================================================================
// Token Estimator — fast heuristic prompt token count
// ============================================================================

class TokenEstimator {
public:
    // Estimate tokens for text (heuristic: ~4 chars/token for English code)
    static int estimate(const std::string& text);

    // More accurate estimate with language-specific rules
    static int estimateCode(const std::string& code, const std::string& language);

    // Check if prompt fits within model context window
    static bool fitsContext(const std::string& prompt, size_t contextWindow);
};

// ============================================================================
// Thompson Sampling — multi-armed bandit model selection
// ============================================================================

class ThompsonSampler {
public:
    ThompsonSampler();

    // Register a model arm
    void addArm(const std::string& modelId);

    // Record reward (0 = bad, 1 = good)
    void recordReward(const std::string& modelId, float reward);

    // Sample: returns the model to try
    std::string sample() const;

    // Get current alpha/beta for a model
    std::pair<float, float> getParams(const std::string& modelId) const;

private:
    struct ArmState {
        float alpha = 1.0f;  // Prior successes + 1
        float beta = 1.0f;   // Prior failures + 1
    };

    std::unordered_map<std::string, ArmState> m_arms;
    mutable std::mt19937 m_rng;
    mutable std::mutex m_mutex;
};

// ============================================================================
// ModelCascadeRouter — main router
// ============================================================================

class ModelCascadeRouter {
public:
    ModelCascadeRouter();
    ~ModelCascadeRouter();

    // Non-copyable
    ModelCascadeRouter(const ModelCascadeRouter&) = delete;
    ModelCascadeRouter& operator=(const ModelCascadeRouter&) = delete;

    // ---- Model Registration ----
    void registerModel(const ModelCapability& model);
    void removeModel(const std::string& modelId);
    const ModelCapability* getModel(const std::string& modelId) const;
    std::vector<std::string> availableModels() const;

    // ---- Routing ----
    RoutingResult route(TaskType task, const std::string& prompt);
    RoutingResult routeWithPreference(TaskType task, const std::string& prompt,
                                       const std::string& preferredModel);

    // ---- Fallback Chain ----
    void setFallbackChain(const std::vector<std::string>& chain);
    std::string getNextFallback(const std::string& failedModel) const;

    // ---- Telemetry Integration ----
    void recordLatency(const std::string& modelId, float latencyMs);
    void recordSuccess(const std::string& modelId, TaskType task);
    void recordFailure(const std::string& modelId, TaskType task);
    void recordAcceptance(const std::string& modelId, bool accepted);

    // ---- Circuit Breakers ----
    bool isModelAvailable(const std::string& modelId) const;

    // ---- Stats ----
    const LatencyHistogram* getLatencyHistogram(const std::string& modelId) const;
    float getModelAcceptanceRate(const std::string& modelId) const;

    // ---- Singleton ----
    static ModelCascadeRouter& Global();

private:
    RoutingDecision selectModel(TaskType task, int estimatedTokens) const;

    std::unordered_map<std::string, ModelCapability> m_models;
    std::unordered_map<std::string, std::unique_ptr<CircuitBreaker>> m_breakers;
    std::unordered_map<std::string, std::unique_ptr<LatencyHistogram>> m_histograms;
    std::vector<std::string> m_fallbackChain;
    ThompsonSampler m_sampler;

    // Acceptance tracking
    std::unordered_map<std::string, std::pair<uint64_t, uint64_t>> m_acceptCounts; // accepted, total

    mutable std::mutex m_mutex;
};

} // namespace Routing
} // namespace RawrXD
