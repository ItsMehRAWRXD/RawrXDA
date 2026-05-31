// ============================================================================
// stream_transformer.hpp — Deterministic Stream Transformation Layer
// ============================================================================
// A programmable stream reshaper and analyzer with introspection and pacing
// control. NOT a model controller — this post-processes chunks already emitted
// by the backend.
//
// Addresses 7 critical issues from previous design:
//   1. Real token alignment via byte offsets (not naive splitting)
//   2. No artificial latency — only delays when tokens are TOO fast
//   3. Robust SSE parsing with incremental buffer and \n\n boundaries
//   4. Buffer remainder — keeps unparsed fragments
//   5. Correct naming: StreamTransformer (not SingleTokenController)
//   6. Dual timing model: arrivalLatency / processingDelay / effectiveLatency
//   7. Chunk-size detection (not model batching detection)
//
// Pattern: Pull-based stream adapter with stateful buffering.
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <deque>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <atomic>
#include <mutex>
#include <memory>
#include <optional>
#include <variant>
#include <algorithm>
#include <cctype>
#include <cstring>

namespace RawrXD {
namespace Streaming {

// ============================================================================
// Timing Model — Three independent latency tracks
// ============================================================================

struct TokenTiming {
    // When the chunk arrived from the network
    std::chrono::steady_clock::time_point arrivalTime;
    // How long we intentionally delayed before yielding
    std::chrono::milliseconds processingDelay{0};
    // What the user actually perceived (arrival + delay)
    std::chrono::milliseconds effectiveLatency{0};
    // Time since previous token's arrival (raw network timing)
    std::chrono::milliseconds interArrival{0};
};

// ============================================================================
// Semantic Chunk Types — Emits words/phrases/sentences, not pseudo-tokens
// ============================================================================

enum class ChunkType : uint8_t {
    WORD,        // e.g. "hello", "world"
    WHITESPACE,  // " ", "\t", "\n"
    PUNCTUATION, // ".", ",", "!"
    NEWLINE,     // "\n" or "\r\n"
    CODE_BLOCK,  // ``` fenced code
    MARKDOWN,    // **bold**, *italic*
    NUMBER,      // "42", "3.14"
    URL,         // "https://..."
    UNKNOWN
};

struct SemanticChunk {
    std::string text;
    ChunkType type = ChunkType::UNKNOWN;
    size_t byteOffset = 0;      // Position in original stream
    size_t byteLength = 0;      // Length in bytes
    TokenTiming timing;
    // If we have real tokenizer alignment, these are populated
    std::optional<std::vector<uint32_t>> tokenIds;
    std::optional<size_t> tokenCount;
};

// ============================================================================
// Stream Transformer Configuration
// ============================================================================

struct StreamTransformerConfig {
    // Pacing control
    float targetMinInterTokenMs = 5.0f;   // Only delay if faster than this
    float targetMaxInterTokenMs = 80.0f;  // Warn if slower than this
    float adaptiveTargetTPS = 20.0f;      // Target tokens per second

    // Chunking strategy
    bool emitSemanticChunks = true;       // Words/phrases vs raw characters
    bool preserveCodeBlocks = true;       // Keep ``` blocks intact
    bool preserveMarkdown = true;         // Keep **bold** etc intact
    size_t maxChunkLength = 64;           // Max chars per chunk

    // SSE parsing
    size_t maxBufferSize = 65536;         // 64KB max buffer before flush
    bool strictSseParsing = true;         // Require "data: " prefix

    // Backpressure
    bool enableBackpressure = true;       // Pause reader when UI saturated
    size_t maxPendingChunks = 100;        // Buffer before backpressure

    // Tokenizer alignment (optional — requires tiktoken or similar)
    std::string tokenizerModel = "";      // e.g. "cl100k_base" — empty = disabled
};

// ============================================================================
// SSE Event — Proper incremental parsing result
// ============================================================================

struct SseEvent {
    std::string data;           // The data payload
    std::string event;          // Event type (optional)
    std::string id;             // Event ID (optional)
    bool isDone = false;        // "[DONE]" marker
    bool isValid = false;       // Successfully parsed
};

// ============================================================================
// Stream State — Observable introspection
// ============================================================================

struct StreamState {
    std::string streamId;
    size_t totalChunks = 0;
    size_t totalBytes = 0;
    float averageInterArrivalMs = 0.0f;
    float currentTPS = 0.0f;
    bool isStalled = false;
    bool backpressureActive = false;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point lastChunkTime;

