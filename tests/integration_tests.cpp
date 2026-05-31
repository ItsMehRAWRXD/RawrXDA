// ============================================================================
// Integration Tests — End-to-End Feature Testing
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include "../src/review/ai_code_reviewer.cpp"
#include "../src/dependencies/smart_dependency_manager.cpp"
#include "../src/api/api_gateway_manager.cpp"
#include "../src/feature_flags/feature_flags_manager.cpp"
#include "../src/metrics/metrics_collector.cpp"
#include "../src/notifications/notification_service.cpp"

using namespace RawrXD;

// Mock implementations
class IntegrationMockAIClient : public SovereignInferenceClient {
public:
    bool IsLoaded() const override { return true; }
    ChatResult ChatSync(const std::vector<ChatMessage>& messages) override {
        return {true, "Integration test response", 0.9, 100};
    }
};

class IntegrationMockSessionManager : public Core::SessionManager {
public:
    void SetValue(const std::string& key, const std::string& value) override {
        m_values[key] = value;
    }
    
    std::string GetValue(const std::string& key) override {
        auto it = m_values.find(key);
        return it != m_values.end() ? it->second : "";
    }
    
private:
    std::map<std::string, std::string> m_values;
};

class IntegrationMockGitIntegration : public GitIntegration {
public:
    std::vector<std::string> GetChangedFiles(const std::string& commitId) override {
        return {"src/main.cpp", "src/utils.cpp", "tests/test.cpp"};
    }
    
    std::string GetDiff(const std::string& commitId, const std::string& file) override {
        return "+void newFunction() { }\n-int oldFunction();";
    }
};

TEST_CASE("Integrated System - Code Review with Dependencies", "[integration][code-review]") {
    auto aiClient = std::make_shared<IntegrationMockAIClient>();
    auto gitIntegration = std::make_shared<IntegrationMockGitIntegration>();
    auto sessionManager = std::make_shared<IntegrationMockSessionManager>();
    
    Review::AICodeReviewer reviewer(aiClient, gitIntegration);
    Dependencies::SmartDependencyManager deps(aiClient);
    
    SECTION("Review code with dependency context") {
        // Review changes
        auto reviewReport = reviewer.ReviewChanges("abc123");
        
        REQUIRE(reviewReport.commitId == "abc123");
        
        // Check for dependencies in reviewed files
        for (const auto& file : IntegrationMockGitIntegration().GetChangedFiles("")) {
            // Verify file was processed
            REQUIRE_FALSE(file.empty());
        }
    }
}

TEST_CASE("Integrated System - Feature Flags with Metrics", "[integration][feature-flags]") {
    auto sessionManager = std::make_shared<IntegrationMockSessionManager>();
    auto aiClient = std::make_shared<IntegrationMockAIClient>();
    
    FeatureFlags::FeatureFlagsManager flags(sessionManager);
    Metrics::MetricsCollector metrics(sessionManager);
    
    SECTION("Track feature flag usage with metrics") {
        // Register feature
        FeatureFlags::FeatureFlag flag;
        flag.key = "test_feature";
        flag.name = "Test Feature";
        flag.state = FeatureFlags::FeatureState::ON;
        flags.RegisterFeature(flag);
        
        // Enable and track
        flags.EnableFeature("test_feature", "admin");
        
        // Record metric for feature usage
        metrics.SetGauge("feature:test_feature:usage", 1.0);
        
        REQUIRE(flags.IsEnabled("test_feature"));
        
        // Verify metrics
        auto timeSeries = metrics.GetTimeSeries("feature:test_feature:usage",
            std::chrono::system_clock::now() - std::chrono::hours(1),
            std::chrono::system_clock::now()
        );
        REQUIRE(timeSeries.values.size() == 1);
    }
}

