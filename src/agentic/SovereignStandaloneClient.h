#pragma once
// ============================================================================
// SovereignStandaloneClient.h
// Drop-in replacement for NativeInferenceClient / AgentOllamaClient
// Uses SovereignStandaloneEngine.asm — zero HTTP, zero llama.cpp dependency
// ============================================================================

#include "AgentOllamaClient.h"   // Reuse ChatMessage, InferenceResult
#include <string>
#include <vector>
#include <atomic>

namespace RawrXD {
namespace Agent {

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
};

class SovereignStandaloneClient {
public:
    explicit SovereignStandaloneClient(const SovereignModelConfig& cfg = {});
    ~SovereignStandaloneClient();

    bool LoadModel(const std::string& gguf_path);
    void UnloadModel();
    bool IsLoaded() const;

    // Drop-in API compatible with NativeInferenceClient
    InferenceResult ChatSync(const std::vector<ChatMessage>& messages,
                             const nlohmann::json& tools = nlohmann::json::array());

    uint64_t GetTotalRequests() const { return m_totalRequests.load(); }

private:
    std::string BuildPrompt(const std::vector<ChatMessage>& messages);

    SovereignModelConfig  m_config;
    bool                  m_ok = false;
    std::string           m_lastError;
    std::atomic<uint64_t> m_totalRequests{0};
};

} // namespace Agent
} // namespace RawrXD