    // Chunk type distribution
    std::unordered_map<ChunkType, size_t> chunkTypeCounts;
};

// ============================================================================
// Stream Transformer — Core Class
// ============================================================================

class StreamTransformer {
public:
    using ChunkCallback = std::function<void(const SemanticChunk&)>;
    using StallCallback = std::function<void(const StreamState&)>;
    using ResumeCallback = std::function<void(const StreamState&)>;
    using BackpressureCallback = std::function<void(bool active)>;

    explicit StreamTransformer(const StreamTransformerConfig& config = {});
    ~StreamTransformer();

    // ---- Main API: Transform a raw SSE stream into semantic chunks ----

    // Push raw bytes from the network stream. Call repeatedly as chunks arrive.
    // Returns semantic chunks that are ready to emit.
    std::vector<SemanticChunk> pushBytes(const uint8_t* data, size_t len);

    // Push a complete string (convenience wrapper)
    std::vector<SemanticChunk> pushString(const std::string& str);

    // Signal end of stream. Flushes remaining buffer.
    std::vector<SemanticChunk> endStream();

    // ---- Pacing Control ----

    // Call this before yielding each chunk to the UI.
    // Returns the recommended delay in milliseconds (0 = no delay needed).
    // Only delays when tokens arrive FASTER than targetMinInterTokenMs.
    float getRecommendedDelayMs() const;

    // Apply the recommended delay (blocking). Use in UI thread.
    void applyDelay();

    // ---- Backpressure ----

    // Call when UI has consumed a chunk
    void markChunkConsumed();

    // Check if backpressure is active (reader should pause)
    bool isBackpressureActive() const;

    // ---- State Queries ----

    const StreamState& getState() const { return state_; }
    bool isStreamActive() const { return streamActive_; }

    // ---- Event Subscriptions ----

    void onChunk(ChunkCallback cb);
    void onStall(StallCallback cb);
    void onResume(ResumeCallback cb);
    void onBackpressure(BackpressureCallback cb);

    // ---- Control ----

    void reset();
    void shutdown();

private:
    StreamTransformerConfig config_;
    StreamState state_;
    std::atomic<bool> streamActive_{false};
    std::atomic<bool> shutdown_{false};

    // SSE parsing buffer — incremental, keeps unparsed fragments
    std::string sseBuffer_;
    bool sseBufferHasPartialLine_ = false;

    // Text accumulation buffer for semantic chunking
    std::string textBuffer_;

    // Pending chunks (for backpressure)
    std::deque<SemanticChunk> pendingChunks_;
    std::atomic<size_t> pendingCount_{0};

    // Timing
    std::chrono::steady_clock::time_point lastArrivalTime_;
    std::chrono::steady_clock::time_point lastYieldTime_;
    std::deque<float> recentInterArrivals_;  // For TPS calculation

    // Callbacks
    mutable std::mutex callbackMutex_;
    std::vector<ChunkCallback> chunkCallbacks_;
    std::vector<StallCallback> stallCallbacks_;
    std::vector<ResumeCallback> resumeCallbacks_;
    std::vector<BackpressureCallback> backpressureCallbacks_;

    // ---- SSE Parsing ----

    std::vector<SseEvent> parseSseEvents();
    std::optional<SseEvent> parseSseEvent(const std::string& block);
    std::string extractDeltaContent(const std::string& data);

    // ---- Semantic Chunking ----

    std::vector<SemanticChunk> chunkText(const std::string& text, size_t baseOffset);
    ChunkType classifyChunk(const std::string& text) const;
    bool isCodeBlockBoundary(const std::string& text) const;
    bool isMarkdownBoundary(const std::string& text) const;

    // ---- Timing ----

