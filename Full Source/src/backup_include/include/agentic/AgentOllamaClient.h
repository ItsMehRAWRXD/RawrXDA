#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif

#include <nlohmann/json.hpp>

namespace RawrXD {
namespace Agent {

// =============================================================================
// AgentOllamaClient.h — Phase 27: LLM/Ollama Backend Client
// =============================================================================

using json = nlohmann::json;

struct OllamaConfig {
    std::string host = "localhost";
    uint32_t    port = 11434;
    std::string chat_model = "deepseek-coder";
    std::string fim_model = "deepseek-coder:base";
    float       temperature = 0.7f;
    float       top_p = 0.9f;
    uint32_t    max_tokens = 2048;
    uint32_t    num_ctx = 8192;
    uint32_t    timeout_ms = 30000;
};

struct InferenceResult {
    bool        success = false;
    std::string model;
    std::string content;
    uint64_t    tokens_generated = 0;
    uint64_t    time_ms = 0;
    std::string error;
};

class AgentOllamaClient {
public:
    explicit AgentOllamaClient(const OllamaConfig& config);
    AgentOllamaClient(); // Default constructor
    ~AgentOllamaClient();

    // Identity / Status
    bool        TestConnection();
    std::string GetVersion();
    std::vector<std::string> ListModels();

    // Config Access
    const OllamaConfig& GetConfig() const { return m_config; }
    void                SetConfig(const OllamaConfig& cfg) { m_config = cfg; }

    // Blocking Inference
    InferenceResult Generate(const std::string& prompt);
    InferenceResult Chat(const std::vector<std::pair<std::string, std::string>>& messages);

    // Streaming Inference
    typedef void (*StreamCallback)(const std::string& chunk, void* userData);
    void        StreamChat(const std::vector<std::pair<std::string, std::string>>& messages,
                           StreamCallback callback, void* userData);
    void        CancelStream();

private:
    OllamaConfig m_config;
    std::atomic<bool> m_cancelStream{false};

#ifdef _WIN32
    void        InitWinHTTP();
    void        CleanupWinHTTP();
    HINTERNET   m_hSession = nullptr;
    HINTERNET   m_hConnect = nullptr;

    std::string MakePostRequest(const char* endpoint, const json& body, bool stream = false,
                                StreamCallback callback = nullptr, void* userData = nullptr);
    std::string MakeGetRequest(const char* endpoint);
#endif
};

} // namespace Agent
} // namespace RawrXD