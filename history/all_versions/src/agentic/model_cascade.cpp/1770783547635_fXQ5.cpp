// ============================================================================
// model_cascade.cpp — Model Router Intelligence Implementation
// ============================================================================
// Thompson sampling, circuit breakers, latency histograms, token estimation,
// and cascading fallback chain execution.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "agentic/model_cascade.h"

#include <algorithm>
#include <numeric>
#include <cmath>
#include <cstring>
#include <sstream>

namespace RawrXD {
namespace Routing {

// ============================================================================
// Names
// ============================================================================

const char* quantFormatName(QuantizationFormat fmt) {
    switch (fmt) {
        case QuantizationFormat::NONE:    return "none";
        case QuantizationFormat::Q4_0:    return "Q4_0";
        case QuantizationFormat::Q4_1:    return "Q4_1";
        case QuantizationFormat::Q4_K_S:  return "Q4_K_S";
        case QuantizationFormat::Q4_K_M:  return "Q4_K_M";
        case QuantizationFormat::Q5_0:    return "Q5_0";
        case QuantizationFormat::Q5_1:    return "Q5_1";
        case QuantizationFormat::Q5_K_S:  return "Q5_K_S";
        case QuantizationFormat::Q5_K_M:  return "Q5_K_M";
        case QuantizationFormat::Q6_K:    return "Q6_K";
        case QuantizationFormat::Q8_0:    return "Q8_0";
        case QuantizationFormat::F16:     return "F16";
        case QuantizationFormat::F32:     return "F32";
        case QuantizationFormat::IQ2_XXS: return "IQ2_XXS";
        case QuantizationFormat::IQ2_XS:  return "IQ2_XS";
        case QuantizationFormat::IQ3_XXS: return "IQ3_XXS";
        case QuantizationFormat::IQ3_S:   return "IQ3_S";
        default: return "unknown";
    }
}

const char* taskTypeName(TaskType type) {
    switch (type) {
        case TaskType::CODE_COMPLETION:  return "code_completion";
        case TaskType::CHAT:             return "chat";
        case TaskType::REFACTOR:         return "refactor";
        case TaskType::EXPLAIN:          return "explain";
        case TaskType::COMMIT_MSG:       return "commit_msg";
        case TaskType::CODE_REVIEW:      return "code_review";
        case TaskType::BUG_FIX:          return "bug_fix";
        case TaskType::DOCUMENTATION:    return "documentation";
        case TaskType::TEST_GENERATION:  return "test_generation";
        case TaskType::TRANSLATION:      return "translation";
        default: return "unknown";
    }
}

// ============================================================================
// Circuit Breaker
// ============================================================================

CircuitBreaker::CircuitBreaker(int failureThreshold, int recoveryTimeoutMs)
    : m_failureThreshold(failureThreshold)
    , m_recoveryTimeoutMs(recoveryTimeoutMs) {}

void CircuitBreaker::recordSuccess() {
    m_failures.store(0, std::memory_order_relaxed);
}

void CircuitBreaker::recordFailure() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_failures.fetch_add(1, std::memory_order_relaxed);
    m_lastFailure = std::chrono::steady_clock::now();
}

bool CircuitBreaker::isOpen() const {
    int f = m_failures.load(std::memory_order_relaxed);
    if (f < m_failureThreshold) return false;

    // Check if recovery timeout has elapsed
    std::lock_guard<std::mutex> lock(m_mutex);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - m_lastFailure).count();
    return elapsed < m_recoveryTimeoutMs;
}

void CircuitBreaker::reset() {
    m_failures.store(0, std::memory_order_relaxed);
}

// ============================================================================
// Latency Histogram
// ============================================================================

LatencyHistogram::LatencyHistogram(size_t maxSamples)
    : m_maxSamples(maxSamples) {
    m_samples.reserve(maxSamples);
}

void LatencyHistogram::recordLatency(float ms) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sorted = false;

    if (m_samples.size() >= m_maxSamples) {
        // Evict oldest (shift left)
        m_samples.erase(m_samples.begin());
    }
    m_samples.push_back(ms);
}

