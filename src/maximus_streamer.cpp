#include "maximus_streamer.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <future>
#include <mutex>
#include <queue>
#include <random>
#include <sstream>
#include <thread>
#include <unordered_set>

namespace Maximus {

// =============================================================================
// INTERNAL IMPLEMENTATION
// =============================================================================

class MaximusStreamerImpl : public MaximusStreamer {
public:
    explicit MaximusStreamerImpl(StreamerConfig config)
        : config_(std::move(config))
        , buffer_(config_.bufferSize * 8, 0)  // 8 bytes per token avg
        , metrics_{}
    {
        tokenBuffer_.reserve(config_.bufferSize);
        tokenIdBuffer_.reserve(config_.bufferSize);
    }
    
    ~MaximusStreamerImpl() override {
        cancel();
        if (streamThread_.joinable()) {
            streamThread_.join();
        }
    }
    
    // =========================================================================
    // SIMPLE API
    // =========================================================================
    
    void stream(
        const std::string& prompt,
        TokenCallback onToken,
        std::function<void()> onComplete
    ) override {
        std::lock_guard<std::mutex> lock(streamMutex_);
        
        if (streaming_) {
            if (onError_) onError_("Stream already in progress");
            return;
        }
        
        prompt_ = prompt;
        onToken_ = std::move(onToken);
        onComplete_ = std::move(onComplete);
        
        startStream();
    }
    
    void streamSemantic(
        const std::string& prompt,
        SentenceCallback onSentence,
        ThoughtCallback onThought,
        CodeBlockCallback onCodeBlock
    ) override {
        std::lock_guard<std::mutex> lock(streamMutex_);
        
        if (streaming_) {
            if (onError_) onError_("Stream already in progress");
            return;
        }
        
        prompt_ = prompt;
        onSentence_ = std::move(onSentence);
        onThought_ = std::move(onThought);
        onCodeBlock_ = std::move(onCodeBlock);
        semanticMode_ = true;
        
        startStream();
    }
    
    // =========================================================================
    // ITERATOR
    // =========================================================================
    
    TokenIterator begin() override {
        TokenIterator it;
        it.impl_ = std::make_unique<TokenIterator::Impl>(this);
        return it;
    }
    
    TokenIterator end() override {
        TokenIterator it;
        it.impl_ = std::make_unique<TokenIterator::Impl>(this, true);
        return it;
    }
    
    // =========================================================================
    // CONTROL
    // =========================================================================
    
    void requestMore(uint32_t count) override {
        std::lock_guard<std::mutex> lock(flowMutex_);
        requestedTokens_ += count;
        flowCv_.notify_one();
    }
    
    void pause() override {
        paused_.store(true);
    }
    
    void resume() override {
        paused_.store(false);
        flowCv_.notify_all();
    }
    
    bool isPaused() const override {
        return paused_.load();
    }
    
    void cancel() override {
        cancelled_.store(true);
        flowCv_.notify_all();
        streamCv_.notify_all();
    }
    
    bool isCancelled() const override {
        return cancelled_.load();
    }
    
    // =========================================================================
    // TIME-TRAVEL
    // =========================================================================
    
    Checkpoint checkpoint() override {
        std::lock_guard<std::mutex> lock(stateMutex_);
        
        Checkpoint cp;
        cp.id = nextCheckpointId_++;
        cp.position = static_cast<uint32_t>(tokenBuffer_.size());
        cp.contextSnapshot = compressContext();
        cp.createdAt = std::chrono::system_clock::now();
        
        checkpoints_.push_back(cp);
        
        // Trim old checkpoints
        while (checkpoints_.size() > config_.maxCheckpoints) {
            checkpoints_.pop_front();
        }
        
        if (onCheckpoint_) {
            onCheckpoint_(cp);
        }
        
        return cp;
    }
    
    bool rewind(const Checkpoint& cp) override {
        return rewind(cp.position);
    }
    
    bool rewind(uint32_t position) override {
        std::lock_guard<std::mutex> lock(stateMutex_);
        
        if (position >= tokenBuffer_.size()) {
            return false;
        }
        
        // Mark tokens as rewound
        for (size_t i = position; i < tokenBuffer_.size(); ++i) {
            tokenBuffer_[i].flags |= TokenFlags::Rewound;
        }
        
        // Truncate
        tokenBuffer_.resize(position);
        tokenIdBuffer_.resize(position);
        
        // Update buffer position
        bufferUsed_ = 0;
        for (const auto& tok : tokenBuffer_) {
            bufferUsed_ += tok.text.size();
        }
        
        metrics_.rewindCount++;
        
        return true;
    }
    
