#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>
#include <deque>
#include <condition_variable>

namespace Maximus {

// =============================================================================
// CORE TYPES - Zero overhead, cache-friendly
// =============================================================================

struct Token {
    std::string_view text;      // Zero-copy view into buffer
    float confidence;           // 0.0 - 1.0 (model's probability)
    float entropy;              // Uncertainty measure
    uint32_t tokenId;           // Raw token ID
    uint32_t position;          // Position in sequence
    uint64_t timestampNs;       // Nanosecond timestamp
    uint16_t flags;             // See TokenFlags below
};

enum TokenFlags : uint16_t {
    None            = 0,
    EndOfSentence   = 1 << 0,   // Complete sentence
    EndOfThought    = 1 << 1,   // Semantic boundary
    EndOfCodeBlock  = 1 << 2,   // Code block closed
    EndOfParagraph  = 1 << 3,   // Paragraph boundary
    Speculative     = 1 << 4,   // From speculative decoding
    Corrected       = 1 << 5,   // Speculative token that was accepted
    Rewound         = 1 << 6,   // Token was backtracked
    Cached          = 1 << 7,   // From KV cache
    Prefetched      = 1 << 8,   // Prefetched token
};

struct StreamMetrics {
    double tokensPerSecond;         // Rolling average
    double tokensPerSecondInstant;  // Instantaneous
    uint64_t totalTokens;
    uint64_t speculativeHits;       // Speculative tokens accepted
    uint64_t speculativeMisses;     // Speculative tokens rejected
    uint64_t cacheHits;
    uint64_t cacheMisses;
    uint64_t rewindCount;
    double averageConfidence;
    double averageEntropy;
    uint64_t contextBytes;
    uint64_t peakMemoryBytes;
    std::chrono::microseconds totalTime;
    std::chrono::microseconds firstTokenLatency;
};

struct Checkpoint {
    uint64_t id;
    uint32_t position;
    std::string contextSnapshot;    // Compressed context state
    std::chrono::system_clock::time_point createdAt;
};

// =============================================================================
// CONFIGURATION - Sensible defaults, one-line setup
// =============================================================================

struct StreamerConfig {
    // Throughput
    uint32_t bufferSize = 4096;             // Token buffer size
    uint32_t prefetchTokens = 8;            // Speculative lookahead
    uint32_t batchSize = 1;                 // Parallel request batch
    
    // Semantic streaming
    bool streamBySentence = true;           // Wait for complete sentences
    bool streamByThought = false;           // Wait for semantic boundaries
    bool streamByCodeBlock = true;          // Wait for code blocks
    float minConfidenceThreshold = 0.0f;    // Filter low-confidence tokens
    
    // Auto-compact
    bool autoCompact = true;                // Auto-compress context
    uint32_t compactThreshold = 32768;      // Tokens before compact
    float compactRatio = 0.3f;              // Keep ratio after compact
    
    // Time-travel
    bool enableCheckpoints = true;          // Enable rewind
    uint32_t checkpointInterval = 256;      // Tokens per checkpoint
    uint32_t maxCheckpoints = 16;           // Max stored checkpoints
    
    // Flow control
    bool backpressure = true;               // Enable consumer pacing
    uint32_t maxQueueDepth = 1024;          // Max tokens in queue
    
    // Performance
    bool zeroCopy = true;                   // Zero-copy token views
    bool parallelSpeculative = true;        // Parallel speculative decode
    uint32_t speculativeWorkers = 2;        // Draft model threads
};

// =============================================================================
// CALLBACK TYPES
// =============================================================================

using TokenCallback = std::function<void(const Token&)>;
using SentenceCallback = std::function<void(std::string_view)>;
using ThoughtCallback = std::function<void(std::string_view, float confidence)>;
using CodeBlockCallback = std::function<void(std::string_view, const std::string& language)>;
using MetricsCallback = std::function<void(const StreamMetrics&)>;
using CheckpointCallback = std::function<void(const Checkpoint&)>;
using ErrorCallback = std::function<void(const std::string&)>;

// =============================================================================
// STREAMER INTERFACE - Drop-in simplicity
// =============================================================================

class MaximusStreamer {
public:
    // Factory: One line to create
    static std::unique_ptr<MaximusStreamer> create(StreamerConfig config = {});
    
