#pragma once
// ============================================================================
// SovereignInferenceClient.h — Host-less Native Inference Client
// Drop-in replacement for NativeInferenceClient (AgentOllamaClient.h)
// Zero HTTP. Zero JSON. Direct llama.dll + ggml-vulkan.dll via LlamaNativeBridge.
// ============================================================================

#include "AgentOllamaClient.h"   // Reuse ChatMessage, InferenceResult, callbacks
#include <string>
#include <vector>
#include <cstdint>
#include <memory>

namespace RawrXD {
namespace Agent {

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
struct SovereignModelConfig {
    std::string model_path;          // Path to .gguf weights
    uint32_t    context_size = 8192;
    uint32_t    n_batch = 512;
    uint32_t    n_gpu_layers = 99;   // -1 = auto/max
    float       temperature = 0.3f;
    float       repeat_penalty = 1.10f;
    uint32_t    max_tokens = 4096;
    bool        enable_speculative = true;
    uint32_t    draft_tokens = 5;
    std::string dll_dir;             // Directory containing llama.dll
    uint32_t    kv_quant_type_k = 0; // 0=default, 1=Q8_0, 2=Q4_0, 3=Q4_K
    uint32_t    kv_quant_type_v = 0; // 0=default, 1=Q8_0, 2=Q4_0, 3=Q4_K
};

// ---------------------------------------------------------------------------
// SovereignInferenceClient — direct-in-process inference via LlamaNativeBridge
// ---------------------------------------------------------------------------
class SovereignInferenceClient {
public:
    explicit SovereignInferenceClient(const SovereignModelConfig& cfg = {});
    ~SovereignInferenceClient();

    bool LoadModel(const std::string& gguf_path);
    void UnloadModel();
    bool IsLoaded() const;
    void ClearKVCache();

    // Drop-in API compatible with NativeInferenceClient
    InferenceResult ChatSync(const std::vector<ChatMessage>& messages,
                             const nlohmann::json& tools = nlohmann::json::array());
    bool ChatStream(const std::vector<ChatMessage>& messages,
                    const nlohmann::json& tools,
                    TokenCallback on_token,
                    ToolCallCallback on_tool_call,
                    DoneCallback on_done,
                    ErrorCallback on_error);

    uint64_t GetTotalRequests() const { return m_totalRequests.load(); }
    double   GetAvgTokensPerSec() const;

    void SetConfig(const SovereignModelConfig& cfg) { m_config = cfg; }
    const SovereignModelConfig& GetConfig() const { return m_config; }

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
    SovereignModelConfig  m_config;
    std::atomic<uint64_t> m_totalRequests{0};
    std::atomic<uint64_t> m_totalTokens{0};
    double                m_totalDurationMs{0.0};
};

} // namespace Agent
} // namespace RawrXD
