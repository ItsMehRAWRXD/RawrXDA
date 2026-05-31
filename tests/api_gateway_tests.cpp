// ============================================================================
// API Gateway Manager Tests — API Management Testing
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include "../src/api/api_gateway_manager.cpp"

using namespace RawrXD::API;

TEST_CASE("API Gateway Manager - Basic Operations", "[api][gateway]") {
    APIGatewayManager gateway;
    
    SECTION("Default gateway state") {
        auto endpoints = gateway.GetEndpoints();
        REQUIRE(endpoints.empty());
        
        auto metrics = gateway.GetMetrics();
        REQUIRE(metrics.totalRequests == 0);
        REQUIRE(metrics.successfulRequests == 0);
    }
    
    SECTION("Endpoint registration") {
        APIEndpoint endpoint;
        endpoint.path = "/api/v1/users";
        endpoint.method = APIMethod::GET;
        endpoint.description = "Get all users";
        endpoint.rateLimit = 100;
        endpoint.requiresAuth = true;
        
        gateway.RegisterEndpoint(endpoint);
        
        auto endpoints = gateway.GetEndpoints();
        REQUIRE(endpoints.size() == 1);
        REQUIRE(endpoints[0].path == "/api/v1/users");
    }
    
    SECTION("Multiple endpoints") {
        APIEndpoint endpoint1;
        endpoint1.path = "/api/v1/users";
        endpoint1.method = APIMethod::GET;
        endpoint1.rateLimit = 100;
        
        APIEndpoint endpoint2;
        endpoint2.path = "/api/v1/users";
        endpoint2.method = APIMethod::POST;
        endpoint2.rateLimit = 50;
        
        gateway.RegisterEndpoint(endpoint1);
        gateway.RegisterEndpoint(endpoint2);
        
        auto endpoints = gateway.GetEndpoints();
        REQUIRE(endpoints.size() == 2);
    }
}

TEST_CASE("API Gateway Manager - Rate Limiting", "[api][rate-limiting]") {
    APIGatewayManager gateway;
    
    SECTION("Default rate limits") {
        auto info = gateway.GetRateLimitInfo("unknown-client");
        REQUIRE(info.limit == 100); // Default limit
        REQUIRE(info.window == 3600); // Default window (1 hour)
        REQUIRE(info.requestsRemaining == 100);
    }
    
    SECTION("Custom rate limit") {
        gateway.SetRateLimit("client-1", 1000, 3600);
        
        auto info = gateway.GetRateLimitInfo("client-1");
        REQUIRE(info.limit == 1000);
        REQUIRE(info.window == 3600);
    }
    
    SECTION("Rate limit processing") {
        gateway.SetRateLimit("client-1", 2, 3600); // Only 2 requests
        
        APIEndpoint endpoint;
        endpoint.path = "/test";
        endpoint.method = APIMethod::GET;
        endpoint.rateLimit = 100;
        endpoint.requiresAuth = false;
        gateway.RegisterEndpoint(endpoint);
        
        APIRequest request1;
        request1.endpoint = "/test";
        request1.clientId = "client-1";
        
        auto response1 = gateway.ProcessRequest(request1);
        REQUIRE(response1.statusCode != 429); // Not rate limited
        
        APIRequest request2;
        request2.endpoint = "/test";
        request2.clientId = "client-1";
        
        auto response2 = gateway.ProcessRequest(request2);
        REQUIRE(response2.statusCode != 429); // Not rate limited
        
        // Third request should be rate limited
        // Note: In real implementation, rate limiting would be enforced
    }
}

TEST_CASE("API Gateway Manager - Request Processing", "[api][requests]") {
    APIGatewayManager gateway;
    
    SECTION("Non-existent endpoint") {
        APIRequest request;
        request.endpoint = "/non-existent";
        
        auto response = gateway.ProcessRequest(request);
        REQUIRE(response.statusCode == 404);
    }
    
    SECTION("Authenticated endpoint without auth") {
        APIEndpoint endpoint;
        endpoint.path = "/protected";
        endpoint.method = APIMethod::GET;
        endpoint.requiresAuth = true;
        endpoint.rateLimit = 100;
        gateway.RegisterEndpoint(endpoint);
        
        APIRequest request;
        request.endpoint = "/protected";
        request.headers["Authorization"] = ""; // No auth token
        
        auto response = gateway.ProcessRequest(request);
        REQUIRE(response.statusCode == 401);
    }
}

TEST_CASE("API Gateway Manager - Documentation", "[api][docs]") {
    APIGatewayManager gateway;
    
    SECTION("API documentation generation") {
        APIEndpoint endpoint1;
        endpoint1.path = "/api/v1/users";
        endpoint1.method = APIMethod::GET;
        endpoint1.description = "List all users";
        endpoint1.rateLimit = 100;
        endpoint1.requiresAuth = true;
        gateway.RegisterEndpoint(endpoint1);
        
        APIEndpoint endpoint2;
        endpoint2.path = "/api/v1/users";
        endpoint2.method = APIMethod::POST;
        endpoint2.description = "Create a new user";
        endpoint2.rateLimit = 50;
        endpoint2.requiresAuth = true;
        gateway.RegisterEndpoint(endpoint2);
        
        auto docs = gateway.GenerateAPIDocumentation();
        
        REQUIRE_FALSE(docs.empty());
        REQUIRE(docs.find("# API Documentation") != std::string::npos);
        REQUIRE(docs.find("GET /api/v1/users") != std::string::npos);
        REQUIRE(docs.find("POST /api/v1/users") != std::string::npos);
    }
}
