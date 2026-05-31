// ============================================================================
// SovereignStandaloneClient.cpp
// Drop-in replacement for AgentOllamaClient / NativeInferenceClient
// Uses SovereignStandaloneEngine.asm — zero HTTP, zero llama.cpp dependency
// ============================================================================
#include "SovereignStandaloneClient.h"
#include "../asm/SovereignStandaloneEngine.hpp"
#include <Windows.h>
#include <chrono>
#include <sstream>

namespace RawrXD {
namespace Agent {

SovereignStandaloneClient::SovereignStandaloneClient(const SovereignModelConfig& cfg)
    : m_config(cfg), m_ok(false) {}

SovereignStandaloneClient::~SovereignStandaloneClient() {
    if (m_ok) {
        Engine_Shutdown();
    }
}

bool SovereignStandaloneClient::LoadModel(const std::string& path) {
    if (m_ok) {
        Engine_Shutdown();
        m_ok = false;
    }

    std::wstring wpath(path.begin(), path.end());
    m_ok = Engine_Initialize(wpath.c_str());
    if (!m_ok) {
        m_lastError = "Engine_Initialize failed (invalid path or OOM)";
        return false;
    }

    // Pre-warm with minimal inference to initialize caches
    char warmup_buffer[256] = {};
    Engine_Infer_Speculative("Hello", warmup_buffer);

    return true;
}

void SovereignStandaloneClient::UnloadModel() {
    if (m_ok) {
        Engine_Shutdown();
        m_ok = false;
    }
}

bool SovereignStandaloneClient::IsLoaded() const {
    return m_ok;
}

InferenceResult SovereignStandaloneClient::ChatSync(
    const std::vector<ChatMessage>& messages,
    const nlohmann::json& /*tools*/
) {
    InferenceResult res{};
    if (!m_ok) {
        res.error_message = m_lastError.empty() ? "[ERR] Sovereign engine offline" : m_lastError;
        return res;
    }

    auto t0 = std::chrono::high_resolution_clock::now();

    std::string prompt = BuildPrompt(messages);
    if (prompt.empty()) {
        res.error_message = "[ERR] Empty prompt";
        return res;
    }

    char output[8192] = {};
    int nTokens = Engine_Infer_Speculative(prompt.c_str(), output);

    auto t1 = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);

    if (nTokens > 0) {
        res.success = true;
        res.response = output;
        res.completion_tokens = static_cast<uint64_t>(nTokens);
        res.total_duration_ms = dur.count();
        res.tokens_per_sec = (dur.count() > 0)
            ? (nTokens * 1000.0 / dur.count())
            : 0.0;
    } else {
        res.error_message = "[ERR] Inference produced no tokens";
    }

    return res;
}

std::string SovereignStandaloneClient::BuildPrompt(const std::vector<ChatMessage>& messages) {
    std::string prompt;
    for (const auto& m : messages) {
        prompt += m.role + ": " + m.content + "\n";
    }
    prompt += "assistant: ";
    return prompt;
}

} // namespace Agent
} // namespace RawrXD