float LatencyHistogram::percentile(float p) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_samples.empty()) return 0.0f;

    if (!m_sorted) {
        m_sortedCache = m_samples;
        std::sort(m_sortedCache.begin(), m_sortedCache.end());
        m_sorted = true;
    }

    size_t idx = static_cast<size_t>(p * (m_sortedCache.size() - 1));
    return m_sortedCache[idx];
}

float LatencyHistogram::mean() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_samples.empty()) return 0.0f;
    float sum = std::accumulate(m_samples.begin(), m_samples.end(), 0.0f);
    return sum / static_cast<float>(m_samples.size());
}

size_t LatencyHistogram::sampleCount() const {
    return m_samples.size();
}

void LatencyHistogram::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_samples.clear();
    m_sortedCache.clear();
    m_sorted = false;
}

// ============================================================================
// Token Estimator
// ============================================================================

int TokenEstimator::estimate(const std::string& text) {
    // Heuristic: ~4 characters per token for English/code
    // More accurate: count whitespace-separated tokens and punctuation
    if (text.empty()) return 0;

    int tokens = 0;
    bool inWord = false;

    for (char c : text) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            if (!inWord) {
                ++tokens;
                inWord = true;
            }
        } else {
            inWord = false;
            // Punctuation/operators count as tokens
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                ++tokens;
            }
        }
    }

    // Adjust: subword tokenization typically yields ~1.3x more tokens
    return static_cast<int>(tokens * 1.3f);
}

int TokenEstimator::estimateCode(const std::string& code, const std::string& language) {
    int base = estimate(code);

    // Language-specific adjustments
    if (language == "python") {
        return static_cast<int>(base * 0.9f);  // Python is more token-efficient
    } else if (language == "cpp" || language == "c") {
        return static_cast<int>(base * 1.1f);  // C++ has more syntax tokens
    } else if (language == "asm" || language == "masm") {
        return static_cast<int>(base * 1.2f);  // ASM mnemonics → many tokens
    }

    return base;
}

bool TokenEstimator::fitsContext(const std::string& prompt, size_t contextWindow) {
    return static_cast<size_t>(estimate(prompt)) < contextWindow;
}

// ============================================================================
// Thompson Sampling
// ============================================================================

ThompsonSampler::ThompsonSampler()
    : m_rng(std::random_device{}()) {}

void ThompsonSampler::addArm(const std::string& modelId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_arms[modelId] = {1.0f, 1.0f};
}

void ThompsonSampler::recordReward(const std::string& modelId, float reward) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_arms.find(modelId);
    if (it == m_arms.end()) return;

    // Beta-Bernoulli update
    if (reward > 0.5f) {
        it->second.alpha += reward;
    } else {
        it->second.beta += (1.0f - reward);
    }
}

std::string ThompsonSampler::sample() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_arms.empty()) return "";

    std::string bestArm;
    float bestSample = -1.0f;

    for (const auto& [id, state] : m_arms) {
        // Sample from Beta(alpha, beta)
        std::gamma_distribution<float> gammaAlpha(state.alpha, 1.0f);
        std::gamma_distribution<float> gammaBeta(state.beta, 1.0f);

        float x = gammaAlpha(const_cast<std::mt19937&>(m_rng));
        float y = gammaBeta(const_cast<std::mt19937&>(m_rng));
        float sample = x / (x + y);

        if (sample > bestSample) {
            bestSample = sample;
            bestArm = id;
        }
    }

    return bestArm;
}

std::pair<float, float> ThompsonSampler::getParams(const std::string& modelId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_arms.find(modelId);
    if (it == m_arms.end()) return {1.0f, 1.0f};
    return {it->second.alpha, it->second.beta};
}

// ============================================================================
// ModelCascadeRouter
// ============================================================================

ModelCascadeRouter::ModelCascadeRouter() = default;
ModelCascadeRouter::~ModelCascadeRouter() = default;

ModelCascadeRouter& ModelCascadeRouter::Global() {
    static ModelCascadeRouter instance;
    return instance;
}

