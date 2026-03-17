#pragma once

#include <string>
#include <vector>
#include <memory>

namespace RawrXD {
namespace LLMAdapter {

// GGUF model runner for inference
class GGUFRunner {
public:
    GGUFRunner();
    ~GGUFRunner();
    
    // Initialize the runner
    bool Initialize();
    
    // Load a GGUF model
    bool LoadModel(const std::string& path);
    
    // Unload current model
    void UnloadModel();
    
    // Generate text
    std::string Generate(const std::string& prompt, int max_tokens = 512);
    
    // Generate text with streaming
    void GenerateStreaming(const std::string& prompt, 
                          std::function<void(const std::string&)> callback,
                          int max_tokens = 512);
    
    // Get model info
    std::string GetModelInfo() const;
    
    // Check if model is loaded
    bool IsModelLoaded() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace LLMAdapter
} // namespace RawrXD