    virtual ~MaximusStreamer() = default;
    
    // =========================================================================
    // SIMPLE API - 90% use cases
    // =========================================================================
    
    // Stream to callback - fire and forget
    virtual void stream(
        const std::string& prompt,
        TokenCallback onToken,
        std::function<void()> onComplete = nullptr
    );
    
    // Stream with semantic boundaries - smarter streaming
    virtual void streamSemantic(
        const std::string& prompt,
        SentenceCallback onSentence,
        ThoughtCallback onThought = nullptr,
        CodeBlockCallback onCodeBlock = nullptr
    );
    
    // Generator-style iteration (C++20 coroutines friendly)
    class TokenIterator {
    public:
        TokenIterator();
        ~TokenIterator();
        TokenIterator(TokenIterator&&) noexcept;
        TokenIterator& operator=(TokenIterator&&) noexcept;
        Token operator*() const;
        TokenIterator& operator++();
        bool operator!=(const TokenIterator& other) const;
        bool done() const;
    private:
        friend class MaximusStreamer;
        friend class MaximusStreamerImpl;
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
    
    virtual TokenIterator begin();
    virtual TokenIterator end();
    
    // =========================================================================
    // STREAMING CONTROL
    // =========================================================================
    
    // Backpressure: tell streamer you're ready for more
    virtual void requestMore(uint32_t count = 1);
    
    // Pause/Resume without losing state
    virtual void pause();
    virtual void resume();
    virtual bool isPaused() const;
    
    // Cancel entirely
    virtual void cancel();
    virtual bool isCancelled() const;
    
    // =========================================================================
    // TIME-TRAVEL - Never heard of before
    // =========================================================================
    
    // Create checkpoint for later rewind
    virtual Checkpoint checkpoint();
    
    // Rewind to checkpoint (discards tokens after)
    virtual bool rewind(const Checkpoint& cp);
    virtual bool rewind(uint32_t position);
    
    // Get recent checkpoints
    virtual std::vector<Checkpoint> getCheckpoints() const;
    
    // Time-travel metrics: what would have happened?
    struct AlternateHistory {
        std::string generatedText;
        float probability;
        uint32_t divergedAt;
    };
    virtual std::vector<AlternateHistory> getAlternateHistories(uint32_t position);
    
    // =========================================================================
    // SPECULATIVE DECODING - Transparent, automatic
    // =========================================================================
    
    // Auto-enabled when draft model available
    virtual void setDraftModel(void* draftModel);  // Opaque pointer to your model
    
    // Speculative stats
    virtual float speculativeAcceptRate() const;
    
    // =========================================================================
    // AUTO-COMPACT - Transparent context management
    // =========================================================================
    
    // Manually trigger compact (usually automatic)
    virtual void compactContext();
    
    // Get current context size
    virtual uint32_t contextTokenCount() const;
    virtual uint32_t contextByteCount() const;
    
    // =========================================================================
    // METRICS & OBSERVABILITY
    // =========================================================================
    
    virtual StreamMetrics metrics() const;
    virtual void setMetricsCallback(MetricsCallback cb, std::chrono::milliseconds interval = std::chrono::milliseconds(100));
    
    // Real-time TPS display
    virtual std::string metricsString() const;  // "1423.7 TPS | 98.2% confidence | 127 cache hits"
    
    // =========================================================================
    // CALLBACK REGISTRATION
    // =========================================================================
    
