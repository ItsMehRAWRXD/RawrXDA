#pragma once


/**
 * @class OllamaProxy
 * @brief Lightweight fallback proxy to Ollama REST API
 * 
 * Only used when:
 * 1. Model exists in Ollama registry but not as plain .gguf in OllamaModels
 * 2. GGUF uses quantization our engine doesn't support yet
 * 
 * Preserves all custom optimizations 95% of the time by using direct GGUF loading.
 * Falls back to Ollama only for edge cases (new quants, unsupported architectures).
 */
class OllamaProxy : public void {

public:
    explicit OllamaProxy(void* parent = nullptr);
    ~OllamaProxy();
    
    // Set which Ollama model to use (e.g., "llama3.2:3b", "unlocked-350M:latest")
    void setModel(const std::string& modelName);
    std::string currentModel() const { return m_modelName; }
    
    // Check if Ollama is running and model is available
    bool isOllamaAvailable();
    bool isModelAvailable(const std::string& modelName);
    
    // Generate response using Ollama API with streaming
    void generateResponse(const std::string& prompt, 
                         float temperature = 0.8f,
                         int maxTokens = 512);
    
    // Stop current generation
    void stopGeneration();


    // Emitted for each token during streaming (compatible with AgenticEngine)
    void tokenArrived(const std::string& token);
    
    // Emitted when generation completes
    void generationComplete();
    
    // Emitted on errors
    void error(const std::string& message);
    
private:
    void onNetworkReply();
    void onNetworkError(void*::NetworkError code);
    
private:
    std::string m_modelName;
    std::string m_ollamaUrl;  // Default: http://localhost:11434
    void** m_networkManager;
    void** m_currentReply;
    
    std::vector<uint8_t> m_buffer;  // Buffer for partial SSE events
};

