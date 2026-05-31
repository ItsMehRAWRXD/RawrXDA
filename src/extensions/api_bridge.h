/**
 * @file api_bridge.h
 * @brief Extension ↔ Host communication bridge
 * 
 * @author RawrXD Extension Team
 * @version 1.0.0
 */

#pragma once

#include <nlohmann/json.hpp>
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <queue>
#include <functional>
#include <future>
#include <chrono>

namespace RawrXD::Extensions {

// ============================================================================
// Message Types
// ============================================================================

enum class MessageType {
    Request,
    Response,
    Event
};

// ============================================================================
// Message Structure
// ============================================================================

struct Message {
    MessageType type;
    int64_t targetId;
    std::string data;
};

// ============================================================================
// Request/Response Structures
// ============================================================================

struct Request {
    int64_t id = -1;
    int64_t targetId = -1;
    std::string method;
    std::string params;
    std::chrono::steady_clock::time_point timestamp;
};

struct Response {
    int64_t id = -1;
    std::string result;
    bool success = false;
    std::string error;
    std::chrono::steady_clock::time_point timestamp;
};

// ============================================================================
// Handler Types
// ============================================================================

using RequestHandler = std::function<bool(int64_t targetId, const std::string& params, std::string& result)>;
using EventHandler = std::function<void(int64_t targetId, const std::string& data)>;

// ============================================================================
// API Bridge
// ============================================================================

class APIBridge {
public:
    APIBridge();
    ~APIBridge();
    
    // Request/Response
    int64_t sendRequest(int64_t targetId, const std::string& method,
                       const std::string& params);
    bool sendResponse(int64_t requestId, const std::string& result,
                     bool success, const std::string& error);
    bool sendEvent(const std::string& event, const std::string& data);
    
    // Subscriptions
    bool subscribe(int64_t subscriberId, const std::string& event);
    bool unsubscribe(int64_t subscriberId, const std::string& event);
    
    // Message processing
    bool processNextMessage();
    size_t getPendingMessageCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_messageQueue.size();
    }
    
    // Handler registration
    void registerRequestHandler(const std::string& method, const RequestHandler& handler);
    void unregisterRequestHandler(const std::string& method);
    void registerEventHandler(const std::string& event, const EventHandler& handler);
    
private:
    void handleRequest(int64_t targetId, const std::string& data);
    void handleResponse(int64_t targetId, const std::string& data);
    void handleEvent(int64_t targetId, const std::string& data);
    
    std::string serializeRequest(const Request& request);
    std::string serializeResponse(const Response& response);
    
    mutable std::mutex m_mutex;
    int64_t m_nextRequestId;
    
    std::queue<Message> m_messageQueue;
    std::map<int64_t, Request> m_pendingRequests;
    std::map<int64_t, std::promise<nlohmann::json>> m_responseWaiters;
    std::map<std::string, std::set<int64_t>> m_eventSubscribers;
    std::map<std::string, RequestHandler> m_requestHandlers;
    std::map<std::string, std::vector<EventHandler>> m_eventHandlers;
};

} // namespace RawrXD::Extensions