    virtual void onSentence(SentenceCallback cb);
    virtual void onThought(ThoughtCallback cb);
    virtual void onCodeBlock(CodeBlockCallback cb);
    virtual void onError(ErrorCallback cb);
    virtual void onCheckpoint(CheckpointCallback cb);
    
    // =========================================================================
    // LOW-LEVEL ACCESS (for advanced users)
    // =========================================================================
    
    // Direct buffer access (zero-copy)
    virtual const char* buffer() const;
    virtual uint32_t bufferUsed() const;
    
    // Token at position
    virtual Token tokenAt(uint32_t position) const;
    
    // Full generated text
    virtual std::string_view text() const;
    virtual std::string textCopy() const;
    
    // Raw token IDs
    virtual const std::vector<uint32_t>& tokenIds() const;

protected:
    MaximusStreamer() = default;
};

// Default implementations for pure virtual-like methods
inline void MaximusStreamer::stream(const std::string&, TokenCallback, std::function<void()>) {}
inline void MaximusStreamer::streamSemantic(const std::string&, SentenceCallback, ThoughtCallback, CodeBlockCallback) {}
inline MaximusStreamer::TokenIterator MaximusStreamer::begin() { return TokenIterator(); }
inline MaximusStreamer::TokenIterator MaximusStreamer::end() { return TokenIterator(); }
inline void MaximusStreamer::requestMore(uint32_t) {}
inline void MaximusStreamer::pause() {}
inline void MaximusStreamer::resume() {}
inline bool MaximusStreamer::isPaused() const { return false; }
inline void MaximusStreamer::cancel() {}
inline bool MaximusStreamer::isCancelled() const { return false; }
inline Checkpoint MaximusStreamer::checkpoint() { return Checkpoint{}; }
inline bool MaximusStreamer::rewind(const Checkpoint&) { return false; }
inline bool MaximusStreamer::rewind(uint32_t) { return false; }
inline std::vector<Checkpoint> MaximusStreamer::getCheckpoints() const { return {}; }
inline std::vector<MaximusStreamer::AlternateHistory> MaximusStreamer::getAlternateHistories(uint32_t) { return {}; }
inline void MaximusStreamer::setDraftModel(void*) {}
inline float MaximusStreamer::speculativeAcceptRate() const { return 0.0f; }
inline void MaximusStreamer::compactContext() {}
inline uint32_t MaximusStreamer::contextTokenCount() const { return 0; }
inline uint32_t MaximusStreamer::contextByteCount() const { return 0; }
inline StreamMetrics MaximusStreamer::metrics() const { return StreamMetrics{}; }
inline void MaximusStreamer::setMetricsCallback(MetricsCallback, std::chrono::milliseconds) {}
inline std::string MaximusStreamer::metricsString() const { return ""; }
inline void MaximusStreamer::onSentence(SentenceCallback) {}
inline void MaximusStreamer::onThought(ThoughtCallback) {}
inline void MaximusStreamer::onCodeBlock(CodeBlockCallback) {}
inline void MaximusStreamer::onError(ErrorCallback) {}
inline void MaximusStreamer::onCheckpoint(CheckpointCallback) {}
inline const char* MaximusStreamer::buffer() const { return ""; }
inline uint32_t MaximusStreamer::bufferUsed() const { return 0; }
inline Token MaximusStreamer::tokenAt(uint32_t) const { return Token{}; }
inline std::string_view MaximusStreamer::text() const { return std::string_view(); }
inline std::string MaximusStreamer::textCopy() const { return ""; }
inline const std::vector<uint32_t>& MaximusStreamer::tokenIds() const { static std::vector<uint32_t> empty; return empty; }

// =============================================================================
// DROP-IN WRAPPER - Replace your existing streamer in 1 line
// =============================================================================

// If you have: void my_stream(const string& prompt, function<void(string)> cb)
// Replace with:
//   auto streamer = Maximus::makeStreamer();
//   streamer->stream(prompt, [&](auto& tok) { cb(std::string(tok.text)); });

inline std::unique_ptr<MaximusStreamer> makeStreamer(StreamerConfig config = {}) {
    return MaximusStreamer::create(std::move(config));
}

// =============================================================================
// CONVENIENCE BUILDERS
// =============================================================================

class StreamerBuilder {
public:
    StreamerBuilder() = default;
    