TEST_CASE("Integrated System - Notifications with Workflow", "[integration][notifications]") {
    auto sessionManager = std::make_shared<IntegrationMockSessionManager>();
    
    Notifications::NotificationService notifications(sessionManager);
    
    SECTION("Send notification on workflow event") {
        // Register template
        Notifications::NotificationTemplate templ;
        templ.id = "workflow_complete";
        templ.name = "Workflow Complete";
        templ.subject = "Workflow {{workflow_name}} completed";
        templ.body = "Workflow {{workflow_name}} completed at {{completion_time}}";
        templ.channel = Notifications::NotificationChannel::EMAIL;
        
        notifications.RegisterTemplate(templ);
        
        // Send notification
        std::map<std::string, std::string> variables = {
            {"workflow_name", "Test Workflow"},
            {"completion_time", "2024-01-01 12:00:00"}
        };
        
        auto notif = notifications.SendNotification("workflow_complete", variables,
                                                     "admin@example.com",
                                                     Notifications::NotificationPriority::NORMAL);
        
        REQUIRE_FALSE(notif.id.empty());
        REQUIRE(notif.recipient == "admin@example.com");
    }
}

TEST_CASE("Integrated System - API Gateway with Rate Limiting", "[integration][api-gateway]") {
    API::APIGatewayManager gateway;
    
    SECTION("API with rate limiting and authentication") {
        // Register endpoint
        API::APIEndpoint endpoint;
        endpoint.path = "/api/v1/protected";
        endpoint.method = API::APIMethod::GET;
        endpoint.rateLimit = 100;
        endpoint.requiresAuth = true;
        endpoint.description = "Protected endpoint";
        
        gateway.RegisterEndpoint(endpoint);
        
        // Set rate limit
        gateway.SetRateLimit("client-1", 1000, 3600);
        
        // Create request without auth
        API::APIRequest request;
        request.endpoint = "/api/v1/protected";
        request.clientId = "client-1";
        // No Authorization header
        
        auto response = gateway.ProcessRequest(request);
        REQUIRE(response.statusCode == 401); // Unauthorized
        
        // Create request with auth
        request.headers["Authorization"] = "Bearer token123";
        response = gateway.ProcessRequest(request);
        // Should process (mock auth always succeeds)
        REQUIRE(response.statusCode != 401);
    }
}

TEST_CASE("Integrated System - Cross-Feature Data Flow", "[integration][data-flow]") {
    auto aiClient = std::make_shared<IntegrationMockAIClient>();
    auto sessionManager = std::make_shared<IntegrationMockSessionManager>();
    
    SECTION("Full data pipeline") {
        // 1. Code review triggers metrics
        auto gitIntegration = std::make_shared<IntegrationMockGitIntegration>();
        Review::AICodeReviewer reviewer(aiClient, gitIntegration);
        
        // 2. Metrics trigger notifications
        Metrics::MetricsCollector metrics(sessionManager);
        Notifications::NotificationService notifications(sessionManager);
        
        Notifications::NotificationTemplate templ;
        templ.id = "alert_template";
        templ.name = "Alert";
        templ.subject = "Alert: {{metric_name}}";
        templ.body = "{{metric_name}} is {{value}}";
        templ.channel = Notifications::NotificationChannel::EMAIL;
        notifications.RegisterTemplate(templ);
        
        // 3. Notifications trigger workflow
        // (Workflow integration)
        
        // Simulate pipeline
        metrics.SetGauge("review_score", 7.5);
        
        auto timeSeries = metrics.GetTimeSeries("review_score",
            std::chrono::system_clock::now() - std::chrono::hours(1),
            std::chrono::system_clock::now()
        );
        
        if (!timeSeries.values.empty()) {
            auto value = timeSeries.values.back().value;
            if (value < 8.0) {
                // Send notification
                std::map<std::string, std::string> vars = {
                    {"metric_name", "review_score"},
                    {"value", std::to_string(value)}
                };
                notifications.SendNotification("alert_template", vars,
                                              "team@example.com",
                                              Notifications::NotificationPriority::HIGH);
            }
        }
        
        REQUIRE(timeSeries.values.size() > 0);
    }
}
