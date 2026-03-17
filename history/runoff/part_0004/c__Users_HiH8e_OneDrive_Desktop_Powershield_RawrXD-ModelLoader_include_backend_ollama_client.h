#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <map>

namespace RawrXD {
namespace Backend {

// Ollama API response structures
struct OllamaModel {
    std::string name;
    std::string modified_at;
    uint64_t size;
    std::string digest;
    std::map<std::string, std::string> details;
};

struct OllamaChatMessage {
    std::string role;      // "user", "assistant", "system"
    std::string content;
    std::map<std::string, std::string> metadata;
};

struct OllamaGenerateRequest {
    std::string model;
    std::string prompt;
    std::vector<std::string> images;  // base64 encoded
    bool stream = true;
    std::map<std::string, double> options;  // temperature, top_p, etc.
};

struct OllamaChatRequest {
    std::string model;
    std::vector<OllamaChatMessage> messages;
    bool stream = true;
    std::map<std::string, double> options;
};

struct OllamaResponse {
    std::string model;
    std::string response;
    bool done;
    
    // Metrics
    uint64_t total_duration = 0;      // nanoseconds
    uint64_t load_duration = 0;
    uint64_t prompt_eval_count = 0;
    uint64_t prompt_eval_duration = 0;
    uint64_t eval_count = 0;
    uint64_t eval_duration = 0;
    
    // For chat
    OllamaChatMessage message;
    
    // Error handling
    bool error = false;
    std::string error_message;
};

// Callback types
using StreamCallback = std::function<void(const OllamaResponse&)>;
using ErrorCallback = std::function<void(const std::string&)>;
using CompletionCallback = std::function<void(const OllamaResponse&)>;

class OllamaClient {
public:
    OllamaClient(const std::string& base_url = "http://localhost:11434");
    ~OllamaClient();

    // Connection management
    bool testConnection();
    void setBaseUrl(const std::string& url);
    std::string getBaseUrl() const { return m_base_url; }
    void setTimeout(int seconds) { m_timeout_seconds = seconds; }
    
    // Model management
    std::vector<OllamaModel> listModels();
    bool pullModel(const std::string& model_name, StreamCallback progress = nullptr);
    bool deleteModel(const std::string& model_name);
    bool copyModel(const std::string& source, const std::string& destination);
    OllamaModel showModel(const std::string& model_name);
    
    // Generation (streaming)
    bool generate(const OllamaGenerateRequest& request, 
                 StreamCallback on_chunk,
                 ErrorCallback on_error = nullptr,
                 CompletionCallback on_complete = nullptr);
    
    // Chat (streaming)
    bool chat(const OllamaChatRequest& request,
             StreamCallback on_chunk,
             ErrorCallback on_error = nullptr,
             CompletionCallback on_complete = nullptr);
    
    // Synchronous generation (blocks until complete)
    OllamaResponse generateSync(const OllamaGenerateRequest& request);
    OllamaResponse chatSync(const OllamaChatRequest& request);
    
    // Embeddings
    std::vector<float> embeddings(const std::string& model, const std::string& prompt);
    
    // Server info
    std::string getVersion();
    bool isRunning();

private:
    std::string m_base_url;
    int m_timeout_seconds;
    
    // HTTP request helpers
    std::string makeGetRequest(const std::string& endpoint);
    std::string makePostRequest(const std::string& endpoint, const std::string& json_body);
    bool makeStreamingPostRequest(const std::string& endpoint, 
                                  const std::string& json_body,
                                  StreamCallback on_chunk,
                                  ErrorCallback on_error,
                                  CompletionCallback on_complete);
    
    // JSON parsing helpers
    OllamaResponse parseResponse(const std::string& json);
    std::vector<OllamaModel> parseModels(const std::string& json);
    OllamaModel parseModel(const std::string& json);
    std::string createGenerateRequestJson(const OllamaGenerateRequest& req);
    std::string createChatRequestJson(const OllamaChatRequest& req);
};

} // namespace Backend
} // namespace RawrXD
