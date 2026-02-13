// ============================================================================
// completion_feedback.h — Completion Feedback / Acceptance Tracking
// ============================================================================
// Tracks inline completion accept/reject/ignore events, extracts features,
// and trains an online logistic regression model to predict acceptance.
// Feeds back into model cascade router quality scoring.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once
#include <memory>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <deque>
#include <chrono>
#include <cmath>

namespace RawrXD {
namespace Telemetry {

// ============================================================================
// Completion Outcome
// ============================================================================

enum class CompletionOutcome : uint8_t {
    ACCEPTED     = 0,   // User pressed Tab / accepted
    REJECTED     = 1,   // User pressed Esc / dismissed
    IGNORED      = 2,   // Completion timed out or was replaced
    PARTIAL      = 3,   // Partially accepted (typed some chars)
    EDITED       = 4    // Accepted then immediately edited
};

const char* outcomeName(CompletionOutcome outcome);

// ============================================================================
// Completion Event
// ============================================================================

struct CompletionEvent {
    std::string completionId;       // Unique completion ID
    std::string modelId;            // Which model generated it
    std::string language;           // File language
    std::string taskType;           // "inline", "fim", "chat", "refactor"

    CompletionOutcome outcome;
    uint64_t    timestampMs;        // When completion was shown
    float       latencyMs;          // Time to generate
    float       displayDurationMs;  // How long displayed before action

    // Completion content
    int         completionTokens;   // Token count of completion
    int         completionLines;    // Line count
    int         completionChars;    // Character count

    // Context features
    int         prefixTokens;       // Tokens of context sent
    int         cursorLine;         // Line number in file
    int         cursorIndent;       // Indentation level
    bool        insideFunction;     // Cursor inside a function body
    bool        afterNewline;       // Cursor at start of new line
    bool        hasImports;         // File has imports above

    // Quality signals
    float       perplexity;         // Model perplexity for this completion
    float       meanLogprob;        // Average log probability
    float       topTokenProb;       // Probability of first token
};

// ============================================================================
// Feature Vector for ML
// ============================================================================

struct FeatureVector {
    std::vector<float> features;
    float              label;       // 1.0 = accepted, 0.0 = rejected

    static FeatureVector fromEvent(const CompletionEvent& event);
};

// ============================================================================
// Online Logistic Regression
// ============================================================================

class OnlineLogisticRegression {
public:
    explicit OnlineLogisticRegression(int numFeatures,
                                       float learningRate = 0.01f,
                                       float l2Lambda = 0.001f);

    // Update weights with single example (SGD step)
    void update(const FeatureVector& example);

    // Predict probability of acceptance
    float predict(const std::vector<float>& features) const;

    // Get current weights
    const std::vector<float>& getWeights() const { return m_weights; }
    float getBias() const { return m_bias; }

    // Stats
    int   sampleCount() const { return m_sampleCount; }
    float accuracy() const;

    // Reset
    void reset();

    // Serialize / Deserialize
    std::vector<uint8_t> serialize() const;
    bool deserialize(const std::vector<uint8_t>& data);

private:
    static float sigmoid(float x) {
        return 1.0f / (1.0f + std::exp(-x));
    }

    std::vector<float> m_weights;
    float              m_bias;
    float              m_learningRate;
    float              m_l2Lambda;
    int                m_sampleCount;

    // Running accuracy
    int m_correct;
    int m_total;
};

// ============================================================================
// Feedback Result
// ============================================================================

struct FeedbackResult {
    bool        success;
    const char* detail;
    int         errorCode;

    static FeedbackResult ok(const char* msg = "OK") {
        FeedbackResult r;
        r.success   = true;
        r.detail    = msg;
        r.errorCode = 0;
        return r;
    }

    static FeedbackResult error(const char* msg, int code = -1) {
        FeedbackResult r;
        r.success   = false;
        r.detail    = msg;
        r.errorCode = code;
        return r;
    }
};

// ============================================================================
// Feedback Stats
// ============================================================================

struct FeedbackStats {
    uint64_t totalEvents;
    uint64_t accepted;
    uint64_t rejected;
    uint64_t ignored;
    uint64_t partial;
    float    acceptanceRate;
    float    modelAccuracy;            // Prediction accuracy
    float    avgLatencyMs;
    float    avgDisplayDurationMs;

    // Per-model stats
    struct ModelStats {
        std::string modelId;
        uint64_t    events;
        float       acceptanceRate;
        float       avgLatencyMs;
    };
    std::vector<ModelStats> modelStats;

    // Per-language stats
    struct LanguageStats {
        std::string language;
        uint64_t    events;
        float       acceptanceRate;
    };
    std::vector<LanguageStats> languageStats;
};

// ============================================================================
// Completion Feedback Engine
// ============================================================================

class CompletionFeedback {
public:
    CompletionFeedback();
    ~CompletionFeedback();

    // Singleton
    static CompletionFeedback& Global();

    // ---- Event Recording ----

    FeedbackResult recordEvent(const CompletionEvent& event);

    // Convenience methods
    FeedbackResult recordAccepted(const std::string& completionId,
                                   float displayDurationMs = 0.0f);
    FeedbackResult recordRejected(const std::string& completionId,
                                   float displayDurationMs = 0.0f);
    FeedbackResult recordIgnored(const std::string& completionId);

    // ---- Prediction ----

    // Predict whether a completion will be accepted (0.0 - 1.0)
    float predictAcceptance(const CompletionEvent& event) const;

    // Should we show this completion? (threshold-based)
    bool shouldShow(const CompletionEvent& event, float threshold = 0.3f) const;

    // ---- Statistics ----

    FeedbackStats getStats() const;
    FeedbackStats getRecentStats(int lastN = 100) const;

    // ---- Model Management ----

    FeedbackResult saveModel(const std::string& path) const;
    FeedbackResult loadModel(const std::string& path);

    // Reset all data
    void reset();

    // ---- Configuration ----

    void setMaxEvents(size_t max) { m_maxEvents = max; }
    void setLearningRate(float lr);

private:
    // Feature extraction from event
    FeatureVector extractFeatures(const CompletionEvent& event) const;

    // Update per-model and per-language aggregates
    void updateAggregates(const CompletionEvent& event);

    // Events ring buffer
    std::deque<CompletionEvent> m_events;
    size_t m_maxEvents = 10000;

    // Pending completions (shown but not yet acted on)
    std::unordered_map<std::string, CompletionEvent> m_pendingCompletions;

    // Online model
    std::unique_ptr<OnlineLogisticRegression> m_model;

    // Aggregates
    struct ModelAggregate {
        uint64_t events     = 0;
        uint64_t accepted   = 0;
        float    totalLatency = 0.0f;
    };
    std::unordered_map<std::string, ModelAggregate> m_modelAggregates;

    struct LangAggregate {
        uint64_t events   = 0;
        uint64_t accepted = 0;
    };
    std::unordered_map<std::string, LangAggregate> m_langAggregates;

    mutable std::mutex m_mutex;
};

} // namespace Telemetry
} // namespace RawrXD
