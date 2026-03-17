#pragma once

#include <string>
#include <vector>
#include <functional>
#include <map>
#include <optional>
#include <cstdint>
#include "utils/logger.h"
#include <nlohmann/json.hpp>

namespace RawrXD {
namespace Backend {

struct OllamaModel {
    std::string name;
    std::string modified_at;
    uint64_t size;
    std::string digest;
    std::string format;
    std::string family;
    std::string parameter_size;
    std::string quantization_level;
};

struct OllamaGenerateRequest {
    std::string model;
    std::string prompt;
    std::string system;
    std::string template_str;
    std::string context; 
    bool stream = false;
    std::map<std::string, std::string> options;
};

struct OllamaChatRequest {
    std::string model;
    struct Message {
        std::string role;
        std::string content;
        std::vector<std::string> images;
    };
    std::vector<Message> messages;
    bool stream = false;
    std::map<std::string, std::string> options;
};

struct OllamaResponse {
    std::string model;
    std::string created_at;
    std::string response; // For generate
    // For chat:
    struct Message {
        std::string role;
        std::string content;
    } message;
    
    bool done = false;
    int64_t total_duration = 0;
    int64_t load_duration = 0;
    int64_t prompt_eval_count = 0;
    int64_t prompt_eval_duration = 0;
    int64_t eval_count = 0;
    int64_t eval_duration = 0;
    std::string context; // For generate
};

class OllamaClient {
public:
    using StreamCallback = std::function<void(const OllamaResponse&)>;
    using ErrorCallback = std::function<void(const std::string&)>;
    using CompletionCallback = std::function<void(const OllamaResponse&)>;

    // Constructor. The base URL and timeout can be overridden via environment variables:
    //  - OLLAMA_BASE_URL (string): override base URL (default: http://localhost:11434)
    //  - OLLAMA_TIMEOUT_SECONDS (int): override request timeout in seconds (default: 300)
    OllamaClient(const std::string& base_url = "http://localhost:11434");
    ~OllamaClient();

    // Logger component name used for structured logging
    static constexpr const char* LOGGER_COMPONENT = "OllamaClient";

    void setBaseUrl(const std::string& url);
    bool testConnection();
    std::string getVersion();
    bool isRunning();

    std::vector<OllamaModel> listModels();

    OllamaResponse generateSync(const OllamaGenerateRequest& request);
    OllamaResponse chatSync(const OllamaChatRequest& request);

    bool generate(const OllamaGenerateRequest& request,
                  StreamCallback on_chunk,
                  ErrorCallback on_error,
                  CompletionCallback on_complete);

    bool chat(const OllamaChatRequest& request,
              StreamCallback on_chunk,
              ErrorCallback on_error,
              CompletionCallback on_complete);

    std::vector<float> embeddings(const std::string& model, const std::string& prompt);

#ifdef UNIT_TESTING
    // Test helpers (expose internal parsers for unit tests)
    std::vector<OllamaModel> parseModelsForTest(const std::string& json_response) { return parseModels(json_response); }
    OllamaResponse parseResponseForTest(const std::string& json_response) { return parseResponse(json_response); }
#endif

private:
    std::string m_base_url;
    int m_timeout_seconds;

    std::string makeGetRequest(const std::string& endpoint);
    std::string makePostRequest(const std::string& endpoint, const std::string& json_body);
    bool makeStreamingPostRequest(const std::string& endpoint, 
                                  const std::string& json_body,
                                  StreamCallback on_chunk,
                                  ErrorCallback on_error,
                                  CompletionCallback on_complete);

    std::vector<OllamaModel> parseModels(const std::string& json_response);
    OllamaResponse parseResponse(const std::string& json_response);

    // (Parsing implementations are in the .cpp to avoid duplication)
    std::string createGenerateRequestJson(const OllamaGenerateRequest& request);
    std::string createChatRequestJson(const OllamaChatRequest& request);
};

} // namespace Backend
} // namespace RawrXD
