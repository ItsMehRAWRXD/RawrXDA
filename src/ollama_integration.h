#pragma once

#include <string>
#include <vector>
#include <functional>

namespace OllamaIntegration {

struct CompletionRequest {
    std::string model;           // e.g., "codellama:7b"
    std::string prompt;          // Code prefix
    float temperature = 0.7f;
    float top_p = 0.9f;
    int num_predict = 256;       // Max tokens
    bool stream = false;
};

struct CompletionResponse {
    std::string text;            // Generated completion
    bool success = false;
    std::string error_message;
};

// Query Ollama API for code completion
// Returns completion or empty string if Ollama unavailable
CompletionResponse QueryCompletion(const CompletionRequest& req);

// Test connectivity to Ollama server
bool IsOllamaAvailable(const std::string& host = "localhost", int port = 11434);

// Get list of available models
std::vector<std::string> GetAvailableModels();

// Start background suggester thread (for async completions)
void StartAsyncSuggester(std::function<void(const std::string&)> on_suggestion);
void StopAsyncSuggester();

} // namespace OllamaIntegration