    std::vector<Checkpoint> getCheckpoints() const override {
        std::lock_guard<std::mutex> lock(stateMutex_);
        return std::vector<Checkpoint>(checkpoints_.begin(), checkpoints_.end());
    }
    
    std::vector<MaximusStreamer::AlternateHistory> 
    getAlternateHistories(uint32_t position) override {
        // This would require integration with model's sampling state
        // For now, return placeholder
        std::vector<AlternateHistory> histories;
        
        // In a real implementation, we'd query the model's alternative
        // token probabilities at that position
        
        return histories;
    }
    
    // =========================================================================
    // SPECULATIVE
    // =========================================================================
    
    void setDraftModel(void* draftModel) override {
        draftModel_ = draftModel;
        speculativeEnabled_ = (draftModel != nullptr);
    }
    
    float speculativeAcceptRate() const override {
        uint64_t total = metrics_.speculativeHits + metrics_.speculativeMisses;
        if (total == 0) return 0.0f;
        return static_cast<float>(metrics_.speculativeHits) / total;
    }
    
    // =========================================================================
    // AUTO-COMPACT
    // =========================================================================
    
    void compactContext() override {
        std::lock_guard<std::mutex> lock(stateMutex_);
        
        if (tokenBuffer_.size() < 100) return;
        
        // Keep start and end, compress middle
        uint32_t keepStart = static_cast<uint32_t>(tokenBuffer_.size() * 0.2f);
        uint32_t keepEnd = static_cast<uint32_t>(tokenBuffer_.size() * config_.compactRatio * 0.8f);
        
        // In real implementation, this would:
        // 1. Extract semantic summaries from middle section
        // 2. Create compressed representation
        // 3. Update model's KV cache
        
        // For now, just track that we did it
        metrics_.contextBytes = keepStart * 4 + keepEnd * 4;
    }
    
    uint32_t contextTokenCount() const override {
        return static_cast<uint32_t>(tokenBuffer_.size());
    }
    
    uint32_t contextByteCount() const override {
        return bufferUsed_;
    }
    
    // =========================================================================
    // METRICS
    // =========================================================================
    
    StreamMetrics metrics() const override {
        std::lock_guard<std::mutex> lock(metricsMutex_);
        return metrics_;
    }
    
    void setMetricsCallback(
        MetricsCallback cb, 
        std::chrono::milliseconds interval
    ) override {
        metricsCallback_ = std::move(cb);
        metricsInterval_ = interval;
    }
    
    std::string metricsString() const override {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1);
        oss << metrics_.tokensPerSecond << " TPS | ";
        oss << (metrics_.averageConfidence * 100.0) << "% confidence | ";
        oss << metrics_.cacheHits << " cache hits";
        
        if (metrics_.speculativeHits > 0 || metrics_.speculativeMisses > 0) {
            oss << " | " << (speculativeAcceptRate() * 100.0) << "% spec accept";
        }
        
        return oss.str();
    }
    
    // =========================================================================
    // CALLBACKS
    // =========================================================================
    
    void onSentence(SentenceCallback cb) override { onSentence_ = std::move(cb); }
    void onThought(ThoughtCallback cb) override { onThought_ = std::move(cb); }
    void onCodeBlock(CodeBlockCallback cb) override { onCodeBlock_ = std::move(cb); }
    void onError(ErrorCallback cb) override { onError_ = std::move(cb); }
    void onCheckpoint(CheckpointCallback cb) override { onCheckpoint_ = std::move(cb); }
    
    // =========================================================================
    // LOW-LEVEL
    // =========================================================================
    
    const char* buffer() const override { return buffer_.data(); }
    uint32_t bufferUsed() const override { return bufferUsed_; }
    
    Token tokenAt(uint32_t position) const override {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (position >= tokenBuffer_.size()) {
            return Token{};
        }
        return tokenBuffer_[position];
    }
    
    std::string_view text() const override {
        return std::string_view(buffer_.data(), bufferUsed_);
    }
    
    std::string textCopy() const override {
        return std::string(buffer_.data(), bufferUsed_);
    }
    
    const std::vector<uint32_t>& tokenIds() const override {
        return tokenIdBuffer_;
    }
    
private:
    // =========================================================================
    // INTERNAL TYPES
    // =========================================================================
    
    struct SemanticBoundary {
        uint32_t position;
        uint16_t type;  // TokenFlags
        float confidence;
    };
    
