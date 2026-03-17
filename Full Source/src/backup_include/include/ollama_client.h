#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace RawrXD {
namespace Backend {

struct OllamaModel {
    std::string id;                    // Unique identifier (name by default)
    std::string name;
    std::string digest;
    uint64_t size = 0;
    std::string modified_at;
    std::string format;
    std::string family;
    std::string parameter_size;
    std::string quantization_level;
};

struct OllamaGenerateRequest {
    std::string model;
    std::string prompt;
    bool stream = true;
    std::map<std::string, double> options;
};

struct OllamaChatMessage {
    std::string role;
    std::string content;
};

struct OllamaResponse {
    bool error = false;
    std::string error_message;

    std::string model;
    std::string response;
    OllamaChatMessage message;
    bool done = false;

    uint64_t total_duration = 0;
    uint64_t prompt_eval_count = 0;
    uint64_t eval_count = 0;
    uint64_t load_duration = 0;
    uint64_t prompt_eval_duration = 0;
    uint64_t eval_duration = 0;
};

struct OllamaChatRequest {
    std::string model;
    bool stream = true;
    std::map<std::string, double> options;
    std::vector<OllamaChatMessage> messages;
};

using StreamCallback = std::function<void(const std::string& chunk)>;
using ErrorCallback = std::function<void(const std::string& error)>;
using CompletionCallback = std::function<void(const OllamaResponse& response)>;

class OllamaClient {
public:
    explicit OllamaClient(const std::string& base_url);
    ~OllamaClient();

    void setBaseUrl(const std::string& url);
    bool testConnection();
    std::string getVersion();
    bool isRunning();

    std::vector<OllamaModel> listModels();

    /**
     * @brief Filter models using a generic predicate function
     * @param models Vector of models to filter
     * @param predicate Function returning true for models to keep
     * @return Filtered vector of models
     */
    std::vector<OllamaModel> filterModels(
        const std::vector<OllamaModel>& models,
        std::function<bool(const OllamaModel&)> predicate) const;

    /**
     * @brief Find a single model by ID (equivalent to JavaScript pfs function)
     * @param models Vector of models to search
     * @param targetId The ID to match
     * @return Pointer to matching model, or nullptr if not found
     */
    const OllamaModel* findModelById(
        const std::vector<OllamaModel>& models,
        const std::string& targetId) const;

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

private:
    std::string createGenerateRequestJson(const OllamaGenerateRequest& req);
    std::string createChatRequestJson(const OllamaChatRequest& req);
    OllamaResponse parseResponse(const std::string& json_str);
    std::vector<OllamaModel> parseModels(const std::string& json_str);

    std::string makeGetRequest(const std::string& endpoint);
    std::string makePostRequest(const std::string& endpoint, const std::string& json_body);
    bool makeStreamingPostRequest(const std::string& endpoint,
                                  const std::string& json_body,
                                  StreamCallback on_chunk,
                                  ErrorCallback on_error,
                                  CompletionCallback on_complete);

    std::string m_base_url;
    int m_timeout_seconds = 300;
};

} // namespace Backend
} // namespace RawrXD
