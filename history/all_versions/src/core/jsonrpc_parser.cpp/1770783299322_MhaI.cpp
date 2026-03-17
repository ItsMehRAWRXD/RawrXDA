// ============================================================================
// jsonrpc_parser.cpp — JSON-RPC 2.0 Parser Implementation
// ============================================================================
// Full JSON-RPC 2.0 + LSP Content-Length framing support.
// Uses nlohmann/json for parsing. No exceptions. PatchResult-style.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "core/jsonrpc_parser.hpp"

#include <sstream>
#include <cstring>
#include <algorithm>

namespace RawrXD {
namespace RPC {

// ============================================================================
// RequestId Helpers
// ============================================================================

std::string idToString(const RequestId& id) {
    if (std::holds_alternative<std::monostate>(id)) return "null";
    if (std::holds_alternative<int64_t>(id)) return std::to_string(std::get<int64_t>(id));
    return std::get<std::string>(id);
}

json idToJson(const RequestId& id) {
    if (std::holds_alternative<std::monostate>(id)) return nullptr;
    if (std::holds_alternative<int64_t>(id)) return std::get<int64_t>(id);
    return std::get<std::string>(id);
}

RequestId idFromJson(const json& j) {
    if (j.is_null()) return std::monostate{};
    if (j.is_number_integer()) return j.get<int64_t>();
    if (j.is_string()) return j.get<std::string>();
    return std::monostate{};
}

// ============================================================================
// RPCError
// ============================================================================

json RPCError::toJson() const {
    json j;
    j["code"] = code;
    j["message"] = message;
    if (!data.is_null()) {
        j["data"] = data;
    }
    return j;
}

RPCError RPCError::fromJson(const json& j) {
    RPCError err;
    err.code = j.value("code", 0);
    err.message = j.value("message", "Unknown error");
    if (j.contains("data")) {
        err.data = j["data"];
    }
    return err;
}

RPCError RPCError::make(ErrorCode code, const std::string& msg, const json& data) {
    return {static_cast<int>(code), msg, data};
}

// ============================================================================
// RPCRequest
// ============================================================================

json RPCRequest::toJson() const {
    json j;
    j["jsonrpc"] = "2.0";
    j["method"] = method;

    if (!params.is_null()) {
        j["params"] = params;
    }

    if (!isNotification) {
        j["id"] = idToJson(id);
    }

    return j;
}

RPCRequest RPCRequest::fromJson(const json& j) {
    RPCRequest req;
    req.method = j.value("method", "");

    if (j.contains("params")) {
        req.params = j["params"];
    }

    if (j.contains("id")) {
        req.id = idFromJson(j["id"]);
        req.isNotification = false;
    } else {
        req.isNotification = true;
    }

    return req;
}

RPCRequest RPCRequest::makeRequest(RequestId id, const std::string& method, const json& params) {
    RPCRequest req;
    req.id = id;
    req.method = method;
    req.params = params;
    req.isNotification = false;
    return req;
}

RPCRequest RPCRequest::makeNotification(const std::string& method, const json& params) {
    RPCRequest req;
    req.method = method;
    req.params = params;
    req.isNotification = true;
    return req;
}

// ============================================================================
// RPCResponse
// ============================================================================

json RPCResponse::toJson() const {
    json j;
    j["jsonrpc"] = "2.0";
    j["id"] = idToJson(id);

    if (isError && error.has_value()) {
        j["error"] = error->toJson();
    } else {
        j["result"] = result;
    }

    return j;
}

RPCResponse RPCResponse::fromJson(const json& j) {
    RPCResponse resp;
    resp.id = j.contains("id") ? idFromJson(j["id"]) : RequestId{};

    if (j.contains("error")) {
        resp.error = RPCError::fromJson(j["error"]);
        resp.isError = true;
    } else {
        resp.result = j.value("result", json{});
        resp.isError = false;
    }

    return resp;
}

RPCResponse RPCResponse::makeResult(RequestId id, const json& result) {
    RPCResponse resp;
    resp.id = id;
    resp.result = result;
    resp.isError = false;
    return resp;
}

RPCResponse RPCResponse::makeError(RequestId id, ErrorCode code,
                                    const std::string& msg, const json& data) {
    RPCResponse resp;
    resp.id = id;
    resp.error = RPCError::make(code, msg, data);
    resp.isError = true;
    return resp;
}

// ============================================================================
// RPCBatch
// ============================================================================

RPCBatch RPCBatch::parse(const std::string& rawJson) {
    RPCBatch batch;

    json parsed;
    try {
        parsed = json::parse(rawJson, nullptr, false);
    } catch (...) {
        // Parse error — return empty batch
        batch.isBatch = false;
        return batch;
    }

    if (parsed.is_discarded()) {
        batch.isBatch = false;
        return batch;
    }

    if (parsed.is_array()) {
        batch.isBatch = true;
        for (const auto& item : parsed) {
            batch.requests.push_back(RPCRequest::fromJson(item));
        }
    } else if (parsed.is_object()) {
        batch.isBatch = false;
        // Check if it's a response or request
        if (parsed.contains("method")) {
            batch.requests.push_back(RPCRequest::fromJson(parsed));
        }
    }

    return batch;
}

std::string RPCBatchResponse::serialize() const {
    if (responses.empty()) return "";
    if (responses.size() == 1) {
        return responses[0].toJson().dump();
    }

    json arr = json::array();
    for (const auto& resp : responses) {
        arr.push_back(resp.toJson());
    }
    return arr.dump();
}

// ============================================================================
// LSP Content-Length Framing
// ============================================================================

void LSPFrameParser::feed(const char* data, size_t len, MessageHandler handler) {
    m_buffer.append(data, len);

    while (!m_buffer.empty()) {
        if (m_state == State::HEADER) {
            // Look for \r\n\r\n separator
            auto sep = m_buffer.find("\r\n\r\n");
            if (sep == std::string::npos) return;  // Need more data

            // Parse Content-Length from headers
            std::string headers = m_buffer.substr(0, sep);
            m_contentLength = 0;

            // Find Content-Length header (case-insensitive)
            std::string lowerHeaders = headers;
            std::transform(lowerHeaders.begin(), lowerHeaders.end(),
                          lowerHeaders.begin(), ::tolower);
            auto clPos = lowerHeaders.find("content-length:");
            if (clPos != std::string::npos) {
                auto valStart = clPos + 15;  // strlen("content-length:")
                while (valStart < lowerHeaders.size() && lowerHeaders[valStart] == ' ')
                    ++valStart;
                m_contentLength = std::stoull(headers.substr(valStart));
            }

            m_buffer.erase(0, sep + 4);
            m_state = State::BODY;
        }

        if (m_state == State::BODY) {
            if (m_buffer.size() < m_contentLength) return;  // Need more data

            std::string body = m_buffer.substr(0, m_contentLength);
            m_buffer.erase(0, m_contentLength);
            m_state = State::HEADER;

            if (handler) handler(body);
        }
    }
}

void LSPFrameParser::reset() {
    m_buffer.clear();
    m_state = State::HEADER;
    m_contentLength = 0;
}

std::string LSPFrameParser::encode(const std::string& jsonBody) {
    std::ostringstream out;
    out << "Content-Length: " << jsonBody.size() << "\r\n\r\n" << jsonBody;
    return out.str();
}

// ============================================================================
// RPCDispatcher
// ============================================================================

void RPCDispatcher::onMethod(const std::string& method, MethodHandler handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_methods[method] = std::move(handler);
}

void RPCDispatcher::onNotification(const std::string& method, NotificationHandler handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_notifications[method] = std::move(handler);
}

RPCResponse RPCDispatcher::dispatch(const RPCRequest& request) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (request.isNotification) {
        auto it = m_notifications.find(request.method);
        if (it != m_notifications.end()) {
            it->second(request.params);
        }
        // Notifications don't get responses
        return RPCResponse::makeResult(std::monostate{}, nullptr);
    }