    void recordArrival();
    void updateTiming(SemanticChunk& chunk);
    float calculateCurrentTPS() const;
    void checkStall();

    // ---- Backpressure ----

    void updateBackpressure();

    // ---- Event Dispatch ----

    void dispatchChunk(const SemanticChunk& chunk);
    void dispatchStall();
    void dispatchResume();
    void dispatchBackpressure(bool active);
};

// ============================================================================
// Inline Implementation
// ============================================================================

inline StreamTransformer::StreamTransformer(const StreamTransformerConfig& config)
    : config_(config)
{
    state_.startTime = std::chrono::steady_clock::now();
    state_.lastChunkTime = state_.startTime;
    lastArrivalTime_ = state_.startTime;
    lastYieldTime_ = state_.startTime;
}

inline StreamTransformer::~StreamTransformer() {
    shutdown();
}

inline std::vector<SemanticChunk> StreamTransformer::pushBytes(
    const uint8_t* data, size_t len)
{
    if (shutdown_.load() || !data || len == 0) {
        return {};
    }

    streamActive_ = true;
    recordArrival();

    // Append to SSE buffer (Issue #4: keep remainder)
    sseBuffer_.append(reinterpret_cast<const char*>(data), len);

    // Check buffer size limit
    if (sseBuffer_.size() > config_.maxBufferSize) {
        // Force flush to prevent unbounded growth
        auto chunks = endStream();
        sseBuffer_.clear();
        return chunks;
    }

    // Parse SSE events (Issue #3: robust incremental parsing)
    auto events = parseSseEvents();

    std::vector<SemanticChunk> allChunks;

    for (const auto& event : events) {
        if (!event.isValid) continue;
        if (event.isDone) {
            streamActive_ = false;
            continue;
        }

        // Extract content from delta
        std::string content = extractDeltaContent(event.data);
        if (content.empty()) continue;

        // Accumulate text for semantic chunking
        textBuffer_ += content;

        // Issue #5: Semantic chunking (words/phrases/sentences)
        if (config_.emitSemanticChunks) {
            auto chunks = chunkText(textBuffer_, state_.totalBytes);
            if (!chunks.empty()) {
                // Keep remainder in buffer
                size_t consumed = 0;
                for (const auto& c : chunks) {
                    consumed = std::max(consumed, c.byteOffset + c.byteLength);
                }
                if (consumed > 0 && consumed <= textBuffer_.size()) {
                    textBuffer_ = textBuffer_.substr(consumed);
                }

                for (auto& chunk : chunks) {
                    updateTiming(chunk);
                    state_.totalChunks++;
                    state_.totalBytes += chunk.byteLength;
                    state_.chunkTypeCounts[chunk.type]++;
                    allChunks.push_back(chunk);
                    dispatchChunk(chunk);
                }
            }
        } else {
            // Raw character mode
            SemanticChunk chunk;
            chunk.text = content;
            chunk.type = ChunkType::UNKNOWN;
            chunk.byteOffset = state_.totalBytes;
            chunk.byteLength = content.size();
            updateTiming(chunk);
            state_.totalChunks++;
            state_.totalBytes += content.size();
            allChunks.push_back(chunk);
            dispatchChunk(chunk);
        }
    }

    // Check stall BEFORE updating lastChunkTime
    checkStall();

    // Update state
    state_.currentTPS = calculateCurrentTPS();
    state_.lastChunkTime = std::chrono::steady_clock::now();

    // Update backpressure
    pendingCount_ += allChunks.size();
    updateBackpressure();

    return allChunks;
}

inline std::vector<SemanticChunk> StreamTransformer::pushString(
    const std::string& str)
{
    return pushBytes(reinterpret_cast<const uint8_t*>(str.data()), str.size());
}

inline std::vector<SemanticChunk> StreamTransformer::endStream()
{
    std::vector<SemanticChunk> finalChunks;

    // Flush remaining text buffer
    if (!textBuffer_.empty()) {
        auto chunks = chunkText(textBuffer_, state_.totalBytes);
        if (chunks.empty()) {
            // Force flush as raw text (e.g., incomplete code block)
            SemanticChunk chunk;
            chunk.text = textBuffer_;
            chunk.type = ChunkType::UNKNOWN;
            chunk.byteOffset = state_.totalBytes;
            chunk.byteLength = textBuffer_.size();
            updateTiming(chunk);
            state_.totalChunks++;
            state_.totalBytes += textBuffer_.size();
            finalChunks.push_back(chunk);
            dispatchChunk(chunk);
        } else {
            for (auto& chunk : chunks) {
                updateTiming(chunk);
                state_.totalChunks++;
                state_.totalBytes += chunk.byteLength;
                finalChunks.push_back(chunk);
                dispatchChunk(chunk);
            }
        }
        textBuffer_.clear();
    }

    // Flush any remaining SSE buffer
    if (!sseBuffer_.empty()) {
        auto events = parseSseEvents();
        for (const auto& event : events) {
            if (event.isValid && !event.isDone) {
                std::string content = extractDeltaContent(event.data);
                if (!content.empty()) {
                    SemanticChunk chunk;
                    chunk.text = content;
                    chunk.type = ChunkType::UNKNOWN;
                    chunk.byteOffset = state_.totalBytes;
                    chunk.byteLength = content.size();
                    updateTiming(chunk);
                    state_.totalChunks++;
                    state_.totalBytes += content.size();
                    finalChunks.push_back(chunk);
                    dispatchChunk(chunk);
                }
            }
        }
        sseBuffer_.clear();
    }

    streamActive_ = false;
    return finalChunks;
}

// ============================================================================
// SSE Parsing — Robust incremental parsing with \n\n boundaries
// ============================================================================

inline std::vector<SseEvent> StreamTransformer::parseSseEvents()
{
    std::vector<SseEvent> events;

    // Issue #3: Find \n\n boundaries (SSE spec)
    size_t pos = 0;
    while (true) {
        size_t boundary = sseBuffer_.find("\n\n", pos);
        if (boundary == std::string::npos) {
            // Keep remainder for next call
            if (pos > 0) {
                sseBuffer_ = sseBuffer_.substr(pos);
            }
            break;
        }

        std::string block = sseBuffer_.substr(pos, boundary - pos);
        pos = boundary + 2;

        auto event = parseSseEvent(block);
        if (event && event->isValid) {
            events.push_back(*event);
        }
    }

    return events;
}

inline std::optional<SseEvent> StreamTransformer::parseSseEvent(
    const std::string& block)
{
    SseEvent event;
    event.isValid = true;

    std::istringstream stream(block);
    std::string line;

    while (std::getline(stream, line)) {
        // Remove \r if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty()) continue;

        if (line.substr(0, 6) == "data: ") {
            if (!event.data.empty()) {
                event.data += "\n";
            }
            event.data += line.substr(6);
        } else if (line.substr(0, 7) == "event: ") {
            event.event = line.substr(7);
        } else if (line.substr(0, 4) == "id: ") {
            event.id = line.substr(4);
        }
    }

    if (event.data == "[DONE]") {
        event.isDone = true;
    }

    // Strict mode: require data prefix
    if (config_.strictSseParsing && event.data.empty() && !event.isDone) {
        event.isValid = false;
    }

    return event;
}

inline std::string StreamTransformer::extractDeltaContent(
    const std::string& data)
{
    // Parse JSON: {"choices":[{"delta":{"content":"..."}}]}
    // Simple extraction — in production use a proper JSON parser
    size_t contentPos = data.find("\"content\":\"");
    if (contentPos == std::string::npos) {
        // Try alternate format
        contentPos = data.find("\"content\": \"");
        if (contentPos == std::string::npos) {
            return data; // Return raw if no JSON structure
        }
        contentPos += 11;
    } else {
        contentPos += 11;
    }

    size_t endPos = data.find("\"", contentPos);
    if (endPos == std::string::npos) {
        return data.substr(contentPos);
    }

    std::string content = data.substr(contentPos, endPos - contentPos);

    // Unescape
    std::string result;
    result.reserve(content.size());
    for (size_t i = 0; i < content.size(); ++i) {
        if (content[i] == '\\' && i + 1 < content.size()) {
            switch (content[i + 1]) {
                case 'n': result += '\n'; ++i; break;
                case 't': result += '\t'; ++i; break;
                case 'r': result += '\r'; ++i; break;
                case '\\': result += '\\'; ++i; break;
                case '"': result += '"'; ++i; break;
                default: result += content[i]; break;
            }
        } else {
            result += content[i];
        }
    }

    return result;
}

// ============================================================================
// Semantic Chunking — Words, phrases, sentences (not pseudo-tokens)
// ============================================================================

inline std::vector<SemanticChunk> StreamTransformer::chunkText(
    const std::string& text, size_t baseOffset)
{
    std::vector<SemanticChunk> chunks;

    if (text.empty()) return chunks;

    size_t i = 0;
    size_t currentOffset = baseOffset;

    while (i < text.size()) {
        SemanticChunk chunk;
        chunk.byteOffset = currentOffset;

        // Check for code block boundary
        if (config_.preserveCodeBlocks && isCodeBlockBoundary(text.substr(i))) {
            size_t end = text.find("```", i + 3);
            if (end == std::string::npos) {
                // Incomplete code block — keep in buffer
                break;
            }
            end += 3;
            chunk.text = text.substr(i, end - i);
            chunk.type = ChunkType::CODE_BLOCK;
            chunk.byteLength = chunk.text.size();
            chunks.push_back(chunk);
            i = end;
            currentOffset += chunk.byteLength;
            continue;
        }

        // Check for markdown
        if (config_.preserveMarkdown && isMarkdownBoundary(text.substr(i))) {
            // Find end of markdown span
            size_t end = i + 1;
            while (end < text.size() && !std::isspace(static_cast<unsigned char>(text[end]))) {
                ++end;
            }
            chunk.text = text.substr(i, end - i);
            chunk.type = ChunkType::MARKDOWN;
            chunk.byteLength = chunk.text.size();
            chunks.push_back(chunk);
            i = end;
            currentOffset += chunk.byteLength;
            continue;
        }

        // Check for URL
        if (text.substr(i, 7) == "http://" || text.substr(i, 8) == "https://") {
            size_t end = i;
            while (end < text.size() && !std::isspace(static_cast<unsigned char>(text[end]))) {
                ++end;
            }
            chunk.text = text.substr(i, end - i);
            chunk.type = ChunkType::URL;
            chunk.byteLength = chunk.text.size();
            chunks.push_back(chunk);
            i = end;
            currentOffset += chunk.byteLength;
            continue;
        }

        // Check for number
        if (std::isdigit(static_cast<unsigned char>(text[i]))) {
            size_t end = i;
            while (end < text.size() && (std::isdigit(static_cast<unsigned char>(text[end])) ||
                   text[end] == '.' || text[end] == ',')) {
                ++end;
            }
            chunk.text = text.substr(i, end - i);
            chunk.type = ChunkType::NUMBER;
            chunk.byteLength = chunk.text.size();
            chunks.push_back(chunk);
            i = end;
            currentOffset += chunk.byteLength;
            continue;
        }

        // Whitespace
        if (std::isspace(static_cast<unsigned char>(text[i]))) {
            size_t end = i;
            while (end < text.size() && std::isspace(static_cast<unsigned char>(text[end]))) {
                ++end;
            }
            chunk.text = text.substr(i, end - i);
            chunk.type = (chunk.text.find('\n') != std::string::npos) ?
                         ChunkType::NEWLINE : ChunkType::WHITESPACE;
            chunk.byteLength = chunk.text.size();
            chunks.push_back(chunk);
            i = end;
            currentOffset += chunk.byteLength;
            continue;
        }

        // Punctuation
        if (std::ispunct(static_cast<unsigned char>(text[i]))) {
            chunk.text = text.substr(i, 1);
            chunk.type = ChunkType::PUNCTUATION;
            chunk.byteLength = 1;
            chunks.push_back(chunk);
            i++;
            currentOffset++;
            continue;
        }

        // Word — accumulate until word boundary
        size_t end = i;
        while (end < text.size() && !std::isspace(static_cast<unsigned char>(text[end])) &&
               !std::ispunct(static_cast<unsigned char>(text[end]))) {
            ++end;
        }

        chunk.text = text.substr(i, end - i);
        chunk.type = ChunkType::WORD;
        chunk.byteLength = chunk.text.size();

        // Check max length — split long words
        if (chunk.text.size() > config_.maxChunkLength) {
            size_t splitPos = config_.maxChunkLength;
            chunk.text = text.substr(i, splitPos);
            chunk.byteLength = chunk.text.size();
            chunks.push_back(chunk);
            i += splitPos;
            currentOffset += splitPos;
            continue;
        }

        chunks.push_back(chunk);
        i = end;
        currentOffset += chunk.byteLength;
    }

    return chunks;
}

inline ChunkType StreamTransformer::classifyChunk(const std::string& text) const
{
    if (text.empty()) return ChunkType::UNKNOWN;
    if (text == "\n" || text == "\r\n") return ChunkType::NEWLINE;
    if (text.find("```") == 0) return ChunkType::CODE_BLOCK;
    if (text.find("http://") == 0 || text.find("https://") == 0) return ChunkType::URL;
    if (std::all_of(text.begin(), text.end(),
                    [](char c) { return std::isspace(static_cast<unsigned char>(c)); }))
        return ChunkType::WHITESPACE;
    if (std::all_of(text.begin(), text.end(),
                    [](char c) { return std::ispunct(static_cast<unsigned char>(c)); }))
        return ChunkType::PUNCTUATION;
    if (std::all_of(text.begin(), text.end(),
                    [](char c) { return std::isdigit(static_cast<unsigned char>(c)) ||
                                        c == '.' || c == ','; }))
        return ChunkType::NUMBER;
    return ChunkType::WORD;
}

inline bool StreamTransformer::isCodeBlockBoundary(const std::string& text) const
{
    return text.size() >= 3 && text.substr(0, 3) == "```";
}

inline bool StreamTransformer::isMarkdownBoundary(const std::string& text) const
{
    return text.size() >= 2 &&
           ((text[0] == '*' && text[1] == '*') ||
            (text[0] == '_' && text[1] == '_') ||
            (text[0] == '`' && text[1] == '`'));
}

// ============================================================================
// Timing — Dual model: arrival / processing / effective
// ============================================================================

inline void StreamTransformer::recordArrival()
{
    auto now = std::chrono::steady_clock::now();

    if (state_.totalChunks > 0) {
        auto interArrival = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastArrivalTime_).count();
        recentInterArrivals_.push_back(static_cast<float>(interArrival));

        // Keep last 20 samples
        while (recentInterArrivals_.size() > 20) {
            recentInterArrivals_.pop_front();
        }
    }

