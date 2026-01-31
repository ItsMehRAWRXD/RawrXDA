#pragma once


/**
 * @brief Request structure for unified inference backend
 */
struct UnifiedRequest {
    std::string prompt;
    int64_t  reqId;
    std::string backend;   // "local" | "llama" | "openai" | "claude" | "gemini"
    std::string apiKey;
};

/**
 * @brief Unified backend supporting local GGUF and remote API inference
 * 
 * Handles streaming responses from:
 * - Local GGUF models (via InferenceEngine worker thread)
 * - llama.cpp HTTP server (self-hosted)
 * - OpenAI API (gpt-3.5-turbo, gpt-4)
 * - Anthropic Claude API (claude-3-sonnet)
 * - Google Gemini API (gemini-pro)
 */
class UnifiedBackend : public void {

public:
    explicit UnifiedBackend(void* parent = nullptr);
    
    /**
     * @brief Submit inference request to configured backend
     * @param req Request with prompt, backend ID, and optional API key
     */
    void submit(const UnifiedRequest& req);
    
    /**
     * @brief Set the local inference engine (for "local" backend)
     */
    void setLocalEngine(void* engine) { m_localEngine = engine; }

    /**
     * @brief Emitted for each token during streaming inference
     * @param reqId Request identifier
     * @param token Single token or character from model
     */
    void streamToken(int64_t reqId, const std::string& token);
    
    /**
     * @brief Emitted when streaming inference completes
     * @param reqId Request identifier
     */
    void streamFinished(int64_t reqId);
    
    /**
     * @brief Emitted on inference error
     * @param reqId Request identifier
     * @param error Error message
     */
    void error(int64_t reqId, const std::string& error);

private:
    void onLocalDone(int64_t id, const std::string& answer);

private:
    void submitLlamaCpp(const UnifiedRequest& req);
    void submitOpenAI(const UnifiedRequest& req);
    void submitClaude(const UnifiedRequest& req);
    void submitGemini(const UnifiedRequest& req);
    
    void** m_nam{nullptr};
    void* m_localEngine{nullptr};
};


