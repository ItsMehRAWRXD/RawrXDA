#pragma once
#include <string>
#include <vector>
#include <chrono>

namespace RawrXD::ML {

/**
 * MLInferenceEngine — Handles HTTP communication with RawrEngine backend
 * Supports token streaming, telemetry, and structured output
 */
class MLInferenceEngine {
public:
    struct InferenceResult {
        std::string response;
        size_t tokenCount;
        int64_t latencyMs;
        bool success;
    };

    struct TelemetryData {
        std::string timestamp;
        std::string model;
        std::string prompt;
        size_t promptTokens;
        size_t completionTokens;
        int64_t ttfFirstToken;  // Time to first token (ms)
        int64_t ttfCompletionTime;
        float tokensPerSecond;
        std::string status;  // "success", "error", "timeout"
    };

    static MLInferenceEngine& getInstance();

    /**
     * Initialize inference backend connection
     * Returns true if RawrEngine is reachable on localhost:23959
     */
    bool initialize();

    /**
     * Send prompt to model, receive response with streaming
     * callback is invoked per token for live streaming display
     */
    InferenceResult query(
        const std::string& prompt,
        std::function<void(const std::string& token)> onTokenCallback = nullptr,
        size_t maxTokens = 512
    );

    /**
     * Get structured telemetry from last inference
     */
    TelemetryData getLastTelemetry() const { return m_lastTelemetry; }

    /**
     * Export telemetry as JSON
     */
    std::string telemetryToJSON() const;

    /**
     * Export inference result as JSON
     */
    std::string resultToJSON(const InferenceResult& result) const;

    void shutdown();

private:
    MLInferenceEngine() = default;
    ~MLInferenceEngine() = default;

    bool connectToRawrEngine();
    std::string buildJSONRequest(const std::string& prompt, size_t maxTokens);
    std::vector<std::string> parseTokensFromResponse(const std::string& response);

    bool m_initialized{false};
    std::string m_engineEndpoint{"http://localhost:23959"};
    TelemetryData m_lastTelemetry;
    std::chrono::steady_clock::time_point m_sessionStart;
};

/**
 * Streaming token observer — called per token for UI updates
 */
class TokenStreamObserver {
public:
    virtual ~TokenStreamObserver() = default;
    virtual void onToken(const std::string& token) = 0;
    virtual void onComplete(size_t totalTokens) = 0;
    virtual void onError(const std::string& error) = 0;
};

}
