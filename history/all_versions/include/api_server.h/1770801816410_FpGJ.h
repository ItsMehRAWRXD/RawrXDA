#pragma once

#include <atomic>
#include <thread>
#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <mutex>
#include <queue>
#include <functional>
#include <unordered_map>

// Forward declarations
struct AppState;

// Simple JSON-like structure for request parsing
struct JsonValue {
    std::string string_value;
    std::vector<JsonValue> array_value;
    std::unordered_map<std::string, JsonValue> object_value;
    bool is_string = false;
    bool is_array = false;
    bool is_object = false;
};

// Chat message structure for OpenAI compatibility
struct ChatMessage {
    std::string role;
    std::string content;
    std::string name;
};

// HTTP request structure
struct HttpRequest {
    std::string method;
    std::string path;
    std::string body;
    std::chrono::steady_clock::time_point received_time;
    std::string client_id;
};

// HTTP response structure 
struct HttpResponse {
    int status_code;
    std::string body;
    std::string content_type;
};

// ============================================================================
// WebSocket Client — Represents a connected WebSocket client
// ============================================================================
struct WSClient {
    uint64_t    id;
    void*       socketHandle;       // SOCKET or handle, platform-specific
    std::string clientId;
    bool        connected;
    bool        subscribedMemory;   // Subscribed to memory stats push
    bool        subscribedModel;    // Subscribed to model state push
    bool        subscribedPatches;  // Subscribed to patch events push
    bool        subscribedEvents;   // Subscribed to all events
    uint64_t    lastEventSequence;  // For event polling/reconciliation
    uint64_t    connectedAt;        // GetTickCount64() timestamp
    uint64_t    lastPingSent;
    uint64_t    lastPongReceived;
    std::string remoteAddr;
};

// ============================================================================
// WebSocket Frame — Parsed WS frame
// ============================================================================
struct WSFrame {
    enum class Opcode : uint8_t {
        Continuation = 0x0,
        Text         = 0x1,
        Binary       = 0x2,
        Close        = 0x8,
        Ping         = 0x9,
        Pong         = 0xA,
    };
    Opcode      opcode;
    bool        fin;
    bool        masked;
    uint8_t     maskKey[4];
    std::string payload;
};

class APIServer {
public:
    explicit APIServer(AppState& app_state);
    ~APIServer();

    bool Start(uint16_t port);
    bool Stop();
    bool IsRunning() const { return is_running_.load(); }
    uint16_t GetPort() const { return port_; }

    // Request handlers
    void HandleGenerateRequest(const std::string& request, std::string& response);
    void HandleChatCompletionsRequest(const std::string& request, std::string& response);
    void HandleTagsRequest(std::string& response);
    void HandlePullRequest(const std::string& request, std::string& response);

    // Inference utilities
    std::string GenerateCompletion(const std::string& prompt);
    std::string GenerateChatCompletion(const std::vector<ChatMessage>& messages);

    // ---- WebSocket Support ----

    // WebSocket upgrade handler — called when HTTP upgrade request is detected
    bool HandleWebSocketUpgrade(const HttpRequest& request, void* clientSocket);

    // Push state to all subscribed WebSocket clients
    void BroadcastMemoryStats();
    void BroadcastModelState();
    void BroadcastPatchEvent(const std::string& eventJson);
    void BroadcastFullState();

    // Send a WebSocket text frame to a specific client
    bool SendWSText(uint64_t clientId, const std::string& payload);

    // ---- Full State Snapshot (for reconnection reconciliation) ----
    // Returns the full shared state as JSON from the MMF
    std::string GetFullStateJson() const;

    // Get connected WebSocket client count
    size_t GetWSClientCount() const;

private:
    // Production HTTP server utilities
    void InitializeHttpServer();
    void ProcessPendingRequests();
    void HandleClientConnections();

    // ---- WebSocket Internals ----
    void WSPushThread();                // Background thread for state push
    void WSCleanupDeadClients();        // Remove disconnected clients
    bool WSHandshake(const HttpRequest& request, void* clientSocket);
    WSFrame WSParseFrame(const uint8_t* data, size_t len, size_t* consumed);
    std::vector<uint8_t> WSEncodeFrame(WSFrame::Opcode opcode, const std::string& payload);
    void WSProcessMessage(uint64_t clientId, const std::string& message);
    std::string WSComputeAcceptKey(const std::string& clientKey);

    // JSON parsing utilities
    JsonValue ParseJsonRequest(const std::string& request);
    std::string ExtractPromptFromRequest(const JsonValue& request);
    std::vector<ChatMessage> ExtractMessagesFromRequest(const JsonValue& request);
    bool ValidateMessageFormat(const std::vector<ChatMessage>& messages);
    
    // Response generation utilities
    std::string CreateErrorResponse(const std::string& error_message);
    std::string CreateGenerateResponse(const std::string& completion);
    std::string CreateChatCompletionResponse(const std::string& completion, const JsonValue& request);
    
    // Logging utilities
    void LogRequestReceived(const std::string& endpoint, size_t content_length);
    void LogRequestCompleted(const std::string& endpoint, size_t response_length);
    void LogRequestError(const std::string& endpoint, const std::string& error);
    void LogServerError(const std::string& error);
    
    // Performance monitoring utilities
    void RecordRequestMetrics(const std::string& endpoint, 
                            std::chrono::milliseconds duration,
                            bool success);
    void UpdateConnectionMetrics(int active_connections);
    
    // Security utilities
    bool ValidateRequest(const HttpRequest& request);
    bool CheckRateLimit(const std::string& client_id);
    void UpdateRateLimit(const std::string& client_id);

    // CoT engine proxy — bridges to Python rawrxd_cot_engine.py Flask backend
    bool ProxyToCotEngine(const std::string& request_body, std::string& response,
                          const std::string& path = "/api/cot",
                          const std::string& method = "POST");

private:
    AppState& app_state_;
    std::atomic<bool> is_running_;
    uint16_t port_;
    std::unique_ptr<std::thread> server_thread_;
    
    // Connection management
    std::queue<HttpRequest> pending_requests_;
    std::mutex request_queue_mutex_;
    std::atomic<int> active_connections_{0};

    // ---- WebSocket State ----
    std::unordered_map<uint64_t, std::unique_ptr<WSClient>> ws_clients_;
    mutable std::mutex ws_clients_mutex_;
    std::unique_ptr<std::thread> ws_push_thread_;
    std::atomic<bool> ws_push_running_{false};
    uint64_t ws_next_client_id_{1};
    
    // Push interval configuration
    uint32_t ws_memory_push_interval_ms_{500};   // 500ms for memory stats
    uint32_t ws_model_push_interval_ms_{2000};   // 2s for model state
    uint32_t ws_event_poll_interval_ms_{100};     // 100ms for event polling
    uint64_t ws_last_event_sequence_{0};          // Global event cursor

    // Rate limiting
    std::mutex rate_limit_mutex_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_request_times_;
    std::unordered_map<std::string, int> request_counts_;
    
    // Performance metrics
    std::atomic<uint64_t> total_requests_{0};
    std::atomic<uint64_t> successful_requests_{0};
    std::atomic<uint64_t> failed_requests_{0};
    std::chrono::steady_clock::time_point start_time_;
    std::atomic<uint64_t> ws_messages_sent_{0};
    std::atomic<uint64_t> ws_messages_received_{0};
};