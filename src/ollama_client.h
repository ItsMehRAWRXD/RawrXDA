#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <atomic>

namespace RawrXD {
namespace Backend {

// ─── Model & Request Structures ─────────────────────────────────────

struct OllamaModel {
    std::string id;
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

// ─── Tool Calling Structures (Ollama /api/chat tool_use) ────────────

struct ToolCallFunction {
    std::string name;
    std::string arguments;  // JSON string of arguments
};

struct ToolCall {
    std::string id;
    std::string type = "function";
    ToolCallFunction function;
};

struct ToolParameterProperty {
    std::string type;
    std::string description;
};

struct ToolFunctionSchema {
    std::string name;
    std::string description;
    std::map<std::string, ToolParameterProperty> properties;
    std::vector<std::string> required;
};

struct ToolDefinition {
    std::string type = "function";
    ToolFunctionSchema function;
};

// ─── Chat Message (extended for tool calling) ───────────────────────

struct OllamaChatMessage {
    std::string role;       // "system", "user", "assistant", "tool"
    std::string content;
    // Tool calling fields
    std::vector<ToolCall> tool_calls;    // Present when assistant requests tools
    std::string tool_call_id;            // Present when role == "tool" (result)
};

// ─── Response ───────────────────────────────────────────────────────

struct NativeInferenceResponse {
    bool error = false;
    std::string error_message;

    std::string model;
    std::string response;
    OllamaChatMessage message;
    bool done = false;

    // Tool calls extracted from response
    bool has_tool_calls = false;
    std::vector<ToolCall> tool_calls;

    // Timing metrics
    uint64_t total_duration = 0;
    uint64_t prompt_eval_count = 0;
    uint64_t eval_count = 0;
    uint64_t load_duration = 0;
    uint64_t prompt_eval_duration = 0;
    uint64_t eval_duration = 0;
};

// ─── Requests ───────────────────────────────────────────────────────

struct OllamaChatRequest {
    std::string model;
    bool stream = true;
    std::map<std::string, double> options;
    std::vector<OllamaChatMessage> messages;
    // Tool calling
    std::vector<ToolDefinition> tools;
};

// ─── Connection Health ──────────────────────────────────────────────

struct ConnectionHealth {
    bool connected = false;
    std::string version;
    uint32_t model_count = 0;
    uint64_t latency_ms = 0;
    uint64_t last_check_ms = 0;
    std::string status_text;
};

// ─── Retry Configuration ────────────────────────────────────────────

struct RetryConfig {
    int max_retries = 3;
    int base_delay_ms = 1000;
    int max_delay_ms = 16000;
    double backoff_multiplier = 2.0;
};

// ─── Callbacks ──────────────────────────────────────────────────────

using StreamCallback = std::function<void(const std::string& chunk)>;
using ErrorCallback = std::function<void(const std::string& error)>;
using CompletionCallback = std::function<void(const NativeInferenceResponse& response)>;
using ToolExecutor = std::function<std::string(const std::string& tool_name, const std::string& arguments_json)>;

// ─── Client ─────────────────────────────────────────────────────────

class NativeClient {
public:
    explicit NativeClient(const std::string& base_url = "http://localhost:11435");
    ~NativeClient();

    // --- Configuration ---
    void setBaseUrl(const std::string& url);
    void setTimeoutSeconds(int seconds);
    void setRetryConfig(const RetryConfig& config);

    // --- Connection ---
    bool testConnection();
    std::string getVersion();
    bool isRunning();
    ConnectionHealth healthCheck();

    // --- Model Listing ---
    std::vector<OllamaModel> listModels();

    std::vector<OllamaModel> filterModels(
        const std::vector<OllamaModel>& models,
        std::function<bool(const OllamaModel&)> predicate) const;

    const OllamaModel* findModelById(
        const std::vector<OllamaModel>& models,
        const std::string& targetId) const;

    // --- Synchronous Generation ---
    NativeInferenceResponse generateSync(const OllamaGenerateRequest& request);
    NativeInferenceResponse chatSync(const OllamaChatRequest& request);

    // --- Streaming Generation ---
    bool generate(const OllamaGenerateRequest& request,
                  StreamCallback on_chunk,
                  ErrorCallback on_error,
                  CompletionCallback on_complete);

    bool chat(const OllamaChatRequest& request,
              StreamCallback on_chunk,
              ErrorCallback on_error,
              CompletionCallback on_complete);

    // --- Tool-Augmented Chat ---
    NativeInferenceResponse chatWithTools(
        const OllamaChatRequest& request,
        ToolExecutor executor,
        int max_tool_rounds = 5);

    // --- Embeddings ---
    std::vector<float> embeddings(const std::string& model, const std::string& prompt);

    // --- Cancellation ---
    void cancelStream();
    bool isCancelled() const;

private:
    // URL parsing
    struct ParsedUrl {
        std::string host = "localhost";
        uint16_t port = 11434;
        bool https = false;
    };
    ParsedUrl parseBaseUrl() const;

    // JSON building
    std::string createGenerateRequestJson(const OllamaGenerateRequest& req);
    std::string createChatRequestJson(const OllamaChatRequest& req);

    // Response parsing
    NativeInferenceResponse parseResponse(const std::string& json_str);
    std::vector<OllamaModel> parseModels(const std::string& json_str);
    // parseToolCalls is implemented internally (uses nlohmann::json)

    // HTTP transport (WinHTTP)
    std::string makeGetRequest(const std::string& endpoint);
    std::string makePostRequest(const std::string& endpoint, const std::string& json_body);
    bool makeStreamingPostRequest(const std::string& endpoint,
                                  const std::string& json_body,
                                  StreamCallback on_chunk,
                                  ErrorCallback on_error,
                                  CompletionCallback on_complete);

    // Retry wrapper
    std::string makeGetRequestWithRetry(const std::string& endpoint);
    std::string makePostRequestWithRetry(const std::string& endpoint, const std::string& json_body);

    std::string m_base_url;
    int m_timeout_seconds = 300;
    RetryConfig m_retry_config;
    std::atomic<bool> m_cancelled{false};
};

} // namespace Backend
} // namespace RawrXD