    // =========================================================================
    // STREAMING ENGINE
    // =========================================================================
    
    void startStream() {
        cancelled_.store(false);
        paused_.store(false);
        streaming_ = true;
        startTime_ = std::chrono::steady_clock::now();
        firstTokenTime_ = startTime_;
        firstToken_ = true;
        
        // Reset state
        tokenBuffer_.clear();
        tokenIdBuffer_.clear();
        bufferUsed_ = 0;
        sentenceBuffer_.clear();
        codeBlockBuffer_.clear();
        requestedTokens_ = UINT32_MAX;  // Unlimited by default
        
        if (streamThread_.joinable()) {
            streamThread_.join();
        }
        
        streamThread_ = std::thread(&MaximusStreamerImpl::streamLoop, this);
    }
    
    void streamLoop() {
        try {
            // PHASE 1: Initialize context
            initializeContext();
            
            // PHASE 2: Main generation loop
            while (!cancelled_.load() && !generationDone_) {
                // Backpressure check
                if (config_.backpressure) {
                    waitForConsumer();
                }
                
                // Pause check
                while (paused_.load() && !cancelled_.load()) {
                    std::unique_lock<std::mutex> lock(flowMutex_);
                    flowCv_.wait_for(lock, std::chrono::milliseconds(10));
                }
                
                if (cancelled_.load()) break;
                
                // Generate next token(s)
                if (speculativeEnabled_ && config_.parallelSpeculative) {
                    generateSpeculative();
                } else {
                    generateSingle();
                }
                
                // Check auto-compact
                if (config_.autoCompact && tokenBuffer_.size() >= config_.compactThreshold) {
                    compactContext();
                }
                
                // Create checkpoint if needed
                if (config_.enableCheckpoints && 
                    tokenBuffer_.size() % config_.checkpointInterval == 0) {
                    checkpoint();
                }
                
                // Update metrics
                updateMetrics();
            }
            
            // PHASE 3: Finalize
            finalizeStream();
            
        } catch (const std::exception& e) {
            if (onError_) {
                onError_(std::string("Stream error: ") + e.what());
            }
        }
        
        streaming_ = false;
        
        if (onComplete_) {
            onComplete_();
        }
    }
    
    void initializeContext() {
        // In real implementation:
        // - Process prompt tokens
        // - Initialize KV cache
        // - Set up sampling state
        
        // Track first token latency start
        firstTokenTime_ = std::chrono::steady_clock::now();
    }
    
    void generateSingle() {
        // Placeholder: In real implementation, call your model here
        // This demonstrates the token emission pattern
        
        // Simulated token generation
        Token tok;
        tok.position = static_cast<uint32_t>(tokenBuffer_.size());
        tok.timestampNs = getTimestampNs();
        
        // ... your model generates the token here ...
        // tok.text = generated_text;
        // tok.tokenId = generated_id;
        // tok.confidence = model_probability;
        // tok.entropy = calculate_entropy(logits);
        
        emitToken(tok);
    }
    
    void generateSpeculative() {
        // Speculative decoding:
        // 1. Draft model generates K tokens
        // 2. Main model verifies in parallel
        // 3. Accept matching prefix, reject rest
        
        std::vector<Token> draftTokens;
        draftTokens.reserve(config_.prefetchTokens);
        
        // Draft model generates
        for (uint32_t i = 0; i < config_.prefetchTokens && !generationDone_; ++i) {
            Token tok;
            tok.position = static_cast<uint32_t>(tokenBuffer_.size() + i);
            tok.flags = TokenFlags::Speculative;
            // ... draft model generates ...
            draftTokens.push_back(tok);
        }
        
        // Main model verifies
        uint32_t accepted = 0;
        for (auto& tok : draftTokens) {
            // ... verify with main model ...
            bool accept = true; // = verify_token(tok);
            
            if (accept) {
                tok.flags |= TokenFlags::Corrected;
                tok.flags &= ~TokenFlags::Speculative;
                emitToken(tok);
                accepted++;
                metrics_.speculativeHits++;
            } else {
                metrics_.speculativeMisses++;
                break;
            }
        }
        
        // If we didn't accept all, generate remaining with main model
        for (uint32_t i = accepted; i < config_.prefetchTokens && !generationDone_; ++i) {
            generateSingle();
        }
    }
    
