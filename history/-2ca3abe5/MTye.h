#pragma once

#include <string>
#include <vector>
#include <memory>

namespace RawrXD {
namespace Backend {

// Ollama API client for model inference
class OllamaClient {
public:
    OllamaClient();
    ~OllamaClient();
    
    // Initialize the client
    bool Initialize(const std::string& base_url = "http://localhost:11434");
    
    // Generate completion
    std::string GenerateCompletion(const std::string& prompt, const std::string& model = "");
    
    // Generate chat completion
    std::string GenerateChatCompletion(const std::vector<std::string>& messages, const std::string& model = "");
    
    // List available models
    std::vector<std::string> ListModels();
    
    // Pull a model
    bool PullModel(const std::string& model_name);
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Chat message structure
struct OllamaChatMessage {
    std::string role;  // "system", "user", "assistant"
    std::string content;
    
    OllamaChatMessage() = default;
    OllamaChatMessage(const std::string& r, const std::string& c) : role(r), content(c) {}
};

} // namespace Backend
} // namespace RawrXD