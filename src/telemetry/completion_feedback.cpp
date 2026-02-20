// ============================================================================
// completion_feedback.cpp — Completion Feedback Engine Implementation
// ============================================================================
// Event recording, feature extraction, online logistic regression training,
// acceptance prediction, and per-model/per-language stats aggregation.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "telemetry/completion_feedback.h"
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <sstream>
#include <algorithm>
#include <fstream>
#include <cstring>

namespace RawrXD {
namespace Telemetry {

// ============================================================================
// Outcome names
// ============================================================================

const char* outcomeName(CompletionOutcome outcome) {
    switch (outcome) {
        case CompletionOutcome::ACCEPTED: return "accepted";
        case CompletionOutcome::REJECTED: return "rejected";
        case CompletionOutcome::IGNORED:  return "ignored";
        case CompletionOutcome::PARTIAL:  return "partial";
        case CompletionOutcome::EDITED:   return "edited";
        default: return "unknown";
    }
}

// ============================================================================
// Feature Vector
// ============================================================================

static constexpr int NUM_FEATURES = 14;

FeatureVector FeatureVector::fromEvent(const CompletionEvent& event) {
    FeatureVector fv;
    fv.features.resize(NUM_FEATURES);

    // Normalized features
    fv.features[0]  = event.latencyMs / 1000.0f;                    // Latency (sec)
    fv.features[1]  = event.displayDurationMs / 5000.0f;            // Display time (5s norm)
    fv.features[2]  = static_cast<float>(event.completionTokens) / 100.0f; // Token count
    fv.features[3]  = static_cast<float>(event.completionLines) / 10.0f;   // Line count
    fv.features[4]  = static_cast<float>(event.prefixTokens) / 1000.0f;    // Context size
    fv.features[5]  = static_cast<float>(event.cursorIndent) / 8.0f;       // Indent level
    fv.features[6]  = event.insideFunction ? 1.0f : 0.0f;
    fv.features[7]  = event.afterNewline ? 1.0f : 0.0f;
    fv.features[8]  = event.hasImports ? 1.0f : 0.0f;
    fv.features[9]  = event.perplexity / 100.0f;                    // Perplexity  
    fv.features[10] = event.meanLogprob + 5.0f;                     // Shift logprob to positive
    fv.features[11] = event.topTokenProb;                            // First token prob [0,1]
    fv.features[12] = (event.taskType == "fim") ? 1.0f : 0.0f;      // FIM indicator
    fv.features[13] = static_cast<float>(event.completionChars) / 200.0f; // Char count

    // Label: 1.0 for accepted/partial, 0.0 for rejected/ignored
    fv.label = (event.outcome == CompletionOutcome::ACCEPTED ||
                event.outcome == CompletionOutcome::PARTIAL) ? 1.0f : 0.0f;

    return fv;
}

// ============================================================================
// Online Logistic Regression
// ============================================================================

OnlineLogisticRegression::OnlineLogisticRegression(int numFeatures,
                                                    float learningRate,
                                                    float l2Lambda)
    : m_weights(numFeatures, 0.0f)
    , m_bias(0.0f)
    , m_learningRate(learningRate)
    , m_l2Lambda(l2Lambda)
    , m_sampleCount(0)
    , m_correct(0)
    , m_total(0) {}

void OnlineLogisticRegression::update(const FeatureVector& example) {
    if (example.features.size() != m_weights.size()) return;

    // Forward pass
    float z = m_bias;
    for (size_t i = 0; i < m_weights.size(); ++i) {
        z += m_weights[i] * example.features[i];
    }
    float pred = sigmoid(z);

    // Gradient: dL/dw = (pred - label) * x + lambda * w
    float error = pred - example.label;

    // Update weights (SGD with L2 regularization)
    for (size_t i = 0; i < m_weights.size(); ++i) {
        m_weights[i] -= m_learningRate * (error * example.features[i] +
                                          m_l2Lambda * m_weights[i]);
    }
    m_bias -= m_learningRate * error;

    m_sampleCount++;

    // Track accuracy
    bool predClass = (pred >= 0.5f);
    bool labelClass = (example.label >= 0.5f);
    if (predClass == labelClass) m_correct++;
    m_total++;
}

float OnlineLogisticRegression::predict(const std::vector<float>& features) const {
    if (features.size() != m_weights.size()) return 0.5f;

    float z = m_bias;
    for (size_t i = 0; i < m_weights.size(); ++i) {
        z += m_weights[i] * features[i];
    }
    return sigmoid(z);
}

float OnlineLogisticRegression::accuracy() const {
    if (m_total == 0) return 0.5f;
    return static_cast<float>(m_correct) / static_cast<float>(m_total);
}

void OnlineLogisticRegression::reset() {
    std::fill(m_weights.begin(), m_weights.end(), 0.0f);
    m_bias = 0.0f;
    m_sampleCount = 0;
    m_correct = 0;
    m_total = 0;
}

std::vector<uint8_t> OnlineLogisticRegression::serialize() const {
    std::vector<uint8_t> data;

    // Format: [numWeights:4][weights:4*N][bias:4][sampleCount:4][correct:4][total:4]
    int numWeights = static_cast<int>(m_weights.size());
    size_t totalSize = 4 + numWeights * 4 + 4 + 4 + 4 + 4;
    data.resize(totalSize);

    uint8_t* ptr = data.data();
    memcpy(ptr, &numWeights, 4); ptr += 4;
    memcpy(ptr, m_weights.data(), numWeights * 4); ptr += numWeights * 4;
    memcpy(ptr, &m_bias, 4); ptr += 4;
    memcpy(ptr, &m_sampleCount, 4); ptr += 4;
    memcpy(ptr, &m_correct, 4); ptr += 4;
    memcpy(ptr, &m_total, 4);

    return data;
}

bool OnlineLogisticRegression::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 4) return false;