    lastArrivalTime_ = now;
}

inline void StreamTransformer::updateTiming(SemanticChunk& chunk)
{
    chunk.timing.arrivalTime = lastArrivalTime_;

    // Calculate inter-arrival
    if (state_.totalChunks > 0 && !recentInterArrivals_.empty()) {
        chunk.timing.interArrival = std::chrono::milliseconds(
            static_cast<long>(recentInterArrivals_.back()));
    }

    // Issue #2: Only delay if tokens are TOO fast
    float interArrivalMs = chunk.timing.interArrival.count();
    if (interArrivalMs < config_.targetMinInterTokenMs && interArrivalMs > 0) {
        // Token arrived faster than minimum — add delay for smooth UX
        chunk.timing.processingDelay = std::chrono::milliseconds(
            static_cast<long>(config_.targetMinInterTokenMs - interArrivalMs));
    } else {
        chunk.timing.processingDelay = std::chrono::milliseconds(0);
    }

    // Effective latency = what user perceives
    chunk.timing.effectiveLatency = chunk.timing.interArrival + chunk.timing.processingDelay;
}

inline float StreamTransformer::calculateCurrentTPS() const
{
    if (recentInterArrivals_.empty()) return 0.0f;

    float avgInterArrival = 0.0f;
    for (float ms : recentInterArrivals_) {
        avgInterArrival += ms;
    }
    avgInterArrival /= recentInterArrivals_.size();

    if (avgInterArrival <= 0.0f) return 0.0f;

    return 1000.0f / avgInterArrival;
}

inline void StreamTransformer::checkStall()
{
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - state_.lastChunkTime).count();

