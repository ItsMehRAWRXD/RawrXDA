// ============================================================================
// jsonrpc_parser.hpp — JSON-RPC 2.0 Parser
// ============================================================================
// Robust JSON-RPC 2.0 implementation for LSP and Ollama tool calling.
// Supports request/response ID correlation, batch requests, error objects.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <variant>
#include <optional>

// Uses nlohmann/json internally
#include <nlohmann/json.hpp>

namespace RawrXD {
namespace RPC {

using json = nlohmann::json;

// ============================================================================
// JSON-RPC ID — can be int, string, or null
// ============================================================================

using RequestId = std::variant<std::monostate, int64_t, std::string>;

std::string idToString(const RequestId& id);
json idToJson(const RequestId& id);
RequestId idFromJson(const json& j);

// ============================================================================
// Error Codes (JSON-RPC 2.0 + LSP extensions)
// ============================================================================

enum class ErrorCode : int {
    // JSON-RPC 2.0
    PARSE_ERROR      = -32700,
    INVALID_REQUEST  = -32600,
    METHOD_NOT_FOUND = -32601,
    INVALID_PARAMS   = -32602,
    INTERNAL_ERROR   = -32603,

    // LSP specific
    SERVER_NOT_INITIALIZED = -32002,
    UNKNOWN_ERROR_CODE     = -32001,
    REQUEST_CANCELLED      = -32800,
    CONTENT_MODIFIED       = -32801,
    SERVER_CANCELLED       = -32802,
    REQUEST_FAILED         = -32803,
};

// ============================================================================
// JSON-RPC Error
// ============================================================================

struct RPCError {
    int code;
    std::string message;
    json data;   // Optional additional data

    json toJson() const;
    static RPCError fromJson(const json& j);
    static RPCError make(ErrorCode code, const std::string& msg, const json& data = {});
};

// ============================================================================
// JSON-RPC Request
// ============================================================================

struct RPCRequest {
    RequestId id;           // Null for notifications
    std::string method;
    json params;            // Object or array
    bool isNotification;    // True if no ID (fire-and-forget)

    json toJson() const;
    static RPCRequest fromJson(const json& j);
    static RPCRequest makeRequest(RequestId id, const std::string& method, const json& params = {});
    static RPCRequest makeNotification(const std::string& method, const json& params = {});
};

// ============================================================================
// JSON-RPC Response
// ============================================================================

struct RPCResponse {
    RequestId id;
    json result;            // Present on success
    std::optional<RPCError> error;  // Present on error
    bool isError;

    json toJson() const;
    static RPCResponse fromJson(const json& j);
    static RPCResponse makeResult(RequestId id, const json& result);
    static RPCResponse makeError(RequestId id, ErrorCode code,
                                  const std::string& msg, const json& data = {});
};

// ============================================================================
// Batch Support
// ============================================================================

struct RPCBatch {
    std::vector<RPCRequest> requests;
    bool isBatch;

    static RPCBatch parse(const std::string& rawJson);
};

struct RPCBatchResponse {
    std::vector<RPCResponse> responses;

    std::string serialize() const;
};

// ============================================================================
// LSP Content-Length Framing
// ============================================================================

class LSPFrameParser {
public:
    // Feed raw bytes from pipe/socket. Calls handler for each complete message.
    using MessageHandler = std::function<void(const std::string& jsonBody)>;

    void feed(const char* data, size_t len, MessageHandler handler);
    void reset();

    // Encode a message with Content-Length header
    static std::string encode(const std::string& jsonBody);

private:
    std::string m_buffer;
    enum class State { HEADER, BODY } m_state = State::HEADER;
    size_t m_contentLength = 0;
};

// ============================================================================
// JSON-RPC Handler Registry
// ============================================================================

class RPCDispatcher {
public:
    using MethodHandler = std::function<json(const json& params)>;
    using NotificationHandler = std::function<void(const json& params)>;

    // Register a method (expects response)
    void onMethod(const std::string& method, MethodHandler handler);

    // Register a notification (no response)
    void onNotification(const std::string& method, NotificationHandler handler);

    // Dispatch a single request → response
    RPCResponse dispatch(const RPCRequest& request);

    // Dispatch a batch → batch response
    RPCBatchResponse dispatchBatch(const RPCBatch& batch);

    // Pending response correlation
    void registerPendingRequest(RequestId id,
                                 std::function<void(const RPCResponse&)> callback);
    void handleResponse(const RPCResponse& response);

    // Check if method is registered
    bool hasMethod(const std::string& method) const;

private:
    std::unordered_map<std::string, MethodHandler> m_methods;
    std::unordered_map<std::string, NotificationHandler> m_notifications;
    std::unordered_map<std::string, std::function<void(const RPCResponse&)>> m_pendingRequests;
    mutable std::mutex m_mutex;
};

} // namespace RPC
} // namespace RawrXD
