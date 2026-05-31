// ============================================================================
// API Gateway Manager — API Management and Orchestration
// Centralized API management with rate limiting and monitoring
// ============================================================================
#pragma once
#include "../inference/SovereignInferenceClient.h"
#include "../performance/realtime_profiler.h"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <queue>
#include <chrono>

namespace RawrXD::API {

enum class APIMethod {
    GET,
    POST,
    PUT,
    DELETE,
    PATCH,
    HEAD,
    OPTIONS
};

enum class APIStatus {
    ACTIVE,
    DEPRECATED,
    SUNSET,
    EXPERIMENTAL
};

struct APIEndpoint {
    std::string path;
    APIMethod method;
    std::string description;
    std::vector<std::string> parameters;
    std::string responseSchema;
    APIStatus status;
    int rateLimit;
    bool requiresAuth;
    std::chrono::system_clock::time_point createdAt;
};

struct APIRequest {
    std::string id;
    std::string endpoint;
    std::map<std::string, std::string> headers;
    std::string body;
    std::chrono::system_clock::time_point receivedAt;
    std::string clientId;
};

struct APIResponse {
    int statusCode;
    std::map<std::string, std::string> headers;
    std::string body;
    std::chrono::system_clock::time_point processedAt;
    int processingTimeMs;
};

struct RateLimitInfo {
    std::string clientId;
    int requestsRemaining;
    int resetTime;
    int limit;
    int window;
};

struct APIMetrics {
    int totalRequests;
    int successfulRequests;
    int failedRequests;
    double averageLatency;
    std::map<int, int> statusCodeDistribution;
    std::chrono::system_clock::time_point measuredAt;
};

class APIGatewayManager {
public:
    APIGatewayManager() {
        InitializeRateLimits();
    }

    void RegisterEndpoint(const APIEndpoint& endpoint) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::string key = GetEndpointKey(endpoint.method, endpoint.path);
        m_endpoints[key] = endpoint;
    }

    APIResponse ProcessRequest(const APIRequest& request) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        APIResponse response;
        response.processedAt = std::chrono::system_clock::now();
        
        // Check rate limit
        if (!CheckRateLimit(request.clientId)) {
            response.statusCode = 429; // Too Many Requests
            response.body = "{\"error\":\"Rate limit exceeded\"}";
            return response;
        }
        
        // Find endpoint
        auto method = ParseMethod(request.endpoint);
        auto key = GetEndpointKey(method, request.endpoint);
        auto it = m_endpoints.find(key);
        
        if (it == m_endpoints.end()) {
            response.statusCode = 404;
            response.body = "{\"error\":\"Endpoint not found\"}";
            return response;
        }
        
        // Check authentication
        if (it->second.requiresAuth && !AuthenticateRequest(request)) {
            response.statusCode = 401;
            response.body = "{\"error\":\"Unauthorized\"}";
            return response;
        }
        
        // Process request
        auto startTime = std::chrono::steady_clock::now();
        response = RouteRequest(request, it->second);
        auto endTime = std::chrono::steady_clock::now();
        
        response.processingTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime).count();
        
        // Update metrics
        UpdateMetrics(request, response);
        
        return response;
    }

    RateLimitInfo GetRateLimitInfo(const std::string& clientId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        RateLimitInfo info;
        info.clientId = clientId;
        
        auto it = m_rateLimits.find(clientId);
        if (it != m_rateLimits.end()) {
            info.requestsRemaining = it->second.remaining;
            info.limit = it->second.limit;
            info.window = it->second.window;
        } else {
            info.requestsRemaining = 100; // Default
            info.limit = 100;
            info.window = 3600; // 1 hour
        }
        
        return info;
    }

    void SetRateLimit(const std::string& clientId, int limit, int window) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        RateLimitEntry entry;
        entry.limit = limit;
        entry.window = window;
        entry.remaining = limit;
        entry.resetTime = std::chrono::system_clock::now() + std::chrono::seconds(window);
        
        m_rateLimits[clientId] = entry;
    }

    APIMetrics GetMetrics() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_metrics;
    }

    std::vector<APIEndpoint> GetEndpoints() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::vector<APIEndpoint> endpoints;
        for (const auto& [key, endpoint] : m_endpoints) {
            endpoints.push_back(endpoint);
        }
        return endpoints;
    }

    std::string GenerateAPIDocumentation() {
        std::ostringstream doc;
        doc << "# API Documentation\n\n";
        doc << "## Endpoints\n\n";
        
        std::lock_guard<std::mutex> lock(m_mutex);
        
        for (const auto& [key, endpoint] : m_endpoints) {
            doc << "### " << MethodToString(endpoint.method) << " " << endpoint.path << "\n\n";
            doc << endpoint.description << "\n\n";
            doc << "**Status:** " << StatusToString(endpoint.status) << "\n";
            doc << "**Rate Limit:** " << endpoint.rateLimit << " requests/minute\n";
            doc << "**Authentication:** " << (endpoint.requiresAuth ? "Required" : "Optional") << "\n\n";
            
            if (!endpoint.parameters.empty()) {
                doc << "**Parameters:**\n";
                for (const auto& param : endpoint.parameters) {
                    doc << "- `" << param << "`\n";
                }
                doc << "\n";
            }
        }
        
        return doc.str();
    }