void ModelCascadeRouter::registerModel(const ModelCapability& model) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_models[model.modelId] = model;
    m_breakers[model.modelId] = std::make_unique<CircuitBreaker>();
    m_histograms[model.modelId] = std::make_unique<LatencyHistogram>();
    m_sampler.addArm(model.modelId);
    m_acceptCounts[model.modelId] = {0, 0};
}

void ModelCascadeRouter::removeModel(const std::string& modelId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_models.erase(modelId);
    m_breakers.erase(modelId);
    m_histograms.erase(modelId);
    m_acceptCounts.erase(modelId);
}

const ModelCapability* ModelCascadeRouter::getModel(const std::string& modelId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_models.find(modelId);
    return (it != m_models.end()) ? &it->second : nullptr;
}

std::vector<std::string> ModelCascadeRouter::availableModels() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> models;
    for (const auto& [id, _] : m_models) {
        if (isModelAvailable(id)) {
            models.push_back(id);
        }
    }
    return models;
}

RoutingResult ModelCascadeRouter::route(TaskType task, const std::string& prompt) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_models.empty()) {
        return RoutingResult::error("No models registered");
    }

    int estimatedTokens = TokenEstimator::estimate(prompt);
    auto decision = selectModel(task, estimatedTokens);

    if (decision.modelId.empty()) {
        return RoutingResult::error("No suitable model found");
    }

    return RoutingResult::ok(std::move(decision));
}

RoutingResult ModelCascadeRouter::routeWithPreference(TaskType task, const std::string& prompt,
                                                       const std::string& preferredModel) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Try preferred model first
    if (!preferredModel.empty()) {
        auto it = m_models.find(preferredModel);
        if (it != m_models.end() && isModelAvailable(preferredModel)) {
            int tokens = TokenEstimator::estimate(prompt);
            if (static_cast<size_t>(tokens) < it->second.contextWindow) {
                RoutingDecision dec;
                dec.modelId = preferredModel;
                dec.confidence = 0.9f;
                dec.taskType = task;
                dec.estimatedTokens = tokens;
                dec.reason = "User preferred model";

                // Build fallback chain
                std::ostringstream chain;
                chain << preferredModel;
                for (const auto& fb : m_fallbackChain) {
                    if (fb != preferredModel) chain << " -> " << fb;
                }
                dec.fallbackChain = chain.str();

                return RoutingResult::ok(std::move(dec));
            }
        }
    }

    return route(task, prompt);
}

void ModelCascadeRouter::setFallbackChain(const std::vector<std::string>& chain) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_fallbackChain = chain;
}

std::string ModelCascadeRouter::getNextFallback(const std::string& failedModel) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = std::find(m_fallbackChain.begin(), m_fallbackChain.end(), failedModel);
    if (it != m_fallbackChain.end() && ++it != m_fallbackChain.end()) {
        return *it;
    }
    return "";
}

void ModelCascadeRouter::recordLatency(const std::string& modelId, float latencyMs) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_histograms.find(modelId);
    if (it != m_histograms.end()) {
        it->second->recordLatency(latencyMs);
    }

    // Update model capability
    auto mit = m_models.find(modelId);
    if (mit != m_models.end()) {
        mit->second.latencyP50 = it->second->p50();
        mit->second.latencyP95 = it->second->p95();
        mit->second.latencyP99 = it->second->p99();
    }
}

void ModelCascadeRouter::recordSuccess(const std::string& modelId, TaskType task) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_breakers.find(modelId);
    if (it != m_breakers.end()) {
        it->second->recordSuccess();
    }
    m_sampler.recordReward(modelId, 1.0f);

    // Update task affinity
    auto mit = m_models.find(modelId);
    if (mit != m_models.end()) {
        float& aff = mit->second.taskAffinities[static_cast<uint8_t>(task)];
        aff = aff * 0.9f + 0.1f;  // Exponential moving average toward 1.0
    }
}

