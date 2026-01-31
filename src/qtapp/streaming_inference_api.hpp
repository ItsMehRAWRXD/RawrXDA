#pragma once


#include <functional>

/**
 * @brief Streaming inference API with token-by-token callbacks
 * 
 * Features:
 * - Real-time token streaming
 * - Progress callbacks
 * - Backpressure handling
 * - Cancellation support
 * - Partial result delivery
 */
class StreamingInferenceAPI : public void {

public:
    using TokenCallback = std::function<void(const std::string& token, int position)>;
    using ProgressCallback = std::function<void(int tokensGenerated, int totalTokens)>;
    using CompletionCallback = std::function<void(const std::string& fullResult)>;
    using ErrorCallback = std::function<void(const std::string& error)>;

    explicit StreamingInferenceAPI(void* parent = nullptr);
    ~StreamingInferenceAPI();

    /**
     * @brief Start streaming inference
     * @param modelPath Path to GGUF model
     * @param prompt Input prompt
     * @param maxTokens Maximum tokens to generate
     * @param temperature Sampling temperature
     * @return Stream ID for tracking
     */
    int64_t startStream(const std::string& modelPath, const std::string& prompt,
                       int maxTokens = 256, float temperature = 0.7f);

    /**
     * @brief Cancel an active stream
     */
    bool cancelStream(int64_t streamId);

    /**
     * @brief Check if stream is active
     */
    bool isStreamActive(int64_t streamId) const;

    /**
     * @brief Set token callback (called for each generated token)
     */
    void setTokenCallback(TokenCallback callback);

    /**
     * @brief Set progress callback (called periodically)
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Set completion callback (called when stream finishes)
     */
    void setCompletionCallback(CompletionCallback callback);

    /**
     * @brief Set error callback
     */
    void setErrorCallback(ErrorCallback callback);

    void tokenGenerated(int64_t streamId, const std::string& token, int position);
    void progressUpdated(int64_t streamId, int tokensGenerated, int totalTokens);
    void streamCompleted(int64_t streamId, const std::string& fullResult);
    void streamFailed(int64_t streamId, const std::string& error);
    void streamCancelled(int64_t streamId);

public:
    void onTokenReady(int64_t streamId, const std::string& token);
    void onStreamProgress(int64_t streamId, int current, int total);
    void onStreamComplete(int64_t streamId, const std::string& result);
    void onStreamError(int64_t streamId, const std::string& error);

private:
    struct StreamState {
        int64_t id;
        std::string modelPath;
        std::string prompt;
        std::string partialResult;
        int tokensGenerated = 0;
        int maxTokens;
        bool active = true;
    };

    std::unordered_map<int64_t, StreamState> m_activeStreams;
    int64_t m_nextStreamId = 1;
    
    TokenCallback m_tokenCallback;
    ProgressCallback m_progressCallback;
    CompletionCallback m_completionCallback;
    ErrorCallback m_errorCallback;
};