    auto it = m_methods.find(request.method);
    if (it == m_methods.end()) {
        return RPCResponse::makeError(request.id, ErrorCode::METHOD_NOT_FOUND,
                                       "Method not found: " + request.method);
    }

    json result = it->second(request.params);
    return RPCResponse::makeResult(request.id, result);
}

RPCBatchResponse RPCDispatcher::dispatchBatch(const RPCBatch& batch) {
    RPCBatchResponse batchResp;
    for (const auto& req : batch.requests) {
        auto resp = dispatch(req);
        // Don't include responses for notifications
        if (!req.isNotification) {
            batchResp.responses.push_back(std::move(resp));
        }
    }
    return batchResp;
}

void RPCDispatcher::registerPendingRequest(RequestId id,
                                            std::function<void(const RPCResponse&)> callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pendingRequests[idToString(id)] = std::move(callback);
}

void RPCDispatcher::handleResponse(const RPCResponse& response) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string key = idToString(response.id);
    auto it = m_pendingRequests.find(key);
    if (it != m_pendingRequests.end()) {
        it->second(response);
        m_pendingRequests.erase(it);
    }
}

bool RPCDispatcher::hasMethod(const std::string& method) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_methods.count(method) > 0 || m_notifications.count(method) > 0;
}

} // namespace RPC
} // namespace RawrXD
