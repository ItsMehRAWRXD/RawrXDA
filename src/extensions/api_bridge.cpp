/**
 * @file api_bridge.cpp
 * @brief Extension ↔ Host communication bridge
 * 
 * Provides:
 * - Bidirectional message passing
 * - Request/response pattern
 * - Event broadcasting
 * - Serialization/deserialization
 * 
 * @author RawrXD Extension Team
 * @version 1.0.0
 */

#include "api_bridge.h"
#include <nlohmann/json.hpp>
#include <chrono>

namespace RawrXD::Extensions {

// ============================================================================
// APIBridge Implementation
// ============================================================================

APIBridge::APIBridge() : m_nextRequestId(1) {
}

APIBridge::~APIBridge() = default;

// ============================================================================
// Request/Response
// ============================================================================

int64_t APIBridge::sendRequest(int64_t targetId, const std::string& method,
                              const std::string& params) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    int64_t requestId = m_nextRequestId++;
    
    Request request;
    request.id = requestId;
    request.targetId = targetId;
    request.method = method;
    request.params = params;
    request.timestamp = std::chrono::steady_clock::now();
    
    m_pendingRequests[requestId] = request;
    
    // In production, this would send over IPC
    // For now, we queue it for processing
    m_messageQueue.push({MessageType::Request, targetId, serializeRequest(request)});
    
    return requestId;
}

bool APIBridge::sendResponse(int64_t requestId, const std::string& result,
                            bool success, const std::string& error) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_pendingRequests.find(requestId);
    if (it == m_pendingRequests.end()) {
        return false;
    }
    
    Response response;
    response.id = requestId;
    response.result = result;
    response.success = success;
    response.error = error;
    response.timestamp = std::chrono::steady_clock::now();
    
    m_pendingRequests.erase(it);
    
    // Queue response
    m_messageQueue.push({MessageType::Response, it->second.targetId, 
                        serializeResponse(response)});
    
    return true;
}

bool APIBridge::sendEvent(const std::string& event, const std::string& data) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    nlohmann::json msg;
    msg["type"] = "event";
    msg["event"] = event;
    msg["data"] = data;
    msg["timestamp"] = std::chrono::steady_clock::now().time_since_epoch().count();
    
    // Broadcast to all subscribers
    auto it = m_eventSubscribers.find(event);
    if (it != m_eventSubscribers.end()) {
        for (int64_t subscriberId : it->second) {
            m_messageQueue.push({MessageType::Event, subscriberId, msg.dump()});
        }
    }
    
    return true;
}

// ============================================================================
// Subscription Management
// ============================================================================

bool APIBridge::subscribe(int64_t subscriberId, const std::string& event) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_eventSubscribers[event].insert(subscriberId);
    return true;
}

bool APIBridge::unsubscribe(int64_t subscriberId, const std::string& event) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_eventSubscribers.find(event);
    if (it == m_eventSubscribers.end()) {
        return false;
    }
    
    it->second.erase(subscriberId);
    if (it->second.empty()) {
        m_eventSubscribers.erase(it);
    }
    
    return true;
}

// ============================================================================
// Message Processing
// ============================================================================

bool APIBridge::processNextMessage() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_messageQueue.empty()) {
        return false;
    }
    
    Message msg = m_messageQueue.front();
    m_messageQueue.pop();
    
    switch (msg.type) {
        case MessageType::Request:
            handleRequest(msg.targetId, msg.data);
            break;
        case MessageType::Response:
            handleResponse(msg.targetId, msg.data);
            break;
        case MessageType::Event:
            handleEvent(msg.targetId, msg.data);
            break;
    }
    
    return true;
}

void APIBridge::handleRequest(int64_t targetId, const std::string& data) {
    // Parse request
    try {
        nlohmann::json req = nlohmann::json::parse(data);
        std::string method = req.value("method", "");
        std::string params = req.value("params", "");
        int64_t requestId = req.value("id", 0);
        
        // Route to handler
        auto it = m_requestHandlers.find(method);
        if (it != m_requestHandlers.end()) {
            std::string result;
            bool success = it->second(targetId, params, result);
            sendResponse(requestId, result, success, "");
        } else {
            sendResponse(requestId, "", false, "Method not found: " + method);
        }
    } catch (...) {
        // Invalid request
    }
}

void APIBridge::handleResponse(int64_t targetId, const std::string& data) {
    // Parse response
    try {
        nlohmann::json resp = nlohmann::json::parse(data);
        int64_t requestId = resp.value("id", 0);
        
        // Notify waiter
        auto it = m_responseWaiters.find(requestId);
        if (it != m_responseWaiters.end()) {
            it->second.set_value(resp);
            m_responseWaiters.erase(it);
        }
    } catch (...) {
        // Invalid response
    }
}

void APIBridge::handleEvent(int64_t targetId, const std::string& data) {
    // Parse event
    try {
        nlohmann::json event = nlohmann::json::parse(data);
        std::string eventName = event.value("event", "");
        
        // Route to handlers
        auto it = m_eventHandlers.find(eventName);
        if (it != m_eventHandlers.end()) {
            for (const auto& handler : it->second) {
                handler(targetId, event.value("data", ""));
            }
        }
    } catch (...) {
        // Invalid event
    }
}

// ============================================================================
// Handler Registration
// ============================================================================

void APIBridge::registerRequestHandler(const std::string& method,
                                      const RequestHandler& handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_requestHandlers[method] = handler;
}

void APIBridge::unregisterRequestHandler(const std::string& method) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_requestHandlers.erase(method);
}

void APIBridge::registerEventHandler(const std::string& event,
                                    const EventHandler& handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_eventHandlers[event].push_back(handler);
}

// ============================================================================
// Serialization
// ============================================================================

std::string APIBridge::serializeRequest(const Request& request) {
    nlohmann::json req;
    req["id"] = request.id;
    req["targetId"] = request.targetId;
    req["method"] = request.method;
    req["params"] = request.params;
    req["timestamp"] = request.timestamp.time_since_epoch().count();
    return req.dump();
}

std::string APIBridge::serializeResponse(const Response& response) {
    nlohmann::json resp;
    resp["id"] = response.id;
    resp["result"] = response.result;
    resp["success"] = response.success;
    resp["error"] = response.error;
    resp["timestamp"] = response.timestamp.time_since_epoch().count();
    return resp.dump();
}

} // namespace RawrXD::Extensions
