#pragma once

#include "RawrXD_Foundation.h"
#include "RawrXD_SignalSlot.h"
#include <string>

using RawrXD::String;
using RawrXD::Signal;

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
class OllamaProxy {
public:
    explicit OllamaProxy(void* parent = nullptr);
    ~OllamaProxy();
    
    // Set which Ollama model to use (e.g., "llama3.2:3b", "unlocked-350M:latest")
    void setModel(const String& modelName);
    String currentModel() const { return m_modelName; }
    
    // Check if Ollama is running and model is available
    bool isOllamaAvailable();
    bool isModelAvailable(const String& modelName);
    
    // Generate response using Ollama API with streaming
    void generateResponse(const String& prompt, 
                         float temperature = 0.8f,
                         int maxTokens = 512);
    
    // Stop current generation
    void stopGeneration();
    
    // Signals
    // Emitted for each token during streaming (compatible with AgenticEngine)
    Signal<const String&> tokenArrived;
    
    // Emitted when generation completes
    Signal<> generationComplete;
    
    // Emitted on errors
    Signal<const String&> error;
    
private:
    String m_modelName;
    bool m_isRunning;
    
    // Network handles or implementation detail
    void* m_networkManager = nullptr; // Placeholder for WinHttp handle or similar
};