    StreamerBuilder& bufferSize(uint32_t size) { config_.bufferSize = size; return *this; }
    StreamerBuilder& prefetch(uint32_t tokens) { config_.prefetchTokens = tokens; return *this; }
    StreamerBuilder& bySentence(bool enable = true) { config_.streamBySentence = enable; return *this; }
    StreamerBuilder& byThought(bool enable = true) { config_.streamByThought = enable; return *this; }
    StreamerBuilder& byCodeBlock(bool enable = true) { config_.streamByCodeBlock = enable; return *this; }
    StreamerBuilder& minConfidence(float threshold) { config_.minConfidenceThreshold = threshold; return *this; }
    StreamerBuilder& autoCompact(bool enable = true, uint32_t threshold = 32768) {
        config_.autoCompact = enable;
        config_.compactThreshold = threshold;
        return *this;
    }
    StreamerBuilder& checkpoints(bool enable = true, uint32_t interval = 256) {
        config_.enableCheckpoints = enable;
        config_.checkpointInterval = interval;
        return *this;
    }
    StreamerBuilder& backpressure(bool enable = true, uint32_t maxDepth = 1024) {
        config_.backpressure = enable;
        config_.maxQueueDepth = maxDepth;
        return *this;
    }
    StreamerBuilder& speculative(bool enable = true, uint32_t workers = 2) {
        config_.parallelSpeculative = enable;
        config_.speculativeWorkers = workers;
        return *this;
    }
    StreamerBuilder& highThroughput() {
        config_.bufferSize = 8192;
        config_.prefetchTokens = 16;
        config_.parallelSpeculative = true;
        config_.speculativeWorkers = 4;
        config_.zeroCopy = true;
        return *this;
    }
    StreamerBuilder& lowLatency() {
        config_.bufferSize = 512;
        config_.prefetchTokens = 2;
        config_.streamBySentence = false;
        config_.streamByThought = false;
        return *this;
    }
    StreamerBuilder& memoryConscious() {
        config_.bufferSize = 1024;
        config_.autoCompact = true;
        config_.compactThreshold = 16384;
        config_.maxCheckpoints = 4;
        config_.speculativeWorkers = 1;
        return *this;
    }
    
    std::unique_ptr<MaximusStreamer> build() {
        return MaximusStreamer::create(config_);
    }
    
private:
    StreamerConfig config_;
};

// =============================================================================
// ADVANCED FEATURES - Never seen before
// =============================================================================

// SEMANTIC DIFF: Compare two generations semantically, not token-by-token
struct SemanticDiff {
    float similarity;           // 0.0 - 1.0
    std::vector<std::string> addedThoughts;
    std::vector<std::string> removedThoughts;
    std::vector<std::string> modifiedThoughts;
};
SemanticDiff semanticDiff(std::string_view text1, std::string_view text2);

// TOKEN PROVENANCE: What influenced this token?
struct Provenance {
    uint32_t tokenPosition;
    std::vector<uint32_t> influencedBy;     // Context positions that influenced
    std::vector<float> influenceWeights;     // Attention weights
    std::string source;                      // "prompt", "context", "generated"
};
Provenance getTokenProvenance(const MaximusStreamer& streamer, uint32_t position);

// CONFIDENCE HEATMAP: Visualize confidence over generation
struct ConfidenceHeatmap {
    std::vector<float> confidenceByPosition;
    std::vector<float> entropyByPosition;
    std::string renderAscii(uint32_t width = 80) const;
};

} // namespace Maximus