void ModelCascadeRouter::recordFailure(const std::string& modelId, TaskType task) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_breakers.find(modelId);
    if (it != m_breakers.end()) {
        it->second->recordFailure();
    }
    m_sampler.recordReward(modelId, 0.0f);

    // Update task affinity
    auto mit = m_models.find(modelId);
    if (mit != m_models.end()) {
        float& aff = mit->second.taskAffinities[static_cast<uint8_t>(task)];
        aff = aff * 0.9f;  // Decay toward 0
    }
}

void ModelCascadeRouter::recordAcceptance(const std::string& modelId, bool accepted) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_acceptCounts.find(modelId);
    if (it != m_acceptCounts.end()) {
        if (accepted) it->second.first++;
        it->second.second++;
    }
}

bool ModelCascadeRouter::isModelAvailable(const std::string& modelId) const {
    auto it = m_breakers.find(modelId);
    if (it == m_breakers.end()) return false;
    return !it->second->isOpen();
}

const LatencyHistogram* ModelCascadeRouter::getLatencyHistogram(const std::string& modelId) const {
    auto it = m_histograms.find(modelId);
    return (it != m_histograms.end()) ? it->second.get() : nullptr;
}

float ModelCascadeRouter::getModelAcceptanceRate(const std::string& modelId) const {
    auto it = m_acceptCounts.find(modelId);
    if (it == m_acceptCounts.end() || it->second.second == 0) return 0.5f;
    return static_cast<float>(it->second.first) / static_cast<float>(it->second.second);
}

RoutingDecision ModelCascadeRouter::selectModel(TaskType task, int estimatedTokens) const {
    RoutingDecision decision;
    decision.taskType = task;
    decision.estimatedTokens = estimatedTokens;

    // Score each available model
    struct ScoredModel {
        std::string id;
        float score;
    };
    std::vector<ScoredModel> candidates;

    for (const auto& [id, model] : m_models) {
        if (!isModelAvailable(id)) continue;
        if (static_cast<size_t>(estimatedTokens) >= model.contextWindow) continue;

        // Composite score: quality * task_affinity / normalized_latency
        float taskAff = model.getTaskAffinity(task);
        float latencyFactor = 1.0f;
        auto histIt = m_histograms.find(id);
        if (histIt != m_histograms.end() && histIt->second->sampleCount() > 0) {
            float p50 = histIt->second->p50();
            latencyFactor = 1.0f / (1.0f + p50 / 1000.0f);  // Normalize to [0,1]
        }

        float score = model.qualityScore * taskAff * latencyFactor;

        // Bonus for tools support on relevant tasks
        if (model.supportsTools &&
            (task == TaskType::REFACTOR || task == TaskType::BUG_FIX)) {
            score *= 1.2f;
        }

        // Bonus for FIM on completion
        if (model.supportsFIM && task == TaskType::CODE_COMPLETION) {
            score *= 1.3f;
        }

        candidates.push_back({id, score});
    }

    if (candidates.empty()) {
        decision.confidence = 0.0f;
        decision.reason = "No candidates";
        return decision;
    }

    // Sort by score, best first
    std::sort(candidates.begin(), candidates.end(),
              [](const ScoredModel& a, const ScoredModel& b) {
                  return a.score > b.score;
              });

    // Also consider Thompson sampling for exploration
    std::string sampledModel = m_sampler.sample();
    bool useSampled = false;
    for (const auto& c : candidates) {
        if (c.id == sampledModel) {
            useSampled = (std::uniform_real_distribution<float>(0.0f, 1.0f)(
                const_cast<std::mt19937&>(m_sampler.m_rng)) < 0.2f);  // 20% exploration
            break;
        }
    }

    if (useSampled) {
        decision.modelId = sampledModel;
        decision.confidence = 0.6f;
        decision.reason = "Thompson exploration";
    } else {
        decision.modelId = candidates[0].id;
        decision.confidence = std::min(candidates[0].score, 1.0f);
        decision.reason = "Best composite score";
    }

    // Build fallback chain
    std::ostringstream chain;
    chain << decision.modelId;
    for (const auto& c : candidates) {
        if (c.id != decision.modelId) {
            chain << " -> " << c.id;
        }
    }
    decision.fallbackChain = chain.str();

    return decision;
}

} // namespace Routing
} // namespace RawrXD
