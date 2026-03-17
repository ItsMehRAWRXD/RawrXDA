#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * @class OllamaRESTClient
 * @brief Lightweight HTTP client for Ollama /api/tags discovery
 * 
 * Connects to local Ollama server on :11434 to enumerate available models.
 * Models are returned as JSON to be registered with UniversalModelRouter.
 */
class OllamaRESTClient {
public:
    struct OllamaModel {
        std::string name;           // e.g. "llama2:7b"
        std::string description;    // e.g. "Llama 2 7B parameter model"
        std::string id;             // Model ID/GUID
        uint64_t size_bytes;        // Model size
        std::string modified_at;    // ISO timestamp
    };

    OllamaRESTClient();
    ~OllamaRESTClient();

    // Non-copyable
    OllamaRESTClient(const OllamaRESTClient&) = delete;
    OllamaRESTClient& operator=(const OllamaRESTClient&) = delete;

    /**
     * @brief Connect to local Ollama server
     * @param host Default: "localhost"
     * @param port Default: 11434
     * @param timeout_ms Default: 5000
     * @return true if server responds
     */
    bool connect(const std::string& host = "localhost", 
                 int port = 11434, 
                 int timeout_ms = 5000);

    /**
     * @brief Query /api/tags for all available models
     * @return Vector of OllamaModel structs (empty if server unavailable)
     */
    std::vector<OllamaModel> getAvailableModels();

    /**
     * @brief Filter models using a predicate function
     * @param models Vector of models to filter
     * @param predicate Function that returns true for models to keep
     * @return Filtered vector of models
     */
    std::vector<OllamaModel> filterModels(const std::vector<OllamaModel>& models, 
                                         std::function<bool(const OllamaModel&)> predicate);

    /**
     * @brief Find model by ID (equivalent to JavaScript pfs function)
     * @param models Vector of models to search
     * @param targetId The ID to match
     * @return Pointer to matching model, or nullptr if not found
     */
    const OllamaModel* findModelById(const std::vector<OllamaModel>& models, 
                                    const std::string& targetId);

    /**
     * @brief Check if Ollama server is reachable
     * @return true if server responds to ping
     */
    bool isServerReady();

    /**
     * @brief Get raw JSON response from /api/tags (for advanced use)
     * @return JSON array of models
     */
    json getModelsJSON();

private:
    std::string m_baseUrl;  // e.g. "http://localhost:11434"
    CURL* m_curl;
    int m_timeout_ms;

    // CURL helper
    static size_t curlWriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp);
};
