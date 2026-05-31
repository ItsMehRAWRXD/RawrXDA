// ============================================================================
// token_stream_fast_forward_bridge.cpp
// Integration: ProductionTokenStreamHandler + FastForwardController
// ============================================================================
// Bridges the production-hardened token stream (AI_TokenStream.hpp) with the
// FastForward skip-ahead controller (fast_forward_controller.h).
//
// Responsibilities:
//   1. Feed tokens from stream → FF controller for TPS-aware skipping
//   2. Enforce TLS deadlines by cancelling slow streams
//   3. Forward kept tokens to UI/editor with latency metrics
//   4. Handle backpressure from UI by pausing HTTP reads
// ============================================================================

#include "token_stream_fast_forward_bridge.h"
#include <algorithm>

namespace RawrXD {
namespace UI {

// ============================================================================
// Construction / Destruction
// ============================================================================

TokenStreamFFBridge::TokenStreamFFBridge(
    std::shared_ptr<FastForwardController> ffController,
    const BridgeConfig& config)
    : ffController_(std::move(ffController))
    , config_(config)
    , httpReader_(nullptr)
    , running_(false)
    , totalTokensKept_(0)
    , totalTokensSkipped_(0)
{
    // Configure stream handler from bridge config
    rawrxd::aistream::ProductionTokenStreamHandler::Config shConfig;
    shConfig.enforce_single_token = config_.enforceSingleToken;
    shConfig.token_timeout_ms = config_.tokenTimeoutMs;
    shConfig.backpressure_high = config_.backpressureHigh;
    shConfig.backpressure_low = config_.backpressureLow;
    shConfig.stall_detection_ms = config_.stallDetectionMs;

    streamHandler_ = std::make_unique<rawrxd::aistream::ProductionTokenStreamHandler>(shConfig);
}

TokenStreamFFBridge::~TokenStreamFFBridge() {
    shutdown();
}

// ============================================================================
// Stream Lifecycle
// ============================================================================

bool TokenStreamFFBridge::startStream(
    const std::string& messageId,
    const std::string& userPrompt,
    const APIConfig& apiConfig,
    TokenKeptCallback onTokenKept,
    StreamCompleteCallback onComplete,
    StreamErrorCallback onError)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (running_) {
        if (onError) onError("Bridge already has an active stream");
        return false;
    }

    messageId_ = messageId;
    onTokenKept_ = onTokenKept;
    onComplete_ = onComplete;
    onError_ = onError;
    totalTokensKept_ = 0;
    totalTokensSkipped_ = 0;
    running_ = true;

    // Reset stream handler
    streamHandler_->reset(messageId);

    // Set up stream handler callbacks
    streamHandler_->onToken([this](const rawrxd::aistream::TokenInfo& token,
                                   const rawrxd::aistream::StreamState& state) {
        handleToken(token, state);
    });

    streamHandler_->onComplete([this](const rawrxd::aistream::StreamState& state) {
        handleStreamComplete(state);
    });

    streamHandler_->onStall([this](const rawrxd::aistream::StreamState& state) {
        handleStreamStall(state);
    });

    streamHandler_->start();

    // Build HTTP reader
    rawrxd::aistream::AsyncHTTPStreamReader::Config httpConfig;
    httpConfig.host = apiConfig.host;
    httpConfig.port = apiConfig.port;
    httpConfig.path = apiConfig.path;
    httpConfig.request_body = buildRequestBody(userPrompt, apiConfig);
    httpConfig.timeout_ms = apiConfig.timeoutMs;
    httpConfig.connect_timeout_ms = apiConfig.connectTimeoutMs;

    httpReader_ = std::make_unique<rawrxd::aistream::AsyncHTTPStreamReader>(httpConfig);

    bool started = httpReader_->start(
        // Data callback with backpressure
        [this](const uint8_t* data, size_t len) -> bool {
            return handleHTTPData(data, len);
        },
        // Error callback
        [this](const std::string& error) {
            handleHTTPError(error);
        },
        // Complete callback
        [this]() {
            handleHTTPComplete();
        }
    );

    if (!started) {
        running_ = false;
        if (onError) onError("Failed to start HTTP stream");
        return false;
    }

    // Initiate FastForward for this message if auto-FF enabled
    if (ffController_ && config_.enableAutoFF) {
        try {
            QueuedMessage qmsg;
            qmsg.id = messageId;
            qmsg.progress = 0; // Will be updated as tokens arrive
            ffController_->initiateFF(qmsg, "auto_tps_monitoring");
        } catch (...) {
            // FF not critical for stream operation
        }
    }

    return true;
}

void TokenStreamFFBridge::cancelStream(const std::string& reason) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!running_) return;

    running_ = false;

    if (httpReader_) {
        httpReader_->cancel();
    }

    if (streamHandler_) {
        streamHandler_->shutdown();
    }

    if (ffController_ && !messageId_.empty()) {
        ffController_->cancelFF(messageId_, reason);
    }

    if (onError_) {
        onError_("Stream cancelled: " + reason);
    }
}

void TokenStreamFFBridge::shutdown() {
    cancelStream("shutdown");
}

// ============================================================================
// Token Flow: Stream → FF → UI
// ============================================================================