    void emitToken(const Token& tok) {
        // Add to buffer
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            
            // Copy text to buffer
            size_t offset = bufferUsed_;
            if (offset + tok.text.size() < buffer_.size()) {
                std::memcpy(buffer_.data() + offset, tok.text.data(), tok.text.size());
                bufferUsed_ += tok.text.size();
            }
            
            // Store token with view into buffer
            Token stored = tok;
            stored.text = std::string_view(buffer_.data() + offset, tok.text.size());
            tokenBuffer_.push_back(stored);
            tokenIdBuffer_.push_back(tok.tokenId);
        }
        
        // First token latency
        if (firstToken_) {
            firstToken_ = false;
            auto now = std::chrono::steady_clock::now();
            metrics_.firstTokenLatency = 
                std::chrono::duration_cast<std::chrono::microseconds>(now - firstTokenTime_);
        }
        
        // Update total
        metrics_.totalTokens++;
        
        // Callback
        if (onToken_) {
            onToken_(tok);
        }
        
        // Semantic processing
        if (semanticMode_) {
            processSemanticBoundary(tok);
        }
        
        // Check for end
        // if (eos) generationDone_ = true;
    }
    
    void waitForConsumer() {
        std::unique_lock<std::mutex> lock(flowMutex_);
        
        flowCv_.wait(lock, [this]() {
            return requestedTokens_ > 0 || cancelled_.load();
        });
        
        if (requestedTokens_ != UINT32_MAX) {
            --requestedTokens_;
        }
    }
    
    // =========================================================================
    // SEMANTIC PROCESSING
    // =========================================================================
    
    void processSemanticBoundary(const Token& tok) {
        // Accumulate for sentence detection
        sentenceBuffer_ += tok.text;
        
        // Check for sentence end
        if (config_.streamBySentence && isSentenceEnd(tok)) {
            TokenFlags flags = TokenFlags::EndOfSentence;
            
            // Check for thought boundary (semantic)
            if (config_.streamByThought && isThoughtBoundary(sentenceBuffer_)) {
                flags = TokenFlags::EndOfThought;
                
                if (onThought_) {
                    float confidence = calculateThoughtConfidence(sentenceBuffer_);
                    onThought_(sentenceBuffer_, confidence);
                }
            }
            
            tokenBuffer_.back().flags |= flags;
            
            if (onSentence_) {
                onSentence_(sentenceBuffer_);
            }
            
            sentenceBuffer_.clear();
        }
        
        // Code block detection
        if (config_.streamByCodeBlock) {
            processCodeBlock(tok);
        }
    }
    
    bool isSentenceEnd(const Token& tok) const {
        // Simple heuristic - in real implementation, use NLP
        std::string_view t = tok.text;
        if (t == "." || t == "!" || t == "?" || t == "。" || t == "！" || t == "？") {
            return true;
        }
        
        // Check if token ends with sentence-ending punctuation
        if (t.size() > 0) {
            char last = t.back();
            return last == '.' || last == '!' || last == '?';
        }
        
        return false;
    }
    
    bool isThoughtBoundary(const std::string& text) const {
        // Semantic boundary detection
        // In real implementation, use:
        // - Sentence embeddings similarity
        // - Topic shift detection
        // - Discourse markers
        
        // Simple heuristic: transition words
        static const std::unordered_set<std::string> markers = {
            "however", "therefore", "furthermore", "moreover",
            "consequently", "nevertheless", "meanwhile", "finally",
            "additionally", "specifically", "importantly"
        };
        
        std::string lower;
        for (char c : text) {
            lower += std::tolower(static_cast<unsigned char>(c));
        }
        
        for (const auto& marker : markers) {
            if (lower.find(marker) != std::string::npos) {
                return true;
            }
        }
        
        return false;
    }
    
    float calculateThoughtConfidence(const std::string& text) const {
        // In real implementation, analyze coherence
        // For now, return placeholder
        return 0.85f;
    }
    
    void processCodeBlock(const Token& tok) {
        // Track code block state
        std::string_view t = tok.text;
        
        // Detect ```
        if (t == "```" || t.find("```") != std::string_view::npos) {
            inCodeBlock_ = !inCodeBlock_;
            
            if (!inCodeBlock_ && onCodeBlock_) {
                // Code block ended
                std::string language = extractLanguage(codeBlockBuffer_);
                onCodeBlock_(codeBlockBuffer_, language);
                codeBlockBuffer_.clear();
            }
        } else if (inCodeBlock_) {
            codeBlockBuffer_ += tok.text;
        }
    }
    
    std::string extractLanguage(const std::string& codeBlock) const {
        // Extract language from first line after ```
        size_t newline = codeBlock.find('\n');
        if (newline != std::string::npos && newline < 20) {
            std::string lang = codeBlock.substr(0, newline);
            // Trim
            while (!lang.empty() && std::isspace(static_cast<unsigned char>(lang.back()))) {
                lang.pop_back();
            }
            return lang;
        }
        return "";
    }
    
    // =========================================================================
    // METRICS
    // =========================================================================
    
    void updateMetrics() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - startTime_);
        
        metrics_.totalTime = elapsed;
        
        if (elapsed.count() > 0) {
            double seconds = elapsed.count() / 1000000.0;
            metrics_.tokensPerSecond = metrics_.totalTokens / seconds;
        }
        
        // Rolling average for instantaneous TPS
        updateInstantTPS();
        
        // Calculate average confidence and entropy
        if (!tokenBuffer_.empty()) {
            float confSum = 0, entropySum = 0;
            for (const auto& tok : tokenBuffer_) {
                confSum += tok.confidence;
                entropySum += tok.entropy;
            }
            metrics_.averageConfidence = confSum / tokenBuffer_.size();
            metrics_.averageEntropy = entropySum / tokenBuffer_.size();
        }
        
        // Memory tracking
        metrics_.contextBytes = bufferUsed_;
        metrics_.peakMemoryBytes = std::max(metrics_.peakMemoryBytes, 
            static_cast<uint64_t>(buffer_.size() + tokenBuffer_.capacity() * sizeof(Token)));
        
        // Metrics callback
        if (metricsCallback_ && elapsed.count() % metricsInterval_.count() < 1000) {
            metricsCallback_(metrics_);
        }
    }
    
    void updateInstantTPS() {
        auto now = std::chrono::steady_clock::now();
        
        tpsHistory_.push_back({metrics_.totalTokens, now});
        
        // Keep last 100ms
        while (tpsHistory_.size() > 2 && 
               std::chrono::duration_cast<std::chrono::milliseconds>(
                   now - tpsHistory_.front().time
               ).count() > 100) {
            tpsHistory_.pop_front();
        }
        
        if (tpsHistory_.size() >= 2) {
            auto tokens = tpsHistory_.back().tokens - tpsHistory_.front().tokens;
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                tpsHistory_.back().time - tpsHistory_.front().time
            ).count();
            
            if (ms > 0) {
                metrics_.tokensPerSecondInstant = (tokens * 1000.0) / ms;
            }
        }
    }
    
    void finalizeStream() {
        // Flush any remaining semantic buffer
        if (!sentenceBuffer_.empty() && onSentence_) {
            onSentence_(sentenceBuffer_);
        }
        
        if (!codeBlockBuffer_.empty() && onCodeBlock_) {
            onCodeBlock_(codeBlockBuffer_, extractLanguage(codeBlockBuffer_));
        }
    }
    
    // =========================================================================
    // HELPERS
    // =========================================================================
    
    uint64_t getTimestampNs() const {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
    }
    
    std::string compressContext() const {
        // In real implementation, use LZ4 or similar
        return std::string(buffer_.data(), bufferUsed_);
    }
    
    // =========================================================================
    // MEMBER VARIABLES
    // =========================================================================
    
    StreamerConfig config_;
    
    // Streaming state
    std::string prompt_;
    std::atomic<bool> streaming_{false};
    std::atomic<bool> cancelled_{false};
    std::atomic<bool> paused_{false};
    std::atomic<bool> generationDone_{false};
    
    // Buffers
    std::vector<char> buffer_;
    uint32_t bufferUsed_ = 0;
    std::vector<Token> tokenBuffer_;
    std::vector<uint32_t> tokenIdBuffer_;
    
    // Semantic processing
    bool semanticMode_ = false;
    std::string sentenceBuffer_;
    std::string codeBlockBuffer_;
    bool inCodeBlock_ = false;
    
    // Speculative decoding
    void* draftModel_ = nullptr;
    bool speculativeEnabled_ = false;
    
    // Time-travel
    std::deque<Checkpoint> checkpoints_;
    uint64_t nextCheckpointId_ = 1;
    
    // Flow control
    uint32_t requestedTokens_ = UINT32_MAX;
    
    // Metrics
    StreamMetrics metrics_;
    std::chrono::steady_clock::time_point startTime_;
    std::chrono::steady_clock::time_point firstTokenTime_;
    bool firstToken_ = true;
    
    struct TPSHistory {
        uint64_t tokens;
        std::chrono::steady_clock::time_point time;
    };
    std::deque<TPSHistory> tpsHistory_;
    
    // Callbacks
    TokenCallback onToken_;
    SentenceCallback onSentence_;
    ThoughtCallback onThought_;
    CodeBlockCallback onCodeBlock_;
    MetricsCallback metricsCallback_;
    ErrorCallback onError_;
    CheckpointCallback onCheckpoint_;
    std::function<void()> onComplete_;
    std::chrono::milliseconds metricsInterval_{100};
    
    // Threading
    std::thread streamThread_;
    mutable std::mutex streamMutex_;
    mutable std::mutex stateMutex_;
    mutable std::mutex flowMutex_;
    mutable std::mutex metricsMutex_;
    std::condition_variable flowCv_;
    std::condition_variable streamCv_;
    
    friend class TokenIterator;
};