    const uint8_t* ptr = data.data();
    int numWeights;
    memcpy(&numWeights, ptr, 4); ptr += 4;

    if (data.size() < static_cast<size_t>(4 + numWeights * 4 + 4 + 4 + 4 + 4)) return false;

    m_weights.resize(numWeights);
    memcpy(m_weights.data(), ptr, numWeights * 4); ptr += numWeights * 4;
    memcpy(&m_bias, ptr, 4); ptr += 4;
    memcpy(&m_sampleCount, ptr, 4); ptr += 4;
    memcpy(&m_correct, ptr, 4); ptr += 4;
    memcpy(&m_total, ptr, 4);

    return true;
}

// ============================================================================
// Completion Feedback Engine
// ============================================================================

CompletionFeedback::CompletionFeedback()
    : m_model(std::make_unique<OnlineLogisticRegression>(NUM_FEATURES)) {}

CompletionFeedback::~CompletionFeedback() = default;

CompletionFeedback& CompletionFeedback::Global() {
    static CompletionFeedback instance;
    return instance;
}

// ============================================================================
// Event Recording
// ============================================================================

FeedbackResult CompletionFeedback::recordEvent(const CompletionEvent& event) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Add to ring buffer
    m_events.push_back(event);
    while (m_events.size() > m_maxEvents) {
        m_events.pop_front();
    }

    // Remove from pending if present
    m_pendingCompletions.erase(event.completionId);

    // Train model if we have an outcome
    if (event.outcome != CompletionOutcome::IGNORED) {
        auto fv = FeatureVector::fromEvent(event);
        m_model->update(fv);
    }

    // Update aggregates
    updateAggregates(event);

    return FeedbackResult::ok("Recorded");
}

FeedbackResult CompletionFeedback::recordAccepted(const std::string& completionId,
                                                    float displayDurationMs) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_pendingCompletions.find(completionId);
    if (it == m_pendingCompletions.end()) {
        return FeedbackResult::error("Completion not found in pending");
    }

    CompletionEvent event = it->second;
    event.outcome = CompletionOutcome::ACCEPTED;
    event.displayDurationMs = displayDurationMs;

    m_pendingCompletions.erase(it);

    // Re-acquire without lock (calling public method)
    m_mutex.unlock();
    auto result = recordEvent(event);
    m_mutex.lock();
    return result;
}

FeedbackResult CompletionFeedback::recordRejected(const std::string& completionId,
                                                    float displayDurationMs) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_pendingCompletions.find(completionId);
    if (it == m_pendingCompletions.end()) {
        return FeedbackResult::error("Completion not found in pending");
    }

    CompletionEvent event = it->second;
    event.outcome = CompletionOutcome::REJECTED;
    event.displayDurationMs = displayDurationMs;

    m_pendingCompletions.erase(it);

    m_mutex.unlock();
    auto result = recordEvent(event);
    m_mutex.lock();
    return result;
}

FeedbackResult CompletionFeedback::recordIgnored(const std::string& completionId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_pendingCompletions.find(completionId);
    if (it != m_pendingCompletions.end()) {
        CompletionEvent event = it->second;
        event.outcome = CompletionOutcome::IGNORED;
        m_pendingCompletions.erase(it);

        m_mutex.unlock();
        auto result = recordEvent(event);
        m_mutex.lock();
        return result;
    }

    return FeedbackResult::ok("Already removed");
}

// ============================================================================
// Prediction
// ============================================================================

float CompletionFeedback::predictAcceptance(const CompletionEvent& event) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto fv = FeatureVector::fromEvent(event);
    return m_model->predict(fv.features);
}

bool CompletionFeedback::shouldShow(const CompletionEvent& event, float threshold) const {
    return predictAcceptance(event) >= threshold;
}

// ============================================================================
// Statistics
// ============================================================================