private:
    mutable std::mutex m_mutex;
    std::map<std::string, APIEndpoint> m_endpoints;
    APIMetrics m_metrics;
    
    struct RateLimitEntry {
        int limit;
        int window;
        int remaining;
        std::chrono::system_clock::time_point resetTime;
    };
    std::map<std::string, RateLimitEntry> m_rateLimits;

    void InitializeRateLimits() {
        // Default rate limits
        SetRateLimit("default", 100, 3600); // 100 requests per hour
    }

    std::string GetEndpointKey(APIMethod method, const std::string& path) {
        return MethodToString(method) + ":" + path;
    }

    APIMethod ParseMethod(const std::string& endpoint) {
        // Parse method from endpoint string
        return APIMethod::GET; // Default
    }

    bool CheckRateLimit(const std::string& clientId) {
        auto it = m_rateLimits.find(clientId);
        if (it == m_rateLimits.end()) {
            // Use default
            it = m_rateLimits.find("default");
            if (it == m_rateLimits.end()) {
                return true; // No limit
            }
        }
        
        auto& entry = it->second;
        
        // Check if window has reset
        if (std::chrono::system_clock::now() > entry.resetTime) {
            entry.remaining = entry.limit;
            entry.resetTime = std::chrono::system_clock::now() + std::chrono::seconds(entry.window);
        }
        
        if (entry.remaining <= 0) {
            return false;
        }
        
        entry.remaining--;
        return true;
    }

    bool AuthenticateRequest(const APIRequest& request) {
        // Check authentication token
        auto authIt = request.headers.find("Authorization");
        if (authIt == request.headers.end()) {
            return false;
        }
        
        // Validate token
        return ValidateToken(authIt->second);
    }

    bool ValidateToken(const std::string& token) {
        // Token validation logic
        return !token.empty();
    }

    APIResponse RouteRequest(const APIRequest& request, const APIEndpoint& endpoint) {
        APIResponse response;
        
        // Route to appropriate handler
        // This would integrate with your backend services
        
        response.statusCode = 200;
        response.body = "{\"status\":\"success\"}";
        
        return response;
    }

    void UpdateMetrics(const APIRequest& request, const APIResponse& response) {
        m_metrics.totalRequests++;
        
        if (response.statusCode >= 200 && response.statusCode < 300) {
            m_metrics.successfulRequests++;
        } else {
            m_metrics.failedRequests++;
        }
        
        m_metrics.statusCodeDistribution[response.statusCode]++;
        
        // Update average latency
        if (m_metrics.totalRequests > 1) {
            m_metrics.averageLatency = (m_metrics.averageLatency * (m_metrics.totalRequests - 1) + 
                                       response.processingTimeMs) / m_metrics.totalRequests;
        } else {
            m_metrics.averageLatency = response.processingTimeMs;
        }
        
        m_metrics.measuredAt = std::chrono::system_clock::now();
    }

    std::string MethodToString(APIMethod method) {
        switch (method) {
            case APIMethod::GET: return "GET";
            case APIMethod::POST: return "POST";
            case APIMethod::PUT: return "PUT";
            case APIMethod::DELETE: return "DELETE";
            case APIMethod::PATCH: return "PATCH";
            case APIMethod::HEAD: return "HEAD";
            case APIMethod::OPTIONS: return "OPTIONS";
            default: return "UNKNOWN";
        }
    }

    std::string StatusToString(APIStatus status) {
        switch (status) {
            case APIStatus::ACTIVE: return "Active";
            case APIStatus::DEPRECATED: return "Deprecated";
            case APIStatus::SUNSET: return "Sunset";
            case APIStatus::EXPERIMENTAL: return "Experimental";
            default: return "Unknown";
        }
    }
};

} // namespace RawrXD::API
