#pragma once

// ============================================================================
// OllamaProxy — C++20, no Qt. Lightweight proxy to Ollama REST API.
// ============================================================================
// Use WinHTTP or similar in .cpp; this header declares the API only.
// ============================================================================

#include <functional>
#include <string>
#include <vector>

/**
 * Lightweight fallback proxy to Ollama REST API.
 * Used when model exists in Ollama but not as plain .gguf, or GGUF uses
 * quantization the engine doesn't support. Preserves direct GGUF loading when possible.
 */
class OllamaProxy {
public:
    using TokenFn = std::function<void(const std::string&)>;
    using CompleteFn = std::function<void()>;
    using ErrorFn = std::function<void(const std::string&)>;

    OllamaProxy() = default;
    ~OllamaProxy();

    void setModel(const std::string& modelName);
    std::string currentModel() const { return m_modelName; }

    bool isOllamaAvailable();
    bool isModelAvailable(const std::string& modelName);

    void generateResponse(const std::string& prompt,
                         float temperature = 0.8f,
                         int maxTokens = 512);

    void stopGeneration();

    void setOnTokenArrived(TokenFn fn) { m_onTokenArrived = std::move(fn); }
    void setOnGenerationComplete(CompleteFn fn) { m_onGenerationComplete = std::move(fn); }
    void setOnError(ErrorFn fn) { m_onError = std::move(fn); }

private:
    void onNetworkReply();
    void onNetworkError(int code);

    std::string m_modelName;
    std::string m_ollamaUrl = "http://localhost:11434";
    void* m_networkContext = nullptr;  // WinHTTP or similar
    void* m_currentRequest = nullptr;

    std::vector<uint8_t> m_buffer;

    TokenFn m_onTokenArrived;
    CompleteFn m_onGenerationComplete;
    ErrorFn m_onError;
};