FeedbackStats CompletionFeedback::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    FeedbackStats stats = {};
    stats.totalEvents = m_events.size();

    float totalLatency = 0.0f;
    float totalDisplay = 0.0f;

    for (const auto& event : m_events) {
        switch (event.outcome) {
            case CompletionOutcome::ACCEPTED: stats.accepted++; break;
            case CompletionOutcome::REJECTED: stats.rejected++; break;
            case CompletionOutcome::IGNORED:  stats.ignored++; break;
            case CompletionOutcome::PARTIAL:  stats.partial++; break;
            default: break;
        }
        totalLatency += event.latencyMs;
        totalDisplay += event.displayDurationMs;
    }

    if (stats.totalEvents > 0) {
        stats.acceptanceRate = static_cast<float>(stats.accepted + stats.partial) /
                               static_cast<float>(stats.totalEvents);
        stats.avgLatencyMs = totalLatency / static_cast<float>(stats.totalEvents);
        stats.avgDisplayDurationMs = totalDisplay / static_cast<float>(stats.totalEvents);
    }

    stats.modelAccuracy = m_model->accuracy();

    // Per-model stats
    for (const auto& [modelId, agg] : m_modelAggregates) {
        FeedbackStats::ModelStats ms;
        ms.modelId = modelId;
        ms.events  = agg.events;
        ms.acceptanceRate = (agg.events > 0)
            ? static_cast<float>(agg.accepted) / static_cast<float>(agg.events)
            : 0.0f;
        ms.avgLatencyMs = (agg.events > 0)
            ? agg.totalLatency / static_cast<float>(agg.events)
            : 0.0f;
        stats.modelStats.push_back(std::move(ms));
    }

    // Per-language stats
    for (const auto& [lang, agg] : m_langAggregates) {
        FeedbackStats::LanguageStats ls;
        ls.language = lang;
        ls.events   = agg.events;
        ls.acceptanceRate = (agg.events > 0)
            ? static_cast<float>(agg.accepted) / static_cast<float>(agg.events)
            : 0.0f;
        stats.languageStats.push_back(std::move(ls));
    }

    return stats;
}

FeedbackStats CompletionFeedback::getRecentStats(int lastN) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    FeedbackStats stats = {};

    size_t start = (m_events.size() > static_cast<size_t>(lastN))
        ? m_events.size() - lastN : 0;

    for (size_t i = start; i < m_events.size(); ++i) {
        stats.totalEvents++;
        switch (m_events[i].outcome) {
            case CompletionOutcome::ACCEPTED: stats.accepted++; break;
            case CompletionOutcome::REJECTED: stats.rejected++; break;
            case CompletionOutcome::IGNORED:  stats.ignored++; break;
            case CompletionOutcome::PARTIAL:  stats.partial++; break;
            default: break;
        }
    }

    if (stats.totalEvents > 0) {
        stats.acceptanceRate = static_cast<float>(stats.accepted + stats.partial) /
                               static_cast<float>(stats.totalEvents);
    }

    return stats;
}

// ============================================================================
// Model Persistence
// ============================================================================

FeedbackResult CompletionFeedback::saveModel(const std::string& path) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto data = m_model->serialize();
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return FeedbackResult::error("Cannot open file for writing");
    }

    file.write(reinterpret_cast<const char*>(data.data()),
               static_cast<std::streamsize>(data.size()));
    if (!file.good()) {
        return FeedbackResult::error("Write failed");
    }

    return FeedbackResult::ok("Model saved");
}

FeedbackResult CompletionFeedback::loadModel(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return FeedbackResult::error("Cannot open file for reading");
    }

    auto size = file.tellg();
    file.seekg(0);

    std::vector<uint8_t> data(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(data.data()), size);

    if (!m_model->deserialize(data)) {
        return FeedbackResult::error("Deserialization failed");
    }

    return FeedbackResult::ok("Model loaded");
}

// ============================================================================
// Configuration
// ============================================================================

void CompletionFeedback::setLearningRate(float lr) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Recreate model with new learning rate (preserving weights isn't trivial)
    auto data = m_model->serialize();
    m_model = std::make_unique<OnlineLogisticRegression>(NUM_FEATURES, lr);
    m_model->deserialize(data);
}

void CompletionFeedback::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_events.clear();
    m_pendingCompletions.clear();
    m_model->reset();
    m_modelAggregates.clear();
    m_langAggregates.clear();
}

// ============================================================================
// Aggregate Updates
// ============================================================================

void CompletionFeedback::updateAggregates(const CompletionEvent& event) {
    // Per-model
    auto& ma = m_modelAggregates[event.modelId];
    ma.events++;
    if (event.outcome == CompletionOutcome::ACCEPTED ||
        event.outcome == CompletionOutcome::PARTIAL) {
        ma.accepted++;
    }
    ma.totalLatency += event.latencyMs;

    // Per-language
    auto& la = m_langAggregates[event.language];
    la.events++;
    if (event.outcome == CompletionOutcome::ACCEPTED ||
        event.outcome == CompletionOutcome::PARTIAL) {
        la.accepted++;
    }
}

} // namespace Telemetry
} // namespace RawrXD