    bool shouldBeStalled = (elapsed > config_.targetMaxInterTokenMs * 2);

    if (shouldBeStalled && !state_.isStalled) {
        state_.isStalled = true;
        dispatchStall();
    } else if (!shouldBeStalled && state_.isStalled) {
        state_.isStalled = false;
        dispatchResume();
    }
}

// ============================================================================
// Pacing — Only delay when tokens are too fast
// ============================================================================

inline float StreamTransformer::getRecommendedDelayMs() const
{
    if (recentInterArrivals_.empty()) return 0.0f;

    float lastInterArrival = recentInterArrivals_.back();

    // Issue #2: Only delay if faster than target minimum
    if (lastInterArrival < config_.targetMinInterTokenMs) {
        return config_.targetMinInterTokenMs - lastInterArrival;
    }

    return 0.0f;
}

inline void StreamTransformer::applyDelay()
{
    float delayMs = getRecommendedDelayMs();
    if (delayMs > 0.0f) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(static_cast<long>(delayMs)));
    }
}

// ============================================================================
// Backpressure — Pause reader when UI saturated
// ============================================================================

inline void StreamTransformer::markChunkConsumed()
{
    if (pendingCount_.load() > 0) {
        pendingCount_.fetch_sub(1);
    }
    updateBackpressure();
}