// =============================================================================
// FACTORY
// =============================================================================

std::unique_ptr<MaximusStreamer> MaximusStreamer::create(StreamerConfig config) {
    return std::make_unique<MaximusStreamerImpl>(std::move(config));
}

// =============================================================================
// ITERATOR CONSTRUCTOR/DESTRUCTOR
// =============================================================================

MaximusStreamer::TokenIterator::TokenIterator() = default;
MaximusStreamer::TokenIterator::~TokenIterator() = default;
MaximusStreamer::TokenIterator::TokenIterator(TokenIterator&&) noexcept = default;
MaximusStreamer::TokenIterator& MaximusStreamer::TokenIterator::operator=(TokenIterator&&) noexcept = default;

// =============================================================================
// ITERATOR IMPLEMENTATION
// =============================================================================

struct MaximusStreamer::TokenIterator::Impl {
    MaximusStreamerImpl* streamer;
    bool isEnd;
    uint32_t position;
    Token currentToken;
    
    Impl(MaximusStreamerImpl* s, bool end = false)
        : streamer(s), isEnd(end), position(0) {}
};

Token MaximusStreamer::TokenIterator::operator*() const {
    return impl_->currentToken;
}

MaximusStreamer::TokenIterator& MaximusStreamer::TokenIterator::operator++() {
    impl_->position++;
    
    // Wait for next token
    while (impl_->streamer->tokenBuffer_.size() <= impl_->position &&
           impl_->streamer->streaming_.load()) {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    
    if (impl_->position < impl_->streamer->tokenBuffer_.size()) {
        impl_->currentToken = impl_->streamer->tokenBuffer_[impl_->position];
    } else {
        impl_->isEnd = true;
    }
    
    return *this;
}

bool MaximusStreamer::TokenIterator::operator!=(const TokenIterator& other) const {
    return impl_->isEnd != other.impl_->isEnd;
}

bool MaximusStreamer::TokenIterator::done() const {
    return impl_->isEnd;
}

// =============================================================================
// ADVANCED FEATURES IMPLEMENTATION
// =============================================================================

SemanticDiff semanticDiff(std::string_view text1, std::string_view text2) {
    SemanticDiff diff;
    
    // In real implementation:
    // 1. Extract thoughts/sentences from both texts
    // 2. Match semantically using embeddings
    // 3. Compute diff
    
    // Placeholder
    diff.similarity = 0.5f;
    
    return diff;
}

Provenance getTokenProvenance(const MaximusStreamer& streamer, uint32_t position) {
    Provenance prov;
    prov.tokenPosition = position;
    
    // In real implementation, extract attention weights from model
    // Placeholder
    prov.source = "generated";
    
    return prov;
}

std::string ConfidenceHeatmap::renderAscii(uint32_t width) const {
    std::string result;
    
    if (confidenceByPosition.empty()) return result;
    
    // Group into width buckets
    uint32_t bucketSize = std::max(1u, static_cast<uint32_t>(confidenceByPosition.size()) / width);
    
    for (size_t i = 0; i < confidenceByPosition.size(); i += bucketSize) {
        float sum = 0;
        uint32_t count = 0;
        
        for (size_t j = i; j < std::min(i + bucketSize, confidenceByPosition.size()); ++j) {
            sum += confidenceByPosition[j];
            ++count;
        }
        
        float avg = sum / count;
        
        // Map to ASCII
        char c;
        if (avg > 0.95f) c = '█';
        else if (avg > 0.9f) c = '▓';
        else if (avg > 0.8f) c = '▒';
        else if (avg > 0.7f) c = '░';
        else if (avg > 0.5f) c = '·';
        else c = ' ';
        
        result += c;
    }
    
    return result;
}

} // namespace Maximus
