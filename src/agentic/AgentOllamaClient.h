// =============================================================================
// AgentOllamaClient.h — HTTP streaming client for Ollama Agentic + FIM inference
// =============================================================================
// Wraps Ollama HTTP APIs with:
//   1. Tool-calling dispatch (function calling via structured output)
//   2. FIM (Fill-in-Middle) streaming for Ghost Text completions
//   3. Conversation history management
//   4. Token streaming with callback interface
//
// Ollama HTTP integration only (/api/chat, /api/generate, /api/tags).
// No exceptions — all errors via AgentResult pattern.
// =============================================================================
#pragma once

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#include <deque>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace RawrXD {
namespace Agent {

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
struct OllamaConfig {
    std::string host = "127.0.0.1";
    uint16_t port = 11434;
    std::string chat_model;                         // Active chat model tag
    std::string fim_model;                          // Active FIM model tag
    int timeout_ms = 120000;
    float temperature = 0.2f;
    float top_p = 0.9f;
    int max_tokens = 4096;
    int fim_max_tokens = 256;                       // Short for ghost text
    int num_ctx = 8192;                             // Context window
    bool use_gpu = true;
    int num_gpu = 99;                               // All layers on GPU
};

// ---------------------------------------------------------------------------
// Chat message
// ---------------------------------------------------------------------------
struct ChatMessage {
    std::string role;       // "system", "user", "assistant", "tool"
    std::string content;
    std::string tool_call_id;           // For tool responses
    json tool_calls;                     // For assistant tool-call requests
};

// ---------------------------------------------------------------------------
// Streaming callbacks
// ---------------------------------------------------------------------------
using TokenCallback    = std::function<void(const std::string& token)>;
using ToolCallCallback = std::function<void(const std::string& tool_name, const json& args)>;
using DoneCallback     = std::function<void(const std::string& full_response,
                                            uint64_t prompt_tokens,
                                            uint64_t completion_tokens,
                                            double tokens_per_sec)>;
using ErrorCallback    = std::function<void(const std::string& error)>;

// ---------------------------------------------------------------------------
// Inference result (no exceptions)
// ---------------------------------------------------------------------------
struct InferenceResult {
    bool success;
    std::string response;
    std::string error_message;

    // Tool call fields
    bool has_tool_calls;
    std::vector<std::pair<std::string, json>> tool_calls;  // [(name, args)]

    // Perf metrics
    uint64_t prompt_tokens;
    uint64_t completion_tokens;
    double tokens_per_sec;
    double total_duration_ms;

    static InferenceResult ok(const std::string& resp) {
        InferenceResult r;
        r.success = true;
        r.response = resp;
        r.has_tool_calls = false;
        r.prompt_tokens = 0;
        r.completion_tokens = 0;
        r.tokens_per_sec = 0;
        r.total_duration_ms = 0;
        return r;
    }
    static InferenceResult error(const std::string& msg) {
        InferenceResult r;
        r.success = false;
        r.error_message = msg;
        r.has_tool_calls = false;
        r.prompt_tokens = 0;
        r.completion_tokens = 0;
        r.tokens_per_sec = 0;
        r.total_duration_ms = 0;
        return r;
    }
};

struct OllamaHealth {
    bool ok = false;
    int model_count = 0;
    int latency_ms = 0;
    std::string version;
};

// ---------------------------------------------------------------------------
// AgentOllamaClient — HTTP streaming interface for agentic + FIM
// ---------------------------------------------------------------------------
class AgentOllamaClient {
public:
    explicit AgentOllamaClient(const OllamaConfig& config = {});
    ~AgentOllamaClient();

    // -- Connection --
    bool TestConnection();
    OllamaHealth TestConnectionWithStats();
    std::string GetVersion();
    std::vector<std::string> ListModels();

    // -- Chat API (agentic, with tool calling) --
    InferenceResult ChatSync(const std::vector<ChatMessage>& messages,
                             const json& tools = json::array());

    bool ChatStream(const std::vector<ChatMessage>& messages,
                    const json& tools,
                    TokenCallback on_token,
                    ToolCallCallback on_tool_call,
                    DoneCallback on_done,
                    ErrorCallback on_error);

    // -- FIM API (ghost text completions) --
    InferenceResult FIMSync(const std::string& prefix,
                            const std::string& suffix,
                            const std::string& filename = "");

    bool FIMStream(const std::string& prefix,
                   const std::string& suffix,
                   const std::string& filename,
                   TokenCallback on_token,
                   DoneCallback on_done,
                   ErrorCallback on_error);

    // -- Cancel ongoing stream --
    void CancelStream();
    bool IsStreaming() const { return m_streaming.load(); }

    // -- Configuration --
    void SetConfig(const OllamaConfig& config);
    const OllamaConfig& GetConfig() const { return m_config; }

    // -- Stats --
    uint64_t GetTotalRequests() const { return m_totalRequests.load(); }
    double GetAvgTokensPerSec() const;

    // -- Enhancement: Connection warmup + model health --
    bool WarmupConnection();
    bool CheckModelHealth(const std::string& modelName);

    // -- Enhancement: ChatSync with automatic retry --
    InferenceResult ChatSyncWithRetry(
        const std::vector<ChatMessage>& messages,
        const json& tools = json::array(),
        int maxRetries = 3);

    // -- Enhancement: Structured metrics snapshot --
    struct MetricsSnapshot {
        uint64_t totalRequests = 0;
        uint64_t totalTokens = 0;
        double avgTokensPerSec = 0.0;
        bool isStreaming = false;
        int consecutiveErrors = 0;
        std::string chatModel;
        std::string fimModel;
        std::string host;
        uint16_t port = 0;
    };
    MetricsSnapshot GetMetricsSnapshot() const;

private:
    // RawrXD native inference helpers
    std::string BuildPromptFromMessages(const std::vector<ChatMessage>& messages,
                                        const json& tools) const;
    void ParseToolCallsFromResponse(const std::string& response, InferenceResult& result) const;

    OllamaConfig m_config;
    std::mutex m_mutex;
    std::atomic<bool> m_streaming{false};
    std::atomic<bool> m_cancelRequested{false};
    std::atomic<uint64_t> m_totalRequests{0};
    std::atomic<uint64_t> m_totalTokens{0};
    std::atomic<uint64_t> m_nextRequestId{1};
    double m_totalDurationMs{0.0};
    int m_consecutiveErrors{0};
    std::deque<std::string> m_recentErrors;
    bool ShouldEmitError(const std::string& msg);
};

} // namespace Agent
} // namespace RawrXD
