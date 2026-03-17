#pragma once

#include <string>
#include <vector>
#include <memory>

namespace RawrXD {
namespace Backend {

// Chat message structure
struct OllamaChatMessage {
    std::string role;  // "system", "user", "assistant"
    std::string content;
    
    OllamaChatMessage() = default;
    OllamaChatMessage(const std::string& r, const std::string& c) : role(r), content(c) {}
};

// Chat request structure
struct OllamaChatRequest {
    std::vector<OllamaChatMessage> messages;
    std::string model;
    
    OllamaChatRequest() = default;
    OllamaChatRequest(const std::vector<OllamaChatMessage>& m, const std::string& model_name = "")
        : messages(m), model(model_name) {}
};

// Chat response structure
struct OllamaChatResponse {
    std::string response;
    bool done;
    
    OllamaChatResponse() : done(false) {}
    OllamaChatResponse(const std::string& resp, bool d) : response(resp), done(d) {}
};

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
    
    // Generate chat completion with structured messages
    std::string GenerateChatCompletion(const std::vector<OllamaChatMessage>& messages, const std::string& model = "");
    
    // Synchronous chat
    OllamaChatResponse ChatSync(const OllamaChatRequest& request);
    
    // List available models
    std::vector<std::string> ListModels();
    
    // Pull a model
    bool PullModel(const std::string& model_name);
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Backend
} // namespace RawrXD