// ============================================================================
// rawrxd_serve.h — Ollama-compatible HTTP server (Win32 HTTP Server API)
// ============================================================================
// Zero-dependency HTTP server using httpapi.lib / http.sys.
// Endpoints:  /api/tags, /api/generate, /api/chat, /api/pull, /api/delete,
//             /api/show, /api/ps
// Wire format: NDJSON streaming identical to Ollama's wire protocol.
// ============================================================================
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <http.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "rawrxd_model_registry.h"

namespace RawrXD {
namespace Serve {

// ============================================================================
// ChatMessage — Ollama chat message
// ============================================================================
struct ChatMessage {
    std::string role;    // "system", "user", "assistant"
    std::string content;
};

// ============================================================================
// GenerateRequest — parsed from /api/generate or /api/chat
// ============================================================================
struct GenerateRequest {
    std::string              model;
    std::string              prompt;          // /api/generate
    std::vector<ChatMessage> messages;        // /api/chat
    bool                     stream  = true;
    float                    temperature = 0.7f;
    int                      num_predict = 512;
    float                    top_p       = 0.9f;
    int                      top_k       = 40;
    float                    repeat_penalty = 1.1f;
    int                      seed = -1;
    std::string              system;         // system prompt override
};

// ============================================================================
// Token streaming callback
// ============================================================================
using StreamTokenFn = std::function<void(const std::string& token, bool done)>;

// ============================================================================
// Inference backend interface — pluggable
// ============================================================================
struct InferenceBackend {
    // Load model from path; returns true on success
    std::function<bool(const std::string& modelPath)> loadModel;

    // Unload current model
    std::function<void()> unloadModel;

    // Generate tokens, streaming via callback; returns full response
    std::function<std::string(const GenerateRequest& req,
                              StreamTokenFn onToken)> generate;

    // Check if a model is loaded
    std::function<bool()> isLoaded;

    // Get currently loaded model name
    std::function<std::string()> currentModel;
};

// ============================================================================
// ServeConfig
// ============================================================================
struct ServeConfig {
    std::string host = "127.0.0.1";
    uint16_t    port = 11434;
    int         maxConcurrent = 4;
    std::vector<std::string> modelDirs;
};

// ============================================================================
// RawrXDServer — Win32 httpapi server
// ============================================================================
class RawrXDServer {
public:
    RawrXDServer();
    ~RawrXDServer();

    // Non-copyable
    RawrXDServer(const RawrXDServer&) = delete;
    RawrXDServer& operator=(const RawrXDServer&) = delete;

    // Lifecycle
    bool start(const ServeConfig& cfg, InferenceBackend backend);
    void stop();
    bool isRunning() const { return m_running.load(); }

    // Access registry
    ModelRegistry& registry() { return m_registry; }

private:
    // HTTP processing
    void requestLoop();
    void handleRequest(HTTP_REQUEST* req);

    // Route handlers
    void handleApiTags(HTTP_REQUEST* req);
    void handleApiGenerate(HTTP_REQUEST* req);
    void handleApiChat(HTTP_REQUEST* req);
    void handleApiShow(HTTP_REQUEST* req);
    void handleApiDelete(HTTP_REQUEST* req);
    void handleApiPs(HTTP_REQUEST* req);
    void handleNotFound(HTTP_REQUEST* req);

    // HTTP helpers
    std::string readRequestBody(HTTP_REQUEST* req);
    void sendResponse(HTTP_REQUEST* req, int statusCode,
                      const std::string& contentType,
                      const std::string& body);
    void sendStreamChunk(HTTP_REQUEST* req, HTTP_REQUEST_ID requestId,
                         const std::string& chunk);
    void sendStreamEnd(HTTP_REQUEST* req, HTTP_REQUEST_ID requestId);

    // JSON helpers
    std::string buildTagsJson() const;
    std::string buildShowJson(const ModelEntry& entry) const;
    std::string buildPsJson() const;
    std::string buildGenerateChunk(const std::string& model,
                                    const std::string& token,
                                    bool done) const;
    std::string buildChatChunk(const std::string& model,
                                const std::string& token,
                                bool done) const;
    GenerateRequest parseGenerateRequest(const std::string& body) const;
    GenerateRequest parseChatRequest(const std::string& body) const;

    // State
    std::atomic<bool>   m_running{false};
    HANDLE              m_reqQueue = nullptr;
    HTTP_SERVER_SESSION_ID m_sessionId = 0;
    HTTP_URL_GROUP_ID   m_urlGroupId = 0;
    std::thread         m_listenerThread;
    ServeConfig         m_config;
    InferenceBackend    m_backend;
    ModelRegistry       m_registry;
    std::string         m_currentModel;
    mutable std::mutex  m_inferenceMu;

    // Request buffer
    static constexpr size_t REQ_BUF_SIZE = 8192;
};

} // namespace Serve
} // namespace RawrXD