inline bool StreamTransformer::isBackpressureActive() const
{
    return state_.backpressureActive;
}

inline void StreamTransformer::updateBackpressure()
{
    bool shouldBeActive = config_.enableBackpressure &&
                          pendingCount_.load() >= config_.maxPendingChunks;

    if (shouldBeActive != state_.backpressureActive) {
        state_.backpressureActive = shouldBeActive;
        dispatchBackpressure(shouldBeActive);
    }
}

// ============================================================================
// Event Subscriptions
// ============================================================================

inline void StreamTransformer::onChunk(ChunkCallback cb) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    chunkCallbacks_.push_back(std::move(cb));
}

inline void StreamTransformer::onStall(StallCallback cb) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    stallCallbacks_.push_back(std::move(cb));
}

inline void StreamTransformer::onResume(ResumeCallback cb) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    resumeCallbacks_.push_back(std::move(cb));
}

inline void StreamTransformer::onBackpressure(BackpressureCallback cb) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    backpressureCallbacks_.push_back(std::move(cb));
}

// ============================================================================
// Event Dispatch
// ============================================================================

inline void StreamTransformer::dispatchChunk(const SemanticChunk& chunk) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    for (auto& cb : chunkCallbacks_) {
        cb(chunk);
    }
}

inline void StreamTransformer::dispatchStall() {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    for (auto& cb : stallCallbacks_) {
        cb(state_);
    }
}

inline void StreamTransformer::dispatchResume() {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    for (auto& cb : resumeCallbacks_) {
        cb(state_);
    }
}

inline void StreamTransformer::dispatchBackpressure(bool active) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    for (auto& cb : backpressureCallbacks_) {
        cb(active);
    }
}

// ============================================================================
// Control
// ============================================================================

inline void StreamTransformer::reset()
{
    sseBuffer_.clear();
    textBuffer_.clear();
    pendingChunks_.clear();
    pendingCount_ = 0;
    recentInterArrivals_.clear();
    state_ = StreamState{};
    state_.startTime = std::chrono::steady_clock::now();
    state_.lastChunkTime = state_.startTime;
    lastArrivalTime_ = state_.startTime;
    lastYieldTime_ = state_.startTime;
    streamActive_ = false;
    state_.backpressureActive = false;
}

inline void StreamTransformer::shutdown()
{
    shutdown_ = true;
    streamActive_ = false;
}

} // namespace Streaming
} // namespace RawrXD