void TokenStreamFFBridge::handleToken(
    const rawrxd::aistream::TokenInfo& token,
    const rawrxd::aistream::StreamState& state)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!running_) return;

    // Update FF controller progress
    if (ffController_ && !messageId_.empty()) {
        ffController_->updateProgress(messageId_, 1);
    }

    // Check if FF wants us to skip this token
    bool keep = true;
    if (ffController_ && !messageId_.empty()) {
        keep = ffController_->shouldKeepToken(messageId_, token.index);
    }

    if (keep) {
        ++totalTokensKept_;

        // Build enriched token info with FF state
        EnrichedTokenInfo enriched;
        enriched.token = token;
        enriched.streamTotalTokens = state.total_tokens;
        enriched.streamAverageLatencyMs = state.average_latency_ms;
        enriched.streamMode = static_cast<StreamMode>(state.mode);
        enriched.tokensSkipped = totalTokensSkipped_;
        enriched.tokensKept = totalTokensKept_;

        // Forward to UI
        if (onTokenKept_) {
            onTokenKept_(enriched);
        }
    } else {
        ++totalTokensSkipped_;
    }

    // Check TLS deadline
    checkTLSDeadline();
}

void TokenStreamFFBridge::handleStreamComplete(
    const rawrxd::aistream::StreamState& state)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!running_) return;
    running_ = false;

    // Complete FF
    if (ffController_ && !messageId_.empty()) {
        ffController_->completeFF(messageId_);
    }

    // Forward completion
    if (onComplete_) {
        StreamSummary summary;
        summary.messageId = messageId_;
        summary.totalTokens = state.total_tokens;
        summary.tokensKept = totalTokensKept_;
        summary.tokensSkipped = totalTokensSkipped_;
        summary.averageLatencyMs = state.average_latency_ms;
        summary.mode = static_cast<StreamMode>(state.mode);
        summary.completed = true;
        summary.errorMessage.clear();

        onComplete_(summary);
    }
}

void TokenStreamFFBridge::handleStreamStall(
    const rawrxd::aistream::StreamState& state)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!running_) return;

    // Check if we should abort due to stall
    if (state.stall_count >= config_.maxStallCount) {
        cancelStream("Max stall count exceeded (" + std::to_string(state.stall_count) + ")");
        return;
    }

    // Notify via error callback but don't cancel yet
    if (onError_) {
        onError_("Stream stalled, stall count: " + std::to_string(state.stall_count));
    }
}

// ============================================================================
// HTTP Callbacks
// ============================================================================

bool TokenStreamFFBridge::handleHTTPData(const uint8_t* data, size_t len) {
    if (!streamHandler_) return false;

    bool accepted = streamHandler_->feedChunk(messageId_, data, len);

    if (!accepted) {
        // Backpressure triggered - pause HTTP reads
        // They'll resume when buffer drains
        return false;
    }

    return true;
}

void TokenStreamFFBridge::handleHTTPError(const std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!running_) return;
    running_ = false;

    if (ffController_ && !messageId_.empty()) {
        ffController_->cancelFF(messageId_, "http_error: " + error);
    }

    if (onError_) {
        onError_(error);
    }
}

void TokenStreamFFBridge::handleHTTPComplete() {
    // Stream handler will detect [DONE] or EOF and call onComplete
    // This is just the HTTP layer completing
}

// ============================================================================
// TLS Deadline Check
// ============================================================================

void TokenStreamFFBridge::checkTLSDeadline() {
    if (!ffController_ || messageId_.empty()) return;

    auto* ffState = ffController_->getFFState(messageId_);
    if (!ffState) return;

    if (ffState->isExpired()) {
        cancelStream("TLS deadline exceeded");
    }
}

// ============================================================================
// Request Builder
// ============================================================================

std::string TokenStreamFFBridge::buildRequestBody(
    const std::string& prompt,
    const APIConfig& apiConfig)
{
    // Build OpenAI-compatible chat completion request
    std::string body = "{";
    body += "\"model\":\"" + apiConfig.model + "\",";
    body += "\"messages\":[{\"role\":\"user\",\"content\":\"";
    body += escapeJSON(prompt);
    body += "\"}],";
    body += "\"stream\":true,";
    body += "\"temperature\":" + std::to_string(apiConfig.temperature) + ",";
    body += "\"max_tokens\":" + std::to_string(apiConfig.maxTokens);
    body += "}";
    return body;
}

std::string TokenStreamFFBridge::escapeJSON(const std::string& s) {
    std::string result;
    result.reserve(s.length() * 2);
    for (char c : s) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    result += buf;
                } else {
                    result += c;
                }
        }
    }
    return result;
}

// ============================================================================
// Queries
// ============================================================================

TokenStreamFFBridge::StreamStats TokenStreamFFBridge::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);

    StreamStats stats;
    stats.messageId = messageId_;
    stats.totalTokensKept = totalTokensKept_;
    stats.totalTokensSkipped = totalTokensSkipped_;
    stats.isRunning = running_;

    if (streamHandler_) {
        auto state = streamHandler_->getState();
        stats.streamTotalTokens = state.total_tokens;
        stats.streamAverageLatencyMs = state.average_latency_ms;
        stats.streamMode = static_cast<StreamMode>(state.mode);
    }

    if (ffController_ && !messageId_.empty()) {
        auto* ffState = ffController_->getFFState(messageId_);
        if (ffState) {
            stats.ffCurrentTPS = ffState->currentTPS;
            stats.ffRemainingMs = ffState->remainingMs();
            stats.ffIsExpired = ffState->isExpired();
        }
    }

    return stats;
}

bool TokenStreamFFBridge::isRunning() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return running_;
}

} // namespace UI
} // namespace RawrXD
