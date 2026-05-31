// =============================================================================
// NativeInferenceClient.h — Native streaming client for Agentic + FIM inference
// =============================================================================
// Chat/FIM sync: BackendOrchestrator (native / Titan-isolated inference).
// Chat/FIM stream: native NDJSON streaming
// RAWRXD_STREAM_VIA_ORCHESTRATOR=1 → orchestrator only (Titan/native; batch completion).
// RAWRXD_STREAM_FALLBACK_ORCHESTRATOR=1 → on direct Ollama failure, retry via orchestrator.
//
// Provides:
//   1. Tool-calling dispatch (function calling via structured output)
//   2. FIM (Fill-in-Middle) streaming for Ghost Text completions
//   3. Conversation history management
//   4. Token streaming with callback interface
// No exceptions — all errors via AgentResult pattern.
// =============================================================================
#pragma once

#include "context_config.h"
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace RawrXD
{
namespace Prediction
{
class NativeStreamProvider;
}
namespace Agent
{

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
struct NativeInferenceConfig
{
    // Streaming env: RAWRXD_STREAM_VIA_ORCHESTRATOR=1 → orchestrator only.
    // RAWRXD_STREAM_FALLBACK_ORCHESTRATOR=1 → if direct Ollama fails, use orchestrator.
    std::string host = "127.0.0.1";
    uint16_t port = 11435;   // IDE embedded server (avoid conflict with external Ollama 11434)
    std::string chat_model;  // Active chat model tag
    std::string fim_model;   // Active FIM model tag
    int timeout_ms = 120000;
    float temperature = 0.2f;
    float top_p = 0.9f;
    int max_tokens = 4096;
    int fim_max_tokens = 256;                      // Short for ghost text
    int num_ctx = RawrXD::ContextLimits::DEFAULT;  // Unified context window
    bool use_gpu = true;
    int num_gpu = 99;  // All layers on GPU
    
    // TPS-based warmup for large models (prompt processing to heat up GPU/KV cache)
    double tps_warmup_threshold = 5.0;  // Below this tok/s, trigger warmup
    int warmup_max_tokens = 64;           // Short batch completion to warm model
    bool enable_auto_warmup = true;       // Auto-warmup on poor TPS detection
};

// ---------------------------------------------------------------------------
// Chat message 
// ---------------------------------------------------------------------------
struct ChatMessage
{
    std::string role;  // "system", "user", "assistant", "tool"
    std::string content;
    std::string tool_call_id;  // For tool responses
    json tool_calls;           // For assistant tool-call requests
};

// ---------------------------------------------------------------------------
// Streaming callbacks
// ---------------------------------------------------------------------------
using TokenCallback = std::function<void(const std::string& token)>;
using ToolCallCallback = std::function<void(const std::string& tool_name, const json& args)>;
using DoneCallback = std::function<void(const std::string& full_response, uint64_t prompt_tokens,
                                        uint64_t completion_tokens, double tokens_per_sec)>;
using ErrorCallback = std::function<void(const std::string& error)>;

// ---------------------------------------------------------------------------
// Inference result (no exceptions)
// ---------------------------------------------------------------------------
struct InferenceResult
{
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

    static InferenceResult ok(const std::string& resp)
    {
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
    static InferenceResult error(const std::string& msg)
    {
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

struct NativeInferenceHealth
{
    bool ok = false;
    int model_count = 0;
    int latency_ms = 0;
    std::string version;
};

// ---------------------------------------------------------------------------
// NativeInferenceClient — native streaming interface for agentic + FIM
// ---------------------------------------------------------------------------
class NativeInferenceClient
{
  public:
    explicit NativeInferenceClient(const NativeInferenceConfig& config = {});
    ~NativeInferenceClient();

    // -- Connection --
    bool TestConnection();
    NativeInferenceHealth TestConnectionWithStats();
    std::string GetVersion();
    std::vector<std::string> ListModels();

    // -- Chat API (agentic, with tool calling) --
    InferenceResult ChatSync(const std::vector<ChatMessage>& messages, const json& tools = json::array());

    /// Default: WinHTTP NDJSON to host:port. Env RAWRXD_STREAM_VIA_ORCHESTRATOR=1 → orchestrator only;
    /// RAWRXD_STREAM_FALLBACK_ORCHESTRATOR=1 → orchestrator if direct fails.
    bool ChatStream(const std::vector<ChatMessage>& messages, const json& tools, TokenCallback on_token,
                    ToolCallCallback on_tool_call, DoneCallback on_done, ErrorCallback on_error);

    // -- FIM API (ghost text completions) --
    InferenceResult FIMSync(const std::string& prefix, const std::string& suffix, const std::string& filename = "");

    /// Same routing as ChatStream (direct Ollama vs env orchestrator).
    bool FIMStream(const std::string& prefix, const std::string& suffix, const std::string& filename,
                   TokenCallback on_token, DoneCallback on_done, ErrorCallback on_error);

    // -- Cancel ongoing stream --
    void CancelStream();
    bool IsStreaming() const { return m_streaming.load(); }

    // -- Configuration --
    void SetConfig(const NativeInferenceConfig& config);
    const NativeInferenceConfig& GetConfig() const { return m_config; }

    // -- Stats --
    uint64_t GetTotalRequests() const { return m_totalRequests.load(); }
    double GetAvgTokensPerSec() const;

    // -- Enhancement: Connection warmup + model health --
    bool WarmupConnection();
    bool CheckModelHealth(const std::string& modelName);
    
    // -- Enhancement: TPS-based model warmup for large models --
    /// Performs a batch completion to warm up GPU/KV cache for better streaming TPS.
    /// Call when TPS is detected as poor (below config.tps_warmup_threshold).
    /// Returns true if warmup was performed, false if skipped (already warm or disabled).
    bool WarmupModelForStreaming();
    
    /// Check if model needs warmup based on recent TPS metrics.
    bool NeedsWarmup() const;
    
    /// Mark model as warmed (skip future warmups until cooldown).
    void MarkWarmed();
    
    /// Get last measured TPS (for warmup decision).
    double GetLastMeasuredTPS() const { return m_lastMeasuredTPS; }

    // -- Enhancement: ChatSync with automatic retry --
    InferenceResult ChatSyncWithRetry(const std::vector<ChatMessage>& messages, const json& tools = json::array(),
                                      int maxRetries = 3);

    // -- Enhancement: Structured metrics snapshot --
    struct MetricsSnapshot
    {
        uint64_t totalRequests = 0;
        uint64_t totalTokens = 0;
        double avgTokensPerSec = 0.0;
        bool isStreaming = false;
        int consecutiveErrors = 0;
        std::string chatModel;
        std::string fimModel;
        std::string host;
        uint16_t port = 0;
        /// e.g. "orchestrator" | "direct" | "direct+fallback" (from RAWRXD_STREAM_* env at snapshot time).
        std::string streamRouting;
        // TPS warmup state
        double lastMeasuredTPS = 0.0;
        bool modelWarmed = false;
        double tpsWarmupThreshold = 0.0;
    };
    MetricsSnapshot GetMetricsSnapshot() const;

    /// RAII: registers \p http for CancelStream() for the duration of a direct Ollama HTTP stream.
    struct StreamCancelScope
    {
        NativeInferenceClient* c;
        StreamCancelScope(NativeInferenceClient* cc, Prediction::NativeStreamProvider* hh);
        ~StreamCancelScope();
        StreamCancelScope(const StreamCancelScope&) = delete;
        StreamCancelScope& operator=(const StreamCancelScope&) = delete;
    };

  private:
    // RawrXD native inference helpers
    std::string BuildPromptFromMessages(const std::vector<ChatMessage>& messages, const json& tools) const;
    void ParseToolCallsFromResponse(const std::string& response, InferenceResult& result) const;

    NativeInferenceConfig m_config;
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

    /// Direct POST /api/generate NDJSON (true streaming). Default when RAWRXD_STREAM_VIA_ORCHESTRATOR is unset.
    bool runChatStreamDirect(const std::string& prompt, const json& tools, TokenCallback on_token,
                                   ToolCallCallback on_tool_call, DoneCallback on_done, ErrorCallback on_error);
    /// BackendOrchestrator queue (batch completion; stream_cb fires once). Set env RAWRXD_STREAM_VIA_ORCHESTRATOR=1.
    bool runChatStreamViaOrchestrator(const std::string& prompt, const json& tools, TokenCallback on_token,
                                      ToolCallCallback on_tool_call, DoneCallback on_done, ErrorCallback on_error);

    bool runFimStreamDirect(const std::string& prompt, TokenCallback on_token, DoneCallback on_done,
                                  ErrorCallback on_error);
    bool runFimStreamViaOrchestrator(const std::string& prompt, TokenCallback on_token, DoneCallback on_done,
                                     ErrorCallback on_error);

    /// Active Ollama HTTP stream (for CancelStream → WinHTTP cancel bit).
    Prediction::NativeStreamProvider* m_activeStreamHttp{nullptr};
    
    // TPS-based warmup state
    std::atomic<bool> m_modelWarmed{false};
    std::atomic<double> m_lastMeasuredTPS{0.0};
    std::chrono::steady_clock::time_point m_lastWarmupTime;
    
    /// Internal: perform the actual warmup batch completion.
    bool performWarmupCompletion();
};

/// Same label as MetricsSnapshot::streamRouting (RAWRXD_STREAM_VIA_ORCHESTRATOR / RAWRXD_STREAM_FALLBACK_ORCHESTRATOR).
inline std::string GetNativeStreamRoutingEnvLabel()
{
    const char* via = std::getenv("RAWRXD_STREAM_VIA_ORCHESTRATOR");
    if (via != nullptr && std::strcmp(via, "1") == 0)
    {
        return "orchestrator";
    }
    std::string out = "direct";
    const char* fb = std::getenv("RAWRXD_STREAM_FALLBACK_ORCHESTRATOR");
    if (fb != nullptr && std::strcmp(fb, "1") == 0)
    {
        out += "+fallback";
    }
    return out;
}

/// TPS threshold for warmup trigger (configurable via NativeInferenceConfig::tps_warmup_threshold)
constexpr double kDefaultTPSWarmupThreshold = 5.0;  // tok/s

/// Cooldown period before re-warmup is allowed (seconds)
constexpr int kWarmupCooldownSeconds = 300;

}  // namespace Agent
}  // namespace RawrXD
