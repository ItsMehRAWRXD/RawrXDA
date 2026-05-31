// ============================================================================
// token_stream_fast_forward_bridge.h
// Integration: ProductionTokenStreamHandler + FastForwardController
// ============================================================================
// Provides a unified bridge that:
//   - Accepts tokens from the production-hardened stream handler
//   - Routes them through FastForward for TPS-aware skipping
//   - Forwards kept tokens to UI with latency metrics
//   - Enforces TLS deadlines and handles backpressure
// ============================================================================

#pragma once

#include "fast_forward_controller.h"
#include "../../AI_TokenStream.hpp"
#include <memory>
#include <mutex>
#include <functional>
#include <string>
#include <stdint.h>

namespace RawrXD {
namespace UI {

// ============================================================================
// Enriched Token Info (stream + FF metrics)
// ============================================================================

enum class StreamMode : uint8_t {
    Single   = 0,
    Batch    = 1,
    Adaptive = 2
};

struct EnrichedTokenInfo {
    rawrxd::aistream::TokenInfo token;
    uint32_t streamTotalTokens = 0;
    uint32_t streamAverageLatencyMs = 0;
    StreamMode streamMode = StreamMode::Single;
    uint32_t tokensSkipped = 0;
    uint32_t tokensKept = 0;
};

struct StreamSummary {
    std::string messageId;
    uint32_t totalTokens = 0;
    uint32_t tokensKept = 0;
    uint32_t tokensSkipped = 0;
    uint32_t averageLatencyMs = 0;
    StreamMode mode = StreamMode::Single;
    bool completed = false;
    std::string errorMessage;
};

struct StreamStats {
    std::string messageId;
    uint32_t totalTokensKept = 0;
    uint32_t totalTokensSkipped = 0;
    uint32_t streamTotalTokens = 0;
    uint32_t streamAverageLatencyMs = 0;
    StreamMode streamMode = StreamMode::Single;
    float ffCurrentTPS = 0.0f;
    float ffRemainingMs = 0.0f;
    bool ffIsExpired = false;
    bool isRunning = false;
};

// ============================================================================
// Bridge Configuration
// ============================================================================

struct BridgeConfig {
    bool enforceSingleToken = true;
    uint32_t tokenTimeoutMs = 2000;
    uint32_t backpressureHigh = 200;
    uint32_t backpressureLow = 50;
    uint32_t stallDetectionMs = 5000;
    uint32_t maxStallCount = 3;
    bool enableAutoFF = true;
};

struct APIConfig {
    std::wstring host = L"localhost";
    uint16_t port = 8000;
    std::wstring path = L"/v1/chat/completions";
    std::string apiKey;
    std::string model = "gpt-4-turbo-preview";
    float temperature = 0.7f;
    uint32_t maxTokens = 4096;
    uint32_t timeoutMs = 120000;
    uint32_t connectTimeoutMs = 10000;
};

// ============================================================================
// Callbacks
// ============================================================================

using TokenKeptCallback     = std::function<void(const EnrichedTokenInfo&)>;
using StreamCompleteCallback = std::function<void(const StreamSummary&)>;
using StreamErrorCallback    = std::function<void(const std::string&)>;

// ============================================================================
// Bridge Class
// ============================================================================

class TokenStreamFFBridge {
public:
    explicit TokenStreamFFBridge(
        std::shared_ptr<FastForwardController> ffController,
        const BridgeConfig& config = BridgeConfig{}
    );
    ~TokenStreamFFBridge();

    // ---- Stream Lifecycle ----

    bool startStream(
        const std::string& messageId,
        const std::string& userPrompt,
        const APIConfig& apiConfig,
        TokenKeptCallback onTokenKept,
        StreamCompleteCallback onComplete,
        StreamErrorCallback onError
    );

    void cancelStream(const std::string& reason);
    void shutdown();

    // ---- Queries ----

    StreamStats getStats() const;
    bool isRunning() const;

private:
    std::shared_ptr<FastForwardController> ffController_;
    BridgeConfig config_;

    std::unique_ptr<rawrxd::aistream::ProductionTokenStreamHandler> streamHandler_;
    std::unique_ptr<rawrxd::aistream::AsyncHTTPStreamReader> httpReader_;

    mutable std::mutex mutex_;
    std::string messageId_;
    bool running_;
    uint32_t totalTokensKept_;
    uint32_t totalTokensSkipped_;

    TokenKeptCallback onTokenKept_;
    StreamCompleteCallback onComplete_;
    StreamErrorCallback onError_;

    // ---- Internal Handlers ----

    void handleToken(const rawrxd::aistream::TokenInfo& token,
                     const rawrxd::aistream::StreamState& state);
    void handleStreamComplete(const rawrxd::aistream::StreamState& state);
    void handleStreamStall(const rawrxd::aistream::StreamState& state);

    bool handleHTTPData(const uint8_t* data, size_t len);
    void handleHTTPError(const std::string& error);
    void handleHTTPComplete();

    void checkTLSDeadline();

    std::string buildRequestBody(const std::string& prompt, const APIConfig& apiConfig);
    std::string escapeJSON(const std::string& s);
};

} // namespace UI
} // namespace RawrXD
